/* SPDX-License-Identifier: MIT. Gallium/FDL register semantics, no runtime. */
#include "nvn_native/color_target.h"
#include "nvn_native/runtime_fdl.h"
#include "nvn_native/pm4.h"
#include "nvn.h"
#include <errno.h>
#include <string.h>
/* Semantic public NVN domains, never native encodings. FDL independently
 * validates color-format support and supplies its true storage width. */
static int components(uint32_t format,uint32_t *mask,uint32_t *srgb)
{
 *mask=0;*srgb=0;
 switch(format){
 case NVN_FORMAT_R8:case NVN_FORMAT_R8SN:case NVN_FORMAT_R16F:
 case NVN_FORMAT_R16:case NVN_FORMAT_R16SN:case NVN_FORMAT_R32F:*mask=1;return 0;
 case NVN_FORMAT_RG8:case NVN_FORMAT_RG8SN:case NVN_FORMAT_RG16F:
 case NVN_FORMAT_RG16:case NVN_FORMAT_RG16SN:case NVN_FORMAT_RG32F:*mask=3;return 0;
 case NVN_FORMAT_RGBA8:case NVN_FORMAT_RGBA8SN:case NVN_FORMAT_RGBA16F:
 case NVN_FORMAT_RGBA16:case NVN_FORMAT_RGBA16SN:case NVN_FORMAT_RGBA32F:
 case NVN_FORMAT_BGRA8:*mask=15;return 0;
 case NVN_FORMAT_RGBA8_SRGB:case NVN_FORMAT_BGRA8_SRGB:*mask=15;*srgb=1;return 0;
 case NVN_FORMAT_R11G11B10F:*mask=7;return 0;
 default:return ENOTSUP;
 }
}
int nn_color_target_create(uint32_t chip,uint32_t format,uint32_t width,uint32_t height,
 uint32_t pitch,struct nn_tex_native_view *out,struct nn_color_target_info *info)
{
 if(info)memset(info,0,sizeof(*info));
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));uint32_t mask,srgb;int e=components(format,&mask,&srgb);if(e)return e;
 if(nn_fdl_metadata_version()!=1)return ENOTSUP;
 const struct nn_fdl_image_desc d={.schema=1,.chip=chip,.format=format,.target=1,
 .width=width,.height=height,.levels=1,.layers=1,.level_count=1,.layer_count=1,
 .usage=NN_FDL_SAMPLED|NN_FDL_COLOR_TARGET,.explicit_pitch=pitch};
 struct nn_fdl_image_view v;if((e=nn_fdl_image(&d,&v)))return e;
 if(v.schema!=1||v.chip!=chip||v.format!=format||v.target!=1||v.width!=width||v.height!=height||
    v.levels!=1||v.layers!=1||v.base_level||v.base_layer||v.level_count!=1||v.layer_count!=1||
    v.block_width!=1||v.block_height!=1||!v.bytes_per_block||!v.color_target||v.view_offset||
    v.bytes>UINT32_MAX||v.required_span>UINT32_MAX||v.descriptor_address_word!=4||
    (pitch&&v.level[0].pitch!=pitch))return EINVAL;
 /* Original FDL uses SP_PS_MRT_REG bits8/9 for SINT/UINT. The admitted shader
  * recipe is float32vec4, so neither integer output interpretation is legal. */
 if(v.sp_ps_mrt_reg&~UINT32_C(255))return ENOTSUP;
 struct nn_tex_native_view n={.schema=1,.chip=chip,.format=format,.target=1,.levels=1,.layers=1,.samples=1,
  .width=width,.height=height,.pitch=v.level[0].pitch,.layer_bytes=v.level[0].layer_stride,
  .bytes=(uint32_t)v.bytes,.alignment=v.alignment,.required_span_bytes=(uint32_t)v.required_span,
  .descriptor_address_word=v.descriptor_address_word,.descriptor_offset=(uint32_t)v.view_offset,
  .descriptor_address_mask=v.descriptor_address_mask,.rb_mrt_buf_info=v.rb_mrt_buf_info,
  .rb_mrt_pitch=v.rb_mrt_pitch,.rb_mrt_array_pitch=v.rb_mrt_array_pitch,.sp_ps_mrt_reg=v.sp_ps_mrt_reg};
 memcpy(n.texture_descriptor,v.descriptor,sizeof(n.texture_descriptor));*out=n;
 if(info)*info=(struct nn_color_target_info){v.bytes_per_block,mask,srgb,v.sp_ps_mrt_reg};
 return 0;
}
int nn_color_target_validate(const struct nn_tex_native_view *v,struct nn_color_target_info *info)
{
 if(info)memset(info,0,sizeof(*info));
 if(!v)return EINVAL;
 struct nn_tex_native_view actual;struct nn_color_target_info metadata;
 int e=nn_color_target_create(v->chip,v->format,v->width,v->height,0,&actual,&metadata);if(e)return e;
 /* Explicit and default FDL layouts can have the same pitch but different
  * base alignment/span. The legacy value does not store this request bit,
  * so validate either complete real-FDL result, never just equal pitch. */
 if(memcmp(v,&actual,sizeof(actual))){
  e=nn_color_target_create(v->chip,v->format,v->width,v->height,v->pitch,&actual,&metadata);if(e)return e;
  if(memcmp(v,&actual,sizeof(actual)))return EINVAL;
 }
 if(info)*info=metadata;
 return 0;
}
int nn_color_target_words(const struct nn_tex_native_view *v,uint32_t *words,size_t capacity,size_t *count)
{
 if(!count)return EINVAL;
 *count=0;if(!words&&capacity)return EINVAL;
 struct nn_color_target_info info;int e=nn_color_target_validate(v,&info);if(e)return e;
 if(!words&&!capacity){*count=14;return 0;}
 if(capacity<14)return ENOSPC;
 const uint32_t registers[]={0x8822,0x8823,0x8824,0xa996,0x8102,0x880f,0xa98a};
 const uint32_t values[]={v->rb_mrt_buf_info,v->rb_mrt_pitch,v->rb_mrt_array_pitch,
  v->sp_ps_mrt_reg,info.color_format,info.srgb,info.srgb};
 uint32_t staged[14];for(unsigned i=0;i<7;i++){staged[2*i]=nvn_pm4_packet4(registers[i],1);staged[2*i+1]=values[i];}
 memcpy(words,staged,sizeof(staged));*count=14;return 0;
}
