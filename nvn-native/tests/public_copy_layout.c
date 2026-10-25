/* SPDX-License-Identifier: MIT. HOST case generator using actual calculator. */
#include "public_copy_cases.h"
#include "block_copy_probe_cases.h"
#include <stdio.h>
static uint64_t align64(uint64_t v,uint64_t a){return (v+a-1)/a*a;}
int main(void)
{
 puts("[");for(unsigned n=0;n<NN_PUBLIC_COPY_CASES;n++){
  unsigned f=n/6,variant=n/3%2,family=n%3;struct nn_tex_source_layout s;struct nn_block_copy_region r;struct nn_block_copy_plan p;
  unsigned target=4,layers=3,levels=3,width,height,format;
  if(f<37){unsigned shared;if(nn_block_probe_case(f*4+variant*2,&s,&p,&shared))return 1;r=p.region;format=s.format;width=s.level[0].width;height=s.level[0].height;}
  else{
   target=1;layers=levels=1;format=f==37?0x42:1;width=f==37?64:31;height=f==37?64:17;
   struct nn_tex_source_desc d={.target=1,.format=format,.width=width,.height=height,.layers=1,.levels=1,.block_height_log2=-1};if(nn_tex_source_layout(&d,&s))return 1;
   unsigned x=variant?s.bytes_per_block:0,y=variant?1:0,rb=s.level[0].row_bytes-(variant?s.bytes_per_block*2:0),rows=s.level[0].blocks_y-(variant?2:0),rs=rb+variant,ims=((rows-1)*rs+rb+variant)|variant;
   r=(struct nn_block_copy_region){0,0,1,x,y,rb,rows,rs,ims,0};
  }
  const struct nn_tex_source_level *m=s.level+r.level;unsigned xo=r.x_bytes/s.bytes_per_block*s.block_w,yo=r.y_rows*s.block_h;
  unsigned rw=r.row_bytes/s.bytes_per_block*s.block_w,rh=r.rows*s.block_h;if(rw>m->width-xo)rw=m->width-xo;if(rh>m->height-yo)rh=m->height-yo;
  unsigned dest=(unsigned)(variant?align64(512+s.payload_bytes+68,512):1024),linear=(unsigned)(variant?512+s.payload_bytes+68:68);
  uint64_t span=(uint64_t)(r.layers-1)*r.linear_image_stride+(uint64_t)(r.rows-1)*r.linear_row_stride+r.row_bytes,lb=align64(span,4);
  uint64_t bytes0=align64((variant?(family==2?dest+s.payload_bytes:linear+lb):512+s.payload_bytes)+4096,4096),bytes1=variant?0:align64((family==2?dest+s.payload_bytes:linear+lb)+4096,4096);
  printf("%s{\"id\":%u,\"family\":%u,\"variant\":%u,\"target\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"layers\":%u,\"levels\":%u,\"flags\":%u,\"shared\":%u,\"payload_bytes\":%llu,\"layer_stride\":%llu,\"base_bh\":%u,\"canonical_offset\":512,\"destination_offset\":%u,\"linear_offset\":%u,\"linear_bytes\":%llu,\"pool_bytes\":[%llu,%llu],\"level\":%u,\"base_layer\":%u,\"layer_count\":%u,\"x_bytes\":%u,\"y_rows\":%u,\"row_bytes\":%u,\"rows\":%u,\"row_stride\":%u,\"image_stride\":%u,\"region\":[[%u,%u,0,%u,%u,%u],[0,0,0,%u,%u,%u]],\"mips\":[",n?",":"",n,family,variant,target,format,width,height,layers,levels,variant?8:0,variant,(unsigned long long)s.payload_bytes,(unsigned long long)s.layer_stride,s.base_block_height_log2,dest,linear,(unsigned long long)lb,(unsigned long long)bytes0,(unsigned long long)bytes1,r.level,r.base_layer,r.layers,r.x_bytes,r.y_rows,r.row_bytes,r.rows,r.linear_row_stride,r.linear_image_stride,xo,yo,rw,rh,r.layers,rw,rh,r.layers);
  for(unsigned i=0;i<s.levels;i++){const struct nn_tex_source_level *l=s.level+i;printf("%s[%llu,%llu,%u,%u,%u,%u,%u,%u,%u]",i?",":"",(unsigned long long)l->offset,(unsigned long long)l->bytes,l->width,l->height,l->blocks_x,l->blocks_y,l->row_bytes,l->pitch,l->block_height_log2);}puts("]}");
 }puts("]");return 0;
}
