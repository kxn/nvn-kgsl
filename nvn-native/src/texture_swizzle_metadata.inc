/* SPDX-License-Identifier: MIT. Public metadata behavior from reconstructed
 * C PFNs and existing independent SDK53.314 texture/view reports. Native
 * sampled consumers gate the effective snapshot; no swizzle GPU support is
 * implied by storing a descriptor value. */
static int texture_swizzle_values(NVNtextureSwizzle r,NVNtextureSwizzle g,
 NVNtextureSwizzle b,NVNtextureSwizzle a)
{return (unsigned)r<=NVN_TEXTURE_SWIZZLE_A&&(unsigned)g<=NVN_TEXTURE_SWIZZLE_A&&
 (unsigned)b<=NVN_TEXTURE_SWIZZLE_A&&(unsigned)a<=NVN_TEXTURE_SWIZZLE_A;}
static void api_nvnTextureBuilderSetSwizzle(NVNtextureBuilder *key,NVNtextureSwizzle r,
 NVNtextureSwizzle g,NVNtextureSwizzle b,NVNtextureSwizzle a)
{
 struct object *o=find(key,TB);if(!o)return;
 if(!texture_swizzle_values(r,g,b,a)){error(EINVAL);return;}
 o->texture.swizzle_set=1;o->texture.swizzle[0]=(unsigned char)r;
 o->texture.swizzle[1]=(unsigned char)g;o->texture.swizzle[2]=(unsigned char)b;o->texture.swizzle[3]=(unsigned char)a;
}
static void api_nvnTextureBuilderSetDepthStencilMode(NVNtextureBuilder *key,NVNtextureDepthStencilMode mode)
{
 struct object *o=find(key,TB);if(!o)return;
 if((unsigned)mode>NVN_TEXTURE_DEPTH_STENCIL_MODE_STENCIL){error(EINVAL);return;}
 o->texture.depth_mode=(unsigned char)mode;
}
static void texture_swizzle_out(const struct texture_desc *t,NVNtextureSwizzle *r,
 NVNtextureSwizzle *g,NVNtextureSwizzle *b,NVNtextureSwizzle *a)
{
 *r=(NVNtextureSwizzle)texture_swizzle_component(t,0);*g=(NVNtextureSwizzle)texture_swizzle_component(t,1);
 *b=(NVNtextureSwizzle)texture_swizzle_component(t,2);*a=(NVNtextureSwizzle)texture_swizzle_component(t,3);
}
#define TEXTURE_SAMPLING_GETTERS(prefix,type,kind_value) \
static void api_##prefix##GetSwizzle(const type *key,NVNtextureSwizzle *r,NVNtextureSwizzle *g,NVNtextureSwizzle *b,NVNtextureSwizzle *a) { \
 struct object *o=find(key,kind_value);if(!o)return; \
 if(!r||!g||!b||!a){error(EINVAL);return;}texture_swizzle_out(&o->texture,r,g,b,a); } \
static NVNtextureDepthStencilMode api_##prefix##GetDepthStencilMode(const type *key) { \
 struct object *o=find(key,kind_value);return o?(NVNtextureDepthStencilMode)o->texture.depth_mode:NVN_TEXTURE_DEPTH_STENCIL_MODE_DEPTH; }
TEXTURE_SAMPLING_GETTERS(nvnTextureBuilder,NVNtextureBuilder,TB)
TEXTURE_SAMPLING_GETTERS(nvnTexture,NVNtexture,TEXTURE)
#undef TEXTURE_SAMPLING_GETTERS
static void api_nvnTextureViewSetSwizzle(NVNtextureView *key,NVNtextureSwizzle r,
 NVNtextureSwizzle g,NVNtextureSwizzle b,NVNtextureSwizzle a)
{
 struct resource_view v;int e=resource_view_read(key,&v);
 if(!key||e||!texture_swizzle_values(r,g,b,a)){error(e?e:EINVAL);return;}
 v.swizzle=1;v.swizzle_value[0]=(unsigned char)r;v.swizzle_value[1]=(unsigned char)g;
 v.swizzle_value[2]=(unsigned char)b;v.swizzle_value[3]=(unsigned char)a;resource_view_write(key,&v);
}
static void api_nvnTextureViewSetDepthStencilMode(NVNtextureView *key,NVNtextureDepthStencilMode mode)
{
 struct resource_view v;int e=resource_view_read(key,&v);
 if(!key||e||(unsigned)mode>NVN_TEXTURE_DEPTH_STENCIL_MODE_STENCIL){error(e?e:EINVAL);return;}
 v.mode=1;v.mode_value=(unsigned char)mode;resource_view_write(key,&v);
}
static NVNboolean api_nvnTextureViewGetSwizzle(const NVNtextureView *key,NVNtextureSwizzle *r,
 NVNtextureSwizzle *g,NVNtextureSwizzle *b,NVNtextureSwizzle *a)
{
 struct resource_view v;int e=resource_view_read(key,&v);
 if(!key||e||!r||!g||!b||!a){error(e?e:EINVAL);return 0;}
 *r=(NVNtextureSwizzle)v.swizzle_value[0];*g=(NVNtextureSwizzle)v.swizzle_value[1];
 *b=(NVNtextureSwizzle)v.swizzle_value[2];*a=(NVNtextureSwizzle)v.swizzle_value[3];return v.swizzle;
}
static NVNboolean api_nvnTextureViewGetDepthStencilMode(const NVNtextureView *key,NVNtextureDepthStencilMode *mode)
{
 struct resource_view v;int e=resource_view_read(key,&v);
 if(!key||e||!mode){error(e?e:EINVAL);return 0;}
 *mode=(NVNtextureDepthStencilMode)v.mode_value;return v.mode;
}
static NVNboolean api_nvnTextureViewCompare(const NVNtextureView *a,const NVNtextureView *b)
{
 if(!a||!b){error(EINVAL);return 0;}
 /* Compare ignores unused payload and padding. Other consumers may enforce
  * the private encoding's zero tail; it never participates in equality. */
 struct resource_view x,y;memcpy(&x,a,sizeof(x));memcpy(&y,b,sizeof(y));
 if(resource_view_fields_valid(&x)||resource_view_fields_valid(&y)){error(EINVAL);return 0;}
 if(x.levels!=y.levels||x.layers!=y.layers||x.format!=y.format||x.target!=y.target||
    x.swizzle!=y.swizzle||x.mode!=y.mode)return 0;
 return (!x.levels||(x.base_level==y.base_level&&x.level_count==y.level_count))&&
  (!x.layers||(x.base_layer==y.base_layer&&x.layer_count==y.layer_count))&&
  (!x.format||x.format_value==y.format_value)&&(!x.target||x.target_value==y.target_value)&&
  (!x.swizzle||!memcmp(x.swizzle_value,y.swizzle_value,4))&&(!x.mode||x.mode_value==y.mode_value);
}
