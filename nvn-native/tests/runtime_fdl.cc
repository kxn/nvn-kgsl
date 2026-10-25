/* SPDX-License-Identifier: MIT. HOST reference only, no GPU or Android run. */
#include "nvn_native/runtime_fdl.h"
#include "freedreno_layout.h"
#include "freedreno_dev_info.h"
#include "fd6_hw.h"
#include "fd6_format_table.h"
#include <nvn.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <initializer_list>
static_assert(NVN_FORMAT_RGBA8==37&&NVN_FORMAT_R32F==10&&NVN_FORMAT_RGBA_DXT1==0x42,"public numeric ABI");
static unsigned checks,images,buffers;
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"fail:%d:%s\n",__LINE__,#x);return 1;}}while(0)
static enum pipe_format format(uint32_t f)
{switch(f){
case NVN_FORMAT_R11G11B10F:return PIPE_FORMAT_R11G11B10_FLOAT;
case NVN_FORMAT_R8:return PIPE_FORMAT_R8_UNORM;
case NVN_FORMAT_R8SN:return PIPE_FORMAT_R8_SNORM;
case NVN_FORMAT_R8UI:return PIPE_FORMAT_R8_UINT;
case NVN_FORMAT_R8I:return PIPE_FORMAT_R8_SINT;
case NVN_FORMAT_R16F:return PIPE_FORMAT_R16_FLOAT;
case NVN_FORMAT_R16:return PIPE_FORMAT_R16_UNORM;
case NVN_FORMAT_R16SN:return PIPE_FORMAT_R16_SNORM;
case NVN_FORMAT_R16UI:return PIPE_FORMAT_R16_UINT;
case NVN_FORMAT_R16I:return PIPE_FORMAT_R16_SINT;
case NVN_FORMAT_R32F:return PIPE_FORMAT_R32_FLOAT;
case NVN_FORMAT_R32UI:return PIPE_FORMAT_R32_UINT;
case NVN_FORMAT_R32I:return PIPE_FORMAT_R32_SINT;
case NVN_FORMAT_RG8:return PIPE_FORMAT_R8G8_UNORM;
case NVN_FORMAT_RG8SN:return PIPE_FORMAT_R8G8_SNORM;
case NVN_FORMAT_RG8UI:return PIPE_FORMAT_R8G8_UINT;
case NVN_FORMAT_RG8I:return PIPE_FORMAT_R8G8_SINT;
case NVN_FORMAT_RG16F:return PIPE_FORMAT_R16G16_FLOAT;
case NVN_FORMAT_RG16:return PIPE_FORMAT_R16G16_UNORM;
case NVN_FORMAT_RG16SN:return PIPE_FORMAT_R16G16_SNORM;
case NVN_FORMAT_RG16UI:return PIPE_FORMAT_R16G16_UINT;
case NVN_FORMAT_RG16I:return PIPE_FORMAT_R16G16_SINT;
case NVN_FORMAT_RG32F:return PIPE_FORMAT_R32G32_FLOAT;
case NVN_FORMAT_RG32UI:return PIPE_FORMAT_R32G32_UINT;
case NVN_FORMAT_RG32I:return PIPE_FORMAT_R32G32_SINT;
case NVN_FORMAT_RGB32F:return PIPE_FORMAT_R32G32B32_FLOAT;
case NVN_FORMAT_RGB32UI:return PIPE_FORMAT_R32G32B32_UINT;
case NVN_FORMAT_RGB32I:return PIPE_FORMAT_R32G32B32_SINT;
case NVN_FORMAT_RGBA8:return PIPE_FORMAT_R8G8B8A8_UNORM;
case NVN_FORMAT_RGBA8SN:return PIPE_FORMAT_R8G8B8A8_SNORM;
case NVN_FORMAT_RGBA8UI:return PIPE_FORMAT_R8G8B8A8_UINT;
case NVN_FORMAT_RGBA8I:return PIPE_FORMAT_R8G8B8A8_SINT;
case NVN_FORMAT_RGBA16F:return PIPE_FORMAT_R16G16B16A16_FLOAT;
case NVN_FORMAT_RGBA16:return PIPE_FORMAT_R16G16B16A16_UNORM;
case NVN_FORMAT_RGBA16SN:return PIPE_FORMAT_R16G16B16A16_SNORM;
case NVN_FORMAT_RGBA16UI:return PIPE_FORMAT_R16G16B16A16_UINT;
case NVN_FORMAT_RGBA16I:return PIPE_FORMAT_R16G16B16A16_SINT;
case NVN_FORMAT_RGBA32F:return PIPE_FORMAT_R32G32B32A32_FLOAT;
case NVN_FORMAT_RGBA32UI:return PIPE_FORMAT_R32G32B32A32_UINT;
case NVN_FORMAT_RGBA32I:return PIPE_FORMAT_R32G32B32A32_SINT;
case NVN_FORMAT_RGBA8_SRGB:return PIPE_FORMAT_R8G8B8A8_SRGB;
case NVN_FORMAT_BGRA8:return PIPE_FORMAT_B8G8R8A8_UNORM;
case NVN_FORMAT_BGRA8_SRGB:return PIPE_FORMAT_B8G8R8A8_SRGB;
case NVN_FORMAT_RGB_DXT1:return PIPE_FORMAT_DXT1_RGB;
case NVN_FORMAT_RGBA_DXT1:return PIPE_FORMAT_DXT1_RGBA;
case NVN_FORMAT_RGBA_DXT3:return PIPE_FORMAT_DXT3_RGBA;
case NVN_FORMAT_RGBA_DXT5:return PIPE_FORMAT_DXT5_RGBA;
case NVN_FORMAT_RGB_DXT1_SRGB:return PIPE_FORMAT_DXT1_SRGB;
case NVN_FORMAT_RGBA_DXT1_SRGB:return PIPE_FORMAT_DXT1_SRGBA;
case NVN_FORMAT_RGBA_DXT3_SRGB:return PIPE_FORMAT_DXT3_SRGBA;
case NVN_FORMAT_RGBA_DXT5_SRGB:return PIPE_FORMAT_DXT5_SRGBA;
default:return PIPE_FORMAT_NONE;}}
static int zero(const void *v,size_t n){const unsigned char *p=(const unsigned char*)v;while(n--)if(*p++)return 0;return 1;}
static int image(const nn_fdl_image_desc &d)
{
 nn_fdl_image_view v;C(!nn_fdl_image(&d,&v));images++;
 fd_dev_id id={};id.chip_id=d.chip;const struct fd_dev_info *info=fd_dev_info_raw(&id);
 fdl_image_params p={};p.format=format(d.format);p.nr_samples=1;p.width0=d.width;p.height0=d.height;p.depth0=1;p.mip_levels=d.levels;p.array_size=d.layers;p.tile_mode=TILE6_LINEAR;
 fdl_layout l={};fdl_explicit_layout e={};e.pitch=d.explicit_pitch;C(fdl6_layout_image(&l,info,&p,d.explicit_pitch?&e:nullptr));
 C(v.bytes==l.size&&v.alignment==l.base_align);uint64_t span=l.layer_first?(uint64_t)l.layer_size*d.layers:l.size;if(span<l.size)span=l.size;C(v.required_span==span);
 C(v.block_width==util_format_get_blockwidth(p.format)&&v.block_height==util_format_get_blockheight(p.format)&&v.bytes_per_block==util_format_get_blocksize(p.format));
 for(unsigned k=0;k<d.levels;k++){C(v.level[k].pitch==fdl_pitch(&l,k));C(v.level[k].offset==fdl_surface_offset(&l,k,0));C(v.level[k].layer_stride==fdl_layer_stride(&l,k));C(v.level[k].span==(uint64_t)fdl_pitch(&l,k)*util_format_get_nblocksy(p.format,u_minify(d.height,k)));}
 fdl_view_args a={};a.iova=d.address;a.base_miplevel=d.base_level;a.level_count=d.level_count;a.base_array_layer=d.base_layer;a.layer_count=d.layer_count;a.min_lod_clamp=(float)d.base_level;a.format=p.format;a.type=FDL_VIEW_TYPE_2D;
 a.swiz[0]=PIPE_SWIZZLE_X;a.swiz[1]=PIPE_SWIZZLE_Y;a.swiz[2]=PIPE_SWIZZLE_Z;a.swiz[3]=PIPE_SWIZZLE_W;const fdl_layout *layouts[3]={&l,nullptr,nullptr};fdl6_view direct={};fdl6_view_init<A6XX>(&direct,layouts,&a,info->props.has_z24uint_s8uint);
 if(!d.address){direct.descriptor[4]&=~(uint32_t)v.descriptor_address_mask;direct.descriptor[5]&=~(uint32_t)(v.descriptor_address_mask>>32);}
 C(!memcmp(v.descriptor,direct.descriptor,64)&&v.view_offset==direct.offset);C(v.view_width==u_minify(d.width,d.base_level)&&v.view_height==u_minify(d.height,d.base_level));
 if(d.usage&NN_FDL_COLOR_TARGET){C(v.color_target&&v.rb_mrt_buf_info==direct.RB_MRT_BUF_INFO&&v.sp_ps_mrt_reg==direct.SP_PS_MRT_REG);C(v.rb_mrt_pitch==A6XX_RB_MRT_PITCH(direct.pitch)&&v.rb_mrt_array_pitch==A6XX_RB_MRT_ARRAY_PITCH(direct.layer_size));}
 else C(!v.color_target&&!v.rb_mrt_buf_info&&!v.rb_mrt_pitch&&!v.rb_mrt_array_pitch&&!v.sp_ps_mrt_reg);
 return 0;
}
static int buffer(uint32_t f,uint32_t bytes,uint64_t address)
{
 nn_fdl_buffer_desc d={1,0x06050002,f,bytes,address};nn_fdl_buffer_view v;C(!nn_fdl_buffer(&d,&v));buffers++;uint32_t direct[16];const uint8_t swiz[4]={0,1,2,3};fdl6_buffer_view_init<A6XX>(direct,format(f),swiz,address,bytes,1,FDL_SSBO_EMULATION_DISABLED);C(!memcmp(v.descriptor,direct,64));C(v.element_bytes==util_format_get_blocksize(format(f))&&v.elements==bytes/v.element_bytes);C(v.base_address+v.required_prefix==address&&v.required_span==v.required_prefix+bytes);return 0;
}
static unsigned volumes;
static int volume(const nn_fdl_volume_desc &d)
{
 nn_fdl_volume_view v;C(!nn_fdl_volume(&d,&v));volumes++;
 fd_dev_id id={};id.chip_id=d.chip;const struct fd_dev_info *info=fd_dev_info_raw(&id);
 fdl_image_params p={};p.format=format(d.format);p.nr_samples=1;p.width0=d.width;p.height0=d.height;p.depth0=d.depth;
 p.is_3d=true;p.mip_levels=d.levels;p.array_size=1;p.tile_mode=TILE6_LINEAR;
 fdl_layout l={};C(fdl6_layout_image(&l,info,&p,nullptr));C(!l.layer_first&&!l.ubwc);
 C(v.schema==1&&v.chip==d.chip&&v.format==d.format&&v.width==d.width&&v.height==d.height&&v.depth==d.depth&&v.levels==d.levels);
 C(v.bytes==l.size&&v.required_span==l.size&&v.alignment==l.base_align);
 C(v.base_level==d.base_level&&v.level_count==d.level_count&&v.view_depth==u_minify(d.depth,d.base_level));
 C(v.view_width==u_minify(d.width,d.base_level)&&v.view_height==u_minify(d.height,d.base_level));
 C(v.block_width==util_format_get_blockwidth(p.format)&&v.block_height==util_format_get_blockheight(p.format)&&v.bytes_per_block==util_format_get_blocksize(p.format));
 for(unsigned k=0;k<d.levels;k++){
  const auto &m=v.level[k];C(m.width==u_minify(d.width,k)&&m.height==u_minify(d.height,k)&&m.depth==u_minify(d.depth,k));
  C(m.pitch==fdl_pitch(&l,k)&&m.slice_stride==fdl_layer_stride(&l,k)&&m.offset==fdl_surface_offset(&l,k,0));
  C(m.span==m.slice_stride*m.depth&&m.offset+m.span<=v.bytes);
  C(m.slice_stride>=uint64_t(m.pitch)*util_format_get_nblocksy(p.format,m.height));
 }
 C(v.min_layer_size==l.slices[d.levels-1].size0);
 fdl_view_args a={};a.iova=d.address;a.base_miplevel=d.base_level;a.level_count=d.level_count;a.base_array_layer=0;a.layer_count=1;
 a.min_lod_clamp=(float)d.base_level;a.format=p.format;a.type=FDL_VIEW_TYPE_3D;
 a.swiz[0]=PIPE_SWIZZLE_X;a.swiz[1]=PIPE_SWIZZLE_Y;a.swiz[2]=PIPE_SWIZZLE_Z;a.swiz[3]=PIPE_SWIZZLE_W;
 const fdl_layout *layouts[3]={&l,nullptr,nullptr};fdl6_view direct={};fdl6_view_init<A6XX>(&direct,layouts,&a,info->props.has_z24uint_s8uint);
 if(!d.address){direct.descriptor[4]&=~(uint32_t)v.descriptor_address_mask;direct.descriptor[5]&=~(uint32_t)(v.descriptor_address_mask>>32);}
 C(v.descriptor_address_word==4&&!memcmp(v.descriptor,direct.descriptor,64)&&v.view_offset==direct.offset);
 C((v.descriptor[3]&A6XX_TEX_MEMOBJ_3_MIN_LAYERSZ__MASK)==A6XX_TEX_MEMOBJ_3_MIN_LAYERSZ(v.min_layer_size));
 if(d.depth>1){
  p.is_3d=false;p.depth0=1;p.array_size=d.depth;fdl_layout array={};C(fdl6_layout_image(&array,info,&p,nullptr));
  C(array.layer_first);a.type=FDL_VIEW_TYPE_2D;a.layer_count=d.depth;const fdl_layout *ar[3]={&array,nullptr,nullptr};fdl6_view av={};fdl6_view_init<A6XX>(&av,ar,&a,info->props.has_z24uint_s8uint);
  if(!d.address){av.descriptor[4]&=~(uint32_t)v.descriptor_address_mask;av.descriptor[5]&=~(uint32_t)(v.descriptor_address_mask>>32);}
  C(memcmp(v.descriptor,av.descriptor,64)!=0); /* Never a 2D array descriptor. */
 }
 return 0;
}
static int volume_cases(void)
{
 C(nn_fdl_volume_metadata_version()==1&&nn_fdl_metadata_version()==1);
 const unsigned dims[][3]={{1,1,1},{1,3,9},{3,1,17},{35,19,9},{32,32,32},{64,64,3},{127,73,17},{8,11,128},{2048,1,1},{1,2048,1},{1,1,2048},{197,113,37}};
 uint32_t seed=20261024;
 for(unsigned f:{37u,0x41u,0x42u,0x43u,0x44u})for(unsigned i=0;i<52;i++){
  unsigned w,h,z;if(i<12){w=dims[i][0];h=dims[i][1];z=dims[i][2];}else{seed=seed*1664525+1013904223;w=1+seed%128;seed=seed*1664525+1013904223;h=1+seed%128;seed=seed*1664525+1013904223;z=1+seed%64;}
  unsigned max=w>h?w:h;if(z>max)max=z;unsigned levels=1;while(max>>=1)levels++;if(i%2==0)levels=1;
  nn_fdl_volume_desc d={};d.schema=1;d.chip=0x06050002;d.format=f;d.width=w;d.height=h;d.depth=z;d.levels=levels;
  d.base_level=levels>1?1:0;d.level_count=levels-d.base_level;d.usage=NN_FDL_SAMPLED;d.address=i%2?UINT64_C(0x123450000):0;C(!volume(d));
  if(levels>1){d.base_level=levels-1;d.level_count=1;d.address=0;C(!volume(d));}
 }
 nn_fdl_volume_desc good={1,0x06050002,37,35,19,9,4,1,3,NN_FDL_SAMPLED,0};nn_fdl_volume_view v;
 C(nn_fdl_volume(nullptr,&v)==EINVAL&&zero(&v,sizeof(v)));C(nn_fdl_volume(&good,nullptr)==EINVAL);
 for(unsigned n=0;n<21;n++){
  auto d=good;int error=ENOTSUP;
  switch(n){case 0:d.schema=2;break;case 1:d.chip=0x43030b00;break;case 2:d.format=NVN_FORMAT_DEPTH16;break;case 3:d.width=0;break;case 4:d.height=0;break;case 5:d.depth=0;break;case 6:d.width=2049;break;case 7:d.height=2049;break;case 8:d.depth=2049;break;case 9:d.levels=0;break;case 10:d.levels=16;break;case 11:d.levels=7;error=EINVAL;break;case 12:d.base_level=4;error=EINVAL;break;case 13:d.level_count=0;error=EINVAL;break;case 14:d.level_count=4;error=EINVAL;break;case 15:d.usage=3;break;case 16:d.usage=0;break;case 17:d.address=1;error=EINVAL;break;case 18:d.address=UINT64_C(1)<<49;error=EINVAL;break;case 19:d.address=(UINT64_C(1)<<49)-4096;error=EINVAL;break;case 20:d.width=d.height=d.depth=2048;d.levels=1;d.base_level=0;d.level_count=1;error=E2BIG;break;}
  memset(&v,0xa5,sizeof(v));C(nn_fdl_volume(&d,&v)==error&&zero(&v,sizeof(v)));
 }
 return 0;
}
/* Public NVN semantic traits are checked separately from the PIPE lookup:
 * channel count/width, normalized versus signed/unsigned integer/float, sRGB,
 * and BGRA component order. Original FDL performs every layout/descriptor. */
enum semantic_kind {kind_unorm,kind_snorm,kind_uint,kind_sint,kind_floating,kind_srgb};
struct color_case {uint32_t nvn;unsigned channels,bits;semantic_kind kind;};
static const color_case colors[]={
 {NVN_FORMAT_R8,1,8,kind_unorm},
 {NVN_FORMAT_R8SN,1,8,kind_snorm},
 {NVN_FORMAT_R8UI,1,8,kind_uint},
 {NVN_FORMAT_R8I,1,8,kind_sint},
 {NVN_FORMAT_R16F,1,16,kind_floating},
 {NVN_FORMAT_R16,1,16,kind_unorm},
 {NVN_FORMAT_R16SN,1,16,kind_snorm},
 {NVN_FORMAT_R16UI,1,16,kind_uint},
 {NVN_FORMAT_R16I,1,16,kind_sint},
 {NVN_FORMAT_R32F,1,32,kind_floating},
 {NVN_FORMAT_R32UI,1,32,kind_uint},
 {NVN_FORMAT_R32I,1,32,kind_sint},
 {NVN_FORMAT_RG8,2,8,kind_unorm},
 {NVN_FORMAT_RG8SN,2,8,kind_snorm},
 {NVN_FORMAT_RG8UI,2,8,kind_uint},
 {NVN_FORMAT_RG8I,2,8,kind_sint},
 {NVN_FORMAT_RG16F,2,16,kind_floating},
 {NVN_FORMAT_RG16,2,16,kind_unorm},
 {NVN_FORMAT_RG16SN,2,16,kind_snorm},
 {NVN_FORMAT_RG16UI,2,16,kind_uint},
 {NVN_FORMAT_RG16I,2,16,kind_sint},
 {NVN_FORMAT_RG32F,2,32,kind_floating},
 {NVN_FORMAT_RG32UI,2,32,kind_uint},
 {NVN_FORMAT_RG32I,2,32,kind_sint},
 {NVN_FORMAT_RGBA8,4,8,kind_unorm},
 {NVN_FORMAT_RGBA8SN,4,8,kind_snorm},
 {NVN_FORMAT_RGBA8UI,4,8,kind_uint},
 {NVN_FORMAT_RGBA8I,4,8,kind_sint},
 {NVN_FORMAT_RGBA16F,4,16,kind_floating},
 {NVN_FORMAT_RGBA16,4,16,kind_unorm},
 {NVN_FORMAT_RGBA16SN,4,16,kind_snorm},
 {NVN_FORMAT_RGBA16UI,4,16,kind_uint},
 {NVN_FORMAT_RGBA16I,4,16,kind_sint},
 {NVN_FORMAT_RGBA32F,4,32,kind_floating},
 {NVN_FORMAT_RGBA32UI,4,32,kind_uint},
 {NVN_FORMAT_RGBA32I,4,32,kind_sint},
 {NVN_FORMAT_RGBA8_SRGB,4,8,kind_srgb},
 {NVN_FORMAT_BGRA8,4,8,kind_unorm},
 {NVN_FORMAT_BGRA8_SRGB,4,8,kind_srgb},
};
static_assert(sizeof(colors)/sizeof(colors[0])==39,"nondepth color mapping count");
static_assert(NVN_FORMAT_RGBA16F==0x29&&NVN_FORMAT_RGBA32F==0x2e&&NVN_FORMAT_RG16F==0x11&&NVN_FORMAT_RG32F==0x16,"public float format ABI");
static int extended_format_cases(void)
{
 unsigned previous_checks=checks,previous_images=images,previous_volumes=volumes;
 fd_dev_id id={};id.chip_id=0x06050002;const struct fd_dev_info *info=fd_dev_info_raw(&id);C(info);
 uint32_t seed=0x43fd1201;
 for(const auto &f:colors){
  const auto pipe=format(f.nvn);C(pipe!=PIPE_FORMAT_NONE);
  const auto *trait=util_format_description(pipe);C(trait);
  C(util_format_get_nr_components(pipe)==f.channels&&trait->channel[0].size==f.bits);
  C(util_format_get_blockwidth(pipe)==1&&util_format_get_blockheight(pipe)==1);
  C(util_format_get_blocksize(pipe)==f.channels*f.bits/8);
  C(util_format_is_pure_uint(pipe)==(f.kind==kind_uint));C(util_format_is_pure_sint(pipe)==(f.kind==kind_sint));
  C(util_format_is_float(pipe)==(f.kind==kind_floating));C(util_format_is_snorm(pipe)==(f.kind==kind_snorm));
  C(util_format_is_unorm(pipe)==(f.kind==kind_unorm||f.kind==kind_srgb));C(util_format_is_srgb(pipe)==(f.kind==kind_srgb));
  C(!util_format_is_depth_or_stencil(pipe)&&!util_format_is_compressed(pipe));
  C(fd6_texture_format_supported(info,pipe,TILE6_LINEAR,false));C(fd6_color_format_supported(info,pipe,TILE6_LINEAR));
  bool bgra=f.nvn==NVN_FORMAT_BGRA8||f.nvn==NVN_FORMAT_BGRA8_SRGB;
  C(trait->swizzle[0]==(bgra?PIPE_SWIZZLE_Z:PIPE_SWIZZLE_X));
  if(f.channels==4)C(trait->swizzle[2]==(bgra?PIPE_SWIZZLE_X:PIPE_SWIZZLE_Z));
  for(unsigned i=0;i<32;i++){
   const unsigned dims[][2]={{1,1},{197,113},{2880,1620},{16384,1},{1,16384},{8191,17},{127,73}};
   unsigned w,h;if(i<7){w=dims[i][0];h=dims[i][1];}else{seed=seed*1664525+1013904223;w=1+seed%2048;seed=seed*1664525+1013904223;h=1+seed%1024;}
   unsigned max=w>h?w:h,levels=1;while(max>>=1)levels++;if(i%2==0)levels=1;
   nn_fdl_image_desc d={};d.schema=1;d.chip=id.chip_id;d.format=f.nvn;d.target=i%3?1:4;d.width=w;d.height=h;d.levels=levels;d.layers=d.target==4?3:1;d.base_level=levels>1?1:0;d.level_count=levels-d.base_level;d.base_layer=d.layers-1;d.layer_count=1;d.usage=NN_FDL_SAMPLED;d.address=i%2?UINT64_C(0x123450000):0;C(!image(d));
   d.usage=NN_FDL_COLOR_TARGET;d.level_count=1;C(!image(d));
  }
  for(unsigned i=0;i<16;i++){
   const unsigned dims[][3]={{1,1,1},{35,19,9},{32,32,32},{127,73,17},{8,11,128},{2048,1,1},{1,2048,1},{1,1,2048}};
   unsigned w,h,z;if(i<8){w=dims[i][0];h=dims[i][1];z=dims[i][2];}else{seed=seed*1664525+1013904223;w=1+seed%128;seed=seed*1664525+1013904223;h=1+seed%64;seed=seed*1664525+1013904223;z=1+seed%32;}
   unsigned max=w>h?w:h;if(z>max)max=z;unsigned levels=1;while(max>>=1)levels++;if(i%2==0)levels=1;
   nn_fdl_volume_desc d={};d.schema=1;d.chip=id.chip_id;d.format=f.nvn;d.width=w;d.height=h;d.depth=z;d.levels=levels;d.base_level=levels>1?1:0;d.level_count=levels-d.base_level;d.usage=NN_FDL_SAMPLED;d.address=i%2?UINT64_C(0x123450000):0;C(!volume(d));
   d.usage=NN_FDL_COLOR_TARGET;nn_fdl_volume_view rejected;memset(&rejected,0xa5,sizeof(rejected));C(nn_fdl_volume(&d,&rejected)==ENOTSUP&&zero(&rejected,sizeof(rejected)));
  }
  if(f.nvn!=NVN_FORMAT_RGBA8){
   nn_fdl_image_desc d={};d.schema=1;d.chip=id.chip_id;d.format=f.nvn;d.target=1;d.width=d.height=d.levels=d.layers=d.level_count=d.layer_count=1;d.usage=NN_FDL_SAMPLED;d.explicit_pitch=64;nn_fdl_image_view rejected;C(nn_fdl_image(&d,&rejected)==ENOTSUP&&zero(&rejected,sizeof(rejected)));
  }
 }
 for(uint32_t f:{uint32_t(NVN_FORMAT_RGB_DXT1),uint32_t(NVN_FORMAT_RGBA_DXT1),uint32_t(NVN_FORMAT_RGBA_DXT3),uint32_t(NVN_FORMAT_RGBA_DXT5),uint32_t(NVN_FORMAT_STENCIL8),uint32_t(NVN_FORMAT_DEPTH16),uint32_t(NVN_FORMAT_DEPTH24),uint32_t(NVN_FORMAT_DEPTH32F),uint32_t(NVN_FORMAT_DEPTH24_STENCIL8),uint32_t(NVN_FORMAT_DEPTH32F_STENCIL8)}){
  nn_fdl_image_desc d={};d.schema=1;d.chip=id.chip_id;d.format=f;d.target=1;d.width=d.height=64;d.levels=d.layers=d.level_count=d.layer_count=1;d.usage=NN_FDL_COLOR_TARGET;nn_fdl_image_view rejected;memset(&rejected,0xa5,sizeof(rejected));C(nn_fdl_image(&d,&rejected)==ENOTSUP&&zero(&rejected,sizeof(rejected)));
 }
 printf("HOST extendedFDL checks=%u addedImages=%u addedVolumes=%u commonColorFormats=39 totalImageFormats=43 newMetadataFormats=38; AndroidExecution=false GPU=false\n",checks-previous_checks,images-previous_images,volumes-previous_volumes);
 return 0;
}

static int packed_color_cases(void)
{
 const uint32_t f=NVN_FORMAT_R11G11B10F;const auto pf=format(f);C(pf==PIPE_FORMAT_R11G11B10_FLOAT);
 const auto *trait=util_format_description(pf);C(trait&&trait->nr_channels==3);
 C(trait->channel[0].size==11&&trait->channel[1].size==11&&trait->channel[2].size==10);
 C(util_format_get_blocksize(pf)==4&&util_format_is_float(pf));
 for(unsigned i=0;i<12;i++){
  nn_fdl_image_desc d={};d.schema=1;d.chip=0x06050002;d.format=f;d.target=i%2?1:4;
  d.width=i%3?197:2880;d.height=i%3?113:1620;d.levels=i%2?1:8;d.layers=d.target==4?3:1;
  d.base_level=d.levels>1?2:0;d.level_count=1;d.base_layer=d.layers-1;d.layer_count=1;
  d.usage=NN_FDL_SAMPLED|NN_FDL_COLOR_TARGET;d.address=i%2?UINT64_C(0x123450000):0;C(!image(d));
 }
 for(unsigned i=0;i<4;i++){
  nn_fdl_volume_desc d={};d.schema=1;d.chip=0x06050002;d.format=f;
  d.width=i?35:1;d.height=i?19:1;d.depth=i?9:1;d.levels=i?4:1;
  d.base_level=i?1:0;d.level_count=d.levels-d.base_level;d.usage=NN_FDL_SAMPLED;
  d.address=i%2?UINT64_C(0x123450000):0;C(!volume(d));
 }
 nn_fdl_buffer_desc b={};b.schema=1;b.chip=0x06050002;b.format=f;b.bytes=256;
 nn_fdl_buffer_view v;memset(&v,0xa5,sizeof(v));C(nn_fdl_buffer(&b,&v)==ENOTSUP&&zero(&v,sizeof(v)));
 return 0;
}

static int srgb_bc_cases(void)
{
 for(unsigned f=0x45;f<=0x48;f++)for(unsigned i=0;i<12;i++){
  nn_fdl_image_desc d={};d.schema=1;d.chip=0x06050002;d.format=f;d.target=i%3?1:4;d.width=i%2?127:37;d.height=i%2?65:21;d.levels=i%2?1:3;d.layers=d.target==4?3:1;d.base_level=d.levels-1;d.level_count=1;d.base_layer=d.layers-1;d.layer_count=1;d.usage=NN_FDL_SAMPLED;d.address=i%2?UINT64_C(0x123450000):0;
  C(!image(d));nn_fdl_image_view v;C(!nn_fdl_image(&d,&v));
  C(util_format_is_srgb(format(f)));C(v.descriptor[0]&A6XX_TEX_MEMOBJ_0_SRGB);
  auto linear=d;linear.format=f-4;nn_fdl_image_view u;C(!nn_fdl_image(&linear,&u));
  C(v.bytes==u.bytes&&v.required_span==u.required_span&&v.block_width==4&&v.block_height==4&&v.bytes_per_block==(f<0x47?8u:16u));
  C(!(u.descriptor[0]&A6XX_TEX_MEMOBJ_0_SRGB));
  for(unsigned level=0;level<d.levels;level++)C(!memcmp(v.level+level,u.level+level,sizeof(v.level[0])));
  auto bad=d;bad.usage=NN_FDL_COLOR_TARGET;memset(&u,0xa5,sizeof(u));C(nn_fdl_image(&bad,&u)==ENOTSUP&&zero(&u,sizeof(u)));
  nn_fdl_volume_desc vd={1,d.chip,f,32,32,3,3,i%3,1,NN_FDL_SAMPLED,d.address};C(!volume(vd));nn_fdl_volume_view vv;C(!nn_fdl_volume(&vd,&vv));C(vv.descriptor[0]&A6XX_TEX_MEMOBJ_0_SRGB);
 }
 return 0;
}
static int buffer_family_cases(void)
{
 const unsigned start=checks,old=buffers;
 for(unsigned f=1;f<=154;f++){
  if(!((f>=1&&f<=24)||(f>=34&&f<=48)||f==154))continue;
  unsigned cpp=util_format_get_blocksize(format(f));C(cpp>0);
  for(unsigned count:{1u,7u,32767u,32768u,65536u,65537u})for(unsigned prefix:{0u,cpp,(63u/cpp)*cpp}){
   C(!buffer(f,count*cpp,UINT64_C(0x123450000)+prefix));
  }
  C(!buffer(f,(UINT32_MAX/cpp>0x3fffffffu?0x3fffffffu:UINT32_MAX/cpp)*cpp,0));
  nn_fdl_buffer_desc d={1,0x06050002,f,cpp,1};nn_fdl_buffer_view v;memset(&v,0xa5,sizeof(v));
  if(cpp>1)C(nn_fdl_buffer(&d,&v)==EINVAL&&zero(&v,sizeof(v)));
  if(cpp==1){d.address=0;d.bytes=0x40000000;C(nn_fdl_buffer(&d,&v)==E2BIG&&zero(&v,sizeof(v)));}
  if(cpp==12){d.address=4;C(nn_fdl_buffer(&d,&v)==EINVAL&&zero(&v,sizeof(v)));d.address=12;C(!nn_fdl_buffer(&d,&v));}
  if(cpp>1){d.address=0;d.bytes=cpp-1;C(nn_fdl_buffer(&d,&v)==EINVAL&&zero(&v,sizeof(v)));}
 }
 for(unsigned f:{0u,25u,29u,49u,50u,56u,57u,61u,64u,65u,66u,156u,0xffffffffu}){
  nn_fdl_buffer_desc d={1,0x06050002,f,16,0};nn_fdl_buffer_view v;memset(&v,0xa5,sizeof(v));
  C(nn_fdl_buffer(&d,&v)==ENOTSUP&&zero(&v,sizeof(v)));
 }
 printf("HOST bufferFDL family checks=%u descriptors=%u formats=40; GPU=false AndroidExecution=false\n",checks-start,buffers-old);
 return 0;
}
static int sampled_format_cases(void)
{
 unsigned count=0,floating=0,integer=0,compressed=0;
 for(unsigned f=0;f<200;f++){
  struct nn_fdl_image_desc d={};d.schema=1;d.chip=0x06050002;d.format=f;d.target=1;d.width=3;d.height=5;d.levels=d.layers=d.level_count=d.layer_count=1;d.usage=NN_FDL_SAMPLED;
  struct nn_fdl_image_view view;int available=nn_fdl_image(&d,&view);struct nn_fdl_format_info v;memset(&v,0xa5,sizeof(v));int e=nn_fdl_image_format_info(d.chip,f,&v);
  if(available){C(e==ENOTSUP&&zero(&v,sizeof(v)));continue;}
  C(!e&&v.schema==1&&v.chip==d.chip&&v.format==f);enum pipe_format pf=format(f);const struct util_format_description *actual=util_format_description(pf);
  C(v.block_width==view.block_width&&v.block_height==view.block_height&&v.bytes_per_block==view.bytes_per_block);
  unsigned result=util_format_is_pure_uint(pf)?NN_FDL_RESULT_UINT:util_format_is_pure_sint(pf)?NN_FDL_RESULT_SINT:NN_FDL_RESULT_FLOAT;
  C(v.result_type==result&&v.snorm==util_format_is_snorm(pf)&&v.normalized==unsigned(util_format_is_unorm(pf)||util_format_is_snorm(pf))&&v.srgb==util_format_is_srgb(pf));
  for(unsigned i=0;i<4;i++)C(v.channel_bits[i]==actual->channel[i].size);
  count++;if(v.block_width>1)compressed++;else if(result==NN_FDL_RESULT_FLOAT)floating++;else integer++;
 }
 struct nn_fdl_format_info bad;memset(&bad,0xa5,sizeof(bad));C(nn_fdl_image_format_info(0x43030b00,37,&bad)==ENOTSUP&&zero(&bad,sizeof(bad)));C(nn_fdl_image_format_info(0x06050002,37,nullptr)==EINVAL);
 C(count==48&&floating==22&&integer==18&&compressed==8);
 printf("HOST actual FDL sampled traits checks include48 formats Float22 Integer18 BC8; GPU=false AndroidExecution=false\n");return 0;
}
int main(void)
{
 C(!sampled_format_cases());
 C(!buffer_family_cases());
 C(!volume_cases());
 C(nn_fdl_metadata_version()==1);const unsigned dims[][2]={{1,1},{2,2},{3,5},{64,64},{127,73},{2880,1620},{197,113},{8191,17},{16384,1},{1,16384},{16384,16384}};
 const uint32_t formats[]={37,0x41,0x42,0x43,0x44};uint32_t seed=20261024;
 for(unsigned f:formats)for(unsigned i=0;i<75;i++){
  unsigned w,h;if(i<11){w=dims[i][0];h=dims[i][1];}else{seed=seed*1664525+1013904223;w=1+seed%2048;seed=seed*1664525+1013904223;h=1+seed%2048;}
  unsigned max=w>h?w:h,levels=1;while(max>>=1)levels++;if(i%2==0)levels=1;
  nn_fdl_image_desc d={};d.schema=1;d.chip=0x06050002;d.format=f;d.target=i%3?1:4;d.width=w;d.height=h;d.levels=levels;d.layers=d.target==4&&i>10?3:1;d.base_level=levels>1?1:0;d.level_count=levels-d.base_level;d.base_layer=d.layers-1;d.layer_count=1;d.usage=NN_FDL_SAMPLED;d.address=i%2?UINT64_C(0x123450000):0;C(!image(d));
  if(f==37){d.usage|=NN_FDL_COLOR_TARGET;d.level_count=1;C(!image(d));}
 }
 for(unsigned w:{1u,197u,2880u})for(unsigned extra:{0u,64u}){
  nn_fdl_image_desc d={};d.schema=1;d.chip=0x06050002;d.format=37;d.target=1;d.width=w;d.height=113;d.levels=d.layers=d.level_count=d.layer_count=1;d.usage=3;d.explicit_pitch=((w*4+63)/64)*64+extra;C(!image(d));
 }
 for(unsigned f:{37u,10u})for(unsigned bytes:{4u,8u,124u,128u,131072u,0xfffffffcu})for(uint64_t address:{UINT64_C(0),UINT64_C(0x123450000),UINT64_C(0x123450004),UINT64_C(0x12345003c)})C(!buffer(f,bytes,address));
 nn_fdl_image_desc good={};good.schema=1;good.chip=0x06050002;good.format=37;good.target=1;good.width=good.height=64;good.levels=good.layers=good.level_count=good.layer_count=1;good.usage=3;nn_fdl_image_view v;
 C(nn_fdl_image(nullptr,&v)==EINVAL&&zero(&v,sizeof(v)));C(nn_fdl_image(&good,nullptr)==EINVAL);
 for(unsigned n=0;n<18;n++){
  auto d=good;int error=ENOTSUP;
  switch(n){case 0:d.schema=2;break;case 1:d.chip=0x43030b00;break;case 2:d.format=NVN_FORMAT_DEPTH16;break;case 3:d.target=2;break;case 4:d.width=0;break;case 5:d.width=16385;break;case 6:d.levels=16;break;case 7:d.layers=0;break;case 8:d.layers=2;break;case 9:d.base_level=1;error=EINVAL;break;case 10:d.level_count=2;error=EINVAL;break;case 11:d.base_layer=1;error=EINVAL;break;case 12:d.layer_count=2;error=EINVAL;break;case 13:d.usage=4;break;case 14:d.format=0x42;break;case 15:d.address=1;error=EINVAL;break;case 16:d.explicit_pitch=257;break;case 17:d.width=d.height=16384;d.layers=2048;d.target=4;error=E2BIG;break;}
  C(nn_fdl_image(&d,&v)==error&&zero(&v,sizeof(v)));
 }
 nn_fdl_buffer_desc b={1,0x06050002,37,4,0};nn_fdl_buffer_view bv;C(nn_fdl_buffer(nullptr,&bv)==EINVAL&&zero(&bv,sizeof(bv)));C(nn_fdl_buffer(&b,nullptr)==EINVAL);
 for(unsigned i=0;i<6;i++){auto d=b;int error=EINVAL;switch(i){case 0:d.format=0x42;error=ENOTSUP;break;case 1:d.bytes=0;break;case 2:d.bytes=3;break;case 3:d.address=1;break;case 4:d.address=UINT64_C(1)<<49;break;case 5:d.address=(UINT64_C(1)<<49)-4;d.bytes=8;break;}C(nn_fdl_buffer(&d,&bv)==error&&zero(&bv,sizeof(bv)));}
 printf("HOST runtimeFDL checks=%u images=%u buffers=%u volumes=%u; AndroidExecution=false GPU=false\n",checks,images,buffers,volumes);
 C(!extended_format_cases());C(!packed_color_cases());C(!srgb_bc_cases());return 0;
}
