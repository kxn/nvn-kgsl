/* SPDX-License-Identifier: MIT. Authored qualification inputs, no runtime codec. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "nvn_native/texture_source.h"
#ifdef __cplusplus
}
#endif
#include <errno.h>
#include <stdint.h>
#include <string.h>
#define NN_DEPTH_SHADOW_CASES 54u
#define NN_DEPTH_SHADOW_REPEATS 2u
struct nn_depth_shadow_case {unsigned format,profile,selected,role,origin,level,layer,compare;struct nn_tex_source_layout source;};
static inline int nn_depth_shadow_case(unsigned n,struct nn_depth_shadow_case *out)
{
 if(!out||n>=NN_DEPTH_SHADOW_CASES)return EINVAL;
 struct nn_depth_shadow_case c={0};c.format=50+n/18;c.profile=n/9%2;c.selected=n/3%3;c.role=n%3;c.origin=(c.role+c.selected)&1;c.level=c.selected;c.layer=c.profile?c.selected:0;c.compare=c.role==1?5:c.role==2?4:2;
 struct nn_tex_source_desc d={0};d.format=c.format;d.target=c.profile?4:1;d.width=c.profile?127:73;d.height=c.profile?65:41;d.levels=3;d.layers=c.profile?3:1;d.block_height_log2=-1;
 int e=nn_tex_source_layout(&d,&c.source);if(e)return e;*out=c;return 0;
}
static inline unsigned nn_depth_input_bank(unsigned x,unsigned y,unsigned level,unsigned layer,unsigned repeat)
{return(x+2*y+level+3*layer+repeat)&3u;}
static inline uint32_t nn_depth_input_word(unsigned format,unsigned x,unsigned y,unsigned level,unsigned layer,unsigned repeat)
{
 static const uint32_t values[3][4]={{8192,24576,40959,57343},{2097152,6291456,10485759,14680063},{0x3e000000,0x3ec00000,0x3f200000,0x3f600000}};
 uint32_t v=values[format-50][nn_depth_input_bank(x,y,level,layer,repeat)];if(format==51)v|=((x+3*y+layer+level+repeat)&255u)<<24;return v;
}
static inline unsigned char nn_depth_guard_byte(size_t at,unsigned test,unsigned repeat)
{return(unsigned char)((at*13u+test*23u+repeat*17u)^0xa5u);}
/* Input-only initialization: authored numeric depth controls written directly
 * into their canonical source storage, never a game-image conversion. */
static inline int nn_depth_fixture_input(const struct nn_depth_shadow_case*c,unsigned test,unsigned repeat,unsigned char *map,size_t bytes)
{
 if(!c||!map||bytes<8192+c->source.payload_bytes)return EINVAL;
 for(size_t at=0;at<bytes;at++)map[at]=nn_depth_guard_byte(at,test,repeat);
 for(unsigned level=0;level<c->source.levels;level++)for(unsigned layer=0;layer<c->source.layers;layer++)
  for(unsigned y=0;y<c->source.level[level].height;y++)for(unsigned x=0;x<c->source.level[level].width;x++){
   uint32_t word=nn_depth_input_word(c->format,x,y,level,layer,repeat);
   for(unsigned b=0;b<c->source.bytes_per_block;b++){uint64_t at;int e=nn_tex_source_offset(&c->source,level,layer,x,y,b,&at);if(e)return e;map[4096+at]=(unsigned char)(word>>(b*8));}
  }
 return 0;
}
