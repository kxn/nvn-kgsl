/* SPDX-License-Identifier: MIT. Public identities are opaque borrowed keys;
 * querying never dereferences them or pins an owner. Actual initialization
 * still requires the internal live owner and matching resource domain. */
#define BUILDER_DEVICE_GET(name,public_type,kind_value) \
static const NVNdevice *api_##name(const public_type *key) \
{struct object *o=find(key,kind_value);return o?o->builder_device_key:NULL;}
BUILDER_DEVICE_GET(nvnQueueBuilderGetDevice,NVNqueueBuilder,QB)
BUILDER_DEVICE_GET(nvnMemoryPoolBuilderGetDevice,NVNmemoryPoolBuilder,PB)
BUILDER_DEVICE_GET(nvnBufferBuilderGetDevice,NVNbufferBuilder,BB)
BUILDER_DEVICE_GET(nvnTextureBuilderGetDevice,NVNtextureBuilder,TB)
BUILDER_DEVICE_GET(nvnWindowBuilderGetDevice,NVNwindowBuilder,WB)
#undef BUILDER_DEVICE_GET
static const NVNmemoryPool *api_nvnEventBuilderGetStorage(const NVNeventBuilder *key,int64_t *offset)
{
 struct object *o=find(key,EVENT_BUILDER);if(!o)return NULL;
 if(!offset){error(EINVAL);return NULL;}*offset=(int64_t)o->offset;return o->builder_pool_key;
}
static const NVNmemoryPool *api_nvnEventBuilderGetMemoryPool(const NVNeventBuilder *key)
{struct object *o=find(key,EVENT_BUILDER);return o?o->builder_pool_key:NULL;}
static int64_t api_nvnEventBuilderGetMemoryOffset(const NVNeventBuilder *key)
{struct object *o=find(key,EVENT_BUILDER);return o?(int64_t)o->offset:0;}
static const NVNmemoryPool *api_nvnEventGetMemoryPool(const NVNevent *key)
{struct object *o=find(key,EVENT);return o?(const NVNmemoryPool *)o->pool->key:NULL;}
static int64_t api_nvnEventGetMemoryOffset(const NVNevent *key)
{struct object *o=find(key,EVENT);return o?(int64_t)o->offset:0;}
static int api_nvnWindowBuilderGetNumTextures(const NVNwindowBuilder *key)
{struct object *o=find(key,WB);return o?o->window_builder.count:0;}
static const NVNtexture *api_nvnWindowBuilderGetTexture(const NVNwindowBuilder *key,int index)
{
 struct object *o=find(key,WB);if(!o)return NULL;
 const struct window_builder_state *b=&o->window_builder;
 if(index<0 || index>=b->count || !b->textures){error(EINVAL);return NULL;}
 return b->textures[index]; /* Original borrowed array, not a guessed owner. */
}
static int api_nvnWindowGetNumTextures(const NVNwindow *key)
{struct object *o=find(key,WINDOW);return o?(int)o->window->count:0;}
