#include "nvn_native/probe.h"
#include "nvn_native/pm4.h"
#include "nvn_native/kgsl.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef NVN_CS_HEADER
#include NVN_CS_HEADER
#endif

int nn_device_info_probe(char *json, size_t capacity)
{
    struct nn_device_identity identity;
    int e = nn_device_inspect(&identity);
    int n = snprintf(json, capacity,
        "{\"schema\":1,\"operation\":\"kgsl_device_info\",\"uid\":%u,\"errno\":%d,"
        "\"chip_id\":%u,\"gpu_id\":%u,\"device_id\":%u,\"mmu_enabled\":%u,"
        "\"page_bytes\":%zu,\"gmem_bytes\":%zu,\"gpu_commands_submitted\":false,\"passed\":%s}\n",
        (unsigned)getuid(), e, identity.chip_id, identity.gpu_id, identity.device_id,
        identity.mmu_enabled, identity.page_bytes, identity.gmem_bytes, e ? "false" : "true");
    return !e && n >= 0 && (size_t)n < capacity ? 0 : -1;
}

int nvn_kgsl_probe(char *json, size_t capacity)
{
    const char *stage = "open", *submission = "not_attempted", *operation = "cp_mem_write";
    int error = 0, cleanup = 0, completed = 0, passed = 0;
    nn_device *device = NULL; nn_queue *queue = NULL; nn_bo *bo = NULL;
    uint32_t chip = 0, flags = 0, timestamp = 0, observed = 0, expected = 0x4e564e31;
    size_t page = 0, allocation = 0, bytes = 0, gmem = 0;
    uint32_t output_offset = 256, command_bytes = 24;
    if ((error = nn_device_open(&device))) goto done;
    chip = nn_device_chip(device); page = nn_device_page(device); allocation = page; gmem = nn_device_gmem_bytes(device);
#ifdef NVN_CS_HEADER
    operation = "compute_shader"; allocation = NVN_CS_BO_BYTES;
    output_offset = NVN_CS_OUTPUT_OFFSET; expected = NVN_CS_EXPECTED;
    command_bytes = NVN_CS_COMMAND_DWORDS * sizeof(uint32_t);
#endif
    stage = "allocate";
    if ((error = nn_bo_alloc(device, allocation, &bo))) goto done;
    bytes = nn_bo_size(bo); flags = nn_bo_flags(bo);
    void *map = nn_bo_map(bo);
    volatile uint32_t *result = (volatile uint32_t *)((char *)map + output_offset);
#ifdef NVN_CS_HEADER
    stage = "cs_artifact";
    if (sizeof(nvn_cs_blob) > bytes || output_offset + 4 > bytes || command_bytes >= 4096) {
        error = EINVAL; goto done;
    }
    memset(map, 0, bytes); memcpy(map, nvn_cs_blob, sizeof(nvn_cs_blob));
    for (size_t i = 0; i < sizeof(nvn_cs_relocs) / sizeof(nvn_cs_relocs[0]); i++) {
        size_t offset = nvn_cs_relocs[i] * sizeof(uint32_t);
        uint64_t relative;
        if (offset + sizeof(relative) > sizeof(nvn_cs_blob)) { error = EINVAL; goto done; }
        memcpy(&relative, (char *)map + offset, sizeof(relative));
        if (relative >= bytes || nn_bo_address(bo) > UINT64_MAX - relative) { error = EINVAL; goto done; }
        uint64_t address = nn_bo_address(bo) + relative;
        memcpy((char *)map + offset, &address, sizeof(address));
    }
#else
    uint64_t dst = nn_bo_address(bo) + output_offset;
    uint32_t *cmd = map;
    cmd[0] = nvn_pm4_packet7(NVN_CP_MEM_WRITE, 3);
    cmd[1] = dst; cmd[2] = dst >> 32; cmd[3] = expected;
    cmd[4] = nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE, 0);
    cmd[5] = nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME, 0);
#endif
    *result = 0;
    stage = "context";
    if ((error = nn_queue_create(device, &queue))) goto done;
    stage = "submit"; submission = "unknown";
    if ((error = nn_queue_submit(queue, bo, 0, command_bytes, NULL, 0, &timestamp))) goto done;
    submission = "accepted"; stage = "wait";
    if ((error = nn_queue_finish(queue, 1000))) goto done;
    completed = 1; observed = *result; passed = observed == expected;
    stage = passed ? "complete" : "readback";
    if (!passed) error = EIO;
done:
    // Uncertain completion retains shared-backend ownership until the app exits.
    if (queue) { int e = nn_queue_destroy(queue); if (e) cleanup = e; }
    if (bo) { int e = nn_bo_destroy(bo); if (e) cleanup = e; }
    if (device) { int e = nn_device_close(device); if (e) cleanup = e; }
    int n = snprintf(json, capacity,
        "{\"schema\":1,\"operation\":\"%s\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,"
        "\"cleanup_errno\":%d,\"chip_id\":%u,\"page_bytes\":%zu,\"bo_bytes\":%zu,\"gmem_bytes\":%zu,"
        "\"bo_flags\":%u,\"submission\":\"%s\",\"timestamp\":%u,\"completed\":%s,"
        "\"expected\":%u,\"observed\":%u,\"passed\":%s}\n",
        operation, (unsigned)getuid(), stage, error, cleanup, chip, page, bytes, gmem, flags,
        submission, timestamp, completed ? "true" : "false", expected, observed,
        passed && !cleanup ? "true" : "false");
    return passed && !cleanup && n >= 0 && (size_t)n < capacity ? 0 : -1;
}
