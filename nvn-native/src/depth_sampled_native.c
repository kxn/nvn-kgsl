/* SPDX-License-Identifier: MIT. Original Program provenance + actual depth FDL. */
#include "nvn_native/depth_sampled_native.h"
#include "nvn_native/sampled_image.h"
#include "nvn_native/pipeline_native_program.h"
#include <errno.h>
#include <string.h>
int nn_depth_sampled_program(const nn_pipeline_program*p,const struct nn_pipeline_shader_recipe*r,unsigned binding,uint32_t*out)
{
 if(!out)return EINVAL;
 *out=0;uint32_t operation=0;
 int e=nn_sampled_image_program(p,r,binding,&operation);if(e)return e;
 uint32_t typed=0;if((e=nn_depth_sampled_types(nn_pipeline_program_texture_types(p),binding,&typed)))return e;
 if(typed!=operation)return EINVAL;
 *out=typed;return 0;
}
int nn_depth_sampled_descriptor(const struct nn_depth_fdl_view*v,const struct nn_pipeline_buffer*b,uint32_t out[16])
{
 if(!out)return EINVAL;
 memset(out,0,64);int e=nn_depth_sampled_validate(v);if(e)return e;
 if(v->desc.address||!b||!b->bo||b->offset>nn_bo_size(b->bo)||b->bytes>nn_bo_size(b->bo)-b->offset||v->required_span>b->bytes)return EINVAL;
 uint64_t a=nn_bo_address(b->bo);if(!a||b->offset>UINT64_MAX-a||b->bytes>UINT64_MAX-a-b->offset)return EOVERFLOW;
 struct nn_depth_fdl_desc actual=v->desc;actual.address=a+b->offset;
 struct nn_depth_fdl_view addressed;if((e=nn_depth_fdl(&actual,&addressed)))return e;
 memcpy(out,addressed.descriptor,64);return 0;
}
