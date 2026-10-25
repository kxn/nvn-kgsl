/* SPDX-License-Identifier: MIT. Certified original source; no native shader capability. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct nn_shader_source_entry {
 uint32_t stage,profile; /* stage0 VS/1 FS; profile1 v5+880,2 v5+898 */
 const unsigned char *source,*control;
 size_t source_bytes,control_bytes,code_bytes;
 unsigned char source_id[32]; /* normal provenance label, never admission hash */
};
struct nn_shader_source_archive {uint32_t version;const struct nn_shader_source_entry *entries;size_t count;};
struct nn_shader_source_catalogue;
struct nn_shader_source_program;
/* Trusted native provider's original byte blob is immutable and must outlive
 * this catalogue. Metadata index is copied. No guest pointer is accepted here.
 * Input full-byte ordering is checked, not silently sorted or compacted. */
int nn_shader_source_catalogue_create(const struct nn_shader_source_archive *,struct nn_shader_source_catalogue **);
void nn_shader_source_catalogue_destroy(struct nn_shader_source_catalogue *);
size_t nn_shader_source_catalogue_count(const struct nn_shader_source_catalogue *);
const struct nn_shader_source_entry *nn_shader_source_entry_at(const struct nn_shader_source_catalogue *,size_t);
/* Control sizes are certified catalogue extents, never inferred guest lengths.
 * Input bytes must already have been copied by a safe reader. No dereference of
 * GPUVA/guest control, CPU cache operation, BO allocation or shader compilation. */
size_t nn_shader_source_control_sizes(const struct nn_shader_source_catalogue *,size_t out[2]);
int nn_shader_source_candidates(const struct nn_shader_source_catalogue *,const void *control,size_t bytes,size_t *first,size_t *end);
/* Full code+C1/control match. Output NULL on any failure. Entries must be actual
 * members of this catalogue. Exactly VS+FS, no other stages/ABI accepted. */
int nn_shader_source_program_create(const struct nn_shader_source_catalogue *,const struct nn_shader_source_entry *vs,const struct nn_shader_source_entry *fs,struct nn_shader_source_program **);
void nn_shader_source_program_destroy(struct nn_shader_source_program *);
const struct nn_shader_source_entry *nn_shader_source_program_stage(const struct nn_shader_source_program *,unsigned);
#ifdef __cplusplus
}
#endif
