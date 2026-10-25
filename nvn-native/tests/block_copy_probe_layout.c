/* SPDX-License-Identifier: MIT. HOST metadata caller, never GPU pixels. */
#include "block_copy_probe_cases.h"
#include <stdio.h>
int main(void)
{
 puts("[");for(unsigned n=0;n<NN_BLOCK_PROBE_CASES;n++){
 struct nn_tex_source_layout s;struct nn_block_copy_plan p;unsigned shared;if(nn_block_probe_case(n,&s,&p,&shared))return 1;
 const struct nn_block_copy_region *b=&p.region;
 printf("%s{\"id\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"block_w\":%u,\"block_h\":%u,\"bytes_per_block\":%u,\"payload_bytes\":%llu,\"layer_stride\":%llu,\"shared\":%u,\"canonical_offset\":512,\"linear_offset\":%zu,\"bo_bytes\":[%zu,%zu],\"region\":[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u],\"args\":[",n?",":"",n,s.format,s.level[0].width,s.level[0].height,s.block_w,s.block_h,s.bytes_per_block,(unsigned long long)s.payload_bytes,(unsigned long long)s.layer_stride,shared,nn_block_probe_linear_offset(&p,shared),nn_block_probe_bo_bytes(&p,shared,0),nn_block_probe_bo_bytes(&p,shared,1),b->level,b->base_layer,b->layers,b->x_bytes,b->y_rows,b->row_bytes,b->rows,b->linear_row_stride,b->linear_image_stride,b->direction);
 const unsigned *a=(const unsigned*)&p.args;for(unsigned i=0;i<16;i++)printf("%s%u",i?",":"",a[i]);
 printf("],\"groups\":[%u,%u,%u],\"levels\":[",p.groups[0],p.groups[1],p.groups[2]);
 for(unsigned i=0;i<s.levels;i++){const struct nn_tex_source_level *m=s.level+i;printf("%s[%llu,%llu,%u,%u,%u,%u,%u,%u,%u]",i?",":"",(unsigned long long)m->offset,(unsigned long long)m->bytes,m->width,m->height,m->blocks_x,m->blocks_y,m->row_bytes,m->pitch,m->block_height_log2);}
 puts("]}");}puts("]");return 0;
}
