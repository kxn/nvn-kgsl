/* SPDX-License-Identifier: MIT. Metadata only; not NVN/FDL/GPU qualification. */
#pragma once
#include "texture_source.h"
struct nn_tex_volume_desc {
 uint32_t format,width,height,depth,levels;
 int32_t block_depth_log2; /* -1=published normal chooser; explicit0..4 */
};
struct nn_tex_volume_level {
 uint64_t offset,bytes;
 uint32_t width,height,depth,blocks_x,blocks_y,row_bytes,pitch,block_depth_log2;
};
struct nn_tex_volume_layout {
 uint32_t schema,format,levels,block_w,block_h,bytes_per_block,base_block_depth_log2;
 uint64_t payload_bytes;
 struct nn_tex_volume_level level[NN_TEX_SOURCE_MAX_LEVELS];
};
int nn_tex_volume_layout(const struct nn_tex_volume_desc *,struct nn_tex_volume_layout *);
int nn_tex_volume_validate(const struct nn_tex_volume_layout *);
int nn_tex_volume_offset(const struct nn_tex_volume_layout *,uint32_t level,uint32_t z,
 uint32_t block_x,uint32_t block_y,uint32_t byte_in_block,uint64_t *);
