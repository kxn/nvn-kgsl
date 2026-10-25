/* SPDX-License-Identifier: MIT. Original canonical block storage -> Adreno CS.
 * Metadata/owner admission only on CPU; image data is read and written by GPU.
 * This subset supports flags0, same-format 2D/array/3D pixel regions; it does not
 * assert that scaled/reinterpreted/other flag combinations are invalid NVN. */
static int copy_handle_resource_access(const struct handle *h,struct object *o)
{
#if NN_NVN_BLOCK_COPY || NN_NVN_CLEAR_COLOR
    if(o->kind==TEXTURE&&o->source_layout){
        int copy=0;
        for(const struct job *j=h->jobs;j;j=j->next)
            if(j->copy_source==o||j->copy_destination==o){copy=1;break;}
        for(const struct job_borrow *b=h->borrowed_jobs;!copy&&b;b=b->next)
            if(b->job->copy_source==o||b->job->copy_destination==o)copy=1;
        if(copy){
            int e=nn_tex_source_validate(o->source_layout);if(e){error(e);return e;}
            return gpu_pool_access(o->pool,o->offset,o->size,"nvnQueueSubmitCommands raw-block copy");
        }
    }
#else
    (void)h;
#endif
    return gpu_object_access(o,"nvnQueueSubmitCommands");
}
#if NN_NVN_BLOCK_COPY
/* The original storage remains canonical. Select one separately typed
 * hardware kernel for array BH addressing or true volume BD/Z addressing. */
struct canonical_native_plan {
    unsigned volume;
    const struct nn_cs_image *image;
    uint64_t canonical_bytes,linear_bytes;
    union { struct nn_block_copy_plan array; struct nn_volume_copy_plan volume; } typed;
};
struct canonical_native_binding {
    uint32_t args[16],groups[3];
    struct nn_cs_buffer ubos[16],ssbos[16];
};
static int canonical_native_plan(struct object *device,const struct object *texture,
 const struct canonical_copy_region *r,unsigned direction,struct canonical_native_plan *out)
{
    memset(out,0,sizeof(*out));int e;
    struct canonical_native_plan p={0};
    p.volume=texture->texture.target==NVN_TEXTURE_TARGET_3D;
    p.image=p.volume?device->volume_copy:device->block_copy;
    if(!p.image)return ENOTSUP;
    if(p.volume){
        struct nn_tex_volume_layout source;
        if((e=nn_tex_source_volume(texture->source_layout,&source)))return e;
        struct nn_volume_copy_region region={r->level,r->base_layer,r->layers,r->x_bytes,r->y_rows,
            r->row_bytes,r->rows,r->linear_row_stride,r->linear_image_stride,direction};
        if((e=nn_volume_copy_plan(&source,&region,&p.typed.volume)))return e;
        p.canonical_bytes=p.typed.volume.canonical_bytes;p.linear_bytes=p.typed.volume.linear_bytes;
    }else{
        struct nn_block_copy_region region={r->level,r->base_layer,r->layers,r->x_bytes,r->y_rows,
            r->row_bytes,r->rows,r->linear_row_stride,r->linear_image_stride,direction};
        if((e=nn_block_copy_plan(texture->source_layout,&region,&p.typed.array)))return e;
        p.canonical_bytes=p.typed.array.canonical_bytes;p.linear_bytes=p.typed.array.linear_bytes;
    }
    *out=p;return 0;
}
static int canonical_native_bind(struct object *device,const struct object *texture,
 const struct canonical_native_plan *p,const struct nn_block_copy_range *c,
 const struct nn_block_copy_range *l,const struct nn_block_copy_range *u,struct canonical_native_binding *out)
{
    memset(out,0,sizeof(*out));int e;
    if(p->volume){
        struct nn_tex_volume_layout source;struct nn_volume_copy_binding b;
        if((e=nn_tex_source_volume(texture->source_layout,&source))||
           (e=nn_volume_copy_bind(device->gpu,p->image,&source,&p->typed.volume,c,l,u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)))return e;
        memcpy(out->args,&b.args,sizeof(out->args));memcpy(out->groups,b.groups,sizeof(out->groups));
        memcpy(out->ubos,b.ubos,sizeof(out->ubos));memcpy(out->ssbos,b.ssbos,sizeof(out->ssbos));
    }else{
        struct nn_block_copy_binding b;
        if((e=nn_block_copy_bind(device->gpu,p->image,texture->source_layout,&p->typed.array,c,l,u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)))return e;
        memcpy(out->args,&b.args,sizeof(out->args));memcpy(out->groups,b.groups,sizeof(out->groups));
        memcpy(out->ubos,b.ubos,sizeof(out->ubos));memcpy(out->ssbos,b.ssbos,sizeof(out->ssbos));
    }
    return 0;
}
/* Imported anonymous private mappings have no alternate mmap aliases. Strip
 * Android TBI exactly as the KGSL ADDR import, before addition/comparison. */
static int canonical_copy_alias(const struct resource_span *a,const struct resource_span *b)
{
    size_t an=span_bytes(a),bn=span_bytes(b);
    uint64_t aa,ba;int e;
    if(a->pool->external||b->pool->external)return ENOTSUP;
    if((e=bridge_cpu_range(a->pool,0,a->pool->size,&aa))||
       (e=bridge_cpu_range(b->pool,0,b->pool->size,&ba)))return e;
    if((e=private_storage((const void *)(uintptr_t)aa,a->pool->size))||
       (e=private_storage((const void *)(uintptr_t)ba,b->pool->size)))return e;
    if(a->pool->bo==b->pool->bo&&range_overlap(a->offset,an,b->offset,bn))return EINVAL;
    aa+=a->offset;ba+=b->offset;
    return range_overlap(aa,an,ba,bn)?EINVAL:0;
}
#endif /* NN_NVN_BLOCK_COPY: arena protection also applies to CP copies. */
static int canonical_copy_arena_range(const struct resource_span *destination,
 const struct object *pool,size_t offset,size_t bytes)
{
    size_t n=span_bytes(destination);uint64_t address,other;int e;
    if(!pool||!bytes)return 0;
    if(destination->pool->bo==pool->bo&&range_overlap(destination->offset,n,offset,bytes))return ENOTSUP;
    if((e=bridge_cpu_range(destination->pool,destination->offset,n,&address)))return e;
    if(pool->storage){
        if((e=bridge_cpu_range(pool,offset,bytes,&other)))return e;
        if(range_overlap(address,n,other,bytes))return ENOTSUP;
    }
    return 0;
}
static int canonical_copy_arena(const struct resource_span *destination)
{
    int e;
    for(const struct object *o=objects;o;o=o->next)if(o->kind==COMMAND){
        const struct recording *r=o->record;
        if((e=canonical_copy_arena_range(destination,r->pool,r->offset,r->capacity)))return e;
        if(r->current)for(const struct command_chunk *c=r->current->chunks;c;c=c->next)
            if((e=canonical_copy_arena_range(destination,c->pool,c->offset,c->bytes)))return e;
    }
    /* A published handle still stages its original bytes even if the command
     * object moved arenas or was finalized. Protect all retained chunks. */
    for(const struct handle *h=handles;h;h=h->next){
        if(h->chunks){
            for(const struct command_chunk *c=h->chunks;c;c=c->next)
                if((e=canonical_copy_arena_range(destination,c->pool,c->offset,c->bytes)))return e;
        }else if((e=canonical_copy_arena_range(destination,h->commands,h->offset,h->bytes)))return e;
    }
    return 0;
}
#if NN_NVN_BLOCK_COPY
static int record_canonical_copy_native(struct object *command,struct object *texture,const NVNtextureView *view,
 const NVNcopyRegion *region,uint64_t address,int to_texture)
{
    struct recording *r=command->record;struct object *device=command->device;
    if(!r->active)return ENOTSUP;
    /* The memory callback may invoke resource APIs. Keep this identity alive,
     * then derive every borrowed pool/owner only after the callback returns. */
    if(texture->refs==SIZE_MAX)return EOVERFLOW;
    texture->refs++;int reserve=command_reserve(command,24);texture->refs--;
    if(reserve)return reserve;
    struct canonical_copy_region raw;int e=canonical_copy_region(texture,view,region,
        r->copy_row_stride,r->copy_image_stride,&raw);if(e)return e;
    struct canonical_native_plan plan;
    if((e=canonical_native_plan(device,texture,&raw,(unsigned)!!to_texture,&plan)))return e;
    if(!address||address%4||plan.linear_bytes>SIZE_MAX)return EINVAL;
    struct object *linear=address_pool(device,address,(size_t)plan.linear_bytes);
    if(!linear||linear->external||texture->pool->external)return ENOTSUP;
    struct resource_span canonical={texture->pool,texture,texture->offset,(size_t)plan.canonical_bytes,(size_t)plan.canonical_bytes,1};
    struct resource_span buffer={linear,NULL,(size_t)(address-nn_bo_address(linear->bo)),(size_t)plan.linear_bytes,(size_t)plan.linear_bytes,1};
    if((e=gpu_pool_access(canonical.pool,canonical.offset,canonical.row_bytes,"canonical raw-block copy"))||
       (e=gpu_pool_access(linear,buffer.offset,buffer.row_bytes,"linear raw-block copy"))||
       (e=canonical_copy_alias(&canonical,&buffer))||
       (e=canonical_copy_arena(to_texture?&canonical:&buffer)))return e;
    /* Allocation/registration is driver-private. Source pixels are not read.
     * Reject pin overflow before any recording bytes or owner changes. */
    struct object *resources[]={texture,texture->pool,linear};
    for(unsigned i=0;i<3;i++)if(resources[i]->refs==SIZE_MAX)return EOVERFLOW;
    if(device->refs>SIZE_MAX-3)return EOVERFLOW;
    if((e=gpu_pool_access(r->pool,r->offset,r->capacity,"raw-block command arena")))return e;
    if(!nn_bo_map(r->pool->bo))return ENOTSUP;
    struct job *code=calloc(1,sizeof(*code)),*uniform=calloc(1,sizeof(*uniform)),*dispatch=calloc(1,sizeof(*dispatch));
    if(!code||!uniform||!dispatch){free(code);free(uniform);free(dispatch);return ENOMEM;}
    size_t bytes=0,old=r->current->count;
    const struct nn_cs_image *image=plan.image;
    if((e=nn_bo_alloc(device->gpu,image->code_bytes,&code->bo))||
       (e=nn_bo_alloc(device->gpu,64,&uniform->bo)))goto fail;
    void *code_map=nn_bo_map(code->bo),*uniform_map=nn_bo_map(uniform->bo);
    if(!code_map||!uniform_map){e=ENOTSUP;goto fail;}
    struct nn_block_copy_range c={canonical.pool->bo,0,canonical.pool->size,canonical.offset};
    struct nn_block_copy_range b={linear->bo,0,linear->size,buffer.offset};
    struct nn_block_copy_range u={uniform->bo,0,nn_bo_size(uniform->bo),0};
    struct canonical_native_binding binding;
    if((e=canonical_native_bind(device,texture,&plan,&c,&b,&u,&binding)))goto fail;
    memcpy(code_map,image->code,image->code_bytes);memcpy(uniform_map,&binding.args,sizeof(binding.args));
    if((e=nn_cs_encode(device->gpu,code->bo,image,binding.ubos,binding.ssbos,binding.groups,&dispatch->bo,&bytes)))goto fail;
    if((e=nn_bo_cache(code->bo,0,image->code_bytes,0))||
       (e=nn_bo_cache(uniform->bo,0,sizeof(binding.args),0))||
       (e=nn_bo_cache(dispatch->bo,0,nn_bo_size(dispatch->bo),0)))goto fail;
    for(unsigned i=0;i<3;i++)if((e=add_resource(r->current,resources[i])))goto fail;
    uint64_t ib=nn_bo_address(dispatch->bo);
    uint32_t *w=(uint32_t *)((char *)nn_bo_map(r->pool->bo)+r->offset+r->used);
    w[0]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);w[1]=(uint32_t)ib;w[2]=(uint32_t)(ib>>32);w[3]=(uint32_t)(bytes/4);
    r->used+=16;
    dispatch->copy_source=to_texture?NULL:texture;dispatch->copy_destination=to_texture?texture:NULL;
    const struct resource_span *destination=to_texture?&canonical:&buffer;
    dispatch->copy_destination_pool=destination->pool;dispatch->copy_destination_offset=destination->offset;
    dispatch->copy_destination_bytes=span_bytes(destination);
    code->compute=uniform->compute=dispatch->compute=1;
    code->next=uniform;uniform->next=dispatch;dispatch->next=r->current->jobs;r->current->jobs=code;device->refs+=3;
    return 0;
fail:
    while(r->current->count>old)r->current->resources[--r->current->count]->refs--;
    struct job *owned=NULL,*jobs[]={code,uniform,dispatch};
    for(unsigned i=0;i<3;i++){
        if(jobs[i]->bo){jobs[i]->next=owned;owned=jobs[i];device->refs++;}
        else free(jobs[i]);
    }
    retire_jobs(device,owned);return e;
}
#endif

/* Queue's first pass repeats destination admission against arenas bound after
 * recording. Borrowed jobs preserve these immutable destination coordinates. */
static int copy_handle_preflight(const struct handle *h)
{
    for(const struct job *j=h->jobs;j;j=j->next)if(j->copy_destination_pool){
        struct resource_span d={j->copy_destination_pool,NULL,j->copy_destination_offset,j->copy_destination_bytes,j->copy_destination_bytes,1};
        int e=canonical_copy_arena(&d);if(e)return e;
    }
    for(const struct job_borrow *b=h->borrowed_jobs;b;b=b->next)if(b->job->copy_destination_pool){
        const struct job *j=b->job;
        struct resource_span d={j->copy_destination_pool,NULL,j->copy_destination_offset,j->copy_destination_bytes,j->copy_destination_bytes,1};
        int e=canonical_copy_arena(&d);if(e)return e;
    }
    return 0;
}
