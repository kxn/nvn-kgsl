/* SPDX-License-Identifier: MIT. Actual FDL sampled-only true3D binding. */
#pragma once
#include "runtime_fdl.h"
#include "kgsl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Exact address-zero FDL volume metadata, six owner formats. All mip/view
 * fields are regenerated, never inferred from a 2D descriptor or a row pitch.
 * First native consumer is TXF/TXS only; no sampler, filtering, storage image,
 * compressed decode on CPU, RT or guest resource resolution is admitted. */
int nn_volume_sampled_validate(const struct nn_fdl_volume_view *);
int nn_volume_sampled_descriptor(const struct nn_fdl_volume_view *,nn_bo *,size_t,size_t,uint32_t out[16]);
#ifdef __cplusplus
}
#endif
