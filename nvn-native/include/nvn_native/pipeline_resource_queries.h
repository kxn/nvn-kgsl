/* SPDX-License-Identifier: MIT. Source resource identity, not native descriptor layout. */
#pragma once
#include "nvn_native/pipeline_frontend.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NN_PIPELINE_RESOURCE_QUERIES_VERSION 1u
#define NN_PIPELINE_RESOURCE_BINDINGS 192u
/* Existing core handle domains. These are neither NVN pool IDs nor native slots. */
enum nn_pipeline_handle_domain { NN_PIPELINE_COMBINED,NN_PIPELINE_SEPARATE_TEXTURE,NN_PIPELINE_SEPARATE_SAMPLER };
/* Trusted CPU values copied by the frontend after it validates the actual
 * selected descriptor pool, registration/raw-slot integrity, backing range,
 * same-device/access policy and owner pin. Never construct this from an
 * archive requirement or guest-provided struct. Pointers are opaque CPU owner
 * identities; the module never dereferences or emits them into GPU memory.
 * It borrows ownership and does not pin, allocate, cache, submit or retire.
 * Copying this value does not retain either owner. */
struct nn_pipeline_registered_texture {
 const void *descriptor_pool,*backing_pool;
 uint32_t registered,descriptor_id,target,format,width,height,depth,levels,samples;
 uint64_t offset,bytes;
};
struct nn_pipeline_resource_binding {
 uint32_t stage,domain,selector,low_word_defined;
 /* Already validated/normalized by the existing public bind's handle decoder.
  * Preserve guest identity, but samplerless low-word reads do not require a
  * defined high CB2 word (scalar Bind does not write that word). */
 uint32_t payload;uint64_t guest_carrier;
 struct nn_pipeline_registered_texture texture;
};
/* Translator handle packing, independently of descriptor IDs/native indices.
 * Only CB2 combined and separate-handle tables are proved here. Scalar bind
 * reads low32; the unused neighbouring CB word is not fabricated. */
struct nn_pipeline_resource_reference {
 uint32_t texture_domain,texture_selector,texture_word;
 uint32_t sampler_domain,sampler_selector,sampler_word,has_sampler;
};
int nn_pipeline_resource_reference(int32_t handle_word,int32_t cbuf_slot,
 struct nn_pipeline_resource_reference *);
struct nn_pipeline_resource_queries {
 uint32_t version,graphics_texture_cb_known,graphics_texture_cb;
 const void *selected_texture_pool;
 const struct nn_pipeline_resource_binding *bindings;size_t binding_count;
};
struct nn_pipeline_resolved_texture {
 uint32_t stage,cb_slot,word,selector,descriptor_id,sampler_type;
 struct nn_pipeline_registered_texture texture;
};
/* SDK53.314 successful standard graphics QueueInitialize establishes CB2.
 * BindSeparateTexture writes low32 at0x168+8*APIselector. A cbuf=-1 immediate
 * handle word110/112 therefore addresses selector10/11, not descriptor IDs.
 * Only an explicitly known CB2 contract, VS/FS and original table offsets
 * are admitted. Packed separate handles and combined table use distinct
 * domains; other CBs, direct/ID/constant sampler forms stay unknown.
 * Current concrete resource subset: ordinary actual2D or a registered typed
 * plain color BUFFER with exact width*format-element byte span.
 * Ordinary2D permits mip chains; BUFFER remains depth1/mip1/non-MS.
 * BUFFER source type2 is distinct from2D type3 even at height1. This does
 * not assert a particular real game binding or admit native GPU fetch.
 * 1D/array/MS remain explicit ENOTSUP.
 * QuerySamplerType describes source dimension; it is not hardware format,
 * normalization, native layout or permission to encode a Draw.
 * Failure clears out. No expected shader query values enter this interface. */
int nn_pipeline_resource_texture(const struct nn_pipeline_resource_queries *,uint32_t stage,
 int32_t handle_word,int32_t cbuf_slot,struct nn_pipeline_resolved_texture *out);
int nn_pipeline_resource_query(void *,uint32_t,const struct nn_pipeline_query *,uint32_t out[32],size_t *count);
struct nn_pipeline_frontend_resource_queries {
 struct nn_pipeline_frontend_queries graphics;
 struct nn_pipeline_resource_queries resources;
};
/* Compose with the existing graphics/capability query implementation. Actual
 * registered2D/BUFFER use the translator's exact scalar color format enum,
 * independently of native format/operation admission. Missing representations
 * (including BGRA/sRGB/BC/depth) remain unsupported, never default RGBA8 or
 * Unknown. Unknown support-buffer namespaces stay unsupported. Ordinary actual2D
 * normalization follows the explicit public-definition + independent-RE
 * policy; it is not an observed SDK53.314 descriptor or shader-expected value.
 * Source-program variant selection and native descriptor materialization MUST
 * use this SAME immutable per-enqueue snapshot. Record-time selectors/pool
 * identities may be retained, but registration is late-bound: do not select
 * using the old entry and then encode a newly registered entry. */
int nn_pipeline_frontend_resource_query(void *,uint32_t,const struct nn_pipeline_query *,uint32_t out[32],size_t *count);
#ifdef __cplusplus
}
#endif
