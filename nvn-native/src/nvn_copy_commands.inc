/* NVN CopyCommands: immutable record-time command/metadata snapshot. Private
 * child jobs retain their original GPUVAs through independent borrow owners.
 * Source handle, arena and CPU metadata are never revisited at enqueue. */
static void copy_commands_handle_cleanup(struct handle*h)
{
    while(h->borrowed_jobs){
        struct job_borrow*b=h->borrowed_jobs;h->borrowed_jobs=b->next;
        struct job*j=b->job;
        if(!j->borrowers){error(EPROTO);free(b);continue;}
        j->borrowers--;int reap=!j->borrowers&&j->owner_retired;free(b);
        if(reap)reap_jobs(h->device);
    }
}
static int copy_borrow_job(struct handle*plan,struct job*j)
{
    if(!j || !j->bo || j->borrowers==SIZE_MAX)return EOVERFLOW;
    for(struct job_borrow*b=plan->borrowed_jobs;b;b=b->next)if(b->job==j)return 0;
    struct job_borrow*b=malloc(sizeof(*b));if(!b)return ENOMEM;
    *b=(struct job_borrow){plan->borrowed_jobs,j};plan->borrowed_jobs=b;j->borrowers++;return 0;
}
static void copy_plan_discard(struct handle*plan)
{
    for(size_t i=0;i<plan->count;i++)plan->resources[i]->refs--;
    for(size_t i=0;i<plan->cpu_count;i++)plan->cpu_resources[i]->refs--;
    free(plan->resources);free(plan->cpu_resources);free_draws(plan->draws);command_copy_free(plan->copies);plan->copies=NULL;
    counter_free_ops(plan->counter_ops);plan->counter_ops=NULL;
    copy_commands_handle_cleanup(plan);
}
static int copy_source_bytes(const struct handle*h,unsigned char*dst)
{
    if(h->chunks){
        size_t used=0;
        for(struct command_chunk*c=h->chunks;c;c=c->next){
            if(!c->pool || !c->pool->bo || !nn_bo_map(c->pool->bo) || c->offset>c->pool->size ||
                c->bytes>c->pool->size-c->offset || used>h->bytes || c->bytes>h->bytes-used)return EINVAL;
            memcpy(dst+used,(const char*)nn_bo_map(c->pool->bo)+c->offset,c->bytes);used+=c->bytes;
        }
        return used==h->bytes?0:EINVAL;
    }
    if(!h->commands || !h->commands->bo || !nn_bo_map(h->commands->bo) || h->offset>h->commands->size ||
        h->bytes>h->commands->size-h->offset)return EINVAL;
    memcpy(dst,(const char*)nn_bo_map(h->commands->bo)+h->offset,h->bytes);return 0;
}
static int copy_source_metadata(struct handle*plan,const struct handle*src,size_t adjustment,int prefix)
{
    int e=0;
    if(src->count>65535 || src->cpu_count>65535)return E2BIG;
    for(size_t i=0;i<src->count;i++){
        if(src->resources[i]->refs==SIZE_MAX)return EOVERFLOW;
        if((e=add_resource(plan,src->resources[i])))return e;
    }
    for(size_t i=0;i<src->cpu_count;i++){
        if(src->cpu_resources[i]->refs==SIZE_MAX)return EOVERFLOW;
        if((e=add_cpu_resource(plan,src->cpu_resources[i])))return e;
    }
    for(struct job*j=src->jobs;j;j=j->next)if((e=copy_borrow_job(plan,j)))return e;
    for(struct job_borrow*b=src->borrowed_jobs;b;b=b->next)if((e=copy_borrow_job(plan,b->job)))return e;
    for(struct late_draw*d=src->draws;d;d=d->next){
        if(d->command_offset>src->bytes || src->bytes-d->command_offset<16 || d->command_offset>SIZE_MAX-adjustment)return EINVAL;
        struct late_draw*clone=malloc(sizeof(*clone));if(!clone)return ENOMEM;
        *clone=*d;
        if(clone->generic_pipeline && (e=pipeline_record_retain(&clone->pipeline))){free(clone);return e;}
        clone->command_offset+=adjustment;clone->next=plan->draws;plan->draws=clone;
    }
#if NN_NVN_DEPTH_TARGETS
    if((e=command_copy_depth_clone(plan,src,adjustment)))return e;
#endif
    if((e=counter_clone_metadata(plan,src,adjustment)))return e;
    for(unsigned i=0;i<src->wait_count;i++){
        struct wait_snapshot wait=src->waits[i];
        /* Kernel dependencies cannot be hoisted over a preceding copied or
         * recorded GPU command. A completed condition needs no batch wait. */
        if(prefix){int complete=0;e=nn_queue_timestamp_status(wait.queue->queue,wait.timestamp,&complete);
            if(e)return e;
            if(!complete)return ENOTSUP;
            continue;
        }
        if((e=append_wait(plan->waits,&plan->wait_count,wait)))return e;
    }
    return 0;
}
static int copy_merge_array(struct object**a,size_t na,struct object**b,size_t nb,struct object***out,size_t*count)
{
    *out=NULL;*count=0;
    if(na>65535 || nb>65535 || na>65535-nb)return E2BIG;
    struct object**p=malloc((na+nb+1)*sizeof(*p));if(!p)return ENOMEM;
    if(na)memcpy(p,a,na*sizeof(*p));
    size_t n=na;
    for(size_t i=0;i<nb;i++){
        size_t j=0;while(j<n&&p[j]!=b[i])j++;
        if(j==n)p[n++]=b[i];
    }
    *out=p;*count=n;return 0;
}
static int copy_contains(struct object**a,size_t n,struct object*b)
{for(size_t i=0;i<n;i++)if(a[i]==b)return 1;return 0;}
static void copy_commit_arrays(struct handle*dst,struct handle*plan,struct object**gpu,size_t ngpu,struct object**cpu,size_t ncpu)
{
    for(size_t i=0;i<plan->count;i++)if(copy_contains(dst->resources,dst->count,plan->resources[i]))plan->resources[i]->refs--;
    for(size_t i=0;i<plan->cpu_count;i++)if(copy_contains(dst->cpu_resources,dst->cpu_count,plan->cpu_resources[i]))plan->cpu_resources[i]->refs--;
    free(dst->resources);free(dst->cpu_resources);dst->resources=gpu;dst->count=ngpu;dst->cpu_resources=cpu;dst->cpu_count=ncpu;
    free(plan->resources);free(plan->cpu_resources);plan->resources=plan->cpu_resources=NULL;plan->count=plan->cpu_count=0;
    while(plan->borrowed_jobs){
        struct job_borrow*b=plan->borrowed_jobs;plan->borrowed_jobs=b->next;int duplicate=0;
        for(struct job*j=dst->jobs;j;j=j->next)if(j==b->job)duplicate=1;
        for(struct job_borrow*p=dst->borrowed_jobs;p;p=p->next)if(p->job==b->job)duplicate=1;
        if(duplicate){b->job->borrowers--;free(b);}
        else{b->next=dst->borrowed_jobs;dst->borrowed_jobs=b;}
    }
#if NN_NVN_DEPTH_TARGETS
    command_copy_depth_transfer(dst,plan);
#endif
    while(plan->draws){struct late_draw*d=plan->draws;plan->draws=d->next;d->next=dst->draws;dst->draws=d;}
    struct counter_op **tail=&dst->counter_ops;while(*tail)tail=&(*tail)->next;
    *tail=plan->counter_ops;plan->counter_ops=NULL;
}
static void api_nvnCommandBufferCopyCommands(NVNcommandBuffer*key,int count,const NVNcommandHandle*ids)
{
    struct object*o=recording_command(key);if(!o)return;
    if(count<0 || count>65535 || (count&&!ids)){error(EINVAL);return;}
    if(!count)return;
    struct recording*r=o->record;
    if(r->in_callback){error(EDEADLK);return;}
    NVNcommandHandle*selection=malloc((size_t)count*sizeof(*selection));if(!selection){error(ENOMEM);return;}
    memcpy(selection,ids,(size_t)count*sizeof(*selection));
    size_t bytes=0;int e=0;
    for(int i=0;i<count;i++){
        struct handle*h=handles;while(h&&(h->id!=selection[i]||h->public_retired))h=h->next;
        if(!h || h->device!=o->device || !h->bytes || h->bytes%4){e=EINVAL;break;}
        if(h->call_nodes){e=ENOTSUP;break;}
        if(h->bytes>SIZE_MAX-bytes-8){e=EOVERFLOW;break;}bytes+=h->bytes;
    }
    if(e){free(selection);error(e);return;}
    unsigned char*snapshot=malloc(bytes);if(!snapshot){free(selection);error(ENOMEM);return;}
    struct handle plan={.device=o->device};struct nn_command_sequence *sequence_plan=NULL;
    const struct nn_command_sequence **sequences=calloc((size_t)count,sizeof(*sequences));
    size_t *adjustments=calloc((size_t)count,sizeof(*adjustments));
    if(!sequences||!adjustments){free(sequences);free(adjustments);free(selection);free(snapshot);error(ENOMEM);return;}
    size_t base=r->logical_bytes+r->used-r->start,used=0;
    if(bytes>SIZE_MAX-base){e=EOVERFLOW;goto fail;}
    for(int i=0;i<count;i++){
        struct handle*h=handles;while(h&&(h->id!=selection[i]||h->public_retired))h=h->next;
        e=copy_source_bytes(h,snapshot+used);if(e)goto fail;
        e=copy_source_metadata(&plan,h,base+used,base+used!=0);if(e)goto fail;
        sequences[i]=h->call_sequence;adjustments[i]=base+used;
        used+=h->bytes;
    }
    /* Stage independent state/binding owners before command_reserve can call
     * the application. Source handles may be finalized inside that callback. */
    e=nn_command_sequence_create(r->current,&command_owner_ops,o->device,&sequence_plan);if(e)goto fail;
    size_t saved_limit=r->current->call_limit;r->current->call_limit=base+bytes;
    e=sequence_copy_flat(sequence_plan,(unsigned)count,sequences,adjustments);
    r->current->call_limit=saved_limit;if(e)goto fail;
    /* All source memory, refs and selectors are independent before invoking
     * a user callback. Finalizing a source in that callback cannot dangle. */
    e=command_reserve(o,bytes+8);if(e)goto fail;
    struct wait_snapshot merged[16];unsigned nwait=r->current->wait_count;
    memcpy(merged,r->current->waits,nwait*sizeof(*merged));
    for(unsigned i=0;i<plan.wait_count;i++)if((e=append_wait(merged,&nwait,plan.waits[i])))goto fail;
    struct object**gpu=NULL,**cpu=NULL;size_t ngpu=0,ncpu=0;
    e=copy_merge_array(r->current->resources,r->current->count,plan.resources,plan.count,&gpu,&ngpu);if(e)goto fail;
    e=copy_merge_array(r->current->cpu_resources,r->current->cpu_count,plan.cpu_resources,plan.cpu_count,&cpu,&ncpu);
    if(e){free(gpu);goto fail;}
    if((e=command_capture_close(o))||(e=sequence_append_prepared(r->current->call_sequence,sequence_plan))){free(gpu);free(cpu);goto fail;}
    memcpy((char*)nn_bo_map(r->pool->bo)+r->offset+r->used,snapshot,bytes);
    copy_commit_arrays(r->current,&plan,gpu,ngpu,cpu,ncpu);
    memcpy(r->current->waits,merged,nwait*sizeof(*merged));r->current->wait_count=nwait;r->used+=bytes;
    r->current->call_limit=r->current->call_cursor=base+bytes;
fail:
    nn_command_sequence_destroy(sequence_plan);free(sequences);free(adjustments);
    free(selection);free(snapshot);copy_plan_discard(&plan);if(e)error(e);
}
