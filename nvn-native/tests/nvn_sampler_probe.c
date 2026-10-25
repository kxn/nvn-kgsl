/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200112L
#include "nvn_native/nvn.h"
#include "nvn_native/profile.h"
#include "sampler_cases.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_TEXTURE_API_HEADER

#if defined(NVN_TEXTURE_NOACCESS_POOL) || defined(NVN_TEXTURE_MODES) || defined(NVN_TEXTURE_UNCACHED_POOLS) || defined(NVN_TEXTURE_DESCRIPTOR_FLAGS)
#error "Sampler qualification excludes other texture qualifiers"
#endif
#ifndef NVN_TEXTURE_NOACCESS_POOL
#define NVN_TEXTURE_NOACCESS_POOL 0
#endif
#if NVN_TEXTURE_NOACCESS_POOL != 0 && NVN_TEXTURE_NOACCESS_POOL != 1
#error "NVN_TEXTURE_NOACCESS_POOL must be 0 or 1"
#endif
#ifndef NVN_TEXTURE_MODES
#define NVN_TEXTURE_MODES 0
#endif
#if NVN_TEXTURE_MODES != 0 && NVN_TEXTURE_MODES != 1
#error "NVN_TEXTURE_MODES must be 0 or 1"
#endif

#ifndef NVN_TEXTURE_UNCACHED_POOLS
#define NVN_TEXTURE_UNCACHED_POOLS 0
#endif
#if NVN_TEXTURE_UNCACHED_POOLS != 0 && NVN_TEXTURE_UNCACHED_POOLS != 1
#error "NVN_TEXTURE_UNCACHED_POOLS must be 0 or 1"
#endif

#ifndef NVN_TEXTURE_DESCRIPTOR_FLAGS
#define NVN_TEXTURE_DESCRIPTOR_FLAGS 0
#endif
#if NVN_TEXTURE_DESCRIPTOR_FLAGS != 0 && NVN_TEXTURE_DESCRIPTOR_FLAGS != 0x12 && NVN_TEXTURE_DESCRIPTOR_FLAGS != 0x14
#error "NVN_TEXTURE_DESCRIPTOR_FLAGS must be 0, 0x12 or 0x14"
#endif
#if NVN_TEXTURE_DESCRIPTOR_FLAGS && NVN_TEXTURE_UNCACHED_POOLS
#error "Descriptor-only qualifier retains qualified cached GPU-data pools"
#endif
#if NVN_TEXTURE_DESCRIPTOR_FLAGS
#include "nvn_native/kgsl.h"
#include "nvn_native/pm4.h"
/* Qualification-only read observer. No core object introspection, no packet
 * changes, no substitute submission, no lifetime/cache synchronization changes.
 * Normal link must explicitly use --wrap=nn_queue_submit. */
static struct {
    const struct nn_gfx_image *image;
    struct { uint64_t address; size_t bytes; } ranges[4];
    unsigned ranges_count, maps, attempts, delegated, accepted, rejected;
    unsigned object_checks, private_jobs, private_bases, invalidate_skips;
    int active, last_errno;
} descriptor_audit;
static int descriptor_audit_range(uint64_t address, size_t bytes)
{
    if (!address || !bytes || address > UINT64_MAX-bytes || descriptor_audit.ranges_count>=4) return EINVAL;
    for (unsigned i=0;i<descriptor_audit.ranges_count;i++)
        if (address < descriptor_audit.ranges[i].address+descriptor_audit.ranges[i].bytes &&
            descriptor_audit.ranges[i].address < address+bytes) return EINVAL;
    unsigned n=descriptor_audit.ranges_count++;
    descriptor_audit.ranges[n].address=address; descriptor_audit.ranges[n].bytes=bytes; return 0;
}
static int descriptor_audit_excludes(uint64_t address, size_t bytes)
{
    if (!address || !bytes || address > UINT64_MAX-bytes) return EOVERFLOW;
    for (unsigned i=0;i<descriptor_audit.ranges_count;i++)
        if (address < descriptor_audit.ranges[i].address+descriptor_audit.ranges[i].bytes &&
            descriptor_audit.ranges[i].address < address+bytes) return ENOTSUP;
    return 0;
}
static int descriptor_audit_object(const nn_bo *bo)
{
    if (!bo) return EINVAL;
    int e=descriptor_audit_excludes(nn_bo_address(bo),nn_bo_size(bo));
    if (!e) ++descriptor_audit.object_checks;
    return e;
}
static int descriptor_audit_validate(nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count)
{
    const struct nn_gfx_image *image=descriptor_audit.image;
    if (!descriptor_audit.active || descriptor_audit.ranges_count!=4 || !image ||
        image->schema!=3 || image->profile!=NN_GFX_TEXTURE_2D_NORMALIZED || image->chip!=0x06050002 ||
        !image->patches || !image->patch_count || !image->command_words ||
        count!=7 || !refs || offset || bytes!=24) return EINVAL;
    int e=descriptor_audit_object(ib); if (e) return e;
    for(size_t i=0;i<count;i++) if ((e=descriptor_audit_object(refs[i]))) return e;
    if (bytes>nn_bo_size(ib) || !nn_bo_map(ib)) return EINVAL;
    const uint32_t *words=nn_bo_map(ib);
    if (words[0]!=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3) || words[3]!=image->command_words ||
        words[4]!=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0) || words[5]!=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0)) return EINVAL;
    uint64_t child=(uint64_t)words[1]|((uint64_t)words[2]<<32);
    if (child>UINT64_MAX-(uint64_t)image->command_words*4) return EOVERFLOW;
    nn_bo *job=NULL;
    for(size_t i=0;i<count;i++) if (nn_bo_address(refs[i])==child) {
        if (job || refs[i]==ib) return EINVAL;
        job=refs[i];
    }
    if (!job || (uint64_t)image->command_words*4>nn_bo_size(job) || !nn_bo_map(job)) return EINVAL;
    const uint32_t *native=nn_bo_map(job); unsigned seen=0, bases=0;
    for(unsigned i=0;i<image->patch_count;i++) {
        const struct nn_gfx_patch *p=image->patches+i;
        if (p->kind!=NN_GFX_TEXTURE_DESCRIPTORS && p->kind!=NN_GFX_SAMPLERS) continue;
        if (p->word>=image->command_words-1) return EINVAL;
        size_t table_bytes=(size_t)(p->kind==NN_GFX_TEXTURE_DESCRIPTORS ? image->texture_descriptor_words : image->sampler_words)*4;
        if (!table_bytes || p->offset>table_bytes || !p->bytes || p->bytes>table_bytes-p->offset) return EINVAL;
        uint64_t address=(uint64_t)native[p->word]|((uint64_t)native[p->word+1]<<32);
        if (address<p->offset) return EINVAL;
        uint64_t base=address-p->offset;
        if (base>UINT64_MAX-table_bytes) return EOVERFLOW;
        if (base<child || base-child>nn_bo_size(job) || table_bytes>nn_bo_size(job)-(base-child)) return EINVAL;
        if ((e=descriptor_audit_excludes(base,table_bytes))) return e;
        seen|=1u<<(p->kind==NN_GFX_SAMPLERS); bases++;
    }
    /* This qualified original profile has VS+FS table bases for each domain. */
    if(seen!=3 || bases!=4) return EINVAL;
    descriptor_audit.private_bases+=bases; ++descriptor_audit.private_jobs; return 0;
}
int __real_nn_queue_submit(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,uint32_t *);
int __wrap_nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,
                           nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
    ++descriptor_audit.attempts;
    int e=descriptor_audit_validate(ib,offset,bytes,refs,count);
    if (e) { ++descriptor_audit.rejected; descriptor_audit.last_errno=e; return e; }
    ++descriptor_audit.delegated;
    e=__real_nn_queue_submit(q,ib,offset,bytes,refs,count,timestamp);
    if (!e) ++descriptor_audit.accepted;
    else descriptor_audit.last_errno=e;
    return e;
}
#endif

/* Qualification caller only: every device/resource/record/submit operation uses
 * the NVN ABI. The immutable offline cache contains the original VS and FS.
 * Pool IDs, NVN selectors and native shader indices deliberately differ. */
#define GUARDED(type) struct { uint64_t before[2]; type value; uint64_t after[2]; }
static int texture_probe_bytes(const void *memory, size_t bytes, unsigned value, uint64_t *checks)
{
    const unsigned char *p = memory;
    for (size_t i = 0; i < bytes; ++i) {
        if (p[i] != value) return EIO;
        ++*checks;
    }
    return 0;
}

int nn_nvn_sampler_probe(const char *directory, char *json, size_t capacity)
{
    enum { SOURCE, TARGET, VERTEX, UNIFORM, TEX0, TEX1, POISON,
           TEX_POOL_A, TEX_POOL_B, SAMP_POOL_A, SAMP_POOL_B, COMMAND, POOLS };
    enum { VS, FS, RT, VB, VUB, FUB, VIEWS };
    enum { TID = 257, SID = 259, POISON_TID = 258, POISON_SID = 260,
           DESCRIPTOR_COUNT = 272, DESCRIPTOR_OFFSET = 4096, SAMPLE_OFFSET = 4096 };
    const struct nn_gfx_image *image = nn_test_graphics.image;
    const char *stage = "resolver", *submission = "not_attempted";
    int error = 0, cleanup = 0, pending = 0, registered = 0;
    int device_ready = 0, queue_ready = 0, program_ready = 0, rt_ready = 0;
    int command_ready = 0, sampled_ready = 0, sampler_ready = 0;
    int device_flags = 0, flags_explicit = 0, api_major = -1, api_minor = -1;
    int pool_ready[POOLS] = {0}, buffer_ready[VIEWS] = {0};
    int tpool_ready[2] = {0}, spool_ready[2] = {0};
    unsigned completed = 0, submissions = 0, finishes = 0, channel_checks = 0, diagnostic_channels = 0;
    unsigned reasons[4] = {0}, diagnostic_pixels = 0, rejections = 0;
    unsigned late_registrations = 0, texture_registrations = 0, sampler_registrations = 0;
    unsigned texture_object_finalizations = 0, sampler_object_finalizations = 0;
    unsigned inactive_border_checks = 0;
    unsigned pool_busy_checks = 0, program_busy_checks = 0, descriptor_busy_checks = 0;
    unsigned recorded_handles = 0, copied_binding_arrays = 0, pool_identity_cases = 0;
    uint64_t guard_checks = 0, readonly_checks = 0;
    uint64_t mode_guard_checks = 0, mode_readonly_checks = 0;
    uint64_t *active_guard_checks = &guard_checks, *active_readonly_checks = &readonly_checks;
    unsigned mode_completed = 0, mode_submissions = 0, mode_channel_checks = 0;
    unsigned mode_shaded = 0, mode_discard = 0, mode_clipped = 0;
    unsigned mode_recorded = 0, mode_released = 0, mode_getter_checks = 0, mode_busy_checks = 0;
    unsigned mode_snapshot_mutations = 0, mode_clipped_case_mask = 0;
    unsigned mode_noaccess_flushes = 0, mode_noaccess_invalidates = 0;
    char mode_readback_names[1024] = ""; size_t mode_names_bytes = 0;
    unsigned noaccess_map_checks = 0, noaccess_gpuva_checks = 0;
    unsigned noaccess_pool_flushes = 0, noaccess_pool_invalidates = 0;
    unsigned noaccess_buffer_cache_calls = 0, noaccess_range_rejections = 0;
    unsigned noaccess_texel_seed_bytes = 0;
    unsigned uncached_pool_maps = 0, uncached_buffer_maps = 0, public_cache_calls = 0;
    unsigned skipped_pool_flushes = 0, skipped_pool_invalidates = 0;
    unsigned skipped_buffer_cache = 0, skipped_cache_ranges = 0;
    unsigned hot_target_preloads = 0, hot_target_word_checks = 0;
#if NVN_TEXTURE_NOACCESS_POOL
    int noaccess_buffer_ready = 0;
#endif
    unsigned bad_case = ~0u, bad_x = ~0u, bad_y = ~0u, bad_channel = ~0u, bad_pool = ~0u;
    unsigned bad_actual = 0, bad_expected = 0;
    size_t bad_offset = SIZE_MAX;
    unsigned char *memory[POOLS] = {0}, *expected[POOLS] = {0};
    size_t sizes[POOLS] = {0}, offsets[VIEWS] = {0}, lengths[VIEWS] = {0};
    const unsigned view_pool[VIEWS] = {SOURCE, SOURCE, TARGET, VERTEX, UNIFORM, UNIFORM};
    NVNbufferAddress addresses[VIEWS] = {0};
    NVNcommandHandle handles[2] = {0}, mode_handle = 0;
    size_t mode_control_used=0,command_used[2]={0},control_used[2]={0};
    struct sampler_objects {
    _Alignas(16) unsigned char mode_control[512];
    _Alignas(16) unsigned char control[2][512];
    GUARDED(NVNdevice) device;
    GUARDED(NVNqueue) queue;
    GUARDED(NVNprogram) program;
    GUARDED(NVNtexture) rt, sampled_object;
    GUARDED(NVNsampler) sampler;
    GUARDED(NVNcommandBuffer) command;
    GUARDED(NVNmemoryPool) pools[POOLS];
    GUARDED(NVNbuffer) buffers[VIEWS];
#if NVN_TEXTURE_NOACCESS_POOL
    GUARDED(NVNbuffer) noaccess_buffer;
#endif
    GUARDED(NVNtexturePool) texture_pools[2];
    GUARDED(NVNsamplerPool) sampler_pools[2];
    } *objects=calloc(1,sizeof(*objects));
    if(!objects)return -1;
#define mode_control (objects->mode_control)
#define control (objects->control)
#define device (objects->device)
#define queue (objects->queue)
#define program (objects->program)
#define rt (objects->rt)
#define sampled_object (objects->sampled_object)
#define sampler (objects->sampler)
#define command (objects->command)
#define pools (objects->pools)
#define buffers (objects->buffers)
#define texture_pools (objects->texture_pools)
#define sampler_pools (objects->sampler_pools)
    NVNdeviceBuilder db; NVNqueueBuilder qb; NVNmemoryPoolBuilder pb;
    NVNbufferBuilder bb; NVNtextureBuilder tb; NVNsamplerBuilder sb;
    NVNvertexAttribState attributes[3]; NVNvertexStreamState stream;
    struct nn_texture_case_inputs input; struct nn_sampler_case sampler_case;
    (void)nn_texture_case_pixel;
    const unsigned char poison_texel[4] = {255,0,255,255};
#define ENTRY(name, type) type name __attribute__((unused)) = (type)nvnBootstrapLoader(#name); if (!name) error = ENOSYS;
#include "../src/nvn_entries.inc"
#undef ENTRY
#define CALL(expression) do { expression; if ((error = nn_nvn_error())) goto done; } while (0)
#define BOOL(expression) do { NVNboolean ok_ = (expression); int e_ = nn_nvn_error(); \
    if (!ok_ || e_) { error = e_ ? e_ : EIO; goto done; } } while (0)
#define REQUIRE(expression) do { int ok_ = !!(expression); int e_ = nn_nvn_error(); \
    if (!ok_ || e_) { error = e_ ? e_ : EINVAL; goto done; } } while (0)
#define BUSY(expression, counter) do { expression; int e_ = nn_nvn_error(); \
    nn_nvn_clear_error(); if (e_ != EBUSY) { error = e_ ? e_ : EINVAL; goto done; } ++counter; } while (0)
#define POOL_FLUSH(pool, bytes) do { \
    if (NVN_TEXTURE_UNCACHED_POOLS) ++skipped_pool_flushes; \
    else { ++public_cache_calls; CALL(nvnMemoryPoolFlushMappedRange(pool, 0, bytes)); } \
} while (0)
#define POOL_INVALIDATE(pool, bytes) do { \
    if (NVN_TEXTURE_UNCACHED_POOLS) ++skipped_pool_invalidates; \
    else { ++public_cache_calls; CALL(nvnMemoryPoolInvalidateMappedRange(pool, 0, bytes)); } \
} while (0)
#define OBJECT_GUARD(object) do { \
    if ((error = texture_probe_bytes((object).before, sizeof((object).before), 0x5a, active_guard_checks)) || \
        (error = texture_probe_bytes((object).after, sizeof((object).after), 0x5a, active_guard_checks))) goto save; \
} while (0)
    if (!json || !capacity) { free(objects); return -1; }
    if (error) goto done;
    nn_nvn_clear_error(); stage = "profile";
#if NVN_TEXTURE_DESCRIPTOR_FLAGS
    memset(&descriptor_audit,0,sizeof(descriptor_audit)); descriptor_audit.image=image;
    if (!image || image->chip!=0x06050002) { error=ENOTSUP; goto done; }
#endif
    if (!directory || !image || sysconf(_SC_PAGESIZE) != 4096 ||
        image->chip != 0x06050002 || image->gmem_bytes != 1179648 || image->schema != 3 || !image->state_patches || image->state_patch_count != NN_GFX_STATE_KIND_COUNT ||
        image->profile != NN_GFX_TEXTURE_2D_NORMALIZED ||
        image->width != 64 || image->height != 64 || image->target_pitch != 256 ||
        image->target_bytes != 16384 || image->vertex_bytes != 96 || image->vertex_stride != 32 ||
        image->matrix_bytes != 64 || image->fs_uniform_bytes != 64 || image->texture_width != 2 ||
        image->texture_height != 2 || image->texture_pitch != 256 || image->texture_bytes != 4096 ||
        image->texture_slots != 2 || !image->texture_descriptors || !image->samplers ||
        !nn_test_graphics.vs.source_code || !nn_test_graphics.fs.source_code ||
        !nn_test_graphics.vs.source_control || !nn_test_graphics.fs.source_control ||
        !nn_test_graphics.vs.source_bytes || !nn_test_graphics.fs.source_bytes ||
        nn_test_graphics.vs.source_bytes > 1048576 || nn_test_graphics.fs.source_bytes > 1048576 ||
        nn_test_graphics.sampled_count != 1 || !nn_test_graphics.sampled) { error = ENOTSUP; goto done; }
    const struct nn_nvn_sampled *map = nn_test_graphics.sampled;
    if (map->stage != NVN_SHADER_STAGE_FRAGMENT || map->native_texture_index != 1 || map->native_sampler_index != 1 ||
        map->texture_domain != NN_NVN_HANDLE_SEPARATE || map->texture_slot != 10 ||
        map->sampler_domain != NN_NVN_HANDLE_SEPARATE || map->sampler_slot != 0 ||
        map->texture_cbuf != 2 || map->texture_word != 110 || map->sampler_cbuf != 2 || map->sampler_word != 346) {
        error = ENOTSUP; goto done;
    }
    memset(&device, 0x5a, sizeof(device)); memset(&queue, 0x5a, sizeof(queue));
    memset(&program, 0x5a, sizeof(program)); memset(&rt, 0x5a, sizeof(rt));
    memset(&sampled_object, 0x5a, sizeof(sampled_object)); memset(&sampler, 0x5a, sizeof(sampler));
    memset(&command, 0x5a, sizeof(command)); memset(pools, 0x5a, sizeof(pools));
    memset(buffers, 0x5a, sizeof(buffers)); memset(texture_pools, 0x5a, sizeof(texture_pools));
    memset(sampler_pools, 0x5a, sizeof(sampler_pools)); memset(control, 0xa5, sizeof(control));
#if NVN_TEXTURE_NOACCESS_POOL
    memset(&noaccess_buffer, 0x5a, sizeof(noaccess_buffer));
#endif
    offsets[VS] = 4096; lengths[VS] = nn_test_graphics.vs.source_bytes;
    offsets[FS] = ((offsets[VS] + lengths[VS] + 4095) & ~(size_t)4095) + 4096;
    lengths[FS] = nn_test_graphics.fs.source_bytes;
    sizes[SOURCE] = ((offsets[FS] + lengths[FS] + 4095) & ~(size_t)4095) + 4096;
    offsets[RT] = 4096; lengths[RT] = image->target_bytes; sizes[TARGET] = 24576;
    offsets[VB] = 256; lengths[VB] = 96; sizes[VERTEX] = 4096;
    offsets[VUB] = 64; lengths[VUB] = 64; offsets[FUB] = 256; lengths[FUB] = 64; sizes[UNIFORM] = 4096;
    for (unsigned i = TEX0; i <= POISON; ++i) sizes[i] = 12288;
    for (unsigned i = TEX_POOL_A; i <= SAMP_POOL_B; ++i) sizes[i] = 16384;
    sizes[COMMAND] = 12288;
    stage = "device";
    CALL(nvnDeviceBuilderSetDefaults(&db));
#ifdef NVN_TEXTURE_DEVICE_FLAGS
    device_flags = NVN_TEXTURE_DEVICE_FLAGS; flags_explicit = 1;
    CALL(nvnDeviceBuilderSetFlags(&db, device_flags));
#endif
    BOOL(nvnDeviceInitialize(&device.value, &db)); device_ready = 1;
    struct { uint32_t before; int value; uint32_t after; } version = {0x13579bdf, -1, 0x2468ace0};
    CALL(nvnDeviceGetInteger(&device.value, NVN_DEVICE_INFO_API_MAJOR_VERSION, &version.value));
    REQUIRE(version.before == 0x13579bdf && version.after == 0x2468ace0 && version.value == NN_NVN_ABI_MAJOR);
    api_major = version.value;
    CALL(nvnDeviceGetInteger(&device.value, NVN_DEVICE_INFO_API_MINOR_VERSION, &version.value));
    REQUIRE(version.before == 0x13579bdf && version.after == 0x2468ace0 && version.value == NN_NVN_ABI_MINOR);
    api_minor = version.value;
    CALL(nvnDeviceSetWindowOriginMode(&device.value, NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT));
    CALL(nvnDeviceSetDepthMode(&device.value, NVN_DEPTH_MODE_NEAR_IS_MINUS_W));
    REQUIRE(nvnDeviceGetWindowOriginMode(&device.value) == NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT); ++mode_getter_checks;
    REQUIRE(nvnDeviceGetDepthMode(&device.value) == NVN_DEPTH_MODE_NEAR_IS_MINUS_W); ++mode_getter_checks;
    /* Builder reuse cannot mutate this initialized device's resource domains. */
    CALL(nvnDeviceBuilderSetDefaults(&db));
    if ((error = nn_nvn_register_graphics(&device.value, &nn_test_graphics))) goto done;
    REQUIRE(nn_nvn_error() == 0); registered = 1;
    CALL(nvnQueueBuilderSetDefaults(&qb)); CALL(nvnQueueBuilderSetDevice(&qb, &device.value));
    BOOL(nvnQueueInitialize(&queue.value, &qb)); queue_ready = 1;
    stage = "memory-pools";
    for (unsigned i = 0; i < POOLS; ++i) {
        if ((error = posix_memalign((void **)&memory[i], 4096, sizes[i]))) goto done;
        expected[i] = malloc(sizes[i]); if (!expected[i]) { error = ENOMEM; goto done; }
        memset(memory[i], 0xa5, sizes[i]);
#if NVN_TEXTURE_NOACCESS_POOL
        if (i == POISON) {
            /* Seed before pool creation; imported CPU_NO_ACCESS contents are
             * thereafter GPU input only. Never CPU-read this pool as oracle. */
            for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 2; ++x) {
                memcpy(memory[i] + SAMPLE_OFFSET + y * image->texture_pitch + x * 4, poison_texel, 4);
                noaccess_texel_seed_bytes += 4;
            }
        }
#endif
        CALL(nvnMemoryPoolBuilderSetDefaults(&pb)); CALL(nvnMemoryPoolBuilderSetDevice(&pb, &device.value));
        CALL(nvnMemoryPoolBuilderSetStorage(&pb, memory[i], sizes[i]));
        int pool_flags = (NVN_TEXTURE_UNCACHED_POOLS ? NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED :
                                                    NVN_MEMORY_POOL_FLAGS_CPU_CACHED) | NVN_MEMORY_POOL_FLAGS_GPU_CACHED |
                         (i == SOURCE ? NVN_MEMORY_POOL_FLAGS_SHADER_CODE : 0);
#if NVN_TEXTURE_NOACCESS_POOL
        if (i == POISON) pool_flags = NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS |
                                    NVN_MEMORY_POOL_FLAGS_GPU_CACHED | NVN_MEMORY_POOL_FLAGS_COMPRESSIBLE;
#endif
#if NVN_TEXTURE_DESCRIPTOR_FLAGS
        if (i>=TEX_POOL_A && i<=SAMP_POOL_B) pool_flags=NVN_TEXTURE_DESCRIPTOR_FLAGS;
#endif
        CALL(nvnMemoryPoolBuilderSetFlags(&pb, pool_flags));
        BOOL(nvnMemoryPoolInitialize(&pools[i].value, &pb)); pool_ready[i] = 1;
#if NVN_TEXTURE_NOACCESS_POOL
        if (i == POISON) {
            REQUIRE(pool_flags == 0xa1);
            REQUIRE(nvnMemoryPoolMap(&pools[i].value) == NULL); ++noaccess_map_checks;
            REQUIRE(nvnMemoryPoolGetBufferAddress(&pools[i].value) != 0); ++noaccess_gpuva_checks;
        } else
#endif
        { REQUIRE(nvnMemoryPoolMap(&pools[i].value) == memory[i]);
          if (NVN_TEXTURE_UNCACHED_POOLS) ++uncached_pool_maps; }
#if NVN_TEXTURE_DESCRIPTOR_FLAGS
        if (i>=TEX_POOL_A && i<=SAMP_POOL_B) {
            ++descriptor_audit.maps;
            NVNbufferAddress a=nvnMemoryPoolGetBufferAddress(&pools[i].value);
            if (nn_nvn_error()) { error=nn_nvn_error(); goto done; }
            if ((error=descriptor_audit_range(a,sizes[i]))) goto done;
        }
#endif
    }
    stage = "buffers";
    for (unsigned i = 0; i < VIEWS; ++i) {
        CALL(nvnBufferBuilderSetDefaults(&bb)); CALL(nvnBufferBuilderSetDevice(&bb, &device.value));
        CALL(nvnBufferBuilderSetStorage(&bb, &pools[view_pool[i]].value, offsets[i], lengths[i]));
        BOOL(nvnBufferInitialize(&buffers[i].value, &bb)); buffer_ready[i] = 1;
        REQUIRE(nvnBufferMap(&buffers[i].value) == memory[view_pool[i]] + offsets[i]);
        if (NVN_TEXTURE_UNCACHED_POOLS) ++uncached_buffer_maps;
        addresses[i] = nvnBufferGetAddress(&buffers[i].value); REQUIRE(addresses[i] != 0);
        REQUIRE(addresses[i] == nvnMemoryPoolGetBufferAddress(&pools[view_pool[i]].value) + offsets[i]);
    }
#if NVN_TEXTURE_NOACCESS_POOL
    stage = "noaccess-buffer";
    CALL(nvnBufferBuilderSetDefaults(&bb)); CALL(nvnBufferBuilderSetDevice(&bb, &device.value));
    CALL(nvnBufferBuilderSetStorage(&bb, &pools[POISON].value, SAMPLE_OFFSET, image->texture_bytes));
    BOOL(nvnBufferInitialize(&noaccess_buffer.value, &bb)); noaccess_buffer_ready = 1;
    REQUIRE(nvnBufferMap(&noaccess_buffer.value) == NULL); ++noaccess_map_checks;
    NVNbufferAddress noaccess_address = nvnBufferGetAddress(&noaccess_buffer.value);
    REQUIRE(noaccess_address != 0 && noaccess_address ==
            nvnMemoryPoolGetBufferAddress(&pools[POISON].value) + SAMPLE_OFFSET); ++noaccess_gpuva_checks;
    if (NVN_TEXTURE_UNCACHED_POOLS) {
        /* This variant must make zero public cache calls, including NOP and
         * invalid-range NOACCESS diagnostics qualified by the baseline. */
        skipped_buffer_cache += 2; skipped_cache_ranges += 4;
    } else {
        ++public_cache_calls; CALL(nvnBufferFlushMappedRange(&noaccess_buffer.value, 0, image->texture_bytes)); ++noaccess_buffer_cache_calls;
        ++public_cache_calls; CALL(nvnBufferInvalidateMappedRange(&noaccess_buffer.value, 0, image->texture_bytes)); ++noaccess_buffer_cache_calls;
#define NOACCESS_RANGE_ERROR(expression) do { ++public_cache_calls; expression; int e_ = nn_nvn_error(); \
        nn_nvn_clear_error(); if (e_ != EINVAL) { error = e_ ? e_ : EINVAL; goto done; } ++noaccess_range_rejections; } while (0)
        NOACCESS_RANGE_ERROR(nvnMemoryPoolFlushMappedRange(&pools[POISON].value, -1, 1));
        NOACCESS_RANGE_ERROR(nvnMemoryPoolInvalidateMappedRange(&pools[POISON].value, sizes[POISON], 1));
        NOACCESS_RANGE_ERROR(nvnBufferFlushMappedRange(&noaccess_buffer.value, -1, 1));
        NOACCESS_RANGE_ERROR(nvnBufferInvalidateMappedRange(&noaccess_buffer.value, image->texture_bytes, 1));
#undef NOACCESS_RANGE_ERROR
    }
    CALL(nvnBufferFinalize(&noaccess_buffer.value)); noaccess_buffer_ready = 0;
    /* Subsequent POISON busy rejection must be due to descriptor retention,
     * not this temporary public Buffer object. */
#endif
    memcpy(memory[SOURCE] + offsets[VS], nn_test_graphics.vs.source_code, lengths[VS]);
    memcpy(memory[SOURCE] + offsets[FS], nn_test_graphics.fs.source_code, lengths[FS]);
    POOL_FLUSH(&pools[SOURCE].value, sizes[SOURCE]);
    stage = "program";
    BOOL(nvnProgramInitialize(&program.value, &device.value)); program_ready = 1;
    const NVNshaderData shaders[2] = {{addresses[FS], nn_test_graphics.fs.source_control},
                                     {addresses[VS], nn_test_graphics.vs.source_control}};
    BOOL(nvnProgramSetShaders(&program.value, 2, shaders));
    stage = "descriptor-pools";
    for (unsigned i = 0; i < 2; ++i) {
        BOOL(nvnTexturePoolInitialize(&texture_pools[i].value, &pools[TEX_POOL_A + i].value,
                                      DESCRIPTOR_OFFSET, DESCRIPTOR_COUNT)); tpool_ready[i] = 1;
        BOOL(nvnSamplerPoolInitialize(&sampler_pools[i].value, &pools[SAMP_POOL_A + i].value,
                                      DESCRIPTOR_OFFSET, DESCRIPTOR_COUNT)); spool_ready[i] = 1;
        REQUIRE(nvnTexturePoolGetMemoryPool(&texture_pools[i].value) == &pools[TEX_POOL_A + i].value);
        REQUIRE(nvnSamplerPoolGetMemoryPool(&sampler_pools[i].value) == &pools[SAMP_POOL_A + i].value);
        REQUIRE(nvnTexturePoolGetMemoryOffset(&texture_pools[i].value) == DESCRIPTOR_OFFSET);
        REQUIRE(nvnSamplerPoolGetMemoryOffset(&sampler_pools[i].value) == DESCRIPTOR_OFFSET);
        REQUIRE(nvnTexturePoolGetSize(&texture_pools[i].value) == DESCRIPTOR_COUNT);
        REQUIRE(nvnSamplerPoolGetSize(&sampler_pools[i].value) == DESCRIPTOR_COUNT);
    }
    stage = "render-target";
    CALL(nvnTextureBuilderSetDefaults(&tb)); CALL(nvnTextureBuilderSetDevice(&tb, &device.value));
    CALL(nvnTextureBuilderSetFlags(&tb, NVN_TEXTURE_FLAGS_LINEAR));
    CALL(nvnTextureBuilderSetTarget(&tb, NVN_TEXTURE_TARGET_2D));
    CALL(nvnTextureBuilderSetSize2D(&tb, 64, 64)); CALL(nvnTextureBuilderSetLevels(&tb, 1));
    CALL(nvnTextureBuilderSetSamples(&tb, 0)); CALL(nvnTextureBuilderSetFormat(&tb, NVN_FORMAT_RGBA8));
    CALL(nvnTextureBuilderSetStride(&tb, 256));
    REQUIRE(nvnTextureBuilderGetStorageSize(&tb) == image->target_bytes);
    REQUIRE(nvnTextureBuilderGetStorageAlignment(&tb) == 64);
    CALL(nvnTextureBuilderSetStorage(&tb, &pools[TARGET].value, offsets[RT]));
    BOOL(nvnTextureInitialize(&rt.value, &tb)); rt_ready = 1;
    REQUIRE(nvnTextureGetTextureAddress(&rt.value) == addresses[RT]);
    NVNseparateTextureHandle texture_handle = nvnDeviceGetSeparateTextureHandle(&device.value, TID);
    REQUIRE(texture_handle.value == ((UINT64_C(1)<<32) | TID));
    NVNseparateSamplerHandle sampler_handle = nvnDeviceGetSeparateSamplerHandle(&device.value, SID);
    REQUIRE(sampler_handle.value == ((uint64_t)SID << 20));
    NVNtextureHandle combined = nvnDeviceGetTextureHandle(&device.value, POISON_TID, POISON_SID);
    /* Original SDK CombinedHandle includes its bit32 carrier marker. */
    REQUIRE(combined == ((UINT64_C(1)<<32) | (uint64_t)POISON_TID | ((uint64_t)POISON_SID << 20)));
    /* Three rejected recordings, then two independent successful handles. Pool
     * entries intentionally remain unregistered until AFTER both recordings. */
    for (unsigned record = 0; record < 5; ++record) {
        unsigned which = record == 4 ? 1 : 0;
        size_t command_offset = record < 3 ? 0 : (which + 1) * 4096;
        stage = record < 3 ? "record-negative" : "record";
        memset(control[which], 0xa5, sizeof(control[which]));
        BOOL(nvnCommandBufferInitialize(&command.value, &device.value)); command_ready = 1;
        CALL(nvnCommandBufferAddCommandMemory(&command.value, &pools[COMMAND].value, command_offset, 4096));
        CALL(nvnCommandBufferAddControlMemory(&command.value, control[which], sizeof(control[which])));
        CALL(nvnCommandBufferBeginRecording(&command.value));
        CALL(nvnCommandBufferBindProgram(&command.value, &program.value,
                                        NVN_SHADER_STAGE_VERTEX_BIT | NVN_SHADER_STAGE_FRAGMENT_BIT));
        CALL(nvnCommandBufferBindUniformBuffer(&command.value, NVN_SHADER_STAGE_VERTEX, 0, addresses[VUB], 64));
        CALL(nvnCommandBufferBindUniformBuffer(&command.value, NVN_SHADER_STAGE_FRAGMENT, 0, addresses[FUB], 64));
        for (unsigned i = 0; i < 3; ++i) {
            CALL(nvnVertexAttribStateSetDefaults(&attributes[i]));
            CALL(nvnVertexAttribStateSetFormat(&attributes[i], i == 2 ? NVN_FORMAT_RGBA32F : NVN_FORMAT_RG32F,
                                               i == 2 ? 16 : i * 8));
            CALL(nvnVertexAttribStateSetStreamIndex(&attributes[i], 0));
        }
        CALL(nvnVertexStreamStateSetDefaults(&stream)); CALL(nvnVertexStreamStateSetStride(&stream, 32));
        CALL(nvnVertexStreamStateSetDivisor(&stream, 0));
        CALL(nvnCommandBufferBindVertexAttribState(&command.value, 3, attributes));
        CALL(nvnCommandBufferBindVertexStreamState(&command.value, 1, &stream));
        memset(attributes, 0, sizeof(attributes)); memset(&stream, 0, sizeof(stream));
        CALL(nvnCommandBufferBindVertexBuffer(&command.value, 0, addresses[VB], 96));
        const NVNtexture *targets[1] = {&rt.value};
        CALL(nvnCommandBufferSetRenderTargets(&command.value, 1, targets, NULL, NULL, NULL));
        CALL(nvnCommandBufferSetViewport(&command.value, 0, 0, 64, 64));
        CALL(nvnCommandBufferSetScissor(&command.value, 0, 0, 64, 64));
        CALL(nvnCommandBufferSetDepthRange(&command.value, 0, 1));
        CALL(nvnCommandBufferSetTexturePool(&command.value, &texture_pools[which].value));
        CALL(nvnCommandBufferSetSamplerPool(&command.value, &sampler_pools[which].value));
        if (record == 0) {
            CALL(nvnCommandBufferBindTexture(&command.value, NVN_SHADER_STAGE_FRAGMENT, 10, combined));
            CALL(nvnCommandBufferBindSeparateSampler(&command.value, NVN_SHADER_STAGE_FRAGMENT, 0, sampler_handle));
        } else if (record == 1) {
            CALL(nvnCommandBufferBindSeparateTexture(&command.value, NVN_SHADER_STAGE_FRAGMENT, 11, texture_handle));
            CALL(nvnCommandBufferBindSeparateSampler(&command.value, NVN_SHADER_STAGE_FRAGMENT, 0, sampler_handle));
        } else if (record == 2) {
            CALL(nvnCommandBufferBindSeparateTexture(&command.value, NVN_SHADER_STAGE_FRAGMENT, 10, texture_handle));
            CALL(nvnCommandBufferBindSeparateSampler(&command.value, NVN_SHADER_STAGE_FRAGMENT, 1, sampler_handle));
        } else {
            NVNseparateTextureHandle texture_array[1] = {texture_handle};
            NVNseparateSamplerHandle sampler_array[1] = {sampler_handle};
            CALL(nvnCommandBufferBindSeparateTextures(&command.value, NVN_SHADER_STAGE_FRAGMENT, 10, 1, texture_array));
            CALL(nvnCommandBufferBindSeparateSamplers(&command.value, NVN_SHADER_STAGE_FRAGMENT, 0, 1, sampler_array));
            /* Invalid if accidentally consumed: sampler260 is never registered. */
            texture_array[0].value = POISON_TID; sampler_array[0].value = (uint64_t)POISON_SID << 20;
            CALL(nvnCommandBufferBindTexture(&command.value, NVN_SHADER_STAGE_FRAGMENT, 10, combined));
            CALL(nvnCommandBufferBindSeparateTexture(&command.value, NVN_SHADER_STAGE_FRAGMENT, 11, texture_array[0]));
            CALL(nvnCommandBufferBindSeparateSampler(&command.value, NVN_SHADER_STAGE_FRAGMENT, 1, sampler_array[0]));
            copied_binding_arrays += 2;
        }
        size_t before_draw = nvnCommandBufferGetCommandMemoryUsed(&command.value); REQUIRE(nn_nvn_error() == 0);
        nvnCommandBufferDrawArrays(&command.value, NVN_DRAW_PRIMITIVE_TRIANGLES, 0, 3);
        if (record < 3) {
            int rejected = nn_nvn_error(); nn_nvn_clear_error();
            if (rejected != EINVAL && rejected != ENOTSUP) { error = rejected ? rejected : EINVAL; goto done; }
            REQUIRE(nvnCommandBufferGetCommandMemoryUsed(&command.value) == before_draw); ++rejections;
        } else {
            REQUIRE(nn_nvn_error() == 0);
            handles[which] = nvnCommandBufferEndRecording(&command.value); REQUIRE(handles[which] != 0);
            command_used[which] = nvnCommandBufferGetCommandMemoryUsed(&command.value);
            REQUIRE(command_used[which] > 0 && command_used[which] <= 4096);
            control_used[which] = nvnCommandBufferGetControlMemoryUsed(&command.value);
            REQUIRE(control_used[which] > 0 && control_used[which] <= sizeof(control[which]));
            ++recorded_handles;
        }
        CALL(nvnCommandBufferFinalize(&command.value)); command_ready = 0;
    }
    stage = "register-after-record";
    for (unsigned entry = 0; entry < 2; ++entry) {
        unsigned backing = entry ? POISON : TEX0;
        CALL(nvnTextureBuilderSetDefaults(&tb)); CALL(nvnTextureBuilderSetDevice(&tb, &device.value));
        CALL(nvnTextureBuilderSetFlags(&tb, NVN_TEXTURE_FLAGS_LINEAR));
        CALL(nvnTextureBuilderSetTarget(&tb, NVN_TEXTURE_TARGET_2D));
        CALL(nvnTextureBuilderSetSize2D(&tb, 2, 2)); CALL(nvnTextureBuilderSetLevels(&tb, 1));
        CALL(nvnTextureBuilderSetSamples(&tb, 0)); CALL(nvnTextureBuilderSetFormat(&tb, NVN_FORMAT_RGBA8));
        CALL(nvnTextureBuilderSetStride(&tb, image->texture_pitch));
        REQUIRE(nvnTextureBuilderGetStorageSize(&tb) == image->texture_bytes);
        REQUIRE(nvnTextureBuilderGetStorageAlignment(&tb) == image->texture_alignment);
        CALL(nvnTextureBuilderSetStorage(&tb, &pools[backing].value, SAMPLE_OFFSET));
        BOOL(nvnTextureInitialize(&sampled_object.value, &tb)); sampled_ready = 1;
        if (!entry) {
            CALL(nvnTexturePoolRegisterTexture(&texture_pools[0].value, TID, &sampled_object.value, NULL));
            ++texture_registrations; ++late_registrations;
        } else {
            CALL(nvnTexturePoolRegisterTexture(&texture_pools[0].value, POISON_TID, &sampled_object.value, NULL));
            CALL(nvnTexturePoolRegisterTexture(&texture_pools[1].value, TID, &sampled_object.value, NULL));
            texture_registrations += 2;
        }
        CALL(nvnTextureFinalize(&sampled_object.value)); sampled_ready = 0; ++texture_object_finalizations;
        BUSY(nvnMemoryPoolFinalize(&pools[backing].value), pool_busy_checks);
    }
    CALL(nvnSamplerBuilderSetDefaults(&sb)); CALL(nvnSamplerBuilderSetDevice(&sb, &device.value));
    CALL(nvnSamplerBuilderSetMinMagFilter(&sb, NVN_MIN_FILTER_NEAREST, NVN_MAG_FILTER_NEAREST));
    CALL(nvnSamplerBuilderSetWrapMode(&sb, NVN_WRAP_MODE_CLAMP_TO_EDGE, NVN_WRAP_MODE_CLAMP_TO_EDGE, NVN_WRAP_MODE_CLAMP_TO_EDGE));
    CALL(nvnSamplerBuilderSetLodClamp(&sb, 0, 0)); CALL(nvnSamplerBuilderSetLodBias(&sb, 0));
    CALL(nvnSamplerBuilderSetCompare(&sb, NVN_COMPARE_MODE_NONE, NVN_COMPARE_FUNC_LESS));
    CALL(nvnSamplerBuilderSetMaxAnisotropy(&sb, 1));
    CALL(nvnSamplerBuilderSetReductionFilter(&sb, NVN_SAMPLER_REDUCTION_AVERAGE));
    BOOL(nvnSamplerInitialize(&sampler.value, &sb)); sampler_ready = 1;
    for (unsigned i = 0; i < 2; ++i) {
        CALL(nvnSamplerPoolRegisterSampler(&sampler_pools[i].value, SID, &sampler.value)); ++sampler_registrations;
        BUSY(nvnTexturePoolFinalize(&texture_pools[i].value), descriptor_busy_checks);
        BUSY(nvnSamplerPoolFinalize(&sampler_pools[i].value), descriptor_busy_checks);
    }
    CALL(nvnSamplerFinalize(&sampler.value)); sampler_ready = 0; ++sampler_object_finalizations;
    /* Four original profiles plus a separately counted same-ID/different-pool
     * GPU diagnostic. No additional GPU program or altered shader is used. */
    for (unsigned test = 0; test < NN_SAMPLER_CASES; ++test) {
        int is_mode = 0;
        unsigned which = 0;
        unsigned backing = test ? TEX1 : TEX0;
        struct nn_texture_mode_case mode = {0};
        unsigned case_clipped_pixels = 0;
        active_guard_checks = is_mode ? &mode_guard_checks : &guard_checks;
        active_readonly_checks = is_mode ? &mode_readonly_checks : &readonly_checks;
        stage = "prepare-input";
        if (nn_sampler_case_prepare(test,&input,&sampler_case)) { error=EINVAL; goto done; }
        stage="register-sampler-state";
        CALL(nvnSamplerBuilderSetDefaults(&sb)); CALL(nvnSamplerBuilderSetDevice(&sb,&device.value));
        CALL(nvnSamplerBuilderSetMinMagFilter(&sb,(NVNminFilter)sampler_case.min,(NVNmagFilter)sampler_case.mag));
        CALL(nvnSamplerBuilderSetWrapMode(&sb,(NVNwrapMode)sampler_case.s,(NVNwrapMode)sampler_case.t,NVN_WRAP_MODE_CLAMP_TO_EDGE));
        /* All supported wraps avoid border fetches, including out-of-range UV.
         * Vary the preserved color independently of the original texel oracle. */
        float border[4]={((int)(test%9)-4)*.25f,2,-3,(test&1)?1:0},observed[4];
        CALL(nvnSamplerBuilderSetBorderColor(&sb,border));
        CALL(nvnSamplerBuilderGetBorderColor(&sb,observed));
        REQUIRE(!memcmp(border,observed,sizeof(border)));++inactive_border_checks;
        CALL(nvnSamplerBuilderSetLodClamp(&sb,0,1000));
        BOOL(nvnSamplerInitialize(&sampler.value,&sb)); sampler_ready=1;
        CALL(nvnSamplerGetBorderColor(&sampler.value,observed));
        REQUIRE(!memcmp(border,observed,sizeof(border)));++inactive_border_checks;
        CALL(nvnSamplerPoolRegisterSampler(&sampler_pools[0].value,SID,&sampler.value)); ++sampler_registrations;
        CALL(nvnSamplerFinalize(&sampler.value)); sampler_ready=0; ++sampler_object_finalizations;
        /* Registered value outlives Sampler and its Builder. Replay uses the
         * same recorded handle while the slot is re-registered each enqueue. */
        CALL(nvnSamplerBuilderSetDefaults(&sb));
        if (is_mode) {
            stage = "mode-record";
            /* Offset0 never aliases the two baseline handles at4096/8192.
             * One completed mode handle is released before this arena is reused. */
            REQUIRE(mode_handle == 0);
            memset(mode_control, 0xa5, sizeof(mode_control));
            CALL(nvnDeviceSetWindowOriginMode(&device.value, (NVNwindowOriginMode)mode.window_origin));
            CALL(nvnDeviceSetDepthMode(&device.value, (NVNdepthMode)mode.depth_mode));
            REQUIRE(nvnDeviceGetWindowOriginMode(&device.value) == (NVNwindowOriginMode)mode.window_origin); ++mode_getter_checks;
            REQUIRE(nvnDeviceGetDepthMode(&device.value) == (NVNdepthMode)mode.depth_mode); ++mode_getter_checks;
            BOOL(nvnCommandBufferInitialize(&command.value, &device.value)); command_ready = 1;
            CALL(nvnCommandBufferAddCommandMemory(&command.value, &pools[COMMAND].value, 0, 4096));
            CALL(nvnCommandBufferAddControlMemory(&command.value, mode_control, sizeof(mode_control)));
            CALL(nvnCommandBufferBeginRecording(&command.value));
            CALL(nvnCommandBufferBindProgram(&command.value, &program.value,
                                            NVN_SHADER_STAGE_VERTEX_BIT | NVN_SHADER_STAGE_FRAGMENT_BIT));
            CALL(nvnCommandBufferBindUniformBuffer(&command.value, NVN_SHADER_STAGE_VERTEX, 0, addresses[VUB], 64));
            CALL(nvnCommandBufferBindUniformBuffer(&command.value, NVN_SHADER_STAGE_FRAGMENT, 0, addresses[FUB], 64));
            for (unsigned i = 0; i < 3; ++i) {
                CALL(nvnVertexAttribStateSetDefaults(&attributes[i]));
                CALL(nvnVertexAttribStateSetFormat(&attributes[i], i == 2 ? NVN_FORMAT_RGBA32F : NVN_FORMAT_RG32F,
                                                   i == 2 ? 16 : i * 8));
                CALL(nvnVertexAttribStateSetStreamIndex(&attributes[i], 0));
            }
            CALL(nvnVertexStreamStateSetDefaults(&stream)); CALL(nvnVertexStreamStateSetStride(&stream, 32));
            CALL(nvnVertexStreamStateSetDivisor(&stream, 0));
            CALL(nvnCommandBufferBindVertexAttribState(&command.value, 3, attributes));
            CALL(nvnCommandBufferBindVertexStreamState(&command.value, 1, &stream));
            memset(attributes, 0, sizeof(attributes)); memset(&stream, 0, sizeof(stream));
            CALL(nvnCommandBufferBindVertexBuffer(&command.value, 0, addresses[VB], 96));
            const NVNtexture *targets[1] = {&rt.value};
            CALL(nvnCommandBufferSetRenderTargets(&command.value, 1, targets, NULL, NULL, NULL));
            CALL(nvnCommandBufferSetViewport(&command.value, 0, 0, 64, 64));
            CALL(nvnCommandBufferSetScissor(&command.value, 0, 0, 64, 64));
            CALL(nvnCommandBufferSetDepthRange(&command.value, 0, 1));
            CALL(nvnCommandBufferSetTexturePool(&command.value, &texture_pools[which].value));
            CALL(nvnCommandBufferSetSamplerPool(&command.value, &sampler_pools[which].value));
            CALL(nvnCommandBufferBindSeparateTexture(&command.value, NVN_SHADER_STAGE_FRAGMENT, 10, texture_handle));
            CALL(nvnCommandBufferBindSeparateSampler(&command.value, NVN_SHADER_STAGE_FRAGMENT, 0, sampler_handle));
            CALL(nvnCommandBufferDrawArrays(&command.value, NVN_DRAW_PRIMITIVE_TRIANGLES, 0, 3));
            mode_handle = nvnCommandBufferEndRecording(&command.value); REQUIRE(mode_handle != 0); ++mode_recorded;
            REQUIRE(nvnCommandBufferGetCommandMemoryUsed(&command.value) > 0 &&
                    nvnCommandBufferGetCommandMemoryUsed(&command.value) <= 4096);
            mode_control_used = nvnCommandBufferGetControlMemoryUsed(&command.value);
            REQUIRE(mode_control_used > 0 && mode_control_used <= sizeof(mode_control));
            CALL(nvnCommandBufferFinalize(&command.value)); command_ready = 0;
            /* If enqueue incorrectly reads mutable device state it will use the
             * opposite Y/clip convention and fail the strict original oracle. */
            CALL(nvnDeviceSetWindowOriginMode(&device.value, (NVNwindowOriginMode)(1 - mode.window_origin)));
            CALL(nvnDeviceSetDepthMode(&device.value, (NVNdepthMode)(1 - mode.depth_mode)));
            REQUIRE(nvnDeviceGetWindowOriginMode(&device.value) == (NVNwindowOriginMode)(1 - mode.window_origin)); ++mode_getter_checks;
            REQUIRE(nvnDeviceGetDepthMode(&device.value) == (NVNdepthMode)(1 - mode.depth_mode)); ++mode_getter_checks;
            ++mode_snapshot_mutations;
        }
        if (test == 1) {
            stage = "register-replacement";
            CALL(nvnTextureBuilderSetDefaults(&tb)); CALL(nvnTextureBuilderSetDevice(&tb, &device.value));
            CALL(nvnTextureBuilderSetFlags(&tb, NVN_TEXTURE_FLAGS_LINEAR));
            CALL(nvnTextureBuilderSetTarget(&tb, NVN_TEXTURE_TARGET_2D));
            CALL(nvnTextureBuilderSetSize2D(&tb, 2, 2)); CALL(nvnTextureBuilderSetLevels(&tb, 1));
            CALL(nvnTextureBuilderSetSamples(&tb, 0)); CALL(nvnTextureBuilderSetFormat(&tb, NVN_FORMAT_RGBA8));
            CALL(nvnTextureBuilderSetStride(&tb, image->texture_pitch));
            CALL(nvnTextureBuilderSetStorage(&tb, &pools[TEX1].value, SAMPLE_OFFSET));
            BOOL(nvnTextureInitialize(&sampled_object.value, &tb)); sampled_ready = 1;
            CALL(nvnTexturePoolRegisterTexture(&texture_pools[0].value, TID, &sampled_object.value, NULL));
            ++texture_registrations; ++late_registrations;
            CALL(nvnTextureFinalize(&sampled_object.value)); sampled_ready = 0; ++texture_object_finalizations;
            BUSY(nvnMemoryPoolFinalize(&pools[TEX1].value), pool_busy_checks);
        }
        for (unsigned i = TARGET; i <= POISON; ++i) {
#if NVN_TEXTURE_NOACCESS_POOL
            if (i == POISON) continue;
#endif
            memset(memory[i], 0xa5, sizes[i]);
        }
        unsigned char *target = memory[TARGET] + offsets[RT];
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x)
            memcpy(target + y * image->target_pitch + x * 4, input.clear, 4);
        memcpy(memory[VERTEX] + offsets[VB], input.vertices, sizeof(input.vertices));
        memcpy(memory[UNIFORM] + offsets[VUB], input.vs_ubo, sizeof(input.vs_ubo));
        memcpy(memory[UNIFORM] + offsets[FUB], input.fs_ubo, sizeof(input.fs_ubo));
        for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 2; ++x) {
#if !NVN_TEXTURE_NOACCESS_POOL
            memcpy(memory[POISON] + SAMPLE_OFFSET + y * image->texture_pitch + x * 4, poison_texel, 4);
#endif
            memcpy(memory[backing] + SAMPLE_OFFSET + y * image->texture_pitch + x * 4, input.texels + (y * 2 + x) * 4, 4);
            /* From case1 onward the old backing is observably wrong. */
            if (test) memcpy(memory[TEX0] + SAMPLE_OFFSET + y * image->texture_pitch + x * 4, poison_texel, 4);
        }

        for (unsigned i = 0; i < POOLS; ++i) {
#if NVN_TEXTURE_NOACCESS_POOL
            if (i == POISON) continue; /* No CPU payload read after creation. */
#endif
            memcpy(expected[i], memory[i], sizes[i]);
        }
        stage = "flush";
        for (unsigned i = TARGET; i <= POISON; ++i) {
            POOL_FLUSH(&pools[i].value, sizes[i]);
#if NVN_TEXTURE_NOACCESS_POOL
            if (i == POISON && !NVN_TEXTURE_UNCACHED_POOLS) { if (is_mode) ++mode_noaccess_flushes; else ++noaccess_pool_flushes; }
#endif
        }
        if (NVN_TEXTURE_UNCACHED_POOLS) {
            stage = "hot-target-preload";
            /* Read every old RT cacheline through the original caller pointer
             * immediately before submit. Volatile prevents the compiler from
             * substituting earlier clear stores. Residency itself is not a
             * userspace guarantee. No clean/invalidate or alternate CPU map. */
            const volatile unsigned char *hot = target;
            for (size_t word = 0; word < image->target_bytes / 4; ++word) {
                for (unsigned channel = 0; channel < 4; ++channel)
                    if (hot[word * 4 + channel] != input.clear[channel]) { error = EIO; goto done; }
                ++hot_target_word_checks;
            }
            ++hot_target_preloads;
        }
#if NVN_TEXTURE_DESCRIPTOR_FLAGS
        descriptor_audit.active=1;
#endif
        stage = "submit"; submission = "unknown"; pending = 1;
        const NVNcommandHandle *submit_handle = is_mode ? &mode_handle : &handles[which];
        CALL(nvnQueueSubmitCommands(&queue.value, 1, submit_handle));
        submission = "accepted"; if (is_mode) ++mode_submissions; else ++submissions;
        if (is_mode) BUSY(nvnProgramFinalize(&program.value), mode_busy_checks);
        else BUSY(nvnProgramFinalize(&program.value), program_busy_checks);
        stage = "wait"; CALL(nvnQueueFinish(&queue.value)); pending = 0; ++finishes;
        stage = "invalidate";
        for (unsigned i = 0; i < POOLS; ++i) {
#if NVN_TEXTURE_DESCRIPTOR_FLAGS
            /* These bytes are exclusively CPU metadata, with no GPU writer.
             * Do not apply the render-target readback protocol to CPU tables. */
            if (i>=TEX_POOL_A && i<=SAMP_POOL_B) { ++descriptor_audit.invalidate_skips; continue; }
#endif
            POOL_INVALIDATE(&pools[i].value, sizes[i]);
#if NVN_TEXTURE_NOACCESS_POOL
            if (i == POISON && !NVN_TEXTURE_UNCACHED_POOLS) { if (is_mode) ++mode_noaccess_invalidates; else ++noaccess_pool_invalidates; }
#endif
        }
        stage = "pixels";
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x) {
            struct nn_texture_expected pixel = is_mode ? nn_texture_mode_case_pixel(&input, &mode, x, y) :
                                                        nn_sampler_case_pixel(&input, &sampler_case, x, y);
            if (is_mode) {
                if (pixel.reason == NN_TEXTURE_SHADED) ++mode_shaded;
                else if (pixel.reason == NN_TEXTURE_DISCARD) ++mode_discard;
                else if (pixel.reason == NN_TEXTURE_DEPTH_CLIPPED) { ++mode_clipped; ++case_clipped_pixels; }
                else { error = EINVAL; goto save; }
            } else {
                if (pixel.reason >= NN_TEXTURE_INVALID) { error = EINVAL; goto save; }
                ++reasons[pixel.reason];
            }
            for (unsigned c = 0; c < 4; ++c) {
                unsigned actual = target[y * image->target_pitch + x * 4 + c];
                unsigned delta = actual > pixel.rgba[c] ? actual - pixel.rgba[c] : pixel.rgba[c] - actual;
                if (delta > pixel.tolerance) {
                    bad_case = test; bad_x = x; bad_y = y; bad_channel = c;
                    bad_actual = actual; bad_expected = pixel.rgba[c]; error = EIO; goto save;
                }
                if (is_mode) ++mode_channel_checks; else ++channel_checks;
            }
        }
        stage = "guards";
        for (unsigned i = 0; i < POOLS; ++i) for (size_t byte = 0; byte < sizes[i]; ++byte) {
#if NVN_TEXTURE_NOACCESS_POOL
            if (i == POISON) break; /* Cannot claim host payload/guard readback. */
#endif
            if (i == TARGET && byte >= offsets[RT] && byte - offsets[RT] < lengths[RT]) continue;
            if (memory[i][byte] != expected[i][byte]) {
                bad_case = test; bad_pool = i; bad_offset = byte;
                bad_actual = memory[i][byte]; bad_expected = expected[i][byte]; error = EIO; goto save;
            }
            ++*active_readonly_checks;
            if ((i == TARGET && (byte < offsets[RT] || byte >= offsets[RT] + lengths[RT])) ||
                ((i == TEX0 || i == TEX1 || i == POISON) && (byte < SAMPLE_OFFSET || byte >= SAMPLE_OFFSET + image->texture_bytes)))
                ++*active_guard_checks;
        }
        OBJECT_GUARD(device); OBJECT_GUARD(queue); OBJECT_GUARD(program); OBJECT_GUARD(rt);
        OBJECT_GUARD(sampled_object); OBJECT_GUARD(sampler); OBJECT_GUARD(command);
#if NVN_TEXTURE_NOACCESS_POOL
        OBJECT_GUARD(noaccess_buffer);
#endif
        for (unsigned i = 0; i < POOLS; ++i) { OBJECT_GUARD(pools[i]); }
        for (unsigned i = 0; i < VIEWS; ++i) { OBJECT_GUARD(buffers[i]); }
        for (unsigned i = 0; i < 2; ++i) { OBJECT_GUARD(texture_pools[i]); OBJECT_GUARD(sampler_pools[i]);
            if ((error = texture_probe_bytes(control[i] + control_used[i], sizeof(control[i]) - control_used[i], 0xa5, active_guard_checks))) goto save;
        }
        if (is_mode && (error = texture_probe_bytes(mode_control + mode_control_used,
                sizeof(mode_control) - mode_control_used, 0xa5, active_guard_checks))) goto save;
save:;
        char path[1024];
        int n=snprintf(path,sizeof(path),"%s/sampler-%02u.rgba",directory,test);
        if (n < 0 || (size_t)n >= sizeof(path)) { if (!error) error = ENAMETOOLONG; goto done; }
        FILE *file = fopen(path, "wb"); if (!file) { if (!error) error = errno; goto done; }
        int write_error = fwrite(target, 1, image->target_bytes, file) != image->target_bytes;
        int close_error = fclose(file);
        if (write_error || close_error) { if (!error) error = EIO; goto done; }
        if (error) goto done;
        if (is_mode) {
            n = snprintf(mode_readback_names + mode_names_bytes, sizeof(mode_readback_names) - mode_names_bytes,
                         "%s\"%s.rgba\"", mode_names_bytes ? "," : "", input.name);
            if (n < 0 || (size_t)n >= sizeof(mode_readback_names) - mode_names_bytes) { error = ENOSPC; goto done; }
            mode_names_bytes += (size_t)n;
            CALL(nvnDeviceFinalizeCommandHandle(&device.value, mode_handle)); mode_handle = 0; ++mode_released;
            if (case_clipped_pixels == 4096) mode_clipped_case_mask |= 1u << (test - 5);
            ++mode_completed;
        } else ++completed;
    }
    if(reasons[NN_TEXTURE_OUTSIDE] || reasons[NN_TEXTURE_DISCARD] ||
       reasons[NN_TEXTURE_SHADED]!=NN_SAMPLER_CASES*4096u){error=EINVAL;goto done;}
    stage = "complete";
done:
#if NVN_TEXTURE_DESCRIPTOR_FLAGS
    descriptor_audit.active=0;
#endif
    /* Keep imported pages alive after uncertain GPU completion. The runner
     * terminates this qualification process; never free pending resources. */
    if (pending) cleanup = EBUSY;
    else {
#define CLEAN(expression) do { nn_nvn_clear_error(); expression; int e_ = nn_nvn_error(); if (e_) { cleanup = e_; goto cleanup_failed; } } while (0)
        if (command_ready) CLEAN(nvnCommandBufferFinalize(&command.value));
        if (mode_handle) CLEAN(nvnDeviceFinalizeCommandHandle(&device.value, mode_handle));
        for (unsigned i = 0; i < 2; ++i) if (handles[i]) CLEAN(nvnDeviceFinalizeCommandHandle(&device.value, handles[i]));
        if (sampled_ready) CLEAN(nvnTextureFinalize(&sampled_object.value));
        if (sampler_ready) CLEAN(nvnSamplerFinalize(&sampler.value));
        if (rt_ready) CLEAN(nvnTextureFinalize(&rt.value));
        if (program_ready) CLEAN(nvnProgramFinalize(&program.value));
        for (unsigned i = 0; i < 2; ++i) {
            if (tpool_ready[i]) CLEAN(nvnTexturePoolFinalize(&texture_pools[i].value));
            if (spool_ready[i]) CLEAN(nvnSamplerPoolFinalize(&sampler_pools[i].value));
        }
#if NVN_TEXTURE_NOACCESS_POOL
        if (noaccess_buffer_ready) CLEAN(nvnBufferFinalize(&noaccess_buffer.value));
#endif
        for (unsigned i = 0; i < VIEWS; ++i) if (buffer_ready[i]) CLEAN(nvnBufferFinalize(&buffers[i].value));
        for (unsigned i = 0; i < POOLS; ++i) if (pool_ready[i]) CLEAN(nvnMemoryPoolFinalize(&pools[i].value));
        if (queue_ready) CLEAN(nvnQueueFinalize(&queue.value));
        if (device_ready) CLEAN(nvnDeviceFinalize(&device.value));
        if (!cleanup) for (unsigned i = 0; i < POOLS; ++i) free(memory[i]);
#undef CLEAN
    }
cleanup_failed:
    for (unsigned i = 0; i < POOLS; ++i) free(expected[i]);
    int passed = !error && !cleanup && completed==NN_SAMPLER_CASES &&
        channel_checks==NN_SAMPLER_CASES*16384u && submissions==NN_SAMPLER_CASES && finishes==NN_SAMPLER_CASES &&
        rejections==3 && recorded_handles==2 && copied_binding_arrays==4 &&
        texture_object_finalizations==3 && sampler_object_finalizations==1+NN_SAMPLER_CASES &&
        texture_registrations==4 && sampler_registrations==2+NN_SAMPLER_CASES &&
        inactive_border_checks==2*NN_SAMPLER_CASES &&
        pool_busy_checks==3 && descriptor_busy_checks==4 && program_busy_checks==NN_SAMPLER_CASES &&
        reasons[NN_TEXTURE_SHADED]==NN_SAMPLER_CASES*4096u && guard_checks && readonly_checks;
#if NVN_TEXTURE_NOACCESS_POOL
    passed = passed && noaccess_map_checks == 2 && noaccess_gpuva_checks == 2 &&
             noaccess_pool_flushes == (NVN_TEXTURE_UNCACHED_POOLS ? 0u : 5u) &&
             noaccess_pool_invalidates == (NVN_TEXTURE_UNCACHED_POOLS ? 0u : 5u) &&
             noaccess_buffer_cache_calls == (NVN_TEXTURE_UNCACHED_POOLS ? 0u : 2u) &&
             noaccess_range_rejections == (NVN_TEXTURE_UNCACHED_POOLS ? 0u : 4u) &&
             noaccess_texel_seed_bytes == 16;
#endif
    if (NVN_TEXTURE_MODES) passed = passed && mode_completed == NN_TEXTURE_MODE_CASE_COUNT &&
        mode_submissions == 12 && mode_recorded == 12 && mode_released == 12 && mode_busy_checks == 12 &&
        mode_channel_checks == 196608 && mode_shaded == 18432 && mode_discard == 6144 && mode_clipped == 24576 &&
        mode_getter_checks == 50 && mode_snapshot_mutations == 12 && mode_clipped_case_mask == 0xb2cu &&
        mode_guard_checks && mode_readonly_checks &&
        (!NVN_TEXTURE_NOACCESS_POOL ||
         (mode_noaccess_flushes == (NVN_TEXTURE_UNCACHED_POOLS ? 0u : 12u) &&
          mode_noaccess_invalidates == (NVN_TEXTURE_UNCACHED_POOLS ? 0u : 12u)));
    if (NVN_TEXTURE_UNCACHED_POOLS) {
        unsigned draws = 5 + (NVN_TEXTURE_MODES ? NN_TEXTURE_MODE_CASE_COUNT : 0);
        passed = passed && uncached_pool_maps == POOLS - (NVN_TEXTURE_NOACCESS_POOL ? 1u : 0u) &&
            uncached_buffer_maps == VIEWS && public_cache_calls == 0 &&
            skipped_pool_flushes == 1 + 6 * draws && skipped_pool_invalidates == POOLS * draws &&
            skipped_buffer_cache == (NVN_TEXTURE_NOACCESS_POOL ? 2u : 0u) &&
            skipped_cache_ranges == (NVN_TEXTURE_NOACCESS_POOL ? 4u : 0u) &&
            hot_target_preloads == draws && hot_target_word_checks == 4096 * draws;
    }
#if NVN_TEXTURE_DESCRIPTOR_FLAGS
    unsigned audit_draws=5+(NVN_TEXTURE_MODES ? NN_TEXTURE_MODE_CASE_COUNT : 0);
    passed=passed && descriptor_audit.maps==4 && descriptor_audit.ranges_count==4 &&
        descriptor_audit.attempts==audit_draws && descriptor_audit.delegated==audit_draws &&
        descriptor_audit.accepted==audit_draws && !descriptor_audit.rejected && !descriptor_audit.last_errno &&
        descriptor_audit.object_checks==8*audit_draws && descriptor_audit.private_jobs==audit_draws &&
        descriptor_audit.private_bases==4*audit_draws && descriptor_audit.invalidate_skips==4*audit_draws;
#endif
    int n = snprintf(json, capacity,
        "{\"schema\":1,\"operation\":\"nvn_sampler\",\"uid\":%u,\"stage\":\"%s\","
        "\"descriptor_software_tables\":%s,\"descriptor_metadata_flags\":%u,"
        "\"descriptor_metadata_map_checks\":%u,\"descriptor_metadata_ranges\":%u,"
        "\"descriptor_audit_attempts\":%u,\"descriptor_audit_delegated\":%u,\"descriptor_audit_accepted\":%u,"
        "\"descriptor_audit_rejections\":%u,\"descriptor_audit_errno\":%d,"
        "\"descriptor_excluded_object_checks\":%u,\"descriptor_private_jobs\":%u,\"descriptor_private_base_checks\":%u,"
        "\"descriptor_metadata_invalidate_skips\":%u,"
        "\"uncached_pools_enabled\":%s,\"uncached_pool_flags\":%u,\"uncached_shader_pool_flags\":%u,"
        "\"uncached_pool_map_checks\":%u,\"uncached_buffer_map_checks\":%u,\"public_cache_calls\":%u,"
        "\"skipped_pool_flush_calls\":%u,\"skipped_pool_invalidate_calls\":%u,"
        "\"skipped_buffer_cache_calls\":%u,\"skipped_cache_range_checks\":%u,"
        "\"hot_target_preloads\":%u,\"hot_target_word_checks\":%u,"
        "\"texture_modes\":%s,\"mode_completed_cases\":%u,\"mode_submissions\":%u,\"mode_recorded_handles\":%u,"
        "\"mode_released_handles\":%u,\"mode_channel_checks\":%u,\"mode_shaded_pixels\":%u,\"mode_discard_pixels\":%u,"
        "\"mode_depth_clipped_pixels\":%u,\"mode_getter_checks\":%u,\"mode_program_busy_rejections\":%u,"
        "\"mode_snapshot_mutations\":%u,\"mode_clipped_case_mask\":%u,"
        "\"mode_guard_byte_checks\":%llu,\"mode_readonly_byte_checks\":%llu,\"mode_noaccess_pool_flush_calls\":%u,"
        "\"mode_noaccess_pool_invalidate_calls\":%u,\"mode_readback_names\":[%s],"
        "\"noaccess_pool_enabled\":%s,\"noaccess_pool_flags\":%u,\"noaccess_map_checks\":%u,\"noaccess_gpuva_checks\":%u,"
        "\"noaccess_pool_flush_calls\":%u,\"noaccess_pool_invalidate_calls\":%u,\"noaccess_buffer_cache_calls\":%u,"
        "\"noaccess_range_rejections\":%u,\"noaccess_texel_seed_bytes\":%u,\"noaccess_host_payload_guard_checks\":0,"
        "\"noaccess_diagnostic_cases\":%u,\"noaccess_diagnostic_channel_checks\":%u,"
        "\"device_flags\":%d,\"device_flags_explicit\":%s,\"abi_profile\":\"%s\",\"api_major\":%d,\"api_minor\":%d,"
        "\"errno\":%d,\"cleanup_errno\":%d,\"chip_id\":%u,\"gmem_bytes\":%u,\"device_matches_artifact\":%s,"
        "\"submission\":\"%s\",\"completed_cases\":%u,\"submissions\":%u,\"finishes\":%u,\"recorded_handles\":%u,"
        "\"reused_ib\":%s,\"resources_retained\":%s,\"width\":64,\"height\":64,\"pitch\":256,\"target_bytes\":16384,"
        "\"command_bytes\":%zu,\"diagnostic_command_bytes\":%zu,\"channel_checks\":%u,"
        "\"outside_pixels\":%u,\"discard_pixels\":%u,\"shaded_pixels\":%u,\"diagnostic_channel_checks\":%u,"
        "\"diagnostic_shaded_pixels\":%u,\"pool_identity_cases\":%u,\"binding_rejections\":%u,"
        "\"binding_array_copies\":%u,\"late_descriptor_registrations\":%u,\"texture_registrations\":%u,\"sampler_registrations\":%u,"
        "\"texture_object_finalizations\":%u,\"sampler_object_finalizations\":%u,\"inactive_border_checks\":%u,\"backing_pool_busy_rejections\":%u,"
        "\"descriptor_pool_busy_rejections\":%u,\"program_busy_rejections\":%u,\"texture_pool_id\":257,\"sampler_pool_id\":259,"
        "\"texture_api_slot\":10,\"sampler_api_slot\":0,\"native_texture_index\":1,\"native_sampler_index\":1,"
        "\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,"
        "\"bad_channel\":%u,\"bad_pool\":%u,\"bad_offset\":%zu,\"bad_actual\":%u,\"bad_expected\":%u,\"passed\":%s}\n",
        (unsigned)getuid(), stage,
#if NVN_TEXTURE_DESCRIPTOR_FLAGS
        "true",(unsigned)NVN_TEXTURE_DESCRIPTOR_FLAGS,descriptor_audit.maps,descriptor_audit.ranges_count,
        descriptor_audit.attempts,descriptor_audit.delegated,descriptor_audit.accepted,descriptor_audit.rejected,
        descriptor_audit.last_errno,descriptor_audit.object_checks,descriptor_audit.private_jobs,descriptor_audit.private_bases,descriptor_audit.invalidate_skips,
#else
        "false",0u,0u,0u,0u,0u,0u,0u,0,0u,0u,0u,0u,
#endif
        NVN_TEXTURE_UNCACHED_POOLS ? "true" : "false",
        NVN_TEXTURE_UNCACHED_POOLS ? 0x22u : 0u, NVN_TEXTURE_UNCACHED_POOLS ? 0x62u : 0u,
        uncached_pool_maps, uncached_buffer_maps, public_cache_calls,
        skipped_pool_flushes, skipped_pool_invalidates, skipped_buffer_cache, skipped_cache_ranges,
        hot_target_preloads, hot_target_word_checks,
        NVN_TEXTURE_MODES ? "true" : "false", mode_completed, mode_submissions,
        mode_recorded, mode_released, mode_channel_checks, mode_shaded, mode_discard, mode_clipped,
        mode_getter_checks, mode_busy_checks, mode_snapshot_mutations, mode_clipped_case_mask, (unsigned long long)mode_guard_checks, (unsigned long long)mode_readonly_checks,
        mode_noaccess_flushes, mode_noaccess_invalidates, mode_readback_names,
        NVN_TEXTURE_NOACCESS_POOL ? "true" : "false",
        NVN_TEXTURE_NOACCESS_POOL ? 0xa1u : 0u, noaccess_map_checks, noaccess_gpuva_checks,
        noaccess_pool_flushes, noaccess_pool_invalidates, noaccess_buffer_cache_calls,
        noaccess_range_rejections, noaccess_texel_seed_bytes,
        NVN_TEXTURE_NOACCESS_POOL ? pool_identity_cases : 0u,
        NVN_TEXTURE_NOACCESS_POOL ? diagnostic_channels : 0u,
        device_flags, flags_explicit ? "true" : "false", NN_NVN_ABI_PROFILE,
        api_major, api_minor, error, cleanup, registered ? image->chip : 0, registered ? image->gmem_bytes : 0,
        registered ? "true" : "false", submission, completed, submissions, finishes, recorded_handles,
        submissions > 1 ? "true" : "false", cleanup ? "true" : "false", command_used[0], command_used[1], channel_checks,
        reasons[NN_TEXTURE_OUTSIDE], reasons[NN_TEXTURE_DISCARD], reasons[NN_TEXTURE_SHADED], diagnostic_channels,
        diagnostic_pixels, pool_identity_cases, rejections, copied_binding_arrays, late_registrations,
        texture_registrations, sampler_registrations, texture_object_finalizations, sampler_object_finalizations,
        inactive_border_checks,pool_busy_checks, descriptor_busy_checks, program_busy_checks, (unsigned long long)guard_checks,
        (unsigned long long)readonly_checks, bad_case, bad_x, bad_y, bad_channel, bad_pool, bad_offset,
        bad_actual, bad_expected, passed ? "true" : "false");
#undef POOL_INVALIDATE
#undef POOL_FLUSH
#undef OBJECT_GUARD
#undef BUSY
#undef REQUIRE
#undef BOOL
#undef CALL
#undef mode_control
#undef control
#undef device
#undef queue
#undef program
#undef rt
#undef sampled_object
#undef sampler
#undef command
#undef pools
#undef buffers
#undef texture_pools
#undef sampler_pools
    if(!cleanup)free(objects);
    return passed && n >= 0 && (size_t)n < capacity ? 0 : -1;
}
#undef GUARDED
