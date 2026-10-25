/* SPDX-License-Identifier: MIT. Calls actual FDL; metadata only, no pixels. */
#include "nvn_native/runtime_fdl.h"
#include "nvn_native/color_target.h"
#include "sampler_mip_cases.h"
#include <stdio.h>
static void hex(const void*p,size_t n)
{const unsigned char*b=p;putchar('"');for(size_t i=0;i<n;i++)printf("%02x",b[i]);putchar('"');}
int main(void)
{
 struct nn_tex_native_view target;
 if(nn_color_target_create(0x06050002,37,NN_SAMPLER_MIP_WIDTH,NN_SAMPLER_MIP_HEIGHT,0,&target,NULL))return 1;
 printf("{\"target\":{\"width\":%u,\"height\":%u,\"pitch\":%u,\"span\":%u,\"hex\":",target.width,target.height,target.pitch,target.required_span_bytes);hex(&target,sizeof(target));printf("},\"views\":[");
 for(unsigned view=0;view<2;view++){
  struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=NN_SAMPLER_MIP_SOURCE_WIDTH,.height=NN_SAMPLER_MIP_SOURCE_HEIGHT,.levels=NN_SAMPLER_MIP_SOURCE_LEVELS,.layers=1,.base_level=view?2:0,.level_count=view?4:7,.layer_count=1,.usage=NN_FDL_SAMPLED};struct nn_fdl_image_view v;
  if(nn_fdl_image(&d,&v))return 2;
  printf("%s{\"format\":%u,\"width\":%u,\"height\":%u,\"levels\":%u,\"view_width\":%u,\"view_height\":%u,\"base_level\":%u,\"level_count\":%u,\"span\":%llu,\"mips\":[",view?",":"",v.format,v.width,v.height,v.levels,v.view_width,v.view_height,v.base_level,v.level_count,(unsigned long long)v.required_span);
  for(unsigned i=0;i<v.levels;i++)printf("%s{\"width\":%u,\"height\":%u,\"offset\":%llu,\"pitch\":%u,\"layer_stride\":%u}",i?",":"",v.level[i].width,v.level[i].height,(unsigned long long)v.level[i].offset,v.level[i].pitch,v.level[i].layer_stride);
  printf("],\"hex\":");hex(&v,sizeof(v));printf("}");
 }
 puts("]}");return 0;
}
