/* SPDX-License-Identifier: MIT. Actual native depth, no COLOR-role alias. */
#pragma once
#include "depth_shadow.h"
#include "sampler_state.h"
#include "pipeline_texture_types.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NN_DEPTH_SAMPLED_RESOURCES 7u
struct nn_depth_sampled_plan {
 struct nn_depth_fdl_desc descriptor;
 struct nn_depth_fdl_view native; /* actual address-zero TILE6_3 depth view */
 struct nn_depth_a2d_view aliases;
 struct nn_block_copy_plan copy; /* selected canonical view -> LINEAR stage */
};
/* D16/D24X8/D32F Generic16Bx2 only. A native2D selected mip/layer view,
 * not array-shader/mip-filter permission. Caller supplies explicit depth/0/1
 * swizzles from its independently established contract; no NVN default guessed.
 * All native metadata comes from the unchanged actual depth FDL facade. */
int nn_depth_sampled_plan(const struct nn_tex_source_layout *,uint32_t level,uint32_t layer,
 const uint32_t swizzle[4],struct nn_depth_sampled_plan *);
int nn_depth_sampled_validate(const struct nn_depth_fdl_view *);
/* Exact normal NIR operation facts must already be bound to the selected
 * Program/recipe/SPIR-V owner. This pure table validator cannot establish that
 * provenance. Supported noncompare float native2D roles SAMPLE/FETCH/SIZE/TG4;
 * Gather componentR only, no offsets; unknown bits/dimensions reject. */
int nn_depth_sampled_types(const struct nn_pipeline_texture_types *,unsigned binding,uint32_t *operations);
int nn_depth_sampled_sampler(const struct nn_depth_fdl_view *,const struct nn_sampler_state *,
 const struct nn_sampler_layout *,uint32_t words[4]);
struct nn_depth_sampled_shadow {
 struct nn_depth_attachment native;
 struct nn_depth_readback linear;
 nn_bo *code,*uniform;
 struct nn_depth_shadow_step copy,upload;
 struct nn_block_copy_range canonical; /* borrowed, always held by caller */
 struct nn_depth_sampled_plan plan;
 uint32_t ready;
};
/* Seven complete submission refs: canonical + SIX actual private BOs. No
 * readback/store jobs are built for a sampled-only immutable enqueue version.
 * Caller first preflights complete Draw/resource/alias/capacity metadata. This
 * function additionally revalidates every own input before first BO allocation.
 * Images/canonical are never CPU mapped, copied or cache-operated. Failure can
 * retain nonNULL BOs if cleanup fails; quarantine/retry destroy safely. */
int nn_depth_sampled_create(nn_device *,const struct nn_cs_image *,const struct nn_pipeline_platform *,
 const struct nn_tex_source_layout *,const struct nn_depth_sampled_plan *,const struct nn_block_copy_range *,
 size_t resource_capacity,struct nn_depth_sampled_shadow *);
int nn_depth_sampled_destroy(struct nn_depth_sampled_shadow *);
int nn_depth_sampled_resources(const struct nn_depth_sampled_shadow *,nn_bo **,size_t capacity,size_t *count);
/* Fresh CS untile then A2D UPLOAD, both with full existing cache ordering;
 * caller then performs its actual native shader read. Finish before replay or
 * CPU canonical changes. Unknown completion retains the entire owner+canonical. */
int nn_depth_sampled_sequence(const struct nn_depth_sampled_shadow *,struct nn_depth_shadow_step load[2]);
/* Borrowed actual native owner and exact FDL depth descriptor. Output zero on
 * failure. Descriptor/array span is not derived from a color format or pitch. */
int nn_depth_sampled_binding(const struct nn_depth_sampled_shadow *,struct nn_depth_attachment *);
#ifdef __cplusplus
}
#endif
