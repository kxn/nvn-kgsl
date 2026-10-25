/* SPDX-License-Identifier: MIT. Explicit native integration, not an NVN ABI. */
#pragma once
#include "nvn_native/nvn.h"
#include "nvn_native/pipeline_source.h"
struct nn_pipeline_depth_archive;
struct nn_pipeline_storage_archive;
#ifdef __cplusplus
extern "C" {
#endif
int nn_nvn_register_pipeline_depth_archive(NVNdevice *,const struct nn_pipeline_depth_archive *);
int nn_nvn_register_pipeline_storage_archive(NVNdevice *,const struct nn_pipeline_storage_archive *);

/* Strict bounded read callback: returns exact bytes read or a short/negative
 * count; errno accompanies negative return. It must not reenter NVN or alter
 * object lifetimes. Normal host can adapt its existing
 * dm_self_read; plain core never links a host syscall/helper by itself. */
typedef long (*nn_nvn_pipeline_input_reader)(void *user,void *out,const void *source,size_t bytes);
/* Explicit immutable source catalogue, before any Program exists on this
 * Device. Caller keeps catalogue and reader context alive until DeviceFinalize.
 * No automatic registration, dynamic compiler, fixed-image or fixture fallback.
 * A source-only VS+FS Program can bind independently of resources. Nonempty
 * Draw requires exact runtime specialization, supported resource mappings and
 * an explicitly checked native view; every unresolved domain returns ENOTSUP. */
/* Trusted normal full-source catalogue. Native specialization remains
 * independent. Provider read-only original blob outlives Device; metadata index
 * is copied and Program owns a deep copy of each actually installed stage. */
int nn_nvn_register_shader_source_catalogue(NVNdevice *,const struct nn_shader_source_archive *);
int nn_nvn_register_pipeline_archive(NVNdevice *,const struct nn_pipeline_archive *,nn_nvn_pipeline_input_reader,void *user);
/* Optional normal actual-NIR metadata registration, atomically with the source
 * archive, before any Program exists. This does not open a resource target or
 * claim shader/GPU qualification. Untyped registration remains supported. */
int nn_nvn_register_typed_pipeline_archive(NVNdevice *,const struct nn_pipeline_typed_archive *,nn_nvn_pipeline_input_reader,void *user);
/* Actual normal FDL descriptor views, immutable through DeviceFinalize. Missing
 * dimensions are a finite archive coverage miss, never an NVN dimension limit.
 * This explicit source-cache helper does not calculate FDL or infer descriptors. */
int nn_nvn_register_pipeline_views(NVNdevice *,const struct nn_tex_native_view *,size_t count);
/* Explicit normal verified runtime FDL build required. Device owns immutable
 * views until safe finalization; activate before Programs exist. No static
 * catalogue fallback while enabled. No public texture extent is changed. */
int nn_nvn_enable_pipeline_runtime_views(NVNdevice *);
#ifdef __cplusplus
}
#endif
