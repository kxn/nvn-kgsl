/* SPDX-License-Identifier: MIT. Normal output sidecars, exact source variant. */
#pragma once
#include "nvn_native/pipeline_archive.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Parallel to archive->pairs, exact recipe pointer identity. Every v2 pair
 * requires its real compiler depth sidecar. A v1 pair may carry a verified
 * writes0/reg252 sidecar for ordinary raster depth, independently of textures.
 * All metadata is immutable for Device/Program/handle/submission lifetime. */
struct nn_pipeline_depth_archive {
 uint32_t version;const struct nn_pipeline_typed_archive *typed;
 const struct nn_pipeline_depth_export *const *pairs;size_t pair_count;
};
int nn_pipeline_depth_archive_validate(const struct nn_pipeline_depth_archive *);
int nn_pipeline_select_depth_export(const struct nn_pipeline_depth_archive *,
 const struct nn_offline_graphics_stage *,const struct nn_offline_graphics_stage *,
 const struct nn_pipeline_shader_recipe *,const struct nn_pipeline_depth_export **);
#ifdef __cplusplus
}
#endif
