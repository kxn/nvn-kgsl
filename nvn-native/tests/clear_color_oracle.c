/* SPDX-License-Identifier: MIT. HOST ONLY expected images, never Android. */
#include "clear_color_cases.h"
#include "public_copy_cases.h"
#include "util/format/u_format.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
static enum pipe_format pf(unsigned f)
{
 switch(f){
 case 1:return PIPE_FORMAT_R8_UNORM;case 2:return PIPE_FORMAT_R8_SNORM;case 5:return PIPE_FORMAT_R16_FLOAT;
 case 6:return PIPE_FORMAT_R16_UNORM;case 7:return PIPE_FORMAT_R16_SNORM;case 10:return PIPE_FORMAT_R32_FLOAT;
 case 13:return PIPE_FORMAT_R8G8_UNORM;case 14:return PIPE_FORMAT_R8G8_SNORM;case 17:return PIPE_FORMAT_R16G16_FLOAT;
 case 18:return PIPE_FORMAT_R16G16_UNORM;case 19:return PIPE_FORMAT_R16G16_SNORM;case 22:return PIPE_FORMAT_R32G32_FLOAT;
 case 37:return PIPE_FORMAT_R8G8B8A8_UNORM;case 38:return PIPE_FORMAT_R8G8B8A8_SNORM;case 41:return PIPE_FORMAT_R16G16B16A16_FLOAT;
 case 42:return PIPE_FORMAT_R16G16B16A16_UNORM;case 43:return PIPE_FORMAT_R16G16B16A16_SNORM;case 46:return PIPE_FORMAT_R32G32B32A32_FLOAT;
 case 154:return PIPE_FORMAT_B8G8R8A8_UNORM;case 56:return PIPE_FORMAT_R8G8B8A8_SRGB;case 156:return PIPE_FORMAT_B8G8R8A8_SRGB;
 case 63:return PIPE_FORMAT_R11G11B10_FLOAT;default:abort();
 }
}
int main(int argc,char **argv)
{
 if(argc!=2||mkdir(argv[1],0700))return 1;
 unsigned files=0;uint64_t bytes=0;
 for(unsigned n=0;n<NN_CLEAR_PROBE_CASES;n++){
  struct nn_clear_probe_case c;if(nn_clear_probe_case(n,&c))return 2;
  uint32_t color[2][4]={{0}},mask[2][4]={{0}};
  const struct util_format_description *d=util_format_description(pf(c.format));
  for(unsigned op=0;op<2;op++){
   util_format_pack_description(pf(c.format))->pack_rgba_float((uint8_t*)color[op],c.cpp,c.color[op],16,1,1);
   /* Independent logical-to-storage mapping for the selected format family;
    * neither the runtime constant adapter nor the shader is invoked. */
   unsigned channels=c.format==63?3:(c.format<=10?1:c.format<=22?2:4);
   for(unsigned logical=0;logical<channels;logical++)if(c.mask[op]&(1u<<logical)){
    unsigned storage=(c.format==154||c.format==156)&&logical<3?2-logical:logical;
    unsigned bits=c.format==63?(logical==2?10:11):c.cpp*8/channels;
    unsigned first=c.format==63?logical*11:storage*bits;
    for(unsigned bit=first;bit<first+bits;bit++)mask[op][bit/32]|=1u<<(bit%32);
   }
  }
  if(!d||d->block.bits!=c.cpp*8)return 6;
  for(unsigned origin=0;origin<2;origin++)for(unsigned phase=0;phase<2;phase++)for(unsigned repeat=0;repeat<2;repeat++){
   unsigned char *expected=malloc(c.pool_bytes);if(!expected)return 3;memset(expected,nn_clear_probe_seed(n,origin,phase,repeat),c.pool_bytes);
   for(unsigned op=0;op<2;op++){
    int x0=c.rect[op].x,y0=c.rect[op].y,x1=x0+c.rect[op].width,y1=y0+c.rect[op].height;
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>(int)c.width)x1=c.width;
    if(y1>(int)c.height)y1=c.height;
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++)for(unsigned byte=0;byte<c.cpp;byte++){
     uint64_t at=NN_CLEAR_PROBE_TARGET_OFFSET+nn_public_copy_oracle_offset(&c.source,0,0,x*c.cpp+byte,y);
     assert(at<c.pool_bytes);unsigned m=((unsigned char*)mask[op])[byte];
     expected[at]=(expected[at]&~m)|(((unsigned char*)color[op])[byte]&m);
    }
   }
   char path[1200];if(snprintf(path,sizeof(path),"%s/clear-color-%03u-o%u-p%u-r%u.raw",argv[1],n,origin,phase,repeat)>=(int)sizeof(path))return 4;
   FILE *f=fopen(path,"wbx");if(!f||fwrite(expected,1,c.pool_bytes,f)!=c.pool_bytes||fclose(f))return 5;free(expected);files++;bytes+=c.pool_bytes;
  }
 }
 printf("{\"cases\":%u,\"files\":%u,\"bytes\":%llu,\"GPU_execution\":false,\"HOST_expected_only\":true}\n",NN_CLEAR_PROBE_CASES,files,(unsigned long long)bytes);return 0;
}
