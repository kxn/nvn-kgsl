/* HOST pure canonical metadata; no pixels converted and no GPU execution. */
#include <stdio.h>
#include "nvn_native/texture_source.h"
#include NN_CASC_GATHER_CASES_HEADER
int main(void)
{
 putchar('[');
 for(unsigned i=0;i<NN_CASC_GATHER_DEPTH_CASE_COUNT;i++){
  const struct nn_casc_gather_depth_case*c=NN_CASC_GATHER_DEPTH_CASES+i;
  struct nn_tex_source_desc d={.target=1,.format=c->format,.width=c->width<<c->mip,.height=c->height<<c->mip,.levels=c->mip+1,.layers=1,.block_height_log2=-1};struct nn_tex_source_layout l;
  if(nn_tex_source_layout(&d,&l))return 2;
  printf("%s{\"case\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"origin\":%u,\"wrap_index\":%u,\"selected_mip\":%u,\"compare\":%u,\"clear_depth_bits\":%u,\"target_pool_bytes\":%llu,\"target_payload\":%llu,\"cpp\":%u,\"level\":[",i?",":"",i,c->format,c->width,c->height,c->origin,c->wrap,c->mip,c->compare,c->compare==5?0:0x3f800000u,(unsigned long long)(8192+((l.payload_bytes+4095)&~4095ull)),(unsigned long long)l.payload_bytes,l.bytes_per_block);
  for(unsigned m=0;m<l.levels;m++){const struct nn_tex_source_level*x=l.level+m;printf("%s{\"source_offset\":%llu,\"source_bytes\":%llu,\"row_bytes\":%u,\"rows\":%u,\"source_pitch\":%u,\"block_height_log2\":%u}",m?",":"",(unsigned long long)x->offset,(unsigned long long)x->bytes,x->row_bytes,x->height,x->pitch,x->block_height_log2);}
  printf("]}");
 }
 puts("]");return fflush(stdout)?1:0;
}
