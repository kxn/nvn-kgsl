/* SPDX-License-Identifier: MIT. Authored raw block initialization only; no image conversion. */
#pragma once
#include <stdint.h>
#include <string.h>
static inline void nn_volume_texture_block(unsigned fmt,unsigned id,unsigned repeat,unsigned level,unsigned z,unsigned x,unsigned y,unsigned char out[16])
{
 unsigned seed=17+id*7+repeat*29+level*31+z*13+x*9+y*3;memset(out,0,16);
 if(fmt==37){out[0]=(unsigned char)seed;out[1]=(unsigned char)(seed*3+47);out[2]=(unsigned char)(seed*7+19);out[3]=255;return;}
 if(fmt==0x29){const uint16_t half[]={0,0x3400,0x3800,0x3c00};for(unsigned c=0;c<4;c++){uint16_t v=half[c==3?3:(seed+c)%4];out[c*2]=v;out[c*2+1]=v>>8;}return;}
 unsigned offset=fmt>=0x43?8:0;uint16_t color=(uint16_t)(((seed%30+1)<<11)|(((seed*3)%62+1)<<5)|((seed*5)%30+1)),a=(seed&1)?0:color,b=(seed&1)?color:0;
 out[offset]=a;out[offset+1]=a>>8;out[offset+2]=b;out[offset+3]=b>>8;uint32_t indexes=0;for(unsigned i=0;i<16;i++)indexes|=((i+seed)%4)<<(i*2);memcpy(out+offset+4,&indexes,4);
 if(fmt==0x43){uint64_t alpha=0;for(unsigned i=0;i<16;i++)alpha|=(uint64_t)((i+seed)%16)<<(i*4);memcpy(out,&alpha,8);}
 if(fmt==0x44){out[0]=200;out[1]=20;uint64_t alpha=0;for(unsigned i=0;i<16;i++)alpha|=(uint64_t)((i+seed)%8)<<(i*3);for(unsigned i=0;i<6;i++)out[2+i]=(unsigned char)(alpha>>(i*8));}
}
