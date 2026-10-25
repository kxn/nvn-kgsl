/* SPDX-License-Identifier: MIT */
#pragma once
#include "nvn_native/compute.h"
#include "nvn_native/texture_source.h"
#ifdef __cplusplus
extern "C" {
#endif
/* This is an actual FDL result from the normal HOST producer or checked shader
 * artifact, never an NVN pitch reinterpreted as native. RGBA8 LINEAR only. */
struct nn_tex_native_layout { uint32_t width,height,pitch,layer_bytes,bytes,alignment; };
struct nn_tex_bridge_bundle {
    uint32_t schema,chip;
    const struct nn_cs_image *gob;
    struct nn_tex_native_layout clear_layout;
};
/* Fresh driver-owned shadow storage cannot alias live imported/pinned caller
 * pages. Canonical BO/offset/address are never changed. Every consumer gets a
 * fresh version, including command replay; no dirty guess or first-use upload.
 * All BOs belong to caller until transferred/retired; error may return an owner
 * if cleanup fails. Never discard that owner. No submit or wait happens here. */
#define NN_TEX_SHADOW_BO_COUNT 5u
struct nn_tex_shadow { nn_bo *bos[NN_TEX_SHADOW_BO_COUNT]; size_t pre_bytes,post_bytes; };
enum { NN_TEX_SHADOW_STORAGE,NN_TEX_SHADOW_CODE,NN_TEX_SHADOW_UNIFORM,NN_TEX_SHADOW_PRE,NN_TEX_SHADOW_POST };
int nn_tex_bridge_bundle_validate(const struct nn_tex_bridge_bundle *);
int nn_tex_bridge_source_validate(const struct nn_tex_source_layout *);
int nn_tex_shadow_create(nn_device *,const struct nn_tex_bridge_bundle *,nn_bo *,size_t,size_t,
                         const struct nn_tex_source_layout *,const struct nn_tex_native_layout *,struct nn_tex_shadow *);
int nn_tex_shadow_destroy(struct nn_tex_shadow *);
/* Complete same-submission ordering: full CCU clean before canonical reads;
 * each compute producer flushes its writes; invalidate before graphics reads;
 * graphics child must have its checked clean tail; full clean before retile;
 * retile completion+invalidate before subsequent canonical consumers.
 * Child BO and all source BOs/shadow BOs remain caller-owned and must be refs.
 * Wrapper contains private timestamp scratch, never caller memory. */
/* At most16 actually consumed native textures plus one target input. Borrowed
 * input/output owners are never destroyed or rebound by the flatten helper. */
#define NN_TEX_SHADOW_CHAIN_MAX_INPUTS 17u
int nn_tex_shadow_chain(nn_device *,const struct nn_tex_shadow *const *,size_t,
                        nn_bo *,size_t,const struct nn_tex_shadow *,nn_bo **,size_t *);
/* Direct canonical<->borrowed native(AHB allowed) conversion. No native mapping
 * or CPU cache operation is required. Caller proves independent pinned source
 * and destination backing; this API does not infer physical nonalias from VA.
 * Source layout GOB/RGBA8/2D/one mip only; native extent is actual FDL. Native
 * bytes excludes prefix offset. Caller refs both borrowed BOs + all owner BOs.
 * Submit after explicit producer dependencies; Finish before SC/CPU/owner free.
 * Unknown submission keeps borrowed owners and this entire group alive. */
#define NN_TEX_CONVERSION_BO_COUNT 4u
struct nn_tex_conversion { nn_bo *bos[NN_TEX_CONVERSION_BO_COUNT];size_t entry_bytes; };
enum { NN_TEX_CONVERSION_CODE,NN_TEX_CONVERSION_UNIFORM,NN_TEX_CONVERSION_DISPATCH,NN_TEX_CONVERSION_ENTRY };
int nn_tex_conversion_create(nn_device *,const struct nn_tex_bridge_bundle *,nn_bo *canonical,
 size_t canonical_offset,size_t canonical_bytes,const struct nn_tex_source_layout *,const struct nn_tex_native_layout *,
 nn_bo *native,size_t native_offset,size_t native_bytes,uint32_t direction,struct nn_tex_conversion *);
int nn_tex_conversion_destroy(struct nn_tex_conversion *);

#ifdef __cplusplus
}
#endif
