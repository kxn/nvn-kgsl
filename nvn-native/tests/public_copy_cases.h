/* SPDX-License-Identifier: MIT. Qualification metadata, not a runtime layout. */
#pragma once
#include "nvn_native/texture_source.h"
#include <nvn.h>
#ifndef NN_PUBLIC_COPY_CASES
#define NN_PUBLIC_COPY_CASES 234u
#endif
struct nn_public_copy_case {
 unsigned id,family,variant,target,format,width,height,layers,levels,flags,shared;
 struct nn_tex_source_layout source;
 unsigned level,base_layer,layer_count,x_bytes,y_rows,row_bytes,rows,row_stride,image_stride;
 unsigned canonical_offset,destination_offset,linear_offset;
 size_t linear_bytes,pool_bytes[2];
 NVNcopyRegion region[2];
};

/* Authored CPU qualification oracle only. Independent inverse-sector equation,
 * including true Z slabs/BD; never used by the runtime resource encoder. */
static inline uint64_t nn_public_copy_oracle_offset(const struct nn_tex_source_layout *s,unsigned level,unsigned layer,unsigned x,unsigned y)
{
 const struct nn_tex_source_level *m=s->level+level;unsigned xd=x%64/4,yl=y%8;
 unsigned k=(xd&8)*8+((yl&6)<<3)+(xd&4)*2+((yl&1)<<2)+(xd&3);
 if(s->volume_depth){
  unsigned d=1u<<s->level_block_depths[level];
  return m->offset+(((uint64_t)(layer/d)*((m->blocks_y+7)/8)+y/8)*(m->pitch/64)+x/64)*512*d+(uint64_t)(layer%d)*512+k*4+x%4;
 }
 unsigned h=1u<<m->block_height_log2;
 return layer*s->layer_stride+m->offset+((uint64_t)(y/(8*h))*(m->pitch/64)+x/64)*512*h+((y%(8*h))/8)*512+k*4+x%4;
}
