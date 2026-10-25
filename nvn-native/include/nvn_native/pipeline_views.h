/* SPDX-License-Identifier: MIT. Actual runtime FDL CPU metadata, no BOs/GPU. */
#pragma once
#include "nvn_native/texture_native_view.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct nn_pipeline_views;
/* One immutable-chip cache. No eviction: returned values remain valid until
 * destroy. Owner must outlive all recorded/submitted consumers. Calls use the
 * frontend's existing serialization; this API adds no concurrent mutation. */
int nn_pipeline_views_create(uint32_t chip,struct nn_pipeline_views **out);
/* Ordinary RGBA8 2D, mip/layer/sample1, LINEAR/nonUBWC. pitch0 requests actual
 * FDL default shadow storage; otherwise an actual native LINEAR pitch. No
 * canonical GOB pitch or public NVN size/alignment is inferred here. */
int nn_pipeline_views_get(struct nn_pipeline_views *,uint32_t width,uint32_t height,
                          uint32_t pitch,const struct nn_tex_native_view **out);
/* Typed single-attachment float-output color view. format is part of the
 * immutable cache key. Non-RGBA8 formats require NN_PIPELINE_COLOR_TARGETS;
 * actual FDL still controls explicit pitch admission (currently RGBA8 only).
 * This does NOT grant sampled-format or canonical conversion capability. */
int nn_pipeline_views_get_format(struct nn_pipeline_views *,uint32_t format,
 uint32_t width,uint32_t height,uint32_t pitch,const struct nn_tex_native_view **out);
size_t nn_pipeline_views_count(const struct nn_pipeline_views *);
void nn_pipeline_views_destroy(struct nn_pipeline_views *);
#ifdef __cplusplus
}
#endif
