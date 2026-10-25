/* HOST endpoint/color oracle and actual source layout inventory. No GPU/codec. */
#include "bc_mip_cases.h"
#include "nvn_native/texture_source.h"
#include <stdio.h>
int main(int argc,char **argv)
{
 const unsigned shapes[2][2]={{64,64},{127,65}};
 int inventory=argc==2&&!strcmp(argv[1],"--cases");if(argc!=1&&!inventory)return 2;
 if(inventory)putchar('[');
 for(unsigned f=0x41;f<=0x48;f++)for(unsigned shape=0;shape<2;shape++)for(unsigned view=0;view<3;view++){
  unsigned width=shapes[shape][0],height=shapes[shape][1],index=(f-0x41)*6+shape*3+view;
  struct nn_tex_source_desc d={.format=f,.target=1,.width=width,.height=height,.levels=4,.layers=1,.block_height_log2=-1};
  struct nn_tex_source_layout l;if(nn_tex_source_layout(&d,&l))return 3;
  if(inventory){printf("%s{\"case\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"levels\":4,\"view\":%u,\"base0\":%u,\"count0\":%u,\"base1\":%u,\"count1\":%u,\"payload_bytes\":%llu,\"source_pool_bytes\":%llu,\"metadata_flags\":%u}",index?",":"",index,f,width,height,view,nn_bc_mip_base(view,0),nn_bc_mip_count(view,0),nn_bc_mip_base(view,1),nn_bc_mip_count(view,1),(unsigned long long)l.payload_bytes,(unsigned long long)(8192+((l.payload_bytes+4095)&~4095ull)),(f&1)?0x12:0x14);continue;}
  for(unsigned repeat=0;repeat<2;repeat++){
   unsigned base=nn_bc_mip_base(view,repeat),w=l.level[base].width,h=l.level[base].height;
   for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
    uint8_t v[4];nn_bc_mip_expected(f,((2*x+1)*w)/128,((2*y+1)*h)/128,base,repeat,v);
    if(fwrite(v,1,4,stdout)!=4)return 1;
   }
  }
 }
 if(inventory)puts("]");
 return fflush(stdout)?1:0;
}
