/* SPDX-License-Identifier: MIT. Native GPU command-plan metadata, not guest CPU
 * execution. No NVN entry or GPU opcode is defined by this foundation API. */
#pragma once
#include "nvn_native/render_state.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_command_sequence;
/* An actual handle/body owns its sequence. Native spans borrow that same
 * owner. A CALL strongly holds the child owner, whose graph must remain valid
 * until release; the child never holds its own owner (no ownership cycle).
 * release must use the backend's real retirement ledger for busy GPU memory. */
struct nn_command_owner_ops {
 int (*retain)(void *owner);
 void (*release)(void *owner);
 int (*validate_span)(const void *owner,size_t offset,size_t bytes,unsigned kind);
};
enum nn_command_leaf_kind { NN_COMMAND_RAW=0,NN_COMMAND_DRAW=1,NN_COMMAND_COMPUTE=2,NN_COMMAND_COUNTER=3,NN_COMMAND_COPY=4 };
#define NN_COMMAND_LEAF_KIND_BITS UINT32_C(31)
#define NN_COMMAND_STATE_BLEND(target) (UINT64_C(1)<<(target))
#define NN_COMMAND_STATE_COLOR (UINT64_C(1)<<8)
#define NN_COMMAND_STATE_MASK (UINT64_C(1)<<9)
#define NN_COMMAND_STATE_POLYGON (UINT64_C(1)<<10)
#define NN_COMMAND_STATE_DEPTH_STENCIL (UINT64_C(1)<<11)
#define NN_COMMAND_STATE_MULTISAMPLE (UINT64_C(1)<<12)
#define NN_COMMAND_STATE_BLEND_COLOR (UINT64_C(1)<<13)
#define NN_COMMAND_STATE_DEPTH_BOUNDS (UINT64_C(1)<<14)
#define NN_COMMAND_STATE_POLYGON_OFFSET (UINT64_C(1)<<15)
#define NN_COMMAND_STATE_STENCIL_VALUE_FRONT (UINT64_C(1)<<16)
#define NN_COMMAND_STATE_STENCIL_VALUE_BACK (UINT64_C(1)<<17)
#define NN_COMMAND_STATE_STENCIL_WRITE_FRONT (UINT64_C(1)<<18)
#define NN_COMMAND_STATE_STENCIL_WRITE_BACK (UINT64_C(1)<<19)
#define NN_COMMAND_STATE_STENCIL_REF_FRONT (UINT64_C(1)<<20)
#define NN_COMMAND_STATE_STENCIL_REF_BACK (UINT64_C(1)<<21)
#define NN_COMMAND_STATE_PATCH_SIZE (UINT64_C(1)<<22)
#define NN_COMMAND_STATE_VIEWPORT (UINT64_C(1)<<23)
#define NN_COMMAND_STATE_SCISSOR (UINT64_C(1)<<24)
#define NN_COMMAND_STATE_DEPTH_RANGE (UINT64_C(1)<<25)
#define NN_COMMAND_STATE_BITS ((UINT64_C(1)<<26)-1)
/* Compact canonical binding slots: no guest pointer is dereferenced. */
enum nn_command_binding_domain { NN_COMMAND_PROGRAM,NN_COMMAND_UBO,NN_COMMAND_SSBO,
 NN_COMMAND_VERTEX_BUFFER,NN_COMMAND_TEXTURE_POOL,NN_COMMAND_SAMPLER_POOL,
 NN_COMMAND_COMBINED_TEXTURE,NN_COMMAND_SEPARATE_TEXTURE,NN_COMMAND_SEPARATE_SAMPLER,
 NN_COMMAND_IMAGE,NN_COMMAND_TARGET,NN_COMMAND_VERTEX_ATTRIB,NN_COMMAND_VERTEX_STREAM };
#define NN_COMMAND_BINDING_COUNT 673u
#define NN_COMMAND_BINDING_WORDS 11u
struct nn_command_reference_ops {int (*retain)(void *);void (*release)(void *);};
struct nn_command_binding {
 void *owner;const struct nn_command_reference_ops *ops;
 uint64_t address,guest;size_t offset,bytes;uint32_t flags;
};
#define NN_COMMAND_BINDING_HIGH_DEFINED 1u
#define NN_COMMAND_BINDING_LAST_PLURAL 2u
#define NN_COMMAND_BINDING_PRESERVE_HIGH UINT32_C(0x80000000)
struct nn_command_binding_write {unsigned index;struct nn_command_binding value;};
struct nn_command_state {
 uint64_t defined;
 struct nn_render_state render;
 float viewport[4],depth_range[2];int32_t scissor[4];
 uint64_t binding_defined[NN_COMMAND_BINDING_WORDS];
 struct nn_command_binding bindings[NN_COMMAND_BINDING_COUNT];
};
struct nn_command_leaf {
 size_t offset,bytes;uint64_t required_state;uint64_t semantic_id;unsigned kind;
 uint64_t required_bindings[NN_COMMAND_BINDING_WORDS];
};
/* domain is an opaque exact device identity; it is never dereferenced. */
int nn_command_sequence_create(void *owner,const struct nn_command_owner_ops *ops,
 const void *domain,struct nn_command_sequence **out);
/* Construction is transactional. Caller state/payload is copied by value.
 * Only explicit GPU-state writes belong here. CPU recording controls such as
 * copy row/image stride never become inherited GPU state by this API. */
int nn_command_sequence_state(struct nn_command_sequence *,uint64_t mask,const struct nn_command_state *);
int nn_command_sequence_binding_writes(struct nn_command_sequence *,unsigned count,const struct nn_command_binding_write *);
int nn_command_sequence_bindings(struct nn_command_sequence *,enum nn_command_binding_domain,
 unsigned stage,unsigned first,unsigned count,const struct nn_command_binding *);
/* Returned state bindings borrow sequence/initial owners during visit. Clone
 * explicitly before persisting beyond those lifetimes. clone leaves out
 * untouched on failure; caller must release an old owned out before reuse. */
int nn_command_binding_index(enum nn_command_binding_domain,unsigned stage,unsigned slot,unsigned *out);
const struct nn_command_binding *nn_command_state_binding(const struct nn_command_state *,enum nn_command_binding_domain,unsigned stage,unsigned slot);
int nn_command_state_overlay_render(struct nn_command_state *destination,const struct nn_command_state *source);
int nn_command_state_clone(const struct nn_command_state *,struct nn_command_state *out);
void nn_command_state_release(struct nn_command_state *);
int nn_command_sequence_native(struct nn_command_sequence *,const struct nn_command_leaf *);
int nn_command_sequence_calls(struct nn_command_sequence *,unsigned,const struct nn_command_sequence *const *);
int nn_command_sequence_call(struct nn_command_sequence *,const struct nn_command_sequence *child);
int nn_command_sequence_unsupported(struct nn_command_sequence *);
int nn_command_sequence_seal(struct nn_command_sequence *);
void nn_command_sequence_destroy(struct nn_command_sequence *);
/* Preflight first: incomplete state, unqualified leaf family, unknown op,
 * owner validation failure, depth or expansion limits cause no callback or
 * final-state write. State returned from a child remains current in its
 * parent. final_state may alias initial_state. No default state is invented.
 * visit only constructs a transaction owned by caller; it must not submit GPU
 * work. Caller discards that transaction if any callback fails. */
typedef int (*nn_command_sequence_visitor)(void *context,const void *owner,
 const struct nn_command_leaf *,const struct nn_command_state *);
int nn_command_sequence_visit(const struct nn_command_sequence *,uint32_t qualified_kinds,
 const struct nn_command_state *initial_state,nn_command_sequence_visitor,void *context,
 struct nn_command_state *final_state,size_t *native_count);
/* Observe accepted native submission state only; unmodeled nodes explicitly invalidate all prior state. */
int nn_command_sequence_observe(const struct nn_command_sequence *,const struct nn_command_state *,struct nn_command_state *);
#ifdef __cplusplus
}
#endif
