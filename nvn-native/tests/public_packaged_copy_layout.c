/* SPDX-License-Identifier: MIT. Actual source metadata; not a pixel codec. */
#include "public_copy_cases.h"
#include <stdio.h>
static uint64_t up(uint64_t v,uint64_t a){return(v+a-1)/a*a;}
int main(void)
{
 puts("[");for(unsigned n=0;n<72;n++){
  unsigned format=n/36?0x44:0x25,bh=n/6%6,variant=n/3%2,family=n%3;
  unsigned width=variant?73:128,height=variant?(format==0x44?651:163):128,target=variant?4:1,layers=variant?3:1,levels=variant?4:8;
  struct nn_tex_source_desc d={.target=target,.format=format,.width=width,.height=height,.layers=layers,.levels=levels,.block_height_log2=(int)bh};struct nn_tex_source_layout s;if(nn_tex_source_layout(&d,&s))return 1;
  unsigned level=variant?0:(bh&1u),base=variant?1:0,count=variant?2:1;const struct nn_tex_source_level *m=s.level+level;
  unsigned xb=variant?s.bytes_per_block:0,yb=variant?1:0,rb=m->row_bytes-(variant?2*s.bytes_per_block:0),rows=m->blocks_y-(variant?2:0),rs=rb+variant,ims=((rows-1)*rs+rb+variant)|variant;
  unsigned xo=xb/s.bytes_per_block*s.block_w,yo=yb*s.block_h,rw=rb/s.bytes_per_block*s.block_w,rh=rows*s.block_h;if(rw>m->width-xo)rw=m->width-xo;if(rh>m->height-yo)rh=m->height-yo;
  uint64_t span=(uint64_t)(count-1)*ims+(uint64_t)(rows-1)*rs+rb,linear=variant?512+s.payload_bytes+68:68,dest=variant?up(512+s.payload_bytes+68,512):1024,lb=up(span,4);
  uint64_t bytes0=up((variant?(family==2?dest+s.payload_bytes:linear+lb):512+s.payload_bytes)+4096,4096),bytes1=variant?0:up((family==2?dest+s.payload_bytes:linear+lb)+4096,4096);
  printf("%s{\"id\":%u,\"family\":%u,\"variant\":%u,\"target\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"layers\":%u,\"levels\":%u,\"flags\":0,\"shared\":%u,\"payload_bytes\":%llu,\"layer_stride\":%llu,\"base_bh\":%u,\"canonical_offset\":512,\"destination_offset\":%llu,\"linear_offset\":%llu,\"linear_bytes\":%llu,\"pool_bytes\":[%llu,%llu],\"level\":%u,\"base_layer\":%u,\"layer_count\":%u,\"x_bytes\":%u,\"y_rows\":%u,\"row_bytes\":%u,\"rows\":%u,\"row_stride\":%u,\"image_stride\":%u,\"region\":[[%u,%u,0,%u,%u,%u],[0,0,0,%u,%u,%u]],\"mips\":[",n?",":"",n,family,variant,target,format,width,height,layers,levels,variant,(unsigned long long)s.payload_bytes,(unsigned long long)s.layer_stride,s.base_block_height_log2,(unsigned long long)dest,(unsigned long long)linear,(unsigned long long)lb,(unsigned long long)bytes0,(unsigned long long)bytes1,level,base,count,xb,yb,rb,rows,rs,ims,xo,yo,rw,rh,count,rw,rh,count);
  for(unsigned i=0;i<s.levels;i++){const struct nn_tex_source_level *l=s.level+i;printf("%s[%llu,%llu,%u,%u,%u,%u,%u,%u,%u]",i?",":"",(unsigned long long)l->offset,(unsigned long long)l->bytes,l->width,l->height,l->blocks_x,l->blocks_y,l->row_bytes,l->pitch,l->block_height_log2);}puts("]}");
 }puts("]");return 0;
}
