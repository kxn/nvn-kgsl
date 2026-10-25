/* SPDX-License-Identifier: MIT. Native synchronous texel transfer subset.
 * SDK53.314476e50/4770b0 copy caller bytes before returning and do not enqueue
 * or wait GPU producers. This backend uses actual GPU byte conversion, and
 * returns successfully only after its private dispatch is complete. Pending
 * producers, non-CPU-accessible/unknown mappings, unsupported views and alias
 * overlaps reject explicitly. This is not a claim that NVN must wait queues.
 * Caller allocation/mapping lifetime is still the ordinary public contract.
 * Fault/unknown completion is sticky-fatal at the host bridge; retained pins
 * protect pages and all GPU owners, not permission to reuse caller contents. */
static int texel_cache_range(struct object *t,const NVNtextureView *view,size_t *offset,size_t *bytes)
{
    struct resource_view v;int e=resource_view_read(view,&v);if(e)return e;
    unsigned layers=t->texture.target==NVN_TEXTURE_TARGET_2D_ARRAY?(unsigned)t->texture.depth:1,first=0;
    if((e=view_offset_range((int)layers,v.layers,v.base_layer,v.layer_count,&first)))return e;
    unsigned count=v.layers&&v.layer_count?(unsigned)v.layer_count:layers-first;
    uint64_t stride=t->size;
    if(t->source_layout){
        if((e=nn_tex_source_validate(t->source_layout)))return e;
        if(t->source_layout->layers!=layers||t->source_layout->payload_bytes!=t->size)return EINVAL;
        stride=t->source_layout->layer_stride;
    }else if(layers!=1)return ENOTSUP;
    uint64_t rel=(uint64_t)first*stride,n=(uint64_t)count*stride;
    if(!n||rel>t->size||n>t->size-rel||t->offset>t->pool->size||rel>t->pool->size-t->offset||n>t->pool->size-t->offset-rel)return EINVAL;
    *offset=t->offset+(size_t)rel;*bytes=(size_t)n;return 0;
}
static void texture_cache_texels(const NVNtexture *key,const NVNtextureView *view,
 const NVNcopyRegion *region,int from_gpu)
{
    (void)region; /* This release ignores region, format and mip selection. */
    struct object *t=find(key,TEXTURE);if(!t)return;
    if(t->pool->external){error(ENOTSUP);return;}
    size_t offset,bytes;int e=texel_cache_range(t,view,&offset,&bytes);
    if(!e&&(t->pool->flags&NVN_MEMORY_POOL_FLAGS_CPU_CACHED))e=nn_bo_cache(t->bo,offset,bytes,from_gpu);
    error(e);
}
static void api_nvnTextureFlushTexels(const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r)
{texture_cache_texels(t,v,r,0);}
static void api_nvnTextureInvalidateTexels(const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r)
{texture_cache_texels(t,v,r,1);}
#if NN_NVN_BLOCK_COPY
struct texel_transfer_owner {
    struct texel_transfer_owner *next;struct object *texture,*device,*linear_pool;
    nn_bo *imported,*code,*uniform,*commands;nn_queue *queue;
};
static struct texel_transfer_owner *retained_texel_transfers;
static int texel_transfer_cleanup(struct texel_transfer_owner *p)
{
    int e;
    if(p->queue){if((e=nn_queue_destroy(p->queue)))return e;p->queue=NULL;}
    nn_bo **bos[]={&p->commands,&p->uniform,&p->code,&p->imported};
    for(unsigned i=0;i<4;i++)if(*bos[i]){if((e=nn_bo_destroy(*bos[i])))return e;*bos[i]=NULL;}
    if(p->linear_pool)p->linear_pool->refs--;
    p->texture->refs--;p->device->refs--;free(p);return 0;
}
static int texel_cpu_address(const struct object *pool,size_t offset,size_t bytes,uint64_t *address)
{
    if(!pool||!pool->storage||pool->external||offset>pool->size||bytes>pool->size-offset)return ENOTSUP;
    uint64_t base=(uintptr_t)pool->storage&UINT64_C(0x00ffffffffffffff);
    if(offset>UINT64_MAX-base||bytes>UINT64_MAX-base-offset)return EOVERFLOW;
    *address=base+offset;return 0;
}
static int texel_alias_idle(const struct object *texture,uint64_t linear,size_t linear_bytes,
 uint64_t canonical,size_t canonical_bytes)
{
    /* All same-driver duplicate imports and Buffer/SSBO aliases share the
     * actual caller pages. Never infer physical independence from GPUVAs. */
    for(const struct object *pool=objects;pool;pool=pool->next)if(pool->kind==POOL&&pool->storage&&!pool->external){
        uint64_t address;int e=texel_cpu_address(pool,0,pool->size,&address);if(e)return e;
        if(range_overlap(address,pool->size,linear,linear_bytes)||
           range_overlap(address,pool->size,canonical,canonical_bytes)){
            if(pool->device!=texture->device)return ENOTSUP;
            if((e=nn_bo_idle(pool->bo)))return e;
        }
    }
    return nn_bo_idle(texture->bo);
}
static int texel_destination_arena(uint64_t address,size_t bytes)
{
    for(const struct object *o=objects;o;o=o->next)if(o->kind==COMMAND){
        const struct recording *r=o->record;uint64_t arena;int e;
        if(r->control&&r->control_capacity){uint64_t control=(uintptr_t)r->control&UINT64_C(0x00ffffffffffffff);
            if(range_overlap(address,bytes,control,r->control_capacity))return ENOTSUP;}
        if(r->pool){if((e=texel_cpu_address(r->pool,r->offset,r->capacity,&arena)))return e;
            if(range_overlap(address,bytes,arena,r->capacity))return ENOTSUP;}
        if(r->current)for(const struct command_chunk *c=r->current->chunks;c;c=c->next){
            if((e=texel_cpu_address(c->pool,c->offset,c->bytes,&arena)))return e;
            if(range_overlap(address,bytes,arena,c->bytes))return ENOTSUP;}
    }
    for(const struct handle *h=handles;h;h=h->next){
        uint64_t arena;int e;
        if(h->chunks){for(const struct command_chunk *c=h->chunks;c;c=c->next){
            if((e=texel_cpu_address(c->pool,c->offset,c->bytes,&arena)))return e;
            if(range_overlap(address,bytes,arena,c->bytes))return ENOTSUP;}}
        else if(h->commands&&h->bytes){if((e=texel_cpu_address(h->commands,h->offset,h->bytes,&arena)))return e;
            if(range_overlap(address,bytes,arena,h->bytes))return ENOTSUP;}
    }
    return 0;
}
#endif
static int canonical_texel_transfer(struct object *t,const NVNtextureView *view,
 const NVNcopyRegion *region,void *caller,ptrdiff_t row,ptrdiff_t image,int to_texture)
{
#if !NN_NVN_BLOCK_COPY
    (void)t;(void)view;(void)region;(void)caller;(void)row;(void)image;(void)to_texture;return ENOTSUP;
#else
    if(!t->device->block_copy||!t->source_layout||t->pool->external||
       !(t->pool->flags&NVN_MEMORY_POOL_FLAGS_CPU_CACHED))return ENOTSUP;
    struct canonical_copy_region raw;int e=canonical_copy_region(t,view,region,row,image,&raw);if(e)return e;
    struct nn_block_copy_region selected={raw.level,raw.base_layer,raw.layers,raw.x_bytes,raw.y_rows,
        raw.row_bytes,raw.rows,raw.linear_row_stride,raw.linear_image_stride,(unsigned)!!to_texture};
    struct nn_block_copy_plan plan;if((e=nn_block_copy_plan(t->source_layout,&selected,&plan)))return e;
    uintptr_t p=(uintptr_t)caller&UINT64_C(0x00ffffffffffffff);size_t span=(size_t)plan.linear_bytes;
    size_t page=nn_device_page(t->device->gpu);
    if(!p||p%4||!page||(page&(page-1))||page<4096||span>UINTPTR_MAX-p||p+span>UINTPTR_MAX-(page-1))return EINVAL;
    uintptr_t begin=p&~(uintptr_t)(page-1),end=(p+span+page-1)&~(uintptr_t)(page-1);
    uint64_t canonical;if((e=texel_cpu_address(t->pool,t->offset,t->size,&canonical)))return e;
    uint64_t storage;if((e=texel_cpu_address(t->pool,0,t->pool->size,&storage)))return e;
    if((e=private_storage((const void *)(uintptr_t)storage,t->pool->size))||
       (e=private_storage((const void *)begin,end-begin)))return e;
    if(range_overlap(p,span,canonical,t->size))return EINVAL;
    if((e=gpu_pool_access(t->pool,t->offset,t->size,"TextureRead/WriteTexels"))||
       (e=texel_alias_idle(t,p,span,canonical,t->size))||
       (e=texel_destination_arena(to_texture?canonical:p,to_texture?t->size:span)))return e;
    /* Borrow a fully covering known pool instead of reimporting its pages.
     * Disjoint windows in the same original pool are intentionally legal. */
    struct object *linear=NULL;size_t offset=0;
    for(struct object *pool=objects;pool;pool=pool->next)if(pool->kind==POOL&&pool->storage&&!pool->external&&pool->device==t->device){
        uint64_t address;if((e=texel_cpu_address(pool,0,pool->size,&address)))return e;
        if(p>=address&&p-address<=pool->size&&span<=pool->size-(size_t)(p-address)){
            if(!(pool->flags&NVN_MEMORY_POOL_FLAGS_CPU_CACHED))return ENOTSUP;
            if((e=gpu_pool_access(pool,(size_t)(p-address),span,"TextureRead/WriteTexels caller")))return e;
            linear=pool;offset=(size_t)(p-address);break;
        }
    }
    if(t->refs==SIZE_MAX||t->device->refs==SIZE_MAX||(linear&&linear->refs==SIZE_MAX))return EOVERFLOW;
    struct texel_transfer_owner *owner=calloc(1,sizeof(*owner));if(!owner)return ENOMEM;
    owner->texture=t;owner->device=t->device;owner->linear_pool=linear;t->refs++;t->device->refs++;if(linear)linear->refs++;
    int pending=0;nn_bo *linear_bo=linear?linear->bo:NULL;size_t linear_owner=linear?linear->size:end-begin;
    if(!linear){offset=p-begin;e=nn_bo_import(t->device->gpu,(void *)begin,end-begin,&owner->imported);linear_bo=owner->imported;}
    const struct nn_cs_image *kernel=t->device->block_copy;size_t bytes=0;
    if(!e)e=nn_bo_alloc(t->device->gpu,kernel->code_bytes,&owner->code);
    if(!e)e=nn_bo_alloc(t->device->gpu,64,&owner->uniform);
    struct nn_block_copy_binding binding;
    if(!e){struct nn_block_copy_range c={t->bo,0,t->pool->size,t->offset},b={linear_bo,0,linear_owner,offset},u={owner->uniform,0,nn_bo_size(owner->uniform),0};
        e=nn_block_copy_bind(t->device->gpu,kernel,t->source_layout,&plan,&c,&b,&u,NN_BLOCK_COPY_DISJOINT_PROVEN,&binding);}
    if(!e){void *code=nn_bo_map(owner->code),*uniform=nn_bo_map(owner->uniform);
        if(!code||!uniform)e=ENOTSUP;
        else{memcpy(code,kernel->code,kernel->code_bytes);memcpy(uniform,&binding.args,sizeof(binding.args));}}
    if(!e)e=nn_cs_encode(t->device->gpu,owner->code,kernel,binding.ubos,binding.ssbos,binding.groups,&owner->commands,&bytes);
    if(!e)e=nn_bo_cache(t->bo,t->offset,t->size,0);
    if(!e)e=nn_bo_cache(linear_bo,offset,span,0); /* preserve masked neighbors */
    if(!e)e=nn_bo_cache(owner->code,0,kernel->code_bytes,0);
    if(!e)e=nn_bo_cache(owner->uniform,0,64,0);
    if(!e)e=nn_bo_cache(owner->commands,0,nn_bo_size(owner->commands),0);
    if(!e)e=nn_queue_create(t->device->gpu,&owner->queue);
    nn_bo *resources[4]={t->bo};size_t resource_count=1;
    if(linear_bo!=t->bo)resources[resource_count++]=linear_bo;
    resources[resource_count++]=owner->code;resources[resource_count++]=owner->uniform;
    uint32_t timestamp=0;
    if(!e){pending=1;e=nn_queue_submit(owner->queue,owner->commands,0,bytes,resources,resource_count,&timestamp);}
    if(!e)e=nn_queue_finish(owner->queue,1000);
    if(!e){pending=0;e=nn_bo_cache(to_texture?t->bo:linear_bo,to_texture?t->offset:offset,to_texture?t->size:span,1);}
    if(e&&pending){owner->next=retained_texel_transfers;retained_texel_transfers=owner;return e;}
    int cleanup=texel_transfer_cleanup(owner);
    if(cleanup){owner->next=retained_texel_transfers;retained_texel_transfers=owner;}
    return e?e:cleanup;
#endif
}
static void api_nvnTextureWriteTexels(const NVNtexture *key,const NVNtextureView *v,const NVNcopyRegion *r,const void *p)
{struct object *t=find(key,TEXTURE);if(t)error(canonical_texel_transfer(t,v,r,(void *)p,0,0,1));}
static void api_nvnTextureWriteTexelsStrided(const NVNtexture *key,const NVNtextureView *v,const NVNcopyRegion *r,const void *p,ptrdiff_t row,ptrdiff_t image)
{struct object *t=find(key,TEXTURE);if(t)error(canonical_texel_transfer(t,v,r,(void *)p,row,image,1));}
