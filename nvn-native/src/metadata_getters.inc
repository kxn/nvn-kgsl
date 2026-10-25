/* SPDX-License-Identifier: MIT.
 * Public metadata queries over the same owners/inline values consumed by
 * native recording and allocation. These never allocate, submit, wait, mutate
 * resource metadata or invent support for a new GPU view/format/state.
 * Public PFN ABI: pinned reconstructed NVN53.105 C/C++ declarations; current
 * original SDK getter ABI/default-presence corroboration already documented
 * in abi-recovery-sdk-research, queue-sdk-next and texture-view-sdk-research.
 */
#define META_SCALAR(name, public_type, kind_value, result_type, expression) \
static result_type api_##name(const public_type *key) { \
    struct object *o=find(key,kind_value); return o?(expression):(result_type)0; }
META_SCALAR(nvnDeviceBuilderGetFlags,NVNdeviceBuilder,DB,NVNdeviceFlagBits,(NVNdeviceFlagBits)o->flags)
META_SCALAR(nvnMemoryPoolBuilderGetMemory,NVNmemoryPoolBuilder,PB,void *,o->storage)
META_SCALAR(nvnMemoryPoolBuilderGetSize,NVNmemoryPoolBuilder,PB,size_t,o->size)
META_SCALAR(nvnMemoryPoolBuilderGetFlags,NVNmemoryPoolBuilder,PB,NVNmemoryPoolFlags,(NVNmemoryPoolFlags)o->flags)
META_SCALAR(nvnMemoryPoolGetSize,NVNmemoryPool,POOL,size_t,o->size)
META_SCALAR(nvnMemoryPoolGetFlags,NVNmemoryPool,POOL,NVNmemoryPoolFlags,(NVNmemoryPoolFlags)o->flags)
META_SCALAR(nvnBufferBuilderGetMemoryOffset,NVNbufferBuilder,BB,ptrdiff_t,(ptrdiff_t)o->offset)
META_SCALAR(nvnBufferBuilderGetSize,NVNbufferBuilder,BB,size_t,o->size)
META_SCALAR(nvnBufferGetMemoryOffset,NVNbuffer,BUFFER,ptrdiff_t,(ptrdiff_t)o->offset)
META_SCALAR(nvnBufferGetSize,NVNbuffer,BUFFER,size_t,o->size)
META_SCALAR(nvnTextureBuilderGetMemoryOffset,NVNtextureBuilder,TB,ptrdiff_t,(ptrdiff_t)o->offset)
META_SCALAR(nvnTextureGetMemoryOffset,NVNtexture,TEXTURE,ptrdiff_t,(ptrdiff_t)o->offset)
META_SCALAR(nvnQueueBuilderGetFlags,NVNqueueBuilder,QB,int,o->flags)
META_SCALAR(nvnQueueBuilderGetMemorySize,NVNqueueBuilder,QB,int,(int)o->queue_config.bytes)
META_SCALAR(nvnQueueBuilderGetMemory,NVNqueueBuilder,QB,void *,o->queue_config.memory)
META_SCALAR(nvnCommandBufferGetMemoryCallback,NVNcommandBuffer,COMMAND,PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC,o->record->memory_callback)
META_SCALAR(nvnCommandBufferGetMemoryCallbackData,NVNcommandBuffer,COMMAND,void *,o->record->memory_callback_user)
#undef META_SCALAR
/* Builder queries return the stored public opaque key, as do GetDevice and
 * EventBuilderGetStorage. They neither dereference nor validate that key as a
 * live resource. Actual Initialize/consumers require the independent internal
 * owner node, cleared on destruction to prevent allocator-address ABA.
 * This does not establish an NVN guarantee for invalid resource lifetimes.
 * Initialized Buffer/Texture retain their pool strongly. */
static NVNmemoryPool *metadata_pool_key(const struct object *o)
{
    if(!o)return NULL;
    if(o->kind==BB || o->kind==TB)return (NVNmemoryPool *)o->builder_pool_key;
    if(!o->pool){error(EINVAL);return NULL;}
    struct object *p=live_weak(o->pool,POOL);
    if(!p){error(EINVAL);return NULL;}
    return (NVNmemoryPool *)p->key;
}
#define META_POOL(name, public_type, kind_value) \
static NVNmemoryPool *api_##name(const public_type *key) \
{ return metadata_pool_key(find(key,kind_value)); }
META_POOL(nvnBufferBuilderGetMemoryPool,NVNbufferBuilder,BB)
META_POOL(nvnBufferGetMemoryPool,NVNbuffer,BUFFER)
META_POOL(nvnTextureBuilderGetMemoryPool,NVNtextureBuilder,TB)
META_POOL(nvnTextureGetMemoryPool,NVNtexture,TEXTURE)
#undef META_POOL
/* The query exposes explicit fields, not the effective native default
 * budget. SDK defaults leave the exposed field zero and presence false;
 * native allocation independently uses queue_config's effective defaults.
 * Out writes are unconditional even for false, as in the public getter ABI. */
#define META_BUDGET(name, field, bit) \
static NVNboolean api_##name(const NVNqueueBuilder *key,int *value) { \
    struct object *o=find(key,QB);if(!o)return 0; \
    if(!value){error(EINVAL);return 0;} \
    unsigned present=(o->queue_config.set&(bit))!=0; \
    *value=present?(int)o->queue_config.field:0;return (NVNboolean)present; }
META_BUDGET(nvnQueueBuilderGetCommandMemorySize,command,1)
META_BUDGET(nvnQueueBuilderGetComputeMemorySize,compute,2)
META_BUDGET(nvnQueueBuilderGetControlMemorySize,control,4)
META_BUDGET(nvnQueueBuilderGetCommandFlushThreshold,flush,8)
#undef META_BUDGET
static NVNboolean api_nvnTextureViewGetLevels(const NVNtextureView *key,int *base,int *count)
{
    struct resource_view v;int e=resource_view_read(key,&v);
    if(!key||!base||!count||e){error(e?e:EINVAL);return 0;}
    *base=v.base_level;*count=v.level_count;return v.levels;
}
static NVNboolean api_nvnTextureViewGetFormat(const NVNtextureView *key,NVNformat *format)
{
    struct resource_view v;int e=resource_view_read(key,&v);
    if(!key||!format||e){error(e?e:EINVAL);return 0;}
    *format=(NVNformat)v.format_value;return v.format;
}
static NVNboolean api_nvnTextureViewGetTarget(const NVNtextureView *key,NVNtextureTarget *target)
{
    struct resource_view v;int e=resource_view_read(key,&v);
    if(!key||!target||e){error(e?e:EINVAL);return 0;}
    *target=(NVNtextureTarget)v.target_value;return v.target;
}
static void api_nvnVertexAttribStateGetFormat(const NVNvertexAttribState *key,NVNformat *format,ptrdiff_t *offset)
{
    if(!key||!format||!offset){error(EINVAL);return;}
    uint32_t value=attribute_read(key);*format=(NVNformat)(value&255u);*offset=(ptrdiff_t)((value>>8)&65535u);
}
static int api_nvnVertexAttribStateGetStreamIndex(const NVNvertexAttribState *key)
{if(!key){error(EINVAL);return 0;}return (int)(attribute_read(key)>>24);}
static ptrdiff_t api_nvnVertexStreamStateGetStride(const NVNvertexStreamState *key)
{if(!key){error(EINVAL);return 0;}struct vertex_stream v;memcpy(&v,key,sizeof(v));return (ptrdiff_t)v.stride;}
static int api_nvnVertexStreamStateGetDivisor(const NVNvertexStreamState *key)
{if(!key){error(EINVAL);return 0;}struct vertex_stream v;memcpy(&v,key,sizeof(v));return (int)v.divisor;}
