/* SPDX-License-Identifier: MIT. No CPU depth image access or queue ownership. */
#pragma once
#include "nvn_native/depth_attachment.h"
#include "nvn_native/depth_a2d.h"
#include "nvn_native/pipeline_platform.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Pure shared platform contract validation; no allocation, map or GPU work. */
int nn_depth_transfer_platform_validate(nn_device *,const struct nn_pipeline_platform *);
struct nn_depth_readback {nn_bo *storage;struct nn_depth_a2d_view view;uint32_t ready;};
/* A fresh private bidirectional LINEAR staging alias BO (historical type name
 * retained for compatibility). UPLOAD reads it and READBACK writes it. The depth attachment remains borrowed and
 * must outlive this owner and all submissions. Never maps/initializes pixels. */
int nn_depth_readback_create(nn_device *,const struct nn_depth_attachment *,struct nn_depth_readback *);
int nn_depth_readback_validate(const struct nn_depth_attachment *,const struct nn_depth_readback *);
int nn_depth_readback_destroy(struct nn_depth_readback *);
/* Exact actual-FDL clear/readback/upload job, using the same verified device restore
 * and qualified ordering encoder as graphics. Separate command/data pages.
 * All preflight before allocation. On error *job may be nonNULL with bytes0;
 * quarantine/retry destroy. depth/readback/job must be submission refs and stay
 * alive until known Finish, including unknown completion. Never submits/maps
 * depth/readback pixels. CLEAR does not require/read staging; READBACK and UPLOAD require it.
 * No implicit CPU upload/readback or format conversion. */
int nn_depth_transfer_encode(nn_device *,const struct nn_depth_attachment *,const struct nn_depth_readback *,const struct nn_pipeline_platform *,uint32_t op,const struct nn_depth_a2d_region *,float clear_depth,nn_bo **job,size_t *bytes);
/* Borrowed exact submit references. Capacity errors leave array unchanged. */
int nn_depth_transfer_resources(const struct nn_depth_attachment *,const struct nn_depth_readback *,uint32_t op,nn_bo *job,nn_bo **out,size_t capacity,size_t *count);
#ifdef __cplusplus
}
#endif
