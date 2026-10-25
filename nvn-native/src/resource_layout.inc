/* SPDX-License-Identifier: MIT
 * Explicit LINEAR storage only. fd6_layout.c fdl6_layout_image: linear base
 * alignment 64, pitch alignment >=64 (>=128 for 8B, >=256 for 16B), final
 * level height aligned to 4, layer allocation aligned to 4096. No Tegra
 * packaged/tiled bytes are interpreted as an Adreno layout. */
static unsigned linear_cpp(int format)
{
    switch (format) {
    case NVN_FORMAT_R8: case NVN_FORMAT_R8SN: case NVN_FORMAT_R8UI: case NVN_FORMAT_R8I: return 1;
    case NVN_FORMAT_R16F: case NVN_FORMAT_R16: case NVN_FORMAT_R16SN: case NVN_FORMAT_R16UI: case NVN_FORMAT_R16I:
    case NVN_FORMAT_RG8: case NVN_FORMAT_RG8SN: case NVN_FORMAT_RG8UI: case NVN_FORMAT_RG8I: return 2;
    case NVN_FORMAT_R32F: case NVN_FORMAT_R32UI: case NVN_FORMAT_R32I:
    case NVN_FORMAT_RG16F: case NVN_FORMAT_RG16: case NVN_FORMAT_RG16SN: case NVN_FORMAT_RG16UI: case NVN_FORMAT_RG16I:
    case NVN_FORMAT_RGBA8: case NVN_FORMAT_RGBA8SN: case NVN_FORMAT_RGBA8UI: case NVN_FORMAT_RGBA8I: case NVN_FORMAT_RGBA8_SRGB: return 4;
    case NVN_FORMAT_RG32F: case NVN_FORMAT_RG32UI: case NVN_FORMAT_RG32I:
    case NVN_FORMAT_RGBA16F: case NVN_FORMAT_RGBA16: case NVN_FORMAT_RGBA16SN: case NVN_FORMAT_RGBA16UI: case NVN_FORMAT_RGBA16I: return 8;
    case NVN_FORMAT_RGBA32F: case NVN_FORMAT_RGBA32UI: case NVN_FORMAT_RGBA32I: return 16;
    default: return 0;
    }
}
static int linear_texture_bytes(const struct texture_desc *t, size_t *bytes)
{
    unsigned cpp=linear_cpp(t->format), align=cpp>4?cpp*16:64;
    if (!cpp || t->flags!=NVN_TEXTURE_FLAGS_LINEAR ||
        (t->target!=NVN_TEXTURE_TARGET_1D && t->target!=NVN_TEXTURE_TARGET_2D) ||
        t->width<1 || t->width>16384 || t->height<1 || t->height>16384 ||
        (t->target==NVN_TEXTURE_TARGET_1D && t->height!=1) ||
        t->depth!=1 || t->levels!=1 || t->samples || t->stride<0 ||
        (uint64_t)t->stride<(uint64_t)(unsigned)t->width*cpp ||
        t->stride>1048576 || t->stride%align) return ENOTSUP;
    uint64_t n=(uint64_t)t->stride*(((unsigned)t->height+3u)&~3u);
    n=(n+4095u)&~UINT64_C(4095);
    if (n>INT32_MAX || n>SIZE_MAX) return EOVERFLOW;
    *bytes=(size_t)n; return 0;
}
/* Opaque public 40-byte value, deliberately pointer-free and memcpy-safe.
 * Presence/default/zero-count semantics independently checked in SDK53.314
 * 475880..475b48, 491cf0. This private encoding is not SDK object bytes; see
 * docs texture-view-sdk-research.zh-CN.md. Registration selection is stored separately from the source owner. */
struct resource_view { unsigned char levels,layers,format,target; int base_level,level_count,base_layer,layer_count,format_value,target_value; unsigned char swizzle,mode,swizzle_value[4],mode_value; };
_Static_assert(sizeof(struct resource_view)<=sizeof(NVNtextureView),"inline view ABI");
static int resource_view_fields_valid(const struct resource_view *v)
{
 if(v->levels>1||v->layers>1||v->format>1||v->target>1||v->swizzle>1||v->mode>1)return EINVAL;
 if(v->swizzle)for(unsigned i=0;i<4;i++)if(v->swizzle_value[i]>NVN_TEXTURE_SWIZZLE_A)return EINVAL;
 if(v->mode&&v->mode_value>NVN_TEXTURE_DEPTH_STENCIL_MODE_STENCIL)return EINVAL;
 return 0;
}
static int resource_view_read(const NVNtextureView *key, struct resource_view *out)
{
    memset(out,0,sizeof(*out)); if (!key) return 0;
    memcpy(out,key,sizeof(*out));
    if(resource_view_fields_valid(out))return EINVAL;
    for (size_t i=sizeof(*out);i<sizeof(*key);i++) if (((const unsigned char *)key)[i]) return EINVAL;
    return 0;
}
static void resource_view_write(NVNtextureView *key,const struct resource_view *v)
{ memset(key,0,sizeof(*key)); memcpy(key,v,sizeof(*v)); }
/* Swizzle/mode are sampled interpretation, independent of byte layout.
 * Registration snapshots the effective values; native sampled consumers must
 * qualify them rather than silently emit an identity descriptor. */
static int texture_view_sampling_metadata(const struct object *texture,const NVNtextureView *key,struct texture_desc *out)
{
 struct resource_view v;int e=resource_view_read(key,&v);if(e)return e;
 *out=texture->texture;
 if(v.swizzle){out->swizzle_set=1;memcpy(out->swizzle,v.swizzle_value,4);}
 if(v.mode)out->depth_mode=v.mode_value;
 return 0;
}
static int resolve_texture_view(const struct object *texture,const NVNtextureView *key, struct texture_desc *out)
{
    struct resource_view v; int e=resource_view_read(key,&v); if (e) return e;
    /* A full/default view can retain the canonical layout unchanged. Selecting
     * a single mip/layer from a larger resource needs a resolved subresource
     * layout; do not silently publish the original complete resource instead. */
    int layers=texture->texture.target==NVN_TEXTURE_TARGET_2D_ARRAY?texture->texture.depth:1;
    if ((v.levels && (v.base_level!=0 || (v.level_count!=0 && v.level_count!=texture->texture.levels))) ||
        (v.layers && (v.base_layer!=0 || (v.layer_count!=0 && v.layer_count!=layers)))) return ENOTSUP;
    if((e=texture_view_sampling_metadata(texture,key,out)))return e;
    /* Same-format only: CP byte copy does not implement format conversion. */
    if ((v.format && v.format_value!=out->format) || (v.target && v.target_value!=out->target)) return ENOTSUP;
    return 0;
}
