/* SPDX-License-Identifier: MIT. Authored true3D catalogue, real calculator. */
#include "public_copy_cases.h"
#include "block_copy_cases.h"
#include <stdio.h>
static uint64_t up64(uint64_t x,uint64_t a){return(x+a-1)/a*a;}
int main(void)
{
 (void)nn_block_copy_case;
 puts("[");
 for(unsigned n=0;n<228;n++){
  unsigned f=n/6,v=n/3%2,family=n%3,fmt=f<37?nn_block_copy_formats[f]:0x42;
  unsigned w=f<37?35:32,h=f<37?19:32,z=f<37?9:32,levels=f<37?3:1;
  unsigned flags=v&&(fmt==NVN_FORMAT_RGBA8||fmt==NVN_FORMAT_RGBA16F)?NVN_TEXTURE_FLAGS_IMAGE:0;
  struct nn_tex_source_desc d={.flags=flags,.target=NVN_TEXTURE_TARGET_3D,.format=fmt,.width=w,.height=h,.layers=1,.levels=levels,.block_height_log2=-1,.depth=z};
  struct nn_tex_source_layout s;if(nn_tex_source_layout(&d,&s)||nn_tex_source_validate(&s))return 1;
  unsigned level=v&&f<37?1:0;const struct nn_tex_source_level *m=s.level+level;
  unsigned xb=v?1:0,y=v?1:0,bz=v?1:0,rb=(m->blocks_x-xb)*s.bytes_per_block,rows=m->blocks_y-y,ns=s.level_depths[level]-bz;
  unsigned row=rb+v,image=(rows-1)*row+rb;if(v)image=(image+1)|1;
  unsigned xo=xb*s.block_w,yo=y*s.block_h,rw=m->width-xo,rh=m->height-yo;
  uint64_t span=(uint64_t)(ns-1)*image+(uint64_t)(rows-1)*row+rb,lb=up64(span,4);
  unsigned dest=(unsigned)(v?up64(512+s.payload_bytes+68,512):1024),linear=(unsigned)(v?512+s.payload_bytes+68:68);
  uint64_t bytes0=up64((v?(family==2?dest+s.payload_bytes:linear+lb):512+s.payload_bytes)+4096,4096),bytes1=v?0:up64((family==2?dest+s.payload_bytes:linear+lb)+4096,4096);
  printf("%s{\"id\":%u,\"family\":%u,\"variant\":%u,\"target\":2,\"format\":%u,\"width\":%u,\"height\":%u,\"depth\":%u,\"layers\":1,\"levels\":%u,\"flags\":%u,\"shared\":%u,\"payload_bytes\":%llu,\"layer_stride\":%llu,\"base_bh\":0,\"base_bd\":%u,\"canonical_offset\":512,\"destination_offset\":%u,\"linear_offset\":%u,\"linear_bytes\":%llu,\"pool_bytes\":[%llu,%llu],\"level\":%u,\"base_layer\":%u,\"layer_count\":%u,\"x_bytes\":%u,\"y_rows\":%u,\"row_bytes\":%u,\"rows\":%u,\"row_stride\":%u,\"image_stride\":%u,\"region\":[[%u,%u,%u,%u,%u,%u],[%u,%u,0,%u,%u,%u]],\"mips\":[",n?",":"",n,family,v,fmt,w,h,z,levels,flags,v,(unsigned long long)s.payload_bytes,(unsigned long long)s.layer_stride,s.base_block_depth_log2,dest,linear,(unsigned long long)lb,(unsigned long long)bytes0,(unsigned long long)bytes1,level,bz,ns,xb*s.bytes_per_block,y,rb,rows,row,image,xo,yo,bz,rw,rh,ns,xo,yo,rw,rh,ns);
  for(unsigned i=0;i<s.levels;i++){const struct nn_tex_source_level *a=s.level+i;printf("%s[%llu,%llu,%u,%u,%u,%u,%u,%u,%u]",i?",":"",(unsigned long long)a->offset,(unsigned long long)a->bytes,a->width,a->height,a->blocks_x,a->blocks_y,a->row_bytes,a->pitch,a->block_height_log2);}
  printf("],\"mip_depths\":[");for(unsigned i=0;i<s.levels;i++)printf("%s%u",i?",":"",s.level_depths[i]);printf("],\"mip_bds\":[");for(unsigned i=0;i<s.levels;i++)printf("%s%u",i?",":"",s.level_block_depths[i]);puts("]}");
 }
 puts("]");return 0;
}
