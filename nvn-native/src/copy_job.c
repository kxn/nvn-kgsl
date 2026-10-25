/* SPDX-License-Identifier: MIT */
#include "nvn_native/copy_job.h"
#include "copy_body.h"
#include "nvn_native/pm4.h"
#ifndef __user
#define __user
#endif
#include "msm_kgsl.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "adreno_pm4.xml.h"
struct nn_copy_job { struct nn_copy_desc desc; struct nn_copy_stats stats; nn_bo *commands; unsigned char *shadow; };
static int retain(nn_copy_job *j,int e)
{ j->stats.state=NN_COPY_RETAINED; j->stats.resources_retained=1; if(!j->stats.first_error)j->stats.first_error=e; return e; }
static int sequence_patch(const struct nn_gfx_image *i,uint32_t word)
{
    unsigned found=0;
    for(uint32_t n=0;n<i->patch_count;n++)if(i->patches[n].word==word){
        const struct nn_gfx_patch *p=i->patches+n;
        if(p->kind!=NN_GFX_SEQUENCE || p->offset || p->bytes!=4)return 0;
        ++found;
    }
    return found==1;
}
static int clean_tail(const struct nn_gfx_image *i)
{
    if(!i || i->schema!=3 || i->chip!=0x06050002 || i->profile!=NN_GFX_COLOR ||
        i->width!=64 || i->height!=64 || i->target_pitch!=256 || i->target_bytes!=16384 ||
        i->target_layer_bytes!=16384 || i->sequence_bytes!=4 || !i->commands ||
        i->command_words<26 || i->command_words>8192 || !i->patches || !i->patch_count ||
        i->patch_count>i->command_words/2)return ENOTSUP;
    uint32_t start=i->command_words-26;const uint32_t *w=i->commands+start;
    const unsigned positions[5]={0,5,10,12,14};
    const unsigned events[5]={PC_CCU_FLUSH_COLOR_TS,PC_CCU_FLUSH_DEPTH_TS,
        PC_CCU_INVALIDATE_COLOR,PC_CCU_INVALIDATE_DEPTH,CACHE_FLUSH_TS};
    for(unsigned n=0;n<5;n++){
        unsigned p=positions[n];int ts=n<2 || n==4;
        if(w[p]!=nvn_pm4_packet7(CP_EVENT_WRITE,ts?4:1) ||
            w[p+1]!=(CP_EVENT_WRITE_0_EVENT(events[n])|(ts?CP_EVENT_WRITE_0_TIMESTAMP:0)))return ENOTSUP;
        if(ts && (w[p+2] || w[p+3] || !sequence_patch(i,start+p+2)))return ENOTSUP;
    }
    if(w[19]!=nvn_pm4_packet7(CP_WAIT_FOR_IDLE,0) ||
        w[20]!=nvn_pm4_packet7(CP_SKIP_IB2_ENABLE_GLOBAL,1) || w[21] ||
        w[22]!=nvn_pm4_packet7(CP_EVENT_WRITE,1) || w[23]!=CP_EVENT_WRITE_0_EVENT(LRZ_FLUSH_INVALIDATE) ||
        w[24]!=nvn_pm4_packet7(CP_WAIT_FOR_IDLE,0) || w[25]!=nvn_pm4_packet7(CP_WAIT_FOR_ME,0))return ENOTSUP;
    return 0;
}
static int validate(const struct nn_copy_desc *d)
{
    if(!d || !d->device || !d->producer || !d->source || !d->target ||
        !d->owner.cookie || !d->owner.pin || !d->owner.validate_live ||
        !d->owner.prove_disjoint || !d->owner.unpin)return EINVAL;
    if(nn_device_chip(d->device)!=0x06050002 || nn_device_page(d->device)!=4096 ||
        nn_device_gmem_bytes(d->device)!=1179648 || d->cpu_no_access!=1 ||
        d->alias_proof!=NN_COPY_FRESH_PRIVATE_PAGES_AND_FRESH_AHB)return ENOTSUP;
    int e=clean_tail(d->source_image);if(e)return e;
    if(d->source_image->gmem_bytes!=nn_device_gmem_bytes(d->device))return ENOTSUP;
    if(!d->producer_timestamp || nn_queue_timestamp(d->producer)!=d->producer_timestamp)return ESTALE;
    if(d->source==d->target || d->source_offset%64 || d->target_offset%64 ||
        d->source_bytes!=16384 || d->target_bytes!=16384 ||
        d->source_offset>nn_bo_size(d->source) || d->source_bytes>nn_bo_size(d->source)-d->source_offset ||
        d->target_offset>nn_bo_size(d->target) || d->target_bytes>nn_bo_size(d->target)-d->target_offset ||
        nn_bo_address(d->source)>UINT64_MAX-d->source_offset ||
        nn_bo_address(d->source)+d->source_offset!=d->source_gpu_address ||
        (nn_bo_flags(d->target)&(KGSL_MEMFLAGS_GPUREADONLY|KGSL_MEMFLAGS_SECURE)) ||
        (nn_bo_flags(d->source)&KGSL_MEMFLAGS_SECURE))return EINVAL;
    struct nn_copy64_region s={nn_bo_address(d->source),nn_bo_size(d->source),d->source_offset,256,64,64,1,1,1,1,1};
    struct nn_copy64_region t={nn_bo_address(d->target),nn_bo_size(d->target),d->target_offset,256,64,64,1,1,1,1,1};
    size_t words=99;e=nn_copy64_body(0x06050002,&s,&t,NULL,0,&words);
    return e==ENOSPC && !words ? 0 : e?e:EINVAL;
}
int nn_copy_job_create(const struct nn_copy_desc *d,nn_copy_job **out)
{
    if(!out)return EINVAL;
    *out=NULL;
    int e=validate(d);if(e)return e;
    e=d->owner.validate_live(d->owner.cookie);if(e)return e;
    e=d->owner.prove_disjoint(d->owner.cookie);if(e)return e;
    nn_copy_job *j=calloc(1,sizeof(*j));if(!j)return ENOMEM;
    j->desc=*d;j->stats.state=NN_COPY_READY;
    e=d->owner.pin(d->owner.cookie);if(e){free(j);return e;}
    j->stats.owner_pinned=1;*out=j;
    const size_t words=64u*64u*6u+5u;
    e=nn_bo_alloc(d->device,words*4,&j->commands);
    if(!e && !nn_bo_map(j->commands))e=EFAULT;
    if(!e){
        struct nn_copy64_region s={nn_bo_address(d->source),nn_bo_size(d->source),d->source_offset,256,64,64,1,1,1,1,1};
        struct nn_copy64_region t={nn_bo_address(d->target),nn_bo_size(d->target),d->target_offset,256,64,64,1,1,1,1,1};
        size_t n=0;memset(nn_bo_map(j->commands),0xa5,nn_bo_size(j->commands));
        e=nn_copy64_body(0x06050002,&s,&t,nn_bo_map(j->commands),nn_bo_size(j->commands)/4,&n);
        if(!e){j->stats.command_bytes=n*4;j->shadow=malloc(nn_bo_size(j->commands));
            if(!j->shadow)e=ENOMEM;else memcpy(j->shadow,nn_bo_map(j->commands),nn_bo_size(j->commands));}
    }
    if(e){int cleanup=nn_copy_job_destroy(j);if(!cleanup)*out=NULL;return e;}
    return 0;
}
int nn_copy_job_submit(nn_copy_job *j,enum nn_copy_order order)
{
    if(!j)return EINVAL;
    if(j->stats.state!=NN_COPY_READY)return EBUSY;
    if(order!=NN_COPY_SAME_QUEUE && order!=NN_COPY_AFTER_FINISH)return EINVAL;
    int e=validate(&j->desc);if(e)return e;
    e=j->desc.owner.validate_live(j->desc.owner.cookie);if(e)return e;
    e=j->desc.owner.prove_disjoint(j->desc.owner.cookie);if(e)return e;
    if(order==NN_COPY_AFTER_FINISH){
        ++j->stats.producer_finish_calls;
        e=nn_queue_finish(j->desc.producer,1000);if(e)return retain(j,e);
    }
    nn_bo *resources[2]={j->desc.source,j->desc.target};
    ++j->stats.copy_submissions;
    e=nn_queue_submit(j->desc.producer,j->commands,0,j->stats.command_bytes,resources,2,&j->stats.timestamp);
    if(e)return retain(j,e);
    j->stats.state=NN_COPY_SUBMITTED;return 0;
}
int nn_copy_job_finish(nn_copy_job *j,uint32_t ms)
{
    if(!j || !ms || ms>1500)return EINVAL;
    if(j->stats.state!=NN_COPY_SUBMITTED)return EBUSY;
    ++j->stats.finish_calls;
    int e=nn_queue_finish(j->desc.producer,ms);if(e)return retain(j,e);
    const unsigned char *bytes=nn_bo_map(j->commands);
    if(!bytes)return retain(j,EFAULT);
    for(size_t n=0;n<nn_bo_size(j->commands);n++){
        if(bytes[n]!=j->shadow[n])return retain(j,EIO);
        if(n<j->stats.command_bytes)j->stats.command_readonly_checks++;else j->stats.command_guard_checks++;
    }
    j->stats.state=NN_COPY_FINISHED;return 0;
}
int nn_copy_job_stats(const nn_copy_job *j,struct nn_copy_stats *out)
{if(!j || !out)return EINVAL;*out=j->stats;return 0;}
int nn_copy_job_destroy(nn_copy_job *j)
{
    if(!j)return 0;
    if(j->stats.state!=NN_COPY_READY && j->stats.state!=NN_COPY_FINISHED)return EBUSY;
    if(j->commands){int e=nn_bo_destroy(j->commands);if(e)return retain(j,e);j->commands=NULL;}
    if(j->stats.owner_pinned){int e=j->desc.owner.unpin(j->desc.owner.cookie);if(e)return retain(j,e);j->stats.owner_pinned=0;}
    free(j->shadow);free(j);return 0;
}
