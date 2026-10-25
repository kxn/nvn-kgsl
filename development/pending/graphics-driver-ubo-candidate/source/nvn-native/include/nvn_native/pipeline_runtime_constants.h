/* SPDX-License-Identifier: MIT. Actual common IR3 runtime constants. */
#pragma once
#include "pipeline_shader.h"
#include "graphics.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_pipeline_native_artifact;
/* Structural, actual recipe/SPIR-V identity and exact packet coverage. No BO
 * allocations/maps/submission. NULL is allowed only for a recipe with no new
 * address roles. Native bindings are never NVN API indices. */
int nn_pipeline_runtime_constants_validate(const struct nn_pipeline_runtime_constants *,
 const struct nn_pipeline_shader_recipe *,const struct nn_offline_graphics_stage *const [2]);
/* Caller supplies an explicit native DrawID. Indirect packets start their own
 * GPU draw index and require draw_id==0. No indirect BO is mapped or read.
 * Public NVN must independently establish any DrawID mapping before admission. */
int nn_pipeline_runtime_draw(const struct nn_pipeline_runtime_constants *,const struct nn_gfx_draw *,
 uint32_t draw_id,uint32_t words[16],uint32_t *indirect_dst_vec4);
/* Descriptor's entire declared range is included, not only the first push
 * range. Exact vec4 extents avoid exposing unrelated end padding. Strict
 * 16-byte alignment and <=64KiB range are the current implemented subset. */
int nn_pipeline_runtime_ubo_descriptor(struct nn_pipeline_buffer,uint32_t words[2]);
const struct nn_pipeline_runtime_constants *nn_pipeline_program_runtime_constants(const nn_pipeline_program *);
struct nn_pipeline_typed_archive;
struct nn_pipeline_runtime_archive {
 uint32_t version;const struct nn_pipeline_typed_archive *typed;
 const struct nn_pipeline_runtime_constants *const *pairs;size_t pair_count;
};
int nn_pipeline_runtime_archive_validate(const struct nn_pipeline_runtime_archive *);
int nn_pipeline_select_runtime_constants(const struct nn_pipeline_runtime_archive *,
 const struct nn_offline_graphics_stage *,const struct nn_offline_graphics_stage *,
 const struct nn_pipeline_shader_recipe *,const struct nn_pipeline_runtime_constants **);
/* Explicit constructors preserve legacy admission. Optional depth/storage
 * sidecars retain their existing independent feature checks and identities. */
int nn_pipeline_program_create_native_runtime(nn_device *,const struct nn_pipeline_native_artifact *,
 const struct nn_pipeline_depth_export *,const struct nn_pipeline_storage_types *,
 const struct nn_pipeline_runtime_constants *,nn_pipeline_program **,nn_pipeline_program **);
int nn_pipeline_program_create_typed_runtime(nn_device *,const struct nn_offline_graphics_archive *,
 const struct nn_offline_graphics_stage *,const struct nn_offline_graphics_stage *,const struct nn_pipeline_texture_types *,
 const struct nn_pipeline_depth_export *,const struct nn_pipeline_storage_types *,
 const struct nn_pipeline_runtime_constants *,nn_pipeline_program **,nn_pipeline_program **);
int nn_pipeline_shader_preflight_runtime(const struct nn_offline_graphics_stage *const [2],
 const struct nn_pipeline_shader_recipe *,const struct nn_pipeline_depth_export *,
 const struct nn_pipeline_runtime_constants *,const struct nn_pipeline_buffer [2][16]);
#ifdef __cplusplus
}
#endif
