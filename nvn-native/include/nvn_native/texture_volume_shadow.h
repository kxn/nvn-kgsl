/* SPDX-License-Identifier: MIT. Pure GPU materialization plan/owner, no submit. */
#pragma once
#include "nvn_native/volume_copy.h"
#include "nvn_native/runtime_fdl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native sampled-only true3D RGBA8 UNORM, RGBA16F and BC1/2/3 UNORM. Canonical always remains the
 * original true3D BD/Z allocation; source.layers=1, not an array of slices.
 * Copy every mip into its actual FDL native offset/pitch/slice_stride. */
struct nn_volume_shadow_plan {
 struct nn_fdl_volume_desc descriptor;
 struct nn_fdl_volume_view native;
 struct nn_volume_copy_plan copy[NN_FDL_LEVELS];
 uint32_t count;
};
int nn_volume_shadow_plan(const struct nn_tex_source_layout *,uint32_t base_level,uint32_t level_count,
 struct nn_volume_shadow_plan *);
struct nn_volume_shadow {
 nn_bo *storage,*code,*uniform[NN_FDL_LEVELS],*dispatch[NN_FDL_LEVELS];
 size_t dispatch_bytes[NN_FDL_LEVELS];
 nn_bo *canonical;size_t canonical_offset,canonical_bytes;
 struct nn_fdl_volume_desc descriptor;
 struct nn_fdl_volume_view view;
 uint32_t count,ready;
};
struct nn_volume_shadow_dispatch {nn_bo *job;size_t bytes;};
/* Caller supplies a normally verified native-volume-block-copy-v1 image and
 * same-device live canonical owner (including descriptor prefix/ceil4 tail).
 * No source/native pixel map or CPU image write occurs. Fresh private native
 * storage is physically independent of live canonical pages by allocator
 * contract, never inferred merely from distinct caller BOs or GPUVAs.
 * Single mip:4 ownedBOs. Full chain:2+2*N (shared native/code, immutable UBO/IB
 * per mip). Out must be zero/unused; any failure can retain cleanup-failed BOs.
 * Neither create nor any accessor submits, waits, or retains the borrowed BO.
 * Caller holds canonical/core object pins and EVERY returned resource through
 * known Finish. Unknown submit/Finish MUST quarantine the whole owner even if
 * an idle query happens to return0; idle is not completion of an unknown ioctl.
 * RGBA16F transports raw eight-byte texels; no half decoding/quantization.
 * IMAGE source flags do not grant ImageLoad/ImageStore or a storage descriptor.
 * Existing shader/pipeline3D gates are unchanged by metadata success. */
int nn_volume_shadow_create(nn_device *,const struct nn_cs_image *,const struct nn_tex_source_layout *,
 const struct nn_volume_shadow_plan *,const struct nn_block_copy_range *,struct nn_volume_shadow *);
/* Preflight all owned BOs idle before releasing any; failed frees leave every
 * remaining handle reachable. Never destroys borrowed canonical. */
int nn_volume_shadow_destroy(struct nn_volume_shadow *);
/* No partial result on capacity/error; borrowed refs, no lifetime mutation. */
int nn_volume_shadow_resources(const struct nn_volume_shadow *,nn_bo **,size_t,size_t *);
int nn_volume_shadow_dispatches(const struct nn_volume_shadow *,struct nn_volume_shadow_dispatch *,size_t,size_t *);
/* Regenerate actual TYPE3D FDL descriptor at actual fresh allocation base. */
int nn_volume_shadow_descriptor(const struct nn_volume_shadow *,uint32_t out[16]);
#ifdef __cplusplus
}
#endif
