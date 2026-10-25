/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NN_SAMPLER_STATE_VERSION 1u
/* Pure immutable value snapshot. Enum fields use public NVN numeric values,
 * not Adreno encodings; no object pointer or shader identity is stored. */
struct nn_sampler_state {
 uint32_t version,min_filter,mag_filter,wrap[3],compare_mode,compare_func,reduction;
 float lod_min,lod_max,lod_bias,anisotropy,lod_snap,border[4];
};
struct nn_sampler_layout {
 uint32_t width,height,levels,samples,dimensions,normalized,depth,integer;
};
/* Common A650 normalized color2D path. Resource layout is an independent
 * input; 1..15 levels/sample1, no depth/integer. Multiple levels currently
 * require non-mip NEAREST min/mag and all three CLAMP_TO_EDGE wraps.
 * min filters0..5, mag0..1; repeat/mirror-repeat/edge. Exact aniso1/2/4/8/16.
 * LOD bounds finite[-1000,1000], ordered; hardware fields saturate[0,4095/256].
 * Bias signed13 radix8 accepts[-16,4095/256], truncates as Mesa generated pack.
 * Border color is retained metadata and inactive in these three wrap modes.
 * Border-consuming wraps/shadow/reduction-minmax/LOD-snap/unnormalized remain
 * unsupported; no implicit border table or fallback is emitted. */
int nn_sampler_state_validate(const struct nn_sampler_state *state);
/* Emits exactly one 4DWORD sampler, no table hole or GPU allocation. Failure
 * leaves all output unchanged. API callers allocate/zero holes independently. */
int nn_sampler_state_encode(uint32_t chip,const struct nn_sampler_state *state,
                            const struct nn_sampler_layout *layout,uint32_t words[4]);
/* Explicit isotropic normalized FLOAT2D mip profile. Same descriptor words,
 * state fields/ranges and validation; permits all six minfilters, either mag,
 * repeat/mirror-repeat/edge and actual selected mip count1..15. Multiple levels
 * require anisotropy1 until independently qualified. No implicit LOD, target,
 * coordinate, format or shader-operation state is invented here. Caller must
 * supply the actual FDL-selected view and program-owned typed sampling proof.
 * This implementation-stage entry does not enable public consumers; integration
 * uses an explicit feature gate until the separate normal GPU batch passes. */
int nn_sampler_state_encode_mipmapped(uint32_t chip,const struct nn_sampler_state *,
 const struct nn_sampler_layout *,uint32_t words[4]);
/* Noncompare normalized native2D depth role, depth=1, integer=0. Existing
 * state/wrap/filter gates remain; shadow compare, border and reduction reject.
 * Resource owner must independently prove an actual depth FDL descriptor. */
int nn_sampler_state_encode_depth_noncompare(uint32_t chip,const struct nn_sampler_state *,
 const struct nn_sampler_layout *,uint32_t words[4]);
#ifdef __cplusplus
}
#endif
