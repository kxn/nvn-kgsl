/* SPDX-License-Identifier: MIT. Explicit native SPIR-V artifact, never a fake Maxwell envelope. */
#pragma once
#include "pipeline_program.h"
#include "pipeline_texture_types.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_pipeline_native_artifact {
 uint32_t version;
 const struct nn_pipeline_shader_recipe *recipe;
 const uint32_t *spirv[2];size_t spirv_bytes[2];
 const struct nn_pipeline_texture_types *textures;
};
/* Normal standalone compiler outputs only, immutable and provenance-checked by
 * caller. SPIR-V is identity/metadata, never executed or interpreted here.
 * No control blob, Maxwell code, C1 or NVN source matching is invented.
 * Push UBO ranges are actual recipe bindings; no guest-CB mapping is inferred.
 * Code upload/retire uses the same owner/backend as original source programs.
 * Error may retain an owner on failed rollback, exactly as the original API. */
const struct nn_pipeline_texture_types *nn_pipeline_program_texture_types(const nn_pipeline_program *);
int nn_pipeline_program_create_native(nn_device *,const struct nn_pipeline_native_artifact *,
 nn_pipeline_program **,nn_pipeline_program **retained);

/* Default-OFF raw-native v2 admission, exact normal compiler sidecar. */
struct nn_pipeline_depth_export;
int nn_pipeline_program_create_native_depth(nn_device *,const struct nn_pipeline_native_artifact *,const struct nn_pipeline_depth_export *,nn_pipeline_program **,nn_pipeline_program **retained);
const struct nn_pipeline_depth_export *nn_pipeline_program_depth_export(const nn_pipeline_program *);
/* Explicit default-OFF storage role, independent from optional real depth.
 * Legacy constructors do not acquire an unvalidated storage sidecar. */
const struct nn_pipeline_storage_types *nn_pipeline_program_storage_types(const nn_pipeline_program *);
int nn_pipeline_program_create_native_storage(nn_device *,const struct nn_pipeline_native_artifact *,
 const struct nn_pipeline_depth_export *,const struct nn_pipeline_storage_types *,nn_pipeline_program **,nn_pipeline_program **retained);
#ifdef __cplusplus
}
#endif
