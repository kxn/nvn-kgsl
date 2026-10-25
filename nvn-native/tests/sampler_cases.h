/* SPDX-License-Identifier: MIT */
#ifndef NN_SAMPLER_CASES_H
#define NN_SAMPLER_CASES_H
#include "texture_cases.h"
/* Inputs to the unchanged original VS/FS. This reference evaluates a texture
 * sampling equation; it does not execute or interpret shader instructions. */
enum { NN_SAMPLER_CASES=38, NN_SAMPLER_BASE_STATES=18 };
struct nn_sampler_case { unsigned min,mag,s,t,minifying; float offset,span,u,v; int constant; };
static inline int nn_sampler_case_prepare(unsigned n,struct nn_texture_case_inputs *in,struct nn_sampler_case *s)
{
    static const unsigned wraps[3]={1,6,7};
    if(n>=NN_SAMPLER_CASES || !s || nn_texture_case_prepare(0,in))return -1;
    if(n==36)n=0;else if(n==37)n=17;
    memset(s,0,sizeof(*s));s->s=s->t=7;s->offset=-1.5f;s->span=4;
    if(n<18){s->min=s->mag=n/9;s->s=wraps[(n%9)/3];s->t=wraps[n%3];}
    else if(n<22){s->min=n<20?0:1;s->mag=1-s->min;s->minifying=n&1;s->s=1;s->t=6;}
    else if(n<28){s->min=n-22;s->mag=1;s->minifying=1;s->s=1;s->t=6;}
    else if(n<32){s->constant=1;s->u=(n&1)?.75f:.25f;s->v=(n&2)?.75f:.25f;}
    else {s->min=s->mag=1;s->constant=1;s->u=(n&1)?.625f:.375f;s->v=(n&2)?.625f:.375f;}
    if(s->minifying){s->offset=-.25f;s->span=48;}
    /* Alpha255 isolates sampler RGB equations from KIL. The original FS still
     * executes its alpha test and LG2/EX2/mix chain, with gamma=contrast=1. */
    for(unsigned k=0;k<4;k++)in->texels[k*4+3]=255;
    for(unsigned k=0;k<3;k++){
        in->vertices[k][2]=s->constant?s->u:s->offset+(k==1?2*s->span:0);
        in->vertices[k][3]=s->constant?s->v:s->offset+(k==2?2*s->span:0);
    }
    return 0;
}
static inline unsigned nn_sampler_texel(int i,unsigned wrap)
{
    if(wrap==7)return i<0?0:i>1?1:(unsigned)i;
    int j=i%(wrap==6?4:2);if(j<0)j+=wrap==6?4:2;
    return wrap==6?(unsigned)(j<2?j:3-j):(unsigned)j;
}
static inline struct nn_texture_expected nn_sampler_case_pixel(const struct nn_texture_case_inputs *in,
                                    const struct nn_sampler_case *s,unsigned x,unsigned y)
{
    struct nn_texture_expected out={{0,0,0,0},NN_TEXTURE_INVALID,0};
    if(!in||!s||x>=64||y>=64)return out;
    double u=s->constant?s->u:s->offset+s->span*((double)x+.5)/64;
    double v=s->constant?s->v:s->offset+s->span*((double)y+.5)/64;
    unsigned linear=s->minifying?(s->min&1):s->mag;
    double a=u*2-(linear?.5:0),b=v*2-(linear?.5:0);
    int ix=(int)floor(a),iy=(int)floor(b);double fx=linear?a-floor(a):0,fy=linear?b-floor(b):0;
    for(unsigned c=0;c<4;c++){
        double value=0;
        for(unsigned j=0;j<2;j++)for(unsigned i=0;i<2;i++){
            double weight=(i?fx:1-fx)*(j?fy:1-fy);
            unsigned tx=nn_sampler_texel(ix+(int)i,s->s),ty=nn_sampler_texel(iy+(int)j,s->t);
            value+=weight*in->texels[(ty*2+tx)*4+c];
        }
        /* With the declared identity vertex color, mix0/gamma1/contrast1,
         * the independently derived original FS equations reduce to sample.
         * One code covers UNORM quantization and LG2/EX2 rounding; the chosen
         * binary weights are exact in the hardware's fractional filter domain. */
        out.rgba[c]=nn_texture_quantize((float)(value/255.0));
    }
    out.reason=NN_TEXTURE_SHADED;out.tolerance=1;return out;
}
#endif
