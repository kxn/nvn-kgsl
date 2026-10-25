/* SPDX-License-Identifier: MIT. NVN source namespaces to explicit native bindings. */
#pragma once
#include "nvn_native/pipeline_archive.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Original SDK53.314 BindUniformBuffer(s): API0..13 selects sourceCB3..16.
 * Native compiler binding is obtained independently from stage.cbufs. CB1 is
 * immutable original C1 owned by nn_pipeline_program. CB0/2 are never API UBOs.
 * This routine takes already validated same-device/policy-approved BO ranges;
 * it does not establish NVN object refs or pool identity/lifetime. Only actual
 * consumed native UBO patches need a bound API range. Failure zeros output.
 * No support-buffer provider is implied: binding0 not reflected in cbufs is
 * ENOTSUP, rather than treating it as original CB0 or assigning guessed bytes. */
int nn_pipeline_frontend_uniforms(const struct nn_offline_graphics_stage *,
 const struct nn_pipeline_shader_recipe *,const struct nn_pipeline_buffer api[14],
 struct nn_pipeline_buffer native[16]);
/* Typed depth mapping retains the same sourceCB/API/native namespaces; a
 * real exact sidecar must prove v2 (or v1 writes0), no bare version bypass. */
int nn_pipeline_frontend_uniforms_depth(const struct nn_offline_graphics_stage *,
 const struct nn_pipeline_depth_export *,const struct nn_pipeline_buffer api[14],
 struct nn_pipeline_buffer native[16]);
/* Explicit default-off whole-family policies, not physical A650 capabilities.
 * Values describe the shader IR contract selected for this backend. The caller
 * supplies actual frontend state separately; an unknown name/shape fails.
 * Texture queries are deliberately not answered here without runtime resources. */
struct nn_pipeline_frontend_queries {
 uint32_t version,chip,primitive,depth_mode,window_origin;
 struct nn_render_state render;
 uint32_t attribute_types[32],fragment_output_types[8];
 uint32_t early_z_force,viewport_transform_disable,program_point_size_enable;
 float point_size,alpha_test_reference;
 uint32_t transform_feedback,draw_parameters,dual_source_blend,y_negate,halve_primitive_id;
};
/* No implied NVN dynamic defaults. Caller must have recorded each field from
 * explicit Device/Command state or an independently declared backend policy. */
int nn_pipeline_frontend_query(void *,uint32_t stage,const struct nn_pipeline_query *,
 uint32_t values[32],size_t *count);
#ifdef __cplusplus
}
#endif
