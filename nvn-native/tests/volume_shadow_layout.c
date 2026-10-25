/* SPDX-License-Identifier: MIT. Actual HOST FDL metadata, never pixel conversion. */
#include "volume_shadow_cases.h"
#include <stdio.h>
#include <string.h>
static void bytes(const void *v,size_t n){const unsigned char*p=v;putchar('"');for(size_t i=0;i<n;i++)printf("%02x",p[i]);putchar('"');}
int main(void)
{
 puts("[");for(unsigned i=0;i<18;i++){
  struct nn_tex_source_desc d;unsigned base,count;if(nn_volume_shadow_case_spec(i,&d,&base,&count))return 1;
  struct nn_tex_source_layout s;struct nn_volume_shadow_plan p;if(nn_tex_source_layout(&d,&s)||nn_volume_shadow_plan(&s,base,count,&p))return 1;
  printf("%s{\"id\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"depth\":%u,\"levels\":%u,\"base_level\":%u,\"level_count\":%u,\"flags\":%u,\"payload_bytes\":%llu,\"canonical_offset\":512,\"canonical_bo_bytes\":%llu,\"native_bo_bytes\":%llu,\"native_span\":%llu,\"source_hex\":",i?",":"",i,d.format,d.width,d.height,d.depth,d.levels,base,count,d.flags,(unsigned long long)s.payload_bytes,(unsigned long long)((s.payload_bytes+512+4096+4095)&~4095ull),(unsigned long long)((p.native.required_span+4095)&~4095ull),(unsigned long long)p.native.required_span);bytes(&s,sizeof(s));printf(",\"native_hex\":");bytes(&p.native,sizeof(p.native));printf(",\"native_levels\":[");
  for(unsigned l=0;l<s.levels;l++){const struct nn_fdl_volume_level*m=p.native.level+l;printf("%s[%u,%u,%u,%u,%llu,%llu,%llu]",l?",":"",m->width,m->height,m->depth,m->pitch,(unsigned long long)m->offset,(unsigned long long)m->slice_stride,(unsigned long long)m->span);}printf("]}");
 }puts("\n]");return 0;
}
