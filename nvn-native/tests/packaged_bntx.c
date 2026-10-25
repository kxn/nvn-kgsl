/* HOST ONLY strict profile decoder and metadata export; no image conversion. */
#include "nvn_native/packaged_bntx.h"
#include "nvn_native/texture_source.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
int main(void)
{
 uint8_t bytes[8]={2,0,0,0,7,0,1,0};uint32_t h=99;
 T(nn_packaged_bntx_height(NULL,&h)==EINVAL&&h==0);T(nn_packaged_bntx_height(bytes,NULL)==EINVAL);
 for(unsigned bh=0;bh<6;bh++){bytes[0]=bh;h=99;T(!nn_packaged_bntx_height(bytes,&h)&&h==bh);}
 bytes[0]=2;
 for(unsigned bit=0;bit<64;bit++){
  uint8_t bad[8];memcpy(bad,bytes,8);bad[bit/8]^=1u<<(bit%8);h=99;int e=nn_packaged_bntx_height(bad,&h);
  if(bit<3&&bad[0]<=5)T(!e&&h==bad[0]);else T(e==ENOTSUP&&h==0);
 }
 for(unsigned bh=6;bh<8;bh++){bytes[0]=bh;T(nn_packaged_bntx_height(bytes,&h)==ENOTSUP&&h==0);}
 for(unsigned bh=0;bh<6;bh++)for(unsigned format=0;format<2;format++)for(unsigned variant=0;variant<2;variant++){
  struct nn_tex_source_desc d={.target=1,.format=format?0x44:0x25,.width=variant?73:128,.height=variant?(format?651:163):128,.layers=1,.levels=variant?4:8,.block_height_log2=(int)bh};struct nn_tex_source_layout l;T(!nn_tex_source_layout(&d,&l));
  for(unsigned level=0;level<l.levels;level++){
   struct nn_tex_source_level *m=l.level+level;
   for(unsigned y=0;y<m->blocks_y;y++)for(unsigned x=0;x<m->row_bytes;x++){
    uint64_t at=0;T(!nn_tex_source_offset(&l,level,0,x/l.bytes_per_block,y,x%l.bytes_per_block,&at));
    printf("%u %u %u %u %u %u %u %u %llu\n",bh,d.format,variant,level,m->pitch,m->block_height_log2,x,y,(unsigned long long)(at-m->offset));
   }
  }
 }
 fprintf(stderr,"HOST ONLY BNTX writer profile decoder checks=%u variants=6 original_NVNmapping_proven=false GPU=false\n",checks);return 0;
}
