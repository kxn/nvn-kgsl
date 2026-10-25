/* SPDX-License-Identifier: MIT. Runtime vertex snapshot, no fixed profile. */
#pragma once
#include "pipeline_shader.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_pipeline_attribute {uint32_t format,offset,stream;};
struct nn_pipeline_stream {uint32_t stride,divisor;};
struct nn_pipeline_vertex_state {
    uint32_t attribute_count,stream_count;
    struct nn_pipeline_attribute attributes[32];
    struct nn_pipeline_stream streams[32];
    struct nn_pipeline_buffer buffers[32];
};
/* A650 float-decoded family: NVN R32F/RG32F/RGB32F/RGBA32F and
 * normalized RGBA8/RGBA8SN. Integer/scaled/packed/half sources are unsupported. Broad typed
 * setters may retain other valid formats; only actual consumed Draw locations
 * are checked here. Unknown consumed format is ENOTSUP. No buffer ownership is
 * acquired here; each actually consumed stream's BO must be submitted/retained.
 * Hardware fetch windows use actual bound byte ranges, never target dimensions.
 * words=NULL/capacity0 queries size. Failure writes nothing and out_words=0. */
int nn_pipeline_vertex_encode(const struct nn_pipeline_shader_recipe *,
    const struct nn_pipeline_vertex_state *,uint32_t *words,size_t capacity,size_t *out_words);

int nn_pipeline_vertex_encode_depth(const struct nn_pipeline_depth_export *,const struct nn_pipeline_vertex_state *,uint32_t *,size_t,size_t *);
#ifdef __cplusplus
}
#endif
