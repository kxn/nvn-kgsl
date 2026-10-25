/* SPDX-License-Identifier: MIT. Real FDL BUFFER binding; no image/pixel conversion. */
#pragma once
#include "runtime_fdl.h"
#include "kgsl.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {NN_BUFFER_FLOAT=1,NN_BUFFER_UINT=2,NN_BUFFER_SINT=3,NN_BUFFER_SIZE=4};
/* Pure borrowed read-only view. Owner is the original allocation/pool window,
 * not the logical texture range; the complete aligned-down FDL prefix must fit.
 * Caller retains same-device resource+owner through completion, supplies prior
 * GPU producer dependencies/cache protocol and proves physical feedback absence.
 * Distinct BO pointers alone do not prove physical independence of imports.
 * No function maps source pixels, allocates/submits, or mutates BO ownership. */
struct nn_buffer_sampled_binding {
 uint32_t version,format;nn_bo *resource;
 size_t owner_offset,owner_bytes,offset,bytes;
};
int nn_buffer_sampled_view(const struct nn_buffer_sampled_binding *,struct nn_fdl_buffer_view *);
int nn_buffer_sampled_descriptor(const struct nn_buffer_sampled_binding *,uint32_t out[16]);
int nn_buffer_sampled_result(uint32_t format,uint32_t *result);
#ifdef __cplusplus
}
#endif
