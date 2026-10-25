/* SPDX-License-Identifier: MIT. Authored qualification cases, no game identity. */
#pragma once
#include "nvn_native/block_copy.h"
static const unsigned nn_block_copy_formats[]={1,5,6,7,8,9,0x25,0x26,0x27,0x28,0x38,0x9a,0x9c,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50};
#define NN_BLOCK_COPY_FORMAT_COUNT 37u
/* Construct cases using the existing source calculator, never a second layout.
 * heights in BLOCK ROWS exercise the 8/11/17/128 GOB boundaries. */
static int nn_block_copy_case(unsigned format,unsigned height,unsigned level,unsigned partial,unsigned direction,struct nn_tex_source_layout *s,struct nn_block_copy_region *c)
{
 struct nn_tex_source_desc d={.target=4,.format=format,.width=1,.height=1,.layers=3,.levels=1,.block_height_log2=-1};
 int e=nn_tex_source_layout(&d,s);if(e)return e;
 d.width=s->block_w*13-(s->block_w-1);d.height=s->block_h*height-(s->block_h-1);d.levels=3;
 e=nn_tex_source_layout(&d,s);if(e)return e;
 const struct nn_tex_source_level *m=s->level+level;
 unsigned xb=partial&&m->blocks_x>2?1:0,y=partial&&m->blocks_y>2?1:0;
 unsigned blocks=m->blocks_x-(xb?2:0),rows=m->blocks_y-(y?2:0),rb=blocks*s->bytes_per_block;
 unsigned stride=rb+(partial?1:0),image=(rows-1)*stride+rb+(partial?1:0);
 *c=(struct nn_block_copy_region){level,partial?1:0,partial?2:3,xb*s->bytes_per_block,y,rb,rows,stride,image,direction};return 0;
}

/* Independent inverse sector permutation from public minigbm transfer_tile.
 * Only address/lane-set math; never execute a shader or convert image bytes. */
static inline uint64_t nn_block_copy_oracle_offset(const struct nn_tex_source_layout *s,unsigned level,unsigned layer,unsigned x,unsigned y)
{
 const struct nn_tex_source_level *m=s->level+level;unsigned h=1u<<m->block_height_log2;
 unsigned bc=x/64,br=y/(8*h),localx=x%64,localy=y%(8*h),lane=x%4;
 unsigned k=(localy/8)*128+((localx/4)&8)*8+((localy&6)<<3)+((localx/4)&4)*2+((localy&1)<<2)+((localx/4)&3);
 return layer*s->layer_stride+m->offset+((uint64_t)br*(m->pitch/64)+bc)*512*h+k*4+lane;
}
/* Raw authored input pattern only, not an image conversion. A future GPU
 * collector derives expected bytes from this formula, never source readback. */
static inline unsigned char nn_block_copy_pattern(uint64_t offset,unsigned case_id,unsigned repeat,unsigned role)
{
 uint32_t x=(uint32_t)offset*1664525u+1013904223u+(case_id+1u)*2246822519u+repeat*3266489917u+role*668265263u;
 x^=x>>13;x*=1274126177u;x^=x>>16;return (unsigned char)x;
}
