/* SPDX-License-Identifier: MIT */
#pragma once
#include "nvn_native/nvn.h"
#include "ahb.h"
struct nn_ahb_image_pool_report {
    int transferred, resources_retained, fd_index;
    size_t pixel_offset, fd_bytes;
    nn_ahb *borrowed_owner; /* success only: never destroy; lock only after GPU/display release */
    nn_ahb *retained_owner; /* only on uncertain pre-transfer cleanup; keep alive */
};
/* New, explicit host-only pool key. Does not change NVN pool-builder semantics
 * or rebind an initialized ordinary pool/texture. No CPU pixel copy or GPU work.
 * On success the core pool owns the AHB; on error report identifies retention. */
int nn_nvn_ahb_image_pool_create(NVNdevice *, NVNmemoryPool *, struct nn_ahb_image_pool_report *);
