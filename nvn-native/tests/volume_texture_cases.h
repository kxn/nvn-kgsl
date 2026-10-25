/* SPDX-License-Identifier: MIT. Actual FDL mapping, no layout algorithm. */
#pragma once
#include "nvn_native/pipeline.h"
#include <errno.h>
#include <string.h>
static inline int nn_volume_texture_target(unsigned w,unsigned h,struct nn_tex_native_view*out)
{struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=w,.height=h,.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=NN_FDL_SAMPLED|NN_FDL_COLOR_TARGET};struct nn_fdl_image_view v;int e=nn_fdl_image(&d,&v);if(e)return e;if(v.bytes>UINT32_MAX||v.required_span>UINT32_MAX)return EOVERFLOW;*out=(struct nn_tex_native_view){.schema=1,.chip=v.chip,.format=37,.target=1,.levels=1,.layers=1,.samples=1,.width=w,.height=h,.pitch=v.level[0].pitch,.layer_bytes=v.level[0].layer_stride,.bytes=(uint32_t)v.bytes,.alignment=v.alignment,.required_span_bytes=(uint32_t)v.required_span,.descriptor_address_word=v.descriptor_address_word,.descriptor_offset=v.view_offset,.descriptor_address_mask=v.descriptor_address_mask,.rb_mrt_buf_info=v.rb_mrt_buf_info,.rb_mrt_pitch=v.rb_mrt_pitch,.rb_mrt_array_pitch=v.rb_mrt_array_pitch,.sp_ps_mrt_reg=v.sp_ps_mrt_reg};memcpy(out->texture_descriptor,v.descriptor,64);return 0;}
