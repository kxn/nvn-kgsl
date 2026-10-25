/* SPDX-License-Identifier: MIT. Original Mesa conversion of ONE constant. */
#include "nvn_native/clear_color.h"
#include "nvn.h"
#include "util/format/u_format.h"
#include "util/format/u_format_pack.h"
#include "util/format/u_format_other.h"
#include <errno.h>
#include <math.h>
#include <string.h>
typedef void (*pack_function)(uint8_t *,unsigned,const float *,unsigned,unsigned,unsigned);
static int format(uint32_t n,enum pipe_format *p,pack_function *pack)
{
#define F(N,P,S) case NVN_FORMAT_##N:*p=PIPE_FORMAT_##P;*pack=util_format_##S##_pack_rgba_float;return 0
 switch(n){
 F(R8,R8_UNORM,r8_unorm);F(R8SN,R8_SNORM,r8_snorm);
 F(R16F,R16_FLOAT,r16_float);F(R16,R16_UNORM,r16_unorm);F(R16SN,R16_SNORM,r16_snorm);F(R32F,R32_FLOAT,r32_float);
 F(RG8,R8G8_UNORM,r8g8_unorm);F(RG8SN,R8G8_SNORM,r8g8_snorm);
 F(RG16F,R16G16_FLOAT,r16g16_float);F(RG16,R16G16_UNORM,r16g16_unorm);F(RG16SN,R16G16_SNORM,r16g16_snorm);F(RG32F,R32G32_FLOAT,r32g32_float);
 F(RGBA8,R8G8B8A8_UNORM,r8g8b8a8_unorm);F(RGBA8SN,R8G8B8A8_SNORM,r8g8b8a8_snorm);
 F(RGBA16F,R16G16B16A16_FLOAT,r16g16b16a16_float);F(RGBA16,R16G16B16A16_UNORM,r16g16b16a16_unorm);
 F(RGBA16SN,R16G16B16A16_SNORM,r16g16b16a16_snorm);F(RGBA32F,R32G32B32A32_FLOAT,r32g32b32a32_float);
 F(BGRA8,B8G8R8A8_UNORM,b8g8r8a8_unorm);F(RGBA8_SRGB,R8G8B8A8_SRGB,r8g8b8a8_srgb);
 F(BGRA8_SRGB,B8G8R8A8_SRGB,b8g8r8a8_srgb);F(R11G11B10F,R11G11B10_FLOAT,r11g11b10_float);
 default:return ENOTSUP;
 }
#undef F
}
int nn_clear_color_pattern(uint32_t n,const float rgba[4],uint32_t mask,struct nn_clear_color_pattern *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!rgba||mask>15)return EINVAL;
 for(unsigned i=0;i<4;i++)if(!isfinite(rgba[i]))return ENOTSUP;
 enum pipe_format f;pack_function pack;int e=format(n,&f,&pack);if(e)return e;
 const struct util_format_description *d=util_format_description(f);
 if(!d||d->block.width!=1||d->block.height!=1||d->block.depth!=1||
    !d->block.bits||d->block.bits>128||d->block.bits%8)return ENOTSUP;
 unsigned cpp=d->block.bits/8;if(cpp&(cpp-1))return ENOTSUP;
 /* Original packers may require aligned uint16/uint32 output. */
 uint32_t texel[4]={0},selected[4]={0};pack((uint8_t*)texel,cpp,rgba,16,1,1);
 for(unsigned logical=0;logical<4;logical++)if(mask&(1u<<logical)){
  unsigned channel=d->swizzle[logical];if(channel>=4)continue;
  unsigned first=d->channel[channel].shift,bits=d->channel[channel].size;
  if(first>cpp*8||bits>cpp*8-first)return EINVAL;
  for(unsigned bit=first;bit<first+bits;bit++)selected[bit/32]|=1u<<(bit%32);
 }
 struct nn_clear_color_pattern p={.bytes_per_pixel=cpp};
 for(unsigned byte=0;byte<16;byte++){
  ((uint8_t*)p.pattern)[byte]=((const uint8_t*)texel)[byte%cpp];
  ((uint8_t*)p.mask)[byte]=((const uint8_t*)selected)[byte%cpp];
 }
 *out=p;return 0;
}
