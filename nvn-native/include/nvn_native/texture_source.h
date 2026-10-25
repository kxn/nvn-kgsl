/* SPDX-License-Identifier: MIT
 * Bounded Generic16Bx2 source-payload layout. Not a public NVN storage
 * getter or an Adreno FDL allocation/layout. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#define NN_TEX_SOURCE_MAX_LEVELS 15
struct nn_tex_source_desc {
    uint32_t flags,target,format,width,height,layers,levels,samples;
    uint64_t stride;
    /* -1=SDK normal2D chooser; 0..5=explicit known Generic16Bx2 layout.
     * Packaged layout bytes must be separately decoded/validated, never guessed. */
    int32_t block_height_log2;
    /* True3D only: pixel depth; 0=normal depth chooser, 1..5=explicit
     * blockDepthLog2+1. Other targets require both fields to be zero. */
    uint32_t depth,block_depth_log2;
};
struct nn_tex_source_level {
    uint64_t offset,bytes;uint32_t width,height,blocks_x,blocks_y,row_bytes,pitch,block_height_log2;
};
struct nn_tex_source_layout {
    uint32_t schema,flags,target,format,linear,block_w,block_h,bytes_per_block,levels,layers,base_block_height_log2;
    /* Tiled alignment/public extent require device/kind/page evidence.
     * alignment=0 and public_extent_known=0 deliberately mark that unknown.
     * gob_bytes describes the addressing unit, never allocation alignment. */
    uint32_t gob_bytes,public_extent_known;
    uint64_t alignment,layer_stride,payload_bytes;
    struct nn_tex_source_level level[NN_TEX_SOURCE_MAX_LEVELS];
    /* True3D canonical layout. layers stays1; layer_stride is the complete
     * volume mip-chain extent, never a Z-slice pitch. Other targets: all0. */
    uint32_t volume_depth,base_block_depth_log2;
    uint32_t level_depths[NN_TEX_SOURCE_MAX_LEVELS];
    uint32_t level_block_depths[NN_TEX_SOURCE_MAX_LEVELS];
};
/* No pixels are copied on CPU. These functions calculate and validate byte
 * extents/addresses only. Errors zero the output, positive errno. */
int nn_tex_source_layout(const struct nn_tex_source_desc *,struct nn_tex_source_layout *);
int nn_tex_source_validate(const struct nn_tex_source_layout *);
/* layer is an array layer for array targets, a mip-relative Z slice for
 * true3D. The latter uses volume BD/Z addressing, never layer_stride*z. */
int nn_tex_source_offset(const struct nn_tex_source_layout *,uint32_t level,uint32_t layer,
                        uint32_t block_x,uint32_t block_y,uint32_t byte_in_block,uint64_t *out);
struct nn_tex_volume_layout;
int nn_tex_source_volume(const struct nn_tex_source_layout *,struct nn_tex_volume_layout *);
/* Exact source->native conversion arguments. Native pitch/size are supplied by
 * actual FDL, independently. Bind disjoint SSBO source/dest windows; no alias
 * coherence is provided here. Caller chooses direction0=untile,1=retile. */
#define NN_TEX_GOB_COPY_UNIFORM_BYTES 64u
struct nn_tex_gob_copy_args {uint32_t source_base,destination_base,row_bytes,rows,linear_pitch,width_gobs,block_height_log2,direction;};
int nn_tex_gob_copy_args(const struct nn_tex_source_layout *,uint32_t level,uint32_t direction,
                         uint32_t native_pitch,uint64_t native_bytes,struct nn_tex_gob_copy_args *);

struct nn_tex_linear_copy_args {uint32_t source_base,destination_base,row_bytes,rows,source_pitch,destination_pitch;};
int nn_tex_linear_copy_args(const struct nn_tex_source_layout *,uint32_t direction,
    uint32_t native_pitch,uint64_t native_bytes,struct nn_tex_linear_copy_args *);
