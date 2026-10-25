/* SPDX-License-Identifier: MIT. Canonical typed BUFFER resource metadata.
 * SDK53.314475c64..94: width(texels)*format element bytes, no GOB/image pitch.
 * 475550/554: explicit LINEAR alignment32. flags0 chooses format cpp under
 * this backend's immutable common-kindfe configuration, per47558c..59c.
 * This does not construct an Adreno descriptor or admit any GPU consumer. */
#include "nvn_native/texture_buffer_format.h"
static int buffer_texture_bytes(const struct texture_desc *t,size_t *bytes)
{
    if(!t || !bytes)return EINVAL;
    if(t->set!=TEX_COMPLETE || t->target!=NVN_TEXTURE_TARGET_BUFFER ||
       (t->flags!=0 && t->flags!=NVN_TEXTURE_FLAGS_LINEAR && t->flags!=NVN_TEXTURE_FLAGS_LINEAR_RENDER_TARGET) ||
       t->height!=1 || t->depth!=1 || t->levels!=1 || t->samples || t->stride)return ENOTSUP;
    if(t->width<1)return EINVAL;
    uint32_t cpp=0;int e=nn_tex_buffer_element_bytes((uint32_t)t->format,&cpp);if(e)return e;
    /* Public TextureGetStorageSize currently has a signed32-bit result.
     * This is our bounded frontend span, not an invented NVN maximum property. */
    if((size_t)t->width>INT_MAX/cpp)return EOVERFLOW;
    *bytes=(size_t)t->width*cpp;return 0;
}
/* Explicit backend common-kind policy, not a read of a Nintendo Device.
 * SDK47558c..59c chooses cpp when common kind is0 orfe; this backend
 * chooses fe. BUFFER payload remains contiguous and initialized object kind0. */
static int buffer_texture_alignment(const struct object *b,size_t *alignment)
{
    if(b->texture.flags==NVN_TEXTURE_FLAGS_LINEAR_RENDER_TARGET){*alignment=128;return 0;}
    if(b->texture.flags==NVN_TEXTURE_FLAGS_LINEAR){*alignment=32;return 0;}
    struct object *d=live_weak(b->device,DEVICE);
    if(!d || d->texture_source_policy!=SOURCE_POLICY_GENERIC16BX2_NO_COMPRESSION_V1)return ENOTSUP;
    uint32_t cpp=0;int e=nn_tex_buffer_element_bytes((uint32_t)b->texture.format,&cpp);if(e)return e;
    *alignment=cpp==12?4:cpp;return 0;
}
static NVNstorageClass buffer_builder_class(const struct object *b)
{
    size_t bytes=0;int e=b->texture_packaged?ENOTSUP:buffer_texture_bytes(&b->texture,&bytes);
    struct object *d=live_weak(b->device,DEVICE);
    if(!e && (!d || d->texture_source_policy!=SOURCE_POLICY_GENERIC16BX2_NO_COMPRESSION_V1))e=ENOTSUP;
    if(e){error(e);return (NVNstorageClass)0;}
    return (NVNstorageClass)(0x100|SOURCE_KIND_GENERIC16BX2);
}
static NVNstorageClass buffer_object_class(const struct object *o)
{
    size_t bytes=0;int e=buffer_texture_bytes(&o->texture,&bytes);
    if(!e && bytes!=o->size)e=EINVAL;
    if(e){error(e);return (NVNstorageClass)0;}
    /* SDK475c90 stores layout kind0 before475cd0 returns. Initialize
     * passes output=object+58 (476598..5a4), so GetStorageClass reads0 at+70.
     * Do not copy the builder common-kindfe into this initialized object. */
    return (NVNstorageClass)0x100;
}
static int buffer_texture_set_storage(struct object *b,struct object *pool,ptrdiff_t offset)
{
    if(pool->external)return ENOTSUP;
    if(pool->device!=b->device || offset<0 || (size_t)offset>pool->size)return EINVAL;
    size_t bytes=0;int e=b->texture_packaged?ENOTSUP:buffer_texture_bytes(&b->texture,&bytes);if(e)return e;
    uint64_t address=nn_bo_address(pool->bo);
    if(!address || (size_t)offset>UINT64_MAX-address)return EOVERFLOW;
    if(bytes>pool->size-(size_t)offset || bytes>UINT64_MAX-address-(size_t)offset)return EINVAL;
    size_t alignment=0;e=buffer_texture_alignment(b,&alignment);if(e)return e;
    if((address+(size_t)offset)%alignment)return EINVAL;
    b->pool=pool;b->builder_pool_key=(const NVNmemoryPool *)pool->key;b->offset=(size_t)offset;return 0;
}
static NVNboolean buffer_texture_initialize(NVNtexture *key,struct object *b)
{
    size_t bytes=0;int e=b->texture_packaged?ENOTSUP:buffer_texture_bytes(&b->texture,&bytes);
    if(e){error(e);return 0;}
    struct object *pool=live_weak(b->pool,POOL),*device=live_weak(b->device,DEVICE);
    if(!pool || !device || pool->device!=device){error(EINVAL);return 0;}
    e=buffer_texture_set_storage(b,pool,(ptrdiff_t)b->offset);if(e){error(e);return 0;}
    if(gpu_pool_access(pool,b->offset,bytes,"nvnTextureInitialize BUFFER"))return 0;
    struct object *o=create(key,TEXTURE);if(!o)return 0;
    o->pool=pool;pool->refs++;o->device=device;o->bo=pool->bo;
    o->offset=b->offset;o->size=bytes;o->texture=b->texture;return 1;
}
static int descriptor_write(struct object *,int,const void *,struct descriptor_entry);
static int buffer_texture_register(struct object *table,int id,struct object *texture,const NVNtextureView *view)
{
    struct texture_desc resolved;int e=resolve_texture_view(texture,view,&resolved);if(e)return e;
    size_t bytes=0;e=buffer_texture_bytes(&resolved,&bytes);if(e)return e;
    if(texture->source_layout || texture->pool->external)return ENOTSUP;
    if(table->device!=texture->device || bytes!=texture->size)return EINVAL;
    /*32B native software slot, not Maxwell TIC or Adreno image/BUF descriptor.
     * Distinct tag and complete typed snapshot prevent image-layout guessing.
     * Descriptor publication owns the canonical pool independently of Texture. */
    struct texture_slot slot={0x42584e4eu,(uint32_t)resolved.format,(uint32_t)resolved.width,
        1,0,(uint32_t)bytes,nn_bo_address(texture->bo)+texture->offset};
    struct descriptor_entry entry={.pool=texture->pool,.offset=texture->offset,.size=bytes,.valid=1};
    entry.snapshot.texture=resolved;
    return descriptor_write(table,id,&slot,entry);
}
