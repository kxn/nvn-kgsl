/* SPDX-License-Identifier: MIT. Authored input data; no device pixel oracle. */
#pragma once
#include <stdint.h>
#define NN_SAMPLER_MIP_WIDTH 64u
#define NN_SAMPLER_MIP_HEIGHT 64u
#define NN_SAMPLER_MIP_SOURCE_WIDTH 127u
#define NN_SAMPLER_MIP_SOURCE_HEIGHT 73u
#define NN_SAMPLER_MIP_SOURCE_LEVELS 7u
#define NN_SAMPLER_MIP_JOBS 288u
#define NN_SAMPLER_MIP_SUBMITS 576u
#define NN_SAMPLER_MIP_SEQUENCE_OFFSET 45056u
struct nn_sampler_mip_case {
 uint32_t id,min_filter,mag_filter,wrap_index,view,profile;
 uint32_t scale_bits[2],lod_min_bits,lod_max_bits,lod_bias_bits;
};
static inline unsigned char nn_sampler_mip_source(unsigned level,unsigned x,unsigned y,unsigned channel,unsigned repeat)
{return (unsigned char)(32u+(x*13u+y*7u+channel*37u+level*29u+repeat*17u)%192u);}
static inline float nn_sampler_mip_float(uint32_t u)
{union{uint32_t u;float f;}value={.u=u};return value.f;}
