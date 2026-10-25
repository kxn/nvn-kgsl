/* SPDX-License-Identifier: MIT */
#ifndef NN_ANDROID_AHB_H
#define NN_ANDROID_AHB_H
#include <android/hardware_buffer.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nn_ahb nn_ahb;
#define NN_AHB_USAGE (AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN | \
                      AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE)
enum { NN_AHB_MAX_FDS = 8, NN_AHB_MAX_INTS = 1024, NN_AHB_LINK_BYTES = 128 };
struct nn_ahb_fd_info {
    int fd, stat_errno, link_errno;
    int64_t bytes;
    uint32_t mode;
    uint64_t inode;
    uint32_t device_major, device_minor;
    char link[NN_AHB_LINK_BYTES];
};
struct nn_ahb_mapping {
    int attempted, found, shared, matches, fd_index;
    uint64_t offset, vma_file_offset, vma_bytes, payload_bytes;
    char permissions[5];
};
struct nn_ahb_view {
    void *address;
    int32_t bytes_per_pixel, bytes_per_stride;
    struct nn_ahb_mapping mapping;
};
struct nn_ahb_pixel {
    int fd, fd_index; /* borrowed from the live AHB; never close this fd */
    uint64_t offset, bytes; /* bytes is the whole backing fd's fstat size */
    uint32_t pitch, width, height;
    uint64_t payload_bytes; /* FDL layer extent; visible rows may be smaller */
};

/* Positive errno returns; create sets *out=NULL on failure. The old wrapper
 * requests64x64. create_rgba requests explicit RGBA8/layer1/usage819. Actual
 * gralloc pitch and FDL extent are verified; new sizes need GPU qualification. Single owner,
 * no concurrent calls. No KGSL operation is performed by this helper. */
int nn_ahb_create(nn_ahb **out);
int nn_ahb_create_rgba(uint32_t width, uint32_t height, nn_ahb **out);
/* Lock accepts only CPU usage. A successful view is valid until unlock.
 * Failure balances an acquired lock internally; uncertain unlock poisons the
 * owner and destroy returns EBUSY. All successful locks requalify the same
 * unique shared-VMA fd identity and offset. Mapping rejection disables further
 * locks/get_pixel with ENOTSUP but permits destroy after safe unlock.
 * No GPU submit while CPU-locked. */
int nn_ahb_lock(nn_ahb *owner, uint64_t cpu_usage, struct nn_ahb_view *out);
int nn_ahb_unlock(nn_ahb *owner);
/* EBUSY preserves a locked/uncertain owner until process teardown; never retry
 * an uncertain unlock. Caller separately tracks GPU/display ownership: finish
 * the queue and release compositor refs before destroy or another CPU lock. */
int nn_ahb_destroy(nn_ahb *owner);
const AHardwareBuffer_Desc *nn_ahb_description(const nn_ahb *owner);
AHardwareBuffer *nn_ahb_buffer(const nn_ahb *owner); /* borrowed, do not release */
/* Requires at least one successful lock/unlock and no current lock/poison.
 * Returns EAGAIN before identification, EBUSY while CPU-locked/uncertain. */
int nn_ahb_get_pixel(const nn_ahb *owner, struct nn_ahb_pixel *out);

/* Shared qualification primitives for the existing detailed properties probe.
 * They expose diagnostics without duplicating the maps parser or ABI logic. */
int nn_ahb_native_fds(AHardwareBuffer *buffer, struct nn_ahb_fd_info fds[NN_AHB_MAX_FDS],
                      int *version, int *num_fds, int *num_ints, int *queried);
int nn_ahb_identify_mapping(const void *address, uint64_t payload,
                           const struct nn_ahb_fd_info *fds, int fd_count,
                           struct nn_ahb_mapping *mapping);
int nn_ahb_unlock_wait(AHardwareBuffer *buffer, int *safe, int *fence_count);

#ifdef __cplusplus
}
#endif
#endif
