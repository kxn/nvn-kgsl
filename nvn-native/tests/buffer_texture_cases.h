/* SPDX-License-Identifier: MIT. Authored cases/raw words, never runtime pixel conversion. */
#pragma once
#include "nvn_native/texture_buffer_format.h"
#include "nvn_native/texture_buffer_sample.h"
#include <string.h>
enum {NN_BUFFER_CASES=43,NN_BUFFER_DRAWS=132,NN_BUFFER_PREPARES=86};
struct nn_buffer_texture_case {uint32_t id,format,elements,cpp,prefix,float_result,source_offset,source_bytes;};
static inline int nn_buffer_texture_case(unsigned id,struct nn_buffer_texture_case*out)
{
 static const uint32_t formats[40]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,154};
 if(!out||id>=NN_BUFFER_CASES)return EINVAL;
 uint32_t f=id<40?formats[id]:(uint32_t[]){37,10,46}[id-40],cpp,result;
 int e=nn_tex_buffer_element_bytes(f,&cpp);if(e)return e;
 if((e=nn_buffer_sampled_result(f,&result)))return e;
 uint32_t count=id<40?(uint32_t[]){1,13,17,33}[id%4]:(uint32_t[]){32767,32768,65536}[id-40];
 uint32_t prefix=id<40?(id%3)*cpp:(uint32_t[]){4,28,48}[id-40];
 uint32_t offset=4096+prefix,size=(offset+count*cpp+256+4095)&~4095u;
 *out=(struct nn_buffer_texture_case){id,f,count,cpp,prefix,result==NN_BUFFER_FLOAT,offset,size};return 0;
}
static inline unsigned char nn_buffer_texture_byte(const struct nn_buffer_texture_case*c,unsigned repeat,size_t offset)
{
 if(offset<c->source_offset||offset-c->source_offset>=(size_t)c->elements*c->cpp)return 0xa5;
 size_t at=offset-c->source_offset;uint32_t index=(uint32_t)(at/c->cpp),byte=(uint32_t)(at%c->cpp);
 uint32_t seed=17+c->id*7+repeat*29+index*7+(index>>8)*11;
 if(!c->float_result)return(unsigned char)(seed+byte*37);
 unsigned width,kind;
 switch(c->format){
 case 1:case 13:case 37:case 154:width=1;kind=0;break;
 case 2:case 14:case 38:width=1;kind=1;break;
 case 5:case 17:case 41:width=2;kind=2;break;
 case 6:case 18:case 42:width=2;kind=0;break;
 case 7:case 19:case 43:width=2;kind=1;break;
 default:width=4;kind=3;break;
 }
 uint32_t component=byte/width,v=(seed+component*3)%4,word;
 if(kind==0)word=width==1?(uint32_t[]){0,64,128,255}[v]:(uint32_t[]){0,16384,32768,65535}[v];
 else if(kind==1)word=width==1?(uint32_t)(int32_t)(int8_t[]){-127,-64,0,127}[v]:(uint32_t)(int32_t)(int16_t[]){-32767,-16384,0,32767}[v];
 else if(kind==2)word=(uint32_t[]){0,0x3400,0x3800,0x3c00}[v];
 else{float value=(float[]){0,.25f,.5f,1}[v];memcpy(&word,&value,4);}
 return(unsigned char)(word>>((byte%width)*8));
}
