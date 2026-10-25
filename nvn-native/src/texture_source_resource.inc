/* SPDX-License-Identifier: MIT
 * Canonical source-resource v1. Payload math is separate from public extent:
 * this explicit device policy chooses Generic16Bx2 kind0xfe, no NVIDIA
 * compression, hence512 alignment/no big-page rounding. This is not Adreno
 * LINEAR/FDL. Unknown packaged/special-kind bytes and GPU consumers reject. */
#ifndef NN_NVN_BNTX_PACKAGED_EXPERIMENT
#define NN_NVN_BNTX_PACKAGED_EXPERIMENT 0
#endif
#if NN_NVN_BNTX_PACKAGED_EXPERIMENT != 0 && NN_NVN_BNTX_PACKAGED_EXPERIMENT != 1
#error "Experimental BNTX packaged mapping must be explicit0/1"
#endif
#if NN_NVN_BNTX_PACKAGED_EXPERIMENT
#include "nvn_native/packaged_bntx.h"
#endif
static int source_builder_layout(const struct object *b,struct nn_tex_source_layout *out)
{
    if(!b || b->kind!=TB || !out)return EINVAL;
    struct object *d=live_weak(b->device,DEVICE);
    if(!d || d->texture_source_policy!=SOURCE_POLICY_GENERIC16BX2_NO_COMPRESSION_V1)return ENOTSUP;
    const struct texture_desc *t=&b->texture;
    if(t->set!=TEX_COMPLETE || t->flags<0 || (t->flags&~0xdu) ||
       t->stride || t->width<1 || t->height<1 || t->depth<1 ||
       (t->target!=NVN_TEXTURE_TARGET_2D && t->target!=NVN_TEXTURE_TARGET_2D_ARRAY && t->target!=NVN_TEXTURE_TARGET_3D) ||
       (t->target==NVN_TEXTURE_TARGET_3D && (t->flags&~(NVN_TEXTURE_FLAGS_IMAGE|NVN_TEXTURE_FLAGS_COMPRESSIBLE))) ||
       (t->target==NVN_TEXTURE_TARGET_2D && t->depth!=1))return ENOTSUP;
    /* COMPRESSIBLE is retained on the public object. Only after checking the
     * immutable no-compression device policy do we normalize permission into
     * the selected uncompressed payload calculator's input. No compressed
     * bytes/kind or native GPU layout is inferred from that normalization. */
    struct nn_tex_source_desc desc={.flags=(unsigned)t->flags & ~8u,
        .target=(unsigned)t->target,.format=(unsigned)t->format,
        .width=(unsigned)t->width,.height=(unsigned)t->height,
        .layers=t->target==NVN_TEXTURE_TARGET_2D_ARRAY?(unsigned)t->depth:1,
        .levels=(unsigned)t->levels,.samples=(unsigned)t->samples,
        .block_height_log2=-1,.depth=t->target==NVN_TEXTURE_TARGET_3D?(unsigned)t->depth:0};
    if(b->texture_packaged){
#if NN_NVN_BNTX_PACKAGED_EXPERIMENT
        /* Public writer profile, NVN association currently parent-v12 only.
         * Explicit experiment, not generic package support/default inference. */
        if(t->flags||(t->target!=NVN_TEXTURE_TARGET_2D&&t->target!=NVN_TEXTURE_TARGET_2D_ARRAY)||t->samples)return ENOTSUP;
        uint32_t height=0;int decoded=nn_packaged_bntx_height(b->packaged_layout,&height);
        if(decoded)return decoded;
        desc.block_height_log2=(int32_t)height;
#else
        return ENOTSUP;
#endif
    }
    int e=nn_tex_source_layout(&desc,out);
    if(!e && (out->linear || out->gob_bytes!=512 || out->payload_bytes>INT_MAX))return EOVERFLOW;
    return e;
}
static NVNboolean source_texture_initialize(NVNtexture *key,struct object *v)
{
    struct nn_tex_source_layout payload;int e=source_builder_layout(v,&payload);
    if(e){error(e);return 0;}
    struct object *pool=live_weak(v->pool,POOL),*device=live_weak(v->device,DEVICE);
    if(!pool || !device || pool->device!=device){error(EINVAL);return 0;}
    /* Original SDK4766a8: COMPRESSIBLE permission requires eligible pool.
     * VIRTUAL is not implemented by the public pool constructor yet. */
    if(pool->external || ((v->texture.flags&8) && !(pool->flags&0x280))){error(ENOTSUP);return 0;}
    size_t bytes=(size_t)payload.payload_bytes;
    if(gpu_pool_access(pool,v->offset,bytes,"nvnTextureInitialize"))return 0;
    uint64_t address=nn_bo_address(pool->bo);
    if(!address || v->offset>pool->size || bytes>pool->size-v->offset ||
       v->offset>UINT64_MAX-address || (address+v->offset)%SOURCE_KIND_ALIGNMENT){error(EINVAL);return 0;}
    struct nn_tex_source_layout *saved=malloc(sizeof(*saved));
    if(!saved){error(ENOMEM);return 0;}*saved=payload;
    struct object *o=create(key,TEXTURE);if(!o){free(saved);return 0;}
    o->pool=pool;o->device=device;o->bo=pool->bo;o->offset=v->offset;o->size=bytes;
    o->texture=v->texture;o->source_layout=saved;
    o->texture_packaged=v->texture_packaged;memcpy(o->packaged_layout,v->packaged_layout,8);
    pool->refs++;
    return 1;
}
int nn_nvn_get_texture_builder_observation(const NVNtextureBuilder *key,
 struct nn_nvn_texture_builder_observation *out)
{
    if(!key || !out)return EINVAL;
    /* find() is deliberately avoided: diagnostics must preserve prior errors. */
    const struct object *b=NULL;
    for(const struct object *p=objects;p;p=p->next)
        if(p->key==key && p->kind==TB){b=p;break;}
    if(!b)return ENOENT;
    struct object *d=live_weak(b->device,DEVICE);
    const struct texture_desc *t=&b->texture;
    struct nn_nvn_texture_builder_observation v={
        .fields=t->set,.packaged=b->texture_packaged,.device_live=d!=NULL,
        .source_policy=d?(uint32_t)d->texture_source_policy:0,
        .flags=t->flags,.target=t->target,.format=t->format,
        .width=t->width,.height=t->height,.depth=t->depth,.levels=t->levels,
        .samples=t->samples,.stride=t->stride};
    memcpy(v.packaged_layout,b->packaged_layout,sizeof(v.packaged_layout));
    struct nn_tex_source_layout layout;
    v.source_layout_errno=source_builder_layout(b,&layout);
    *out=v;return 0;
}
static NVNstorageClass api_nvnTextureBuilderGetStorageClass(const NVNtextureBuilder *key)
{
    struct object *o=find(key,TB);if(!o)return (NVNstorageClass)0;
    if(o->texture.target==NVN_TEXTURE_TARGET_BUFFER)return buffer_builder_class(o);
    struct nn_tex_source_layout payload;int e=source_builder_layout(o,&payload);
    if(e){error(e);return (NVNstorageClass)0;}
    return (NVNstorageClass)(0x100|SOURCE_KIND_GENERIC16BX2);
}
static NVNstorageClass api_nvnTextureGetStorageClass(const NVNtexture *key)
{
    struct object *o=find(key,TEXTURE);if(!o)return (NVNstorageClass)0;
    if(o->texture.target==NVN_TEXTURE_TARGET_BUFFER)return buffer_object_class(o);
    if(!o->source_layout){error(ENOTSUP);return (NVNstorageClass)0;}
    return (NVNstorageClass)(0x100|SOURCE_KIND_GENERIC16BX2);
}
