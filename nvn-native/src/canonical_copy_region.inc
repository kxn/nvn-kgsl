/* SPDX-License-Identifier: MIT. Public pixel region -> canonical raw blocks.
 * SDK53.314464590/464bf0 use the original target, view base mip/layer,
 * block-ceiled extent and explicit/default linear byte strides. This is
 * metadata admission, independent of any Adreno layout or copy job owner. */
struct canonical_copy_region {
    uint32_t level,base_layer,layers,x_bytes,y_rows,row_bytes,rows;
    uint32_t linear_row_stride,linear_image_stride;
    uint64_t linear_span;
};
static inline int canonical_copy_region(const struct object *texture,const NVNtextureView *view,
 const NVNcopyRegion *region,ptrdiff_t row_stride,ptrdiff_t image_stride,
 struct canonical_copy_region *out)
{
    if(!out)return EINVAL;
    memset(out,0,sizeof(*out));
    if(!texture||!texture->source_layout)return ENOTSUP;
    if(!region)return EINVAL;
    const struct nn_tex_source_layout *s=texture->source_layout;
    const struct texture_desc *t=&texture->texture;
    int e=nn_tex_source_validate(s);if(e)return e;
    int volume=t->target==NVN_TEXTURE_TARGET_3D;
    if(s->linear||(!volume&&t->target!=NVN_TEXTURE_TARGET_2D&&t->target!=NVN_TEXTURE_TARGET_2D_ARRAY))return ENOTSUP;
    unsigned layers=t->target==NVN_TEXTURE_TARGET_2D_ARRAY?(unsigned)t->depth:1;
    if(s->target!=(unsigned)t->target||s->format!=(unsigned)t->format||
       s->levels!=(unsigned)t->levels||s->layers!=layers||s->payload_bytes!=texture->size||
       s->level[0].width!=(unsigned)t->width||s->level[0].height!=(unsigned)t->height||
       (volume&&s->volume_depth!=(unsigned)t->depth))return EINVAL;
    struct resource_view v;if((e=resource_view_read(view,&v)))return e;
    if(v.target&&(unsigned)v.target_value>NVN_TEXTURE_TARGET_BUFFER)return EINVAL;
    /* Addressing still belongs to the original resource. A valid view target
     * is not read by the SDK copy resolver; format reinterpretation requires
     * a separately qualified block-shape conversion and remains unsupported. */
    if(v.format&&v.format_value!=t->format)return ENOTSUP;
    unsigned level=0,layer=0;
    if((e=view_offset_range(t->levels,v.levels,v.base_level,v.level_count,&level))||
       (e=view_offset_range((int)layers,v.layers,v.base_layer,v.layer_count,&layer)))return e;
    const struct nn_tex_source_level *m=s->level+level;
    if(region->xoffset<0||region->yoffset<0||region->zoffset<0||
       region->width<1||region->height<1||region->depth<1||
       (unsigned)region->xoffset>m->width||(unsigned)region->width>m->width-(unsigned)region->xoffset||
       (unsigned)region->yoffset>m->height||(unsigned)region->height>m->height-(unsigned)region->yoffset)return EINVAL;
    /* A true volume has one resource layer. Its selected mip has its own Z
     * extent; slices interleave within GOBs, never at array-layer strides. */
    unsigned available=volume?s->level_depths[level]:
        (v.layers&&v.layer_count?(unsigned)v.layer_count:layers-layer);
    if((unsigned)region->zoffset>=available||(unsigned)region->depth>available-(unsigned)region->zoffset)return EINVAL;
    if((unsigned)region->xoffset%s->block_w||(unsigned)region->yoffset%s->block_h||
       ((unsigned)region->width%s->block_w&&(unsigned)region->xoffset+(unsigned)region->width!=m->width)||
       ((unsigned)region->height%s->block_h&&(unsigned)region->yoffset+(unsigned)region->height!=m->height))return ENOTSUP;
    uint32_t row_bytes=(((unsigned)region->width+s->block_w-1)/s->block_w)*s->bytes_per_block;
    uint32_t rows=((unsigned)region->height+s->block_h-1)/s->block_h;
    unsigned count=(unsigned)region->depth;
    /* A one-row stride is observable only through default layer spacing.
     * An image stride is unused for one layer. Normalize only such inactive
     * private kernel arguments; public command metadata remains unchanged. */
    unsigned row_used=rows>1||(count>1&&!image_stride);
    if((row_used&&row_stride<0)||(count>1&&image_stride<0))return EINVAL;
    uint64_t declared_pitch=row_used&&row_stride?(uint64_t)row_stride:row_bytes;
    uint64_t pitch=rows>1?declared_pitch:row_bytes;
    if(declared_pitch>UINT32_MAX)return EOVERFLOW;
    if(pitch<row_bytes)return EINVAL;
    uint64_t last=(uint64_t)(rows-1)*pitch+row_bytes;
    uint64_t image=count>1?(image_stride?(uint64_t)image_stride:declared_pitch*rows):last;
    if(image<last)return EINVAL;
    if(image>UINT32_MAX)return EOVERFLOW;
    uint64_t span=(uint64_t)(count-1)*image+last;
    if(pitch>UINT32_MAX||image>UINT32_MAX||span>UINT32_MAX-3u)return EOVERFLOW;
    *out=(struct canonical_copy_region){level,layer+(unsigned)region->zoffset,count,
        (unsigned)region->xoffset/s->block_w*s->bytes_per_block,(unsigned)region->yoffset/s->block_h,
        row_bytes,rows,(uint32_t)pitch,(uint32_t)image,span};
    return 0;
}
