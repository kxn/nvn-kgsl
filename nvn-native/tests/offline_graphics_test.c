/* SPDX-License-Identifier: MIT
 * HOST ONLY: immutable source matching/metadata. No shader/packet execution. */
#include "nvn_native/offline_graphics.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include NN_OFFLINE_GRAPHICS_ARCHIVE_HEADER
#ifndef NN_EXPECTED_GRAPHICS_PAIR_COUNT
#define NN_EXPECTED_GRAPHICS_PAIR_COUNT 2
#endif
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
int main(void)
{
    const struct nn_offline_graphics_archive *a=&nn_offline_graphics;
    const struct nn_offline_graphics_stage *out=(void*)1;const struct nn_offline_graphics_pair *pair=(void*)1;
    unsigned c1=0;
    CHECK(a->stage_count==5&&a->pair_count==NN_EXPECTED_GRAPHICS_PAIR_COUNT);
    for(size_t i=0;i<a->stage_count;i++){
        const struct nn_offline_graphics_stage *s=a->stages+i;
        CHECK(nn_offline_graphics_find_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,s->specialization,&out)==0&&out==s);
        CHECK(s->control_bytes==0x880&&s->source_bytes>=s->code_bytes);
        unsigned char *data=malloc(s->source_bytes),*control=malloc(s->control_bytes);CHECK(data&&control);
        memcpy(data,s->source,s->source_bytes);memcpy(control,s->control,s->control_bytes);
        for(size_t n=0;n<s->source_bytes;n++){
            data[n]^=1;CHECK(nn_offline_graphics_find_stage(a,s->stage,data,s->source_bytes,s->control,s->control_bytes,s->specialization,&out)==ENOTSUP&&out==NULL);data[n]^=1;
        }
        for(size_t n=0;n<s->control_bytes;n++){
            control[n]^=1;CHECK(nn_offline_graphics_find_stage(a,s->stage,s->source,s->source_bytes,control,s->control_bytes,s->specialization,&out)==ENOTSUP&&out==NULL);control[n]^=1;
        }
        unsigned char key[32];memcpy(key,s->specialization,32);key[31]^=1;
        CHECK(nn_offline_graphics_find_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,key,&out)==ENOTSUP&&out==NULL);
        CHECK(nn_offline_graphics_find_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes-1,s->specialization,&out)==ENOTSUP&&out==NULL);
        CHECK(nn_offline_graphics_find_stage(a,s->stage,s->source,s->source_bytes-1,s->control,s->control_bytes,s->specialization,&out)==ENOTSUP&&out==NULL);
        if(s->source_bytes>s->code_bytes){
            CHECK(s->source_bytes-s->code_bytes==256);CHECK(s->stage==NN_OFFLINE_FRAGMENT);
            CHECK(nn_offline_graphics_find_stage(a,s->stage,s->source,s->code_bytes,s->control,s->control_bytes,s->specialization,&out)==ENOTSUP&&out==NULL);
            int cb1=0;for(size_t j=0;j<s->cbuf_count;j++)if(s->cbufs[j].slot==1&&s->cbufs[j].binding==4)cb1=1;
            CHECK(cb1);CHECK(strstr(s->reflection_json,"requires explicit runtime handle-source mapping"));c1++;
        }
        free(data);free(control);
    }
    CHECK(c1==1);
    for(size_t i=0;i<a->pair_count;i++){
        const struct nn_offline_graphics_pair *p=a->pairs+i;
        CHECK(nn_offline_graphics_find_pair(a,p->chip,a->stages+p->vs_index,a->stages+p->fs_index,&pair)==0&&pair==p);
        CHECK(nn_offline_graphics_find_pair(a,0,a->stages+p->vs_index,a->stages+p->fs_index,&pair)==ENOTSUP&&pair==NULL);
        CHECK(p->vs_bytes&&p->fs_bytes&&strstr(p->compiler_json,"ubo_push_ranges")&&strstr(p->compiler_json,"sampler_prefetch"));
    }
    for(size_t i=0;i<a->stage_count;i++)for(size_t j=0;j<a->stage_count;j++)if(a->stages[i].stage==0&&a->stages[j].stage==1){
        int expected=ENOTSUP;for(size_t k=0;k<a->pair_count;k++)if(a->pairs[k].vs_index==i&&a->pairs[k].fs_index==j)expected=0;
        CHECK(nn_offline_graphics_find_pair(a,0x06050002,a->stages+i,a->stages+j,&pair)==expected);
        CHECK((pair!=NULL)==(expected==0));
    }
    const struct nn_offline_graphics_stage *s=a->stages;
    CHECK(nn_offline_graphics_find_stage(a,s->stage,NULL,s->source_bytes,s->control,s->control_bytes,s->specialization,&out)==EINVAL&&out==NULL);
    CHECK(nn_offline_graphics_find_stage(a,99,s->source,s->source_bytes,s->control,s->control_bytes,s->specialization,&out)==ENOTSUP&&out==NULL);
    CHECK(nn_offline_graphics_find_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,NULL,&out)==EINVAL&&out==NULL);
    CHECK(nn_offline_graphics_find_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,s->specialization,NULL)==EINVAL);
    struct nn_offline_graphics_stage stages[6];memcpy(stages,a->stages,5*sizeof(*stages));stages[5]=stages[0];
    struct nn_offline_graphics_archive duplicate={stages,6,a->pairs,a->pair_count};
    CHECK(nn_offline_graphics_find_stage(&duplicate,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,s->specialization,&out)==EEXIST&&out==NULL);
    struct nn_offline_graphics_pair pairs[4];memcpy(pairs,a->pairs,a->pair_count*sizeof(*pairs));pairs[a->pair_count]=pairs[0];duplicate=(struct nn_offline_graphics_archive){a->stages,a->stage_count,pairs,a->pair_count+1};
    CHECK(nn_offline_graphics_find_pair(&duplicate,pairs[0].chip,a->stages+pairs[0].vs_index,a->stages+pairs[0].fs_index,&pair)==EEXIST&&pair==NULL);
    duplicate.stage_count=129;CHECK(nn_offline_graphics_find_pair(&duplicate,pairs[0].chip,s,s,&pair)==EINVAL&&pair==NULL);
    struct nn_offline_graphics_stage foreign=*s;CHECK(nn_offline_graphics_find_pair(a,pairs[0].chip,&foreign,s,&pair)==EINVAL&&pair==NULL);
    printf("HOST ONLY shader archive matching checks=%u; GPU execution=false\n",checks);return 0;
}
