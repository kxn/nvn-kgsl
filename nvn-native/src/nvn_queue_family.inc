/* Queue/sync/event/command management correctness candidate, owner-thread only.
 * Native KGSL context timestamps and real CP memory packets; no guest service,
 * renderer substitution or CPU emulation. Parameter limits are deliberate. */
static int new_order_job(struct object *device,unsigned flags,struct job **out,size_t *bytes)
{
    *out=NULL;*bytes=0;
    struct job*j=calloc(1,sizeof(*j));if(!j)return ENOMEM;
    int e=nn_bo_alloc(device->gpu,8192,&j->bo);
    if(!e && !nn_bo_map(j->bo))e=EFAULT;
    if(!e){
        memset(nn_bo_map(j->bo),0,8192);
        size_t words=0;
        e=nn_order_words(nn_device_chip(device->gpu),nn_bo_address(j->bo)+4096,flags,
            nn_bo_map(j->bo),1024,&words);
        if(!e)e=nn_bo_cache(j->bo,0,8192,0);
        if(!e)*bytes=words*4;
    }
    if(e){
        if(j->bo){device->refs++;retire_jobs(device,j);}else free(j);
        return e;
    }
    *out=j;return 0;
}
static int record_order(struct object *command,unsigned flags)
{
    int e=command_reserve(command,24);if(e)return e;
    struct job*j=NULL;size_t bytes=0;
    e=new_order_job(command->device,flags,&j,&bytes);if(e)return e;
    struct recording*r=command->record;
    uint64_t addr=nn_bo_address(j->bo);
    uint32_t*p=(uint32_t*)((char*)nn_bo_map(r->pool->bo)+r->offset+r->used);
    p[0]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);p[1]=addr;p[2]=addr>>32;p[3]=bytes/4;
    r->used+=16;j->next=r->current->jobs;r->current->jobs=j;command->device->refs++;
    return 0;
}
static void queue_family_handle_cleanup(struct handle*h)
{free_chunks(h->chunks);h->chunks=h->chunks_tail=NULL;copy_commands_handle_cleanup(h);}
static void api_nvnCommandBufferBarrier(NVNcommandBuffer*key,int flags)
{
    struct object*o=recording_command(key);if(!o)return;
    if(flags&~0xff || flags&NVN_BARRIER_INVALIDATE_SHADER_BIT){error(ENOTSUP);return;}
    if(!flags)return; /* Empty requested dependency has no operation. */
    /* A conservative full dependency is stronger than the supported order,
     * texture/descriptor and zcull invalidations. Shader I-cache remains an
     * explicit unsupported bit until its SP/HLSQ protocol is qualified. */
    error(record_order(o,NN_ORDER_FULL_CACHE|NN_ORDER_INVALIDATE));
}
static NVNboolean api_nvnSyncInitialize(NVNsync*key,NVNdevice*d)
{
    struct object*device=find(d,DEVICE);if(!device)return 0;
    struct object*s=create(key,SYNC);if(!s)return 0;s->device=device;device->refs++;return 1;
}
static void api_nvnSyncFinalize(NVNsync*key)
{
    struct object*s=find(key,SYNC);if(!s)return;
    if(s->refs){error(EBUSY);return;}
    if(s->fenced&&s->fence.queue)s->fence.queue->refs--;
    s->device->refs--;remove_object(s);
}
static void api_nvnQueueFenceSync(NVNqueue*key,NVNsync*sync_key,NVNsyncCondition condition,int flags)
{
    struct object*q=find(key,QUEUE),*s=find(sync_key,SYNC);if(!q||!s)return;
    if(q->device!=s->device){error(EINVAL);return;}
    if((condition!=NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE &&
        condition!=NVN_SYNC_CONDITION_GRAPHICS_WORLD_SPACE_COMPLETE) || flags&~1){error(ENOTSUP);return;}
    /* KGSL retirement is a conservative full-command completion even when
     * only world-space completion was requested; no partial-stage claim. */
    if(s->refs){error(EBUSY);return;}
    int e=nn_queue_flush(q->queue);if(e){error(e);return;}
    if(flags&1){
        struct job*j=NULL;size_t bytes=0;e=new_order_job(q->device,NN_ORDER_FULL_CACHE,&j,&bytes);
        if(e){error(e);return;}
        uint32_t timestamp=0;
        e=nn_queue_submit(q->queue,j->bo,0,bytes,NULL,0,&timestamp);
        q->device->refs++;retire_jobs(q->device,j);
        if(e){error(e);return;}
    }
    struct wait_snapshot next={q,nn_queue_timestamp(q->queue)};
    q->refs++;
    if(s->fenced&&s->fence.queue)s->fence.queue->refs--;
    s->fence=next;s->fenced=1;s->availability=0;
}
static NVNsyncWaitResult api_nvnSyncWait(const NVNsync*key,uint64_t timeout)
{
    struct object*s=find(key,SYNC);if(!s)return NVN_SYNC_WAIT_RESULT_FAILED;
    if(s->availability)return NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED;
    if(!s->fenced){
        if(!timeout)return NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED;
        error(ENOTSUP);return NVN_SYNC_WAIT_RESULT_FAILED;
    }
    int complete=0,e=nn_queue_timestamp_status(s->fence.queue->queue,s->fence.timestamp,&complete);
    if(e){error(e);return NVN_SYNC_WAIT_RESULT_FAILED;}
    if(complete){reap_jobs(s->device);return NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED;}
    if(!timeout)return NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED;
    /* Finite >=millisecond kernel waits round up. Very long waits are a
     * declared subset, not silently shortened or called successful. */
    if(timeout!=UINT64_MAX && timeout>UINT64_C(4294967294000000)){error(ENOTSUP);return NVN_SYNC_WAIT_RESULT_FAILED;}
    uint32_t ms=timeout==UINT64_MAX?UINT32_MAX:(uint32_t)(timeout/1000000u+!!(timeout%1000000u));
    do { e=nn_queue_wait_timestamp(s->fence.queue->queue,s->fence.timestamp,ms); }
    while(timeout==UINT64_MAX && (e==ETIMEDOUT || e==ETIME));
    if(e==ETIMEDOUT || e==ETIME)return NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED;
    if(e){error(e);return NVN_SYNC_WAIT_RESULT_FAILED;}
    reap_jobs(s->device);return NVN_SYNC_WAIT_RESULT_CONDITION_SATISFIED;
}
static int snapshot_wait(struct object*device,struct object*s,struct wait_snapshot*out)
{
    if(s->device!=device)return EINVAL;
    *out=(struct wait_snapshot){0};
    if(s->availability)return 0;
    if(!s->fenced)return EINVAL;
    int complete=0,e=nn_queue_timestamp_status(s->fence.queue->queue,s->fence.timestamp,&complete);
    if(e)return e;
    if(!complete)*out=s->fence;
    return 0;
}
static int append_wait(struct wait_snapshot*list,unsigned*count,struct wait_snapshot value)
{
    if(!value.queue)return 0;
    for(unsigned i=0;i<*count;i++)if(list[i].queue==value.queue){
        if((int32_t)(value.timestamp-list[i].timestamp)>0)list[i].timestamp=value.timestamp;
        return 0;
    }
    if(*count>=16)return ENOSPC;
    list[(*count)++]=value;return 0;
}
static NVNboolean api_nvnQueueWaitSync(NVNqueue*key,const NVNsync*sync_key)
{
    struct object*q=find(key,QUEUE),*s=find(sync_key,SYNC);if(!q||!s)return 0;
    struct wait_snapshot value;int e=snapshot_wait(q->device,s,&value);if(e){error(e);return 0;}
    if(value.queue==q)return 1;
    unsigned before=q->wait_count;e=append_wait(q->waits,&q->wait_count,value);
    if(e){error(e);return 0;}
    if(q->wait_count!=before)value.queue->refs++;
    return 1;
}
static void api_nvnCommandBufferWaitSync(NVNcommandBuffer*key,const NVNsync*sync_key)
{
    struct object*o=recording_command(key),*s=find(sync_key,SYNC);if(!o||!s)return;
    struct wait_snapshot value;int e=snapshot_wait(o->device,s,&value);if(e){error(e);return;}
    if(!value.queue)return;
    struct recording*r=o->record;
    /* Kernel batch dependencies are before the entire handle. Restrict to
     * its first GPU command; never move a mid-stream dependency earlier and
     * risk a cross-queue deadlock. Already-satisfied waits need no packet. */
    if(r->logical_bytes || r->used!=r->start){error(ENOTSUP);return;}
    if(r->current->wait_count>=16){error(ENOSPC);return;}
    e=add_resource(r->current,value.queue);if(e){error(e);return;}
    e=append_wait(r->current->waits,&r->current->wait_count,value);error(e);
}
static void api_nvnEventBuilderSetDefaults(NVNeventBuilder*b)
{builder(b,EVENT_BUILDER);}
static void api_nvnEventBuilderSetStorage(NVNeventBuilder*b,const NVNmemoryPool*p,int64_t offset)
{
    struct object*o=find(b,EVENT_BUILDER),*pool=find(p,POOL);if(!o||!pool)return;
    if(offset<0 || offset%4 || (uint64_t)offset>pool->size || 4>pool->size-(size_t)offset){error(EINVAL);return;}
    o->pool=pool;o->builder_pool_key=p;o->offset=(size_t)offset;
}
static NVNboolean api_nvnEventInitialize(NVNevent*key,const NVNeventBuilder*b)
{
    struct object*v=find(b,EVENT_BUILDER);if(!v)return 0;
    struct object*pool=live_weak(v->pool,POOL);
    if(!pool || v->offset>pool->size || 4>pool->size-v->offset){error(EINVAL);return 0;}
    if(pool->external || pool->flags&NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS || !nn_bo_map(pool->bo)){
        error(ENOTSUP);return 0;
    }
    if(gpu_pool_access(pool,v->offset,4,"nvnEventInitialize"))return 0;
    const size_t offset=v->offset;
    struct object*e=create(key,EVENT);if(!e)return 0;
    e->pool=pool;e->device=pool->device;e->bo=pool->bo;e->offset=offset;e->size=4;
    pool->refs++;return 1; /* Preserve caller's initial 32-bit event value. */
}
static void api_nvnEventFinalize(NVNevent*key)
{struct object*e=find(key,EVENT);if(!e)return;if(e->refs){error(EBUSY);return;}e->pool->refs--;remove_object(e);}
static uint32_t api_nvnEventGetValue(const NVNevent*key)
{
    struct object*e=find(key,EVENT);if(!e)return 0;
    int err=nn_bo_cache(e->bo,e->offset,4,1);if(err){error(err);return 0;}
    uint32_t*p=(uint32_t*)((char*)nn_bo_map(e->bo)+e->offset);
    return __atomic_load_n(p,__ATOMIC_SEQ_CST);
}
static void api_nvnEventSignal(NVNevent*key,NVNeventSignalMode mode,uint32_t value)
{
    struct object*e=find(key,EVENT);if(!e)return;
    if(mode!=NVN_EVENT_SIGNAL_MODE_WRITE && mode!=NVN_EVENT_SIGNAL_MODE_ADD){error(ENOTSUP);return;}
    uint32_t*p=(uint32_t*)((char*)nn_bo_map(e->bo)+e->offset);
    if(mode==NVN_EVENT_SIGNAL_MODE_ADD)__atomic_fetch_add(p,value,__ATOMIC_SEQ_CST);
    else __atomic_store_n(p,value,__ATOMIC_SEQ_CST);
    /* A publication failure is sticky and retains the owner; do not claim
     * that a failed cache ioctl published the new value to a waiting GPU. */
    error(nn_bo_cache(e->bo,e->offset,4,0));
}
static int event_record_resources(struct object*o,struct object*e)
{
    if(e->device!=o->device)return EINVAL;
    int err=gpu_pool_access(e->pool,e->offset,4,"NVNcommand event");if(err)return err;
    size_t before=o->record->current->count;
    err=add_resource(o->record->current,e);
    if(!err)err=add_resource(o->record->current,e->pool);
    if(err)while(o->record->current->count>before)o->record->current->resources[--o->record->current->count]->refs--;
    return err;
}
static void api_nvnCommandBufferWaitEvent(NVNcommandBuffer*key,const NVNevent*event_key,NVNeventWaitMode mode,uint32_t value)
{
    struct object*o=recording_command(key),*e=find(event_key,EVENT);if(!o||!e)return;
    if(mode!=NVN_EVENT_WAIT_MODE_EQUAL){error(ENOTSUP);return;}
    int err=command_reserve(o,40);if(!err)err=event_record_resources(o,e);if(err){error(err);return;}
    struct recording*r=o->record;uint64_t addr=nn_bo_address(e->bo)+e->offset;
    uint32_t*p=(uint32_t*)((char*)nn_bo_map(r->pool->bo)+r->offset+r->used);
    p[0]=nvn_pm4_packet7(0x3c,6);p[1]=3u|(1u<<4);p[2]=addr;p[3]=addr>>32;
    p[4]=value;p[5]=UINT32_MAX;p[6]=16;p[7]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0);r->used+=32;
}
static void api_nvnCommandBufferSignalEvent(NVNcommandBuffer*key,const NVNevent*event_key,NVNeventSignalMode mode,
                                           NVNeventSignalLocation location,int flags,uint32_t value)
{
    struct object*o=recording_command(key),*e=find(event_key,EVENT);if(!o||!e)return;
    if(mode!=NVN_EVENT_SIGNAL_MODE_WRITE || flags || location<NVN_EVENT_SIGNAL_LOCATION_TOP ||
        location>NVN_EVENT_SIGNAL_LOCATION_BOTTOM){error(ENOTSUP);return;}
    int err=command_reserve(o,44);if(!err)err=event_record_resources(o,e);
    if(!err && location!=NVN_EVENT_SIGNAL_LOCATION_TOP)err=record_order(o,NN_ORDER_FULL_CACHE);
    if(err){error(err);return;}
    struct recording*r=o->record;uint64_t addr=nn_bo_address(e->bo)+e->offset;
    uint32_t*p=(uint32_t*)((char*)nn_bo_map(r->pool->bo)+r->offset+r->used);
    p[0]=nvn_pm4_packet7(NVN_CP_MEM_WRITE,3);p[1]=addr;p[2]=addr>>32;p[3]=value;
    p[4]=nvn_pm4_packet7(0x12,0);r->used+=20;
}
static int debug_record(struct object*o,unsigned type,uint32_t domain,const char*label)
{
    if(!label)return EINVAL;
    size_t len=0;while(len<=1024&&label[len])len++;
    if(len>1024)return E2BIG;
    size_t payload=(len+1+3)/4+3,bytes=(payload+1)*4;
    int e=command_reserve(o,bytes+8);if(e)return e;
    struct recording*r=o->record;uint32_t*p=(uint32_t*)((char*)nn_bo_map(r->pool->bo)+r->offset+r->used);
    p[0]=nvn_pm4_packet7(0x10,(uint16_t)payload);p[1]=0x4e564e44;p[2]=type;p[3]=domain;
    memset(p+4,0,(payload-3)*4);memcpy(p+4,label,len+1);r->used+=bytes;return 0;
}
static void api_nvnCommandBufferPushDebugGroupDynamic(NVNcommandBuffer*key,uint32_t domain,const char*label)
{
    struct object*o=recording_command(key);if(!o)return;
    if(o->record->debug_depth>=32){error(ENOSPC);return;}
    int e=debug_record(o,1,domain,label);if(e){error(e);return;}
    o->record->debug_domains[o->record->debug_depth++]=domain;
}
static void api_nvnCommandBufferPopDebugGroup(NVNcommandBuffer*key)
{
    struct object*o=recording_command(key);if(!o)return;
    if(!o->record->debug_depth){error(EINVAL);return;}
    uint32_t domain=o->record->debug_domains[o->record->debug_depth-1];
    int e=debug_record(o,2,domain,"");if(e){error(e);return;}o->record->debug_depth--;
}
static void api_nvnCommandBufferInsertDebugMarkerDynamic(NVNcommandBuffer*key,uint32_t domain,const char*label)
{struct object*o=recording_command(key);if(o)error(debug_record(o,3,domain,label));}
