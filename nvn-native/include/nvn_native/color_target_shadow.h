/* SPDX-License-Identifier: MIT. Private canonical/FDL render-target version. */
#pragma once
#include "color_target.h"
#include "block_copy.h"
#include "texture_bridge.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_color_target_shadow_plan {
 struct nn_tex_native_view native;
 struct nn_color_target_info info;
 struct nn_block_copy_plan copy[2];
};
/* Real same-format uncompressed float-output color target: 2D/mip1/layer1,
 * canonical Generic16Bx2 source and actual native FDL zero-GPUVA view. No
 * sampling, Z/S, integer output, format conversion, or public storage rebind.
 * Includes input preservation and GPU post-draw writeback plans. */
int nn_color_target_shadow_plan(const struct nn_tex_source_layout *,const struct nn_tex_native_view *,struct nn_color_target_shadow_plan *);
/* Original canonical owner spans must include the complete aligned-down SSBO
 * prefix. Fresh private native/code/uniform/dispatch BOs prove disjointness.
 * No pixel map or submit; all five returned owners must survive unknown
 * completion. Caller refs canonical+all5 and uses existing immutable chain.
 * Error may retain cleanup-failed BOs in out: never discard them. */
int nn_color_target_shadow_create(nn_device *,const struct nn_cs_image *,const struct nn_tex_source_layout *,const struct nn_color_target_shadow_plan *,const struct nn_block_copy_range *,struct nn_tex_shadow *);
#ifdef __cplusplus
}
#endif
