/* SPDX-License-Identifier: MIT. Authored legal BC endpoint banks, NOT a codec.
 * No compressed bytes are read to determine expected pixels. Runtime only
 * uploads authored input bytes through public NVN pools. */
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
static inline unsigned nn_bc_mip_base(unsigned view,unsigned repeat)
{static const unsigned x[3][2]={{0,1},{1,2},{3,0}};return x[view][repeat];}
static inline unsigned nn_bc_mip_count(unsigned view,unsigned repeat)
{static const unsigned x[3][2]={{4,3},{2,2},{1,1}};return x[view][repeat];}
static inline unsigned nn_bc_mip_selector(unsigned format,unsigned x,unsigned y,unsigned bx,unsigned by,unsigned phase)
{const unsigned s[4]={0,1,3,1};unsigned type=(format-0x41)&3;return type<2?s[(x+2*y+bx+3*by+phase)&3]:(x+2*y+by+phase)&1;}
static inline void nn_bc_mip_block(unsigned format,unsigned bx,unsigned by,unsigned mip,unsigned repeat,uint8_t out[16])
{
 unsigned bank=(mip+repeat)&3,type=(format-0x41)&3,color=type<2?0:8;
 unsigned lo=8+2*bank,hi=16+2*bank;uint16_t low=(lo<<11)|((2*lo)<<5)|lo,high=(hi<<11)|((2*hi)<<5)|hi;
 uint16_t a=type<2?low:high,b=type<2?high:low;memset(out,0,16);
 out[color]=a;out[color+1]=a>>8;out[color+2]=b;out[color+3]=b>>8;
 uint32_t codes=0;uint64_t alpha=0;unsigned phase=mip+repeat;
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){
  unsigned n=4*y+x;codes|=nn_bc_mip_selector(format,x,y,bx,by,phase)<<(2*n);
  if(type==2)alpha|=(uint64_t)(((x+y+bx+phase)&3)*5)<<(4*n);
  if(type==3){const unsigned indices[4]={0,1,6,7};alpha|=(uint64_t)indices[(x+y+bx+phase)&3]<<(3*n);}
 }
 for(unsigned i=0;i<4;i++)out[color+4+i]=codes>>(8*i);
 if(type==2)for(unsigned i=0;i<8;i++)out[i]=alpha>>(8*i);
 if(type==3){out[0]=0;out[1]=255;for(unsigned i=0;i<6;i++)out[2+i]=alpha>>(8*i);}
}
static inline uint8_t nn_bc_mip_channel(unsigned value,unsigned bits,unsigned srgb)
{
 double s=(double)value/((1u<<bits)-1);
 if(srgb)s=s<=.04045?s/12.92:pow((s+.055)/1.055,2.4);
 return(uint8_t)floor(255*s+.5);
}
static inline void nn_bc_mip_expected(unsigned format,unsigned x,unsigned y,unsigned mip,unsigned repeat,uint8_t out[4])
{
 unsigned bx=x/4,by=y/4,lx=x%4,ly=y%4,type=(format-0x41)&3,bank=(mip+repeat)&3,phase=mip+repeat;
 unsigned code=nn_bc_mip_selector(format,lx,ly,bx,by,phase),high=type<2?code==1:code==0;
 unsigned value=(high?16:8)+2*bank;
 out[0]=out[2]=nn_bc_mip_channel(value,5,format>=0x45);out[1]=nn_bc_mip_channel(2*value,6,format>=0x45);out[3]=255;
 if(type<2&&code==3){out[0]=out[1]=out[2]=0;if(type==1)out[3]=0;}
 if(type==2)out[3]=((lx+ly+bx+phase)&3)*85;
 if(type==3){const unsigned alpha[4]={0,255,0,255};out[3]=alpha[(lx+ly+bx+phase)&3];}
 /* Bounded original a767 FS: white input, mix0, gamma1/contrast1, alpha0 KIL. */
 if(!out[3]){const uint8_t clear[4]={17,29,43,59};memcpy(out,clear,4);}
}
