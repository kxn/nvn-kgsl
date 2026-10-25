/* HOST-only exact descriptor comparison to original Mesa XML-generated packs. */
#include "nvn_native/sampler_state.h"
#include <nvn.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "adreno/adreno_common.xml.h"
#include "adreno/a6xx_enums.xml.h"
#include "adreno/a6xx_descriptors.xml.h"
static unsigned checks;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);return 1;}}while(0)
static enum a6xx_tex_clamp clamp(uint32_t v)
{switch(v){case NVN_WRAP_MODE_REPEAT:return A6XX_TEX_REPEAT;case NVN_WRAP_MODE_MIRRORED_REPEAT:return A6XX_TEX_MIRROR_REPEAT;default:return A6XX_TEX_CLAMP_TO_EDGE;}}
static float lod(float x){return x<0?0:x>4095.0f/256?4095.0f/256:x;}
int main(void)
{
 struct nn_sampler_state s={.version=1,.min_filter=1,.mag_filter=1,.wrap={1,1,1},.compare_func=2,.lod_max=1000,.anisotropy=1};
 struct nn_sampler_layout l={16384,16384,1,1,2,1,0,0};uint32_t out[4],expected[4];unsigned encoded=0;
 const uint32_t wraps[]={NVN_WRAP_MODE_REPEAT,NVN_WRAP_MODE_MIRRORED_REPEAT,NVN_WRAP_MODE_CLAMP_TO_EDGE};
 const float ranges[][2]={{0,0},{0,1000},{-1000,1000},{1.25f,3.5f}},biases[]={-16,-.5f,0,.5f,4095.0f/256};
 for(unsigned min=0;min<6;min++)for(unsigned mag=0;mag<2;mag++)for(unsigned x=0;x<3;x++)for(unsigned y=0;y<3;y++)for(unsigned z=0;z<3;z++)for(unsigned level=0;level<5;level++)for(unsigned r=0;r<4;r++)for(unsigned b=0;b<5;b++){
  s.min_filter=min;s.mag_filter=mag;s.wrap[0]=wraps[x];s.wrap[1]=wraps[y];s.wrap[2]=wraps[z];unsigned a=0;const unsigned levels[]={1,2,4,8,15};l.levels=levels[level];s.anisotropy=1;s.lod_min=ranges[r][0];s.lod_max=ranges[r][1];s.lod_bias=biases[b];
  /* No supported address mode reads a border, independently of color bits. */
  const uint32_t color_bits[4]={0x3f400000u,0x7fc12345u,0xff800000u,0x80000000u};
  memcpy(s.border,color_bits,sizeof(color_bits));
  enum a6xx_tex_filter mn=(min&1)?(a?A6XX_TEX_ANISO:A6XX_TEX_LINEAR):A6XX_TEX_NEAREST;
  enum a6xx_tex_filter mg=mag?(a?A6XX_TEX_ANISO:A6XX_TEX_LINEAR):A6XX_TEX_NEAREST;
  expected[0]=(min>=4?A6XX_TEX_SAMP_0_MIPFILTER_LINEAR_NEAR:0)|A6XX_TEX_SAMP_0_XY_MAG(mg)|A6XX_TEX_SAMP_0_XY_MIN(mn)|A6XX_TEX_SAMP_0_ANISO((enum a6xx_tex_aniso)a)|A6XX_TEX_SAMP_0_WRAP_S(clamp(wraps[x]))|A6XX_TEX_SAMP_0_WRAP_T(clamp(wraps[y]))|A6XX_TEX_SAMP_0_WRAP_R(clamp(wraps[z]))|A6XX_TEX_SAMP_0_LOD_BIAS(biases[b]);
  expected[1]=(min<=1?A6XX_TEX_SAMP_1_MIPFILTER_LINEAR_FAR:0)|A6XX_TEX_SAMP_1_CUBEMAPSEAMLESSFILTOFF|A6XX_TEX_SAMP_1_MIN_LOD(lod(s.lod_min))|A6XX_TEX_SAMP_1_MAX_LOD(lod(s.lod_max));
  expected[2]=((min&1)&&mag)?A6XX_TEX_SAMP_2_CHROMA_LINEAR:0;expected[3]=0;
  T(!nn_sampler_state_encode_mipmapped(0x06050002,&s,&l,out));T(!memcmp(out,expected,sizeof(out)));encoded++;
  uint32_t legacy[4];int error=nn_sampler_state_encode(0x06050002,&s,&l,legacy);
  if(l.levels==1||(min==NVN_MIN_FILTER_NEAREST&&!mag&&x==2&&y==2&&z==2)){T(!error);T(!memcmp(legacy,out,16));}
  else T(error==ENOTSUP);
 }
 l.levels=1;
 struct nn_sampler_state good={.version=1,.min_filter=1,.mag_filter=1,.wrap={1,1,1},.compare_func=2,.lod_max=1000,.anisotropy=1},bad;
#define BAD(change,error) do{bad=good;change;memset(out,0xa5,sizeof(out));memcpy(expected,out,sizeof(out));T(nn_sampler_state_encode_mipmapped(0x06050002,&bad,&l,out)==(error));T(!memcmp(out,expected,sizeof(out)));}while(0)
 BAD(bad.version=0,EINVAL);BAD(bad.min_filter=6,EINVAL);BAD(bad.mag_filter=2,EINVAL);
 BAD(bad.wrap[0]=NVN_WRAP_MODE_CLAMP_TO_BORDER,ENOTSUP);BAD(bad.wrap[1]=NVN_WRAP_MODE_CLAMP,ENOTSUP);BAD(bad.wrap[2]=NVN_WRAP_MODE_MIRROR_CLAMP_TO_EDGE,ENOTSUP);
 BAD(bad.compare_mode=NVN_COMPARE_MODE_COMPARE_R_TO_TEXTURE,ENOTSUP);BAD(bad.compare_func=0,EINVAL);BAD(bad.reduction=NVN_SAMPLER_REDUCTION_MIN,ENOTSUP);
 BAD(bad.lod_min=2;bad.lod_max=1,EINVAL);BAD(bad.lod_min=NAN,EINVAL);BAD(bad.lod_max=INFINITY,EINVAL);BAD(bad.lod_bias=NAN,EINVAL);BAD(bad.anisotropy=NAN,EINVAL);
 BAD(bad.lod_bias=16,ENOTSUP);BAD(bad.lod_bias=-16.1f,ENOTSUP);BAD(bad.anisotropy=3,ENOTSUP);BAD(bad.border[1]=1;bad.wrap[0]=NVN_WRAP_MODE_CLAMP_TO_BORDER,ENOTSUP);BAD(bad.lod_snap=.5f,ENOTSUP);
 struct nn_sampler_layout layout=l;
#define LAYOUT(field,value) do{layout=l;layout.field=value;T(nn_sampler_state_encode_mipmapped(0x06050002,&good,&layout,out)==ENOTSUP);}while(0)
 LAYOUT(levels,0);LAYOUT(levels,16);LAYOUT(samples,2);LAYOUT(depth,1);LAYOUT(integer,1);LAYOUT(normalized,0);LAYOUT(dimensions,3);LAYOUT(width,0);LAYOUT(height,16385);
 T(nn_sampler_state_encode_mipmapped(0x43030b00,&good,&l,out)==ENOTSUP);T(nn_sampler_state_encode_mipmapped(0x06050002,NULL,&l,out)==EINVAL);T(nn_sampler_state_encode_mipmapped(0x06050002,&good,&l,NULL)==EINVAL);
 layout=l;layout.levels=8;good.wrap[0]=good.wrap[1]=good.wrap[2]=NVN_WRAP_MODE_CLAMP_TO_EDGE;good.min_filter=good.mag_filter=0;
 T(!nn_sampler_state_encode_mipmapped(0x06050002,&good,&layout,out));
 for(unsigned a=2;a<=16;a*=2){good.anisotropy=(float)a;memset(out,0xa5,sizeof(out));memcpy(expected,out,sizeof(out));T(nn_sampler_state_encode_mipmapped(0x06050002,&good,&layout,out)==ENOTSUP);T(!memcmp(out,expected,sizeof(out)));T(!nn_sampler_state_encode(0x06050002,&good,&layout,out));}
 printf("{\"passed\":true,\"checks\":%u,\"generated_packer_cases\":%u,\"gpu_submits\":0}\n",checks,encoded);return 0;
}
