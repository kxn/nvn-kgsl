#define _POSIX_C_SOURCE 200112L
#include "nvn_native/nvn.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_MAXWELL_HEADER

static uint64_t input(unsigned i)
{
    static const uint64_t edges[] = {0, UINT64_MAX, 0xffffffff, UINT64_C(0x100000000),
                                    UINT64_C(0x8000000000000000), UINT64_C(0x7fffffffffffffff)};
    return edges[i % 6] ^ (UINT64_C(0x123456789abcdef) * (i / 6));
}
int nn_maxwell_compute_probe(char *json, size_t capacity)
{
    const char *stage = "resolver";
    int failed = 0, completed = 0, inflight = 0, misses = 0, replayed = 0, binding_rejections = 0;
    size_t mismatch = 0; unsigned current_program = 0, current_count = 0;
    unsigned char got = 0, want = 0;
    char cases[2048] = ""; size_t used = 0;
#define ENTRY(name, type) type name = (type)nvnBootstrapLoader(#name); if (!name) {failed=ENOSYS; goto done;}
#include "../src/nvn_entries.inc"
#undef ENTRY
    NVNdeviceBuilder db; NVNdevice device; NVNqueueBuilder qb; NVNqueue queue;
    NVNmemoryPoolBuilder pb; NVNmemoryPool data_pool, command_pool;
    NVNprogram programs[4]; NVNcommandBuffer command;
    _Alignas(16) unsigned char control[256];
    unsigned char *data = NULL, *commands = NULL, expected[16384];
    const size_t bytes = sizeof(expected), source_offset = 260, target_offset = 4356;
    const size_t c3_offset = 10240, code_offset = 12288;
    if (sysconf(_SC_PAGESIZE) != 4096 || posix_memalign((void **)&data, 4096, bytes) ||
        posix_memalign((void **)&commands, 4096, bytes)) { failed=ENOMEM; goto done; }
    nn_nvn_clear_error();
    stage = "initialize";
    nvnDeviceBuilderSetDefaults(&db); if (!nvnDeviceInitialize(&device, &db)) goto done;
    nvnQueueBuilderSetDefaults(&qb); nvnQueueBuilderSetDevice(&qb, &device);
    if (!nvnQueueInitialize(&queue, &qb)) goto done;
    nvnMemoryPoolBuilderSetDefaults(&pb); nvnMemoryPoolBuilderSetDevice(&pb, &device);
    nvnMemoryPoolBuilderSetFlags(&pb, NVN_MEMORY_POOL_FLAGS_CPU_CACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED |
                                    NVN_MEMORY_POOL_FLAGS_SHADER_CODE);
    nvnMemoryPoolBuilderSetStorage(&pb, data, bytes); if (!nvnMemoryPoolInitialize(&data_pool, &pb)) goto done;
    nvnMemoryPoolBuilderSetFlags(&pb, NVN_MEMORY_POOL_FLAGS_CPU_CACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    nvnMemoryPoolBuilderSetStorage(&pb, commands, bytes); if (!nvnMemoryPoolInitialize(&command_pool, &pb)) goto done;
    uint64_t base = nvnMemoryPoolGetBufferAddress(&data_pool);
    for (unsigned p = 0; p < 4; p++) {
        stage = "program";
        if ((failed = nn_nvn_register_compute(&device, test_programs[p]))) goto done;
        if (!nvnProgramInitialize(&programs[p], &device)) goto done;
        memcpy(data + code_offset, test_programs[p]->source_code, test_programs[p]->source_bytes);
        NVNshaderData shader = {base + code_offset, test_programs[p]->source_control};
        data[code_offset] ^= 1;
        if (nvnProgramSetShaders(&programs[p], 1, &shader) || nn_nvn_error() != ENOTSUP) {failed=EINVAL; goto done;}
        misses++; nn_nvn_clear_error(); data[code_offset] ^= 1;
        if (!nvnProgramSetShaders(&programs[p], 1, &shader)) goto done;
    }
    for (unsigned p = 0; p < 4; p++) for (unsigned count_case = 0; count_case < 3; count_case++) {
        const unsigned counts[] = {1,33,65}; unsigned count = counts[count_case];
        current_program = p; current_count = count;
        memset(data, 0xa5, bytes); memset(commands, 0xa5, bytes);
        memcpy(data + code_offset, test_programs[p]->source_code, test_programs[p]->source_bytes);
        uint64_t addresses[] = {base + source_offset, base + target_offset};
        uint32_t sizes[] = {count * test_oracles[p].stride, count * 8};
        memset(data + c3_offset, 0, 64); // Changed after recording to test GPU consumption.
        stage = "record";
        if (!nvnCommandBufferInitialize(&command, &device)) goto done;
        nvnCommandBufferAddCommandMemory(&command, &command_pool, 0, bytes);
        nvnCommandBufferAddControlMemory(&command, control, sizeof(control));
        nvnCommandBufferBeginRecording(&command);
        nvnCommandBufferBindProgram(&command, &programs[p], NVN_SHADER_STAGE_COMPUTE_BIT);
        NVNbufferRange uniform = {base + c3_offset, 64};
        nvnCommandBufferBindUniformBuffers(&command, NVN_SHADER_STAGE_COMPUTE, 0, 1, &uniform);
        NVNbufferRange storage[2] = {{addresses[0], sizes[0]}, {addresses[1], sizes[1]}};
        if (count_case & 1) {
            nvnCommandBufferBindStorageBuffers(&command, NVN_SHADER_STAGE_COMPUTE, 0, 2, storage);
        } else {
            nvnCommandBufferBindStorageBuffer(&command, NVN_SHADER_STAGE_COMPUTE, 0, storage[0].address, storage[0].size);
            nvnCommandBufferBindStorageBuffer(&command, NVN_SHADER_STAGE_COMPUTE, 1, storage[1].address, storage[1].size);
        }
        if (p == 0 && count_case == 0) {
            nvnCommandBufferBindUniformBuffer(&command, NVN_SHADER_STAGE_COMPUTE, 14, uniform.address, uniform.size);
            if (nn_nvn_error() != EINVAL) {failed=EINVAL; goto done;}
            binding_rejections++; nn_nvn_clear_error();
            // A valid first range followed by an invalid second range must
            // leave both original bindings intact (verified by GPU readback).
            NVNbufferRange bad[2] = {storage[1], {UINT64_MAX - 3,4}};
            nvnCommandBufferBindStorageBuffers(&command, NVN_SHADER_STAGE_COMPUTE, 0, 2, bad);
            if (nn_nvn_error() != EINVAL) {failed=EINVAL; goto done;}
            binding_rejections++; nn_nvn_clear_error();
            nvnCommandBufferBindStorageBuffer(&command, NVN_SHADER_STAGE_COMPUTE, 1, 0, 0);
            nvnCommandBufferDispatchCompute(&command, 1,1,1);
            if (nn_nvn_error() != EINVAL || nvnCommandBufferGetCommandMemoryUsed(&command)) {failed=EINVAL; goto done;}
            binding_rejections++; nn_nvn_clear_error();
            nvnCommandBufferBindStorageBuffer(&command, NVN_SHADER_STAGE_COMPUTE, 1, storage[1].address, storage[1].size);
        }
        nvnCommandBufferDispatchCompute(&command, (count + 31) / 32, 1, 1);
        NVNcommandHandle handle = nvnCommandBufferEndRecording(&command);
        if (!handle || nn_nvn_error()) goto done;
        // No shader substitute: the CPU prepares inputs and checks copy/u64
        // difference kernels. All outputs are written by original GPU shaders.
        for (unsigned i = 0; i < sizes[0] / 8; i++) {
            uint64_t value = input(i); memcpy(data + source_offset + i * 8, &value, 8);
        }
        memcpy(data + c3_offset, &count, 4);
        memcpy(expected, data, bytes);
        for (unsigned i = 0; i < count; i++) {
            size_t offset = source_offset + i * test_oracles[p].stride + test_oracles[p].offset;
            uint64_t a, b; memcpy(&a, data + offset, 8);
            if (test_oracles[p].delta) {memcpy(&b, data + offset + 16, 8); a = b - a;}
            memcpy(expected + target_offset + i * 8, &a, 8);
        }
        nvnMemoryPoolFlushMappedRange(&data_pool, 0, bytes);
        stage = "submit";
        nvnQueueSubmitCommands(&queue, 1, &handle); if (nn_nvn_error()) goto done;
        if (count_case != 2) nvnDeviceFinalizeCommandHandle(&device, handle);
        nvnCommandBufferFinalize(&command);
        if (nn_nvn_error()) goto done;
        nvnProgramFinalize(&programs[p]);
        if (nn_nvn_error() != EBUSY) {failed=EINVAL; goto done;}
        inflight++; nn_nvn_clear_error();
        stage = "finish";
        nvnQueueFinish(&queue); if (nn_nvn_error()) goto done;
        nvnMemoryPoolInvalidateMappedRange(&data_pool, 0, bytes); if (nn_nvn_error()) goto done;
        stage = "readback";
        for (size_t i = 0; i < bytes; i++) if (data[i] != expected[i]) {
            mismatch = i; got = data[i]; want = expected[i]; failed=EIO; goto done;
        }
        completed++;
        used += snprintf(cases + used, sizeof(cases) - used, "%s{\"program\":%u,\"count\":%u,\"passed\":true}", used ? "," : "", p, count);
        if (count_case == 2) {
            stage = "replay";
            const uint32_t replay_count = 33;
            memcpy(data + c3_offset, &replay_count, 4);
            memset(data + target_offset, 0xa5, sizes[1]);
            for (unsigned i = 0; i < sizes[0] / 8; i++) {
                uint64_t value; memcpy(&value, data + source_offset + i * 8, 8);
                value ^= UINT64_C(0xfedcba9801234567);
                memcpy(data + source_offset + i * 8, &value, 8);
            }
            memcpy(expected, data, bytes);
            for (unsigned i = 0; i < replay_count; i++) {
                size_t offset = source_offset + i * test_oracles[p].stride + test_oracles[p].offset;
                uint64_t a,b; memcpy(&a, data + offset, 8);
                if (test_oracles[p].delta) {memcpy(&b, data + offset + 16, 8); a=b-a;}
                memcpy(expected + target_offset + i * 8, &a, 8);
            }
            nvnMemoryPoolFlushMappedRange(&data_pool, 0, bytes);
            nvnQueueSubmitCommands(&queue, 1, &handle);
            nvnDeviceFinalizeCommandHandle(&device, handle);
            nvnQueueFinish(&queue); if (nn_nvn_error()) goto done;
            nvnMemoryPoolInvalidateMappedRange(&data_pool, 0, bytes); if (nn_nvn_error()) goto done;
            for (size_t i = 0; i < bytes; i++) if (data[i] != expected[i]) {
                mismatch=i; got=data[i]; want=expected[i]; failed=EIO; goto done;
            }
            replayed++;
        }
    }
    stage = "cleanup";
    for (unsigned p = 0; p < 4; p++) nvnProgramFinalize(&programs[p]);
    nvnMemoryPoolFinalize(&data_pool); nvnMemoryPoolFinalize(&command_pool);
    nvnQueueFinalize(&queue); nvnDeviceFinalize(&device);
    if (nn_nvn_error()) goto done;
    free(data); free(commands); stage = "complete";
done:
    if (!failed) failed = nn_nvn_error();
    if (strcmp(stage, "complete") && !failed) failed = EIO;
    snprintf(json, capacity, "{\"schema\":1,\"operation\":\"nvn_maxwell_compute\",\"uid\":%u,\"stage\":\"%s\","
        "\"errno\":%d,\"completed_cases\":%d,\"nvn_buffer_slots\":true,\"inflight_program_destroy_rejections\":%d,\"input_miss_rejections\":%d,"
        "\"repeated_handle_cases\":%d,\"binding_rejections\":%d,"
        "\"program\":%u,\"count\":%u,\"mismatch_offset\":%zu,\"observed_byte\":%u,\"expected_byte\":%u,"
        "\"cases\":[%s],\"passed\":%s}\n", (unsigned)getuid(), stage, failed, completed, inflight, misses,
        replayed, binding_rejections, current_program, current_count, mismatch, got, want, cases, failed ? "false" : "true");
    return failed ? -1 : 0;
}
