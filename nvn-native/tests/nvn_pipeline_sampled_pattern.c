/* HOST ONLY C/normal-Mesa-fixture index/layout consistency; no shader/GPU. */
#include NN_SAMPLED_PATTERNS
#include "nvn_native/texture_source.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char**argv)
{
 int inventory=argc==2&&!strcmp(argv[1],"--cases");if(argc!=1&&!inventory)return 2;
 if(inventory){putchar('[');}
 unsigned test=0;
 for(unsigned f=0;f<NN_PUBLIC_SAMPLED_FORMAT_COUNT;f++)for(unsigned shape=0;shape<2;shape++)for(unsigned view=0;view<3;view++,test++){
  const struct nn_public_sampled_fixture *fx=nn_public_sampled_fixtures+f;unsigned width=shape?127:64,height=shape?65:64;
  struct nn_tex_source_desc d={.format=fx->format,.target=1,.width=width,.height=height,.levels=4,.layers=1,.block_height_log2=-1};struct nn_tex_source_layout l;if(nn_tex_source_layout(&d,&l)||l.bytes_per_block!=fx->cpp)return 3;
  if(inventory){printf("%s{\"case\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"levels\":4,\"view\":%u,\"base0\":%u,\"count0\":%u,\"base1\":%u,\"count1\":%u,\"payload_bytes\":%llu,\"source_pool_bytes\":%llu,\"metadata_flags\":%u}",test?",":"",test,fx->format,width,height,view,nn_public_sampled_base(view,0),nn_public_sampled_count(view,0),nn_public_sampled_base(view,1),nn_public_sampled_count(view,1),(unsigned long long)l.payload_bytes,(unsigned long long)(8192+((l.payload_bytes+4095)&~4095ull)),(fx->format&1)?18:20);continue;}
  for(unsigned repeat=0;repeat<2;repeat++){
   unsigned base=nn_public_sampled_base(view,repeat);
   for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
    unsigned sx=((2*x+1)*l.level[base].width)/128,sy=((2*y+1)*l.level[base].height)/128;
    const unsigned char *p=fx->expected[nn_public_sampled_bank(sx,sy,base,repeat)];if(fwrite(p,1,4,stdout)!=4)return 4;
   }
  }
 }
 if(inventory){puts("]");}
 return fflush(stdout)?1:0;
}
