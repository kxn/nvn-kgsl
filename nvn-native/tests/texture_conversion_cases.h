/* SPDX-License-Identifier: MIT */
#pragma once
#include "nvn_native/texture_source.h"
/* Host-produced metadata. Native rows/pitches/offsets/extent come from actual
 * pinned FDL. Source extent is a bounded payload, never public NVN GetSize. */
struct nn_tc_level {
    uint32_t source_offset,source_bytes,row_bytes,rows,source_pitch,block_height_log2;
    uint32_t native_offset,native_pitch,native_layer_stride,native_bytes;
};
struct nn_tc_case {
    struct nn_tex_source_desc desc;
    uint32_t source_bytes,source_layer_stride,native_bytes,linear,levels,layers;
    struct nn_tc_level level[NN_TEX_SOURCE_MAX_LEVELS];
};
#define NN_TC_REPEATS 2u
#define NN_TC_SOURCE_BASE 4096u
#define NN_TC_NATIVE_BASE 8192u
static inline unsigned char nn_tc_pattern(uint32_t offset,unsigned which,unsigned repeat)
{
    uint32_t v=offset^(which*UINT32_C(0x9e3779b9))^(repeat*UINT32_C(0x6a09e667));
    v^=v>>16;v*=UINT32_C(0x7feb352d);v^=v>>15;v*=UINT32_C(0x846ca68b);v^=v>>16;
    return (unsigned char)v;
}
