/* SPDX-License-Identifier: MIT. Authored raw resource inputs and closed math
 * only. This is not a CPU shader interpreter or runtime image conversion. */
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
enum {NN_PUBLIC_BUFFER_FORMATS=12,NN_PUBLIC_BUFFER_JOBS=48};
static const unsigned nn_public_buffer_formats[12]={1,5,6,10,13,17,18,22,37,41,42,46};
static inline unsigned nn_public_buffer_cpp(unsigned f)
{switch(f){case 1:return 1;case 5:case 6:case 13:return 2;case 10:case 17:case 18:case 37:return 4;case 22:case 41:case 42:return 8;case 46:return 16;default:return 0;}}
static inline unsigned nn_public_buffer_components(unsigned f)
{return f<=10?1:f<=22?2:4;}
static inline unsigned nn_public_buffer_width(unsigned f)
{return nn_public_buffer_cpp(f)/nn_public_buffer_components(f);}
static inline uint32_t nn_public_buffer_word(unsigned f,unsigned phase,unsigned index,unsigned channel)
{
 unsigned code=1+(phase+index+channel)%3;
 /* Fourth phase for RGBA formats explicitly exercises the original discard. */
 if(phase==3&&channel==3)code=0;
 if(f==1||f==6||f==13||f==18||f==37||f==42){unsigned width=nn_public_buffer_width(f);return width==1?(uint32_t[]){0,64,128,192}[code]:(uint32_t[]){0,16384,32768,49152}[code];}
 if(f==5||f==17||f==41)return(uint32_t[]){0,0x3400,0x3800,0x3a00}[code];
 float value=(float[]){0,.25f,.5f,.75f}[code];uint32_t word;memcpy(&word,&value,4);return word;
}
static inline unsigned char nn_public_buffer_byte(unsigned f,unsigned phase,size_t at)
{
 unsigned cpp=nn_public_buffer_cpp(f),width=nn_public_buffer_width(f);size_t offset=4096+cpp;
 if(at<offset||at-offset>=17u*cpp)return 0xa5;
 unsigned index=(unsigned)((at-offset)/cpp),byte=(unsigned)((at-offset)%cpp);
 return(unsigned char)(nn_public_buffer_word(f,phase,index,byte/width)>>((byte%width)*8));
}
static inline double nn_public_buffer_value(unsigned f,unsigned phase,unsigned channel)
{
 if(channel>=nn_public_buffer_components(f))return channel==3?1:0;
 unsigned code=1+(phase+channel)%3;if(phase==3&&channel==3)code=0;
 if(f==1||f==13||f==37)return(double[]){0,64./255,128./255,192./255}[code];
 if(f==6||f==18||f==42)return(double[]){0,16384./65535,32768./65535,49152./65535}[code];
 return(double[]){0,.25,.5,.75}[code];
}
static inline void nn_public_buffer_pixel(unsigned f,unsigned phase,unsigned x,unsigned y,unsigned char out[4])
{
 static const unsigned char clear[4]={17,29,43,59};double alpha=nn_public_buffer_value(f,phase,3);
 if(alpha==0){memcpy(out,clear,4);return;}
 double u=(x+.5)/64,v=(y+.5)/64,color[4]={.25+.25*u,.375+.25*v,.5+.125*(u+v),1};
 for(unsigned k=0;k<4;k++)out[k]=(unsigned char)lround(color[k]*nn_public_buffer_value(f,phase,k)*255);
}
