/* SPDX-License-Identifier: MIT. Authored public clear correctness matrix. */
#pragma once
#include "nvn_native/clear_color.h"
#include <errno.h>
#include <string.h>
#define NN_CLEAR_PROBE_CASES 67u
#define NN_CLEAR_PROBE_FILES (NN_CLEAR_PROBE_CASES*8u)
#define NN_CLEAR_PROBE_TARGET_OFFSET 65536u
struct nn_clear_probe_case {
 uint32_t id,format,width,height,state,cpp;size_t pool_bytes;
 struct nn_tex_source_layout source;
 struct nn_clear_color_rect rect[2];float color[2][4];uint32_t mask[2];
};
static inline unsigned nn_clear_probe_seed(unsigned n,unsigned origin,unsigned phase,unsigned repeat)
{return 0x41u+(n&1u)+origin*16u+phase*4u+repeat*2u;}
static inline int nn_clear_probe_case(unsigned n,struct nn_clear_probe_case *out)
{
 static const unsigned formats[]={1,2,5,6,7,10,13,14,17,18,19,22,37,38,41,42,43,46,154,56,156,63};
 if(!out||n>=NN_CLEAR_PROBE_CASES)return EINVAL;
 struct nn_clear_probe_case c={.id=n,.format=n==66?63:formats[n/3],.width=n==66?2880:73,.height=n==66?1620:41,.state=n==66?0:n%3,
 .color={{1,.25f,.5f,.75f},{.5f,1,0,.25f}}};
 struct nn_tex_source_desc d={.target=1,.format=c.format,.width=c.width,.height=c.height,.layers=1,.levels=1,.block_height_log2=-1};
 int e=nn_tex_source_layout(&d,&c.source);if(e)return e;c.cpp=c.source.bytes_per_block;
 c.pool_bytes=(NN_CLEAR_PROBE_TARGET_OFFSET+c.source.payload_bytes+8191)&~(size_t)4095;
 if(c.state==0){c.rect[0]=(struct nn_clear_color_rect){0,0,c.width,c.height};c.mask[0]=15;c.rect[1]=(struct nn_clear_color_rect){c.width/4,c.height/4,c.width/2,c.height/2};c.mask[1]=8;}
 else if(c.state==1){c.rect[0]=(struct nn_clear_color_rect){3,5,17,9};c.mask[0]=5;c.rect[1]=(struct nn_clear_color_rect){10,8,21,17};c.mask[1]=10;}
 else{c.rect[0]=(struct nn_clear_color_rect){c.width-1,c.height-1,4,4};c.mask[0]=1;c.rect[1]=(struct nn_clear_color_rect){c.width+9,c.height+3,4,5};c.mask[1]=15;}
 *out=c;return 0;
}
