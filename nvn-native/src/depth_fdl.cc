/* SPDX-License-Identifier: MIT. Actual original FDL and generated A6 packers. */
#include "nvn_native/depth_fdl.h"
#include "freedreno_layout.h"
#include "freedreno_dev_info.h"
#include "fd6_hw.h"
#include "fd6_format_table.h"
#include "util/format/u_format.h"
#include <nvn.h>
#include <cerrno>
#include <cstring>
static enum pipe_format format(uint32_t f)
{
 switch(f){
 case NVN_FORMAT_STENCIL8:return PIPE_FORMAT_S8_UINT;
 case NVN_FORMAT_DEPTH16:return PIPE_FORMAT_Z16_UNORM;
 case NVN_FORMAT_DEPTH24:return PIPE_FORMAT_Z24X8_UNORM;
 case NVN_FORMAT_DEPTH32F:return PIPE_FORMAT_Z32_FLOAT;
 case NVN_FORMAT_DEPTH24_STENCIL8:return PIPE_FORMAT_Z24_UNORM_S8_UINT;
 case NVN_FORMAT_DEPTH32F_STENCIL8:return PIPE_FORMAT_Z32_FLOAT_S8X24_UINT;
 default:return PIPE_FORMAT_NONE;
 }
}
uint32_t nn_depth_fdl_version(void){return 1;}
int nn_depth_fdl_format(uint32_t f,struct nn_depth_format_info *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));enum pipe_format pf=format(f);
 if(pf==PIPE_FORMAT_NONE)return ENOTSUP;
 const auto*description=util_format_description(pf);struct nn_depth_format_info v={};
 v.schema=1;v.format=f;v.has_depth=util_format_has_depth(description);v.has_stencil=util_format_has_stencil(description);
 v.depth_bits=v.has_depth?util_format_get_component_bits(pf,UTIL_FORMAT_COLORSPACE_ZS,0):0;
 v.stencil_bits=v.has_stencil?util_format_get_component_bits(pf,UTIL_FORMAT_COLORSPACE_ZS,1):0;
 v.source_bytes=util_format_get_blocksize(pf);
 if(v.has_depth&&!v.has_stencil)v.roles=NN_DEPTH_ATTACHMENT|NN_DEPTH_SAMPLED;
 *out=v;return 0;
}
int nn_depth_fdl(const struct nn_depth_fdl_desc *d,struct nn_depth_fdl_view *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!d)return EINVAL;
 struct nn_depth_format_info facts;int e=nn_depth_fdl_format(d->format,&facts);if(e)return e;
 if(d->schema!=1||d->chip!=0x06050002||d->reserved||(d->target!=1&&d->target!=4)||
    !d->width||!d->height||d->width>16384||d->height>16384||!d->levels||d->levels>NN_DEPTH_FDL_LEVELS||
    !d->layers||d->layers>2048||(d->target==1&&d->layers!=1)||!d->roles||(d->roles&~facts.roles))return ENOTSUP;
 for(unsigned i=0;i<4;i++)if(d->swizzle[i]>NN_DEPTH_ONE)return ENOTSUP;
 unsigned m=d->width>d->height?d->width:d->height,maxlevels=1;while(m>>=1)maxlevels++;
 if(d->levels>maxlevels||d->base_level>=d->levels||d->base_layer>=d->layers)return EINVAL;
 fd_dev_id id={};id.chip_id=d->chip;const struct fd_dev_info*info=fd_dev_info_raw(&id);if(!info)return ENOTSUP;
 enum pipe_format pf=format(d->format);enum a6xx_depth_format native=fd6_pipe2depth(pf);
 if((uint32_t)native==UINT32_MAX||!fd6_texture_format_supported(info,pf,TILE6_3,false))return ENOTSUP;
 struct fdl_image_params p={};p.format=pf;p.nr_samples=1;p.width0=d->width;p.height0=d->height;p.depth0=1;
 p.mip_levels=d->levels;p.array_size=d->layers;p.tile_mode=TILE6_3;p.ubwc=false;
 struct fdl_layout l={};if(!fdl6_layout_image(&l,info,&p,nullptr)||l.ubwc||!l.base_align)return ENOTSUP;
 if(!l.size||l.size>UINT32_MAX)return E2BIG;
 struct nn_depth_fdl_view v={};v.desc=*d;v.view_width=u_minify(d->width,d->base_level);v.view_height=u_minify(d->height,d->base_level);
 v.bytes_per_pixel=util_format_get_blocksize(pf);v.alignment=l.base_align;v.native_depth_format=native;v.depth_bits=facts.depth_bits;
 v.bytes=v.required_span=l.size;
 if(l.layer_first&&(uint64_t)l.layer_size*d->layers>v.required_span)v.required_span=(uint64_t)l.layer_size*d->layers;
 for(unsigned i=0;i<d->levels;i++){
  struct nn_depth_fdl_level*q=v.level+i;q->width=u_minify(d->width,i);q->height=u_minify(d->height,i);
  q->pitch=fdl_pitch(&l,i);q->layer_stride=fdl_layer_stride(&l,i);q->offset=fdl_surface_offset(&l,i,0);q->span=l.slices[i].size0;q->tile_mode=fdl_tile_mode(&l,i);
  if(q->tile_mode!=TILE6_3||q->pitch<util_format_get_stride(pf,q->width)||q->layer_stride<q->span)return ENOTSUP;
  uint64_t end=q->offset+(uint64_t)q->layer_stride*(d->layers-1)+q->span;if(end>v.required_span)v.required_span=end;
 }
 if(v.required_span>UINT32_MAX)return E2BIG;
 if(d->address%l.base_align||d->address>=(UINT64_C(1)<<49)||v.required_span>(UINT64_C(1)<<49)-d->address)return EINVAL;
 struct fdl_view_args a={};a.iova=d->address;a.format=pf;a.type=FDL_VIEW_TYPE_2D;a.base_miplevel=d->base_level;a.level_count=1;
 a.base_array_layer=d->base_layer;a.layer_count=1;a.min_lod_clamp=(float)d->base_level;
 for(unsigned i=0;i<4;i++)a.swiz[i]=d->swizzle[i]==NN_DEPTH_R?PIPE_SWIZZLE_X:d->swizzle[i]==NN_DEPTH_ZERO?PIPE_SWIZZLE_0:PIPE_SWIZZLE_1;
 const struct fdl_layout*layouts[3]={&l,nullptr,nullptr};struct fdl6_view view={};fdl6_view_init<A6XX>(&view,layouts,&a,info->props.has_z24uint_s8uint);
 if(view.ubwc_enabled||view.offset!=fdl_surface_offset(&l,d->base_level,d->base_layer))return ENOTSUP;
 v.view_offset=view.offset;v.descriptor_address_word=4;
 v.descriptor_address_mask=uint64_t(A6XX_TEX_MEMOBJ_4_BASE_LO__MASK)|(uint64_t(A6XX_TEX_MEMOBJ_5_BASE_HI__MASK)<<32);
 memcpy(v.descriptor,view.descriptor,sizeof(v.descriptor));
 if(!d->address){v.descriptor[4]&=~(uint32_t)v.descriptor_address_mask;v.descriptor[5]&=~(uint32_t)(v.descriptor_address_mask>>32);}
 /* A6 depth tiling is implicit; the TILEMODE field only exists on A7+. */
 v.rb_depth_buffer_info=A6XX_RB_DEPTH_BUFFER_INFO_DEPTH_FORMAT(native);
 v.rb_depth_buffer_pitch=A6XX_RB_DEPTH_BUFFER_PITCH(fdl_pitch(&l,d->base_level));
 v.rb_depth_buffer_array_pitch=A6XX_RB_DEPTH_BUFFER_ARRAY_PITCH(fdl_layer_stride(&l,d->base_level));
 v.gras_su_depth_buffer_info=A6XX_GRAS_SU_DEPTH_BUFFER_INFO_DEPTH_FORMAT(native);
 *out=v;return 0;
}
int nn_depth_fdl_controls(const struct nn_depth_controls_desc*d,struct nn_depth_controls*out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!d)return EINVAL;
 if(d->schema!=1||d->chip!=0x06050002||d->reserved||d->test_enable>1||d->write_enable>1||d->shader_writes_depth>1||
    d->compare<NVN_DEPTH_FUNC_NEVER||d->compare>NVN_DEPTH_FUNC_ALWAYS)return ENOTSUP;
 if((d->write_enable&&!d->test_enable)||(d->shader_writes_depth?d->depth_regid>=192:d->depth_regid!=252))return EINVAL;
 struct nn_depth_controls v={};
 v.rb_depth_cntl=(d->test_enable?A6XX_RB_DEPTH_CNTL_Z_TEST_ENABLE:0)|(d->write_enable?A6XX_RB_DEPTH_CNTL_Z_WRITE_ENABLE:0)|
   A6XX_RB_DEPTH_CNTL_ZFUNC((enum adreno_compare_func)(d->compare-1))|(d->test_enable?A6XX_RB_DEPTH_CNTL_Z_READ_ENABLE:0);
 v.gras_su_depth_cntl=d->test_enable?A6XX_GRAS_SU_DEPTH_CNTL_Z_TEST_ENABLE:0;
 v.rb_ps_output_cntl=d->shader_writes_depth?A6XX_RB_PS_OUTPUT_CNTL_FRAG_WRITES_Z:0;
 v.sp_ps_output_cntl=A6XX_SP_PS_OUTPUT_CNTL_DEPTH_REGID(d->depth_regid)|A6XX_SP_PS_OUTPUT_CNTL_SAMPMASK_REGID(252)|A6XX_SP_PS_OUTPUT_CNTL_STENCILREF_REGID(252);
 v.rb_depth_plane_cntl=A6XX_RB_DEPTH_PLANE_CNTL_Z_MODE(A6XX_LATE_Z);v.gras_su_depth_plane_cntl=A6XX_GRAS_SU_DEPTH_PLANE_CNTL_Z_MODE(A6XX_LATE_Z);
 *out=v;return 0;
}
