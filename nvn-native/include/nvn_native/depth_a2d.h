/* SPDX-License-Identifier: MIT. Actual-FDL aliases, not CPU depth conversion. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "nvn_native/depth_fdl.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_depth_a2d_surface {
 uint32_t source_info,source_size,destination_info,pitch,width,height,alignment,reserved;
 uint64_t address,offset,bytes;
};
struct nn_depth_a2d_view {
 struct nn_depth_fdl_desc desc;
 uint64_t linear_address;
 struct nn_depth_a2d_surface tiled,linear;
 uint32_t alias_pipe_format,blit_control,output_info,reserved;
};
enum nn_depth_a2d_op { NN_DEPTH_A2D_CLEAR=0,NN_DEPTH_A2D_READBACK=1,NN_DEPTH_A2D_UPLOAD=2 };
struct nn_depth_a2d_region { uint32_t x,y,width,height; };
/* Only A650 depth-only D16/D24X8/D32F, actual TILE6_3 <-> actual FDL
 * LINEAR alias, sample1/nonUBWC, selected one mip/layer. All-zero output on
 * failure; zero addresses permit metadata queries. Does not allocate/map. */
__attribute__((visibility("default"))) int nn_depth_a2d(const struct nn_depth_fdl_desc *,uint64_t linear_address,struct nn_depth_a2d_view *);
/* Pure PM4 BODY, NOT a standalone submission. Caller must restore verified
 * sysmem CCU platform, FULL_CACHE before and after, invalidate before graphics,
 * and keep all BOs live through known retirement. CP_BLIT writes through CCU.
 * CLEAR writes actual tiled storage, READBACK copies it to LINEAR storage,
 * UPLOAD copies the same-format LINEAR alias to actual tiled storage. Each
 * direction independently uses the actual FDL source/destination metadata.
 * No format substitution, CPU pixels, shader or pointer access. NULL words
 * queries exact count. Error leaves words unchanged/count zero. */
__attribute__((visibility("default"))) int nn_depth_a2d_words(const struct nn_depth_a2d_view *,uint32_t op,const struct nn_depth_a2d_region *,float clear_depth,uint32_t *,size_t,size_t *);
#ifdef __cplusplus
}
#endif
