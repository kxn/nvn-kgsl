/* SPDX-License-Identifier: MIT. CPU metadata only; original FDL does the work. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NN_FDL_LEVELS 15u
#define NN_FDL_SAMPLED 1u
#define NN_FDL_COLOR_TARGET 2u
struct nn_fdl_image_desc {
 uint32_t schema,chip,format,target,width,height,levels,layers;
 uint32_t base_level,level_count,base_layer,layer_count,usage,explicit_pitch;
 uint64_t address; /* 0=zero address fields; otherwise actual allocation base */
};
struct nn_fdl_level {uint32_t width,height,pitch,layer_stride;uint64_t offset,span;};
struct nn_fdl_image_view {
 uint32_t schema,chip,format,target,width,height,levels,layers;
 uint32_t block_width,block_height,bytes_per_block,alignment,view_width,view_height;
 uint32_t base_level,level_count,base_layer,layer_count,descriptor_address_word;
 uint64_t bytes,required_span,view_offset,descriptor_address_mask;
 uint32_t descriptor[16],color_target,rb_mrt_buf_info,rb_mrt_pitch,rb_mrt_array_pitch,sp_ps_mrt_reg;
 struct nn_fdl_level level[NN_FDL_LEVELS];
};
struct nn_fdl_buffer_desc {uint32_t schema,chip,format,bytes;uint64_t address;};
struct nn_fdl_buffer_view {
 uint32_t schema,chip,format,element_bytes,elements,descriptor[16];
 uint64_t base_address,required_prefix,required_span,address_mask;
};
/* Separate append-only true3D API. No changes to either existing image or
 * buffer ABI. Sampled-only native linear/nonUBWC/sample1, the same nondepth color
 * format mapping (including BC sRGB) as image metadata; no 3D MRT, sampler or
 * runtime resource capability. */
struct nn_fdl_volume_desc {
 uint32_t schema,chip,format,width,height,depth,levels,base_level,level_count,usage;
 uint64_t address;
};
struct nn_fdl_volume_level {
 uint32_t width,height,depth,pitch;
 uint64_t offset,slice_stride,span; /* full FDL slice allocation * mip depth */
};
struct nn_fdl_volume_view {
 uint32_t schema,chip,format,width,height,depth,levels,base_level,level_count;
 uint32_t view_width,view_height,view_depth,block_width,block_height,bytes_per_block,alignment;
 uint32_t descriptor[16];
 uint64_t bytes,required_span,view_offset,descriptor_address_mask;
 uint32_t descriptor_address_word,min_layer_size; /* last FDL slice.size0, bytes */
 struct nn_fdl_volume_level level[NN_FDL_LEVELS];
};
/* Append-only sampled image traits from the same original image_format map
 * and Mesa description/support table. This is no MRT/filter/shader admission.
 * FLOAT includes normalized and sRGB storage; INTEGER is never silently aliased
 * to a float descriptor. Block dimensions explicitly distinguish compressed. */
enum {NN_FDL_RESULT_FLOAT=1,NN_FDL_RESULT_UINT=2,NN_FDL_RESULT_SINT=3};
struct nn_fdl_format_info {
 uint32_t schema,chip,format,result_type,block_width,block_height,bytes_per_block;
 uint32_t normalized,snorm,srgb,channel_bits[4];
};
__attribute__((visibility("default"))) int nn_fdl_image_format_info(uint32_t chip,uint32_t format,struct nn_fdl_format_info *);
/* Existing metadata_version remains1 for its unchanged image/buffer ABI.
 * Separate feature version avoids pretending old libraries export volume. */
__attribute__((visibility("default"))) uint32_t nn_fdl_volume_metadata_version(void);
__attribute__((visibility("default"))) int nn_fdl_volume(const struct nn_fdl_volume_desc *,struct nn_fdl_volume_view *);
/* Exact A650 native LINEAR, UBWC=false, sample1. Images: R/RG/RGBA8/16 UNORM/SNORM/UINT/SINT,
 * R/RG/RGBA16F/32F/32UI/32I, R11G11B10F, RGBA8/BGRA8 sRGB, BGRA8 UNORM, BC1 RGB+RGBA/
 * BC2/BC3 UNORM and sRGB, 2D/array. Color-target only original Mesa-supported uncompressed
 * nondepth formats with one selected mip/layer. Explicit pitch only RGBA8 mip1/layer1.
 * Buffer: actual FDL BUFFER plain R/RG/RGBA 8/16/32 color, RGB32 and
 * BGRA8, scalar stride1; no packed mappings, compressed/depth/sRGB. Element-aligned length
 * and representable aligned-down64 prefix are mandatory. This is metadata,
 * not permission for the still separately gated native BUFFER shader path. Positive errno,
 * output zero on failure. No image data pointers, BOs, allocations or GPU work.
 * Format mapping is CPU metadata support; shader scalar type, sampler, MRT
 * pipeline and GPU resource admission remain independently qualified.
 * Metadata has no provenance/ownership substitute for an actual live resource.
 * Buffer aligned-down prefix must be covered by caller's true owner allocation.
 * A successful Android cross-link is not evidence of Android runtime/GPU use. */
__attribute__((visibility("default"))) uint32_t nn_fdl_metadata_version(void);
__attribute__((visibility("default"))) int nn_fdl_image(const struct nn_fdl_image_desc *,struct nn_fdl_image_view *);
__attribute__((visibility("default"))) int nn_fdl_buffer(const struct nn_fdl_buffer_desc *,struct nn_fdl_buffer_view *);
#ifdef __cplusplus
}
#endif
