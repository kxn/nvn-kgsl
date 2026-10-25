/* HOST authored-oracle/source extent consistency. No GPU claim or CPU codec. */
#ifndef NVN_NVN_PIPELINE_BC_SRGB
#define NVN_NVN_PIPELINE_BC_SRGB 0
#endif
#if NVN_NVN_PIPELINE_BC_SRGB
#include "bc_srgb_cases.h"
#else
#include "bc_sampled_cases.h"
#endif
#include "nvn_native/texture_source.h"
#include <stdio.h>
int main(int argc,char **argv)
{
 const unsigned shape[3][2]={{8,8},{37,21},{127,65}};
 int inventory=argc==2&&!strcmp(argv[1],"--cases");
 if(argc!=1&&!inventory)return 2;
 if(inventory)putchar('[');
 const unsigned first_format=NVN_NVN_PIPELINE_BC_SRGB?0x45:0x41;
 for(unsigned f=first_format;f<first_format+4;f++)for(unsigned s=0;s<3;s++){
  struct nn_tex_source_desc d={.format=f,.target=1,.width=shape[s][0],.height=shape[s][1],.levels=1,.layers=1,.block_height_log2=-1};
  struct nn_tex_source_layout l;if(nn_tex_source_layout(&d,&l))return 3;
  unsigned index=(f-first_format)*3+s;
  if(inventory){printf("%s{\"case\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"payload_bytes\":%llu,\"source_pool_bytes\":%llu,\"metadata_flags\":%u}",index?",":"",index,f,shape[s][0],shape[s][1],(unsigned long long)l.payload_bytes,(unsigned long long)(8192+((l.payload_bytes+4095)&~4095ull)),(f&1)?0x12:0x14);continue;}
  for(unsigned r=0;r<2;r++)for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
   uint8_t v[4];nn_bc_test_expected(f,((2*x+1)*shape[s][0])/128,((2*y+1)*shape[s][1])/128,r,v);
   if(fwrite(v,1,4,stdout)!=4)return 1;
  }
 }
 if(inventory)puts("]");
 return fflush(stdout)?1:0;
}
