/* SPDX-License-Identifier: MIT
 * Included after record_graphics_draw/record_compute_dispatch. Argument
 * buffers are retained by the resulting handle, never CPU-read at record. */
static int draw_argument(struct object *command, NVNbufferAddress address,
                         size_t minimum, size_t alignment, int remainder,
                         struct nn_gfx_buffer *out)
{
    if (!address || address%alignment || !minimum) { error(EINVAL); return 0; }
    struct object *pool=address_pool(command->device,address,minimum);
    if (!pool) { error(EINVAL); return 0; }
    size_t offset=address-nn_bo_address(pool->bo), bytes=remainder ? pool->size-offset : minimum;
    if (gpu_pool_access(pool,offset,bytes,"NVN draw arguments")) return 0;
    *out=(struct nn_gfx_buffer){pool->bo,offset,bytes}; return 1;
}
static void api_nvnCommandBufferDrawArrays(NVNcommandBuffer *key, NVNdrawPrimitive primitive, int first, int count)
{
    if (primitive==NVN_DRAW_PRIMITIVE_TRIANGLES && first==0 && count==3) {
        record_graphics_draw(key,primitive,first,count,NULL); return;
    }
    struct nn_gfx_draw draw={.mode=NN_GFX_DRAW_DIRECT,.primitive=primitive,
        .first=first,.count=count,.instances=1};
    record_graphics_draw(key,primitive,first,count,&draw);
}
static void api_nvnCommandBufferDrawArraysInstanced(NVNcommandBuffer *key, NVNdrawPrimitive primitive,
                                                   int first, int count, int base_instance, int instances)
{
    if (base_instance<0 || instances<0) { error(EINVAL); return; }
    struct nn_gfx_draw draw={.mode=NN_GFX_DRAW_DIRECT,.primitive=primitive,
        .first=first,.count=count,.instances=instances,.base_instance=base_instance};
    record_graphics_draw(key,primitive,first,count,&draw);
}
static void api_nvnCommandBufferDrawElementsInstanced(NVNcommandBuffer *key, NVNdrawPrimitive primitive,
                   NVNindexType type, int count, NVNbufferAddress index, int base_vertex, int base_instance, int instances)
{
    struct object *o=recording_command(key); if (!o) return;
    if (type<0 || type>NVN_INDEX_TYPE_UNSIGNED_INT || count<0 || base_instance<0 || instances<0) { error(EINVAL); return; }
    if (!count || !instances) return;
    struct nn_gfx_draw draw={.mode=NN_GFX_DRAW_DIRECT,.primitive=primitive,.indexed=1,.index_type=type,
        .count=count,.instances=instances,.base_vertex=base_vertex,.base_instance=base_instance};
    if (!draw_argument(o,index,(size_t)count<<type,1u<<type,1,&draw.index)) return;
    record_graphics_draw(key,primitive,0,count,&draw);
}
static void api_nvnCommandBufferDrawElementsBaseVertex(NVNcommandBuffer *key, NVNdrawPrimitive primitive,
                                    NVNindexType type, int count, NVNbufferAddress index, int base_vertex)
{ api_nvnCommandBufferDrawElementsInstanced(key,primitive,type,count,index,base_vertex,0,1); }
static void api_nvnCommandBufferDrawElements(NVNcommandBuffer *key, NVNdrawPrimitive primitive,
                                    NVNindexType type, int count, NVNbufferAddress index)
{ api_nvnCommandBufferDrawElementsInstanced(key,primitive,type,count,index,0,0,1); }
static void record_indirect_draw(NVNcommandBuffer *key, NVNdrawPrimitive primitive, int indexed,
                    NVNindexType type, NVNbufferAddress index, NVNbufferAddress indirect,
                    NVNbufferAddress count_address, int max_draws, ptrdiff_t stride)
{
    struct object *o=recording_command(key); if (!o) return;
    uint32_t record_bytes=indexed ? 20u : 16u;
    if (type<0 || type>NVN_INDEX_TYPE_UNSIGNED_INT || max_draws<0 ||
        stride<(ptrdiff_t)record_bytes || stride>UINT32_MAX || stride%4) { error(EINVAL); return; }
    if (!max_draws) return;
    uint64_t bytes=(uint64_t)(max_draws-1)*(uint64_t)stride+record_bytes;
    if (bytes>SIZE_MAX) { error(EOVERFLOW); return; }
    struct nn_gfx_draw draw={.mode=count_address ? NN_GFX_DRAW_INDIRECT_COUNT : NN_GFX_DRAW_INDIRECT,
        .primitive=primitive,.indexed=indexed,.index_type=type,.max_draws=max_draws,.stride=stride};
    if (indexed && !draw_argument(o,index,1u<<type,1u<<type,1,&draw.index)) return;
    if (!draw_argument(o,indirect,(size_t)bytes,4,0,&draw.indirect)) return;
    if (count_address && !draw_argument(o,count_address,4,4,0,&draw.indirect_count)) return;
    record_graphics_draw(key,primitive,0,1,&draw);
}
static void api_nvnCommandBufferDrawArraysIndirect(NVNcommandBuffer *key, NVNdrawPrimitive primitive, NVNbufferAddress indirect)
{ record_indirect_draw(key,primitive,0,NVN_INDEX_TYPE_UNSIGNED_BYTE,0,indirect,0,1,16); }
static void api_nvnCommandBufferDrawElementsIndirect(NVNcommandBuffer *key, NVNdrawPrimitive primitive,
                                       NVNindexType type, NVNbufferAddress index, NVNbufferAddress indirect)
{ record_indirect_draw(key,primitive,1,type,index,indirect,0,1,20); }
static void api_nvnCommandBufferMultiDrawArraysIndirectCount(NVNcommandBuffer *key, NVNdrawPrimitive primitive,
                     NVNbufferAddress indirect, NVNbufferAddress count, int max_draws, ptrdiff_t stride)
{
    if (!count) { error(EINVAL); return; }
    record_indirect_draw(key,primitive,0,NVN_INDEX_TYPE_UNSIGNED_BYTE,0,indirect,count,max_draws,stride);
}
static void api_nvnCommandBufferMultiDrawElementsIndirectCount(NVNcommandBuffer *key, NVNdrawPrimitive primitive,
                     NVNindexType type, NVNbufferAddress index, NVNbufferAddress indirect,
                     NVNbufferAddress count, int max_draws, ptrdiff_t stride)
{
    if (!count) { error(EINVAL); return; }
    record_indirect_draw(key,primitive,1,type,index,indirect,count,max_draws,stride);
}
static void api_nvnCommandBufferDispatchCompute(NVNcommandBuffer *key, int x, int y, int z)
{ record_compute_dispatch(key,x,y,z,0); }
static void api_nvnCommandBufferDispatchComputeIndirect(NVNcommandBuffer *key, NVNbufferAddress indirect)
{
    if (!indirect || indirect%4) { error(EINVAL); return; }
    record_compute_dispatch(key,1,1,1,indirect);
}
/* Bind state is meaningful independently of whether a shader uses that slot.
 * Current offline shader profiles have no image mappings; image-using shaders
 * are still rejected by registration, not silently dispatched without images. */
static void api_nvnCommandBufferBindImages(NVNcommandBuffer *key, NVNshaderStage stage,
                                          int first, int count, const NVNimageHandle *images)
{
    struct object *o=recording_command(key); if (!o) return;
    if (stage<0 || stage>NVN_SHADER_STAGE_COMPUTE || first<0 || first>32 || count<0 ||
        count>32-first || (count && !images)) { error(EINVAL); return; }
    NVNimageHandle values[32];
    for (int i=0;i<count;i++) {
        memcpy(values+i,images+i,sizeof(*values));
        if (values[i] && (values[i]>UINT32_MAX || !descriptor_id((int)values[i],TPOOL))) { error(EINVAL); return; }
    }
    struct nn_command_binding deltas[32]={{0}};for(int i=0;i<count;i++)deltas[i].guest=values[i];
    if(count&&command_capture_bindings(o,NN_COMMAND_IMAGE,(unsigned)stage,(unsigned)first,(unsigned)count,deltas))return;
    if (count) memcpy(o->record->images[stage]+first,values,(size_t)count*sizeof(*values));
}
static void api_nvnCommandBufferBindImage(NVNcommandBuffer *key, NVNshaderStage stage, int index, NVNimageHandle image)
{ api_nvnCommandBufferBindImages(key,stage,index,1,&image); }
/* The finite compiler profiles accept only zero native private-memory size.
 * Preserve the caller's scratch binding and ownership for those programs;
 * this does not enable shaders requiring unimplemented native scratch state. */
static void api_nvnCommandBufferSetShaderScratchMemory(NVNcommandBuffer *key, const NVNmemoryPool *pool,
                                                      ptrdiff_t offset, size_t size)
{
    struct object *o=find(key,COMMAND); if (!o) return;
    struct bound proposed={0};
    if (pool) {
        struct object *p=find(pool,POOL); if (!p) return;
        if (p->device!=o->device || offset<0 || (size_t)offset>p->size || size>p->size-(size_t)offset) { error(EINVAL); return; }
        if (gpu_pool_access(p,(size_t)offset,size,"nvnCommandBufferSetShaderScratchMemory")) return;
        proposed=(struct bound){p,(size_t)offset,size,nn_bo_address(p->bo)+(size_t)offset};
    } else if (offset || size) { error(EINVAL); return; }
    if(o->record->active&&command_capture_unknown(o))return;
    if (o->record->shader_scratch.pool) o->record->shader_scratch.pool->refs--;
    o->record->shader_scratch=proposed;
    if (proposed.pool) proposed.pool->refs++;
}
