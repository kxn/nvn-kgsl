/* SPDX-License-Identifier: MIT. Actual source layout metadata, HOST only. */
#include "nvn_native/texture_source.h"
#include "color_target_cases.h"
#include <stdio.h>
int main(void)
{
 printf("[");for(unsigned i=0;i<22;i++){
  struct nn_tex_source_desc d={.flags=1,.target=1,.format=nn_color_formats[i],.width=73,.height=41,.levels=1,.layers=1,.block_height_log2=-1};
  struct nn_tex_source_layout s;int e=nn_tex_source_layout(&d,&s);if(e||s.linear||s.block_w!=1||s.block_h!=1||s.volume_depth||s.payload_bytes>1048576)return 1;
  printf("%s{\"format\":%u,\"cpp\":%u,\"storage_bytes\":%llu,\"pool_bytes\":%llu,\"gob_pitch\":%u,\"block_height_log2\":%u}",i?",":"",d.format,s.bytes_per_block,(unsigned long long)s.payload_bytes,(unsigned long long)((s.payload_bytes+12287)&~UINT64_C(4095)),s.level[0].pitch,s.level[0].block_height_log2);
 }puts("]");return 0;
}
