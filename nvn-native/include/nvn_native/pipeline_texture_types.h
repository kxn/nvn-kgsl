/* SPDX-License-Identifier: MIT. Trusted normal compiler facts, never guest labels. */
#pragma once
#include "pipeline_recipe.h"
#include "offline_graphics.h"
/* Actual NIR operation bits. Every table comes from the corresponding normal
 * compiler invocation and is immutable with its exact recipe + SPIR-V stages.
 * Existing recipe-v1 ABI and old artifacts stay unchanged. */
enum {NN_PIPELINE_TEX_SAMPLE=1,NN_PIPELINE_TEX_FETCH=2,NN_PIPELINE_TEX_SIZE=4,NN_PIPELINE_TEX_GATHER=16};
/* TG4 remains a distinct operation. Count corroborates the conservative
 * offline source resource.other count, never replaces or clears it. */
struct nn_pipeline_gather_usage {
 uint32_t count,component_mask,sampler_mask,offsets;
};
struct nn_pipeline_texture_types {
 uint32_t version;
 const struct nn_pipeline_shader_recipe *recipe;
 const uint32_t *spirv[2];size_t spirv_bytes[2];
 uint32_t texture_mask,sampler_mask,dimensions[16],operations[16];
 /* Appended: old source initializers are zero and preserve their v1 meaning. */
 struct nn_pipeline_gather_usage gathers[16];
};

/* Metadata validation is independent of runtime admission (default OFF).
 * Actual tg4 compiler profile is float, non-shadow normalized2D level0,
 * without offsets; native sampler and texture indices currently coincide.
 * Unknown "other" operations, BUFFER/3D gather and absent corroboration stay
 * rejected. This is a supported subset, not an NVN prohibition. */
#include <errno.h>
static inline int nn_pipeline_texture_type_validate(const struct nn_pipeline_texture_types *t,unsigned i)
{
 if(!t||i>=16)return EINVAL;
 const struct nn_pipeline_gather_usage *g=t->gathers+i;
 uint32_t op=t->operations[i],dim=t->dimensions[i],bit=1u<<i;
 if(!(t->texture_mask&bit))return dim||op||g->count||g->component_mask||g->sampler_mask||g->offsets?EINVAL:0;
 if(dim<1||dim>3||!op||(op&~(1u|2u|4u|16u)))return ENOTSUP;
 if((dim==3&&(op&~(NN_PIPELINE_TEX_FETCH|NN_PIPELINE_TEX_SIZE)))||(dim!=2&&(op&NN_PIPELINE_TEX_GATHER)))return ENOTSUP;
 if(op&NN_PIPELINE_TEX_GATHER){
  if(!g->count||!g->component_mask||(g->component_mask&~15u)||g->sampler_mask!=bit||g->offsets||!(t->sampler_mask&bit))return ENOTSUP;
 }else if(g->count||g->component_mask||g->sampler_mask||g->offsets)return EINVAL;
 return 0;
}
