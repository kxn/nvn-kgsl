/* SPDX-License-Identifier: MIT
 * Native semantic state -> A650 register-only draw segment. Hardware facts:
 * Mesa26.2.5 a6xx.xml/adreno_common.xml, fd6_blend.cc/fd6_rasterizer.cc/
 * fd6_emit.cc. No Gallium/Vulkan runtime is linked. */
#include "nvn_native/render_state.h"
#include "nvn_native/pm4.h"
#include <nvn.h>
#ifndef NN_PIPELINE_FRONT_FACING
#define NN_PIPELINE_FRONT_FACING 0
#endif
#include <errno.h>
#include <math.h>
#include <string.h>

static int factor(uint32_t value)
{
    switch(value) {
    case NVN_BLEND_FUNC_ZERO:return 0;case NVN_BLEND_FUNC_ONE:return 1;
    case NVN_BLEND_FUNC_SRC_COLOR:return 4;case NVN_BLEND_FUNC_ONE_MINUS_SRC_COLOR:return 5;
    case NVN_BLEND_FUNC_SRC_ALPHA:return 6;case NVN_BLEND_FUNC_ONE_MINUS_SRC_ALPHA:return 7;
    case NVN_BLEND_FUNC_DST_ALPHA:return 10;case NVN_BLEND_FUNC_ONE_MINUS_DST_ALPHA:return 11;
    case NVN_BLEND_FUNC_DST_COLOR:return 8;case NVN_BLEND_FUNC_ONE_MINUS_DST_COLOR:return 9;
    case NVN_BLEND_FUNC_SRC_ALPHA_SATURATE:return 16;
    case NVN_BLEND_FUNC_CONSTANT_COLOR:return 12;case NVN_BLEND_FUNC_ONE_MINUS_CONSTANT_COLOR:return 13;
    case NVN_BLEND_FUNC_CONSTANT_ALPHA:return 14;case NVN_BLEND_FUNC_ONE_MINUS_CONSTANT_ALPHA:return 15;
    case NVN_BLEND_FUNC_SRC1_COLOR:return 20;case NVN_BLEND_FUNC_ONE_MINUS_SRC1_COLOR:return 21;
    case NVN_BLEND_FUNC_SRC1_ALPHA:return 22;case NVN_BLEND_FUNC_ONE_MINUS_SRC1_ALPHA:return 23;
    default:return -1;
    }
}
static int dual(uint32_t value){return value>=0x10&&value<=0x13;}
static int compare(uint32_t value){return value>=1&&value<=8;}
static int samples(uint32_t value){return value==0||value==1||value==2||value==4||value==8||value==16;}
static uint32_t bits(float f){uint32_t v;memcpy(&v,&f,4);return v;}
void nn_render_state_defaults(struct nn_render_state *s)
{
    if(!s)return;
    memset(s,0,sizeof(*s));s->version=NN_RENDER_STATE_VERSION;
    for(unsigned i=0;i<8;i++)s->blend[i]=(struct nn_render_blend){2,1,2,1,1,1};
    s->color_mask=UINT32_MAX;s->logic_op=NVN_LOGIC_OP_COPY;s->alpha_func=NVN_ALPHA_FUNC_ALWAYS;
    s->depth.depth_write=1;s->depth.depth_func=NVN_DEPTH_FUNC_LESS;
    for(unsigned i=0;i<2;i++){
        s->depth.stencil_func[i]=NVN_STENCIL_FUNC_ALWAYS;
        s->depth.stencil_fail[i]=s->depth.stencil_depth_fail[i]=s->depth.stencil_depth_pass[i]=NVN_STENCIL_OP_KEEP;
        s->stencil_value_mask[i]=s->stencil_write_mask[i]=UINT32_MAX;
    }
    s->multisample.enable=1;s->multisample.alpha_to_coverage_dither=1;
    memset(s->multisample.sample_locations,0x88,sizeof(s->multisample.sample_locations));
    s->front_face=NVN_FRONT_FACE_CCW;s->polygon_mode=NVN_POLYGON_MODE_FILL;
    s->depth_bounds_max=1.0f;s->patch_size=3;
}
static int validate(const struct nn_render_state *s,const struct nn_render_target_info *t,int depth_raster)
{
    if(!s||!t)return EINVAL;
    if(s->version!=NN_RENDER_STATE_VERSION)return EINVAL;
    if(t->chip!=0x06050002||t->window_origin>1||t->samples!=1)return ENOTSUP;
    if(depth_raster){if(t->color_targets>1||t->depth_attachment!=1)return ENOTSUP;}
    else if(t->color_targets!=1||t->depth_attachment)return ENOTSUP;
    if(s->blend_enable>255||s->logic_op>15||!compare(s->alpha_func)||s->cull_face>3||s->front_face>1||s->polygon_mode>2||s->polygon_offset_enables>7)return EINVAL;
    for(unsigned i=0;i<8;i++){
        const struct nn_render_blend *b=s->blend+i;
        if(factor(b->src_rgb)<0||factor(b->dst_rgb)<0||factor(b->src_alpha)<0||factor(b->dst_alpha)<0||b->equation_rgb<1||b->equation_rgb>5||b->equation_alpha<1||b->equation_alpha>5)return EINVAL;
    }
    const struct nn_render_depth_stencil *d=&s->depth;
    if(d->depth_test>1||d->depth_write>1||d->stencil_test>1||!compare(d->depth_func))return EINVAL;
    for(unsigned i=0;i<2;i++)if(!compare(d->stencil_func[i])||!compare(d->stencil_fail[i])||!compare(d->stencil_depth_fail[i])||!compare(d->stencil_depth_pass[i]))return EINVAL;
    const struct nn_render_multisample *m=&s->multisample;
    if(m->enable>1||!samples(m->samples)||m->alpha_to_coverage>1||m->alpha_to_coverage_dither>1||!samples(m->raster_samples)||m->coverage_modulation>3||m->coverage_to_color>1||m->coverage_output>7||m->locations_enable>1||m->locations_grid_enable>1)return EINVAL;
    if(s->depth_bounds_enable>1||!isfinite(s->depth_bounds_min)||!isfinite(s->depth_bounds_max)||s->depth_bounds_min<0||s->depth_bounds_max>1||s->depth_bounds_min>s->depth_bounds_max||s->patch_size<1||s->patch_size>32)return EINVAL;
    for(unsigned i=0;i<4;i++)if(!isfinite(s->blend_color[i]))return EINVAL;
    if(!isfinite(s->polygon_offset_factor)||!isfinite(s->polygon_offset_units)||!isfinite(s->polygon_offset_clamp))return EINVAL;
    /* The current target has no Z/S storage. Default write=true is harmless
     * with depth testing off; never reject that legitimate default. */
    if(d->depth_test||d->stencil_test||s->depth_bounds_enable)return ENOTSUP;
    if(m->samples>1||m->raster_samples>1||m->alpha_to_coverage||m->coverage_modulation||m->coverage_to_color||m->locations_enable)return ENOTSUP;
    if(s->alpha_func!=NVN_ALPHA_FUNC_ALWAYS)return ENOTSUP;
    if(t->color_targets&&s->logic_op==NVN_LOGIC_OP_COPY&&(s->blend_enable&1)&&(dual(s->blend[0].src_rgb)||dual(s->blend[0].dst_rgb)||dual(s->blend[0].src_alpha)||dual(s->blend[0].dst_alpha)))return ENOTSUP;
    /* Upper-left cull/front-face polarity needs its independent qualification;
     * existing mode qualification has no culling/front-facing shader consumer. */
    if(!NN_PIPELINE_FRONT_FACING&&t->window_origin&&s->cull_face)return ENOTSUP;
    return 0;
}
static int encode(const struct nn_render_state *s,const struct nn_render_target_info *t,uint32_t *words,size_t capacity,size_t *out_words,int depth_raster)
{
    if(!out_words)return EINVAL;
    *out_words=0;int e=validate(s,t,depth_raster);if(e)return e;
    uint32_t staged[64];size_t n=0;
#define REG(r,v) do{staged[n++]=nvn_pm4_packet4((r),1);staged[n++]=(v);}while(0)
    const struct nn_render_blend *b=&s->blend[0];
    uint32_t blend=factor(b->src_rgb)|((b->equation_rgb-1)<<5)|(factor(b->dst_rgb)<<8)|
        (factor(b->src_alpha)<<16)|((b->equation_alpha-1)<<21)|(factor(b->dst_alpha)<<24);
    static const uint8_t rop[16]={0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};
    unsigned logic=t->color_targets&&s->logic_op!=NVN_LOGIC_OP_COPY;
    unsigned enabled=t->color_targets&&(s->blend_enable&1)&&!logic;
    /* For non-COPY logic ops blending is disabled, as fixed-function logic
     * and blending are mutually exclusive. Dest reads also cover logic ops. */
    unsigned read_dest=enabled||(logic&&s->logic_op!=NVN_LOGIC_OP_CLEAR&&s->logic_op!=NVN_LOGIC_OP_COPY_INVERTED&&s->logic_op!=NVN_LOGIC_OP_SET);
    REG(0x8821,blend);REG(0x8820,enabled*3u|(logic<<2)|((uint32_t)rop[s->logic_op]<<3)|(t->color_targets?((s->color_mask&15)<<7):0));
    REG(0xa989,read_dest);REG(0x8865,UINT32_C(0xffff0000)|read_dest);
    for(unsigned i=0;i<4;i++)REG(0x8860+i,bits(s->blend_color[i]));
    /* a6xx_gras_su_cntl: 0.5 linehalfwidth in radix2 is2, bits3..10.
     * The currently supported target is one sample. Polygon lines use the
     * NVN multisample-enable raster mode; filled triangles do not consume it. */
    uint32_t su=s->cull_face|((s->front_face==NVN_FRONT_FACE_CW)<<2)|(2u<<3)|
        ((!!(s->polygon_offset_enables&(1u<<s->polygon_mode)))<<11)|
        (s->multisample.enable<<13);
    REG(0x8090,su);REG(0x8095,bits(s->polygon_offset_factor));REG(0x8096,bits(s->polygon_offset_units));REG(0x8097,bits(s->polygon_offset_clamp));
    REG(0x9108,s->polygon_mode+1);REG(0x9981,s->polygon_mode+1);
    REG(0x8864,0);REG(0x8871,0);REG(0x8880,0);REG(0x8114,0);REG(0x8115,0); /* A6 GRAS_SU_DEPTH/STENCIL_CNTL */
    REG(0x8888,(s->stencil_value_mask[0]&255)|((s->stencil_value_mask[1]&255)<<8));
    REG(0x8889,(s->stencil_write_mask[0]&255)|((s->stencil_write_mask[1]&255)<<8));
    REG(0x8887,(s->stencil_ref[0]&255)|((s->stencil_ref[1]&255)<<8));
    REG(0x8878,bits(s->depth_bounds_min));REG(0x8879,bits(s->depth_bounds_max));
#undef REG
    if(!words){if(capacity)return EINVAL;*out_words=n;return 0;}
    if(capacity<n)return ENOSPC;
    memcpy(words,staged,n*sizeof(*words));*out_words=n;return 0;
}
int nn_render_state_validate(const struct nn_render_state*s,const struct nn_render_target_info*t)
{return validate(s,t,0);}
int nn_render_state_encode(const struct nn_render_state*s,const struct nn_render_target_info*t,uint32_t*w,size_t n,size_t*out)
{return encode(s,t,w,n,out,0);}
int nn_render_state_encode_depth_raster(const struct nn_render_state*s,const struct nn_render_target_info*t,uint32_t*w,size_t n,size_t*out)
{return encode(s,t,w,n,out,1);}
