/* SPDX-License-Identifier: MIT. HOST metadata; no image or shader execution. */
#include "clear_color_cases.h"
#include <stdio.h>
int main(void)
{
 puts("[");for(unsigned i=0;i<NN_CLEAR_PROBE_CASES;i++){
  struct nn_clear_probe_case c;if(nn_clear_probe_case(i,&c))return 1;
  printf("%s{\"id\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"cpp\":%u,\"state\":%u,\"pool_bytes\":%zu,\"payload_bytes\":%llu,\"rects\":[",i?",":"",i,c.format,c.width,c.height,c.cpp,c.state,c.pool_bytes,(unsigned long long)c.source.payload_bytes);
  for(unsigned j=0;j<2;j++)printf("%s[%d,%d,%d,%d]",j?",":"",c.rect[j].x,c.rect[j].y,c.rect[j].width,c.rect[j].height);
  printf("],\"masks\":[%u,%u],\"colors\":[",c.mask[0],c.mask[1]);
  for(unsigned j=0;j<2;j++)printf("%s[%g,%g,%g,%g]",j?",":"",c.color[j][0],c.color[j][1],c.color[j][2],c.color[j][3]);
  puts("]}");
 }puts("]");return 0;
}
