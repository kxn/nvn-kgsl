/* SPDX-License-Identifier: MIT. Typed native texture-unit depth role. */
#pragma once
#include "depth_sampled.h"
#include "pipeline_program.h"
#include "pipeline_shader.h"
#ifdef __cplusplus
extern "C" {
#endif
int nn_depth_sampled_program(const nn_pipeline_program *,const struct nn_pipeline_shader_recipe *,unsigned binding,uint32_t *operations);
/* Exact address-zero depth FDL + true native window, zero result on failure.
 * Rebuild actual addressed view through FDL, no descriptor bit arithmetic. */
int nn_depth_sampled_descriptor(const struct nn_depth_fdl_view *,const struct nn_pipeline_buffer *,uint32_t out[16]);
#ifdef __cplusplus
}
#endif
