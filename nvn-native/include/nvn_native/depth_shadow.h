/* SPDX-License-Identifier: MIT. GPU-only canonical depth layout transport. */
#pragma once
#include "nvn_native/depth_transfer.h"
#include "nvn_native/block_copy.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NN_DEPTH_SHADOW_RESOURCES 10u
struct nn_depth_shadow_plan {
 struct nn_depth_fdl_desc attachment;
 struct nn_depth_a2d_view aliases; /* address-zero exact FDL, not a binding */
 struct nn_block_copy_plan copy[2]; /* whole selected view: untile, retile */
};
/* Canonical Generic16Bx2 D16/D24X8/D32F, 2D or array; one selected mip/layer.
 * No depth/stencil format reinterpretation, LINEAR source or 3D admission. */
int nn_depth_shadow_plan(const struct nn_tex_source_layout *,uint32_t level,uint32_t layer,struct nn_depth_shadow_plan *);
struct nn_depth_shadow_step {nn_bo *job;size_t bytes;};
struct nn_depth_shadow {
 struct nn_depth_attachment attachment;
 struct nn_depth_readback linear;
 nn_bo *code,*uniform[2];
 struct nn_depth_shadow_step copy[2],a2d[2];
 struct nn_block_copy_range canonical; /* borrowed, never freed or CPU-mapped */
 struct nn_depth_shadow_plan plan;
 uint32_t ready;
};
/* Unused zero owner required. Caller supplies a TRUE canonical live window
 * and available submission-ledger capacity >=10. Preflight before allocating;
 * normal verified block-copy image/platform must remain immutable. Nine fresh
 * private BOs remain driver-owned; canonical/source allocation is unchanged.
 * Only code/UBO/command metadata is CPU-written, never any image payload.
 * On failure owner may retain BOs; quarantine/retry destroy even if !ready.
 * No submissions, waits, automatic retirement, or public NVN state changes. */
int nn_depth_shadow_create(nn_device *,const struct nn_cs_image *,const struct nn_pipeline_platform *,
 const struct nn_tex_source_layout *,const struct nn_depth_shadow_plan *,
 const struct nn_block_copy_range *canonical,size_t resource_capacity,struct nn_depth_shadow *);
/* Busy preflight of EVERY owned BO before mutating owner, including !ready
 * failed owners. Other destruction failure stops with remaining handles live. */
int nn_depth_shadow_destroy(struct nn_depth_shadow *);
/* Caller submits load[0],load[1], its own actual depth Draw, store[0],store[1]
 * IN ORDER, all with the complete ten-resource ledger plus its Draw resources.
 * Existing raw-block full cache and A2D full CCU ordering are reused. A single
 * owner cannot be used concurrently; finish before replay/CPU canonical write.
 * Unknown submission/completion retains the entire owner+canonical+Draw. */
int nn_depth_shadow_sequence(const struct nn_depth_shadow *,struct nn_depth_shadow_step load[2],struct nn_depth_shadow_step store[2]);
/* Borrowed ten resources, no ref changes. Capacity failure leaves out intact. */
int nn_depth_shadow_resources(const struct nn_depth_shadow *,nn_bo **out,size_t capacity,size_t *count);
#ifdef __cplusplus
}
#endif
