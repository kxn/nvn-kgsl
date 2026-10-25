/* SPDX-License-Identifier: MIT
 * STAGE 1 ONLY: live kernel metadata gate. No GPU work or cache maintenance.
 * This cannot qualify CPU/GPU coherency or NVN CPU_UNCACHED semantics.
 */
#define _GNU_SOURCE
#define __user
#include "msm_kgsl.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int iocoh_read_ioctl(int fd, unsigned long op, void *arg)
{
    int r;
    do { r = ioctl(fd, op, arg); } while (r < 0 && errno == EINTR);
    return r < 0 ? errno : 0;
}

/* Compatible with other probe entry points. directory is unused: no images.
 * A successful result means the two returned flag sets retained the request,
 * not that shader coherency was tested. Import/free mutations are not retried.
 * On ambiguous import or failed release the fd and original mapping are kept
 * until process teardown; JSON reports retention and no successful gate.
 */
int nn_iocoherent_import_probe(const char *directory, char *json, size_t capacity)
{
    (void)directory;
    if (!json) return EINVAL;
    if (capacity < 2048) return ENOSPC;
    int error = 0, cleanup_error = 0, fd = -1, retained = 0;
    int import_attempted = 0, imported = 0, metadata_passed = 0;
    uint32_t chip = 0, object_id = 0, import_flags = 0, info_flags = 0;
    uint64_t gpu_address = 0, info_useraddr = 0;
    size_t bytes = 0;
    void *storage = MAP_FAILED;
    uintptr_t host = 0;
    unsigned seed_checks = 0;
    const uint64_t requested = (uint64_t)KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT |
                               KGSL_MEMFLAGS_IOCOHERENT;
    long page = sysconf(_SC_PAGESIZE);
    if (page != 4096) { error = ENOTSUP; goto done; }
    bytes = (size_t)page * 2;
    storage = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (storage == MAP_FAILED) { error = errno; goto done; }
    host = (uintptr_t)storage;
#if defined(__aarch64__)
    host &= UINT64_C(0x00ffffffffffffff);
#endif
    /* Fault in the original two pages. No alternative mapping or shadow BO. */
    volatile uint32_t *words = storage;
    for (size_t i = 0; i < bytes / sizeof(*words); i++)
        words[i] = UINT32_C(0xb1730000) ^ (uint32_t)i;
    atomic_thread_fence(memory_order_seq_cst); /* ordering, no cache clean */
    fd = open("/dev/kgsl-3d0", O_RDWR | O_CLOEXEC);
    if (fd < 0) { error = errno; goto done; }
    struct kgsl_devinfo device = {0};
    struct kgsl_device_getproperty property = {
        .type = KGSL_PROP_DEVICE_INFO, .value = &device, .sizebytes = sizeof(device)
    };
    error = iocoh_read_ioctl(fd, IOCTL_KGSL_DEVICE_GETPROPERTY, &property);
    if (error) goto done;
    chip = device.chip_id;
    if (chip != UINT32_C(0x06050002) || !device.mmu_enabled) {
        error = ENOTSUP; goto done;
    }
    struct kgsl_gpuobj_import_useraddr address = {.virtaddr = host};
    uintptr_t private_address = (uintptr_t)&address;
#if defined(__aarch64__)
    private_address &= UINT64_C(0x00ffffffffffffff);
#endif
    struct kgsl_gpuobj_import request = {
        .priv = private_address, .priv_len = bytes, .flags = requested,
        .type = KGSL_USER_MEM_TYPE_ADDR
    };
    import_attempted = 1;
    if (ioctl(fd, IOCTL_KGSL_GPUOBJ_IMPORT, &request)) {
        error = errno;
        /* Failed allocating mutation can have uncertain ownership; keep both. */
        retained = 1;
        goto done;
    }
    imported = 1;
    object_id = request.id;
    import_flags = (uint32_t)request.flags;
    if (!object_id) { error = ENOTSUP; retained = 1; goto done; }
    struct kgsl_gpumem_get_info info = {.id = object_id};
    error = iocoh_read_ioctl(fd, IOCTL_KGSL_GPUMEM_GET_INFO, &info);
    if (error) goto done;
    gpu_address = info.gpuaddr;
    info_flags = info.flags;
    info_useraddr = info.useraddr;
    if (info.id != object_id || info.size != bytes || !gpu_address ||
        gpu_address % (size_t)page ||
        (request.flags & (KGSL_CACHEMODE_MASK | KGSL_MEMFLAGS_IOCOHERENT)) != requested ||
        (info.flags & (KGSL_CACHEMODE_MASK | KGSL_MEMFLAGS_IOCOHERENT)) != requested ||
        ((info.flags | request.flags) & (KGSL_MEMFLAGS_GPUREADONLY | KGSL_MEMFLAGS_SECURE))) {
        error = ENOTSUP; goto done;
    }
    for (size_t i = 0; i < bytes / sizeof(*words); i++) {
        if (words[i] != (UINT32_C(0xb1730000) ^ (uint32_t)i)) {
            error = EIO; goto done;
        }
        seed_checks++;
    }
    metadata_passed = 1;
done:
    if (!retained && imported && object_id) {
        struct kgsl_gpuobj_free release = {.id = object_id};
        if (ioctl(fd, IOCTL_KGSL_GPUOBJ_FREE, &release)) {
            cleanup_error = errno;
            retained = 1;
        }
    }
    if (!retained && storage != MAP_FAILED && munmap(storage, bytes)) {
        cleanup_error = errno;
        retained = 1;
    }
    if (!retained && fd >= 0 && close(fd)) cleanup_error = errno;
    if (cleanup_error && !error) error = cleanup_error;
    if (error) metadata_passed = 0;
    int written = snprintf(json, capacity,
        "{\"schema\":1,\"operation\":\"kgsl_iocoherent_import_metadata\",\"errno\":%d,"
        "\"uid\":%u,\"passed\":%s,"
        "\"scope\":\"stage1_metadata_only\",\"chip_id\":%u,"
        "\"requested_flags\":%" PRIu64 ",\"import_returned_flags\":%u,"
        "\"info_returned_flags\":%u,\"object_id\":%u,\"gpu_address\":%" PRIu64 ","
        "\"submitted_useraddr\":%" PRIuPTR ",\"info_useraddr\":%" PRIu64 ","
        "\"bytes\":%zu,\"seed_word_checks\":%u,\"import_attempted\":%s,"
        "\"metadata_gate_passed\":%s,\"input_pointer_preserved\":%s,"
        "\"physical_identity_measured\":false,\"shader_coherency_qualified\":false,"
        "\"nvn_uncached_qualified\":false,\"gpu_submissions\":0,"
        "\"cache_maintenance_calls\":0,\"cleanup_errno\":%d,\"resources_retained\":%s}",
        error, (unsigned)getuid(), metadata_passed ? "true" : "false",
        chip, requested, import_flags, info_flags, object_id, gpu_address,
        host, info_useraddr, bytes, seed_checks, import_attempted ? "true" : "false",
        metadata_passed ? "true" : "false", import_attempted ? "true" : "false",
        cleanup_error, retained ? "true" : "false");
    if (written < 0 || (size_t)written >= capacity) return ENOSPC;
    return error;
}
