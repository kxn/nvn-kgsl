/* SPDX-License-Identifier: MIT */
#include "nvn_native/graphics.h"
#include "texture_cases.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include NVN_TEXTURE_HEADER

/* A hardware qualification caller. Expected colors come from the independently
 * derived Maxwell closed form, never from the translated target machine code. */
int nn_texture_probe(const char *directory, char *json, size_t capacity)
{
    const struct nn_gfx_image *image = &NVN_GRAPHICS_IMAGE;
    const unsigned kinds[] = {NN_GFX_VS_CODE, NN_GFX_FS_CODE, NN_GFX_TARGET,
        NN_GFX_VERTICES, NN_GFX_MATRIX, NN_GFX_FS_UNIFORM, NN_GFX_TEXTURE};
    enum { COUNT = sizeof(kinds) / sizeof(kinds[0]) };
    nn_device *device = NULL; nn_queue *queue = NULL; nn_bo *job = NULL;
    nn_bo *resources[COUNT] = {0};
    unsigned char *expected[COUNT] = {0};
    struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT] = {0};
    const char *stage = "profile", *submission = "not_attempted";
    unsigned completed = 0, channel_checks = 0, readonly_checks = 0, timestamp = 0;
    unsigned reasons[4] = {0}, guard_checks = 0;
    unsigned bad_case = ~0u, bad_x = ~0u, bad_y = ~0u, bad_channel = ~0u;
    unsigned bad_kind = ~0u, bad_actual = 0, bad_expected = 0;
    size_t bad_offset = SIZE_MAX, command_bytes = 0;
    uint32_t chip = 0; size_t gmem = 0;
    int error = 0, cleanup = 0;
    if (image->schema != 2 || image->profile != NN_GFX_TEXTURE_2D_NORMALIZED ||
        image->width != 64 || image->height != 64 || image->target_pitch != 256 ||
        image->target_bytes != 16384 || image->texture_width != 2 || image->texture_height != 2 ||
        image->texture_pitch < 8 || image->texture_bytes != 4096 || image->vertex_bytes != 96 ||
        image->matrix_bytes != 64 || image->fs_uniform_bytes != 64) { error = ENOTSUP; goto done; }
    stage = "open";
    if ((error = nn_device_open(&device))) goto done;
    chip = nn_device_chip(device); gmem = nn_device_gmem_bytes(device);
    stage = "allocate";
    for (unsigned i = 0; i < COUNT; ++i) {
        unsigned kind = kinds[i];
        size_t bytes = kind == NN_GFX_TARGET ? 4096 + image->target_bytes + 4096 :
                       kind == NN_GFX_TEXTURE ? 4096 + image->texture_bytes + 4096 : 4096;
        if ((error = nn_bo_alloc(device, bytes, resources + i))) goto done;
        expected[i] = malloc(nn_bo_size(resources[i]));
        if (!expected[i]) { error = ENOMEM; goto done; }
        buffers[kind] = (struct nn_gfx_buffer){resources[i],
            kind == NN_GFX_TARGET || kind == NN_GFX_TEXTURE ? 4096 :
            kind == NN_GFX_VERTICES ? 256 : kind == NN_GFX_MATRIX ? 64 : kind == NN_GFX_FS_UNIFORM ? 128 : 0,
            kind == NN_GFX_TARGET ? image->target_bytes : kind == NN_GFX_TEXTURE ? image->texture_bytes :
            kind == NN_GFX_VERTICES ? image->vertex_bytes : kind == NN_GFX_MATRIX ? image->matrix_bytes :
            kind == NN_GFX_FS_UNIFORM ? image->fs_uniform_bytes :
            kind == NN_GFX_VS_CODE ? image->vs_code_bytes : image->fs_code_bytes};
    }
    stage = "encode";
    if ((error = nn_gfx_encode(device, image, buffers, &job, &command_bytes))) goto done;
    stage = "context";
    if ((error = nn_queue_create(device, &queue))) goto done;
    for (unsigned test = 0; test < NN_TEXTURE_CASE_COUNT; ++test) {
        struct nn_texture_case_inputs input;
        if (nn_texture_case_prepare(test, &input)) { error = EINVAL; goto done; }
        for (unsigned i = 0; i < COUNT; ++i) {
            unsigned kind = kinds[i];
            memset(expected[i], 0xa5, nn_bo_size(resources[i]));
            unsigned char *payload = expected[i] + buffers[kind].offset;
            if (kind == NN_GFX_VS_CODE) memcpy(payload, image->vs_code, image->vs_code_bytes);
            else if (kind == NN_GFX_FS_CODE) memcpy(payload, image->fs_code, image->fs_code_bytes);
            else if (kind == NN_GFX_VERTICES) memcpy(payload, input.vertices, sizeof(input.vertices));
            else if (kind == NN_GFX_MATRIX) memcpy(payload, input.vs_ubo, sizeof(input.vs_ubo));
            else if (kind == NN_GFX_FS_UNIFORM) memcpy(payload, input.fs_ubo, sizeof(input.fs_ubo));
            else if (kind == NN_GFX_TEXTURE) for (unsigned y = 0; y < 2; ++y)
                memcpy(payload + y * image->texture_pitch, input.texels + y * 8, 8);
            else if (kind == NN_GFX_TARGET) for (unsigned y = 0; y < image->height; ++y)
                for (unsigned x = 0; x < image->width; ++x)
                    memcpy(payload + y * image->target_pitch + x * 4, input.clear, 4);
            memcpy(nn_bo_map(resources[i]), expected[i], nn_bo_size(resources[i]));
        }
        stage = "submit"; submission = "unknown";
        if ((error = nn_queue_submit(queue, job, 0, command_bytes, resources, COUNT, &timestamp))) goto done;
        stage = "wait"; submission = "accepted";
        if ((error = nn_queue_finish(queue, 1000))) goto done;
        const unsigned char *target = (const unsigned char *)nn_bo_map(buffers[NN_GFX_TARGET].bo) + buffers[NN_GFX_TARGET].offset;
        stage = "pixels";
        for (unsigned y = 0; y < image->height; ++y) for (unsigned x = 0; x < image->width; ++x) {
            struct nn_texture_expected pixel = nn_texture_case_pixel(&input, x, y);
            if (pixel.reason == NN_TEXTURE_INVALID || pixel.reason > NN_TEXTURE_INVALID) { error = EINVAL; goto save; }
            ++reasons[pixel.reason];
            for (unsigned c = 0; c < 4; ++c) {
                unsigned actual = target[y * image->target_pitch + x * 4 + c];
                unsigned delta = actual > pixel.rgba[c] ? actual - pixel.rgba[c] : pixel.rgba[c] - actual;
                if (delta > pixel.tolerance) {
                    bad_case = test; bad_x = x; bad_y = y; bad_channel = c;
                    bad_actual = actual; bad_expected = pixel.rgba[c]; error = EIO; goto save;
                }
                ++channel_checks;
            }
        }
        stage = "guards";
        for (unsigned i = 0; i < COUNT; ++i) for (size_t byte = 0; byte < nn_bo_size(resources[i]); ++byte) {
            unsigned kind = kinds[i];
            const struct nn_gfx_buffer *b = buffers + kind;
            int inside = byte >= b->offset && byte - b->offset < b->bytes;
            if (kind == NN_GFX_TARGET && inside) continue;
            unsigned actual = ((const unsigned char *)nn_bo_map(resources[i]))[byte];
            if (actual != expected[i][byte]) {
                bad_case = test; bad_kind = kind; bad_offset = byte;
                bad_actual = actual; bad_expected = expected[i][byte]; error = EIO; goto save;
            }
            if (inside) ++readonly_checks; else ++guard_checks;
        }
save:;
        char path[1024];
        int length = snprintf(path, sizeof(path), "%s/texture-case-%u.rgba", directory, test);
        if (length < 0 || (size_t)length >= sizeof(path)) { if (!error) error = ENAMETOOLONG; goto done; }
        FILE *file = fopen(path, "wb");
        if (!file) { if (!error) error = errno; goto done; }
        int write_error = fwrite(target, 1, image->target_bytes, file) != image->target_bytes;
        int close_error = fclose(file);
        if (write_error || close_error) { if (!error) error = EIO; goto done; }
        if (error) goto done;
        ++completed;
    }
    if (reasons[NN_TEXTURE_OUTSIDE] != 1024 || reasons[NN_TEXTURE_DISCARD] != 3840 ||
        reasons[NN_TEXTURE_SHADED] != 11520) { error = EINVAL; goto done; }
    stage = "complete";
done:
    if (queue) { int e = nn_queue_destroy(queue); if (e && !cleanup) cleanup = e; }
    if (job) { int e = nn_bo_destroy(job); if (e && !cleanup) cleanup = e; }
    for (unsigned i = 0; i < COUNT; ++i) {
        free(expected[i]);
        if (resources[i]) { int e = nn_bo_destroy(resources[i]); if (e && !cleanup) cleanup = e; }
    }
    if (device) { int e = nn_device_close(device); if (e && !cleanup) cleanup = e; }
    int passed = !error && !cleanup && completed == NN_TEXTURE_CASE_COUNT;
    int n = snprintf(json, capacity,
        "{\"schema\":1,\"operation\":\"maxwell_texture\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,"
        "\"cleanup_errno\":%d,\"chip_id\":%u,\"gmem_bytes\":%zu,\"submission\":\"%s\",\"timestamp\":%u,"
        "\"completed_cases\":%u,\"reused_ib\":%s,\"width\":%u,\"height\":%u,\"pitch\":%u,\"target_bytes\":%u,"
        "\"channel_checks\":%u,\"guard_byte_checks\":%u,\"readonly_byte_checks\":%u,"
        "\"outside_pixels\":%u,\"discard_pixels\":%u,\"shaded_pixels\":%u,"
        "\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,\"bad_kind\":%u,"
        "\"bad_offset\":%zu,\"bad_actual\":%u,\"bad_expected\":%u,\"passed\":%s}\n",
        (unsigned)getuid(), stage, error, cleanup, chip, gmem, submission, timestamp, completed,
        completed > 1 ? "true" : "false", image->width, image->height, image->target_pitch, image->target_bytes,
        channel_checks, guard_checks, readonly_checks, reasons[NN_TEXTURE_OUTSIDE], reasons[NN_TEXTURE_DISCARD], reasons[NN_TEXTURE_SHADED],
        bad_case, bad_x, bad_y, bad_channel, bad_kind, bad_offset, bad_actual, bad_expected, passed ? "true" : "false");
    return passed && n >= 0 && (size_t)n < capacity ? 0 : -1;
}
