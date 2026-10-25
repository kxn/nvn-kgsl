/* SPDX-License-Identifier: MIT. Actual A650 VFD field definitions from public
 * Qualcomm a6xx.xml/fd6_format_table.c/fd6_context.cc; no fixed vertex layout. */
#include "nvn_native/pipeline_vertex.h"
#include "nvn_native/pm4.h"
#include "nvn_native/depth_native.h"
#ifndef NN_PIPELINE_DEPTH_NATIVE
#define NN_PIPELINE_DEPTH_NATIVE 0
#endif
#include <errno.h>
#include <string.h>
static void reg(uint32_t *words,size_t *n,uint32_t address,uint32_t value)
{words[(*n)++]=nvn_pm4_packet4(address,1);words[(*n)++]=value;}
static int format(uint32_t nvn,uint32_t *native,uint32_t *bytes)
{
    switch(nvn) {
    case 0x25:*native=48;*bytes=4;return 0;
    case 0x26:*native=50;*bytes=4;return 0;
    case 0xa:*native=74;*bytes=4;return 0;
    case 0x16:*native=103;*bytes=8;return 0;
    case 0x22:*native=116;*bytes=12;return 0;
    case 0x2e:*native=130;*bytes=16;return 0;
    default:return ENOTSUP;
    }
}
static int vertex_encode(const struct nn_pipeline_shader_recipe *r,const struct nn_pipeline_depth_export*depth,
    const struct nn_pipeline_vertex_state *v,uint32_t *words,size_t capacity,size_t *out_words)
{
    if(!out_words)return EINVAL;
    *out_words=0;
    if(depth){
#if NN_PIPELINE_DEPTH_NATIVE
        int e=nn_depth_native_recipe(depth);if(e)return e;
        if(depth->recipe!=r)return EINVAL;
#else
        return ENOTSUP;
#endif
    }
    if(!r||!v||r->version!=(depth&&depth->writes_depth?NN_PIPELINE_DEPTH_RECIPE_VERSION:NN_PIPELINE_RECIPE_VERSION)||r->chip!=0x06050002||r->input_count>32||
       (r->input_count&&!r->inputs)||v->attribute_count>32||v->stream_count>32)return EINVAL;
    uint32_t used=0,locations=0,fmt[32],size[32];
    for(size_t i=0;i<r->input_count;i++) {
        const struct nn_pipeline_vertex_input *in=r->inputs+i;
        if(in->location>=v->attribute_count||in->location>=32||in->regid>=252||
           !in->compmask||in->compmask>15||(locations&(1u<<in->location)))return EINVAL;
        locations|=1u<<in->location;
        const struct nn_pipeline_attribute *a=v->attributes+in->location;
        if(a->stream>=v->stream_count||a->stream>=32||a->offset>4095)return EINVAL;
        int e=format(a->format,fmt+i,size+i);if(e)return e;
        const struct nn_pipeline_buffer *b=v->buffers+a->stream;
        const struct nn_pipeline_stream *s=v->streams+a->stream;
        if(!b->bo||b->offset>nn_bo_size(b->bo)||b->bytes>nn_bo_size(b->bo)-b->offset||
           b->bytes>UINT32_MAX||a->offset>b->bytes||size[i]>b->bytes-a->offset)return EINVAL;
        uint64_t address=nn_bo_address(b->bo);
        if(!address||b->offset>UINT64_MAX-address||b->bytes>UINT64_MAX-address-b->offset)return EOVERFLOW;
        if((address+b->offset)%4||a->offset%4||s->stride%4)return EINVAL;
        if(s->stride>4095)return ENOTSUP; /* A6XX STRIDE low0/high11. */
        used|=1u<<a->stream;
    }
    if((r->vertex_id_regid!=252&&r->vertex_id_regid>=252)||
       (r->instance_id_regid!=252&&r->instance_id_regid>=252))return EINVAL;
    uint32_t local[512];size_t n=0;
    /* Disable stale windows even when a new program consumes fewer streams. */
    for(unsigned i=0;i<32;i++)reg(local,&n,0xa012+4*i,0);
    for(unsigned i=0;i<32;i++)if(used&(1u<<i)) {
        const struct nn_pipeline_buffer *b=v->buffers+i;uint64_t address=nn_bo_address(b->bo)+b->offset;
        local[n++]=nvn_pm4_packet4(0xa010+4*i,2);local[n++]=(uint32_t)address;local[n++]=(uint32_t)(address>>32);
        reg(local,&n,0xa012+4*i,(uint32_t)b->bytes);reg(local,&n,0xa013+4*i,v->streams[i].stride);
    }
    for(size_t i=0;i<r->input_count;i++) {
        const struct nn_pipeline_vertex_input *in=r->inputs+i;
        const struct nn_pipeline_attribute *a=v->attributes+in->location;
        const struct nn_pipeline_stream *s=v->streams+a->stream;
        uint32_t fetch=a->stream|(a->offset<<5)|(s->divisor?0x20000u:0)|(fmt[i]<<20)|0xc0000000u;
        reg(local,&n,0xa090+2*(uint32_t)i,fetch);reg(local,&n,0xa091+2*(uint32_t)i,s->divisor?s->divisor:1);
        reg(local,&n,0xa0d0+(uint32_t)i,in->compmask|(in->regid<<4));
    }
    reg(local,&n,0xa000,(uint32_t)r->input_count|((uint32_t)r->input_count<<8));
    reg(local,&n,0xa001,r->vertex_id_regid|(r->instance_id_regid<<8)|0xfcfc0000u);
    reg(local,&n,0xa002,0xfcfcu);reg(local,&n,0xa003,0xfcfcfcfcu);reg(local,&n,0xa004,0xfcu);
    reg(local,&n,0xa005,0xfcfcu);reg(local,&n,0xa006,0);
    if(!words&&capacity==0){*out_words=n;return 0;}
    if(!words)return EINVAL;
    if(capacity<n)return ENOSPC;
    memcpy(words,local,n*4);*out_words=n;return 0;
}

int nn_pipeline_vertex_encode(const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_vertex_state*v,uint32_t*w,size_t n,size_t*out){return vertex_encode(r,NULL,v,w,n,out);}
int nn_pipeline_vertex_encode_depth(const struct nn_pipeline_depth_export*d,const struct nn_pipeline_vertex_state*v,uint32_t*w,size_t n,size_t*out){if(!d){if(out)*out=0;return EINVAL;}return vertex_encode(d->recipe,d,v,w,n,out);}
