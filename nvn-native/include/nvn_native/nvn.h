#pragma once
#include <nvn.h>
#include "compute_image.h"
#include "graphics_image.h"
#include "external_image.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Reconstructed external ABI; implementation state is kept outside caller objects.
 * Current API owner must serialize calls. Unsupported resolver names return NULL.
 * Sticky diagnostics expose errors from void NVN calls during qualification. */
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name);
int nn_nvn_error(void);
void nn_nvn_clear_error(void);
/* Native diagnostic only; not an NVN ABI entry or GPU support claim. Snapshot
 * of the first sticky policy rejection; pool pointer is identity, not ownership.
 * Getter never changes the sticky error. clear_error also clears this record. */
enum nn_nvn_gpu_pool_policy {
    NN_NVN_GPU_POOL_CACHED = 0,
    NN_NVN_GPU_UNCACHED_UNQUALIFIED = 1
};
struct nn_nvn_pool_access_observation {
    const NVNmemoryPool *pool;
    int flags;
    enum nn_nvn_gpu_pool_policy policy;
    size_t offset, size;
    const char *consumer;
};
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out);
/* Read-only native diagnostics, not an NVN entry or source-access grant.
 * Caller serializes with core mutations. No guest pointer is dereferenced;
 * offset/available describe a live pool range, never a shader's true length.
 * Getter preserves sticky errors, refs and output on failure. */
struct nn_nvn_program_source_info {
    const NVNmemoryPool *pool;
    int flags;
    enum nn_nvn_gpu_pool_policy policy;
    size_t offset, available;
    unsigned cpu_readable;
};
int nn_nvn_get_program_source_info(const NVNprogram *, NVNbufferAddress,
                                 struct nn_nvn_program_source_info *);
/* Read-only builder diagnostics for an actual unsupported storage query.
 * Stored packaged bytes are metadata, never interpreted as a layout here.
 * No guest reads, allocations, ownership changes or sticky-error changes;
 * output is unchanged on lookup failure. Caller serializes core mutations. */
struct nn_nvn_texture_builder_observation {
    uint32_t fields, packaged, device_live, source_policy;
    int flags, target, format, width, height, depth, levels, samples;
    ptrdiff_t stride;
    int source_layout_errno;
    unsigned char packaged_layout[8];
};
int nn_nvn_get_texture_builder_observation(const NVNtextureBuilder *,
    struct nn_nvn_texture_builder_observation *);
int nn_nvn_api_probe(char *json, size_t capacity);
/* Host-only explicit allocator import. New pool key only; ordinary caller
 * storage/texture backing is never replaced. Ownership transfers on success.
 * The only permitted texture is the full64RGBA payload at pixel_offset. */
int nn_nvn_import_image_pool(NVNdevice *, NVNmemoryPool *, const struct nn_external_image_desc *);
/* Owner-thread hooks for a future Window, not guest NVN entry points. Borrowed
 * view needs a successful pin until its compositor/GPU use safely finishes. */
int nn_nvn_image_pool_view(NVNmemoryPool *, struct nn_external_image_view *);
int nn_nvn_image_pool_pin(NVNmemoryPool *);
int nn_nvn_image_pool_unpin(NVNmemoryPool *);

/* Limited Window host inspection, not a guest ABI entry. Platform lifecycle
 * uses the separate pure-C window_session.h registry, never core callbacks. */
struct nn_window_session_stats;
int nn_nvn_window_get_stats(NVNwindow *, struct nn_window_session_stats *);

/* Initial descriptor contract: 32-byte API slots, 256 reserved entries;
 * pool storage is caller-owned cached NVN memory. Registration writes native
 * software descriptors and snapshots texture layout/backing storage or sampler
 * values. The source NVNtexture/NVNsampler may be finalized afterwards; backing
 * pools remain referenced until replacement or descriptor-pool finalization.
 * These slots are not Maxwell TIC/TSC or directly consumable Adreno packets.
 * Separate and combined handles retain the original low-20/high-12 ID encoding.
 * Only normalized 2D LINEAR RGBA8, one mip, nearest/clamp_to_edge is qualified:
 * 64x64 RT pitch256/size16384, or 2x2 sampled layout pitch256/size4096, both
 * alignment64. Selector bindings currently support vertex/fragment slots0..31.
 * Texture sampling requires explicit checked offline reflection below; arbitrary
 * caller-written descriptor bytes and unknown mappings are not accepted.
 * Unsupported device properties, views, filters and layouts report errors. */
struct nn_nvn_cbuf { uint32_t slot, binding; };
struct nn_nvn_sbuf { uint32_t binding, cbuf_slot, cbuf_word; };
struct nn_nvn_compiled_cs {
    const struct nn_cs_image *image;
    const void *source_code, *source_control;
    size_t source_bytes, control_bytes;
    const struct nn_nvn_cbuf *cbufs; size_t cbuf_count;
    const struct nn_nvn_sbuf *sbufs; size_t sbuf_count;
};
/* Explicit offline compiler-cache population, not an NVN entry. Immutable
 * identity/reflection/image storage must remain valid until DeviceFinalize.
 * Only empty compile-time C1 and the explicit compiler profile are qualified;
 * ProgramSetShaders matches full original input bytes, misses return ENOTSUP. */
int nn_nvn_register_compute(NVNdevice *device, const struct nn_nvn_compiled_cs *compiled);
struct nn_nvn_graphics_stage {
    const void *source_code, *source_control;
    size_t source_bytes, control_bytes;
    const struct nn_nvn_cbuf *cbufs; size_t cbuf_count;
};
enum nn_nvn_handle_domain { NN_NVN_HANDLE_COMBINED = 0, NN_NVN_HANDLE_SEPARATE = 1 };
/* API selector domains are independent of pool IDs and native IR3 indices.
 * Maxwell cbuf words identify the original source operands, not API slots. */
struct nn_nvn_sampled {
    uint32_t stage, native_texture_index, native_sampler_index;
    uint32_t texture_domain, texture_slot, sampler_domain, sampler_slot;
    uint32_t texture_cbuf, texture_word, sampler_cbuf, sampler_word;
};
struct nn_nvn_compiled_graphics {
    const struct nn_gfx_image *image;
    struct nn_nvn_graphics_stage vs, fs;
    const struct nn_nvn_sampled *sampled; size_t sampled_count;
};
/* Immutable offline identity, reflection and image storage must remain valid
 * until DeviceFinalize. SetShaders matches BOTH complete original code/control
 * ranges, independently of input array order. Initial untextured profile maps
 * VS Maxwell c3 (NVN Uniform 0) to compiler binding 1, with no FS constants.
 * The normalized-2D sampled profile additionally maps FS c3 to binding2 and
 * one resolved CB2 separate texture/sampler pair to native indices1/1. Slots,
 * source words and pool IDs are independent. Draw copies selectors and fixed
 * state; each enqueue resolves the current registered pool entries and builds
 * immutable private parent/child IBs with retained backing BOs. Re-registration
 * before a later enqueue is visible without changing prior inflight versions.
 * Callers must synchronize CPU writes to backing texels/constants with GPU use
 * and flush cached mapped ranges; concurrent inflight data writes are outside
 * this qualification. Only the explicit empty-C1 profile is qualified. Color
 * schema1 has no sequence storage; schema2 adds the chip-qualified internal
 * sequence resource. Registration is not runtime compilation or original-NVN
 * descriptor-byte conformance. */
int nn_nvn_register_graphics(NVNdevice *device, const struct nn_nvn_compiled_graphics *compiled);
struct nn_tex_bridge_bundle;
/* Native normal-built artifact lifetime must cover Device. No guest API,
 * shader compilation, public-address change or CPU image conversion. */
int nn_nvn_register_texture_bridge(NVNdevice *,const struct nn_tex_bridge_bundle *);
/* Optional normal-built raw-block copy kernel. Immutable image/code metadata
 * must outlive Device. Public canonical texture storage remains unchanged.
 * This registers an internal producer, not an NVN entry or GPU qualification. */
int nn_nvn_register_block_copy(NVNdevice *,const struct nn_cs_image *);
int nn_nvn_register_volume_copy(NVNdevice *,const struct nn_cs_image *);
/* Optional separately verified finite-float raw constant clear. Internal
 * producer only; registration does not grant Draw/sampler or GPU qualification. */
int nn_nvn_register_clear_color(NVNdevice *,const struct nn_cs_image *);
#ifdef __cplusplus
}
#endif
