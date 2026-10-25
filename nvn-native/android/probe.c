#include <android/native_activity.h>
#include <stdio.h>
#include "nvn_native/probe.h"
#ifdef NVN_WINDOW_SOURCE_SLOTS
void nn_window_source_activity_create(ANativeActivity *);
#endif
#ifdef NVN_FULLSIZE_DISPLAY_SLOTS
void nn_fullsize_display_activity_create(ANativeActivity *);
#endif
#ifdef NVN_BC_SAMPLED_PROBE
void nn_bc_sampled_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_TEXTURE_TEXELS_PROBE
void nn_texture_texels_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_BUFFER_TEXTURE_PROBE
void nn_buffer_texture_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_VOLUME_TEXTURE_PROBE
void nn_volume_texture_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_VOLUME_SHADOW_PROBE
void nn_volume_shadow_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_PUBLIC_VOLUME_COPY_PROBE
void nn_public_volume_copy_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_PUBLIC_COPY_PROBE
void nn_public_copy_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_BLOCK_COPY_PROBE
void nn_block_copy_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_TEXTURE_CONVERSION_PROBE
void nn_conversion_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_SAMPLER_PROBE
void nn_sampler_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_COUNTER_PROBE
void nn_counter_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_COPY_PROBE
void nn_copy_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_WINDOW_SLOTS
void nn_window_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_EXTERNAL_IMAGE_SLOTS
void nn_external_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_AHB_PRESENT
#include "present_activity.h"
#endif
#ifdef NVN_COMPUTE_DRIVERPARAMS_HEADER
int nn_compute_driverparams_probe(char *, size_t);
#endif
#ifdef NVN_DEVICE_INSPECT
int nn_device_info_probe(char *, size_t);
#endif
#ifdef NVN_IOCOHERENT_INSPECT
int nn_iocoherent_import_probe(const char *, char *, size_t);
#endif
#ifdef NVN_AHB_PROBE
int nn_ahb_info_probe(char *, size_t);
#endif
#ifdef NVN_AHB_GRAPHICS_HEADER
int nn_ahb_gpu_probe(const char *, char *, size_t);
#endif
#ifdef NVN_API_PROBE
#include "nvn_native/nvn.h"
#endif
#ifdef NVN_MAXWELL_HEADER
int nn_maxwell_compute_probe(char *, size_t);
#endif
#ifdef NVN_GRAPHICS_HEADER
int nn_graphics_probe(const char *, char *, size_t);
#endif
#ifdef NVN_GRAPHICS_API_HEADER
int nn_nvn_graphics_probe(const char *, char *, size_t);
#endif
#ifdef NVN_TEXTURE_HEADER
int nn_texture_probe(const char *, char *, size_t);
#endif
#ifdef NVN_TEXTURE_API_HEADER
int nn_nvn_texture_probe(const char *, char *, size_t);
#endif
#ifdef NVN_TEXTURE_BRIDGE_PROBE
void nn_texture_bridge_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_NVN_PIPELINE_PROBE
void nn_nvn_pipeline_probe_activity_create(ANativeActivity *);
#endif
#ifdef NVN_NATIVE_PIPELINE_PROBE
void nn_pipeline_probe_activity_create(ANativeActivity *);
#endif
__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity *activity, void *state, size_t size)
{
#ifdef NVN_NVN_PIPELINE_PROBE
    (void)state; (void)size;
    nn_nvn_pipeline_probe_activity_create(activity); return;
#endif
#ifdef NVN_NATIVE_PIPELINE_PROBE
    (void)state; (void)size;
    nn_pipeline_probe_activity_create(activity); return;
#endif
#ifdef NVN_TEXTURE_BRIDGE_PROBE
    (void)state; (void)size;
    nn_texture_bridge_probe_activity_create(activity); return;
#endif

    (void)state; (void)size;
#ifdef NVN_WINDOW_SOURCE_SLOTS
    nn_window_source_activity_create(activity);
    return;
#elif defined(NVN_FULLSIZE_DISPLAY_SLOTS)
    nn_fullsize_display_activity_create(activity);
    return;
#endif
#ifdef NVN_BC_SAMPLED_PROBE
    nn_bc_sampled_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_TEXTURE_TEXELS_PROBE
    nn_texture_texels_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_BUFFER_TEXTURE_PROBE
    nn_buffer_texture_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_VOLUME_TEXTURE_PROBE
    nn_volume_texture_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_VOLUME_SHADOW_PROBE
    nn_volume_shadow_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_PUBLIC_VOLUME_COPY_PROBE
    nn_public_volume_copy_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_PUBLIC_COPY_PROBE
    nn_public_copy_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_BLOCK_COPY_PROBE
    nn_block_copy_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_TEXTURE_CONVERSION_PROBE
    nn_conversion_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_SAMPLER_PROBE
    nn_sampler_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_COUNTER_PROBE
    nn_counter_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_COPY_PROBE
    nn_copy_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_WINDOW_SLOTS
    nn_window_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_EXTERNAL_IMAGE_SLOTS
    nn_external_probe_activity_create(activity);
    return;
#endif
#ifdef NVN_AHB_PRESENT
    nn_present_activity_create(activity);
    return;
#endif
#if defined(NVN_COMPUTE_CALL) && NVN_COMPUTE_CALL
    char json[32768], path[1024];
#else
    char json[8192], path[1024];
#endif
#ifdef NVN_COMPUTE_DRIVERPARAMS_HEADER
    nn_compute_driverparams_probe(json, sizeof(json));
#elif defined(NVN_DEVICE_INSPECT)
    nn_device_info_probe(json, sizeof(json));
#elif defined(NVN_IOCOHERENT_INSPECT)
    nn_iocoherent_import_probe(activity->internalDataPath, json, sizeof(json));
#elif defined(NVN_AHB_PROBE)
    nn_ahb_info_probe(json, sizeof(json));
#elif defined(NVN_AHB_GRAPHICS_HEADER)
    nn_ahb_gpu_probe(activity->internalDataPath, json, sizeof(json));
#elif defined(NVN_TEXTURE_API_HEADER)
    nn_nvn_texture_probe(activity->internalDataPath, json, sizeof(json));
#elif defined(NVN_TEXTURE_HEADER)
    nn_texture_probe(activity->internalDataPath, json, sizeof(json));
#elif defined(NVN_GRAPHICS_API_HEADER)
    nn_nvn_graphics_probe(activity->internalDataPath, json, sizeof(json));
#elif defined(NVN_GRAPHICS_HEADER)
    nn_graphics_probe(activity->internalDataPath, json, sizeof(json));
#elif defined(NVN_MAXWELL_HEADER)
    nn_maxwell_compute_probe(json, sizeof(json));
#elif defined(NVN_API_PROBE)
    nn_nvn_api_probe(json, sizeof(json));
#else
    nvn_kgsl_probe(json, sizeof(json));
#endif
    snprintf(path, sizeof(path), "%s/result.json", activity->internalDataPath);
    FILE *file = fopen(path, "w");
    if (file) { fputs(json, file); fclose(file); }
    ANativeActivity_finish(activity);
}
