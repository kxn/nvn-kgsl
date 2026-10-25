/* SPDX-License-Identifier: MIT. CPU registration selection over a complete
 * canonical source owner. This never rebases/retiles the stored source layout
 * or claims a native subresource descriptor. Consumers gate subsets separately.
 * Existing independent texture-view report establishes count0=remaining range
 * and mip/layer base addressing; no release-SDK invalid-input guarantee is made. */
static int registered_texture_view(const struct object *t,const NVNtextureView *key,
 struct descriptor_selection *out)
{
 memset(out,0,sizeof(*out));struct resource_view v;int e=resource_view_read(key,&v);if(e)return e;
 if(!t||!t->source_layout)return EINVAL;
 if((v.format&&v.format_value!=t->texture.format)||(v.target&&v.target_value!=t->texture.target))return ENOTSUP;
 const struct texture_desc *d=&t->texture;
 unsigned level=0,layer=0;int layers=d->target==NVN_TEXTURE_TARGET_2D_ARRAY?d->depth:1;
 if((e=view_offset_range(d->levels,v.levels,v.base_level,v.level_count,&level))||
    (e=view_offset_range(layers,v.layers,v.base_layer,v.layer_count,&layer)))return e;
 struct descriptor_selection s={0};s.base_level=level;s.base_layer=layer;
 s.level_count=v.levels&&v.level_count?(unsigned)v.level_count:(unsigned)d->levels-level;
 s.layer_count=v.layers&&v.layer_count?(unsigned)v.layer_count:(unsigned)layers-layer;
 ptrdiff_t offset=0;if((e=texture_view_offset(t,key,&offset)))return e;
 if(offset<0||(uint64_t)offset>=t->size)return EINVAL;
 s.source_offset=(uint64_t)offset;
 s.subset=level||layer||s.level_count!=(unsigned)d->levels||s.layer_count!=(unsigned)layers;
 *out=s;return 0;
}
