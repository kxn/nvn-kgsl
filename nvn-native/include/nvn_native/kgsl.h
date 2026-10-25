#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nn_device nn_device;
typedef struct nn_bo nn_bo;
typedef struct nn_queue nn_queue;
struct nn_device_identity {
    uint32_t chip_id, gpu_id, device_id, mmu_enabled;
    size_t page_bytes, gmem_bytes;
};
/* Read-only property query. It neither qualifies an unknown chip nor creates
 * allocations, contexts or GPU work. nn_device_open retains its separate gate. */
int nn_device_inspect(struct nn_device_identity *identity);
/* All return 0 on success or a positive errno. Single-owner-thread API for now.
 * No retries of allocating/submitting ioctls with potentially ambiguous results.
 * Destruction returns EBUSY while in-flight ownership is unresolved. */
int nn_device_open(nn_device **out);
int nn_device_close(nn_device *device);
uint32_t nn_device_chip(const nn_device *device);
size_t nn_device_page(const nn_device *device);
size_t nn_device_gmem_bytes(const nn_device *device);
int nn_bo_alloc(nn_device *device, size_t bytes, nn_bo **out);
int nn_bo_import(nn_device *device, void *storage, size_t bytes, nn_bo **out);
/* Same caller pages and WB CPU mapping, with required hardware IO coherency.
 * Exact A650 gate: both import and GET_INFO must retain IOCOHERENT. No fallback
 * to noncoherent WB and no replacement mapping. This is not a CPU NC PTE. */
int nn_bo_import_coherent(nn_device *device, void *storage, size_t bytes, nn_bo **out);
/* Borrow fd for the ioctl; the kernel retains its own dma-buf reference. The
 * caller keeps the external allocator owner alive through GPU/display use.
 * No CPU mmap is provided: use the allocator's lock/unlock cache protocol.
 * This exact-size A650 import does not establish an image tiling contract. */
int nn_bo_import_dmabuf(nn_device *device, int fd, size_t fd_bytes, nn_bo **out);
int nn_bo_destroy(nn_bo *bo);
void *nn_bo_map(const nn_bo *bo);
uint64_t nn_bo_address(const nn_bo *bo);
size_t nn_bo_size(const nn_bo *bo);
uint32_t nn_bo_flags(const nn_bo *bo);
int nn_bo_cache(nn_bo *bo, size_t offset, size_t size, int from_gpu);
int nn_queue_create(nn_device *device, nn_queue **out);
int nn_queue_destroy(nn_queue *queue);
int nn_queue_submit(nn_queue *queue, nn_bo *commands, size_t offset, size_t bytes,
                    nn_bo *const *resources, size_t resource_count, uint32_t *timestamp);
int nn_queue_finish(nn_queue *queue, uint32_t timeout_ms);
uint32_t nn_queue_timestamp(const nn_queue *queue);
/* Native queue-family candidate: timestamps are kernel context timestamps.
 * Query/wait never claim completion on ioctl failure. Dependencies are copied
 * by KGSL submit and must refer to a live same-device source context. */
struct nn_queue_dependency { nn_queue *queue; uint32_t timestamp; };
int nn_queue_submit_waits(nn_queue *, nn_bo *, size_t, size_t, nn_bo *const *, size_t,
                           const struct nn_queue_dependency *, size_t, uint32_t *);
int nn_queue_timestamp_status(nn_queue *, uint32_t, int *completed);
int nn_queue_wait_timestamp(nn_queue *, uint32_t, uint32_t timeout_ms);
int nn_queue_faults(nn_queue *, int *faults);
int nn_queue_flush(nn_queue *);
#ifdef __cplusplus
}
#endif
