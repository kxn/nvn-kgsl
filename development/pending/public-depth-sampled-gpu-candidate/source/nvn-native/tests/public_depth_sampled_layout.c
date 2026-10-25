/* HOST metadata, no pixel conversion. */
#include <stdio.h>
#include "nvn_native/texture_source.h"
#include NN_PUBLIC_DEPTH_SAMPLED_CASES_HEADER
static void layout(const struct nn_tex_source_layout*l){
 printf("{\"cpp\":%u,\"payload\":%llu,\"pool_bytes\":%llu,\"level\":[",l->bytes_per_block,(unsigned long long)l->payload_bytes,(unsigned long long)(8192+((l->payload_bytes+4095)&~4095ull)));
 for(unsigned m=0;m<l->levels;m++){const struct nn_tex_source_level*x=l->level+m;printf("%s{\"source_offset\":%llu,\"source_bytes\":%llu,\"row_bytes\":%u,\"rows\":%u,\"source_pitch\":%u,\"block_height_log2\":%u}",m?",":"",(unsigned long long)x->offset,(unsigned long long)x->bytes,x->row_bytes,x->height,x->pitch,x->block_height_log2);}printf("]}");}
int main(void){putchar('[');for(unsigned i=0;i<NN_PUBLIC_DEPTH_SAMPLED_CASE_COUNT;i++){
 const struct nn_public_depth_sampled_case*c=NN_PUBLIC_DEPTH_SAMPLED_CASES+i;
 struct nn_tex_source_desc sd={.target=1,.format=c->format,.width=17u<<c->mip,.height=11u<<c->mip,.levels=c->mip+1,.layers=1,.block_height_log2=-1},td={.target=1,.format=c->role?52:37,.width=c->width,.height=c->height,.levels=1,.layers=1,.block_height_log2=-1};struct nn_tex_source_layout s,t;if(nn_tex_source_layout(&sd,&s)||nn_tex_source_layout(&td,&t))return 2;
 printf("%s{\"case\":%u,\"source\":",i?",":"",i);layout(&s);printf(",\"target\":");layout(&t);putchar('}');}puts("]");return fflush(stdout)?1:0;}
