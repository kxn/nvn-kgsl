/* Canonical CPU/GPU/public addresses remain original. Only this private
 * submit-time graphics job receives a fresh native storage view. Each replay
 * has its own conversion version and snapshots the registration then visible.
 * CopyCommands copies only selectors/state; immutable jobs contain no live
 * metadata pointers. No descriptor or caller source bytes are CPU converted. */
static struct nn_tex_native_layout bridge_target_layout(const struct nn_gfx_image *i)
{ return (struct nn_tex_native_layout){i->width,i->height,i->target_pitch,i->target_layer_bytes,i->target_bytes,i->target_alignment}; }
static struct nn_tex_native_layout bridge_sample_layout(const struct nn_gfx_image *i)
{ return (struct nn_tex_native_layout){i->texture_width,i->texture_height,i->texture_pitch,i->texture_layer_bytes,i->texture_bytes,i->texture_alignment}; }
static int bridge_overlap(uint64_t a,size_t an,uint64_t b,size_t bn)
{ return an>UINT64_MAX-a || bn>UINT64_MAX-b || (a<b+bn && b<a+an); }
/* Match the KGSL ADDR import's top-byte-ignore normalization. Normalize the
 * base BEFORE checked offset/range addition, never compare tagged CPU aliases. */
static int bridge_cpu_range(const struct object *pool,size_t offset,size_t bytes,uint64_t *address)
{
    uintptr_t base=(uintptr_t)pool->storage;
#if UINTPTR_MAX > UINT32_MAX
    base&=UINT64_C(0x00ffffffffffffff);
#endif
    if(!base || offset>UINTPTR_MAX-base || bytes>UINTPTR_MAX-base-offset)return EOVERFLOW;
    *address=(uint64_t)(base+offset);return 0;
}
static int bridge_target_arena_alias(const struct object *t)
{
    for(struct object *o=objects;o;o=o->next)if(o->kind==COMMAND && o->record->pool){
        const struct object *p=o->record->pool;
        if(t->pool->bo==p->bo && bridge_overlap(t->offset,t->size,o->record->offset,o->record->capacity))return ENOTSUP;
        if(t->pool->storage && p->storage){
            uint64_t a=0,b=0;int e=bridge_cpu_range(t->pool,t->offset,t->size,&a);if(e)return e;
            e=bridge_cpu_range(p,o->record->offset,o->record->capacity,&b);if(e)return e;
            if(bridge_overlap(a,t->size,b,o->record->capacity))return ENOTSUP;
        }
    }
    return 0;
}
static int bridge_feedback(const struct object *target,const struct descriptor_entry *sample)
{
    if(!target)return 0;
    if(target->bo==sample->pool->bo && bridge_overlap(target->offset,target->size,sample->offset,sample->size))return ENOTSUP;
    if(target->pool->storage && sample->pool->storage){
        uint64_t a=0,b=0;int e=bridge_cpu_range(target->pool,target->offset,target->size,&a);if(e)return e;
        e=bridge_cpu_range(sample->pool,sample->offset,sample->size,&b);if(e)return e;
        if(bridge_overlap(a,target->size,b,sample->size))return ENOTSUP;
    }
    return 0;
}
static void bridge_nodes_finish(struct object *device,struct job **nodes,unsigned count,struct job **owned)
{
    for(unsigned i=0;i<count;i++)if(nodes[i]){
        if(nodes[i]->bo){nodes[i]->next=*owned;*owned=nodes[i];device->refs++;}else free(nodes[i]);
    }
}
/* Resolve current registered sampler values at enqueue, not at record time.
 * Preflight all handles before private parent allocation/cache/submission.
 * A metadata-valid CPU sampler need not be a native-capable GPU sampler. */
static int sampler_draw_preflight(const struct late_draw *draw)
{
    if(draw->generic_pipeline || draw->bridge_clear)return 0;
    const struct nn_gfx_image *image=draw->program->graphics->image;
    if(image->profile!=NN_GFX_TEXTURE_2D_NORMALIZED)return 0;
    const struct object *sp=draw->sampler_pool;
    if(!sp || draw->sampler_id>=sp->slot_count || !sp->slots[draw->sampler_id].valid)return EINVAL;
    const struct nn_sampler_state state=native_sampler_state(&sp->slots[draw->sampler_id].snapshot.sampler);
    return nn_sampler_state_validate(&state);
}
static int bridge_materialize_clear(struct object *,const struct late_draw *,nn_bo *,const struct nn_tex_native_layout *,nn_bo **,size_t *);
static int materialize_draw_bridge(struct object *device,const struct late_draw *draw,
 nn_bo **bo,size_t *bytes,nn_bo **sampled,struct job **owned)
{
    *bo=NULL;*bytes=0;*sampled=NULL;*owned=NULL;
    if(gpu_late_draw_access(draw,"materialize_draw"))return ENOTSUP;
    const struct nn_gfx_image *image=draw->bridge_clear?NULL:draw->program->graphics->image;
    if(!draw->bridge_clear && (!graphics_profile(image)||draw->state.window_origin>1||draw->state.depth_mode>1))return ENOTSUP;
    const int textured=image && image->profile==NN_GFX_TEXTURE_2D_NORMALIZED;
    const struct descriptor_entry *t=NULL,*s=NULL;const struct texture_desc *layout=NULL;
    const struct sampler_desc *sampler=NULL;
    if(textured){
        struct object *tp=draw->texture_pool,*sp=draw->sampler_pool;
        if(!tp||!sp||draw->texture_id>=tp->slot_count||draw->sampler_id>=sp->slot_count)return EINVAL;
        t=tp->slots+draw->texture_id;s=sp->slots+draw->sampler_id;
        if(!t->valid||!t->pool||!s->valid)return EINVAL;
        layout=&t->snapshot.texture;sampler=&s->snapshot.sampler;
        if((t->source_layout?nn_tex_bridge_source_validate(t->source_layout):!texture_profile(layout)) ||
           layout->width!=(int)image->texture_width || layout->height!=(int)image->texture_height ||
           (!t->source_layout&&(layout->stride!=(ptrdiff_t)image->texture_pitch||t->size!=image->texture_bytes)) ||
           !sampler_profile(sampler))return ENOTSUP;
        struct texture_slot td={0x54584e4eu,(uint32_t)layout->format,(uint32_t)layout->width,(uint32_t)layout->height,
            (uint32_t)layout->stride,(uint32_t)t->size,nn_bo_address(t->pool->bo)+t->offset};
        struct sampler_slot sd={0x534e4e53u,(uint32_t)sampler->min,(uint32_t)sampler->mag,
            (uint32_t)sampler->wrap[0]|((uint32_t)sampler->wrap[1]<<8)|((uint32_t)sampler->wrap[2]<<16),
            (uint32_t)sampler->compare|((uint32_t)sampler->compare_func<<8),sampler->lod_min,sampler->lod_max,sampler->lod_bias};
        if(memcmp((char*)nn_bo_map(tp->pool->bo)+tp->offset+(size_t)draw->texture_id*DESCRIPTOR_BYTES,&td,sizeof(td)) ||
           memcmp((char*)nn_bo_map(sp->pool->bo)+sp->offset+(size_t)draw->sampler_id*DESCRIPTOR_BYTES,&sd,sizeof(sd)))return EINVAL;
        int e=bridge_feedback(draw->target,t);if(e)return e;
    }
    if(draw->target->source_layout){int e=bridge_target_arena_alias(draw->target);if(e)return e;}
    /* Reserve owner metadata before allocating GPU objects: even free failure
     * during encode rollback must have a durable place on the retire ledger. */
    struct job *nodes[12]={0};
    for(unsigned i=0;i<12;i++)if(!(nodes[i]=calloc(1,sizeof(*nodes[i])))){
        for(unsigned j=0;j<i;j++)free(nodes[j]);
        return ENOMEM;
    }
    struct nn_tex_shadow target={0},sample={0};
    struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT]={0};int e=0;
    if(draw->bridge_clear){
        const struct nn_tex_native_layout *n=&device->texture_bridge->clear_layout;
        e=nn_tex_shadow_create(device->gpu,device->texture_bridge,draw->target->bo,draw->target->offset,
            draw->target->size,draw->target->source_layout,n,&target);if(e)goto done;
        e=bridge_materialize_clear(device,draw,target.bos[0],n,&nodes[10]->bo,bytes);if(e)goto done;
        const struct nn_tex_shadow *input=&target;
        e=nn_tex_shadow_chain(device->gpu,&input,1,nodes[10]->bo,*bytes,&target,&nodes[11]->bo,bytes);if(e)goto done;
        *bo=nodes[11]->bo;nodes[11]->bo=NULL;goto done;
    }
    buffers[NN_GFX_VS_CODE]=(struct nn_gfx_buffer){draw->program->bo,0,image->vs_code_bytes};
    buffers[NN_GFX_FS_CODE]=(struct nn_gfx_buffer){draw->program->fs_bo,0,image->fs_code_bytes};
    buffers[NN_GFX_TARGET]=(struct nn_gfx_buffer){draw->target->bo,draw->target->offset,draw->target->size};
    buffers[NN_GFX_VERTICES]=(struct nn_gfx_buffer){draw->vertex.pool->bo,draw->vertex.offset,draw->vertex.size};
    buffers[NN_GFX_MATRIX]=(struct nn_gfx_buffer){draw->matrix.pool->bo,draw->matrix.offset,draw->matrix.size};
    if(draw->target->source_layout){
        struct nn_tex_native_layout n=bridge_target_layout(image);
        e=nn_tex_shadow_create(device->gpu,device->texture_bridge,draw->target->bo,draw->target->offset,
            draw->target->size,draw->target->source_layout,&n,&target);if(e)goto done;
        buffers[NN_GFX_TARGET]=(struct nn_gfx_buffer){target.bos[NN_TEX_SHADOW_STORAGE],0,n.bytes};
    }
    if(textured){
        buffers[NN_GFX_FS_UNIFORM]=(struct nn_gfx_buffer){draw->fragment.pool->bo,draw->fragment.offset,draw->fragment.size};
        buffers[NN_GFX_TEXTURE]=(struct nn_gfx_buffer){t->pool->bo,t->offset,t->size};
        if(t->source_layout){
            struct nn_tex_native_layout n=bridge_sample_layout(image);
            e=nn_tex_shadow_create(device->gpu,device->texture_bridge,t->pool->bo,t->offset,t->size,t->source_layout,&n,&sample);if(e)goto done;
            buffers[NN_GFX_TEXTURE]=(struct nn_gfx_buffer){sample.bos[NN_TEX_SHADOW_STORAGE],0,n.bytes};
        }
        const struct nn_sampler_state state=native_sampler_state(sampler);
        e=nn_gfx_encode_sampled_render(device->gpu,image,buffers,&draw->state,draw->draw_override?&draw->draw:NULL,
            &draw->render,&state,&nodes[10]->bo,bytes);if(e)goto done;
        *sampled=t->pool->bo;
    }else{
        e=draw->draw_override?nn_gfx_encode_draw_render(device->gpu,image,buffers,&draw->state,&draw->draw,&draw->render,&nodes[10]->bo,bytes):
            nn_gfx_encode_render(device->gpu,image,buffers,&draw->state,&draw->render,&nodes[10]->bo,bytes);if(e)goto done;
    }
    if(target.bos[0]||sample.bos[0]){
        const struct nn_tex_shadow *inputs[2];size_t count=0;
        if(target.bos[0])inputs[count++]=&target;
        if(sample.bos[0])inputs[count++]=&sample;
        e=nn_tex_shadow_chain(device->gpu,inputs,count,nodes[10]->bo,*bytes,target.bos[0]?&target:NULL,&nodes[11]->bo,bytes);if(e)goto done;
        *bo=nodes[11]->bo;nodes[11]->bo=NULL;
    }else{*bo=nodes[10]->bo;nodes[10]->bo=NULL;}
 done:
    for(unsigned i=0;i<5;i++){nodes[i]->bo=target.bos[i];nodes[i+5]->bo=sample.bos[i];}
    bridge_nodes_finish(device,nodes,12,owned);return e;
}

/* Preserve the existing materializer contract for original LINEAR consumers.
 * New auxiliary owners are explicit only at the bridge-aware submit seam. */
static int draw_needs_bridge(const struct late_draw *draw)
{
    if(draw->bridge_clear || draw->target->source_layout)return 1;
    return draw->texture_pool && draw->texture_id<draw->texture_pool->slot_count &&
        draw->texture_pool->slots[draw->texture_id].source_layout;
}
static size_t draw_ref_capacity(const struct late_draw *draw)
{
    if(draw->generic_pipeline) {
        /* Bound only resources consumed by this immutable snapshot. The public
         * queue control budget is not 128 potential slots per Draw. Shared BOs
         * may deduplicate further in queue_reference; this remains an upper
         * bound including every fresh code/C1, child, shadow and wrapper BO. */
        const struct pipeline_draw_snapshot *p=&draw->pipeline;
        const struct nn_pipeline_shader_recipe *recipe=p->recipe;
        uint32_t streams=0;size_t n=1u+2u+1u; /* target, VS/FS code, child */
        for(size_t i=0;i<recipe->input_count;i++)
            streams|=1u<<p->native.vertex.attributes[recipe->inputs[i].location].stream;
        for(unsigned i=0;i<32;i++)if(streams&(1u<<i))n++;
        for(unsigned stage=0;stage<2;stage++)
            if(p->stages[stage]->source_bytes>p->stages[stage]->code_bytes)n++; /* immutable C1 */
        for(size_t i=0;i<recipe->patch_count;i++)
            if(recipe->patches[i].role==NN_PIPELINE_UBO||recipe->patches[i].role==NN_PIPELINE_DYNAMIC_UBO)n++;
        if(p->native.storage_types)for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<16;i++)if(p->native.storage_types->mask[stage]&(1u<<i))n++;
        n+=p->native.draw.indexed?1u:0u;
        n+=p->native.draw.mode!=NN_GFX_DRAW_DIRECT?1u:0u;
        n+=p->native.draw.mode==NN_GFX_DRAW_INDIRECT_COUNT?1u:0u;
        if(draw->target&&draw->target->source_layout)n+=NN_TEX_SHADOW_BO_COUNT+1u;
        return n;
    }
    if(!draw_needs_bridge(draw))return 2; /* preserve legacy control budget */
    size_t shadows=draw->target->source_layout?1u:0u;
    if(!draw->bridge_clear && draw->texture_pool && draw->texture_id<draw->texture_pool->slot_count &&
       draw->texture_pool->slots[draw->texture_id].source_layout)shadows++;
    /* Five private BOs per shadow, actual graphics/clear child + wrapper,
     * and a sampled canonical BO when the draw actually samples a texture. */
    return shadows*NN_TEX_SHADOW_BO_COUNT+2u+(!draw->bridge_clear && draw->texture_pool?1u:0u);
}
static int materialize_draw(struct object *device,const struct late_draw *draw,nn_bo **bo,size_t *bytes,nn_bo **sampled)
{
    if(draw_needs_bridge(draw)){*bo=NULL;*bytes=0;*sampled=NULL;return ENOTSUP;}
    struct job *owned=NULL;int e=materialize_draw_bridge(device,draw,bo,bytes,sampled,&owned);
    if(owned){retire_jobs(device,owned);return e?e:EPROTO;}return e;
}
