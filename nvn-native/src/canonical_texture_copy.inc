/* SPDX-License-Identifier: MIT. Canonical GOB -> private tight blocks -> GOB.
 * Both stages execute the selected normal array or volume shader; CPU only prepares
 * metadata/code/UBOs. No source/middle/destination pixel mapping or conversion. */
#if NN_NVN_BLOCK_COPY
static int record_canonical_texture_copy_native(struct object *command,struct object *source,const NVNtextureView *sv,
 const NVNcopyRegion *sr,struct object *destination,const NVNtextureView *dv,const NVNcopyRegion *dr)
{
    struct recording *r=command->record;struct object *device=command->device;
    if(!r->active||!source->source_layout||!destination->source_layout)return ENOTSUP;
    if(!sr||!dr)return EINVAL;
    if(sr->width!=dr->width||sr->height!=dr->height||sr->depth!=dr->depth)return ENOTSUP;
    if(source==destination)return EINVAL; /* full canonical windows overlap */
    if(source->refs==SIZE_MAX||destination->refs==SIZE_MAX)return EOVERFLOW;
    source->refs++;destination->refs++;
    int e=command_reserve(command,40);
    source->refs--;destination->refs--;
    if(e)return e;
    /* Derive all borrowed backing identities only after a possible user
     * callback, while both textures were strongly pinned across that callback. */
    struct canonical_copy_region raw[2];
    if((e=canonical_copy_region(source,sv,sr,0,0,raw))||
       (e=canonical_copy_region(destination,dv,dr,0,0,raw+1)))return e;
    if(source->texture.format!=destination->texture.format||raw[0].row_bytes!=raw[1].row_bytes||
       raw[0].rows!=raw[1].rows||raw[0].layers!=raw[1].layers)return ENOTSUP;
    struct object *textures[]={source,destination};struct canonical_native_plan plan[2];
    struct resource_span spans[2];
    for(unsigned i=0;i<2;i++){
        if((e=canonical_native_plan(device,textures[i],raw+i,i,plan+i)))return e;
        spans[i]=(struct resource_span){textures[i]->pool,textures[i],textures[i]->offset,
            (size_t)plan[i].canonical_bytes,(size_t)plan[i].canonical_bytes,1};
        if((e=gpu_pool_access(spans[i].pool,spans[i].offset,spans[i].row_bytes,"canonical texture raw-block copy")))return e;
    }
    /* Cross-target copies require two distinct immutable code owners. This
     * bounded route admits same-kernel pairs; it is not an NVN prohibition. */
    if(plan[0].image!=plan[1].image)return ENOTSUP;
    if(plan[0].linear_bytes!=plan[1].linear_bytes)return EINVAL;
    if((e=canonical_copy_alias(spans,spans+1))||(e=canonical_copy_arena(spans+1)))return e;
    struct object *resources[]={source,destination,source->pool,destination->pool};
    for(unsigned i=0;i<4;i++)if(resources[i]->refs==SIZE_MAX)return EOVERFLOW;
    if(device->refs>SIZE_MAX-6)return EOVERFLOW;
    if((e=gpu_pool_access(r->pool,r->offset,r->capacity,"raw-block command arena")))return e;
    void *arena=nn_bo_map(r->pool->bo);if(!arena)return ENOTSUP;
    struct job *nodes[6]={0};
    for(unsigned i=0;i<6;i++)if(!(nodes[i]=calloc(1,sizeof(*nodes[i])))){
        for(unsigned k=0;k<6;k++)free(nodes[k]);
        return ENOMEM;
    }
    /* One shared code, two immutable UBOs, fresh private middle, two IBs. */
    struct job *code=nodes[0],*uniform[2]={nodes[1],nodes[2]},*middle=nodes[3],*dispatch[2]={nodes[4],nodes[5]};
    size_t bytes[2]={0},old=r->current->count;
    const struct nn_cs_image *image=plan[0].image;
    if((e=nn_bo_alloc(device->gpu,image->code_bytes,&code->bo))||
       (e=nn_bo_alloc(device->gpu,64,&uniform[0]->bo))||
       (e=nn_bo_alloc(device->gpu,64,&uniform[1]->bo))||
       (e=nn_bo_alloc(device->gpu,(size_t)plan[0].linear_bytes,&middle->bo)))goto fail;
    void *code_map=nn_bo_map(code->bo);if(!code_map){e=ENOTSUP;goto fail;}
    memcpy(code_map,image->code,image->code_bytes);
    /* Fresh driver-owned middle allocation cannot alias the live caller
     * imported canonical pages. Its entire padded DWORD span is privately
     * owned; it is never exposed as a public texture/buffer address. */
    for(unsigned i=0;i<2;i++){
        void *uniform_map=nn_bo_map(uniform[i]->bo);if(!uniform_map){e=ENOTSUP;goto fail;}
        struct nn_block_copy_range canonical={spans[i].pool->bo,0,spans[i].pool->size,spans[i].offset};
        struct nn_block_copy_range linear={middle->bo,0,nn_bo_size(middle->bo),0};
        struct nn_block_copy_range u={uniform[i]->bo,0,nn_bo_size(uniform[i]->bo),0};
        struct canonical_native_binding binding;
        if((e=canonical_native_bind(device,textures[i],plan+i,&canonical,&linear,&u,&binding)))goto fail;
        memcpy(uniform_map,&binding.args,sizeof(binding.args));
        if((e=nn_cs_encode(device->gpu,code->bo,image,binding.ubos,binding.ssbos,binding.groups,&dispatch[i]->bo,bytes+i)))goto fail;
    }
    if((e=nn_bo_cache(code->bo,0,image->code_bytes,0)))goto fail;
    for(unsigned i=0;i<2;i++)if((e=nn_bo_cache(uniform[i]->bo,0,64,0))||
        (e=nn_bo_cache(dispatch[i]->bo,0,nn_bo_size(dispatch[i]->bo),0)))goto fail;
    for(unsigned i=0;i<4;i++)if((e=add_resource(r->current,resources[i])))goto fail;
    /* Each normal A6 IB completes CACHE_FLUSH_TS+WAIT_MEM_GTE, and the
     * second begins CACHE_INVALIDATE+WFI/WFM before state/shader loads.
     * Ordered parent IB calls preserve the middle write -> read dependency. */
    uint32_t *w=(uint32_t *)((char *)arena+r->offset+r->used);
    for(unsigned i=0;i<2;i++){
        uint64_t ib=nn_bo_address(dispatch[i]->bo);
        w[4*i]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);w[4*i+1]=(uint32_t)ib;
        w[4*i+2]=(uint32_t)(ib>>32);w[4*i+3]=(uint32_t)(bytes[i]/4);
    }
    r->used+=32;
    dispatch[0]->copy_source=source;dispatch[1]->copy_destination=destination;
    dispatch[1]->copy_destination_pool=spans[1].pool;dispatch[1]->copy_destination_offset=spans[1].offset;
    dispatch[1]->copy_destination_bytes=span_bytes(spans+1);
    for(unsigned i=0;i<6;i++){nodes[i]->compute=1;nodes[i]->next=i+1<6?nodes[i+1]:r->current->jobs;}
    r->current->jobs=nodes[0];device->refs+=6;return 0;
fail:
    while(r->current->count>old)r->current->resources[--r->current->count]->refs--;
    struct job *owned=NULL;
    for(unsigned i=0;i<6;i++){
        if(nodes[i]->bo){nodes[i]->next=owned;owned=nodes[i];device->refs++;}
        else free(nodes[i]);
    }
    retire_jobs(device,owned);return e;
}
#endif
