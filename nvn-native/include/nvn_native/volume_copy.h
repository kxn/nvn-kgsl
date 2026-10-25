/* SPDX-License-Identifier: MIT. Raw 3D block bytes, not sampling/decoding. */
#pragma once
#include "texture_volume.h"
#include "nvn_native/block_copy.h"
struct nn_volume_copy_region {
 uint32_t level,base_z,slices,x_bytes,y_rows,row_bytes,rows;
 uint32_t linear_row_stride,linear_image_stride,direction;
};
struct nn_volume_copy_args {
 uint32_t canonical_base,canonical_slab_stride,width_gobs,block_depth_log2;
 uint32_t region_x_bytes,region_y_rows,row_bytes,rows;
 uint32_t linear_row_stride,linear_image_stride,slices,direction;
 uint32_t linear_span,linear_words,dispatch_width,base_z;
};
struct nn_volume_copy_plan {
 struct nn_volume_copy_args args;uint32_t groups[3];
 uint64_t canonical_bytes,linear_bytes;struct nn_volume_copy_region region;
};
int nn_volume_copy_plan(const struct nn_tex_volume_layout *,const struct nn_volume_copy_region *,struct nn_volume_copy_plan *);
struct nn_volume_copy_binding {
 struct nn_volume_copy_args args;uint32_t groups[3];
 struct nn_cs_buffer ubos[16],ssbos[16];nn_bo *resources[3];
};
/* Normal volume kernel only, fresh private UBO, actual backing nonalias
 * proven by the owning caller. Different BO/GPUVA is not such a proof. */
int nn_volume_copy_bind(nn_device *,const struct nn_cs_image *,const struct nn_tex_volume_layout *,const struct nn_volume_copy_plan *,
 const struct nn_block_copy_range *,const struct nn_block_copy_range *,const struct nn_block_copy_range *,
 enum nn_block_copy_alias,struct nn_volume_copy_binding *);
