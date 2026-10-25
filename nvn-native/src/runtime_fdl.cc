/* SPDX-License-Identifier: MIT. Bounded C facade over original Mesa FDL. */
#include "nvn_native/runtime_fdl.h"
#include "nvn_native/texture_buffer_format.h"
#include "freedreno_layout.h"
#include "freedreno_dev_info.h"
#include "fd6_hw.h"
#include "fd6_format_table.h"
#include "util/format/u_format.h"
#include <cerrno>
#include <cstring>
static_assert(sizeof(nn_fdl_image_desc)==64&&sizeof(nn_fdl_image_view)==680&&sizeof(nn_fdl_buffer_desc)==24&&sizeof(nn_fdl_buffer_view)==120,"fixed C ABI");
static enum pipe_format image_format(uint32_t format)
{
 switch(format){
 case 0x1:return PIPE_FORMAT_R8_UNORM; /* NVN_FORMAT_R8 */
 case 0x2:return PIPE_FORMAT_R8_SNORM; /* NVN_FORMAT_R8SN */
 case 0x3:return PIPE_FORMAT_R8_UINT; /* NVN_FORMAT_R8UI */
 case 0x4:return PIPE_FORMAT_R8_SINT; /* NVN_FORMAT_R8I */
 case 0x5:return PIPE_FORMAT_R16_FLOAT; /* NVN_FORMAT_R16F */
 case 0x6:return PIPE_FORMAT_R16_UNORM; /* NVN_FORMAT_R16 */
 case 0x7:return PIPE_FORMAT_R16_SNORM; /* NVN_FORMAT_R16SN */
 case 0x8:return PIPE_FORMAT_R16_UINT; /* NVN_FORMAT_R16UI */
 case 0x9:return PIPE_FORMAT_R16_SINT; /* NVN_FORMAT_R16I */
 case 0xa:return PIPE_FORMAT_R32_FLOAT; /* NVN_FORMAT_R32F */
 case 0xb:return PIPE_FORMAT_R32_UINT; /* NVN_FORMAT_R32UI */
 case 0xc:return PIPE_FORMAT_R32_SINT; /* NVN_FORMAT_R32I */
 case 0xd:return PIPE_FORMAT_R8G8_UNORM; /* NVN_FORMAT_RG8 */
 case 0xe:return PIPE_FORMAT_R8G8_SNORM; /* NVN_FORMAT_RG8SN */
 case 0xf:return PIPE_FORMAT_R8G8_UINT; /* NVN_FORMAT_RG8UI */
 case 0x10:return PIPE_FORMAT_R8G8_SINT; /* NVN_FORMAT_RG8I */
 case 0x11:return PIPE_FORMAT_R16G16_FLOAT; /* NVN_FORMAT_RG16F */
 case 0x12:return PIPE_FORMAT_R16G16_UNORM; /* NVN_FORMAT_RG16 */
 case 0x13:return PIPE_FORMAT_R16G16_SNORM; /* NVN_FORMAT_RG16SN */
 case 0x14:return PIPE_FORMAT_R16G16_UINT; /* NVN_FORMAT_RG16UI */
 case 0x15:return PIPE_FORMAT_R16G16_SINT; /* NVN_FORMAT_RG16I */
 case 0x16:return PIPE_FORMAT_R32G32_FLOAT; /* NVN_FORMAT_RG32F */
 case 0x17:return PIPE_FORMAT_R32G32_UINT; /* NVN_FORMAT_RG32UI */
 case 0x18:return PIPE_FORMAT_R32G32_SINT; /* NVN_FORMAT_RG32I */
 case 0x25:return PIPE_FORMAT_R8G8B8A8_UNORM; /* NVN_FORMAT_RGBA8 */
 case 0x26:return PIPE_FORMAT_R8G8B8A8_SNORM; /* NVN_FORMAT_RGBA8SN */
 case 0x27:return PIPE_FORMAT_R8G8B8A8_UINT; /* NVN_FORMAT_RGBA8UI */
 case 0x28:return PIPE_FORMAT_R8G8B8A8_SINT; /* NVN_FORMAT_RGBA8I */
 case 0x29:return PIPE_FORMAT_R16G16B16A16_FLOAT; /* NVN_FORMAT_RGBA16F */
 case 0x2a:return PIPE_FORMAT_R16G16B16A16_UNORM; /* NVN_FORMAT_RGBA16 */
 case 0x2b:return PIPE_FORMAT_R16G16B16A16_SNORM; /* NVN_FORMAT_RGBA16SN */
 case 0x2c:return PIPE_FORMAT_R16G16B16A16_UINT; /* NVN_FORMAT_RGBA16UI */
 case 0x2d:return PIPE_FORMAT_R16G16B16A16_SINT; /* NVN_FORMAT_RGBA16I */
 case 0x2e:return PIPE_FORMAT_R32G32B32A32_FLOAT; /* NVN_FORMAT_RGBA32F */
 case 0x2f:return PIPE_FORMAT_R32G32B32A32_UINT; /* NVN_FORMAT_RGBA32UI */
 case 0x30:return PIPE_FORMAT_R32G32B32A32_SINT; /* NVN_FORMAT_RGBA32I */
 case 0x3f:return PIPE_FORMAT_R11G11B10_FLOAT; /* NVN_FORMAT_R11G11B10F; IMAGE only */
 case 0x38:return PIPE_FORMAT_R8G8B8A8_SRGB; /* NVN_FORMAT_RGBA8_SRGB */
 case 0x9a:return PIPE_FORMAT_B8G8R8A8_UNORM; /* NVN_FORMAT_BGRA8 */
 case 0x9c:return PIPE_FORMAT_B8G8R8A8_SRGB; /* NVN_FORMAT_BGRA8_SRGB */
 case 0x41:return PIPE_FORMAT_DXT1_RGB; /* NVN_FORMAT_RGB_DXT1 */
 case 0x42:return PIPE_FORMAT_DXT1_RGBA; /* NVN_FORMAT_RGBA_DXT1 */
 case 0x43:return PIPE_FORMAT_DXT3_RGBA; /* NVN_FORMAT_RGBA_DXT3 */
 case 0x44:return PIPE_FORMAT_DXT5_RGBA; /* NVN_FORMAT_RGBA_DXT5 */
 case 0x45:return PIPE_FORMAT_DXT1_SRGB; /* NVN_FORMAT_RGB_DXT1_SRGB */
 case 0x46:return PIPE_FORMAT_DXT1_SRGBA; /* NVN_FORMAT_RGBA_DXT1_SRGB */
 case 0x47:return PIPE_FORMAT_DXT3_SRGBA; /* NVN_FORMAT_RGBA_DXT3_SRGB */
 case 0x48:return PIPE_FORMAT_DXT5_SRGBA; /* NVN_FORMAT_RGBA_DXT5_SRGB */
 default:return PIPE_FORMAT_NONE;
 }
}
static const struct fd_dev_info *device(uint32_t chip)
{if(chip!=0x06050002)return nullptr;fd_dev_id id={};id.chip_id=chip;return fd_dev_info_raw(&id);}
static uint64_t address_mask(void)
{return uint64_t(A6XX_TEX_MEMOBJ_4_BASE_LO__MASK)|(uint64_t(A6XX_TEX_MEMOBJ_5_BASE_HI__MASK)<<32);}
int nn_fdl_image_format_info(uint32_t chip,uint32_t f,struct nn_fdl_format_info *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));const struct fd_dev_info *info=device(chip);enum pipe_format format=image_format(f);
 if(!info||format==PIPE_FORMAT_NONE||util_format_is_depth_or_stencil(format)||
    !fd6_texture_format_supported(info,format,TILE6_LINEAR,false))return ENOTSUP;
 const struct util_format_description *d=util_format_description(format);if(!d)return EINVAL;
 struct nn_fdl_format_info value={};value.schema=1;value.chip=chip;value.format=f;
 value.result_type=util_format_is_pure_uint(format)?NN_FDL_RESULT_UINT:util_format_is_pure_sint(format)?NN_FDL_RESULT_SINT:NN_FDL_RESULT_FLOAT;
 value.block_width=util_format_get_blockwidth(format);value.block_height=util_format_get_blockheight(format);value.bytes_per_block=util_format_get_blocksize(format);
 value.snorm=util_format_is_snorm(format);value.normalized=util_format_is_unorm(format)||value.snorm;value.srgb=util_format_is_srgb(format);
 for(unsigned i=0;i<4;i++)value.channel_bits[i]=d->channel[i].size;
 *out=value;return 0;
}
uint32_t nn_fdl_metadata_version(void){return 1;}
int nn_fdl_image(const struct nn_fdl_image_desc *d,struct nn_fdl_image_view *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!d)return EINVAL;
 const struct fd_dev_info *info=device(d->chip);enum pipe_format format=image_format(d->format);
 if(d->schema!=1||!info||format==PIPE_FORMAT_NONE||(d->target!=1&&d->target!=4)||
    !d->width||d->width>16384||!d->height||d->height>16384||!d->levels||d->levels>15||
    !d->layers||d->layers>2048||(d->target==1&&d->layers!=1)||!d->usage||(d->usage&~3u))return ENOTSUP;
 unsigned maxdim=d->width>d->height?d->width:d->height,maxlevels=1;while(maxdim>>=1)maxlevels++;
 if(d->levels>maxlevels||d->base_level>=d->levels||!d->level_count||d->level_count>d->levels-d->base_level||
    d->base_layer>=d->layers||!d->layer_count||d->layer_count>d->layers-d->base_layer)return EINVAL;
 /* Format admission comes from original Mesa traits and the chip table.
  * Metadata success does not qualify a shader value type or GPU consumer. */
 if(!fd6_texture_format_supported(info,format,TILE6_LINEAR,false)||util_format_is_depth_or_stencil(format))return ENOTSUP;
 if((d->usage&NN_FDL_COLOR_TARGET)&&
    (util_format_is_compressed(format)||!fd6_color_format_supported(info,format,TILE6_LINEAR)||
     d->level_count!=1||d->layer_count!=1))return ENOTSUP;
 if(d->explicit_pitch&&(d->format!=37||d->levels!=1||d->layers!=1||d->explicit_pitch<(uint64_t)d->width*4||d->explicit_pitch>0x3fffff))return ENOTSUP;
 fdl_image_params p={};p.format=format;p.nr_samples=1;p.width0=d->width;p.height0=d->height;p.depth0=1;p.mip_levels=d->levels;p.array_size=d->layers;p.tile_mode=TILE6_LINEAR;p.ubwc=false;
 fdl_explicit_layout explicit_layout={};explicit_layout.pitch=d->explicit_pitch;fdl_layout l={};
 if(!fdl6_layout_image(&l,info,&p,d->explicit_pitch?&explicit_layout:nullptr)||l.ubwc||!l.base_align)return ENOTSUP;
 /* Original FDL surface offsets/slices are uint32. Keep the first public C
  * facade below4GiB instead of allowing truncation in FDL offset accessors. */
 if(!l.size||l.size>UINT32_MAX)return E2BIG;
 struct nn_fdl_image_view v={};v.schema=1;v.chip=d->chip;v.format=d->format;v.target=d->target;v.width=d->width;v.height=d->height;v.levels=d->levels;v.layers=d->layers;
 v.block_width=util_format_get_blockwidth(format);v.block_height=util_format_get_blockheight(format);v.bytes_per_block=util_format_get_blocksize(format);v.alignment=l.base_align;v.bytes=v.required_span=l.size;
 if(l.layer_first&&(uint64_t)l.layer_size*d->layers>v.required_span)v.required_span=(uint64_t)l.layer_size*d->layers;
 for(unsigned level=0;level<d->levels;level++){
  if(fdl_tile_mode(&l,level)!=TILE6_LINEAR)return ENOTSUP;
  struct nn_fdl_level *m=v.level+level;m->width=u_minify(d->width,level);m->height=u_minify(d->height,level);m->pitch=fdl_pitch(&l,level);m->layer_stride=fdl_layer_stride(&l,level);m->offset=fdl_surface_offset(&l,level,0);m->span=(uint64_t)m->pitch*util_format_get_nblocksy(format,m->height);
  uint64_t end=m->offset+(uint64_t)m->layer_stride*(d->layers-1)+m->span;
  if(m->pitch<util_format_get_stride(format,m->width)||m->layer_stride<m->span)return EINVAL;
  if(end>v.required_span)v.required_span=end;
 }
 if(v.required_span>UINT32_MAX)return E2BIG;
 if(d->address%l.base_align||d->address>=(UINT64_C(1)<<49)||v.required_span>(UINT64_C(1)<<49)-d->address)return EINVAL;
 fdl_view_args a={};a.iova=d->address;a.format=format;a.type=FDL_VIEW_TYPE_2D;a.base_miplevel=d->base_level;a.level_count=d->level_count;a.base_array_layer=d->base_layer;a.layer_count=d->layer_count;a.min_lod_clamp=(float)d->base_level;
 a.swiz[0]=PIPE_SWIZZLE_X;a.swiz[1]=PIPE_SWIZZLE_Y;a.swiz[2]=PIPE_SWIZZLE_Z;a.swiz[3]=PIPE_SWIZZLE_W;
 const fdl_layout *layouts[3]={&l,nullptr,nullptr};fdl6_view view={};fdl6_view_init<A6XX>(&view,layouts,&a,info->props.has_z24uint_s8uint);if(view.ubwc_enabled)return ENOTSUP;
 v.base_level=d->base_level;v.level_count=d->level_count;v.base_layer=d->base_layer;v.layer_count=d->layer_count;v.view_width=u_minify(d->width,d->base_level);v.view_height=u_minify(d->height,d->base_level);v.view_offset=view.offset;v.descriptor_address_word=4;v.descriptor_address_mask=address_mask();memcpy(v.descriptor,view.descriptor,sizeof(v.descriptor));
 if(!d->address){v.descriptor[4]&=~(uint32_t)v.descriptor_address_mask;v.descriptor[5]&=~(uint32_t)(v.descriptor_address_mask>>32);}
 if(d->usage&NN_FDL_COLOR_TARGET){v.color_target=1;v.rb_mrt_buf_info=view.RB_MRT_BUF_INFO;v.rb_mrt_pitch=A6XX_RB_MRT_PITCH(view.pitch);v.rb_mrt_array_pitch=A6XX_RB_MRT_ARRAY_PITCH(view.layer_size);v.sp_ps_mrt_reg=view.SP_PS_MRT_REG;}
 *out=v;return 0;
}
/* Additional formats here are BUFFER-only metadata. Do not widen image/volume
 * consumers by treating this typed linear range as a height-one image. */
static enum pipe_format buffer_format(uint32_t f)
{
 /* This separate 40-format BUFFER subset must not grow when image_format
  * gains packed/depth/compressed mappings. No packed BUFFER GPU admission. */
 if(!((f>=1&&f<=24)||(f>=34&&f<=48)||f==0x9a))return PIPE_FORMAT_NONE;
 switch(f){
 case 0x22:return PIPE_FORMAT_R32G32B32_FLOAT;
 case 0x23:return PIPE_FORMAT_R32G32B32_UINT;
 case 0x24:return PIPE_FORMAT_R32G32B32_SINT;
 default:return image_format(f);
 }
}
int nn_fdl_buffer(const struct nn_fdl_buffer_desc *d,struct nn_fdl_buffer_view *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!d)return EINVAL;
 const struct fd_dev_info *info=device(d->chip);uint32_t cpp=0;
 if(d->schema!=1||!info||nn_tex_buffer_element_bytes(d->format,&cpp))return ENOTSUP;
 enum pipe_format format=buffer_format(d->format);
 if(format==PIPE_FORMAT_NONE||!util_format_is_plain(format)||util_format_is_depth_or_stencil(format)||
    util_format_is_srgb(format)||util_format_get_blockwidth(format)!=1||util_format_get_blockheight(format)!=1||
    util_format_get_blocksize(format)!=cpp||!fd6_texture_format_supported(info,format,TILE6_LINEAR,false))return ENOTSUP;
 /* Actual FDL base is aligned down64; STARTOFFSET is in whole elements.
  * For 12B RGB32, public alignment4 does not imply every address is native
  * representable. Keep CPU construction independent and fail metadata here. */
 if(!d->bytes||d->bytes%cpp||(d->address&63)%cpp||d->address>=(UINT64_C(1)<<49)||d->bytes>(UINT64_C(1)<<49)-d->address)return EINVAL;
 if(d->bytes/cpp>0x3fffffffu)return E2BIG; /* actual native WIDTH15+HEIGHT15 */
 const uint8_t swiz[4]={PIPE_SWIZZLE_X,PIPE_SWIZZLE_Y,PIPE_SWIZZLE_Z,PIPE_SWIZZLE_W};struct nn_fdl_buffer_view v={};
 v.schema=1;v.chip=d->chip;v.format=d->format;v.element_bytes=util_format_get_blocksize(format);v.elements=d->bytes/v.element_bytes;
 fdl6_buffer_view_init<A6XX>(v.descriptor,format,swiz,d->address,d->bytes,1,FDL_SSBO_EMULATION_DISABLED);
 v.base_address=d->address&~UINT64_C(63);v.required_prefix=d->address-v.base_address;v.required_span=v.required_prefix+d->bytes;v.address_mask=address_mask();*out=v;return 0;
}

/* Keep the v1 image/buffer functions and their byte layouts unchanged. */
static_assert(sizeof(nn_fdl_volume_desc)==48&&sizeof(nn_fdl_volume_level)==40&&sizeof(nn_fdl_volume_view)==768,"separate volume C ABI");
uint32_t nn_fdl_volume_metadata_version(void){return 1;}
int nn_fdl_volume(const struct nn_fdl_volume_desc *d,struct nn_fdl_volume_view *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!d)return EINVAL;
 const struct fd_dev_info *info=device(d->chip);enum pipe_format format=image_format(d->format);
 if(d->schema!=1||!info||format==PIPE_FORMAT_NONE||d->usage!=NN_FDL_SAMPLED||
    !d->width||!d->height||!d->depth||d->width>2048||d->height>2048||d->depth>2048||
    !d->levels||d->levels>NN_FDL_LEVELS)return ENOTSUP;
 if(!fd6_texture_format_supported(info,format,TILE6_LINEAR,false)||util_format_is_depth_or_stencil(format))return ENOTSUP;
 unsigned maxdim=d->width>d->height?d->width:d->height;if(d->depth>maxdim)maxdim=d->depth;
 unsigned maxlevels=1;while(maxdim>>=1)maxlevels++;
 if(d->levels>maxlevels||d->base_level>=d->levels||!d->level_count||d->level_count>d->levels-d->base_level)return EINVAL;
 fdl_image_params p={};p.format=format;p.nr_samples=1;p.width0=d->width;p.height0=d->height;p.depth0=d->depth;
 p.is_3d=true;p.mip_levels=d->levels;p.array_size=1;p.tile_mode=TILE6_LINEAR;p.ubwc=false;
 fdl_layout l={};if(!fdl6_layout_image(&l,info,&p,nullptr)||l.ubwc||l.layer_first||!l.base_align)return ENOTSUP;
 if(!l.size||l.size>UINT32_MAX)return E2BIG;
 struct nn_fdl_volume_view v={};v.schema=1;v.chip=d->chip;v.format=d->format;v.width=d->width;v.height=d->height;v.depth=d->depth;
 v.levels=d->levels;v.base_level=d->base_level;v.level_count=d->level_count;v.bytes=v.required_span=l.size;v.alignment=l.base_align;
 v.block_width=util_format_get_blockwidth(format);v.block_height=util_format_get_blockheight(format);v.bytes_per_block=util_format_get_blocksize(format);
 for(unsigned i=0;i<d->levels;i++){
  if(fdl_tile_mode(&l,i)!=TILE6_LINEAR)return ENOTSUP;
  struct nn_fdl_volume_level *m=v.level+i;m->width=u_minify(d->width,i);m->height=u_minify(d->height,i);m->depth=u_minify(d->depth,i);
  m->pitch=fdl_pitch(&l,i);m->offset=fdl_surface_offset(&l,i,0);m->slice_stride=fdl_layer_stride(&l,i);m->span=m->slice_stride*m->depth;
  uint64_t active=(uint64_t)m->pitch*util_format_get_nblocksy(format,m->height);
  if(m->pitch<util_format_get_stride(format,m->width)||m->slice_stride<active||m->offset>v.bytes||m->span>v.bytes-m->offset)return EINVAL;
 }
 if(d->address%l.base_align||d->address>=(UINT64_C(1)<<49)||v.required_span>(UINT64_C(1)<<49)-d->address)return EINVAL;
 fdl_view_args a={};a.iova=d->address;a.format=format;a.type=FDL_VIEW_TYPE_3D;a.base_miplevel=d->base_level;a.level_count=d->level_count;
 a.base_array_layer=0;a.layer_count=1;a.min_lod_clamp=(float)d->base_level;
 a.swiz[0]=PIPE_SWIZZLE_X;a.swiz[1]=PIPE_SWIZZLE_Y;a.swiz[2]=PIPE_SWIZZLE_Z;a.swiz[3]=PIPE_SWIZZLE_W;
 const fdl_layout *layouts[3]={&l,nullptr,nullptr};fdl6_view view={};fdl6_view_init<A6XX>(&view,layouts,&a,info->props.has_z24uint_s8uint);
 if(view.ubwc_enabled)return ENOTSUP;
 v.view_width=u_minify(d->width,d->base_level);v.view_height=u_minify(d->height,d->base_level);v.view_depth=u_minify(d->depth,d->base_level);
 v.view_offset=view.offset;v.descriptor_address_word=4;v.descriptor_address_mask=address_mask();v.min_layer_size=l.slices[d->levels-1].size0;
 memcpy(v.descriptor,view.descriptor,sizeof(v.descriptor));
 if(!d->address){v.descriptor[4]&=~(uint32_t)v.descriptor_address_mask;v.descriptor[5]&=~(uint32_t)(v.descriptor_address_mask>>32);}
 *out=v;return 0;
}
