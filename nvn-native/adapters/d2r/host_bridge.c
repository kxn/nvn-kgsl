#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include "nvn_native/profile.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NN_NVN_OFFLINE_GRAPHICS_PIPELINE_HEADER
#ifndef NN_D2R_HOST_HLE
#error "Graphics source provider requires original bounded host reader"
#endif
#include "nvn_native/pipeline_nvn.h"
#include <dm/host/platform.h>
#include NN_NVN_OFFLINE_GRAPHICS_PIPELINE_HEADER
#ifdef NN_NVN_COMPLETE_SHADER_SOURCE_HEADER
#include NN_NVN_COMPLETE_SHADER_SOURCE_HEADER
#endif
#ifdef NN_NVN_PIPELINE_NATIVE_VIEWS_HEADER
#include NN_NVN_PIPELINE_NATIVE_VIEWS_HEADER
#endif
static long pipeline_input_reader(void *user,void *out,const void *source,size_t bytes)
{(void)user;return dm_self_read(out,source,bytes);}
#endif
#ifdef NN_NVN_OFFLINE_COMPUTE_HEADER
#include NN_NVN_OFFLINE_COMPUTE_HEADER
#endif
#ifdef NN_NVN_TEXTURE_BRIDGE_HEADER
#include NN_NVN_TEXTURE_BRIDGE_HEADER
#endif

#ifdef NN_NVN_BLOCK_COPY_HEADER
#include NN_NVN_BLOCK_COPY_HEADER
static const struct nn_cs_image *const native_block_copy_image=&NVN_COMPILED_IMAGE;
#undef NVN_COMPILED_IMAGE
#endif
#ifdef NN_NVN_VOLUME_COPY_HEADER
#include NN_NVN_VOLUME_COPY_HEADER
static const struct nn_cs_image *const native_volume_copy_image=&NVN_COMPILED_IMAGE;
#undef NVN_COMPILED_IMAGE
#endif
#ifdef NN_NVN_CLEAR_COLOR_HEADER
#include NN_NVN_CLEAR_COLOR_HEADER
static const struct nn_cs_image *const native_clear_color_image=&NVN_COMPILED_IMAGE;
#undef NVN_COMPILED_IMAGE
#endif

#ifndef NN_D2R_NVN_NAMES_HEADER
#error "Generate the verified D2R name set and define NN_D2R_NVN_NAMES_HEADER"
#endif

static _Noreturn void unsupported(const char *reason, const char *name, int error, const char *phase)
{
    struct nn_nvn_pool_access_observation access;
    if (!nn_nvn_get_pool_access_observation(&access)) {
        /* Identity only: the pool may already be retired. Never dereference it
         * or infer a guest instruction address from the native wrapper. */
        fprintf(stderr, "[native NVN D2R] pool_access: pool=%p flags=0x%08" PRIx32
                " policy=%d offset=%zu size=%zu consumer=%s guest_pc=unavailable\n",
                (const void *)access.pool, (uint32_t)access.flags, (int)access.policy,
                access.offset, access.size, access.consumer ? access.consumer : "(unknown)");
    }
    fprintf(stderr, "[native NVN D2R] %s: %s; errno=%d; phase=%s; terminating execution (exit %d)\n",
            reason, name ? name : "(null)", error, phase, NN_D2R_NVN_UNSUPPORTED_EXIT);
    fflush(stderr);
    _Exit(NN_D2R_NVN_UNSUPPORTED_EXIT);
}

static void check_diagnostic(const char *name, const char *phase)
{
    int error = nn_nvn_error();
    if (error) unsupported("driver error", name, error, phase);
}

static PFNNVNGENERICFUNCPTRPROC require_driver(const char *name)
{
    /* Never clear an earlier diagnostic: it may come from an unwrapped host
     * helper and must stop execution before another guest API is attempted. */
    check_diagnostic(name, "before");
    PFNNVNGENERICFUNCPTRPROC function = nvnBootstrapLoader(name);
    check_diagnostic(name, "lookup");
    if (!function) unsupported("unsupported call", name, ENOSYS, "call");
    return function;
}

/* Actual C prototypes preserve integer widths, float registers, output
 * pointers and aggregate return conventions. Only sticky diagnostics cause a
 * driver-error exit: false/zero can be a legitimate API result. */
#define NN_D2R_FORWARD_VOID(name, abi, result, parameters, arguments) \
    static void forward_##name parameters { \
        abi function = (abi)require_driver(#name); \
        function arguments; \
        check_diagnostic(#name, "after"); \
    } \
    static abi const check_##name __attribute__((unused)) = forward_##name;
#define NN_D2R_FORWARD_VALUE(name, abi, result, parameters, arguments) \
    static result forward_##name parameters { \
        abi function = (abi)require_driver(#name); \
        result value = function arguments; \
        check_diagnostic(#name, "after"); \
        return value; \
    } \
    static abi const check_##name __attribute__((unused)) = forward_##name;
#define NN_D2R_FORWARD_RESOLVER(name, abi, result, parameters, arguments) \
    static abi const check_##name __attribute__((unused)) = nn_d2r_nvn_get_proc_address;
/* Undeclared APIs are fatal boundaries, never forwarded through a fabricated
 * function type even if a future driver exposes their names. */
#define NN_D2R_FORWARD_UNKNOWN(name, abi, result, parameters, arguments) \
    static _Noreturn void unknown_abi_##name(void) { \
        unsupported("unknown ABI call", #name, ENOTSUP, "call"); \
    } \
    static abi const check_##name __attribute__((unused)) = unknown_abi_##name;
#define NN_D2R_NVN_ENTRY(name, abi, result, parameters, arguments, form) \
    NN_D2R_FORWARD_##form(name, abi, result, parameters, arguments)
#include NN_D2R_NVN_NAMES_HEADER
#undef NN_D2R_NVN_ENTRY
#undef NN_D2R_FORWARD_VOID
#undef NN_D2R_FORWARD_VALUE
#undef NN_D2R_FORWARD_RESOLVER
#undef NN_D2R_FORWARD_UNKNOWN
_Static_assert(NN_D2R_NVN_NAMES_SCHEMA == 2, "regenerate typed D2R forwarding metadata");
_Static_assert(NN_D2R_NVN_TYPED_FORWARD_COUNT + NN_D2R_NVN_ADAPTER_RESOLVER_COUNT +
               NN_D2R_NVN_UNKNOWN_ABI_COUNT == NN_D2R_NVN_NAMES_COUNT, "generated forwarding count mismatch");

#define NN_D2R_SYMBOL_VOID(name) forward_##name
#define NN_D2R_SYMBOL_VALUE(name) forward_##name
#define NN_D2R_SYMBOL_RESOLVER(name) nn_d2r_nvn_get_proc_address
#define NN_D2R_SYMBOL_UNKNOWN(name) unknown_abi_##name
static const struct {
    const char *name;
    PFNNVNGENERICFUNCPTRPROC function;
} names[] = {
#define NN_D2R_NVN_ENTRY(name, abi, result, parameters, arguments, form) \
    { #name, (PFNNVNGENERICFUNCPTRPROC)NN_D2R_SYMBOL_##form(name) },
#include NN_D2R_NVN_NAMES_HEADER
#undef NN_D2R_NVN_ENTRY
};
#undef NN_D2R_SYMBOL_VOID
#undef NN_D2R_SYMBOL_VALUE
#undef NN_D2R_SYMBOL_RESOLVER
#undef NN_D2R_SYMBOL_UNKNOWN
_Static_assert(sizeof(names) / sizeof(names[0]) == NN_D2R_NVN_NAMES_COUNT,
               "generated D2R name count mismatch");

/* Observe real bootstrap arguments before the generated wrapper performs its
 * lookup/error checks. These are the public PFN types; no guest arguments or
 * diagnostic state are changed. A C return address here is not a guest PC. */
static void observed_device_flags(NVNdeviceBuilder *builder, int flags)
{
    fprintf(stderr, "[native NVN D2R] nvnDeviceBuilderSetFlags: builder=%p flags=0x%08" PRIx32 "\n",
            (void *)builder, (uint32_t)flags);
    forward_nvnDeviceBuilderSetFlags(builder, flags);
}
static NVNboolean observed_device_initialize(NVNdevice *device, const NVNdeviceBuilder *builder)
{
    fprintf(stderr, "[native NVN D2R] profile=%s reported_api=%d.%d identity=project_declared_partial_abi partial=true strict_unsupported=true\n",
            NN_NVN_ABI_PROFILE, NN_NVN_ABI_MAJOR, NN_NVN_ABI_MINOR);
    fprintf(stderr, "[native NVN D2R] nvnDeviceInitialize: device=%p builder=%p\n",
            (void *)device, (const void *)builder);
    NVNboolean result = forward_nvnDeviceInitialize(device, builder);
    fprintf(stderr, "[native NVN D2R] nvnDeviceInitialize: result=%u\n", (unsigned)result);
#ifdef NN_NVN_OFFLINE_COMPUTE_HEADER
    if (result) {
        /* Immutable compile-time storage outlives all device cache nodes.
         * This is an explicit finite offline provider, never a runtime compiler. */
        for (size_t i=0;i<NN_OFFLINE_COMPUTE_COUNT;++i) {
            int e=nn_nvn_register_compute(device,nn_offline_compute_entries[i]);
            if (e) unsupported("offline compute cache registration", "nvnDeviceInitialize", e, "offline-cache");
            check_diagnostic("nvnDeviceInitialize","offline-cache");
        }
        fprintf(stderr,"[native NVN D2R] offline_compute_cache: entries=%zu runtime_compiler=false\n",(size_t)NN_OFFLINE_COMPUTE_COUNT);
    }
#endif
#ifdef NN_NVN_TEXTURE_BRIDGE_HEADER
    if (result) {
        /* Generated immutable code and layout metadata live through Device.
         * No guest pool is rebound, no runtime compiler or qualifier is linked. */
        int e=nn_nvn_register_texture_bridge(device,&NN_TEXTURE_BRIDGE_BUNDLE);
        if (e) unsupported("native texture bridge registration", "nvnDeviceInitialize", e, "texture-bridge");
        check_diagnostic("nvnDeviceInitialize","texture-bridge");
        fprintf(stderr,"[native NVN D2R] native_texture_bridge: canonical_pool_preserved=true gpu_conversion=true runtime_compiler=false\n");
    }
#endif
#ifdef NN_NVN_BLOCK_COPY_HEADER
    if(result){
        int e=nn_nvn_register_block_copy(device,native_block_copy_image);
        if(e)unsupported("native raw-block kernel registration","nvnDeviceInitialize",e,"raw-block-copy");
        check_diagnostic("nvnDeviceInitialize","raw-block-copy");
        fprintf(stderr,"[native NVN D2R] raw_block_copy: canonical_pool_preserved=true flags0_subset=true runtime_compiler=false\n");
    }
#endif
#ifdef NN_NVN_VOLUME_COPY_HEADER
    if(result){
        int e=nn_nvn_register_volume_copy(device,native_volume_copy_image);
        if(e)unsupported("native volume kernel registration","nvnDeviceInitialize",e,"volume-copy");
        check_diagnostic("nvnDeviceInitialize","volume-copy");
        fprintf(stderr,"[native NVN D2R] volume_copy: canonical_pool_preserved=true true3d_z=true flags0_subset=true runtime_compiler=false\n");
    }
#endif
#ifdef NN_NVN_CLEAR_COLOR_HEADER
    if(result){
        int e=nn_nvn_register_clear_color(device,native_clear_color_image);
        if(e)unsupported("native clear kernel registration","nvnDeviceInitialize",e,"clear-color");
        check_diagnostic("nvnDeviceInitialize","clear-color");
        fprintf(stderr,"[native NVN D2R] native_clear_color: canonical_pool_preserved=true gpu_write=true cpu_constant_texels=1 runtime_compiler=false\n");
    }
#endif
#ifdef NN_NVN_OFFLINE_GRAPHICS_PIPELINE_HEADER
    if(result) {
#ifdef NN_GRAPHICS_PIPELINE_HAS_TEXTURE_TYPES
        int e=nn_nvn_register_typed_pipeline_archive(device,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,pipeline_input_reader,NULL);
#else
        int e=nn_nvn_register_pipeline_archive(device,&NN_GRAPHICS_PIPELINE_ARCHIVE,pipeline_input_reader,NULL);
#endif
        if(e)unsupported("offline graphics source registration","nvnDeviceInitialize",e,"graphics-source-cache");
        check_diagnostic("nvnDeviceInitialize","graphics-source-cache");
#if defined(NN_NVN_DEPTH_TARGETS) && NN_NVN_DEPTH_TARGETS
#ifndef NN_GRAPHICS_PIPELINE_HAS_DEPTH_ARCHIVE
#error Native depth requires an explicitly published typed depth archive
#endif
        e=nn_nvn_register_pipeline_depth_archive(device,&NN_GRAPHICS_PIPELINE_DEPTH_ARCHIVE);
        if(e)unsupported("typed depth archive registration","nvnDeviceInitialize",e,"graphics-depth-targets");
        check_diagnostic("nvnDeviceInitialize","graphics-depth-targets");
        fprintf(stderr,"[native NVN D2R] native_depth_targets: exact_program_sidecar=true canonical_gpu_load_store=true stencil=false\n");
#endif
#if defined(NN_NVN_PIPELINE_RUNTIME_VIEWS) && NN_NVN_PIPELINE_RUNTIME_VIEWS
        e=nn_nvn_enable_pipeline_runtime_views(device);
        if(e)unsupported("runtime FDL metadata registration","nvnDeviceInitialize",e,"graphics-runtime-views");
        check_diagnostic("nvnDeviceInitialize","graphics-runtime-views");
        fprintf(stderr,"[native NVN D2R] runtime_graphics_native_views: actual_fdl=true immutable_device_cache=true static_dimensions=false\n");
#elif defined(NN_NVN_PIPELINE_NATIVE_VIEWS_HEADER)
        e=nn_nvn_register_pipeline_views(device,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT);
        if(e)unsupported("offline graphics native-view registration","nvnDeviceInitialize",e,"graphics-native-views");
        check_diagnostic("nvnDeviceInitialize","graphics-native-views");
        fprintf(stderr,"[native NVN D2R] finite_graphics_native_views: entries=%zu unresolved_resources_rejected=true\n",(size_t)NN_TEXTURE_NATIVE_VIEW_COUNT);
#endif
#ifdef NN_NVN_COMPLETE_SHADER_SOURCE_HEADER
        e=nn_nvn_register_shader_source_catalogue(device,&NN_COMPLETE_GRAPHICS_SOURCE_ARCHIVE);
        if(e)unsupported("complete original source registration","nvnDeviceInitialize",e,"complete-shader-sources");
        check_diagnostic("nvnDeviceInitialize","complete-shader-sources");
        fprintf(stderr,"[native NVN D2R] complete_shader_sources: entries=%zu source_identity_only=true native_compilation=false actual_draw_specialization_required=true\n",NN_COMPLETE_GRAPHICS_SOURCE_ARCHIVE.count);
#endif
        fprintf(stderr,"[native NVN D2R] offline_graphics_source_cache: stages=%zu pairs=%zu runtime_compiler=false full_source_match=true per_draw_resolution=true\n",NN_GRAPHICS_PIPELINE_ARCHIVE.stages->stage_count,NN_GRAPHICS_PIPELINE_ARCHIVE.pair_count);
    }
#endif
    return result;
}
static void observed_device_integer(const NVNdevice *device, NVNdeviceInfo property, int *value)
{
    fprintf(stderr, "[native NVN D2R] nvnDeviceGetInteger: device=%p property=%d output=%p\n",
            (const void *)device, (int)property, (void *)value);
    forward_nvnDeviceGetInteger(device, property, value);
    /* The forwarded call has returned with no sticky diagnostic. */
    if (value) fprintf(stderr, "[native NVN D2R] nvnDeviceGetInteger: property=%d value=%d\n",
                       (int)property, *value);
}
/* These entry observations precede require_driver(), including its old-sticky
 * and missing-implementation checks. Do not dereference guest storage or infer
 * a guest instruction address from this C wrapper's return address. */
static void observed_window_origin(NVNdevice *device, NVNwindowOriginMode mode)
{
    fprintf(stderr, "[native NVN D2R] nvnDeviceSetWindowOriginMode: device=%p mode=%d guest_pc=unavailable\n",
            (void *)device, (int)mode);
    fflush(stderr);
    forward_nvnDeviceSetWindowOriginMode(device, mode);
}
static void observed_depth_mode(NVNdevice *device, NVNdepthMode mode)
{
    fprintf(stderr, "[native NVN D2R] nvnDeviceSetDepthMode: device=%p mode=%d guest_pc=unavailable\n",
            (void *)device, (int)mode);
    fflush(stderr);
    forward_nvnDeviceSetDepthMode(device, mode);
}
static void observed_pool_flags(NVNmemoryPoolBuilder *builder, int flags)
{
    fprintf(stderr, "[native NVN D2R] nvnMemoryPoolBuilderSetFlags: builder=%p flags=0x%08" PRIx32 " guest_pc=unavailable\n",
            (void *)builder, (uint32_t)flags);
    fflush(stderr);
    forward_nvnMemoryPoolBuilderSetFlags(builder, flags);
}
static void observed_pool_storage(NVNmemoryPoolBuilder *builder, void *memory, size_t size)
{
    fprintf(stderr, "[native NVN D2R] nvnMemoryPoolBuilderSetStorage: builder=%p storage=%p size=%zu guest_pc=unavailable\n",
            (void *)builder, memory, size);
    fflush(stderr);
    forward_nvnMemoryPoolBuilderSetStorage(builder, memory, size);
}
static NVNboolean observed_pool_initialize(NVNmemoryPool *pool, const NVNmemoryPoolBuilder *builder)
{
    fprintf(stderr, "[native NVN D2R] nvnMemoryPoolInitialize: pool=%p builder=%p guest_pc=unavailable\n",
            (void *)pool, (const void *)builder);
    fflush(stderr);
    NVNboolean result = forward_nvnMemoryPoolInitialize(pool, builder);
    fprintf(stderr, "[native NVN D2R] nvnMemoryPoolInitialize: result=%u\n", (unsigned)result);
    return result;
}
static PFNNVNDEVICEBUILDERSETFLAGSPROC const observe_flags = observed_device_flags;
static PFNNVNDEVICEINITIALIZEPROC const observe_initialize = observed_device_initialize;
static PFNNVNDEVICEGETINTEGERPROC const observe_integer = observed_device_integer;
static PFNNVNDEVICESETWINDOWORIGINMODEPROC const observe_window_origin = observed_window_origin;
static PFNNVNDEVICESETDEPTHMODEPROC const observe_depth_mode = observed_depth_mode;
static PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC const observe_pool_flags = observed_pool_flags;
static PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC const observe_pool_storage = observed_pool_storage;
static PFNNVNMEMORYPOOLINITIALIZEPROC const observe_pool_initialize = observed_pool_initialize;

/* Observations only: no queue budget validation or memory access here. The
 * typed forward retains all sticky/error and return-value decisions. */
static void observed_queue_flags(NVNqueueBuilder *builder, int flags)
{
    fprintf(stderr, "[native NVN D2R] nvnQueueBuilderSetFlags: builder=%p flags=0x%08" PRIx32 " guest_pc=unavailable\n",
            (void *)builder, (uint32_t)flags);
    fflush(stderr);
    forward_nvnQueueBuilderSetFlags(builder, flags);
}
static PFNNVNQUEUEBUILDERSETFLAGSPROC const observe_queue_flags = observed_queue_flags;

#define OBSERVE_QUEUE_SIZE(name, type, field) \
    static void observed_##name(NVNqueueBuilder *builder, size_t value) { \
        fprintf(stderr, "[native NVN D2R] " #name ": builder=%p " #field "=%zu guest_pc=unavailable\n", \
                (void *)builder, value); \
        fflush(stderr); \
        forward_##name(builder, value); \
    } \
    static type const observe_##name = observed_##name;
OBSERVE_QUEUE_SIZE(nvnQueueBuilderSetCommandMemorySize, PFNNVNQUEUEBUILDERSETCOMMANDMEMORYSIZEPROC, size)
OBSERVE_QUEUE_SIZE(nvnQueueBuilderSetComputeMemorySize, PFNNVNQUEUEBUILDERSETCOMPUTEMEMORYSIZEPROC, size)
OBSERVE_QUEUE_SIZE(nvnQueueBuilderSetControlMemorySize, PFNNVNQUEUEBUILDERSETCONTROLMEMORYSIZEPROC, size)
OBSERVE_QUEUE_SIZE(nvnQueueBuilderSetCommandFlushThreshold, PFNNVNQUEUEBUILDERSETCOMMANDFLUSHTHRESHOLDPROC, threshold)
#undef OBSERVE_QUEUE_SIZE

static size_t observed_queue_memory_size(const NVNqueueBuilder *builder)
{
    fprintf(stderr, "[native NVN D2R] nvnQueueBuilderGetQueueMemorySize: builder=%p guest_pc=unavailable\n",
            (const void *)builder);
    fflush(stderr);
    size_t result = forward_nvnQueueBuilderGetQueueMemorySize(builder);
    fprintf(stderr, "[native NVN D2R] nvnQueueBuilderGetQueueMemorySize: result=%zu\n", result);
    return result;
}
static PFNNVNQUEUEBUILDERGETQUEUEMEMORYSIZEPROC const observe_queue_memory_size = observed_queue_memory_size;

static void observed_queue_memory(NVNqueueBuilder *builder, void *memory, size_t size)
{
    fprintf(stderr, "[native NVN D2R] nvnQueueBuilderSetQueueMemory: builder=%p storage=%p size=%zu guest_pc=unavailable\n",
            (void *)builder, memory, size);
    fflush(stderr);
    forward_nvnQueueBuilderSetQueueMemory(builder, memory, size);
}
static PFNNVNQUEUEBUILDERSETQUEUEMEMORYPROC const observe_queue_memory = observed_queue_memory;

static NVNboolean observed_queue_initialize(NVNqueue *queue, const NVNqueueBuilder *builder)
{
    fprintf(stderr, "[native NVN D2R] nvnQueueInitialize: queue=%p builder=%p guest_pc=unavailable\n",
            (void *)queue, (const void *)builder);
    fflush(stderr);
    NVNboolean result = forward_nvnQueueInitialize(queue, builder);
    fprintf(stderr, "[native NVN D2R] nvnQueueInitialize: result=%u\n", (unsigned)result);
    return result;
}
static PFNNVNQUEUEINITIALIZEPROC const observe_queue_initialize = observed_queue_initialize;

/* Scalar observation precedes lookup/driver call. No guest PC is inferred. */
#define OBSERVE_INTERVAL_SET(name,type,pfn) \
static void observed_##name(type *object,int interval) { \
    fprintf(stderr,"[native NVN D2R] " #name ": object=%p interval=%d guest_pc=unavailable\n",(void *)object,interval); \
    fflush(stderr);forward_##name(object,interval); \
} \
static pfn const observe_##name=observed_##name;
#define OBSERVE_INTERVAL_GET(name,type,pfn) \
static int observed_##name(const type *object) { \
    fprintf(stderr,"[native NVN D2R] " #name ": object=%p guest_pc=unavailable\n",(const void *)object); \
    fflush(stderr);int result=forward_##name(object); \
    fprintf(stderr,"[native NVN D2R] " #name ": result=%d\n",result);fflush(stderr);return result; \
} \
static pfn const observe_##name=observed_##name;
OBSERVE_INTERVAL_SET(nvnWindowBuilderSetPresentInterval,NVNwindowBuilder,PFNNVNWINDOWBUILDERSETPRESENTINTERVALPROC)
OBSERVE_INTERVAL_GET(nvnWindowBuilderGetPresentInterval,NVNwindowBuilder,PFNNVNWINDOWBUILDERGETPRESENTINTERVALPROC)
OBSERVE_INTERVAL_SET(nvnWindowSetPresentInterval,NVNwindow,PFNNVNWINDOWSETPRESENTINTERVALPROC)
OBSERVE_INTERVAL_GET(nvnWindowGetPresentInterval,NVNwindow,PFNNVNWINDOWGETPRESENTINTERVALPROC)
#undef OBSERVE_INTERVAL_SET
#undef OBSERVE_INTERVAL_GET

/* Observe the complete texture-builder family before its real typed forward.
 * Scalars/pointer identities only: no inferred flags, guest reads or defaults.
 * A failed layout/create therefore retains the actual preceding arguments. */
#define OBSERVE_TEXTURE_SCALAR(name, pfn, type, format, cast) \
static void observed_##name(NVNtextureBuilder *builder,type value) { \
    fprintf(stderr,"[native NVN D2R] " #name ": builder=%p value=" format " guest_pc=unavailable\n",(void *)builder,cast value); \
    fflush(stderr); forward_##name(builder,value); \
} \
static pfn const observe_##name=observed_##name;
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetFlags,PFNNVNTEXTUREBUILDERSETFLAGSPROC,int,"0x%08" PRIx32,(uint32_t))
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetTarget,PFNNVNTEXTUREBUILDERSETTARGETPROC,NVNtextureTarget,"%d",(int))
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetWidth,PFNNVNTEXTUREBUILDERSETWIDTHPROC,int,"%d",(int))
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetHeight,PFNNVNTEXTUREBUILDERSETHEIGHTPROC,int,"%d",(int))
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetDepth,PFNNVNTEXTUREBUILDERSETDEPTHPROC,int,"%d",(int))
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetSize1D,PFNNVNTEXTUREBUILDERSETSIZE1DPROC,int,"%d",(int))
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetLevels,PFNNVNTEXTUREBUILDERSETLEVELSPROC,int,"%d",(int))
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetSamples,PFNNVNTEXTUREBUILDERSETSAMPLESPROC,int,"%d",(int))
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetFormat,PFNNVNTEXTUREBUILDERSETFORMATPROC,NVNformat,"%d",(int))
OBSERVE_TEXTURE_SCALAR(nvnTextureBuilderSetStride,PFNNVNTEXTUREBUILDERSETSTRIDEPROC,ptrdiff_t,"%td",(ptrdiff_t))
#undef OBSERVE_TEXTURE_SCALAR
static void observed_nvnTextureBuilderSetSize2D(NVNtextureBuilder *builder,int width,int height)
{
    fprintf(stderr,"[native NVN D2R] nvnTextureBuilderSetSize2D: builder=%p width=%d height=%d guest_pc=unavailable\n",(void *)builder,width,height);
    fflush(stderr);forward_nvnTextureBuilderSetSize2D(builder,width,height);
}
static PFNNVNTEXTUREBUILDERSETSIZE2DPROC const observe_nvnTextureBuilderSetSize2D=observed_nvnTextureBuilderSetSize2D;
static void observed_nvnTextureBuilderSetSize3D(NVNtextureBuilder *builder,int width,int height,int depth)
{
    fprintf(stderr,"[native NVN D2R] nvnTextureBuilderSetSize3D: builder=%p width=%d height=%d depth=%d guest_pc=unavailable\n",(void *)builder,width,height,depth);
    fflush(stderr);forward_nvnTextureBuilderSetSize3D(builder,width,height,depth);
}
static PFNNVNTEXTUREBUILDERSETSIZE3DPROC const observe_nvnTextureBuilderSetSize3D=observed_nvnTextureBuilderSetSize3D;
static void observed_nvnTextureBuilderSetStorage(NVNtextureBuilder *builder,NVNmemoryPool *pool,ptrdiff_t offset)
{
    fprintf(stderr,"[native NVN D2R] nvnTextureBuilderSetStorage: builder=%p pool=%p offset=%td guest_pc=unavailable\n",(void *)builder,(void *)pool,offset);
    fflush(stderr);forward_nvnTextureBuilderSetStorage(builder,pool,offset);
}
static PFNNVNTEXTUREBUILDERSETSTORAGEPROC const observe_nvnTextureBuilderSetStorage=observed_nvnTextureBuilderSetStorage;
static void observe_texture_builder_storage(const NVNtextureBuilder *builder)
{
    struct nn_nvn_texture_builder_observation v;
    int e=nn_nvn_get_texture_builder_observation(builder,&v);
    if(e){fprintf(stderr,"[native NVN D2R] texture_builder_storage: observation_errno=%d\n",e);return;}
    fprintf(stderr,"[native NVN D2R] texture_builder_storage: fields=0x%08" PRIx32
        " device_live=%u source_policy=%u flags=0x%08" PRIx32
        " target=%d format=%d width=%d height=%d depth=%d levels=%d samples=%d stride=%td"
        " packaged=%u source_layout_errno=%d packaged_bytes=",
        v.fields,v.device_live,v.source_policy,(uint32_t)v.flags,v.target,v.format,
        v.width,v.height,v.depth,v.levels,v.samples,v.stride,v.packaged,v.source_layout_errno);
    for(unsigned i=0;i<sizeof(v.packaged_layout);i++)fprintf(stderr,"%s%02x",i?":":"",v.packaged_layout[i]);
    fputc('\n',stderr);
}
#define OBSERVE_TEXTURE_SIZE(name,pfn) \
static size_t observed_##name(const NVNtextureBuilder *builder) { \
    fprintf(stderr,"[native NVN D2R] " #name ": builder=%p guest_pc=unavailable\n",(const void *)builder);fflush(stderr); \
    observe_texture_builder_storage(builder);fflush(stderr); \
    size_t result=forward_##name(builder); \
    fprintf(stderr,"[native NVN D2R] " #name ": result=%zu\n",result);return result; \
} \
static pfn const observe_##name=observed_##name;
OBSERVE_TEXTURE_SIZE(nvnTextureBuilderGetStorageSize,PFNNVNTEXTUREBUILDERGETSTORAGESIZEPROC)
OBSERVE_TEXTURE_SIZE(nvnTextureBuilderGetStorageAlignment,PFNNVNTEXTUREBUILDERGETSTORAGEALIGNMENTPROC)
#undef OBSERVE_TEXTURE_SIZE
static NVNboolean observed_nvnTextureInitialize(NVNtexture *texture,const NVNtextureBuilder *builder)
{
    fprintf(stderr,"[native NVN D2R] nvnTextureInitialize: texture=%p builder=%p guest_pc=unavailable\n",(void *)texture,(const void *)builder);fflush(stderr);
    NVNboolean result=forward_nvnTextureInitialize(texture,builder);
    fprintf(stderr,"[native NVN D2R] nvnTextureInitialize: result=%u\n",(unsigned)result);return result;
}
static PFNNVNTEXTUREINITIALIZEPROC const observe_nvnTextureInitialize=observed_nvnTextureInitialize;

/* Descriptor table role is observed before any unsupported/sticky boundary;
 * do not inspect caller storage or infer a guest PC. */
#define OBSERVE_DESCRIPTOR(label,object_type,pfn) \
static NVNboolean observed_##label(object_type *object,const NVNmemoryPool *pool,ptrdiff_t offset,int count) \
{ \
    fprintf(stderr,"[native NVN D2R] " #label ": object=%p pool=%p offset=%td count=%d guest_pc=unavailable\n", \
            (void *)object,(const void *)pool,offset,count); fflush(stderr); \
    NVNboolean result=forward_##label(object,pool,offset,count); \
    fprintf(stderr,"[native NVN D2R] " #label ": result=%u\n",(unsigned)result); return result; \
} \
static pfn const observe_##label=observed_##label;
OBSERVE_DESCRIPTOR(nvnTexturePoolInitialize,NVNtexturePool,PFNNVNTEXTUREPOOLINITIALIZEPROC)
OBSERVE_DESCRIPTOR(nvnSamplerPoolInitialize,NVNsamplerPool,PFNNVNSAMPLERPOOLINITIALIZEPROC)
#undef OBSERVE_DESCRIPTOR

/* Handle construction is distinct from image binding or shader image access.
 * Observe the original signed ID before lookup/sticky checks, without claiming
 * the still-unsupported image path or manufacturing a guest PC. */
static NVNimageHandle observed_image_handle(const NVNdevice *device,int id)
{
    fprintf(stderr,"[native NVN D2R] nvnDeviceGetImageHandle: device=%p id=%d guest_pc=unavailable\n",
            (const void *)device,id); fflush(stderr);
    NVNimageHandle result=forward_nvnDeviceGetImageHandle(device,id);
    fprintf(stderr,"[native NVN D2R] nvnDeviceGetImageHandle: result=0x%016" PRIx64 "\n",result);
    return result;
}
static PFNNVNDEVICEGETIMAGEHANDLEPROC const observe_image_handle=observed_image_handle;

/* Production reuses the original host's bounded self-read helper. Standalone
 * native builds remain independent of dm; host tests may inject a typed reader. */
#if defined(NN_D2R_PROGRAM_INPUT_OBSERVE) && NN_D2R_PROGRAM_INPUT_OBSERVE
#ifndef NN_D2R_HOST_HLE
#error "Production program-input observation requires the original host reader"
#endif
#include <dm/host/platform.h>
#define NN_D2R_PROGRAM_INPUT_READER dm_self_read
#endif
#ifdef NN_D2R_PROGRAM_INPUT_READER
static uint32_t observed_le32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static int observed_read(void *out,const void *address,size_t bytes,const char *part,int index)
{
    uintptr_t start=(uintptr_t)address;
    if (!start || bytes>UINTPTR_MAX-start) {
        fprintf(stderr,"[native NVN D2R] shader_input: index=%d part=%s unavailable=invalid_range\n",index,part);
        return 0;
    }
    long got=NN_D2R_PROGRAM_INPUT_READER(out,address,bytes);
    if (got!=(long)bytes) {
        int read_error=errno;
        fprintf(stderr,"[native NVN D2R] shader_input: index=%d part=%s unavailable=read got=%ld requested=%zu errno=%d\n",
                index,part,got,bytes,got<0?read_error:0);
        return 0;
    }
    return 1;
}
static void observed_program_inputs(NVNprogram *program,int count,const NVNshaderData *shaders)
{
    int saved_errno=errno;
    if (nn_nvn_error() || count<1 || count>6) return;
    NVNshaderData snapshot[6];
    if (!observed_read(snapshot,shaders,(size_t)count*sizeof(snapshot[0]),"array",-1)) goto done;
    for (int i=0;i<count;++i) {
        fprintf(stderr,"[native NVN D2R] shader_input: index=%d data=0x%016" PRIx64 " control=%p\n",
                i,snapshot[i].data,snapshot[i].control);
        struct nn_nvn_program_source_info info;
        int e=nn_nvn_get_program_source_info(program,snapshot[i].data,&info);
        if (e) fprintf(stderr,"[native NVN D2R] shader_source: index=%d unavailable=%d\n",i,e);
        else fprintf(stderr,"[native NVN D2R] shader_source: index=%d pool=%p flags=0x%08" PRIx32
                     " policy=%d offset=%zu pool_remaining=%zu cpu_readable=%u code_bytes=unknown\n",
                     i,(const void *)info.pool,(uint32_t)info.flags,(int)info.policy,info.offset,info.available,info.cpu_readable);
        unsigned char prefix[4],control_snapshot[0x898];
        if (!observed_read(prefix,snapshot[i].control,sizeof(prefix),"control_prefix",i)) continue;
        uint32_t magic=observed_le32(prefix);
        fprintf(stderr,"[native NVN D2R] shader_control: index=%d prefix=0x%08" PRIx32 "\n",i,magic);
        /* This magic/field position is independently checked by the original
         * embedded extractor. It is only an observed candidate header: the
         * raw words are neither validated lengths nor a format/stage claim. */
        if (magic!=UINT32_C(0x98761234)) continue;
        if (!observed_read(control_snapshot,snapshot[i].control,sizeof(control_snapshot),"control_snapshot_898",i)) continue;
        if (observed_le32(control_snapshot)!=magic) {
            fprintf(stderr,"[native NVN D2R] shader_control: index=%d snapshot=unavailable_changed_prefix\n",i);continue;
        }
        fprintf(stderr,"[native NVN D2R] shader_control: index=%d raw700=%" PRIu32 " raw704=%" PRIu32 " lengths_validated=false\n",
                i,observed_le32(control_snapshot+0x700),observed_le32(control_snapshot+0x704));
        /* Original game bytes stay in the private run log, never tracked
         * fixtures. A complete read is required before any snapshot hex. */
        for (size_t offset=0;offset<sizeof(control_snapshot);offset+=32) {
            static const char digits[]="0123456789abcdef";
            char hex[65];
            size_t chunk=sizeof(control_snapshot)-offset; if(chunk>32)chunk=32;
            for (size_t b=0;b<chunk;++b) { unsigned v=control_snapshot[offset+b];hex[2*b]=digits[v>>4];hex[2*b+1]=digits[v&15]; }
            hex[chunk*2]=0;
            fprintf(stderr,"[native NVN D2R] shader_control_snapshot: index=%d profile=caller_898_candidate offset=0x%03zx bytes=%zu hex=%s lengths_validated=false\n",i,offset,chunk,hex);
        }
    }
done:
    fflush(stderr); errno=saved_errno;
}
#endif

/* Observe only ABI scalars and pointer identities. The array and control/code
 * storage have not been validated here; do not dereference them, infer a stage
 * from count, or label a native return address as a guest PC. This observation
 * does not register a shader fixture or change the driver's strict miss. */
static NVNboolean observed_program_shaders(NVNprogram *program,int count,const NVNshaderData *shaders)
{
    fprintf(stderr,"[native NVN D2R] nvnProgramSetShaders: program=%p count=%d shaders=%p guest_pc=unavailable\n",
            (void *)program,count,(const void *)shaders); fflush(stderr);
#ifdef NN_D2R_PROGRAM_INPUT_READER
    observed_program_inputs(program,count,shaders);
#endif
    NVNboolean result=forward_nvnProgramSetShaders(program,count,shaders);
    fprintf(stderr,"[native NVN D2R] nvnProgramSetShaders: result=%u\n",(unsigned)result);
    return result;
}
static PFNNVNPROGRAMSETSHADERSPROC const observe_program_shaders=observed_program_shaders;

/* Observe public descriptor/view/binding calls, never the opaque object bytes.
 * Preserve incoming errno through logging/optional bounded reads. Driver
 * forwarding and its sticky diagnostic contract remain unchanged. */
static void observed_register_texture(const NVNtexturePool *pool,int id,const NVNtexture *texture,const NVNtextureView *view)
{
    int saved_errno=errno;
    fprintf(stderr,"[native NVN D2R] nvnTexturePoolRegisterTexture: pool=%p id=%d texture=%p view=%p view_fields=opaque_not_inspected guest_pc=unavailable\n",(const void *)pool,id,(const void *)texture,(const void *)view);
    fflush(stderr);errno=saved_errno;forward_nvnTexturePoolRegisterTexture(pool,id,texture,view);
}
static PFNNVNTEXTUREPOOLREGISTERTEXTUREPROC const observe_register_texture=observed_register_texture;
static void observed_set_texture_pool(NVNcommandBuffer *command,const NVNtexturePool *pool)
{
    int saved_errno=errno;
    fprintf(stderr,"[native NVN D2R] nvnCommandBufferSetTexturePool: command=%p pool=%p guest_pc=unavailable\n",(void *)command,(const void *)pool);
    fflush(stderr);errno=saved_errno;forward_nvnCommandBufferSetTexturePool(command,pool);
}
static PFNNVNCOMMANDBUFFERSETTEXTUREPOOLPROC const observe_set_texture_pool=observed_set_texture_pool;
static void observed_separate_texture(NVNcommandBuffer *command,NVNshaderStage stage,int index,NVNseparateTextureHandle handle)
{
    int saved_errno=errno;
    fprintf(stderr,"[native NVN D2R] nvnCommandBufferBindSeparateTexture: command=%p domain=separate_texture stage=%d index=%d handle=0x%016" PRIx64 " guest_pc=unavailable\n",(void *)command,(int)stage,index,handle.value);
    fflush(stderr);errno=saved_errno;forward_nvnCommandBufferBindSeparateTexture(command,stage,index,handle);
}
static PFNNVNCOMMANDBUFFERBINDSEPARATETEXTUREPROC const observe_separate_texture=observed_separate_texture;
static void observed_separate_textures(NVNcommandBuffer *command,NVNshaderStage stage,int first,int count,const NVNseparateTextureHandle *handles)
{
    int saved_errno=errno;
    fprintf(stderr,"[native NVN D2R] nvnCommandBufferBindSeparateTextures: command=%p domain=separate_texture stage=%d first=%d count=%d handles=%p guest_pc=unavailable\n",(void *)command,(int)stage,first,count,(const void *)handles);
    if(nn_nvn_error())fprintf(stderr,"[native NVN D2R] texture_binding_array: values=unknown reason=prior_diagnostic\n");
    else if(count==0)fprintf(stderr,"[native NVN D2R] texture_binding_array: values=empty\n");
    else if(count<0||count>32)fprintf(stderr,"[native NVN D2R] texture_binding_array: values=unknown reason=observation_count_limit max=32\n");
    else {
#ifdef NN_D2R_PROGRAM_INPUT_READER
        /* NVN public handle array only. All-or-nothing bounded read; no opaque
         * texture/view/table memory is interpreted and no extra getter runs. */
        _Static_assert(sizeof(NVNseparateTextureHandle)==8,"public separate texture handle ABI");
        uintptr_t start=(uintptr_t)handles;size_t bytes=(size_t)count*sizeof(*handles);
        if(!start||bytes>UINTPTR_MAX-start)fprintf(stderr,"[native NVN D2R] texture_binding_array: values=unknown reason=invalid_range\n");
        else {
            NVNseparateTextureHandle snapshot[32];long got=NN_D2R_PROGRAM_INPUT_READER(snapshot,handles,bytes);
            if(got!=(long)bytes)fprintf(stderr,"[native NVN D2R] texture_binding_array: values=unknown reason=read got=%ld requested=%zu errno=%d\n",got,bytes,got<0?errno:0);
            else for(int i=0;i<count;i++)fprintf(stderr,"[native NVN D2R] texture_binding_array: command=%p domain=separate_texture stage=%d first=%d element=%d handle=0x%016" PRIx64 " guest_pc=unavailable\n",(void *)command,(int)stage,first,i,snapshot[i].value);
        }
#else
        fprintf(stderr,"[native NVN D2R] texture_binding_array: values=unknown reason=reader_not_enabled\n");
#endif
    }
    fflush(stderr);errno=saved_errno;forward_nvnCommandBufferBindSeparateTextures(command,stage,first,count,handles);
}
static PFNNVNCOMMANDBUFFERBINDSEPARATETEXTURESPROC const observe_separate_textures=observed_separate_textures;
static void observed_view_defaults(NVNtextureView *view)
{
    int saved_errno=errno;
    fprintf(stderr,"[native NVN D2R] nvnTextureViewSetDefaults: view=%p fields=public_default_operation_not_readback guest_pc=unavailable\n",(void *)view);
    fflush(stderr);errno=saved_errno;forward_nvnTextureViewSetDefaults(view);
}
static PFNNVNTEXTUREVIEWSETDEFAULTSPROC const observe_view_defaults=observed_view_defaults;
#define OBSERVE_VIEW_PAIR(name,pfn) \
static void observed_##name(NVNtextureView *view,int base,int count) { \
    int saved_errno=errno; \
    fprintf(stderr,"[native NVN D2R] " #name ": view=%p base=%d count=%d guest_pc=unavailable\n",(void *)view,base,count); \
    fflush(stderr);errno=saved_errno;forward_##name(view,base,count); \
} \
static pfn const observe_##name=observed_##name;
OBSERVE_VIEW_PAIR(nvnTextureViewSetLevels,PFNNVNTEXTUREVIEWSETLEVELSPROC)
OBSERVE_VIEW_PAIR(nvnTextureViewSetLayers,PFNNVNTEXTUREVIEWSETLAYERSPROC)
#undef OBSERVE_VIEW_PAIR
#define OBSERVE_VIEW_VALUE(name,pfn,type) \
static void observed_##name(NVNtextureView *view,type value) { \
    int saved_errno=errno; \
    fprintf(stderr,"[native NVN D2R] " #name ": view=%p value=%d guest_pc=unavailable\n",(void *)view,(int)value); \
    fflush(stderr);errno=saved_errno;forward_##name(view,value); \
} \
static pfn const observe_##name=observed_##name;
OBSERVE_VIEW_VALUE(nvnTextureViewSetFormat,PFNNVNTEXTUREVIEWSETFORMATPROC,NVNformat)
OBSERVE_VIEW_VALUE(nvnTextureViewSetTarget,PFNNVNTEXTUREVIEWSETTARGETPROC,NVNtextureTarget)
OBSERVE_VIEW_VALUE(nvnTextureViewSetDepthStencilMode,PFNNVNTEXTUREVIEWSETDEPTHSTENCILMODEPROC,NVNtextureDepthStencilMode)
#undef OBSERVE_VIEW_VALUE
static void observed_view_swizzle(NVNtextureView *view,NVNtextureSwizzle r,NVNtextureSwizzle g,NVNtextureSwizzle b,NVNtextureSwizzle a)
{
    int saved_errno=errno;
    fprintf(stderr,"[native NVN D2R] nvnTextureViewSetSwizzle: view=%p r=%d g=%d b=%d a=%d guest_pc=unavailable\n",(void *)view,(int)r,(int)g,(int)b,(int)a);
    fflush(stderr);errno=saved_errno;forward_nvnTextureViewSetSwizzle(view,r,g,b,a);
}
static PFNNVNTEXTUREVIEWSETSWIZZLEPROC const observe_view_swizzle=observed_view_swizzle;

/* Copy arguments are public ABI. Read only a bounded region value using the
 * existing safe reader; never inspect opaque view/texture/command storage or
 * call extra getters. Observation must preserve errno and sticky diagnostics. */
static void observe_copy_region(const char *role,const NVNcopyRegion *region)
{
    _Static_assert(sizeof(NVNcopyRegion)==24,"public copy region ABI");
    if(nn_nvn_error())fprintf(stderr,"[native NVN D2R] copy_region: role=%s values=unknown reason=prior_diagnostic\n",role);
    else {
#ifdef NN_D2R_PROGRAM_INPUT_READER
        uintptr_t start=(uintptr_t)region;
        if(!start||sizeof(*region)>UINTPTR_MAX-start)
            fprintf(stderr,"[native NVN D2R] copy_region: role=%s values=unknown reason=invalid_range\n",role);
        else {
            NVNcopyRegion snapshot;
            long got=NN_D2R_PROGRAM_INPUT_READER(&snapshot,region,sizeof(snapshot));
            if(got!=(long)sizeof(snapshot))
                fprintf(stderr,"[native NVN D2R] copy_region: role=%s values=unknown reason=read got=%ld requested=%zu errno=%d\n",role,got,sizeof(snapshot),got<0?errno:0);
            else fprintf(stderr,"[native NVN D2R] copy_region: role=%s x=%d y=%d z=%d width=%d height=%d depth=%d guest_pc=unavailable\n",role,snapshot.xoffset,snapshot.yoffset,snapshot.zoffset,snapshot.width,snapshot.height,snapshot.depth);
        }
#else
        (void)region;
        fprintf(stderr,"[native NVN D2R] copy_region: role=%s values=unknown reason=reader_not_enabled\n",role);
#endif
    }
}
static void observed_copy_buffer_texture(NVNcommandBuffer *command,NVNbufferAddress buffer,
 const NVNtexture *texture,const NVNtextureView *view,const NVNcopyRegion *region,int flags)
{
    int saved_errno=errno;
    fprintf(stderr,"[native NVN D2R] nvnCommandBufferCopyBufferToTexture: command=%p buffer=0x%016" PRIx64 " texture=%p view=%p region=%p flags=0x%08" PRIx32 " view_fields=opaque_not_inspected guest_pc=unavailable\n",(void *)command,buffer,(const void *)texture,(const void *)view,(const void *)region,(uint32_t)flags);
    observe_copy_region("destination",region);
    fflush(stderr);errno=saved_errno;forward_nvnCommandBufferCopyBufferToTexture(command,buffer,texture,view,region,flags);
}
static PFNNVNCOMMANDBUFFERCOPYBUFFERTOTEXTUREPROC const observe_copy_buffer_texture=observed_copy_buffer_texture;
static void observed_copy_texture_buffer(NVNcommandBuffer *command,const NVNtexture *texture,
 const NVNtextureView *view,const NVNcopyRegion *region,NVNbufferAddress buffer,int flags)
{
    int saved_errno=errno;
    fprintf(stderr,"[native NVN D2R] nvnCommandBufferCopyTextureToBuffer: command=%p texture=%p view=%p region=%p buffer=0x%016" PRIx64 " flags=0x%08" PRIx32 " view_fields=opaque_not_inspected guest_pc=unavailable\n",(void *)command,(const void *)texture,(const void *)view,(const void *)region,buffer,(uint32_t)flags);
    observe_copy_region("source",region);
    fflush(stderr);errno=saved_errno;forward_nvnCommandBufferCopyTextureToBuffer(command,texture,view,region,buffer,flags);
}
static PFNNVNCOMMANDBUFFERCOPYTEXTURETOBUFFERPROC const observe_copy_texture_buffer=observed_copy_texture_buffer;
static void observed_copy_textures(NVNcommandBuffer *command,const NVNtexture *source,
 const NVNtextureView *source_view,const NVNcopyRegion *source_region,const NVNtexture *destination,
 const NVNtextureView *destination_view,const NVNcopyRegion *destination_region,int flags)
{
    int saved_errno=errno;
    fprintf(stderr,"[native NVN D2R] nvnCommandBufferCopyTextureToTexture: command=%p source=%p source_view=%p source_region=%p destination=%p destination_view=%p destination_region=%p flags=0x%08" PRIx32 " view_fields=opaque_not_inspected guest_pc=unavailable\n",(void *)command,(const void *)source,(const void *)source_view,(const void *)source_region,(const void *)destination,(const void *)destination_view,(const void *)destination_region,(uint32_t)flags);
    observe_copy_region("source",source_region);observe_copy_region("destination",destination_region);
    fflush(stderr);errno=saved_errno;forward_nvnCommandBufferCopyTextureToTexture(command,source,source_view,source_region,destination,destination_view,destination_region,flags);
}
static PFNNVNCOMMANDBUFFERCOPYTEXTURETOTEXTUREPROC const observe_copy_textures=observed_copy_textures;
#define OBSERVE_COPY_STRIDE(name,pfn) \
static void observed_##name(NVNcommandBuffer *command,ptrdiff_t stride) { \
    int saved_errno=errno; \
    fprintf(stderr,"[native NVN D2R] " #name ": command=%p stride=%td guest_pc=unavailable\n",(void *)command,stride); \
    fflush(stderr);errno=saved_errno;forward_##name(command,stride); \
} \
static pfn const observe_##name=observed_##name;
OBSERVE_COPY_STRIDE(nvnCommandBufferSetCopyRowStride,PFNNVNCOMMANDBUFFERSETCOPYROWSTRIDEPROC)
OBSERVE_COPY_STRIDE(nvnCommandBufferSetCopyImageStride,PFNNVNCOMMANDBUFFERSETCOPYIMAGESTRIDEPROC)
#undef OBSERVE_COPY_STRIDE

static PFNNVNGENERICFUNCPTRPROC lookup(const char *name)
{
    if (!name) unsupported("invalid lookup", name, EINVAL, "lookup");
    if (!strcmp(name, "nvnDeviceBuilderSetFlags")) return (PFNNVNGENERICFUNCPTRPROC)observe_flags;
    if (!strcmp(name, "nvnDeviceInitialize")) return (PFNNVNGENERICFUNCPTRPROC)observe_initialize;
    if (!strcmp(name, "nvnDeviceGetInteger")) return (PFNNVNGENERICFUNCPTRPROC)observe_integer;
    if (!strcmp(name, "nvnDeviceSetWindowOriginMode")) return (PFNNVNGENERICFUNCPTRPROC)observe_window_origin;
    if (!strcmp(name, "nvnDeviceSetDepthMode")) return (PFNNVNGENERICFUNCPTRPROC)observe_depth_mode;
    if (!strcmp(name, "nvnMemoryPoolBuilderSetFlags")) return (PFNNVNGENERICFUNCPTRPROC)observe_pool_flags;
    if (!strcmp(name, "nvnMemoryPoolBuilderSetStorage")) return (PFNNVNGENERICFUNCPTRPROC)observe_pool_storage;
    if (!strcmp(name, "nvnMemoryPoolInitialize")) return (PFNNVNGENERICFUNCPTRPROC)observe_pool_initialize;
    if (!strcmp(name, "nvnQueueBuilderSetFlags")) return (PFNNVNGENERICFUNCPTRPROC)observe_queue_flags;
    if (!strcmp(name, "nvnQueueBuilderSetCommandMemorySize")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnQueueBuilderSetCommandMemorySize;
    if (!strcmp(name, "nvnQueueBuilderSetComputeMemorySize")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnQueueBuilderSetComputeMemorySize;
    if (!strcmp(name, "nvnQueueBuilderSetControlMemorySize")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnQueueBuilderSetControlMemorySize;
    if (!strcmp(name, "nvnQueueBuilderSetCommandFlushThreshold")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnQueueBuilderSetCommandFlushThreshold;
    if (!strcmp(name, "nvnQueueBuilderGetQueueMemorySize")) return (PFNNVNGENERICFUNCPTRPROC)observe_queue_memory_size;
    if (!strcmp(name, "nvnQueueBuilderSetQueueMemory")) return (PFNNVNGENERICFUNCPTRPROC)observe_queue_memory;
    if(!strcmp(name,"nvnWindowBuilderSetPresentInterval"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnWindowBuilderSetPresentInterval;
    if(!strcmp(name,"nvnWindowBuilderGetPresentInterval"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnWindowBuilderGetPresentInterval;
    if(!strcmp(name,"nvnWindowSetPresentInterval"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnWindowSetPresentInterval;
    if(!strcmp(name,"nvnWindowGetPresentInterval"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnWindowGetPresentInterval;
    if (!strcmp(name, "nvnQueueInitialize")) return (PFNNVNGENERICFUNCPTRPROC)observe_queue_initialize;
    if (!strcmp(name, "nvnTexturePoolInitialize")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTexturePoolInitialize;
    if (!strcmp(name, "nvnSamplerPoolInitialize")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnSamplerPoolInitialize;
    if (!strcmp(name, "nvnDeviceGetImageHandle")) return (PFNNVNGENERICFUNCPTRPROC)observe_image_handle;
    if (!strcmp(name, "nvnProgramSetShaders")) return (PFNNVNGENERICFUNCPTRPROC)observe_program_shaders;
    if (!strcmp(name, "nvnTextureBuilderSetFlags")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetFlags;
    if (!strcmp(name, "nvnTextureBuilderSetTarget")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetTarget;
    if (!strcmp(name, "nvnTextureBuilderSetWidth")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetWidth;
    if (!strcmp(name, "nvnTextureBuilderSetHeight")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetHeight;
    if (!strcmp(name, "nvnTextureBuilderSetDepth")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetDepth;
    if (!strcmp(name, "nvnTextureBuilderSetSize1D")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetSize1D;
    if (!strcmp(name, "nvnTextureBuilderSetSize2D")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetSize2D;
    if (!strcmp(name, "nvnTextureBuilderSetSize3D")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetSize3D;
    if (!strcmp(name, "nvnTextureBuilderSetLevels")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetLevels;
    if (!strcmp(name, "nvnTextureBuilderSetSamples")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetSamples;
    if (!strcmp(name, "nvnTextureBuilderSetFormat")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetFormat;
    if (!strcmp(name, "nvnTextureBuilderSetStride")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetStride;
    if (!strcmp(name, "nvnTextureBuilderSetStorage")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderSetStorage;
    if (!strcmp(name, "nvnTextureBuilderGetStorageSize")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderGetStorageSize;
    if (!strcmp(name, "nvnTextureBuilderGetStorageAlignment")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureBuilderGetStorageAlignment;
    if (!strcmp(name, "nvnTextureInitialize")) return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureInitialize;
    if (!strcmp(name,"nvnTexturePoolRegisterTexture"))return (PFNNVNGENERICFUNCPTRPROC)observe_register_texture;
    if (!strcmp(name,"nvnCommandBufferSetTexturePool"))return (PFNNVNGENERICFUNCPTRPROC)observe_set_texture_pool;
    if (!strcmp(name,"nvnCommandBufferBindSeparateTexture"))return (PFNNVNGENERICFUNCPTRPROC)observe_separate_texture;
    if (!strcmp(name,"nvnCommandBufferBindSeparateTextures"))return (PFNNVNGENERICFUNCPTRPROC)observe_separate_textures;
    if (!strcmp(name,"nvnTextureViewSetDefaults"))return (PFNNVNGENERICFUNCPTRPROC)observe_view_defaults;
    if (!strcmp(name,"nvnTextureViewSetLevels"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureViewSetLevels;
    if (!strcmp(name,"nvnTextureViewSetLayers"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureViewSetLayers;
    if (!strcmp(name,"nvnTextureViewSetFormat"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureViewSetFormat;
    if (!strcmp(name,"nvnTextureViewSetTarget"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureViewSetTarget;
    if (!strcmp(name,"nvnTextureViewSetDepthStencilMode"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnTextureViewSetDepthStencilMode;
    if (!strcmp(name,"nvnTextureViewSetSwizzle"))return (PFNNVNGENERICFUNCPTRPROC)observe_view_swizzle;
    if (!strcmp(name,"nvnCommandBufferCopyBufferToTexture"))return (PFNNVNGENERICFUNCPTRPROC)observe_copy_buffer_texture;
    if (!strcmp(name,"nvnCommandBufferCopyTextureToBuffer"))return (PFNNVNGENERICFUNCPTRPROC)observe_copy_texture_buffer;
    if (!strcmp(name,"nvnCommandBufferCopyTextureToTexture"))return (PFNNVNGENERICFUNCPTRPROC)observe_copy_textures;
    if (!strcmp(name,"nvnCommandBufferSetCopyRowStride"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnCommandBufferSetCopyRowStride;
    if (!strcmp(name,"nvnCommandBufferSetCopyImageStride"))return (PFNNVNGENERICFUNCPTRPROC)observe_nvnCommandBufferSetCopyImageStride;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (!strcmp(name, names[i].name)) return names[i].function;
    unsupported("unknown lookup", name, ENOSYS, "lookup");
}

/* Keep the registered bootstrap boundary auditable in the final host ELF. */
__attribute__((noinline)) PFNNVNGENERICFUNCPTRPROC nn_d2r_nvn_bootstrap(const char *name)
{
    check_diagnostic("nvnBootstrapLoader", "before");
    return lookup(name);
}

PFNNVNGENERICFUNCPTRPROC nn_d2r_nvn_get_proc_address(const NVNdevice *device,
                                                  const char *name)
{
    (void)device;
    check_diagnostic("nvnDeviceGetProcAddress", "before");
    return lookup(name);
}

static PFNNVNBOOTSTRAPLOADERPROC const check_bootstrap __attribute__((unused)) =
    nn_d2r_nvn_bootstrap;
static PFNNVNDEVICEGETPROCADDRESSPROC const check_get_proc __attribute__((unused)) =
    nn_d2r_nvn_get_proc_address;

#ifdef NN_D2R_HOST_HLE
#include <dm/host/hle.h>
HLE_FORCE("nvnBootstrapLoader",
          "Independent native NVN resolver; never enter the SDK GPU driver",
          PFNNVNGENERICFUNCPTRPROC, nn_d2r_hle_bootstrap, const char *name)
{
    return nn_d2r_nvn_bootstrap(name);
}
static PFNNVNBOOTSTRAPLOADERPROC const check_hle __attribute__((unused)) =
    nn_d2r_hle_bootstrap;
#endif
