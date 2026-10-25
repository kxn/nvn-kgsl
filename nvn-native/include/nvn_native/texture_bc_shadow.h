/* SPDX-License-Identifier: MIT. GPU raw-block -> actual FDL compressed shadow. */
#pragma once
#include "nvn_native/block_copy.h"
#include "nvn_native/runtime_fdl.h"
#include "nvn_native/sampler_state.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Metadata from original FDL, never a copied RGBA8 descriptor/pitch formula.
 * Source is unchanged Generic16Bx2 BC1 RGB/RGBA, BC2, BC3 UNORM/sRGB, 2D/array.
 * All mips/layers are uploaded; the sampled subview is independently selected.
 * The native allocation is read-only sampled storage, never an MRT. */
struct nn_bc_shadow_plan {
 struct nn_fdl_image_desc descriptor;
 struct nn_fdl_image_view native;
 struct nn_block_copy_plan copy[NN_FDL_LEVELS];
 uint32_t count;
};
int nn_bc_shadow_plan(const struct nn_tex_source_layout *,uint32_t base_level,uint32_t level_count,
 uint32_t base_layer,uint32_t layer_count,struct nn_bc_shadow_plan *);
/* Mutable arrays only exist for ownership/rollback. Caller submits EVERY BO
 * returned by resources(), plus borrowed canonical and the eventual graphics
 * job; preserves all owners until Finish, including unknown completion.
 * Neither create nor destroy submits or waits. No CPU canonical/native bytes
 * are mapped. Only code, uniforms, command metadata are CPU-written.
 * create requires zero-initialized unused out; on ANY error it may retain BOs
 * whose destruction failed. Retry destroy; never free this state prematurely.
 * Canonical is borrowed, with a true live allocation window supplied by caller.
 * Fresh nn_bo_alloc storage is driver-private and disjoint from live canonical.
 */
struct nn_bc_shadow {
 nn_bo *storage,*code,*uniform[NN_FDL_LEVELS],*dispatch[NN_FDL_LEVELS];
 size_t dispatch_bytes[NN_FDL_LEVELS];
 nn_bo *canonical;size_t canonical_offset,canonical_bytes;
 struct nn_fdl_image_view view;
 uint32_t count,ready;
};
int nn_bc_shadow_create(nn_device *,const struct nn_cs_image *,const struct nn_tex_source_layout *,
 const struct nn_bc_shadow_plan *,const struct nn_block_copy_range *canonical,struct nn_bc_shadow *);
int nn_bc_shadow_destroy(struct nn_bc_shadow *);
/* No partial result on capacity failure. Returns borrowed resource pointers,
 * no refcount changes; caller's native submission ledger owns lifetime. */
int nn_bc_shadow_resources(const struct nn_bc_shadow *,nn_bo **,size_t capacity,size_t *count);
/* Actual FDL native image descriptor at the actual fresh storage GPUVA.
 * Caller does not patch bitfields, choose alpha swizzles, or guess view offsets.
 * Owner.view is address-zero metadata until successfully created. */
int nn_bc_shadow_descriptor(const struct nn_bc_shadow *,uint32_t out[16]);
/* Finite first sampled pipeline scope: BC1/2/3 UNORM/sRGB, 2D, a selected contiguous
 * mip range, one layer, TXF/TXS or explicit nearest/clamp2D sampling. Storage may contain a full mip chain.
 * Input is exact ADDRESS-ZERO actual FDL metadata; regeneration checks every
 * returned field. No raw guest descriptor is accepted. Other sampled contracts
 * (linear/mip filtering, arrays, render targets) remain unsupported. */
int nn_bc_sampled_validate(const struct nn_fdl_image_view *);
int nn_bc_sampled_descriptor(const struct nn_fdl_image_view *,nn_bo *,size_t offset,size_t bytes,uint32_t out[16]);
int nn_bc_sampled_sampler(const struct nn_fdl_image_view *,const struct nn_sampler_state *,
 const struct nn_sampler_layout *,uint32_t out[4]);
#ifdef __cplusplus
}
#endif
