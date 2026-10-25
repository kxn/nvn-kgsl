/* SPDX-License-Identifier: MIT. Independent actual-FDL depth metadata, not GPU admission. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NN_DEPTH_FDL_LEVELS 15u
#define NN_DEPTH_ATTACHMENT 1u
#define NN_DEPTH_SAMPLED 2u
/* Explicit raw-native sampled swizzles. These do NOT assert an NVN default. */
enum nn_depth_swizzle { NN_DEPTH_R=0,NN_DEPTH_ZERO=1,NN_DEPTH_ONE=2 };
struct nn_depth_format_info {
 uint32_t schema,format,has_depth,has_stencil,depth_bits,stencil_bits,source_bytes;
 /* Only depth-only D16/D24X8/D32F are admitted by the first native contract.
  * Classification of S8/combinedDS is not plane layout or stencil support. */
 uint32_t roles;
};
struct nn_depth_fdl_desc {
 uint32_t schema,chip,format,target,width,height,levels,layers,base_level,base_layer,roles;
 uint32_t swizzle[4],reserved;
 uint64_t address;
};
struct nn_depth_fdl_level {
 uint32_t width,height,pitch,layer_stride,tile_mode,reserved;
 uint64_t offset,span; /* actual FDL slice.size0, not a linear active-row formula */
};
struct nn_depth_fdl_view {
 struct nn_depth_fdl_desc desc;
 uint32_t view_width,view_height,bytes_per_pixel,alignment,native_depth_format,depth_bits;
 uint32_t rb_depth_buffer_info,rb_depth_buffer_pitch,rb_depth_buffer_array_pitch,gras_su_depth_buffer_info;
 uint32_t descriptor[16],descriptor_address_word,reserved;
 uint64_t bytes,required_span,view_offset,descriptor_address_mask;
 struct nn_depth_fdl_level level[NN_DEPTH_FDL_LEVELS];
};
/* Raw native control values. Compare is public NVN DepthFunc1..8. Stencil,
 * bounds/clamp, MSAA, LRZ and early-Z are intentionally not admitted. The caller
 * obtains depth_regid from actual IR3 output metadata, never a fixed shader ID.
 * write_enable requires test_enable; an NVN frontend must map disabled writes
 * correctly instead of passing its inactive default write bit unmodified. */
struct nn_depth_controls_desc {
 uint32_t schema,chip,test_enable,write_enable,compare,shader_writes_depth,depth_regid,reserved;
};
struct nn_depth_controls {
 uint32_t rb_depth_cntl,gras_su_depth_cntl,rb_ps_output_cntl,sp_ps_output_cntl;
 uint32_t rb_depth_plane_cntl,gras_su_depth_plane_cntl,rb_lrz_cntl,gras_lrz_cntl;
};
__attribute__((visibility("default"))) uint32_t nn_depth_fdl_version(void);
__attribute__((visibility("default"))) int nn_depth_fdl_format(uint32_t,struct nn_depth_format_info *);
/* Exact A650 TILE6_3/nonUBWC/sample1 2D/array, a selected single mip/layer.
 * Layout of every mip/layer is returned, but attachment/sampling consumer
 * capability remains independent. No allocations, BOs or pixel access. Positive
 * errno; result is all-zero on failure. No changes to existing FDL ABI/gates. */
__attribute__((visibility("default"))) int nn_depth_fdl(const struct nn_depth_fdl_desc *,struct nn_depth_fdl_view *);
__attribute__((visibility("default"))) int nn_depth_fdl_controls(const struct nn_depth_controls_desc *,struct nn_depth_controls *);
#ifdef __cplusplus
}
#endif
