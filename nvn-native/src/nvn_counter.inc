/* SPDX-License-Identifier: MIT. Actual hardware query jobs, no pixel estimates.
 * The opaque KMD/SQE context-save record does not publicly enumerate the
 * internal RB sample counter. Cross-context isolation remains a hardware
 * qualification boundary, not established merely by per-Queue software BOs.
 */
_Static_assert(sizeof(NVNcounterData)==16 && offsetof(NVNcounterData,timestamp)==8,"counterData ABI");
static int counter_pool_alias(const struct object *a,const struct object *b)
{
    if(a->bo==b->bo)return 1;
    return a->storage && b->storage && range_overlap((uintptr_t)a->storage,a->size,(uintptr_t)b->storage,b->size);
}
static int counter_destination(struct object *command,uint64_t address,struct object **out,size_t *offset)
{
    if(!address || address%16 || address>UINT64_MAX-16)return EINVAL;
    struct object *p=address_pool(command->device,address,16);
    if(!p || p->external)return ENOTSUP;
    if(gpu_pool_access(p,address-nn_bo_address(p->bo),16,"nvnCommandBufferReportCounter"))return ENOTSUP;
    for(struct object *o=objects;o;o=o->next)if(o->kind==COMMAND && o->record->pool && counter_pool_alias(p,o->record->pool))return ENOTSUP;
    for(struct handle *h=handles;h;h=h->next){
        if(counter_pool_alias(p,h->commands))return ENOTSUP;
        for(struct command_chunk *c=h->chunks;c;c=c->next)if(counter_pool_alias(p,c->pool))return ENOTSUP;
    }
    *out=p;*offset=(size_t)(address-nn_bo_address(p->bo));return 0;
}
static void counter_free_ops(struct counter_op *op)
{while(op){struct counter_op *next=op->next;free(op);op=next;}}
/* CopyCommands calls this after cloning normal resource refs. All-or-nothing
 * metadata cloning; report destinations are original retained pool identities.
 * Empty placeholders are never sufficient without these ordered operations. */
static int __attribute__((unused)) counter_clone_metadata(struct handle *dst,const struct handle *src,size_t adjustment)
{
    struct counter_op *list=NULL,**tail=&list;
    for(const struct counter_op *op=src->counter_ops;op;op=op->next){
        if(op->command_offset>SIZE_MAX-adjustment){counter_free_ops(list);return EOVERFLOW;}
        struct counter_op *copy=malloc(sizeof(*copy));if(!copy){counter_free_ops(list);return ENOMEM;}
        *copy=*op;copy->command_offset+=adjustment;copy->next=NULL;*tail=copy;tail=&copy->next;
    }
    struct counter_op **at=&dst->counter_ops;while(*at)at=&(*at)->next;*at=list;return 0;
}
static void counter_record(NVNcommandBuffer *key,enum nn_counter_operation operation,uint64_t address)
{
    struct object *o=recording_command(key);if(!o)return;
    if(nn_device_chip(o->device->gpu)!=0x06050002u){error(ENOTSUP);return;}
    struct object *pool=NULL;size_t offset=0;int e=0;
    if(operation!=NN_COUNTER_RESET_SAMPLES){e=counter_destination(o,address,&pool,&offset);if(e){error(e);return;}}
    struct counter_op *op=calloc(1,sizeof(*op));if(!op){error(ENOMEM);return;}
    e=command_reserve(o,24);if(e){free(op);error(e);return;}
    struct recording *r=o->record;
    if(gpu_pool_access(r->pool,r->offset,r->capacity,"counter command arena")){free(op);return;}
    if(pool && (e=add_resource(r->current,pool))){free(op);error(e);return;}
    *op=(struct counter_op){.operation=operation,.command_offset=r->logical_bytes+r->used-r->start,.destination=pool,.destination_offset=offset};
    struct counter_op **tail=&r->current->counter_ops;while(*tail)tail=&(*tail)->next;*tail=op;
    /* A queue-specific immutable private parent replaces this before submit. */
    memset((char *)nn_bo_map(r->pool->bo)+r->offset+r->used,0,16);r->used+=16;
}
static void api_nvnCommandBufferResetCounter(NVNcommandBuffer *key,NVNcounterType type)
{
    if(type!=NVN_COUNTER_TYPE_SAMPLES_PASSED){if(recording_command(key))error(ENOTSUP);return;}
    counter_record(key,NN_COUNTER_RESET_SAMPLES,0);
}
static void api_nvnCommandBufferReportCounter(NVNcommandBuffer *key,NVNcounterType type,NVNbufferAddress address)
{
    enum nn_counter_operation op;
    switch(type){
    case NVN_COUNTER_TYPE_SAMPLES_PASSED:op=NN_COUNTER_REPORT_SAMPLES;break;
    case NVN_COUNTER_TYPE_TIMESTAMP:op=NN_COUNTER_REPORT_TIMESTAMP;break;
    case NVN_COUNTER_TYPE_TIMESTAMP_TOP:op=NN_COUNTER_REPORT_TIMESTAMP_TOP;break;
    default:if(recording_command(key))error(ENOTSUP);return;
    }
    counter_record(key,op,address);
}
static uint64_t api_nvnDeviceGetTimestampInNanoseconds(const NVNdevice *key,const NVNcounterData *data)
{
    if(!find(key,DEVICE))return 0;
    if(!data){error(EINVAL);return 0;}
    uint64_t ns=0;int e=nn_counter_ticks_to_ns(data->timestamp,&ns);if(e)error(e);return ns;
}
static int counter_queue_release(struct object *q)
{
    if(!q->counter)return 0;
    int e=nn_bo_destroy(q->counter->bo);if(e)return e;
    free(q->counter);q->counter=NULL;return 0;
}
/* Verify packet boundaries from a registered immutable artifact. This is a
 * rejection audit, never a scan-and-patch of arbitrary command data. */
static int counter_image_safe(const struct nn_gfx_image *image)
{
    if(!image || image->chip!=0x06050002u || image->schema!=3 || !image->commands || !image->command_words)return ENOTSUP;
    size_t words=image->command_words;
    for(size_t at=0;at<words;){
        uint32_t h=image->commands[at],n=nvn_pm4_packet_words(h);if(!n || n>words-at)return EINVAL;
        if((h>>28)==4){unsigned reg=(h>>8)&0x7ffffu,count=n-1;
            if((reg<=0x8891 && 0x8891-reg<count) || (reg<=0x8928 && reg+count>0x8927))return ENOTSUP;
        } else {
            unsigned opcode=(h>>16)&0x7f;
            if(opcode==NVN_CP_INDIRECT_BUFFER)return ENOTSUP;
            if(opcode==0x6d && n>=4){unsigned reg=image->commands[at+2];if(reg==0x8891 || reg==0x8927 || reg==0x8928)return ENOTSUP;}
        }
        at+=n;
    }
    return 0;
}
static int counter_bracket(const struct object *q,const struct handle *h)
{
    if(q->counter && q->counter->valid)return 1;
    for(const struct counter_op *op=h->counter_ops;op;op=op->next)
        if(op->operation==NN_COUNTER_RESET_SAMPLES || op->operation==NN_COUNTER_REPORT_SAMPLES)return 1;
    return 0;
}
static int counter_preflight(struct object *q,const struct handle *h,unsigned *valid,size_t *extra)
{
    *extra=0;
    if(q->counter && q->counter->poisoned)return EBUSY;
    if(*valid || h->counter_ops)for(size_t i=0;i<h->count;i++) {
        const struct object *p=h->resources[i];
        if(p->kind==PROGRAM && p->graphics){int e=counter_image_safe(p->graphics->image);if(e)return e;}
    }
    unsigned bracket=*valid;size_t previous=0;unsigned first=1;
    for(const struct counter_op *op=h->counter_ops;op;op=op->next){
        if((unsigned)op->operation>NN_COUNTER_REPORT_TIMESTAMP_TOP)return EINVAL;
        if(op->command_offset>h->bytes || h->bytes-op->command_offset<16 ||
            (!first && op->command_offset<previous+16))return EINVAL;
        first=0;previous=op->command_offset;
        for(const struct late_draw *d=h->draws;d;d=d->next)
            if(op->command_offset<d->command_offset+16 && d->command_offset<op->command_offset+16)return EINVAL;
        if(op->operation==NN_COUNTER_RESET_SAMPLES){*valid=1;bracket=1;}
        else {
            if(op->operation==NN_COUNTER_REPORT_SAMPLES && !*valid)return ENODATA;
            struct object *p=op->destination;
            if(!p || p->kind!=POOL || p->device!=q->device || op->destination_offset>p->size || p->size-op->destination_offset<16)return EINVAL;
            if(gpu_pool_access(p,op->destination_offset,16,"counter preflight"))return ENOTSUP;
        }
        if(++*extra>65535)return E2BIG;
    }
    if(*extra || bracket)(*extra)++; /* state BO */
    if(bracket)*extra+=2; /* before/after-submit query jobs, no extra IB depth */
    size_t jobs=0,draws=0;for(const struct job *j=h->jobs;j;j=j->next)jobs++;
    for(const struct late_draw *d=h->draws;d;d=d->next)draws++;
    if(h->count>65535 || jobs>65535 || draws>65535)return E2BIG;
    if(h->count*2+jobs+draws*2+*extra>q->queue_config.control/sizeof(nn_bo *))return ENOSPC;
    return 0;
}
static int counter_job(struct object *q,enum nn_counter_operation operation,uint64_t dst,nn_bo **refs,size_t *ref_count,size_t ref_capacity,struct job **temporary,uint64_t *address,uint32_t *words)
{
    if(*ref_count>=ref_capacity)return ENOSPC;
    uint32_t command[96];size_t count=0;
    int e=nn_counter_words(nn_device_chip(q->device->gpu),operation,nn_bo_address(q->counter->bo),dst,command,96,&count);if(e)return e;
    struct job *job=calloc(1,sizeof(*job));if(!job)return ENOMEM;
    e=nn_bo_alloc(q->device->gpu,count*4,&job->bo);if(e){free(job);return e;}
    job->next=*temporary;*temporary=job;q->device->refs++;
    memcpy(nn_bo_map(job->bo),command,count*4);
    e=nn_bo_cache(job->bo,0,count*4,0);if(e)return e;
    queue_reference(refs,ref_count,job->bo);*address=nn_bo_address(job->bo);*words=(uint32_t)count;return 0;
}
static void counter_put_ib(uint32_t *w,uint64_t address,uint32_t words)
{w[0]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);w[1]=(uint32_t)address;w[2]=(uint32_t)(address>>32);w[3]=words;}
static int counter_materialize(struct object *q,const struct handle *h,nn_bo *parent,size_t shift,nn_bo **refs,size_t *ref_count,size_t ref_capacity,struct job **temporary)
{
    if(!h->counter_ops && !shift)return 0;
    if(!q->counter){
        struct counter_state *state=calloc(1,sizeof(*state));if(!state)return ENOMEM;
        int e=nn_bo_alloc(q->device->gpu,64,&state->bo);
        if(e){free(state);return e;}q->counter=state;
    }
    if(*ref_count>=ref_capacity)return ENOSPC;
    queue_reference(refs,ref_count,q->counter->bo);
    uint64_t address=0;uint32_t words=0;int e=0;
    if(shift){
        e=counter_job(q,NN_COUNTER_BEGIN_SAMPLES,0,refs,ref_count,ref_capacity,temporary,&address,&words);if(e)return e;
        counter_put_ib(nn_bo_map(parent),address,words);
    }
    for(const struct counter_op *op=h->counter_ops;op;op=op->next){
        uint32_t *patch=(uint32_t *)((char *)nn_bo_map(parent)+shift+op->command_offset);
        if(patch[0]||patch[1]||patch[2]||patch[3])return EINVAL;
        uint64_t dst=op->destination?nn_bo_address(op->destination->bo)+op->destination_offset:0;
        e=counter_job(q,op->operation,dst,refs,ref_count,ref_capacity,temporary,&address,&words);if(e)return e;
        counter_put_ib(patch,address,words);
    }
    if(shift){
        e=counter_job(q,NN_COUNTER_END_SAMPLES,0,refs,ref_count,ref_capacity,temporary,&address,&words);if(e)return e;
        counter_put_ib((uint32_t *)((char *)nn_bo_map(parent)+shift+h->bytes),address,words);
    }
    return 0;
}
static void counter_published(struct object *q,const struct handle *h,int submit_error)
{
    if(!q->counter)return;
    if(submit_error){q->counter->poisoned=1;return;}
    for(const struct counter_op *op=h->counter_ops;op;op=op->next)
        if(op->operation==NN_COUNTER_RESET_SAMPLES)q->counter->valid=1;
}
