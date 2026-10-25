/* SPDX-License-Identifier: MIT. Authored endpoint controls, NOT a BC decoder.
 * Khronos DataFormat1.3.1 sections18.1..4; EXT_texture_sRGB equation cs->cl.
 * No compressed input bytes are inspected to derive expected colors. */
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
static inline unsigned nn_bc_srgb_select(unsigned f,unsigned x,unsigned y,unsigned bx,unsigned by,unsigned r)
{const unsigned s[4]={0,1,3,1};return f<0x47?s[(x+2*y+bx+3*by+r)&3]:(x+2*y+by+r)&1;}
static inline void nn_bc_test_block(unsigned f,unsigned bx,unsigned by,unsigned r,uint8_t out[16])
{
 memset(out,0,16);unsigned c=f<0x47?0:8;uint16_t low=(8u<<11)|(16u<<5)|16u,high=(16u<<11)|(32u<<5)|8u;
 uint16_t a=f<0x47?low:high,b=f<0x47?high:low;
 out[c]=a;out[c+1]=a>>8;out[c+2]=b;out[c+3]=b>>8;uint32_t colors=0;uint64_t alpha=0;
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){
  unsigned n=4*y+x;colors|=nn_bc_srgb_select(f,x,y,bx,by,r)<<(2*n);
  if(f==0x47)alpha|=(uint64_t)(((x+y+bx+r)&3)*5)<<(4*n);
  if(f==0x48){const unsigned a[4]={0,1,6,7};alpha|=(uint64_t)a[(x+y+bx+r)&3]<<(3*n);}
 }
 for(unsigned n=0;n<4;n++)out[c+4+n]=colors>>(8*n);
 if(f==0x47)for(unsigned n=0;n<8;n++)out[n]=alpha>>(8*n);
 if(f==0x48){out[0]=0;out[1]=255;for(unsigned n=0;n<6;n++)out[2+n]=alpha>>(8*n);}
}
static inline uint8_t nn_bc_srgb_linear(unsigned value,unsigned bits)
{double s=(double)value/((1u<<bits)-1),l=s<=0.04045?s/12.92:pow((s+0.055)/1.055,2.4);return(uint8_t)floor(l*255+0.5);}
static inline void nn_bc_test_expected(unsigned f,unsigned x,unsigned y,unsigned r,uint8_t out[4])
{
 unsigned bx=x/4,by=y/4,lx=x%4,ly=y%4,code=nn_bc_srgb_select(f,lx,ly,bx,by,r);out[3]=255;
 unsigned high=f<0x47?code==1:code==0;
 out[0]=nn_bc_srgb_linear(high?16:8,5);out[1]=nn_bc_srgb_linear(high?32:16,6);out[2]=nn_bc_srgb_linear(high?8:16,5);
 if(f<0x47&&code==3){out[0]=out[1]=out[2]=0;if(f==0x46)out[3]=0;}
 if(f==0x47)out[3]=((lx+ly+bx+r)&3)*85;
 if(f==0x48){const unsigned a[4]={0,255,0,255};out[3]=a[(lx+ly+bx+r)&3];}
 if(!out[3]){const uint8_t clear[4]={17,29,43,59};memcpy(out,clear,4);}
}
