/* SPDX-License-Identifier: MIT. Private native depth storage ownership only. */
#pragma once
#include "depth_fdl.h"
#include "kgsl.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_depth_attachment {
 nn_bo *storage;
 struct nn_depth_fdl_view view;
 uint32_t ready;
};
/* Zero unused out required. Preflight metadata before allocation. The descriptor
 * address must be0; storage is fresh driver-private memory. No CPU pixel map,
 * copy/conversion, submit/wait, canonical source or depth-render capability is
 * provided by this owner. On any error storage may remain nonNULL if cleanup
 * failed; caller quarantines this owner+device and retries destroy safely. */
int nn_depth_attachment_create(nn_device *,const struct nn_depth_fdl_desc *,struct nn_depth_attachment *);
int nn_depth_attachment_destroy(struct nn_depth_attachment *);
int nn_depth_attachment_validate(const struct nn_depth_attachment *);
/* Capacity failure leaves the pointer array untouched/count0. Borrowed pointer
 * only; real queue refs and owner outlive Finish, including unknown completion. */
int nn_depth_attachment_resources(const struct nn_depth_attachment *,nn_bo **,size_t capacity,size_t *count);
int nn_depth_attachment_descriptor(const struct nn_depth_attachment *,uint32_t out[16]);
#ifdef __cplusplus
}
#endif
