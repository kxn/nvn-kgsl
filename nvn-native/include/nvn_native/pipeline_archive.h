/* SPDX-License-Identifier: MIT. Explicit runtime specialization, no first variant. */
#pragma once
#include "pipeline.h"
#include "pipeline_texture_types.h"
#ifdef __cplusplus
extern "C" {
#endif
enum nn_pipeline_query_type {NN_PIPELINE_QUERY_BOOL,NN_PIPELINE_QUERY_I32,NN_PIPELINE_QUERY_F32};
/* Callback receives only identity/type/shape, never the expected value. args
 * bit0=handle,bit1=cbufSlot,bit2=slot,bit3=index. Unused args are zero.
 * Typed F32 values use IEEE32 bits, booleans exactly0/1, I32 two's complement. */
struct nn_pipeline_query {const char *name;uint32_t arguments,type,count;int32_t args[4];};
struct nn_pipeline_query_requirement {struct nn_pipeline_query query;uint32_t values[32];};
struct nn_pipeline_stage_requirements {
    uint32_t stage_index;const struct nn_pipeline_query_requirement *queries;size_t query_count;
};
struct nn_pipeline_pair_recipe {uint32_t vs_index,fs_index;const struct nn_pipeline_shader_recipe *recipe;};
struct nn_pipeline_archive {
    const struct nn_offline_graphics_archive *stages;
    const struct nn_pipeline_stage_requirements *requirements;size_t requirement_count;
    const struct nn_pipeline_pair_recipe *pairs;size_t pair_count;
    const struct nn_pipeline_platform *platform;
};
/* Optional immutable extension. Old archive/pair/recipe ABI stays unchanged.
 * Entries correspond one-for-one to archive->pairs, including exact variant
 * indices; native dimensions are compiler facts, never resource query answers.
 * All pointed data must outlive Device/Program/submissions. */
struct nn_pipeline_typed_archive {
 uint32_t version;
 const struct nn_pipeline_archive *archive;
 const struct nn_pipeline_texture_types *const *pairs;size_t pair_count;
};
int nn_pipeline_typed_archive_validate(const struct nn_pipeline_typed_archive *);
int nn_pipeline_select_texture_types(const struct nn_pipeline_typed_archive *,
 const struct nn_offline_graphics_stage *,const struct nn_offline_graphics_stage *,
 const struct nn_pipeline_shader_recipe *,const struct nn_pipeline_texture_types **out);
/* Immutable catalogue structural validation, no query/BO mutation. */
int nn_pipeline_archive_validate(const struct nn_pipeline_archive *);
typedef int (*nn_pipeline_query_resolver)(void *user,uint32_t stage,const struct nn_pipeline_query *,
                                         uint32_t values[32],size_t *count);
/* The source/control slices MUST already be independently validated readable
 * ranges. Exact code+C1+control identity is required. Resolver must read actual
 * current state/capabilities/resource queries; unknown is ENOTSUP. A matched
 * source variant with unknown requirements prevents choosing another variant.
 * No opaque hash is treated as proof of runtime state. Miss ENOTSUP; >1 fully
 * matching specialization EEXIST; out=NULL on error. No GPU/allocations. */
int nn_pipeline_select_stage(const struct nn_pipeline_archive *,uint32_t stage,const void *source,size_t,
    const void *control,size_t,nn_pipeline_query_resolver,void *user,const struct nn_offline_graphics_stage **out);
int nn_pipeline_select_pair(const struct nn_pipeline_archive *,const struct nn_offline_graphics_stage *vs,
    const struct nn_offline_graphics_stage *fs,const struct nn_pipeline_shader_recipe **out);
#ifdef __cplusplus
}
#endif
