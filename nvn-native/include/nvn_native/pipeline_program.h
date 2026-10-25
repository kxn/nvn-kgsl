/* SPDX-License-Identifier: MIT. Generic native shader owner, no Draw template. */
#pragma once
#include "nvn_native/kgsl.h"
#include "nvn_native/offline_graphics.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nn_pipeline_program nn_pipeline_program;
struct nn_pipeline_stage_view {
    const struct nn_offline_graphics_stage *source;
    const struct nn_offline_graphics_pair *pair;
    nn_bo *code,*c1;
    size_t code_bytes,c1_bytes;
    /* Source-CB1 is internal immutable C1 only when that stage has appended
     * source data; UINT32_MAX means no referenced CB1 mapping. Never API slot. */
    uint32_t c1_native_binding;
};
/* Source/control matching must precede this API using validated readable ranges.
 * Exact runtime specialization keys are mandatory; no first/default variant.
 * Archive/keys/code/C1 must remain immutable through owner lifetime. On error
 * program=NULL; retained is NONNULL only if rollback could not safely release
 * all BOs, and caller must quarantine that owner plus its device until destroy.
 * No render state/pipeline/texture/address mapping is implicitly qualified. */
int nn_pipeline_program_create(nn_device *,const struct nn_offline_graphics_archive *,
    const struct nn_offline_graphics_stage *vs,const struct nn_offline_graphics_stage *fs,
    const unsigned char vs_key[32],const unsigned char fs_key[32],
    nn_pipeline_program **program,nn_pipeline_program **retained);
/* Exact normally packaged Maxwell stage pair plus its actual native NIR facts.
 * The table is selected by archive-owned VS/FS variants; no caller type label,
 * raw-native shader or expected query value is substituted for guest resources.
 * Old untyped constructor remains available with its original NULL type table. */
struct nn_pipeline_texture_types;
int nn_pipeline_program_create_typed(nn_device *,const struct nn_offline_graphics_archive *,
 const struct nn_offline_graphics_stage *,const struct nn_offline_graphics_stage *,
 const struct nn_pipeline_texture_types *,nn_pipeline_program **,nn_pipeline_program **retained);
struct nn_pipeline_depth_export;
/* Exact original source/C1 path, independently validated normal depth sidecar.
 * Native typed archive selection precedes this call; no raw shader substitution. */
int nn_pipeline_program_create_typed_depth(nn_device *,const struct nn_offline_graphics_archive *,
 const struct nn_offline_graphics_stage *,const struct nn_offline_graphics_stage *,
 const struct nn_pipeline_texture_types *,const struct nn_pipeline_depth_export *,
 nn_pipeline_program **,nn_pipeline_program **retained);
struct nn_pipeline_storage_types;
int nn_pipeline_program_create_typed_storage(nn_device *,const struct nn_offline_graphics_archive *,
 const struct nn_offline_graphics_stage *,const struct nn_offline_graphics_stage *,const struct nn_pipeline_texture_types *,
 const struct nn_pipeline_depth_export *,const struct nn_pipeline_storage_types *,nn_pipeline_program **,nn_pipeline_program **retained);
/* Caller handles recorded-command CPU refs. All underlying BOs must be in the
 * real submission resource list. Busy leaves owner intact; other destroy errors
 * may partially retire resources and leave a nonready owner for cleanup retry. */
int nn_pipeline_program_destroy(nn_pipeline_program *);
int nn_pipeline_program_stage(const nn_pipeline_program *,uint32_t stage,struct nn_pipeline_stage_view *);
int nn_pipeline_program_resources(const nn_pipeline_program *,nn_bo **bos,size_t capacity,size_t *count);
nn_device *nn_pipeline_program_device(const nn_pipeline_program *);
int nn_pipeline_program_cleanup_error(const nn_pipeline_program *);
#ifdef __cplusplus
}
#endif
