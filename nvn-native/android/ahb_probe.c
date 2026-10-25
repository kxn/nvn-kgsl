/* SPDX-License-Identifier: MIT */
#include "ahb.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct writer { char *data; size_t capacity, used; int failed; };
static void append(struct writer *w, const char *format, ...)
{
    if (w->failed) return;
    va_list args;
    va_start(args, format);
    int n = vsnprintf(w->data + w->used, w->capacity - w->used, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= w->capacity - w->used) w->failed = 1;
    else w->used += (size_t)n;
}
static void string(struct writer *w, const char *s)
{
    append(w, "\"");
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '\\' || c == '"') append(w, "\\%c", c);
        else if (c < 32 || c >= 127) append(w, "\\u%04x", c);
        else append(w, "%c", c);
    }
    append(w, "\"");
}
static int api_errno(int result) { return result < 0 ? -result : result; }

static void mapping_json(struct writer *w, const struct nn_ahb_mapping *m)
{
    append(w, "{\"attempted\":%s,\"vma_found\":%s,\"shared\":%s,\"permissions\":",
        m->attempted ? "true" : "false", m->found ? "true" : "false", m->shared ? "true" : "false");
    string(w, m->permissions);
    append(w, ",\"matching_fd_count\":%d,\"fd_index\":%d,\"pixel_offset\":%" PRIu64
        ",\"payload_bytes\":%" PRIu64 ",\"vma_file_offset\":%" PRIu64 ",\"vma_bytes\":%" PRIu64 "}",
        m->matches, m->fd_index, m->offset, m->payload_bytes, m->vma_file_offset, m->vma_bytes);
}

int nn_ahb_info_probe(char *json, size_t capacity)
{
    if (!json || !capacity) return -1;
    const AHardwareBuffer_Desc requested = {
        .width = 64, .height = 64, .layers = 1,
        .format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
        .usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN |
                 AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,
    };
    AHardwareBuffer_Desc actual = {0};
    AHardwareBuffer *buffer = NULL;
    struct nn_ahb_fd_info fds[NN_AHB_MAX_FDS] = {0};
    const char *stage = "support";
    int error = 0, safe = 1, released = 0, fence_count = 0, allocated = 0;
    int supported = AHardwareBuffer_isSupported(&requested);
    int allocate_result = INT_MIN, write_lock_result = INT_MIN, read_lock_result = INT_MIN;
    int write_unlock_errno = 0, read_unlock_errno = 0;
    struct nn_ahb_mapping write_mapping = {.fd_index = -1}, read_mapping = {.fd_index = -1};
    int mapping_consistent = 0;
    int version = 0, num_fds = 0, num_ints = 0, fd_count = 0;
    int32_t write_bpp = -1, write_stride = -1, read_bpp = -1, read_stride = -1;
    unsigned sentinel_checks = 0;
    if (supported != 1) { error = ENOTSUP; goto done; }
    stage = "allocate";
    allocate_result = AHardwareBuffer_allocate(&requested, &buffer);
    if (allocate_result || !buffer) { error = allocate_result ? api_errno(allocate_result) : EIO; goto done; }
    allocated = 1;
    stage = "describe";
    AHardwareBuffer_describe(buffer, &actual);
    if (actual.width != requested.width || actual.height != requested.height ||
        actual.layers != 1 || actual.format != requested.format || actual.stride < 64 ||
        actual.stride > INT32_MAX / 4 || (actual.usage & requested.usage) != requested.usage) {
        error = ENOTSUP; goto done;
    }
    stage = "native_handle";
    error = nn_ahb_native_fds(buffer, fds, &version, &num_fds, &num_ints, &fd_count);
    if (error) goto done;
    stage = "cpu_write_lock";
    void *address = NULL;
    write_lock_result = AHardwareBuffer_lockAndGetInfo(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
        -1, NULL, &address, &write_bpp, &write_stride);
    if (write_lock_result) { error = api_errno(write_lock_result); goto done; }
    static const unsigned xy[4][2] = {{0,0},{63,0},{0,63},{63,63}};
    static const unsigned char sentinel[4][4] = {
        {0x17,0x29,0x43,0xff}, {0x56,0x78,0x9a,0xbc},
        {0xde,0xf0,0x12,0x34}, {0x65,0x87,0xa9,0xcb},
    };
    if (!address || write_bpp != 4 || write_stride != (int32_t)(actual.stride * 4)) error = ENOTSUP;
    if (!error) {
        stage = "cpu_write_mapping";
        error = nn_ahb_identify_mapping(address, (uint64_t)write_stride * actual.height, fds, fd_count, &write_mapping);
    }
    if (!error) for (unsigned i = 0; i < 4; ++i)
        memcpy((char *)address + (size_t)xy[i][1] * write_stride + xy[i][0] * 4, sentinel[i], 4);
    write_unlock_errno = nn_ahb_unlock_wait(buffer, &safe, &fence_count);
    if (!error) error = write_unlock_errno;
    if (error) goto done;
    stage = "cpu_read_lock";
    address = NULL;
    read_lock_result = AHardwareBuffer_lockAndGetInfo(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
        -1, NULL, &address, &read_bpp, &read_stride);
    if (read_lock_result) { error = api_errno(read_lock_result); goto done; }
    if (!address || read_bpp != write_bpp || read_stride != write_stride) error = ENOTSUP;
    if (!error) {
        stage = "cpu_read_mapping";
        error = nn_ahb_identify_mapping(address, (uint64_t)read_stride * actual.height, fds, fd_count, &read_mapping);
        if (!error) {
            mapping_consistent = write_mapping.fd_index == read_mapping.fd_index &&
                write_mapping.offset == read_mapping.offset && write_mapping.payload_bytes == read_mapping.payload_bytes;
            if (!mapping_consistent) error = ENOTSUP;
        }
    }
    if (!error) for (unsigned i = 0; i < 4; ++i) for (unsigned c = 0; c < 4; ++c) {
        unsigned char v = ((const unsigned char *)address)[(size_t)xy[i][1] * read_stride + xy[i][0] * 4 + c];
        if (v != sentinel[i][c]) error = EIO;
        ++sentinel_checks;
    }
    read_unlock_errno = nn_ahb_unlock_wait(buffer, &safe, &fence_count);
    if (!error) error = read_unlock_errno;
    if (!error) stage = "complete";
done:
    if (buffer && safe) { AHardwareBuffer_release(buffer); released = 1; }
    int passed = !error && allocated && released && mapping_consistent && sentinel_checks == 16 && getuid() >= 10000;
    struct writer w = {.data = json, .capacity = capacity};
    append(&w, "{\"schema\":1,\"operation\":\"ahb_properties\",\"uid\":%u,\"stage\":", (unsigned)getuid());
    string(&w, stage);
    append(&w, ",\"errno\":%d,\"gpu_commands_submitted\":0,\"gpu_execution\":false,\"display_presented\":false,"
        "\"platform_native_handle_extension\":true,\"portable_ndk_handle_api\":false,"
        "\"supported\":%d,\"allocate_result\":%d,\"allocated\":%s,\"buffer_released\":%s,"
        "\"requested\":{\"width\":64,\"height\":64,\"layers\":1,\"format\":%u,\"usage\":%" PRIu64 "},"
        "\"actual\":{\"width\":%u,\"height\":%u,\"layers\":%u,\"format\":%u,\"usage\":%" PRIu64 ",\"stride_pixels\":%u},"
        "\"native_handle\":{\"version\":%d,\"num_fds\":%d,\"num_ints\":%d,\"vendor_ints_read\":false},\"fds\":[",
        error, supported, allocate_result, allocated ? "true" : "false", released ? "true" : "false",
        requested.format, requested.usage, actual.width, actual.height, actual.layers, actual.format, actual.usage,
        actual.stride, version, num_fds, num_ints);
    for (int i = 0; i < fd_count; ++i) {
        const struct nn_ahb_fd_info *f = fds + i;
        append(&w, "%s{\"index\":%d,\"fd\":%d,\"size\":%" PRId64 ",\"mode\":%u,\"inode\":%" PRIu64
            ",\"device_major\":%u,\"device_minor\":%u,\"fstat_errno\":%d,\"readlink_errno\":%d,\"link\":",
            i ? "," : "", i, f->fd, f->bytes, f->mode, f->inode, f->device_major, f->device_minor,
            f->stat_errno, f->link_errno);
        string(&w, f->link); append(&w, "}");
    }
    append(&w, "],\"write_mapping\":"); mapping_json(&w, &write_mapping);
    append(&w, ",\"read_mapping\":"); mapping_json(&w, &read_mapping);
    append(&w, ",\"mapping_consistent\":%s,\"fd_count_queried\":%d,\"write_lock_result\":%d,\"write_bytes_per_pixel\":%d,\"write_bytes_per_stride\":%d,"
        "\"write_unlock_errno\":%d,\"read_lock_result\":%d,\"read_bytes_per_pixel\":%d,\"read_bytes_per_stride\":%d,"
        "\"read_unlock_errno\":%d,\"unlock_fences_waited\":%d,\"cpu_sentinel_checks\":%u,\"passed\":%s}\n",
        mapping_consistent ? "true" : "false", fd_count, write_lock_result, write_bpp, write_stride, write_unlock_errno, read_lock_result, read_bpp,
        read_stride, read_unlock_errno, fence_count, sentinel_checks, passed ? "true" : "false");
    if (w.failed) {
        (void)snprintf(json, capacity, "{\"schema\":1,\"operation\":\"ahb_properties\",\"errno\":%d,\"gpu_commands_submitted\":0,\"passed\":false}\n", ENOSPC);
        return -1;
    }
    return passed ? 0 : -1;
}
