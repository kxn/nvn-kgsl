/* SPDX-License-Identifier: MIT
 * Descriptor facts: pinned Mesa26.2.5 a6xx_descriptors.xml10-46,
 * a6xx_enums.xml386-412; Gallium fd6_texture.cc37-78,253-301.
 * Pure encoder, no Gallium/graphics API runtime and no generated-header build
 * dependency. Reproduction tests compare every field to original XML packers.
 */
#include "nvn_native/sampler_state.h"
#include <nvn.h>
#include <errno.h>
#include <math.h>
#include <string.h>
static int wrap(uint32_t v)
{
 switch(v){case NVN_WRAP_MODE_REPEAT:return 0;case NVN_WRAP_MODE_MIRRORED_REPEAT:return 2;case NVN_WRAP_MODE_CLAMP_TO_EDGE:return 1;default:return -1;}
}
static int aniso(float v)
{
 if(v==1)return 0;
 if(v==2)return 1;
 if(v==4)return 2;
 if(v==8)return 3;
 if(v==16)return 4;
 return -1;
}
int nn_sampler_state_validate(const struct nn_sampler_state *s)
{
 if(!s||s->version!=NN_SAMPLER_STATE_VERSION)return EINVAL;
 if(s->min_filter>NVN_MIN_FILTER_LINEAR_MIPMAP_LINEAR||s->mag_filter>NVN_MAG_FILTER_LINEAR||s->compare_func<NVN_COMPARE_FUNC_NEVER||s->compare_func>NVN_COMPARE_FUNC_ALWAYS)return EINVAL;
 if(wrap(s->wrap[0])<0||wrap(s->wrap[1])<0||wrap(s->wrap[2])<0||s->compare_mode!=NVN_COMPARE_MODE_NONE||s->reduction!=NVN_SAMPLER_REDUCTION_AVERAGE||s->lod_snap!=0)return ENOTSUP;
 if(!isfinite(s->lod_min)||!isfinite(s->lod_max)||!isfinite(s->lod_bias)||!isfinite(s->anisotropy)||s->lod_min>s->lod_max)return EINVAL;
 if(s->lod_min < -1000 || s->lod_max>1000 || s->lod_bias < -16 || s->lod_bias>4095.0f/256 || aniso(s->anisotropy)<0)return ENOTSUP;
 /* Every accepted wrap mode avoids border fetches. Preserve the public color
  * snapshot without constraining its bits or emitting an unused border table.
  * Border-consuming modes still fail above before any descriptor is written. */
 return 0;
}
static uint32_t lod(float v)
{ if(v<0)v=0;if(v>4095.0f/256)v=4095.0f/256;return (uint32_t)(v*256); }
static int encode(uint32_t chip,const struct nn_sampler_state *s,const struct nn_sampler_layout *l,uint32_t words[4],int mipmapped,int depth_noncompare)
{
 if(!words)return EINVAL;
 int e=nn_sampler_state_validate(s);if(e)return e;
 if(chip!=0x06050002||!l||!l->width||l->width>16384||!l->height||l->height>16384||!l->levels||l->levels>15||l->samples!=1||l->dimensions!=2||l->normalized!=1||l->depth!=(uint32_t)depth_noncompare||l->integer)return ENOTSUP;
 /* Current multi-level admission is explicitly non-mip nearest/clamp.
  * FD6's PIPE_TEX_MIPFILTER_NONE sets the same FAR bit below independently
  * of resource level count. Actual descriptor base/range comes from FDL. */
 if(mipmapped&&l->levels>1&&s->anisotropy!=1)return ENOTSUP;
 if(!mipmapped&&l->levels>1&&(s->min_filter!=NVN_MIN_FILTER_NEAREST||s->mag_filter!=NVN_MAG_FILTER_NEAREST||
    s->wrap[0]!=NVN_WRAP_MODE_CLAMP_TO_EDGE||s->wrap[1]!=NVN_WRAP_MODE_CLAMP_TO_EDGE||
    s->wrap[2]!=NVN_WRAP_MODE_CLAMP_TO_EDGE))return ENOTSUP;
 unsigned a=(unsigned)aniso(s->anisotropy),min_linear=s->min_filter&1u,mag_linear=s->mag_filter==NVN_MAG_FILTER_LINEAR;
 uint32_t out[4]={0};
 out[0]=(s->min_filter>=NVN_MIN_FILTER_NEAREST_MIPMAP_LINEAR)|
     ((mag_linear?(a?2u:1u):0u)<<1)|((min_linear?(a?2u:1u):0u)<<3)|
     ((uint32_t)wrap(s->wrap[0])<<5)|((uint32_t)wrap(s->wrap[1])<<8)|((uint32_t)wrap(s->wrap[2])<<11)|(a<<14)|
     (((uint32_t)(int32_t)(s->lod_bias*256)&0x1fffu)<<19);
 out[1]=(1u<<4)|(s->min_filter<=NVN_MIN_FILTER_LINEAR?1u<<6:0)|(lod(s->lod_max)<<8)|(lod(s->lod_min)<<20);
 out[2]=(min_linear&&mag_linear)?1u<<5:0;
 memcpy(words,out,sizeof(out));return 0;
}

/* Explicit candidate profile: the words use the same Gallium/XML encoding.
 * New profile does not authenticate dimensions or imply GPU qualification. */
int nn_sampler_state_encode(uint32_t chip,const struct nn_sampler_state *s,const struct nn_sampler_layout *l,uint32_t words[4])
{return encode(chip,s,l,words,0,0);}
int nn_sampler_state_encode_mipmapped(uint32_t chip,const struct nn_sampler_state *s,const struct nn_sampler_layout *l,uint32_t words[4])
{return encode(chip,s,l,words,1,0);}

/* Gallium fd6 sampler descriptor has no independent depth-format field.
 * Noncompare depth changes only the resource descriptor, not these sampler
 * words. Keep an explicit role rather than bypassing the color-layout guard. */
int nn_sampler_state_encode_depth_noncompare(uint32_t chip,const struct nn_sampler_state*s,const struct nn_sampler_layout*l,uint32_t words[4])
{return encode(chip,s,l,words,0,1);}
