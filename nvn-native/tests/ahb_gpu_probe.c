/* SPDX-License-Identifier: MIT */
#include "nvn_native/graphics.h"
#include "nvn_native/pm4.h"
#include "../android/ahb_frame.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_AHB_GRAPHICS_HEADER

/* This is a bounded device qualification caller, not a compositor/presentation
 * test or execution of a game module. GPU writes go directly to the identified
 * AHB DMA-BUF. CPU access to its pixels occurs only inside AHB lock/unlock. */
static unsigned ahb_probe_rounded(float value) { return (unsigned)(value * 255.0f + 0.5f); }

static int ahb_probe_view(const struct nn_ahb_view *view, const struct nn_ahb_pixel *pixel)
{
    if (!view->address || view->bytes_per_pixel != 4 || view->bytes_per_stride != 256 ||
        !view->mapping.found || !view->mapping.shared || view->mapping.matches != 1 ||
        view->mapping.payload_bytes != 16384 || strcmp(view->mapping.permissions, "rw-s")) return ENOTSUP;
    if (pixel && (view->mapping.fd_index != pixel->fd_index || view->mapping.offset != pixel->offset)) return ENOTSUP;
    return 0;
}

static int ahb_gpu_run(const char *directory, char *json, size_t capacity,
                       int export_frame, nn_ahb **out)
{
    const struct nn_gfx_image *image = &NVN_GRAPHICS_IMAGE;
    const char *stage = "profile", *submission = "not_attempted";
    nn_ahb *owner = NULL;
    struct nn_ahb_pixel pixel = {.fd = -1, .fd_index = -1};
    struct nn_ahb_view view = {0};
    nn_device *device = NULL; nn_queue *queue = NULL;
    nn_bo *cp_job = NULL, *draw_job = NULL;
    nn_bo *resources[NN_GFX_ZERO] = {0};
    struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT] = {0};
    unsigned char *expected_resources[NN_GFX_ZERO] = {0}, *expected_commands = NULL;
    unsigned char shadow[16384], cp_commands[4096];
    const unsigned char background[4] = {0x23, 0x45, 0x67, 0x89};
    const unsigned corner_offsets[4] = {0, 63 * 4, 63 * 256, 63 * 256 + 63 * 4};
    const uint32_t corner_values[4] = {0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff01};
    int error = 0, cleanup = 0, pending = 0, locked = 0, safe = 1;
    int imported = 0, released = 0, map_null = 0, cp_completed = 0;
    int qualified = 0, exported = 0;
    unsigned completed = 0, cp_submissions = 0, draw_submissions = 0, timestamp = 0;
    unsigned cp_checks = 0, channel_checks = 0, lock_count = 0, unlock_count = 0;
    unsigned cache_rejections = 0, busy_rejections = 0;
    uint64_t guard_checks = 0, readonly_checks = 0;
    uint32_t chip = 0, flags = 0;
    size_t gmem = 0, command_bytes = 0;
    unsigned bad_case = ~0u, bad_x = ~0u, bad_y = ~0u, bad_channel = ~0u, bad_kind = ~0u;
    unsigned bad_actual = 0, bad_expected = 0;
    size_t bad_offset = SIZE_MAX;
#define CHECK(expression) do { int e_ = (expression); if (e_) { error = e_; goto done; } } while (0)
#define REQUIRE(expression) do { if (!(expression)) { error = ENOTSUP; goto done; } } while (0)
#define UNLOCK() do { int e_ = nn_ahb_unlock(owner); locked = 0; \
    if (e_) { safe = 0; error = e_; goto done; } ++unlock_count; } while (0)
    if (out) *out = NULL;
    if (!json || !capacity) return -1;
    if (export_frame && !out) { error = EINVAL; goto done; }
    REQUIRE(directory && image->schema == 2 && image->profile == NN_GFX_COLOR &&
        image->chip == 0x06050002 && image->width == 64 && image->height == 64 &&
        image->target_pitch == 256 && image->target_bytes == 16384 &&
        image->vertex_bytes == 72 && image->matrix_bytes == 64 &&
        image->target_alignment && image->vs_code && image->fs_code &&
        image->vs_code_bytes && image->fs_code_bytes && image->vs_code_bytes <= 65536 &&
        image->fs_code_bytes <= 65536 && sysconf(_SC_PAGESIZE) == 4096);
    stage = "open"; CHECK(nn_device_open(&device));
    chip = nn_device_chip(device); gmem = nn_device_gmem_bytes(device);
    REQUIRE(chip == image->chip && gmem == image->gmem_bytes);
    stage = "ahb-create"; CHECK(nn_ahb_create(&owner));
    const AHardwareBuffer_Desc *description = nn_ahb_description(owner);
    REQUIRE(description && description->width == 64 && description->height == 64 && description->layers == 1 &&
        description->format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM && description->stride == 64 &&
        (description->usage & NN_AHB_USAGE) == NN_AHB_USAGE);
    /* Publish a nonuniform CPU pattern before any GPU write. The independent
     * shadow is modified for expected CP writes only AFTER this lock ends. */
    for (size_t i = 0; i < sizeof(shadow); ++i) shadow[i] = (unsigned char)(i * 73u + (i >> 7) * 19u + 0x35u);
    stage = "cp-initialize-lock";
    CHECK(nn_ahb_lock(owner, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, &view)); locked = 1; ++lock_count;
    CHECK(ahb_probe_view(&view, NULL));
    memcpy(view.address, shadow, sizeof(shadow));
    UNLOCK();
    stage = "identify-pixels"; CHECK(nn_ahb_get_pixel(owner, &pixel));
    REQUIRE(pixel.fd >= 0 && pixel.fd_index >= 0 && pixel.width == 64 && pixel.height == 64 && pixel.pitch == 256 &&
        pixel.bytes && pixel.bytes <= SIZE_MAX && pixel.offset <= SIZE_MAX &&
        pixel.offset <= pixel.bytes && sizeof(shadow) <= pixel.bytes - pixel.offset &&
        pixel.offset % image->target_alignment == 0);
    stage = "dmabuf-import";
    CHECK(nn_bo_import_dmabuf(device, pixel.fd, (size_t)pixel.bytes, &resources[NN_GFX_TARGET])); imported = 1;
    REQUIRE(nn_bo_size(resources[NN_GFX_TARGET]) == pixel.bytes);
    REQUIRE(nn_bo_address(resources[NN_GFX_TARGET]) <= UINT64_MAX - pixel.offset - sizeof(shadow));
    REQUIRE((nn_bo_address(resources[NN_GFX_TARGET]) + pixel.offset) % image->target_alignment == 0);
    map_null = nn_bo_map(resources[NN_GFX_TARGET]) == NULL; REQUIRE(map_null);
    flags = nn_bo_flags(resources[NN_GFX_TARGET]);
    for (int direction = 0; direction < 2; ++direction) {
        REQUIRE(nn_bo_cache(resources[NN_GFX_TARGET], (size_t)pixel.offset, sizeof(shadow), direction) == EOPNOTSUPP);
        ++cache_rejections;
    }
    buffers[NN_GFX_TARGET] = (struct nn_gfx_buffer){resources[NN_GFX_TARGET], (size_t)pixel.offset, sizeof(shadow)};
    stage = "allocate-shader-inputs";
    const size_t allocation[NN_GFX_ZERO] = {image->vs_code_bytes, image->fs_code_bytes, 0, 4096, 4096};
    for (unsigned i = 0; i < NN_GFX_ZERO; ++i) {
        if (i == NN_GFX_TARGET) continue;
        CHECK(nn_bo_alloc(device, allocation[i], resources + i));
        REQUIRE(nn_bo_map(resources[i]) != NULL);
        memset(nn_bo_map(resources[i]), 0xa5, nn_bo_size(resources[i]));
        expected_resources[i] = malloc(nn_bo_size(resources[i]));
        if (!expected_resources[i]) { error = ENOMEM; goto done; }
        buffers[i] = (struct nn_gfx_buffer){resources[i], 0, nn_bo_size(resources[i])};
    }
    memcpy(nn_bo_map(resources[NN_GFX_VS_CODE]), image->vs_code, image->vs_code_bytes);
    memcpy(nn_bo_map(resources[NN_GFX_FS_CODE]), image->fs_code, image->fs_code_bytes);
    buffers[NN_GFX_VERTICES].offset = 256; buffers[NN_GFX_VERTICES].bytes = image->vertex_bytes;
    buffers[NN_GFX_MATRIX].offset = 64; buffers[NN_GFX_MATRIX].bytes = image->matrix_bytes;
    stage = "encode"; CHECK(nn_gfx_encode(device, image, buffers, &draw_job, &command_bytes));
    expected_commands = malloc(command_bytes);
    if (!expected_commands) { error = ENOMEM; goto done; }
    memcpy(expected_commands, nn_bo_map(draw_job), command_bytes);
    stage = "cp-encode"; CHECK(nn_bo_alloc(device, sizeof(cp_commands), &cp_job));
    REQUIRE(nn_bo_map(cp_job) && nn_bo_size(cp_job) == sizeof(cp_commands));
    memset(nn_bo_map(cp_job), 0xa5, nn_bo_size(cp_job));
    uint32_t *words = nn_bo_map(cp_job);
    for (unsigned i = 0; i < 4; ++i) {
        uint64_t address = nn_bo_address(resources[NN_GFX_TARGET]) + pixel.offset + corner_offsets[i];
        words[i * 4] = nvn_pm4_packet7(NVN_CP_MEM_WRITE, 3);
        words[i * 4 + 1] = (uint32_t)address; words[i * 4 + 2] = (uint32_t)(address >> 32);
        words[i * 4 + 3] = corner_values[i];
        memcpy(shadow + corner_offsets[i], corner_values + i, 4);
    }
    words[16] = nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE, 0);
    words[17] = nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME, 0);
    memcpy(cp_commands, words, sizeof(cp_commands));
    stage = "context"; CHECK(nn_queue_create(device, &queue));
    stage = "cp-submit"; submission = "unknown"; pending = 1;
    CHECK(nn_queue_submit(queue, cp_job, 0, 18 * sizeof(uint32_t), resources + NN_GFX_TARGET, 1, &timestamp));
    submission = "accepted"; ++cp_submissions;
    REQUIRE(nn_bo_destroy(resources[NN_GFX_TARGET]) == EBUSY); ++busy_rejections;
    stage = "cp-wait"; CHECK(nn_queue_finish(queue, 1000)); pending = 0;
    stage = "cp-read-lock";
    CHECK(nn_ahb_lock(owner, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, &view)); locked = 1; ++lock_count;
    CHECK(ahb_probe_view(&view, &pixel));
    stage = "cp-payload";
    for (size_t i = 0; i < sizeof(shadow); ++i) {
        unsigned actual = ((const unsigned char *)view.address)[i];
        if (actual != shadow[i]) { bad_offset = i; bad_actual = actual; bad_expected = shadow[i]; error = EIO; goto done; }
        ++cp_checks;
    }
    UNLOCK(); cp_completed = 1;
    for (unsigned test = 0; test < 4; ++test) {
        /* Same source-shader algebra and declared profiles as graphics_probe.c:
         * fullscreen red; right-half green; clipped away; interpolated RGB.
         * This constructs expected bytes only in independent CPU storage. */
        float matrix[16] = {0}; matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1;
        if (test == 1) matrix[3] = 1;
        if (test == 2) matrix[3] = 3;
        float vertices[3][6] = {{-1,-1,1,0,0,1}, {3,-1,1,0,0,1}, {-1,3,1,0,0,1}};
        if (test == 1 || test == 2) for (unsigned v = 0; v < 3; ++v) { vertices[v][2] = 0; vertices[v][3] = 1; }
        if (test == 3) { vertices[1][2] = 0; vertices[1][3] = 1; vertices[2][2] = 0; vertices[2][4] = 1; }
        memcpy((char *)nn_bo_map(resources[NN_GFX_VERTICES]) + buffers[NN_GFX_VERTICES].offset, vertices, sizeof(vertices));
        memcpy((char *)nn_bo_map(resources[NN_GFX_MATRIX]) + buffers[NN_GFX_MATRIX].offset, matrix, sizeof(matrix));
        for (unsigned i = 0; i < NN_GFX_ZERO; ++i) if (i != NN_GFX_TARGET)
            memcpy(expected_resources[i], nn_bo_map(resources[i]), nn_bo_size(resources[i]));
        /* CPU writes only the initial background through the allocator's lock.
         * No rendered/shadow image is ever copied into the AHB. */
        stage = "draw-initialize-lock";
        CHECK(nn_ahb_lock(owner, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, &view)); locked = 1; ++lock_count;
        CHECK(ahb_probe_view(&view, &pixel));
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x)
            memcpy((char *)view.address + y * pixel.pitch + x * 4, background, sizeof(background));
        UNLOCK();
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x) {
            unsigned expected[4] = {255,0,0,255};
            if (test == 1 && x >= 32) { expected[0] = 0; expected[1] = 255; }
            if (test == 2 || (test == 1 && x < 32)) for (unsigned c = 0; c < 4; ++c) expected[c] = background[c];
            if (test == 3) {
                float green = (x + 0.5f) / 128.0f, blue = (y + 0.5f) / 128.0f;
                expected[0] = ahb_probe_rounded(1 - green - blue);
                expected[1] = ahb_probe_rounded(green); expected[2] = ahb_probe_rounded(blue);
            }
            for (unsigned c = 0; c < 4; ++c) shadow[y * pixel.pitch + x * 4 + c] = (unsigned char)expected[c];
        }
        stage = "draw-submit"; submission = "unknown"; pending = 1;
        CHECK(nn_queue_submit(queue, draw_job, 0, command_bytes, resources, NN_GFX_ZERO, &timestamp));
        submission = "accepted"; ++draw_submissions;
        REQUIRE(nn_bo_destroy(resources[NN_GFX_TARGET]) == EBUSY); ++busy_rejections;
        stage = "draw-wait"; CHECK(nn_queue_finish(queue, 1000)); pending = 0;
        stage = "draw-read-lock";
        CHECK(nn_ahb_lock(owner, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, &view)); locked = 1; ++lock_count;
        CHECK(ahb_probe_view(&view, &pixel));
        stage = "pixels";
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x) for (unsigned c = 0; c < 4; ++c) {
            unsigned actual = ((const unsigned char *)view.address)[y * pixel.pitch + x * 4 + c];
            unsigned expected = shadow[y * pixel.pitch + x * 4 + c];
            unsigned delta = actual > expected ? actual - expected : expected - actual;
            if (delta > (test == 3 ? 1u : 0u)) {
                bad_case = test; bad_x = x; bad_y = y; bad_channel = c;
                bad_actual = actual; bad_expected = expected; error = EIO; goto save;
            }
            ++channel_checks;
        }
        stage = "guards";
        for (unsigned i = 0; i < NN_GFX_ZERO; ++i) {
            if (i == NN_GFX_TARGET) continue; /* No CPU mapping or fd-tail access. */
            const unsigned char *data = nn_bo_map(resources[i]);
            size_t begin = buffers[i].offset, end = begin +
                (i == NN_GFX_VS_CODE ? image->vs_code_bytes : i == NN_GFX_FS_CODE ? image->fs_code_bytes :
                 i == NN_GFX_VERTICES ? sizeof(vertices) : sizeof(matrix));
            for (size_t byte = 0; byte < nn_bo_size(resources[i]); ++byte) {
                if (data[byte] != expected_resources[i][byte]) {
                    bad_case = test; bad_kind = i; bad_offset = byte;
                    bad_actual = data[byte]; bad_expected = expected_resources[i][byte]; error = EIO; goto save;
                }
                if (byte < begin || byte >= end) ++guard_checks; else ++readonly_checks;
            }
        }
        for (size_t byte = 0; byte < sizeof(cp_commands); ++byte) {
            unsigned actual = ((const unsigned char *)nn_bo_map(cp_job))[byte];
            if (actual != cp_commands[byte]) {
                bad_case = test; bad_offset = byte; bad_actual = actual; bad_expected = cp_commands[byte]; error = EIO; goto save;
            }
            if (byte >= 18 * sizeof(uint32_t)) ++guard_checks; else ++readonly_checks;
        }
        for (size_t byte = 0; byte < command_bytes; ++byte) {
            unsigned actual = ((const unsigned char *)nn_bo_map(draw_job))[byte];
            if (actual != expected_commands[byte]) {
                bad_case = test; bad_offset = byte; bad_actual = actual; bad_expected = expected_commands[byte]; error = EIO; goto save;
            }
            ++readonly_checks;
        }
save:;
        char path[1024];
        int n = snprintf(path, sizeof(path), "%s/ahb-case-%u.rgba", directory, test);
        if (n < 0 || (size_t)n >= sizeof(path)) { if (!error) error = ENAMETOOLONG; goto done; }
        FILE *file = fopen(path, "wb");
        if (!file) { if (!error) error = errno; goto done; }
        int write_error = fwrite(view.address, 1, sizeof(shadow), file) != sizeof(shadow);
        int close_error = fclose(file);
        if (write_error || close_error) { if (!error) error = EIO; goto done; }
        if (error) goto done;
        UNLOCK(); ++completed;
    }
    stage = "complete";
done:
    /* Never unlock twice or release borrowed-fd ownership after an uncertain
     * unlock/GPU wait. Such a process is deliberately retained for teardown. */
    if (locked) {
        int e = nn_ahb_unlock(owner); locked = 0;
        if (e) { safe = 0; if (!error) error = e; }
        else ++unlock_count;
    }
    qualified = cp_completed && cp_checks == 16384 && completed == 4 &&
        channel_checks == 65536 && cp_submissions == 1 && draw_submissions == 4 &&
        lock_count == 10 && unlock_count == 10 && cache_rejections == 2 && busy_rejections == 5 &&
        map_null && guard_checks && readonly_checks;
    if (pending) cleanup = EBUSY;
    else {
        if (queue) { int e = nn_queue_destroy(queue); if (e && !cleanup) cleanup = e; else if (!e) queue = NULL; }
        if (draw_job) { int e = nn_bo_destroy(draw_job); if (e && !cleanup) cleanup = e; if (!e) draw_job = NULL; }
        if (cp_job) { int e = nn_bo_destroy(cp_job); if (e && !cleanup) cleanup = e; if (!e) cp_job = NULL; }
        for (unsigned i = 0; i < NN_GFX_ZERO; ++i) if (resources[i]) {
            int e = nn_bo_destroy(resources[i]);
            if (e && !cleanup) cleanup = e;
            if (!e) resources[i] = NULL;
        }
        if (device) { int e = nn_device_close(device); if (e && !cleanup) cleanup = e; if (!e) device = NULL; }
        /* A frame is exportable only after the complete qualification AND
         * every KGSL release succeeded. No KGSL object refers to this owner.
         * Transfer itself is deferred until JSON formatting also succeeds. */
        exported = export_frame && out && qualified && !error && !cleanup && safe &&
            owner && !queue && !draw_job && !cp_job && !device;
        for (unsigned i = 0; i < NN_GFX_ZERO; ++i) if (resources[i]) exported = 0;
        if (exported) {
            /* Preserve the original GPU image; no pixel lock or CPU copy. */
        } else if (owner && safe && !queue && !resources[NN_GFX_TARGET] && (!export_frame || !cleanup)) {
            int e = nn_ahb_destroy(owner);
            if (e && !cleanup) cleanup = e;
            if (!e) { owner = NULL; released = 1; }
        } else if (owner && !cleanup) cleanup = EBUSY;
    }
    for (unsigned i = 0; i < NN_GFX_ZERO; ++i) free(expected_resources[i]);
    free(expected_commands);
    int passed = !error && !cleanup && qualified && (export_frame ? exported : released);
    int n = snprintf(json, capacity,
        "{\"schema\":1,\"operation\":\"%s\",\"uid\":%u,\"stage\":\"%s\","
        "\"errno\":%d,\"cleanup_errno\":%d,\"chip_id\":%u,\"gmem_bytes\":%zu,"
        "\"submission\":\"%s\",\"timestamp\":%u,\"dmabuf_imported\":%s,\"borrowed_fd_index\":%d,"
        "\"fd_bytes\":%" PRIu64 ",\"pixel_offset\":%" PRIu64 ",\"bo_flags\":%u,"
        "\"width\":64,\"height\":64,\"pitch\":256,\"target_bytes\":16384,"
        "\"cp_completed\":%s,\"cp_submissions\":%u,\"cp_payload_byte_checks\":%u,\"cp_corner_writes\":%u,"
        "\"draw_submissions\":%u,\"completed_cases\":%u,\"channel_checks\":%u,\"reused_ib\":%s,"
        "\"guard_byte_checks\":%" PRIu64 ",\"readonly_byte_checks\":%" PRIu64 ",\"ahb_outside_payload_checked\":false,"
        "\"cpu_lock_count\":%u,\"cpu_unlock_count\":%u,\"imported_map_null\":%s,\"cache_rejections\":%u,"
        "\"inflight_destroy_rejections\":%u,\"buffer_released\":%s,%s\"resources_retained\":%s,"
        "\"display_presented\":false,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,"
        "\"bad_kind\":%u,\"bad_offset\":%zu,\"bad_actual\":%u,\"bad_expected\":%u,\"passed\":%s}\n",
        export_frame ? "kgsl_ahb_present_frame" : "kgsl_ahb_graphics",
        (unsigned)getuid(), stage, error, cleanup, chip, gmem, submission, timestamp, imported ? "true" : "false", pixel.fd_index,
        pixel.bytes, pixel.offset, flags, cp_completed ? "true" : "false", cp_submissions, cp_checks, cp_completed ? 4u : 0u,
        draw_submissions, completed, channel_checks, draw_submissions > 1 ? "true" : "false", guard_checks, readonly_checks,
        lock_count, unlock_count, map_null ? "true" : "false", cache_rejections, busy_rejections,
        released ? "true" : "false", export_frame ? (exported ? "\"buffer_exported\":true," : "\"buffer_exported\":false,") : "",
        (owner && !exported) || pending || cleanup ? "true" : "false",
        bad_case, bad_x, bad_y, bad_channel, bad_kind, bad_offset, bad_actual, bad_expected, passed ? "true" : "false");
#undef UNLOCK
#undef REQUIRE
#undef CHECK
    if (n < 0 || (size_t)n >= capacity) {
        /* Never return an owner without its complete qualification result.
         * All GPU use is finished here; a failed destroy remains retained. */
        if (exported) (void)nn_ahb_destroy(owner);
        return -1;
    }
    if (!passed) return -1;
    if (exported) { *out = owner; owner = NULL; }
    return 0;
}

int nn_ahb_gpu_probe(const char *directory, char *json, size_t capacity)
{
    return ahb_gpu_run(directory, json, capacity, 0, NULL);
}

int nn_ahb_gpu_frame(const char *directory, char *json, size_t capacity, nn_ahb **out)
{
    return ahb_gpu_run(directory, json, capacity, 1, out);
}
