#define _POSIX_C_SOURCE 200112L
#include "nvn_native/nvn.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int nn_nvn_api_probe(char *json, size_t capacity)
{
    const char *stage = "resolver";
    int failed = 0, busy_rejected = 0, guard_ok = 0, result_ok = 0, imported = 0, budget_rejected = 0;
#define ENTRY(name, type) type name = (type)nvnBootstrapLoader(#name); if (!name) { failed = ENOSYS; goto done; }
#include "../src/nvn_entries.inc"
#undef ENTRY
    NVNdeviceBuilder db; NVNdevice device; NVNqueueBuilder qb; NVNqueue queue;
    NVNmemoryPoolBuilder pb; NVNmemoryPool data_pool, command_pool;
    NVNbufferBuilder bb;
    struct { uint64_t before; NVNbuffer value; uint64_t after; } a, b;
    NVNcommandBuffer command;
    _Alignas(16) unsigned char control[512];
    unsigned char *data = NULL, *memory = NULL;
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    if (page != 4096 || posix_memalign((void **)&data, page, page) ||
        posix_memalign((void **)&memory, page, page)) { failed = ENOMEM; goto done; }
    memset(data, 0xa5, page); memset(memory, 0xa5, page);
    memset(&a, 0x5a, sizeof(a)); memset(&b, 0x5a, sizeof(b)); memset(control, 0xa5, sizeof(control));
    nn_nvn_clear_error();
    if (nvnBootstrapLoader("nvnUnknownEntryMustNotSucceed")) { failed = EINVAL; goto done; }
    stage = "device";
    nvnDeviceBuilderSetDefaults(&db);
    if (!nvnDeviceInitialize(&device, &db)) goto done;
    if (nvnDeviceGetProcAddress(&device, "nvnBufferMap") != (PFNNVNGENERICFUNCPTRPROC)nvnBufferMap) { failed = EINVAL; goto done; }
    stage = "queue";
    nvnQueueBuilderSetDefaults(&qb); nvnQueueBuilderSetDevice(&qb, &device);
    if (!nvnQueueInitialize(&queue, &qb)) goto done;
    stage = "import_data_pool";
    nvnMemoryPoolBuilderSetDefaults(&pb); nvnMemoryPoolBuilderSetDevice(&pb, &device);
    nvnMemoryPoolBuilderSetStorage(&pb, data, page);
    nvnMemoryPoolBuilderSetFlags(&pb, NVN_MEMORY_POOL_FLAGS_CPU_CACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!nvnMemoryPoolInitialize(&data_pool, &pb)) goto done;
    stage = "import_command_pool";
    nvnMemoryPoolBuilderSetStorage(&pb, memory, page);
    if (!nvnMemoryPoolInitialize(&command_pool, &pb)) goto done;
    imported = 1;
    stage = "buffer_aliases";
    nvnBufferBuilderSetDefaults(&bb); nvnBufferBuilderSetDevice(&bb, &device);
    nvnBufferBuilderSetStorage(&bb, &data_pool, 128, 64);
    if (!nvnBufferInitialize(&a.value, &bb)) goto done;
    nvnBufferBuilderSetStorage(&bb, &data_pool, 160, 32);
    if (!nvnBufferInitialize(&b.value, &bb)) goto done;
    if (nvnBufferMap(&a.value) != data + 128 || nvnBufferMap(&b.value) != data + 160 ||
        nvnMemoryPoolMap(&data_pool) != data ||
        nvnBufferGetAddress(&b.value) - nvnBufferGetAddress(&a.value) != 32) { failed = EINVAL; goto done; }
    nvnMemoryPoolFlushMappedRange(&data_pool, 0, page);
    stage = "record";
    if (!nvnCommandBufferInitialize(&command, &device)) goto done;
    nvnCommandBufferAddCommandMemory(&command, &command_pool, 0, page);
    nvnCommandBufferAddControlMemory(&command, control, 16);
    nvnCommandBufferBeginRecording(&command);
    budget_rejected = nn_nvn_error() == ENOSPC;
    if (!budget_rejected) { failed = EINVAL; goto done; }
    nn_nvn_clear_error();
    nvnCommandBufferAddControlMemory(&command, control, sizeof(control));
    nvnCommandBufferBeginRecording(&command);
    nvnCommandBufferClearBuffer(&command, nvnBufferGetAddress(&a.value) + 16, 32, 0x11223344);
    nvnCommandBufferClearBuffer(&command, nvnBufferGetAddress(&b.value), 16, 0x55667788);
    NVNcommandHandle handles[2]; handles[0] = nvnCommandBufferEndRecording(&command);
    nvnCommandBufferBeginRecording(&command);
    nvnCommandBufferClearBuffer(&command, nvnBufferGetAddress(&a.value) + 48, 16, 0xaabbccdd);
    handles[1] = nvnCommandBufferEndRecording(&command);
    if (!handles[0] || !handles[1] || nn_nvn_error()) goto done;
    if (nvnCommandBufferGetCommandMemoryUsed(&command) != 116 ||
        nvnCommandBufferGetCommandMemoryFree(&command) != page - 116 ||
        nvnCommandBufferGetControlMemoryUsed(&command) != 48 ||
        nvnCommandBufferGetControlMemoryFree(&command) != sizeof(control) - 48) { failed = EINVAL; goto done; }
    stage = "submit";
    nvnQueueSubmitCommands(&queue, 2, handles);
    if (nn_nvn_error()) goto done;
    // Destroy CPU recording objects/handles before waiting: submitted BOs remain pinned.
    nvnDeviceFinalizeCommandHandle(&device, handles[0]);
    nvnDeviceFinalizeCommandHandle(&device, handles[1]);
    nvnCommandBufferFinalize(&command); nvnBufferFinalize(&a.value); nvnBufferFinalize(&b.value);
    if (nn_nvn_error()) goto done;
    stage = "inflight_lifetime";
    nvnMemoryPoolFinalize(&data_pool);
    busy_rejected = nn_nvn_error() == EBUSY;
    if (!busy_rejected) { failed = EINVAL; goto done; }
    nn_nvn_clear_error();
    nvnMemoryPoolFinalize(&command_pool);
    busy_rejected = nn_nvn_error() == EBUSY;
    if (!busy_rejected) { failed = EINVAL; goto done; }
    nn_nvn_clear_error();
    stage = "finish";
    nvnQueueFinish(&queue);
    if (nn_nvn_error()) goto done;
    nvnMemoryPoolInvalidateMappedRange(&data_pool, 0, page);
    if (nn_nvn_error()) goto done;
    stage = "readback";
    result_ok = 1;
    for (size_t i = 0; i < page; i += 4) {
        uint32_t value; memcpy(&value, data + i, sizeof(value));
        uint32_t expected = i >= 144 && i < 160 ? 0x11223344 :
            (i >= 160 && i < 176 ? 0x55667788 : (i >= 176 && i < 192 ? 0xaabbccdd : 0xa5a5a5a5));
        if (value != expected) result_ok = 0;
    }
    guard_ok = 1;
    for (size_t i = 0; i < sizeof(a); i++) if (((unsigned char *)&a)[i] != 0x5a) guard_ok = 0;
    for (size_t i = 0; i < sizeof(b); i++) if (((unsigned char *)&b)[i] != 0x5a) guard_ok = 0;
    for (size_t i = 48; i < sizeof(control); i++) if (control[i] != 0xa5) guard_ok = 0;
    for (size_t i = 116; i < page; i++) if (memory[i] != 0xa5) guard_ok = 0;
    if (!result_ok || !guard_ok) { failed = EIO; goto done; }
    stage = "cleanup";
    nvnMemoryPoolFinalize(&data_pool); nvnMemoryPoolFinalize(&command_pool);
    nvnQueueFinalize(&queue); nvnDeviceFinalize(&device);
    if (nn_nvn_error()) goto done;
    free(data); free(memory);
    stage = "complete";
done:
    if (!failed) failed = nn_nvn_error();
    if (strcmp(stage, "complete") && !failed) failed = EIO;
    snprintf(json, capacity, "{\"schema\":1,\"operation\":\"nvn_clear_buffer\",\"uid\":%u,"
        "\"stage\":\"%s\",\"errno\":%d,\"imported_storage\":%s,"
        "\"overlapping_views_correct\":%s,\"object_guards_intact\":%s,"
        "\"inflight_destroy_rejected\":%s,\"control_budget_rejected\":%s,\"passed\":%s}\n", (unsigned)getuid(), stage, failed,
        imported ? "true" : "false", result_ok ? "true" : "false", guard_ok ? "true" : "false",
        busy_rejected ? "true" : "false", budget_rejected ? "true" : "false", failed ? "false" : "true");
    return failed ? -1 : 0;
}
