/* SPDX-License-Identifier: MIT. Native depth attachment state, no submit. */
#pragma once
#include "depth_attachment.h"
#include "pipeline_recipe.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Independent version2 contract: exact shader recipe sidecar + corresponding
 * canonical packet-boundary SP/RB depth export words. No code/recipe mutation. */
int nn_depth_native_recipe(const struct nn_pipeline_depth_export *);
/* Pure planned attachment: actual regenerated FDL view with address0 only.
 * No fake BO, allocation or dropped alias check; resident encoding still
 * requires a validated attachment. Returns exact future depth word count. */
int nn_depth_native_plan(const struct nn_depth_fdl_view *,const struct nn_pipeline_depth_export *,
 const struct nn_depth_controls_desc *,size_t *count);
/* Intended only after complete system-memory platform+shader+render state and
 * immediately before its corresponding Draw. Caller independently validates
 * matching framebuffer dimensions/scissor, COLOR/other attachments, aliases,
 * shader resources, all BO refs and full flush/retirement ordering. A650 only,
 * no stencil/UBWC/MSAA/LRZ/earlyZ/bounds. It does not make an old pipeline's
 * depth gate pass or claim a GPU execution. No BO allocation/map/cache/submit.
 * wordsNULL/capacity0 queries length; other errors count0 and untouched words. */
int nn_depth_native_words(const struct nn_depth_attachment *,
 const struct nn_pipeline_depth_export *,const struct nn_depth_controls_desc *,
 uint32_t *words,size_t capacity,size_t *count);
struct nn_render_state;
int nn_depth_native_render(const struct nn_render_state *,const struct nn_depth_controls_desc *,struct nn_render_state *);
#ifdef __cplusplus
}
#endif
