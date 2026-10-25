/* SPDX-License-Identifier: MIT. HOST only actual common FDL view/pack output.
 * No CPU pixels, shader compiler, API runtime or runtime layout approximation. */
#include "freedreno_layout.h"
#include "freedreno_dev_info.h"
#include "fd6_hw.h"
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstdint>
int main(int argc,char **argv)
{
 if(argc!=3&&argc!=4)return EINVAL;
 unsigned w=strtoul(argv[1],nullptr,0),h=strtoul(argv[2],nullptr,0),pitch=argc==4?strtoul(argv[3],nullptr,0):0;
 if(!w||w>16384||!h||h>16384)return EINVAL;
 fd_dev_id id={};id.chip_id=0x06050002;const struct fd_dev_info *info=fd_dev_info_raw(&id);if(!info)return ENOTSUP;
 fdl_image_params p={};p.format=PIPE_FORMAT_R8G8B8A8_UNORM;p.nr_samples=1;p.width0=w;p.height0=h;p.depth0=1;p.mip_levels=1;p.array_size=1;p.tile_mode=TILE6_LINEAR;
 fdl_explicit_layout explicit_layout={};explicit_layout.pitch=pitch;
 fdl_layout layout={};if(!fdl6_layout_image(&layout,info,&p,pitch?&explicit_layout:nullptr)||layout.ubwc||layout.size>UINT32_MAX||fdl_tile_mode(&layout,0)!=TILE6_LINEAR)return ENOTSUP;
 const fdl_layout *layouts[3]={&layout,nullptr,nullptr};fdl_view_args a={};a.level_count=a.layer_count=1;a.format=p.format;a.type=FDL_VIEW_TYPE_2D;
 a.swiz[0]=PIPE_SWIZZLE_X;a.swiz[1]=PIPE_SWIZZLE_Y;a.swiz[2]=PIPE_SWIZZLE_Z;a.swiz[3]=PIPE_SWIZZLE_W;
 fdl6_view v={};fdl6_view_init<A6XX>(&v,layouts,&a,info->props.has_z24uint_s8uint);
 if(v.offset||v.ubwc_enabled)return ENOTSUP;
 uint64_t mask=uint64_t(A6XX_TEX_MEMOBJ_4_BASE_LO__MASK)|(uint64_t(A6XX_TEX_MEMOBJ_5_BASE_HI__MASK)<<32);
 printf("{\"schema\":1,\"chip\":%u,\"format\":37,\"target\":1,\"levels\":1,\"layers\":1,\"samples\":1,\"tile_mode\":%u,\"ubwc\":false,\"width\":%u,\"height\":%u,\"pitch\":%u,\"layer_bytes\":%u,\"bytes\":%llu,\"alignment\":%u,\"required_span_bytes\":%llu,\"descriptor_address_word\":4,\"descriptor_address_mask\":%llu,\"descriptor_offset\":%u,\"rb_mrt_buf_info\":%u,\"rb_mrt_pitch\":%u,\"rb_mrt_array_pitch\":%u,\"sp_ps_mrt_reg\":%u,\"texture_descriptor\":[",(unsigned)id.chip_id,(unsigned)TILE6_LINEAR,w,h,fdl_pitch(&layout,0),fdl_layer_stride(&layout,0),(unsigned long long)layout.size,layout.base_align,(unsigned long long)(layout.size>fdl_layer_stride(&layout,0)?layout.size:fdl_layer_stride(&layout,0)),(unsigned long long)mask,v.offset,v.RB_MRT_BUF_INFO,A6XX_RB_MRT_PITCH(fdl_pitch(&layout,0)),A6XX_RB_MRT_ARRAY_PITCH(fdl_layer_stride(&layout,0)),v.SP_PS_MRT_REG);
 for(unsigned i=0;i<16;i++)printf("%s%u",i?",":"",v.descriptor[i]);
 puts("]}");return 0;
}
