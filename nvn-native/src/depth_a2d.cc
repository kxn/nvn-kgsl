/* SPDX-License-Identifier: MIT. Semantics: pinned fd6_blitter.cc depth aliases,
 * A2D setup/src/dst/clear, actual FDL; generated PM4/register packers. */
#include "nvn_native/depth_a2d.h"
#include "freedreno_layout.h"
#include "freedreno_dev_info.h"
#include "nvn_native/pm4.h"
#include "adreno/adreno_pm4.xml.h"
#include "fd6_hw.h"
#include "fd6_format_table.h"
#include "util/format/u_format.h"
#include <cerrno>
#include <cmath>
#include <cstring>
static enum pipe_format depth_format(unsigned f){switch(f){case 0x32:return PIPE_FORMAT_Z16_UNORM;case 0x33:return PIPE_FORMAT_Z24X8_UNORM;case 0x34:return PIPE_FORMAT_Z32_FLOAT;default:return PIPE_FORMAT_NONE;}}
static enum pipe_format alias_format(unsigned f){switch(f){case 0x32:return PIPE_FORMAT_R16_UNORM;case 0x33:return PIPE_FORMAT_Z24_UNORM_S8_UINT_AS_R8G8B8A8;case 0x34:return PIPE_FORMAT_R32_UINT;default:return PIPE_FORMAT_NONE;}}
static int surface(const struct fd_dev_info *info,const struct fdl_layout*l,enum pipe_format pf,unsigned level,unsigned layer,unsigned layers,uint64_t address,struct nn_depth_a2d_surface*out)
{
 struct fdl_view_args a={};a.iova=address;a.format=pf;a.type=FDL_VIEW_TYPE_2D;a.base_miplevel=level;a.level_count=1;a.base_array_layer=layer;a.layer_count=1;
 for(unsigned i=0;i<4;i++)a.swiz[i]=(enum pipe_swizzle)i;
 const struct fdl_layout*layouts[3]={l,nullptr,nullptr};struct fdl6_view v={};fdl6_view_init<A6XX>(&v,layouts,&a,info->props.has_z24uint_s8uint);
 if(v.ubwc_enabled||!l->base_align||address%l->base_align||!v.width||!v.height||v.width>16384||v.height>16384)return ENOTSUP;
 struct nn_depth_a2d_surface q={};q.source_info=v.TPL1_A2D_SRC_TEXTURE_INFO;q.source_size=v.TPL1_A2D_SRC_TEXTURE_SIZE;q.destination_info=v.RB_A2D_DEST_BUFFER_INFO;
 q.pitch=v.pitch;q.width=v.width;q.height=v.height;q.alignment=l->base_align;q.address=address;q.offset=v.offset;q.bytes=l->size;
 if(l->layer_first&&(uint64_t)l->layer_size*layers>q.bytes)q.bytes=(uint64_t)l->layer_size*layers;
 uint64_t end=(uint64_t)fdl_surface_offset(l,level,layer)+l->slices[level].size0;if(end>q.bytes)q.bytes=end;
 if(q.bytes>UINT32_MAX||address>=(UINT64_C(1)<<49)||q.bytes>(UINT64_C(1)<<49)-address)return EINVAL;
 *out=q;return 0;
}
int nn_depth_a2d(const struct nn_depth_fdl_desc*d,uint64_t linear_address,struct nn_depth_a2d_view*out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!d)return EINVAL;
 struct nn_depth_fdl_view actual;int e=nn_depth_fdl(d,&actual);if(e)return e;
 enum pipe_format dp=depth_format(d->format),ap=alias_format(d->format);if(dp==PIPE_FORMAT_NONE||ap==PIPE_FORMAT_NONE)return ENOTSUP;
 fd_dev_id id={};id.chip_id=d->chip;const auto*info=fd_dev_info_raw(&id);if(!info||!info->props.has_z24uint_s8uint)return ENOTSUP;
 struct fdl_image_params p={};p.format=dp;p.nr_samples=1;p.width0=d->width;p.height0=d->height;p.depth0=1;p.mip_levels=d->levels;p.array_size=d->layers;p.tile_mode=TILE6_3;
 struct fdl_layout tiled={};if(!fdl6_layout_image(&tiled,info,&p,nullptr)||tiled.ubwc||fdl_tile_mode(&tiled,d->base_level)!=TILE6_3)return ENOTSUP;
 struct nn_depth_a2d_view v={};v.desc=*d;v.linear_address=linear_address;v.alias_pipe_format=ap;
 if((e=surface(info,&tiled,ap,d->base_level,d->base_layer,d->layers,d->address,&v.tiled)))return e;
 if(v.tiled.offset!=actual.view_offset||v.tiled.bytes!=actual.required_span)return EPROTO;
 p.format=ap;p.width0=actual.view_width;p.height0=actual.view_height;p.mip_levels=1;p.array_size=1;p.tile_mode=TILE6_LINEAR;
 struct fdl_layout linear={};if(!fdl6_layout_image(&linear,info,&p,nullptr)||linear.ubwc||fdl_tile_mode(&linear,0)!=TILE6_LINEAR)return ENOTSUP;
 if((e=surface(info,&linear,ap,0,0,1,linear_address,&v.linear)))return e;
 if(d->address&&linear_address&&d->address<linear_address+v.linear.bytes&&linear_address<d->address+v.tiled.bytes)return EINVAL;
 enum a6xx_format fmt=fd6_color_format(ap,TILE6_LINEAR);enum a6xx_2d_ifmt ifmt;
 switch(d->format){case 0x32:ifmt=R2D_FLOAT32;break;case 0x33:ifmt=R2D_INT8;break;case 0x34:ifmt=R2D_INT32;break;default:return ENOTSUP;}
 v.blit_control=A6XX_RB_A2D_BLT_CNTL_MASK(0xf)|A6XX_RB_A2D_BLT_CNTL_COLOR_FORMAT(fmt)|A6XX_RB_A2D_BLT_CNTL_IFMT(ifmt)|A6XX_RB_A2D_BLT_CNTL_ROTATE(ROTATE_0);
 v.output_info=A6XX_SP_A2D_OUTPUT_INFO_COLOR_FORMAT(fmt)|A6XX_SP_A2D_OUTPUT_INFO_MASK(0xf)|A6XX_SP_A2D_OUTPUT_INFO_IFMT_TYPE(util_format_is_pure_uint(ap)?OUTPUT_IFMT_2D_UINT:OUTPUT_IFMT_2D_FLOAT);
 *out=v;return 0;
}
int nn_depth_a2d_words(const struct nn_depth_a2d_view*v,uint32_t op,const struct nn_depth_a2d_region*r,float clear_depth,uint32_t*out,size_t cap,size_t*count)
{
 if(!count)return EINVAL;
 *count=0;if(!v||!r)return EINVAL;
 struct nn_depth_a2d_view actual;int e=nn_depth_a2d(&v->desc,v->linear_address,&actual);if(e)return e;
 if(memcmp(v,&actual,sizeof(actual)))return EINVAL;
 if(op>NN_DEPTH_A2D_UPLOAD||!v->desc.address||(op!=NN_DEPTH_A2D_CLEAR&&!v->linear_address)||!r->width||!r->height||r->x>v->tiled.width||r->width>v->tiled.width-r->x||r->y>v->tiled.height||r->height>v->tiled.height-r->y)return EINVAL;
 if(op==NN_DEPTH_A2D_CLEAR&&(!std::isfinite(clear_depth)||clear_depth<0||clear_depth>1))return EINVAL;
 uint32_t words[96];size_t n=0;auto reg=[&](uint32_t a,uint32_t b){words[n++]=nvn_pm4_packet4(a,1);words[n++]=b;};auto addr=[&](uint32_t a,uint64_t b){words[n++]=nvn_pm4_packet4(a,2);words[n++]=(uint32_t)b;words[n++]=b>>32;};
 words[n++]=nvn_pm4_packet7(CP_SET_MARKER,1);words[n++]=A6XX_CP_SET_MARKER_0_MODE(RM6_BLIT2DSCALE);
 reg(REG_A6XX_RB_A2D_BLT_CNTL,v->blit_control|(op==NN_DEPTH_A2D_CLEAR?A6XX_RB_A2D_BLT_CNTL_SOLID_COLOR:0));
 reg(REG_A6XX_GRAS_A2D_BLT_CNTL,v->blit_control|(op==NN_DEPTH_A2D_CLEAR?A6XX_RB_A2D_BLT_CNTL_SOLID_COLOR:0));
 reg(REG_A6XX_SP_A2D_OUTPUT_INFO,v->output_info);reg(REG_A6XX_RB_A2D_PIXEL_CNTL,0);
 const auto&dst=op==NN_DEPTH_A2D_READBACK?v->linear:v->tiled;
 reg(REG_A6XX_RB_A2D_DEST_BUFFER_INFO,dst.destination_info);addr(REG_A6XX_RB_A2D_DEST_BUFFER_BASE,dst.address+dst.offset);reg(REG_A6XX_RB_A2D_DEST_BUFFER_PITCH,A6XX_RB_A2D_DEST_BUFFER_PITCH(dst.pitch));
 reg(REG_A6XX_GRAS_A2D_DEST_TL,A6XX_GRAS_A2D_DEST_TL_X(r->x)|A6XX_GRAS_A2D_DEST_TL_Y(r->y));reg(REG_A6XX_GRAS_A2D_DEST_BR,A6XX_GRAS_A2D_DEST_BR_X(r->x+r->width-1)|A6XX_GRAS_A2D_DEST_BR_Y(r->y+r->height-1));
 if(op==NN_DEPTH_A2D_CLEAR){
  uint32_t bits;memcpy(&bits,&clear_depth,4);uint32_t c[4]={bits,0,0,0};
  if(v->desc.format==0x33){uint32_t z=(uint32_t)(clear_depth*((1u<<24)-1));for(unsigned i=0;i<3;i++)c[i]=(z>>(8*i))&255;}
  for(unsigned i=0;i<4;i++)reg(REG_A6XX_RB_A2D_CLEAR_COLOR_DW0+i,c[i]);
 }else{
  const auto&src=op==NN_DEPTH_A2D_UPLOAD?v->linear:v->tiled;reg(REG_A6XX_TPL1_A2D_SRC_TEXTURE_INFO,src.source_info);reg(REG_A6XX_TPL1_A2D_SRC_TEXTURE_SIZE,src.source_size);addr(REG_A6XX_TPL1_A2D_SRC_TEXTURE_BASE,src.address+src.offset);reg(REG_A6XX_TPL1_A2D_SRC_TEXTURE_PITCH,A6XX_TPL1_A2D_SRC_TEXTURE_PITCH_PITCH(src.pitch));
  reg(REG_A6XX_GRAS_A2D_SRC_XMIN,A6XX_GRAS_A2D_SRC_XMIN(r->x));reg(REG_A6XX_GRAS_A2D_SRC_XMAX,A6XX_GRAS_A2D_SRC_XMAX(r->x+r->width-1));reg(REG_A6XX_GRAS_A2D_SRC_YMIN,A6XX_GRAS_A2D_SRC_YMIN(r->y));reg(REG_A6XX_GRAS_A2D_SRC_YMAX,A6XX_GRAS_A2D_SRC_YMAX(r->y+r->height-1));
 }
 words[n++]=nvn_pm4_packet7(CP_EVENT_WRITE,1);words[n++]=CP_EVENT_WRITE_0_EVENT(DEBUG_LABEL);
 words[n++]=nvn_pm4_packet7(CP_BLIT,1);words[n++]=CP_BLIT_0_OP(BLIT_OP_SCALE);
 if(n>96)return EPROTO;
 if(!out){*count=n;return 0;}
 if(cap<n)return E2BIG;
 memcpy(out,words,n*4);*count=n;return 0;
}
