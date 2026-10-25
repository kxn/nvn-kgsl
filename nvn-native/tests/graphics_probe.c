/* SPDX-License-Identifier: MIT */
#include "nvn_native/graphics.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include NVN_GRAPHICS_HEADER

static unsigned rounded(float value) { return (unsigned)(value * 255.0f + 0.5f); }

int nn_graphics_probe(const char *directory, char *json, size_t capacity)
{
    const struct nn_gfx_image *image = &NVN_GRAPHICS_IMAGE;
    nn_device *device = NULL; nn_queue *queue = NULL; nn_bo *job = NULL;
    nn_bo *resources[NN_GFX_ZERO] = {0};
    struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT] = {0};
    const char *stage = "open", *submission = "not_attempted";
    unsigned completed = 0, pixel_checks = 0, guard_checks = 0, timestamp = 0;
    uint32_t chip = 0; size_t gmem = 0, command_bytes = 0;
    int error = 0, cleanup = 0;
    unsigned bad_case = ~0u, bad_x = ~0u, bad_y = ~0u, bad_channel = ~0u, bad_actual = 0, bad_expected = 0;
    const unsigned char background[4] = {0x23, 0x45, 0x67, 0x89};
    if ((error = nn_device_open(&device))) goto done;
    chip = nn_device_chip(device); gmem = nn_device_gmem_bytes(device);
    if (image->width != 64 || image->height != 64 || image->target_bytes > 1048576) { error = ENOTSUP; goto done; }
    const size_t sizes[NN_GFX_ZERO] = {image->vs_code_bytes, image->fs_code_bytes,
        4096 + image->target_bytes + 4096, 4096, 4096};
    stage = "allocate";
    for (unsigned i = 0; i < NN_GFX_ZERO; ++i) {
        if ((error = nn_bo_alloc(device, sizes[i], resources + i))) goto done;
        memset(nn_bo_map(resources[i]), 0xa5, nn_bo_size(resources[i]));
        buffers[i] = (struct nn_gfx_buffer){resources[i], 0, nn_bo_size(resources[i])};
    }
    memcpy(nn_bo_map(resources[NN_GFX_VS_CODE]), image->vs_code, image->vs_code_bytes);
    memcpy(nn_bo_map(resources[NN_GFX_FS_CODE]), image->fs_code, image->fs_code_bytes);
    buffers[NN_GFX_TARGET].offset = 4096; buffers[NN_GFX_TARGET].bytes = image->target_bytes;
    buffers[NN_GFX_VERTICES].offset = 256; buffers[NN_GFX_VERTICES].bytes = image->vertex_bytes;
    buffers[NN_GFX_MATRIX].offset = 64; buffers[NN_GFX_MATRIX].bytes = image->matrix_bytes;
    stage = "encode";
    if ((error = nn_gfx_encode(device, image, buffers, &job, &command_bytes))) goto done;
    stage = "context";
    if ((error = nn_queue_create(device, &queue))) goto done;
    for (unsigned test = 0; test < 4; ++test) {
        float matrix[16] = {0};
        matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1;
        // Move the original fullscreen triangle to clip x >= 0, then entirely
        // beyond the viewport. Reusing the same IB tests indirect constants.
        if (test == 1) matrix[3] = 1;
        if (test == 2) matrix[3] = 3;
        float vertices[3][6] = {{-1, -1, 1, 0, 0, 1}, {3, -1, 1, 0, 0, 1}, {-1, 3, 1, 0, 0, 1}};
        if (test == 1 || test == 2) for (unsigned v = 0; v < 3; ++v) {
            vertices[v][2] = 0; vertices[v][3] = 1;
        }
        if (test == 3) {
            vertices[1][2] = 0; vertices[1][3] = 1;
            vertices[2][2] = 0; vertices[2][4] = 1;
        }
        unsigned char *target_base = nn_bo_map(resources[NN_GFX_TARGET]);
        memset(target_base, 0xa5, nn_bo_size(resources[NN_GFX_TARGET]));
        unsigned char *target = target_base + buffers[NN_GFX_TARGET].offset;
        for (unsigned y = 0; y < image->height; ++y)
            for (unsigned x = 0; x < image->width; ++x)
                memcpy(target + y * image->target_pitch + x * 4, background, 4);
        memcpy((char *)nn_bo_map(resources[NN_GFX_VERTICES]) + buffers[NN_GFX_VERTICES].offset, vertices, sizeof(vertices));
        memcpy((char *)nn_bo_map(resources[NN_GFX_MATRIX]) + buffers[NN_GFX_MATRIX].offset, matrix, sizeof(matrix));
        stage = "submit"; submission = "unknown";
        if ((error = nn_queue_submit(queue, job, 0, command_bytes, resources, NN_GFX_ZERO, &timestamp))) goto done;
        submission = "accepted"; stage = "wait";
        if ((error = nn_queue_finish(queue, 1000))) goto done;
        stage = "pixels";
        for (unsigned y = 0; y < image->height; ++y) for (unsigned x = 0; x < image->width; ++x) {
            unsigned expected[4] = {255, 0, 0, 255};
            if (test == 1 && x >= 32) { expected[0] = 0; expected[1] = 255; }
            if (test == 2 || (test == 1 && x < 32))
                for (unsigned c = 0; c < 4; ++c) expected[c] = background[c];
            if (test == 3) {
                float green = (x + 0.5f) / 128.0f, blue = (y + 0.5f) / 128.0f;
                expected[0] = rounded(1 - green - blue); expected[1] = rounded(green); expected[2] = rounded(blue);
            }
            for (unsigned c = 0; c < 4; ++c) {
                unsigned actual = target[y * image->target_pitch + x * 4 + c];
                unsigned delta = actual > expected[c] ? actual - expected[c] : expected[c] - actual;
                if (delta > (test == 3 ? 1u : 0u)) {
                    bad_case = test; bad_x = x; bad_y = y; bad_channel = c;
                    bad_actual = actual; bad_expected = expected[c]; error = EIO; goto save_failed;
                }
                ++pixel_checks;
            }
        }
        stage = "guards";
        for (unsigned i = 0; i < NN_GFX_ZERO; ++i) {
            const unsigned char *map = nn_bo_map(resources[i]);
            size_t start = buffers[i].offset, end = start +
                (i == NN_GFX_VS_CODE ? image->vs_code_bytes : i == NN_GFX_FS_CODE ? image->fs_code_bytes :
                 i == NN_GFX_TARGET ? image->target_bytes : i == NN_GFX_VERTICES ? sizeof(vertices) : sizeof(matrix));
            for (size_t byte = 0; byte < nn_bo_size(resources[i]); ++byte) {
                unsigned expected = 0xa5;
                if (byte >= start && byte < end) {
                    if (i == NN_GFX_TARGET) {
                        size_t relative = byte - start;
                        if (relative / image->target_pitch < image->height &&
                            relative % image->target_pitch < image->width * 4) continue;
                    } else {
                        const void *original = i == NN_GFX_VS_CODE ? (const void *)image->vs_code :
                            i == NN_GFX_FS_CODE ? (const void *)image->fs_code :
                            i == NN_GFX_VERTICES ? (const void *)vertices : (const void *)matrix;
                        expected = ((const unsigned char *)original)[byte - start];
                    }
                }
                if (map[byte] != expected) { error = EIO; goto save_failed; }
                ++guard_checks;
            }
        }
save_failed:;
        char path[1024];
        snprintf(path, sizeof(path), "%s/graphics-case-%u.rgba", directory, test);
        FILE *file = fopen(path, "wb");
        if (!file) { if (!error) error = errno; goto done; }
        int write_error = fwrite(target, 1, image->target_bytes, file) != image->target_bytes;
        int close_error = fclose(file);
        if (write_error || close_error) { if (!error) error = EIO; goto done; }
        if (error) goto done;
        ++completed;
    }
    stage = "complete";
done:
    if (queue) { int e = nn_queue_destroy(queue); if (e) cleanup = e; }
    if (job) { int e = nn_bo_destroy(job); if (e) cleanup = e; }
    for (unsigned i = 0; i < NN_GFX_ZERO; ++i) if (resources[i]) {
        int e = nn_bo_destroy(resources[i]); if (e) cleanup = e;
    }
    if (device) { int e = nn_device_close(device); if (e) cleanup = e; }
    int passed = !error && !cleanup && completed == 4;
    int n = snprintf(json, capacity,
        "{\"schema\":1,\"operation\":\"maxwell_graphics\",\"uid\":%u,\"stage\":\"%s\","
        "\"errno\":%d,\"cleanup_errno\":%d,\"chip_id\":%u,\"gmem_bytes\":%zu,"
        "\"submission\":\"%s\",\"timestamp\":%u,\"completed_cases\":%u,\"reused_ib\":true,"
        "\"width\":%u,\"height\":%u,\"pitch\":%u,\"target_bytes\":%u,"
        "\"channel_checks\":%u,\"guard_byte_checks\":%u,\"bad_case\":%u,\"bad_x\":%u,"
        "\"bad_y\":%u,\"bad_channel\":%u,\"bad_actual\":%u,\"bad_expected\":%u,\"passed\":%s}\n",
        (unsigned)getuid(), stage, error, cleanup, chip, gmem, submission, timestamp, completed,
        image->width, image->height, image->target_pitch, image->target_bytes,
        pixel_checks, guard_checks, bad_case, bad_x, bad_y, bad_channel, bad_actual, bad_expected, passed ? "true" : "false");
    return passed && n >= 0 && (size_t)n < capacity ? 0 : -1;
}
