/* SPDX-License-Identifier: MIT. Bounded authored raw-block GPU qualification. */
#pragma once
#include "block_copy_cases.h"
#define NN_BLOCK_PROBE_CASES 148u
#define NN_BLOCK_PROBE_REPEATS 2u
#define NN_BLOCK_PROBE_CANONICAL_OFFSET 512u
static inline int nn_block_probe_case(unsigned n,struct nn_tex_source_layout *s,struct nn_block_copy_plan *p,unsigned *shared)
{
 if(n>=NN_BLOCK_PROBE_CASES)return 22;
 unsigned f=n/4,partial=(n/2)%2,dir=n%2;const unsigned heights[]={8,11,17,128};struct nn_block_copy_region region;
 int e=nn_block_copy_case(nn_block_copy_formats[f],heights[f%4],partial?1+f%2:0,partial,dir,s,&region);if(e)return e;
 if(partial)region.linear_image_stride|=1u;
 *shared=partial;
 return nn_block_copy_plan(s,&region,p);
}
static inline size_t nn_block_probe_linear_offset(const struct nn_block_copy_plan *p,unsigned shared){return shared?NN_BLOCK_PROBE_CANONICAL_OFFSET+(size_t)p->canonical_bytes+68:68;}
static inline size_t nn_block_probe_bo_bytes(const struct nn_block_copy_plan *p,unsigned shared,unsigned index)
{
 if(shared&&index)return 0;
 size_t end=index||shared?nn_block_probe_linear_offset(p,shared)+(size_t)p->linear_bytes:NN_BLOCK_PROBE_CANONICAL_OFFSET+(size_t)p->canonical_bytes;
 return (end+4096+4095)&~(size_t)4095;
}
