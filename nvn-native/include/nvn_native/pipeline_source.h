/* SPDX-License-Identifier: MIT. Source identity before runtime specialization. */
#pragma once
#include "pipeline_archive.h"
#include "shader_source.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_pipeline_source_input {uint32_t stage;const void *source;size_t source_bytes;const void *control;size_t control_bytes;};
/* Immutable archive-owned identity representatives only. These pointers do NOT
 * choose a compiled variant. No caller source/control pointer survives prepare.
 * Owning NVN Device must retain the immutable catalogue until Program/handles
 * and submitted jobs are fully released; no guest lifetime is borrowed. */
struct nn_pipeline_source_program {const struct nn_pipeline_archive *archive;const struct nn_offline_graphics_stage *identity[2];
 /* Optional Program-owned complete identity; native archive remains separate. */
 const struct nn_shader_source_program *binding;};
/* Caller independently validates every readable slice and GPU source pool
 * range/cache policy before this call. Exactly original VS+FS, no other stage.
 * All code+C1+control bytes and boundary metadata must match; out zero on error.
 * Does not issue queries, upload BOs, compile, or promise a Draw pipeline. */
int nn_pipeline_source_prepare(const struct nn_pipeline_archive *,const struct nn_pipeline_source_input *,size_t count,struct nn_pipeline_source_program *out);
/* Takes no ownership. Program/recorded handles keep binding alive. No query,
 * compiler/native variant promise or GPU allocation. */
int nn_pipeline_source_prepare_bound(const struct nn_pipeline_archive *,const struct nn_shader_source_program *,struct nn_pipeline_source_program *);
/* Complete identity only. Does not supply reflection for uncompiled stages.
 * Callers deriving resource facts must still find actual matching compiled
 * metadata and reject an empty match set. */
int nn_pipeline_source_identity(const struct nn_pipeline_source_program *,uint32_t stage,struct nn_pipeline_source_input *out);
/* Resolve actual immutable per-Draw state, never latest mutable device state.
 * Unknown requirements/handles remain unsupported; no first-variant fallback.
 * Caller may then upload exact code/full C1 with nn_pipeline_program_create.
 * No GPU allocations or execution; every output is NULL on any error. */
int nn_pipeline_source_resolve(const struct nn_pipeline_source_program *,nn_pipeline_query_resolver,void *,const struct nn_offline_graphics_stage **vs,const struct nn_offline_graphics_stage **fs,const struct nn_pipeline_shader_recipe **recipe);
#ifdef __cplusplus
}
#endif
