/* SPDX-License-Identifier: MIT */
#pragma once
#include "kgsl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Host-only allocation descriptor, not an NVN pool-builder flag or native
 * handle ABI. A650 RGBA8 LINEAR/layer1/usage819. Shape validation is not
 * GPU/display qualification; new shapes require independent actual evidence. */
enum { NN_EXTERNAL_RGBA8 = 1 };
typedef struct nn_external_image nn_external_image;
struct nn_external_image_desc {
    int fd; /* borrowed only during import; allocator keeps its original fd */
    size_t fd_bytes, pixel_offset, payload_bytes; /* payload = complete FDL layer extent */
    uint32_t width, height, pitch, layers, format, usage;
    void *display_buffer, *owner_cookie;
    /* Success releases owner; any positive error MUST preserve the owner.
     * Failure is quarantined and this callback is never retried. */
    int (*release_owner)(void *);
};
struct nn_external_image_view {
    nn_bo *bo;
    size_t fd_bytes, pixel_offset, payload_bytes;
    uint32_t width, height, pitch, format;
    uint64_t backing_inode; /* actual verified pixel-FD fstat, never caller identity */
    uint32_t backing_dev_major, backing_dev_minor;
    void *display_buffer; /* borrowed until pool/owner retirement */
};
/* Ownership transfers only on success. On failure *out=NULL and the caller
 * still owns the allocator. This function submits no GPU work. */
int nn_external_image_import(nn_device *, const struct nn_external_image_desc *, nn_external_image **out);
int nn_external_image_view(nn_external_image *, struct nn_external_image_view *out);
int nn_external_image_live(const nn_external_image *);
/* Exactly one complete-payload texture claim, no subviews/aliases. */
int nn_external_image_claim_texture(nn_external_image *, size_t offset, size_t bytes);
int nn_external_image_release_texture(nn_external_image *);
/* Optional display/window pins. Caller must detach/finish before unpin.
 * Pins are bookkeeping, never proof that a GPU or release fence completed. */
int nn_external_image_pin(nn_external_image *);
int nn_external_image_unpin(nn_external_image *);
/* Refuses active claims/pins. EBUSY from BO busy check is retryable after a
 * proven GPU Finish; every other destruction error permanently quarantines.
 * BO is destroyed before allocator. No implicit CPU map/cache operation. */
int nn_external_image_destroy(nn_external_image *);
#ifdef __cplusplus
}
#endif
