/* SPDX-License-Identifier: MIT. Independent graphics storage resource role. */
#pragma once
#include "pipeline_shader.h"
#include "runtime_fdl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* These are actual shader-logical bindings, never NVN API selectors. */
#define NN_PIPELINE_STORAGE_READ 1u
#define NN_PIPELINE_STORAGE_WRITE 2u
#define NN_PIPELINE_STORAGE_SIZE 4u
/* Exact normal common-NIR sidecar and real compiled SPIR-V stage identities.
 * Missing sidecar accepts only recipes containing no storage address roles. */
int nn_pipeline_storage_types_validate(const struct nn_pipeline_storage_types *,
 const struct nn_pipeline_shader_recipe *,const struct nn_offline_graphics_stage *const[2],uint32_t *entries);
/* The actual borrowed BO owner supplies a complete bounded view, not a free
 * GPUVA. FDL performs the R32_UINT buffer encoding including the 64B prefix.
 * The caller must retain/qualify all underlying physical storage through GPU
 * completion; pointer inequality is not physical alias independence proof. */
int nn_pipeline_storage_descriptor(struct nn_pipeline_buffer,uint32_t descriptor[16]);
/* Public original NVN CB0 table contract, independently reconstructed from
 * SDK53.314 BindStorage stage constants and public shader-recompiler reverse.
 * VS byte0x110 / FS byte0x510,16byte stride,16 API slots. All other CB/forms
 * reject. Compute0x310 is deliberately not a graphics fallback. */
int nn_pipeline_storage_source_selector(unsigned stage,uint32_t source_cb,uint32_t source_word,unsigned *api_slot);
struct nn_pipeline_typed_archive;
struct nn_pipeline_storage_archive {
 uint32_t version;const struct nn_pipeline_typed_archive *typed;
 const struct nn_pipeline_storage_types *const *pairs;size_t pair_count;
};
int nn_pipeline_storage_archive_validate(const struct nn_pipeline_storage_archive *);
int nn_pipeline_select_storage_types(const struct nn_pipeline_storage_archive *,
 const struct nn_offline_graphics_stage *,const struct nn_offline_graphics_stage *,
 const struct nn_pipeline_shader_recipe *,const struct nn_pipeline_storage_types **);
#ifdef __cplusplus
}
#endif
