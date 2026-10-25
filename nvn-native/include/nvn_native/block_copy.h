/* SPDX-License-Identifier: MIT. Raw canonical GOB <-> linear block bytes. */
#pragma once
#include "nvn_native/compute.h"
#include "nvn_native/texture_source.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_block_copy_region {
 uint32_t level,base_layer,layers,x_bytes,y_rows,row_bytes,rows;
 uint32_t linear_row_stride,linear_image_stride,direction;
};
struct nn_block_copy_args {
 uint32_t canonical_base,canonical_layer_stride,width_gobs,block_height_log2;
 uint32_t region_x_bytes,region_y_rows,row_bytes,rows;
 uint32_t linear_row_stride,linear_image_stride,layers,direction;
 uint32_t linear_span,linear_words,dispatch_width,reserved;
};
struct nn_block_copy_plan {
 struct nn_block_copy_args args;uint32_t groups[3];
 uint64_t canonical_bytes,linear_bytes;struct nn_block_copy_region region;
};
/* Exact raw block region, not an NVN pixel-region/flags interpreter. Existing
 * validated GOB source formats only, positive BYTE strides (may be odd),
 * rowStride>=rowBytes, imageStride>=last active row end. Return padded full
 * DWORD windows. Original source/canonical storage, mip/layer layout unchanged.
 * dir0=GOB->linear: one GLOBAL linear destinationDWORD per invocation.
 * dir1=linear->GOB: one aligned canonical destinationDWORD per row/layer.
 * No CPU bytes copied or format decoding. Failure zeroes output. */
int nn_block_copy_plan(const struct nn_tex_source_layout *,const struct nn_block_copy_region *,struct nn_block_copy_plan *);
struct nn_block_copy_range {nn_bo *bo;size_t owner_offset,owner_bytes,offset;};
enum nn_block_copy_alias {NN_BLOCK_COPY_ALIAS_UNKNOWN,NN_BLOCK_COPY_DISJOINT_PROVEN};
struct nn_block_copy_binding {
 struct nn_block_copy_args args;uint32_t groups[3];
 struct nn_cs_buffer ubos[16],ssbos[16];nn_bo *resources[3];
};
/* All ranges are relative to their ORIGINAL same-device BOs. TRUE owner spans
 * must include aligned-down descriptor prefix and ceil4 tails. Caller must
 * actually prove physical src/dst nonalias before passing DISJOINT_PROVEN;
 * this internal attestation is NOT a verifier or a guest-supplied permission.
 * Distinct BO/GPUVA is never evidence. UNKNOWN explicitly rejects. Same-BO
 * windows are accepted when the caller has proved backing ownership and their
 * complete actual DWORD-access spans do not overlap (not just active bytes).
 * Uniform must be fresh driver-private and disjoint, cover64B. Caller writes
 * returned args there, caches metadata, uses normal nn_cs_encode with actual
 * code BO/image, submits returned refs + code/job, preserves all owners on
 * unknown completion and waits before reads/reuse/free. Also prove exclusive
 * access to whole destinationDWORDs, including masked neighbor bytes; logical
 * pixel ranges alone are insufficient for concurrent read/modify/write jobs.
 * No allocation/map/cache/submit/owner mutation here; errors zero output. */
int nn_block_copy_bind(nn_device *,const struct nn_cs_image *,const struct nn_tex_source_layout *,const struct nn_block_copy_plan *,
 const struct nn_block_copy_range *canonical,const struct nn_block_copy_range *linear,
 const struct nn_block_copy_range *uniform,enum nn_block_copy_alias,struct nn_block_copy_binding *);
#ifdef __cplusplus
}
#endif
