/* SPDX-License-Identifier: MIT. Address query, independent of GPU view support.
 * SDK53.314476890 subtracts the original Texture base after47609c resolves
 * original-layout mip offset + layerStride*baseLayer. View format affects
 * returned dimensions there, target is not read, and counts do not move base.
 * Range/enum rejection here is defensive policy for valid resource queries,
 * not a claim that release SDK checks every undefined input. */
static int view_offset_range(int total,unsigned present,int base,int count,unsigned *out)
{
    if(total<1)return EINVAL;
    if(!present){*out=0;return 0;}
    /* Subtract before comparing count; base+count can overflow int. Count0
     * denotes the nonempty remaining range, not a fixed one-element view. */
    if(base<0 || base>=total || count<0 || count>total-base)return EINVAL;
    *out=(unsigned)base;return 0;
}
static int texture_view_offset(const struct object *o,const NVNtextureView *key,ptrdiff_t *out)
{
    *out=0;struct resource_view v;int e=resource_view_read(key,&v);if(e)return e;
    /* Public NVNformat values0..9c are contiguous (including ASTC enum names
     * using lowercase x); LARGE is a sentinel. Valid enum does not certify a
     * reinterpretation, native format, swizzle, sampler or target-view use. */
    if((v.format && (unsigned)v.format_value>NVN_FORMAT_BGRA8_SRGB) ||
       (v.target && (unsigned)v.target_value>NVN_TEXTURE_TARGET_BUFFER))return EINVAL;
    const struct texture_desc *t=&o->texture;
    int layers=t->target==NVN_TEXTURE_TARGET_2D_ARRAY?t->depth:1;
    unsigned level=0,layer=0;
    if((e=view_offset_range(t->levels,v.levels,v.base_level,v.level_count,&level)) ||
       (e=view_offset_range(layers,v.layers,v.base_layer,v.layer_count,&layer)))return e;
    uint64_t offset=0;
    if(o->source_layout){
        const struct nn_tex_source_layout *l=o->source_layout;
        if(nn_tex_source_validate(l) || l->linear ||
           (t->target!=NVN_TEXTURE_TARGET_2D && t->target!=NVN_TEXTURE_TARGET_2D_ARRAY && t->target!=NVN_TEXTURE_TARGET_3D) ||
           l->target!=(unsigned)t->target || l->format!=(unsigned)t->format ||
           l->levels!=(unsigned)t->levels || l->layers!=(unsigned)layers ||
           l->level[0].width!=(unsigned)t->width || l->level[0].height!=(unsigned)t->height ||
           l->payload_bytes!=o->size)return EINVAL;
        if(t->target==NVN_TEXTURE_TARGET_3D&&l->volume_depth!=(unsigned)t->depth)return EINVAL;
        e=nn_tex_source_offset(l,level,layer,0,0,0,&offset);if(e)return e;
        if(offset>=o->size)return EINVAL;
    }else{
        /* Both finite profiles have one mip/layer, but retain distinct byte
         * validation: BUFFER is width*cpp; LINEAR image is actual FDL storage.
         * No GOB/image descriptor or resource consumer is inferred here. */
        size_t bytes=0;e=t->target==NVN_TEXTURE_TARGET_BUFFER?
            buffer_texture_bytes(t,&bytes):linear_texture_bytes(t,&bytes);if(e)return e;
        if(bytes!=o->size || level || layer)return EINVAL;
    }
    if(offset>(uint64_t)PTRDIFF_MAX)return EOVERFLOW;
    *out=(ptrdiff_t)offset;return 0;
}
