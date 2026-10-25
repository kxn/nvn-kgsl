/* SPDX-License-Identifier: MIT */
#pragma once
#include "kgsl.h"
#include "graphics_image.h"
#include "render_state.h"
#include "sampler_state.h"

#ifdef __cplusplus
extern "C" {
#endif
struct nn_gfx_buffer { nn_bo *bo; size_t offset, bytes; };
enum nn_gfx_draw_mode { NN_GFX_DRAW_DIRECT, NN_GFX_DRAW_INDIRECT, NN_GFX_DRAW_INDIRECT_COUNT };
/* Values are copied per draw. GPU argument buffers remain live and are read
 * at execution, including replay; index_type is 0/1/2 for u8/u16/u32. */
struct nn_gfx_draw {
    uint32_t mode, primitive, indexed, index_type;
    uint32_t first, count, instances, base_instance;
    int32_t base_vertex;
    uint32_t max_draws, stride;
    struct nn_gfx_buffer index, indirect, indirect_count;
};
/* Encode one immutable direct-render IB. All supplied BOs must be retained by
 * the caller and included in the submission's resource list. ZERO storage and
 * texture/sampler descriptor tables are allocated within the returned job;
 * callers must not bind these internal slots. A schema-2 sampled profile also
 * requires FS_UNIFORM and TEXTURE BOs; unused slots must remain empty. */
int nn_gfx_encode(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  nn_bo **job, size_t *command_bytes);
/* Schema3 requires an explicit independent draw snapshot. Legacy raw schemas
 * are accepted only by nn_gfx_encode; passing them here fails closed. Snapshot
 * must be copied at recording time, including the late textured-draw path. */
int nn_gfx_encode_state(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state,
                  nn_bo **job, size_t *command_bytes);
int nn_gfx_encode_draw(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state, const struct nn_gfx_draw *draw,
                  nn_bo **job, size_t *command_bytes);
/* Runtime render snapshots are separate from the compiler artifact schema.
 * These APIs insert fixed-function state after the template's setup and
 * immediately before its draw packet. The legacy APIs keep their raw state. */
int nn_gfx_encode_render(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state, const struct nn_render_state *render,
                  nn_bo **job, size_t *command_bytes);
int nn_gfx_encode_draw_render(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state, const struct nn_gfx_draw *draw,
                  const struct nn_render_state *render, nn_bo **job, size_t *command_bytes);
/* Explicit runtime sampler descriptor override, independent of compiler image
 * schema. Sampled schema3/A650 only; optional draw preserves Draw API variants.
 * Caller holds normal BO resources; copied sampler state needs no GPU ref. */
int nn_gfx_encode_sampled_render(nn_device *device,const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state,const struct nn_gfx_draw *draw,
                  const struct nn_render_state *render,const struct nn_sampler_state *sampler,
                  nn_bo **job,size_t *command_bytes);
#ifdef __cplusplus
}
#endif
