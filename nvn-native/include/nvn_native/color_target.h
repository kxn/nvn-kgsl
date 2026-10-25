/* SPDX-License-Identifier: MIT. Actual-FDL, single color-attachment metadata. */
#pragma once
#include "texture_native_view.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Only float32 vec4 shader outputs, native linear/nonUBWC 2D single-mip,
 * single-layer/sample. Integer shader outputs, Z/S, MRT>1 and GPU work are not
 * implemented here. Metadata success is not a GPU qualification claim. */
struct nn_color_target_info {
 uint32_t bytes_per_pixel,component_mask,srgb,color_format;
};
/* Value results, no BO/address/allocation or hidden owner. Caller keeps the
 * immutable result alive through recording/submission. Actual FDL controls
 * every layout, texture descriptor, RB format/swap and SP output format field.
 * Explicit pitch remains exactly the current FDL facade's admitted subset. */
int nn_color_target_create(uint32_t chip,uint32_t format,uint32_t width,
 uint32_t height,uint32_t explicit_pitch,struct nn_tex_native_view *out,
 struct nn_color_target_info *info);
/* Recompute actual FDL metadata, compare every native-view field. This is a
 * bounded CPU metadata check, not an authenticator or resource lifetime pin.
 * No GOB/canonical pool may be substituted for native storage. */
int nn_color_target_validate(const struct nn_tex_native_view *,struct nn_color_target_info *);
/* Seven packet4 register/value pairs: RB_MRT BUF/PITCH/ARRAY, SP_MRT format,
 * GRAS LRZ color format, RB and SP sRGB controls. No GPUVA or draw packet.
 * Query with words=NULL,capacity=0. Error leaves words untouched/count0.
 * Shader regid/full precision and blend state remain independent snapshots. */
int nn_color_target_words(const struct nn_tex_native_view *,uint32_t *words,
 size_t capacity,size_t *count);
#ifdef __cplusplus
}
#endif
