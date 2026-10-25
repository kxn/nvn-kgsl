/* SPDX-License-Identifier: MIT */
#include "nvn_native/display_shape.h"
#include "ahb.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

/* This is the published Android14 cutils/native_handle.h layout, not a
 * Qualcomm private handle. The flexible tail has the same ABI as its data[0].
 * Never interpret the vendor integers following the fd array.
 *
 * AHardwareBuffer_getNativeHandle is absent from the public NDK header. Its
 * declaration is published in frameworks/native's VNDK hardware_buffer.h;
 * Android14 libandroid.map.txt exports the symbol starting at API26. The
 * installed API29 NDK libandroid stub links it. This probe qualifies that
 * platform extension on this device; it is not a portable NDK API guarantee.
 * Sources and hashes: .local/nvn-native/parallel/android-present-plan.json. */
typedef struct native_handle {
    int version, numFds, numInts;
    int data[];
} native_handle_t;
_Static_assert(sizeof(native_handle_t) == 12, "published native_handle ABI");
_Static_assert(offsetof(native_handle_t, data) == 12, "published native_handle tail");
extern const native_handle_t *AHardwareBuffer_getNativeHandle(const AHardwareBuffer *);

static int api_errno(int result) { return result < 0 ? -result : result; }

/* Published pipa kernel task_mmu.c:366-369 and stat.c:35-36 use the same
 * file inode/device. ION's shared mmap preserves vm_file and vm_pgoff;
 * reject private/COW mappings, whose pgoff can instead encode a PFN.
 * This identifies a CPU payload's backing fd/offset, not a GPU tile layout. */
int nn_ahb_identify_mapping(const void *address, uint64_t payload,
                            const struct nn_ahb_fd_info *fds, int fd_count,
                            struct nn_ahb_mapping *mapping)
{
    if (!address || !payload || !fds || fd_count < 1 || fd_count > NN_AHB_MAX_FDS || !mapping) return EINVAL;
    memset(mapping, 0, sizeof(*mapping));
    mapping->attempted = 1; mapping->fd_index = -1; mapping->payload_bytes = payload;
    uint64_t pointer = (uintptr_t)address;
#if defined(__aarch64__)
    pointer &= UINT64_C(0x00ffffffffffffff);
#endif
    FILE *file = fopen("/proc/self/maps", "r");
    if (!file) return errno;
    char *line = NULL;
    size_t capacity = 0;
    int error = ENOENT;
    for (;;) {
        errno = 0;
        ssize_t length = getline(&line, &capacity, file);
        if (length < 0) {
            if (ferror(file)) error = errno ? errno : EIO;
            break;
        }
        if (length > 4096) { error = ENAMETOOLONG; break; }
        unsigned long long start, end, offset, inode;
        unsigned int device_major, device_minor;
        char permissions[5] = {0};
        if (sscanf(line, "%llx-%llx %4s %llx %x:%x %llu", &start, &end, permissions,
                   &offset, &device_major, &device_minor, &inode) != 7 || start >= end) {
            error = EINVAL; break;
        }
        if (pointer < start || pointer >= end) continue;
        mapping->found = 1;
        memcpy(mapping->permissions, permissions, sizeof(permissions));
        mapping->shared = permissions[3] == 's';
        mapping->vma_file_offset = offset; mapping->vma_bytes = end - start;
        if (strcmp(permissions, "rw-s") || !inode || !payload || payload > end - pointer) {
            error = ENOTSUP; break;
        }
        uint64_t delta = pointer - start;
        if (offset > UINT64_MAX - delta) { error = EOVERFLOW; break; }
        mapping->offset = offset + delta;
        for (int i = 0; i < fd_count; ++i) {
            if (!fds[i].stat_errno && fds[i].inode == inode &&
                fds[i].device_major == device_major && fds[i].device_minor == device_minor) {
                ++mapping->matches;
                mapping->fd_index = i;
            }
        }
        if (mapping->matches != 1) { mapping->fd_index = -1; error = ENOTSUP; break; }
        int64_t bytes = fds[mapping->fd_index].bytes;
        if (bytes < 0 || mapping->offset > (uint64_t)bytes ||
            payload > (uint64_t)bytes - mapping->offset) { error = ERANGE; break; }
        error = 0;
        break;
    }
    free(line);
    if (fclose(file) && !error) error = errno;
    return error;
}
/* This waits on the gralloc unlock fence, not GPU work or a frame timer.
 * An uncertain unlock keeps the AHB alive until process teardown. */
int nn_ahb_unlock_wait(AHardwareBuffer *buffer, int *safe, int *fence_count)
{
    if (!buffer || !safe || !fence_count) return EINVAL;
    int32_t fd = -1;
    int result = AHardwareBuffer_unlock(buffer, &fd);
    int error = api_errno(result);
    if (fd >= 0) {
        ++*fence_count;
        struct pollfd p = {.fd = fd, .events = POLLIN};
        int n = poll(&p, 1, 1000);
        if (!error) {
            if (n < 0) error = errno;
            else if (!n) error = ETIMEDOUT;
            else if (!(p.revents & POLLIN) || (p.revents & (POLLERR | POLLNVAL))) error = EIO;
        }
        if (close(fd) && !error) error = errno;
    }
    if (error) *safe = 0;
    return error;
}

int nn_ahb_native_fds(AHardwareBuffer *buffer, struct nn_ahb_fd_info fds[NN_AHB_MAX_FDS],
                      int *version, int *num_fds, int *num_ints, int *queried)
{
    if (!buffer || !fds || !version || !num_fds || !num_ints || !queried) return EINVAL;
    *version = *num_fds = *num_ints = *queried = 0;
    memset(fds, 0, sizeof(*fds) * NN_AHB_MAX_FDS);
    const native_handle_t *handle = AHardwareBuffer_getNativeHandle(buffer);
    if (!handle) return ENOTSUP;
    *version = handle->version; *num_fds = handle->numFds; *num_ints = handle->numInts;
    if (*version != (int)sizeof(*handle) || *num_fds < 1 || *num_fds > NN_AHB_MAX_FDS ||
        *num_ints < 0 || *num_ints > NN_AHB_MAX_INTS) return ENOTSUP;
    for (int i = 0; i < *num_fds; ++i) {
        struct nn_ahb_fd_info *f = fds + (*queried)++;
        f->fd = handle->data[i];
        if (f->fd < 0) return EBADF;
        struct stat st;
        if (fstat(f->fd, &st)) f->stat_errno = errno;
        else {
            f->bytes = st.st_size; f->mode = st.st_mode; f->inode = st.st_ino;
            f->device_major = major(st.st_dev); f->device_minor = minor(st.st_dev);
        }
        char path[64];
        int n = snprintf(path, sizeof(path), "/proc/self/fd/%d", f->fd);
        if (n < 0 || (size_t)n >= sizeof(path)) f->link_errno = ENAMETOOLONG;
        else {
            ssize_t bytes = readlink(path, f->link, sizeof(f->link) - 1);
            if (bytes < 0) f->link_errno = errno;
            else if ((size_t)bytes == sizeof(f->link) - 1) f->link_errno = ENAMETOOLONG;
            else f->link[bytes] = 0;
        }
        /* Borrowed AHB fds: inspect metadata only; no role/ordering assumption. */
        if (f->stat_errno || f->link_errno) return f->stat_errno ? f->stat_errno : f->link_errno;
    }
    return 0;
}

struct nn_ahb {
    AHardwareBuffer *buffer;
    AHardwareBuffer_Desc description;
    struct nn_ahb_fd_info fds[NN_AHB_MAX_FDS];
    int fd_count, locked, uncertain, identified, rejected;
    struct nn_ahb_pixel pixel;
    /* Immutable identity from the FIRST successful mapping qualification.
     * fds[] is refreshed each lock and cannot serve as the prior identity. */
    uint64_t first_inode, first_fd_bytes;
    uint32_t first_dev_major, first_dev_minor;
};

int nn_ahb_create(nn_ahb **out)
{ return nn_ahb_create_rgba(64,64,out); }

int nn_ahb_create_rgba(uint32_t width, uint32_t height, nn_ahb **out)
{
    if (!out) return EINVAL;
    *out = NULL;
    if (!width || !height || width>16384 || height>16384) return ENOTSUP;
    const AHardwareBuffer_Desc requested = {
        .width = width, .height = height, .layers = 1,
        .format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, .usage = NN_AHB_USAGE,
    };
    if (AHardwareBuffer_isSupported(&requested) != 1) return ENOTSUP;
    nn_ahb *owner = calloc(1, sizeof(*owner));
    if (!owner) return ENOMEM;
    int error = api_errno(AHardwareBuffer_allocate(&requested, &owner->buffer));
    if (!error && !owner->buffer) error = EIO;
    if (error) goto fail;
    AHardwareBuffer_describe(owner->buffer, &owner->description);
    const AHardwareBuffer_Desc *d = &owner->description;
    struct nn_display_rgba8_shape shape;
    if (d->width != width || d->height != height || d->layers != 1 || d->format != requested.format ||
        d->stride > INT32_MAX / 4 || d->usage != requested.usage ||
        nn_display_rgba8_shape(d->width,d->height,d->stride*4,&shape)) {
        error = ENOTSUP; goto fail;
    }
    int version, count, ints;
    error = nn_ahb_native_fds(owner->buffer, owner->fds, &version, &count, &ints, &owner->fd_count);
    if (error) goto fail;
    owner->pixel.fd = owner->pixel.fd_index = -1;
    *out = owner;
    return 0;
fail:
    if (owner->buffer) AHardwareBuffer_release(owner->buffer);
    free(owner);
    return error;
}

int nn_ahb_lock(nn_ahb *owner, uint64_t usage, struct nn_ahb_view *out)
{
    if (!owner || !out) return EINVAL;
    memset(out, 0, sizeof(*out));
    out->bytes_per_pixel = out->bytes_per_stride = -1; out->mapping.fd_index = -1;
    if (owner->locked || owner->uncertain) return EBUSY;
    if (owner->rejected) return ENOTSUP;
    if (!usage || usage & ~(uint64_t)(AHARDWAREBUFFER_USAGE_CPU_READ_MASK | AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK)) return EINVAL;
    uint64_t read = usage & AHARDWAREBUFFER_USAGE_CPU_READ_MASK;
    uint64_t write = usage & AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK;
    if ((read && read != AHARDWAREBUFFER_USAGE_CPU_READ_RARELY && read != AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN) ||
        (write && write != AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY && write != AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN)) return EINVAL;
    int error = api_errno(AHardwareBuffer_lockAndGetInfo(owner->buffer, usage, -1, NULL,
        &out->address, &out->bytes_per_pixel, &out->bytes_per_stride));
    if (error) { out->address = NULL; return error; }
    owner->locked = 1;
    const AHardwareBuffer_Desc *d = &owner->description;
    if (!out->address || out->bytes_per_pixel != 4 || out->bytes_per_stride != (int32_t)(d->stride * 4)) error = ENOTSUP;
    /* Re-read actual handle/fstat metadata for every mapping qualification;
     * a stale cached fd identity must never authorize another allocation. */
    int version, count, ints;
    if (!error) error = nn_ahb_native_fds(owner->buffer,owner->fds,&version,&count,&ints,&owner->fd_count);
    struct nn_display_rgba8_shape shape;
    if (!error) error = nn_display_rgba8_shape(d->width,d->height,(uint32_t)out->bytes_per_stride,&shape);
    if (!error) error = nn_ahb_identify_mapping(out->address, shape.storage_bytes,
                                              owner->fds, owner->fd_count, &out->mapping);
    if (!error && owner->identified && (out->mapping.fd_index != owner->pixel.fd_index ||
        out->mapping.offset != owner->pixel.offset || out->mapping.payload_bytes != owner->pixel.payload_bytes)) error = ENOTSUP;
    if (!error) error = nn_display_rgba8_span(&shape,out->mapping.offset,
        (uint64_t)owner->fds[out->mapping.fd_index].bytes);
    if (!error && owner->identified) {
        const struct nn_ahb_fd_info *actual = owner->fds + out->mapping.fd_index;
        if (actual->inode != owner->first_inode || actual->device_major != owner->first_dev_major ||
            actual->device_minor != owner->first_dev_minor || (uint64_t)actual->bytes != owner->first_fd_bytes)
            error = ENOTSUP;
    }
    if (error) {
        /* Mapping rejection never hands a CPU lock to the caller. Any failed
         * balancing unlock poisons the retained owner through nn_ahb_unlock. */
        owner->rejected = 1;
        int unlock_error = nn_ahb_unlock(owner);
        out->address = NULL;
        return unlock_error ? unlock_error : error;
    }
    const struct nn_ahb_fd_info *fd = owner->fds + out->mapping.fd_index;
    if (!owner->identified) {
        owner->first_inode = fd->inode; owner->first_fd_bytes = (uint64_t)fd->bytes;
        owner->first_dev_major = fd->device_major; owner->first_dev_minor = fd->device_minor;
    }
    owner->pixel = (struct nn_ahb_pixel){.fd = fd->fd, .fd_index = out->mapping.fd_index,
        .offset = out->mapping.offset, .bytes = fd->bytes, .pitch = out->bytes_per_stride,
        .width = d->width, .height = d->height, .payload_bytes = shape.storage_bytes};
    owner->identified = 1;
    return 0;
}

int nn_ahb_unlock(nn_ahb *owner)
{
    if (!owner) return EINVAL;
    if (owner->uncertain) return EBUSY;
    if (!owner->locked) return EINVAL;
    int safe = 1, fences = 0;
    int error = nn_ahb_unlock_wait(owner->buffer, &safe, &fences);
    owner->locked = 0;
    if (!safe) owner->uncertain = 1;
    return error;
}

int nn_ahb_destroy(nn_ahb *owner)
{
    if (!owner) return 0;
    if (owner->locked || owner->uncertain) return EBUSY;
    AHardwareBuffer_release(owner->buffer);
    free(owner);
    return 0;
}
const AHardwareBuffer_Desc *nn_ahb_description(const nn_ahb *owner)
{
    return owner ? &owner->description : NULL;
}
AHardwareBuffer *nn_ahb_buffer(const nn_ahb *owner)
{
    return owner ? owner->buffer : NULL;
}
int nn_ahb_get_pixel(const nn_ahb *owner, struct nn_ahb_pixel *out)
{
    if (!owner || !out) return EINVAL;
    memset(out, 0, sizeof(*out)); out->fd = out->fd_index = -1;
    if (owner->locked || owner->uncertain) return EBUSY;
    if (owner->rejected) return ENOTSUP;
    if (!owner->identified) return EAGAIN;
    *out = owner->pixel;
    return 0;
}
