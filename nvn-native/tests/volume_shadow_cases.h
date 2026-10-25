/* SPDX-License-Identifier: MIT. Authored metadata; no game/pixel codec. */
#pragma once
#include "nvn_native/texture_volume_shadow.h"
#define NN_VOLUME_SHADOW_CASES 18u
#define NN_VOLUME_SHADOW_REPEATS 2u
struct nn_volume_shadow_case {
 uint32_t id,format,width,height,depth,levels,base_level,level_count,flags;
 uint64_t canonical_bo_bytes,native_bo_bytes,canonical_offset;
 unsigned char source[sizeof(struct nn_tex_source_layout)];
 unsigned char native[sizeof(struct nn_fdl_volume_view)];
};
static inline int nn_volume_shadow_case_spec(unsigned id,struct nn_tex_source_desc *s,unsigned *base,unsigned *count)
{
 static const unsigned formats[]={37,0x29,0x41,0x42,0x43,0x44};
 if(id>=18)return 22;
 unsigned v=id%3;*s=(struct nn_tex_source_desc){.target=2,.format=formats[id/3],.width=v==0?32:v==1?35:11,.height=v==0?32:v==1?19:17,
  .depth=v==0?32:v==1?9:5,.layers=1,.levels=v==0?1:v==1?3:4,.flags=formats[id/3]==0x29?4:0,.block_height_log2=-1};
 *base=v==0?0:v==1?1:2;*count=s->levels-*base;return 0;
}

/* Same authored byte pattern as existing raw-block fixtures. Input bytes only;
 * no texture address/layout or CPU image conversion is performed here. */
static inline unsigned char nn_volume_shadow_pattern(uint64_t offset,unsigned id,unsigned repeat,unsigned role)
{uint32_t x=(uint32_t)offset*1664525u+1013904223u+(id+1u)*2246822519u+repeat*3266489917u+role*668265263u;x^=x>>13;x*=1274126177u;x^=x>>16;return (unsigned char)x;}
