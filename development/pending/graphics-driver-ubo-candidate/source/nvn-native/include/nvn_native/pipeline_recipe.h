/* SPDX-License-Identifier: MIT. Shader-dependent A650 state, no render target or draw. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define NN_PIPELINE_RECIPE_VERSION 1u
#define NN_PIPELINE_MAX_VERTEX_INPUTS 32u
#define NN_PIPELINE_MAX_BINDINGS 16u
/* Native compiler bindings, NEVER NVN API slots or descriptor pool IDs. */
enum nn_pipeline_patch_role { NN_PIPELINE_CODE,NN_PIPELINE_UBO,
    NN_PIPELINE_SAMPLERS,NN_PIPELINE_TEXTURES,NN_PIPELINE_STORAGE,
    NN_PIPELINE_DRIVER_PARAMS,NN_PIPELINE_DYNAMIC_UBO };
struct nn_pipeline_address_patch {
    uint32_t word,role,stage,binding,offset,bytes;
};
struct nn_pipeline_vertex_input { uint32_t location,regid,compmask; };
struct nn_pipeline_shader_recipe {
    uint32_t version,chip,code_bytes[2];
    const uint32_t *code[2]; /* exact native identity, not size-only */
    const uint32_t *words;size_t word_count;
    const struct nn_pipeline_address_patch *patches;size_t patch_count;
    const struct nn_pipeline_vertex_input *inputs;size_t input_count;
    uint32_t vertex_id_regid,instance_id_regid;
    uint32_t texture_mask,sampler_mask,texture_table_entries;
    uint32_t fragcoord_mask,has_kill;
};

/* Append-only independent contract. A writes-FragDepth recipe uses version2,
 * so existing color-only consumers must reject its bare recipe. The sidecar
 * is emitted by that same normal IR3 invocation and binds that exact recipe. */
#define NN_PIPELINE_DEPTH_RECIPE_VERSION 2u
struct nn_pipeline_depth_export {
 uint32_t version;
 const struct nn_pipeline_shader_recipe *recipe;
 uint32_t writes_depth,depth_regid,color_outputs,late_z_required,lrz_disabled_required;
};

/* Actual common-NIR storage facts. Logical SPIR-V bindings remain 0..15.
 * The shared Adreno graphics IBO table gives VS slots0..15 and FS16..31,
 * so identical logical bindings in different stages never alias by accident.
 * Unknown dynamic resource selection, atomics and non32bit accesses reject.
 * This sidecar binds exact normal recipe/SPIR-V identities, not NVN slots. */
struct nn_pipeline_storage_types {
 uint32_t version;
 const struct nn_pipeline_shader_recipe *recipe;
 const uint32_t *spirv[2];size_t spirv_bytes[2];
 uint32_t mask[2],operations[2][16]; /* READ1 / WRITE2 / SIZE4 */
};

/* Actual lowered common-IR3 constant requirements. DWORD mask identifies the
 * consumed words, independently from padded upload allocation. UBO mask is
 * residual native binding space after actual constant-range pushing, never
 * an NVN API slot mask. This layout alone grants no runtime admission. */
struct nn_pipeline_runtime_layout {
 uint32_t version;
 const struct nn_pipeline_shader_recipe *recipe;
 uint32_t driver_dwords[2],driver_mask[2],driver_offset_vec4[2],driver_upload_vec4[2];
 uint32_t ubo_mask[2],ubo_entries[2];
};
struct nn_pipeline_runtime_constants {
 uint32_t version;
 const struct nn_pipeline_runtime_layout *layout;
 const uint32_t *spirv[2];size_t spirv_bytes[2];
};
