/* SPDX-License-Identifier: MIT. Shader-state segment of a dynamic pipeline. */
#pragma once
#include "pipeline_program.h"
#include "pipeline_recipe.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_pipeline_buffer {nn_bo *bo;size_t offset,bytes;};
struct nn_pipeline_shader_bindings {
    struct nn_pipeline_buffer ubo[2][NN_PIPELINE_MAX_BINDINGS];
    /* Fresh per-submit private job. Hardware tables are wholly inside this BO,
     * disjoint from shader commands and each other. Never caller CPU tables. */
    nn_bo *job;size_t command_offset,sampler_offset,texture_offset;
    size_t storage_offset;
};
/* Fully validate before writing. out_words=0 on error, job untouched. Actual
 * immutable C1 overrides only its reflected native binding; conflicting input
 * is rejected. Full native code identity must match the owner's pair.
 * Writes a shader-only segment at job+command_offset, without cache publication
 * or submitting. A dynamic encoder must add restore/target/VFD/viewport/render/
 * draw and visibility/cache operations, include every descriptor's texture BO,
 * and publish the entire private job. No texture mapping is inferred here. */
int nn_pipeline_shader_encode(const nn_pipeline_program *,const struct nn_pipeline_shader_recipe *,
    const struct nn_pipeline_shader_bindings *,size_t capacity_words,size_t *out_words);
/* Unique required code/C1/push-UBO/private-job BOs, no texture-data discovery.
 * Capacity failure reports required count but leaves the list untouched. */
int nn_pipeline_shader_resources(const nn_pipeline_program *,const struct nn_pipeline_shader_recipe *,
    const struct nn_pipeline_shader_bindings *,nn_bo **bos,size_t capacity,size_t *count);
/* CPU-only structural/real UBO range validation. Code/C1 and descriptor-table
 * addresses are explicitly nonresident; no fake BO or synthetic GPUVA is used.
 * Actual encode retains address validation after their real allocations. */
int nn_pipeline_shader_preflight(const struct nn_offline_graphics_stage *const [2],
 const struct nn_pipeline_shader_recipe *,const struct nn_pipeline_buffer [2][16]);


int nn_pipeline_shader_preflight_depth(const struct nn_offline_graphics_stage *const [2],const struct nn_pipeline_depth_export *,const struct nn_pipeline_buffer [2][16]);
int nn_pipeline_shader_encode_depth(const nn_pipeline_program *,const struct nn_pipeline_depth_export *,const struct nn_pipeline_shader_bindings *,size_t,size_t *);
int nn_pipeline_shader_resources_depth(const nn_pipeline_program *,const struct nn_pipeline_depth_export *,const struct nn_pipeline_shader_bindings *,nn_bo **,size_t,size_t *);
#ifdef __cplusplus
}
#endif
