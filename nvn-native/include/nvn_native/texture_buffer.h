/* SPDX-License-Identifier: MIT. Actual FDL typed buffer, never a 2D image. */
#pragma once
#include "kgsl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Trusted immutable normal-producer metadata. The normal record pins FDL,
 * generated packers, this header and every value. This structural validator
 * does not authenticate arbitrary caller-created metadata or infer NVN view
 * legality. Only float-result RGBA8 UNORM(37)/R32F(10), identity swizzle,
 * scalar single-level BUFFER are covered. No image pitch/GOB layout exists.
 * Masks/template come from fdl6_buffer_view_init + generated A6xx fields. */
struct nn_tex_buffer_recipe {
 uint32_t schema,chip,format,element_bytes,descriptor[16];
 uint32_t width_mask,height_mask,height_shift,offset_mask,offset_shift;
 uint64_t address_mask;
};
/* Value snapshot. resource is borrowed, canonical owner/range never changes.
 * Caller must retain its live same-device pool/BO and immutable recipe through
 * recording/submission/completion, append resource to actual submit refs, and
 * preserve the owner on unknown completion. Copying this value does not pin a
 * pool or BO. No opaque object registry, hidden allocation or GPU work here. */
struct nn_tex_buffer_binding {
 uint32_t format,element_bytes,elements,descriptor[16];
 nn_bo *resource;
 size_t owner_offset,owner_bytes,offset,bytes;
 uint64_t address,descriptor_base;
};
int nn_tex_buffer_recipe_validate(const struct nn_tex_buffer_recipe *);
/* offset/owner_offset are relative to this same original BO. owner_bytes is
 * the caller-proved pool allocation span, not the descriptor pool. Exact
 * bytes must be element-aligned. The aligned-down descriptor base and entire
 * logical texel range must remain in that owner span. Read-only alias views
 * are allowed; this function proves no physical disjointness and inserts no
 * cache/dependency protocol. The command frontend must check write hazards.
 * On error out is zero. No CPU image mapping/cache or BO lifetime mutation. */
int nn_tex_buffer_bind(const struct nn_tex_buffer_recipe *,nn_bo *,
 size_t owner_offset,size_t owner_bytes,size_t offset,size_t bytes,
 struct nn_tex_buffer_binding *out);
#ifdef __cplusplus
}
#endif
