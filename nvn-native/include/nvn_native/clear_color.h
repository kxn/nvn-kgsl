/* SPDX-License-Identifier: MIT. GPU raw DWORD clear, not CPU image processing. */
#pragma once
#include "nvn_native/compute.h"
#include "nvn_native/texture_source.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_clear_color_pattern { uint32_t bytes_per_pixel,pattern[4],mask[4]; };
/* Packs ONE finite float RGBA texel with original Mesa format functions.
 * The returned16B pattern/mask repeat at the actual texel size. Unsupported
 * channels select no storage bits. No pixels/buffer are read or filled here.
 * Separate constant-metadata module: never part of pure FDL metadata ABI. */
__attribute__((visibility("default")))
int nn_clear_color_pattern(uint32_t format,const float rgba[4],uint32_t component_mask,
                          struct nn_clear_color_pattern *);
struct nn_clear_color_rect { int32_t x,y,width,height; };
struct nn_clear_color_args {
 uint32_t linear,base,pitch,block_height_log2;
 uint32_t x_bytes,y_rows,row_bytes,rows;
 uint32_t pattern[4],mask[4];
};
struct nn_clear_color_plan {
 struct nn_clear_color_args args;uint32_t groups[3],empty;
 uint64_t destination_bytes;
 struct nn_clear_color_rect requested;float rgba[4];uint32_t component_mask;
};
/* Validated 2D, one mip/layer, single-sample source metadata only. Rect is
 * clipped to the target. Whole destination DWORDs have exclusive ownership;
 * shader preserves unselected components, clipped pixels and row padding.
 * No render-target/sampler permission, native view or UBWC is inferred. */
int nn_clear_color_plan(const struct nn_tex_source_layout *,const struct nn_clear_color_rect *,
                        const float rgba[4],uint32_t component_mask,struct nn_clear_color_plan *);
struct nn_clear_color_range {nn_bo *bo;size_t owner_offset,owner_bytes,offset;};
struct nn_clear_color_binding {
 struct nn_clear_color_args args;uint32_t groups[3];
 struct nn_cs_buffer ubos[16],ssbos[16];nn_bo *resources[2];
};
/* Fresh, disjoint driver-private64B UBO plus true destination owner bounds.
 * Caller retains code, destination, uniform and encoded job until known
 * completion, repeats arena admission at submit, preserves unknown owners.
 * No allocation/cache/submit/owner mutation. Empty plans have no GPU bind. */
int nn_clear_color_bind(nn_device *,const struct nn_cs_image *,const struct nn_tex_source_layout *,
 const struct nn_clear_color_plan *,const struct nn_clear_color_range *destination,
 const struct nn_clear_color_range *uniform,struct nn_clear_color_binding *);
#ifdef __cplusplus
}
#endif
