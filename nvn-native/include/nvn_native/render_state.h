/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native semantic values, copied by value at Bind and each Draw. No guest
 * state pointer, object identity, allocation or BO ownership is retained. */
#define NN_RENDER_STATE_VERSION 1u
struct nn_render_blend { uint32_t src_rgb,dst_rgb,src_alpha,dst_alpha,equation_rgb,equation_alpha; };
struct nn_render_depth_stencil {
    uint32_t depth_test,depth_write,depth_func,stencil_test;
    uint32_t stencil_func[2],stencil_fail[2],stencil_depth_fail[2],stencil_depth_pass[2];
};
struct nn_render_multisample {
    uint32_t enable,samples,alpha_to_coverage,alpha_to_coverage_dither;
    uint32_t raster_samples,coverage_modulation,coverage_to_color,coverage_output;
    uint32_t locations_enable,locations_grid_enable;
    uint8_t sample_locations[16]; /* packed XY, copied at Bind; GPU enable still bounded */
};
struct nn_render_state {
    uint32_t version;
    struct nn_render_blend blend[8];
    uint32_t blend_enable,color_mask,logic_op,alpha_func;
    struct nn_render_depth_stencil depth;
    struct nn_render_multisample multisample;
    uint32_t cull_face,front_face,polygon_mode,polygon_offset_enables;
    float blend_color[4];
    uint32_t depth_bounds_enable; float depth_bounds_min,depth_bounds_max;
    float polygon_offset_factor,polygon_offset_units,polygon_offset_clamp;
    uint32_t stencil_value_mask[2],stencil_write_mask[2],stencil_ref[2];
    uint32_t patch_size;
};
struct nn_render_target_info {
    uint32_t chip,window_origin,color_targets,depth_attachment,samples;
};
/* Pure-state scalar defaults independently checked against SDK53.314 setters
 * and getters; these are semantic values, not an SDK object overlay. Dynamic command-state defaults
 * remain explicit backend policy: zero blendcolor/offset/ref, full stencil
 * masks, disabled bounds0..1 and patchsize3. This does not claim the original
 * SDK initializes an unbound command buffer to these dynamic values. */
void nn_render_state_defaults(struct nn_render_state *state);
/* Validation has no side effects. Unknown target/profile/enabled unsupported
 * features return ENOTSUP. Invalid semantic values return EINVAL. */
int nn_render_state_validate(const struct nn_render_state *state,
                             const struct nn_render_target_info *target);
/* Emit a bounded register-only A650 state segment after template fixed state
 * and before the Draw. No pointers/addresses/GPU ops are encoded here. out_words
 * is0 on failure and destination remains untouched. NULL words/capacity0 queries
 * required capacity after successful validation. Origin/depth viewport patches
 * remain the draw encoder's responsibility. */
int nn_render_state_encode(const struct nn_render_state *state,
                           const struct nn_render_target_info *target,
                           uint32_t *words,size_t capacity_words,size_t *out_words);
/* Common raster state for a separately validated native depth attachment.
 * Depth module has already checked/consumed depth controls; depth_test must
 * be neutralized here. Actual target info is depth_attachment1/color0 or1.
 * With zero color, mask/blend/logic/destination-read are disabled. No depth
 * address/test state or target allocation is inferred/emitted by this API. */
int nn_render_state_encode_depth_raster(const struct nn_render_state *,
 const struct nn_render_target_info *,uint32_t *,size_t,size_t *);
#ifdef __cplusplus
}
#endif
