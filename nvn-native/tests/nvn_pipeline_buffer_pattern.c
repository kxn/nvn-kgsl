/* HOST closed authored formula/resource byte crosscheck only; no GPU. */
#include "nvn_pipeline_buffer_cases.h"
#include <stdio.h>
int main(void)
{
 for(unsigned i=0;i<12;i++)for(unsigned phase=0;phase<4;phase++){
  unsigned f=nn_public_buffer_formats[i];unsigned char b[65536];memset(b,0xa5,sizeof(b));
  for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
   nn_public_buffer_pixel(f,phase,x,y,b+4096+y*256+x*4);
   unsigned char*p=b+24576+4096+y*256+x*4;p[0]=0;p[1]=(unsigned char)(x+(phase&~1u));p[2]=(unsigned char)(y+(phase&~1u));p[3]=255;
  }
  for(size_t at=0;at<8192;at++){b[49152+at]=nn_public_buffer_byte(f,phase&~1u,at);b[57344+at]=nn_public_buffer_byte(f,(phase&~1u)+1,at);}
  if(fwrite(b,1,sizeof(b),stdout)!=sizeof(b))return 1;
 }
 return fflush(stdout)?1:0;
}
