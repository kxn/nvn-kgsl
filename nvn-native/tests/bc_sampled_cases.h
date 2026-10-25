/* SPDX-License-Identifier: MIT. Authored legal BC controls + closed test oracle.
 * Khronos EXT_texture_compression_s3tc / dxt1 appendix. This is NOT a decoder:
 * no compressed input bytes are inspected, interpolated or converted on CPU.
 * Only the endpoints/transparent selectors deliberately authored here are used.
 * BC1 RGB preserves alphaONE where identical BC1 RGBA bits are transparent.
 */
#pragma once
#include <stdint.h>
#include <string.h>
static inline unsigned nn_bc_test_selector(unsigned x,unsigned y,unsigned bx,unsigned by,unsigned repeat)
{const unsigned select[4]={0,1,3,1};return select[(x+2*y+bx+3*by+repeat)&3];}
static inline void nn_bc_test_block(unsigned format,unsigned bx,unsigned by,unsigned repeat,uint8_t out[16])
{
 memset(out,0,16);unsigned color=format<0x43?0:8;uint32_t selectors=0;uint64_t alpha=0;
 if(format<0x43){out[color+2]=255;out[color+3]=255;} /* c0=BLACK <= c1=WHITE */
 else{out[color+1]=0xf8;out[color+2]=0xe0;out[color+3]=7;} /* RED > GREEN */
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){
  unsigned n=4*y+x,code=format<0x43?nn_bc_test_selector(x,y,bx,by,repeat):(x+2*y+by+repeat)&1;
  selectors|=code<<(2*n);
  if(format==0x43)alpha|=(uint64_t)(((x+y+bx+repeat)&3)*5)<<(4*n);
  if(format==0x44){const unsigned a[4]={0,1,6,7};alpha|=(uint64_t)a[(x+y+bx+repeat)&3]<<(3*n);}
 }
 for(unsigned n=0;n<4;n++)out[color+4+n]=(uint8_t)(selectors>>(8*n));
 if(format==0x43)for(unsigned n=0;n<8;n++)out[n]=(uint8_t)(alpha>>(8*n));
 if(format==0x44){out[0]=0;out[1]=255;for(unsigned n=0;n<6;n++)out[2+n]=(uint8_t)(alpha>>(8*n));}
}
static inline void nn_bc_test_expected(unsigned format,unsigned x,unsigned y,unsigned repeat,uint8_t out[4])
{
 unsigned bx=x/4,by=y/4,lx=x%4,ly=y%4;out[0]=out[1]=out[2]=0;out[3]=255;
 if(format<0x43){unsigned code=nn_bc_test_selector(lx,ly,bx,by,repeat);
  if(code==1)out[0]=out[1]=out[2]=255;
  if(format==0x42&&code==3)out[3]=0;
 }else{
  out[(lx+2*ly+by+repeat)&1]=255;
  if(format==0x43)out[3]=(uint8_t)(((lx+ly+bx+repeat)&3)*85);
  else{const unsigned alpha[4]={0,255,0,255};out[3]=(uint8_t)alpha[(lx+ly+bx+repeat)&3];}
 }
 /* Original a767 FS constant controls gamma=contrast=1,mix=0,whitevertex:
  * RGB and alpha pass through; exactly zero alpha KIL keeps authored clear. */
 if(!out[3]){const uint8_t clear[4]={17,29,43,59};memcpy(out,clear,4);}
}
