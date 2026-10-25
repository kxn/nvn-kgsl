/* SPDX-License-Identifier: MIT */
#include "nvn_native/offline_graphics.h"
#include <errno.h>
#include <string.h>
static int archive_valid(const struct nn_offline_graphics_archive *a)
{
    return a&&a->stages&&a->stage_count&&a->stage_count<=128&&a->pair_count<=16384&&(!a->pair_count||a->pairs);
}
static int stage_valid(const struct nn_offline_graphics_stage *s)
{
    return s->stage<=NN_OFFLINE_FRAGMENT&&s->source&&s->control&&s->specialization&&s->spirv&&s->reflection_json&&
        s->code_bytes>=128&&s->code_bytes%8==0&&s->source_bytes>=s->code_bytes&&s->source_bytes<=1048576&&
        (s->control_bytes==0x880||s->control_bytes==0x898)&&s->spirv_bytes>=20&&s->spirv_bytes%4==0&&
        s->cbuf_count<=16&&(!s->cbuf_count||s->cbufs);
}
int nn_offline_graphics_find_stage(const struct nn_offline_graphics_archive *a,uint32_t stage,
    const void *source,size_t bytes,const void *control,size_t controls,const unsigned char key[32],const struct nn_offline_graphics_stage **out)
{
    if(!out)return EINVAL;
    *out=NULL;
    if(!archive_valid(a)||!source||!control||!key||!bytes||bytes>1048576||!controls||controls>1048576)return EINVAL;
    if(stage>NN_OFFLINE_FRAGMENT)return ENOTSUP;
    const struct nn_offline_graphics_stage *found=NULL;
    for(size_t i=0;i<a->stage_count;i++){
        const struct nn_offline_graphics_stage *s=a->stages+i;
        if(!stage_valid(s))return EINVAL;
        if(s->stage!=stage||s->source_bytes!=bytes||s->control_bytes!=controls||memcmp(key,s->specialization,32))continue;
        if(memcmp(source,s->source,bytes)||memcmp(control,s->control,controls))continue;
        if(found)return EEXIST;
        found=s;
    }
    if(!found)return ENOTSUP;
    *out=found;return 0;
}
int nn_offline_graphics_find_pair(const struct nn_offline_graphics_archive *a,uint32_t chip,
    const struct nn_offline_graphics_stage *vs,const struct nn_offline_graphics_stage *fs,const struct nn_offline_graphics_pair **out)
{
    if(!out)return EINVAL;
    *out=NULL;
    if(!archive_valid(a)||!vs||!fs)return EINVAL;
    if(chip!=0x06050002&&chip!=0x43030b00)return ENOTSUP;
    size_t vi=SIZE_MAX,fi=SIZE_MAX;
    for(size_t i=0;i<a->stage_count;i++){if(a->stages+i==vs)vi=i;if(a->stages+i==fs)fi=i;}
    if(vi==SIZE_MAX||fi==SIZE_MAX||!stage_valid(vs)||!stage_valid(fs)||vs->stage!=NN_OFFLINE_VERTEX||fs->stage!=NN_OFFLINE_FRAGMENT)return EINVAL;
    const struct nn_offline_graphics_pair *found=NULL;
    for(size_t i=0;i<a->pair_count;i++){
        const struct nn_offline_graphics_pair *p=a->pairs+i;
        if(p->vs_index>=a->stage_count||p->fs_index>=a->stage_count||!p->vs_code||!p->fs_code||!p->compiler_json||!p->vs_bytes||!p->fs_bytes||p->vs_bytes%4||p->fs_bytes%4)return EINVAL;
        if(p->chip==chip&&p->vs_index==vi&&p->fs_index==fi){if(found)return EEXIST;found=p;}
    }
    if(!found)return ENOTSUP;
    *out=found;return 0;
}
