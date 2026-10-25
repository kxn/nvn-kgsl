/* SPDX-License-Identifier: MIT. Uncompressed sampled2D role, not an MRT gate. */
#pragma once
#include "runtime_fdl.h"
#include "block_copy.h"
#include "sampler_state.h"
#include "pipeline_program.h"
#include "pipeline_recipe.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_sampled_image_plan {
 struct nn_fdl_image_desc descriptor;
 struct nn_fdl_image_view native;
 struct nn_fdl_format_info format;
 struct nn_block_copy_plan copy[NN_FDL_LEVELS];
 uint32_t count;
};
/* Actual sampled-only FDL metadata, float-result uncompressed 2D/layer1.
 * No Color22 list, integer reinterpretation, depth, array, volume, sampling
 * instruction, public Query answer, CPU pixel map or submission is inferred.
 * Canonical source retains its whole mip chain; selected view is independent.
 * Fresh per-enqueue upload prevents a stale first-upload-only cached shadow. */
int nn_sampled_image_plan(const struct nn_tex_source_layout *,uint32_t base_level,uint32_t level_count,struct nn_sampled_image_plan *);
int nn_sampled_image_validate(const struct nn_fdl_image_view *,struct nn_fdl_format_info *);
/* Already-native FDL storage only, never a canonical BO labeled as LINEAR.
 * Caller proves same-device ownership, correct encoding and no physical
 * feedback/CPU alias, retains original true owner and producer dependencies.
 * API cannot authenticate arbitrary caller storage as native by its GPUVA. */
struct nn_sampled_image_binding {nn_bo *bo;size_t owner_offset,owner_bytes,offset,bytes;};
int nn_sampled_image_descriptor(const struct nn_fdl_image_view *,const struct nn_sampled_image_binding *,uint32_t out[16]);
int nn_sampled_image_sampler(const struct nn_fdl_image_view *,const struct nn_sampler_state *,const struct nn_sampler_layout *,uint32_t out[4]);
/* Reads the existing program-owned exact recipe/SPIR-V normal NIR table.
 * Native producer currently rejects every non-Float32 image result; this
 * contract must be revised before future integer-compiler acceptance. A tag
 * supplied separately by a Draw caller is not proof. SAMPLE/FETCH/SIZE, dim2;
 * TG4 requires explicit NN_PIPELINE_GATHER plus its exact corroborated typed
 * count/component/sampler/no-offset records. Maxwell actual selectors/query resolution and
 * SNORM carrier compatibility still belong to the real resource resolver. */
int nn_sampled_image_program(const nn_pipeline_program *,const struct nn_pipeline_shader_recipe *,uint32_t binding,uint32_t *operations);
struct nn_sampled_image_shadow;
/* 2+2*N private owners + borrowed canonical. No pixel map or CPU conversion.
 * Fresh storage is physically disjoint from still-live imported canonical.
 * Caller strongly pins canonical/all private refs through real Finish and
 * unknown completion, inserts existing cache/order chain before consumer.
 * Same-device canonical and backing lifetime are caller obligations, as with
 * existing block_copy. Output starts NULL; failure can return retained owner
 * when rollback fails. Destroy errors keep *owner reachable, never free it. */
int nn_sampled_image_shadow_create(nn_device *,const struct nn_cs_image *,const struct nn_tex_source_layout *,const struct nn_sampled_image_plan *,const struct nn_block_copy_range *,struct nn_sampled_image_shadow **);
int nn_sampled_image_shadow_destroy(struct nn_sampled_image_shadow **);
int nn_sampled_image_shadow_resources(const struct nn_sampled_image_shadow *,nn_bo **,size_t capacity,size_t *count);
int nn_sampled_image_shadow_dispatch(const struct nn_sampled_image_shadow *,uint32_t mip,nn_bo **,size_t *bytes);
int nn_sampled_image_shadow_binding(const struct nn_sampled_image_shadow *,struct nn_sampled_image_binding *);
int nn_sampled_image_shadow_view(const struct nn_sampled_image_shadow *,const struct nn_fdl_image_view **);
int nn_sampled_image_shadow_descriptor(const struct nn_sampled_image_shadow *,uint32_t out[16]);
#ifdef __cplusplus
}
#endif
