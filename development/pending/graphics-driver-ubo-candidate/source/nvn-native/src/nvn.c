#include "nvn_native/nvn.h"
#include "nvn_native/profile.h"
#include "nvn_native/pipeline_nvn.h"
#ifndef NN_NVN_SHADER_SOURCE_CATALOGUE
#define NN_NVN_SHADER_SOURCE_CATALOGUE 0
#endif
#if NN_NVN_SHADER_SOURCE_CATALOGUE != 0 && NN_NVN_SHADER_SOURCE_CATALOGUE != 1
#error "NN_NVN_SHADER_SOURCE_CATALOGUE must be 0 or 1"
#endif
#if NN_NVN_SHADER_SOURCE_CATALOGUE && (!NN_NVN_GRAPHICS_SOURCE_CACHE || !NN_PIPELINE_SOURCE_CATALOGUE)
#error "complete original source binding requires source cache and explicit native source catalogue support"
#endif
#include "nvn_native/pipeline_frontend.h"
#include "nvn_native/pipeline_resource_queries.h"
#include "nvn_native/pipeline_storage.h"
#include "nvn_native/pipeline_runtime_constants.h"
#ifndef NN_NVN_GRAPHICS_RUNTIME_CONSTANTS
#define NN_NVN_GRAPHICS_RUNTIME_CONSTANTS 0
#endif
#ifndef NN_PIPELINE_RUNTIME_CONSTANTS
#define NN_PIPELINE_RUNTIME_CONSTANTS 0
#endif
#if NN_NVN_GRAPHICS_RUNTIME_CONSTANTS && (!NN_PIPELINE_RUNTIME_CONSTANTS || !NN_NVN_GRAPHICS_SOURCE_CACHE)
#error "Public graphics runtime constants need native runtime constants and source cache"
#endif
#ifndef NN_NVN_GRAPHICS_STORAGE
#define NN_NVN_GRAPHICS_STORAGE 0
#endif
#ifndef NN_PIPELINE_GRAPHICS_STORAGE
#define NN_PIPELINE_GRAPHICS_STORAGE 0
#endif
#if NN_NVN_GRAPHICS_STORAGE && (!NN_PIPELINE_GRAPHICS_STORAGE || !NN_NVN_GRAPHICS_SOURCE_CACHE)
#error "Public graphics storage needs actual native storage and source cache"
#endif
#ifndef NN_PIPELINE_SAMPLED_IMAGES
#define NN_PIPELINE_SAMPLED_IMAGES 0
#endif
#ifndef NN_NVN_SAMPLED_IMAGES
#define NN_NVN_SAMPLED_IMAGES 0
#endif
#if NN_NVN_SAMPLED_IMAGES && !NN_PIPELINE_SAMPLED_IMAGES
#error "Public sampled images require native sampled image support"
#endif
#if NN_PIPELINE_SAMPLED_IMAGES
#include "nvn_native/sampled_image.h"
#endif
#if NN_PIPELINE_BC_SAMPLED
#include "nvn_native/texture_bc_shadow.h"
#endif
#include "nvn_native/kgsl.h"
#include "nvn_native/pm4.h"
#include "nvn_native/compute.h"
#include "nvn_native/block_copy.h"
#include "nvn_native/volume_copy.h"
#ifndef NN_NVN_CLEAR_COLOR
#define NN_NVN_CLEAR_COLOR 0
#endif
#if NN_NVN_CLEAR_COLOR != 0 && NN_NVN_CLEAR_COLOR != 1
#error "NN_NVN_CLEAR_COLOR must be explicit0/1"
#endif
#if NN_NVN_CLEAR_COLOR
#include "nvn_native/clear_color.h"
struct object;
static int clear_color_target_admit(const struct object *);
#endif
#include "nvn_native/graphics.h"
#include "nvn_native/render_state.h"
#include "nvn_native/window_session.h"
#include "nvn_native/order.h"
#include "nvn_native/counter.h"
#include "nvn_native/texture_source.h"
#include "nvn_native/texture_bridge.h"
#include "nvn_native/pipeline_views.h"
#ifndef NN_PIPELINE_RUNTIME_VIEWS
#define NN_PIPELINE_RUNTIME_VIEWS 0
#endif
#if NN_PIPELINE_RUNTIME_VIEWS != 0 && NN_PIPELINE_RUNTIME_VIEWS != 1
#error "NN_PIPELINE_RUNTIME_VIEWS must be explicit0/1"
#endif
#ifndef NN_NVN_COLOR_TARGETS
#define NN_NVN_COLOR_TARGETS 0
#endif
#if NN_NVN_COLOR_TARGETS != 0 && NN_NVN_COLOR_TARGETS != 1
#error "NN_NVN_COLOR_TARGETS must be explicit0/1"
#endif
#if NN_NVN_COLOR_TARGETS
#if !NN_PIPELINE_COLOR_TARGETS || !NN_PIPELINE_RUNTIME_VIEWS || !NN_NVN_GRAPHICS_SOURCE_CACHE
#error "Public color targets require actual native color target and runtime FDL views"
#endif
#include "nvn_native/color_target_shadow.h"
#endif
#include "nvn_native/window_display.h"
#include "nvn_native/display_shape.h"
#include "nvn_native/window_probe_access.h"
#include <stdatomic.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include "nvn_native/order.h"

#ifndef NN_NVN_DEPTH_SAMPLED
#define NN_NVN_DEPTH_SAMPLED 0
#endif
#if NN_NVN_DEPTH_SAMPLED
#if !NN_PIPELINE_DEPTH_SAMPLED || !NN_NVN_GRAPHICS_SOURCE_CACHE || !NN_NVN_BLOCK_COPY
#error Noncompare depth sampling requires native role, source cache and block kernel
#endif
#include "nvn_native/depth_sampled_native.h"
#endif
#ifndef NN_NVN_DEPTH_TARGETS
#define NN_NVN_DEPTH_TARGETS 0
#endif
#if NN_NVN_DEPTH_TARGETS
#include "nvn_native/depth_shadow.h"
#include "nvn_native/pipeline_depth_archive.h"
#include "nvn_native/depth_native.h"
#endif
struct public_depth_view {uint32_t level,layer;};
enum native_source_policy { SOURCE_POLICY_GENERIC16BX2_NO_COMPRESSION_V1=1 };
enum { SOURCE_KIND_GENERIC16BX2=0xfe, SOURCE_KIND_ALIGNMENT=512 };
enum kind { DB, DEVICE, QB, QUEUE, PB, POOL, BB, BUFFER, COMMAND, PROGRAM, TB, TEXTURE, SB, SAMPLER, TPOOL, SPOOL, WB, WINDOW, SYNC, EVENT_BUILDER, EVENT };
struct object;
struct pipeline_resource_pins;
static void pipeline_pins_release(struct pipeline_resource_pins *);
struct command_copy;
struct job {struct command_copy *copy_owner;size_t command_offset,command_bytes;unsigned projected_dispatch;struct pipeline_resource_pins *resource_pins; struct job *next; nn_bo *bo; nn_pipeline_program *pipeline_owner; struct nn_sampled_image_shadow *sampled_image_owner; struct nn_depth_shadow *depth_owner; struct nn_depth_sampled_shadow *depth_sampled_owner; unsigned compute, owner_retired; size_t borrowers;
    /* Weak identities kept alive by the handle resource list; never used by reaper. */
    struct object *copy_source,*copy_destination,*copy_destination_pool;
    size_t copy_destination_offset,copy_destination_bytes; };
struct job_borrow { struct job_borrow *next; struct job *job; };
struct queue_config { size_t command, compute, control, flush, bytes; void *memory; unsigned set; };
struct queue_runtime { uint64_t tag, submissions, command_bytes, control_bytes, compute_bytes; int owned; size_t bytes; };
#include "nvn_native/command_sequence.h"
#include "command_sequence.inc"
struct command_chunk { struct command_chunk *next; struct object *pool; size_t offset, bytes; };
struct wait_snapshot { struct object *queue; uint32_t timestamp; };
struct debug_hook { PFNNVNDEBUGCALLBACKPROC callback; void *user; };
struct cache { struct cache *next; const struct nn_nvn_compiled_cs *compiled; };
struct graphics_cache { struct graphics_cache *next; const struct nn_nvn_compiled_graphics *compiled; };
struct bound { struct object *pool; size_t offset, size; uint64_t address; };
struct texture_desc { int target, width, height, depth, levels, samples, format, flags; ptrdiff_t stride; unsigned set;
    unsigned char swizzle_set,swizzle[4],depth_mode; };
/* Zero-initialized private descriptors retain public identity defaults. */
static unsigned texture_swizzle_component(const struct texture_desc *t,unsigned channel)
{return t->swizzle_set?t->swizzle[channel]:NVN_TEXTURE_SWIZZLE_R+channel;}
static int texture_sampling_identity(const struct texture_desc *t)
{
 if(t->depth_mode)return 0;
 for(unsigned i=0;i<4;i++)if(texture_swizzle_component(t,i)!=NVN_TEXTURE_SWIZZLE_R+i)return 0;
 return 1;
}
static int texture_sampling_equal(const struct texture_desc *a,const struct texture_desc *b)
{
 if(a->depth_mode!=b->depth_mode)return 0;
 for(unsigned i=0;i<4;i++)if(texture_swizzle_component(a,i)!=texture_swizzle_component(b,i))return 0;
 return 1;
}
struct vertex_stream { uint32_t stride, divisor; };
struct sampler_desc {
    NVNminFilter min; NVNmagFilter mag; NVNwrapMode wrap[3];
    NVNcompareMode compare; NVNcompareFunc compare_func;
    float lod_min, lod_max, lod_bias, anisotropy, border[4], lod_snap;
    NVNsamplerReduction reduction;
};
/* API pools retain the original 32-byte slot spacing. These are native driver
 * software descriptors, not Maxwell TIC/TSC or Adreno hardware descriptors.
 * The draw frontend validates them and translates into immutable job-local
 * hardware descriptors. No caller pointer or host object address is encoded. */
enum { DESCRIPTOR_BYTES = 32, RESERVED_DESCRIPTORS = 256,
       MAX_TEXTURE_DESCRIPTORS = 1 << 20, MAX_SAMPLER_DESCRIPTORS = 4096 };
struct texture_slot { uint32_t tag, format, width, height, stride, bytes; uint64_t address; };
struct sampler_slot { uint32_t tag, min, mag, wrap, compare; float lod_min, lod_max, lod_bias; };
_Static_assert(sizeof(struct texture_slot) == DESCRIPTOR_BYTES, "texture descriptor slot");
_Static_assert(sizeof(struct sampler_slot) == DESCRIPTOR_BYTES, "sampler descriptor slot");
/* Selected range is metadata over the original source, not a rebased layout.
 * Default/whole ranges keep subset0; native consumers must reject subset1
 * until they explicitly implement the selected dimensions/addressing. */
struct descriptor_selection {
 uint32_t base_level,level_count,base_layer,layer_count;
 uint64_t source_offset;unsigned subset;
};
struct descriptor_entry {
    struct nn_tex_source_layout *source_layout; /* owned immutable registration snapshot */
    struct object *pool; size_t offset, size; unsigned valid;
    struct descriptor_selection selection;
    union { struct texture_desc texture; struct sampler_desc sampler; } snapshot;
};
struct pipeline_record_owner {
 size_t references,count;struct object *pins[96];
};
struct pipeline_draw_snapshot {
 const struct nn_offline_graphics_stage *stages[2];
 const struct nn_pipeline_shader_recipe *recipe;
 struct nn_pipeline_draw_state native;
 struct bound vertices[32],uniforms[2][14],storage[2][16];
 struct nn_pipeline_source_program source;
 struct nn_pipeline_frontend_queries queries;
 struct pipeline_record_owner *owner;
 struct object *texture_pool,*sampler_pool;
 uint64_t separate[2][32],guest[2][32];uint32_t defined[2];
 /* Keep the legacy separate fields for source-only fixture compatibility;
  * all domains below are value snapshots from the same Draw boundary. */
 uint64_t combined[2][32],samplers[2][32],combined_guest[2][32],sampler_guest[2][32];
 uint32_t combined_defined[2],sampler_defined[2];
};
struct late_draw {
    struct late_draw *next; size_t command_offset;
    unsigned generic_pipeline; struct pipeline_draw_snapshot pipeline;
    unsigned bridge_clear,depth_clear; uint32_t clear_word; NVNcopyRegion clear_region;
    struct object *program, *target, *texture_pool, *sampler_pool;
    struct object *depth;struct public_depth_view depth_view;
    struct bound vertex, matrix, fragment;
    struct nn_gfx_draw_state state; /* Copied at this Draw, not enqueue. */
    struct nn_render_state render;
    uint32_t texture_id, sampler_id;
    unsigned draw_override; struct nn_gfx_draw draw;
};
struct counter_op { struct counter_op *next; size_t command_offset; enum nn_counter_operation operation; struct object *destination; size_t destination_offset; };
struct counter_state { nn_bo *bo; unsigned valid,poisoned; };
struct command_dispatch {struct command_dispatch *next;size_t command_offset;uint32_t groups[3];uint64_t indirect;struct job *recorded_job;};
struct handle {
    struct command_dispatch *dispatches;struct command_copy *copies;
    size_t call_references,call_cursor,call_limit;unsigned public_retired,call_nodes;
    struct nn_command_sequence *call_sequence;
    struct handle *next; uint64_t id; struct object *device, *commands;
    size_t offset, bytes, count; struct object **resources;
    /* CPU metadata lifetime refs never enter the KGSL GPU object list. */
    size_t cpu_count; struct object **cpu_resources;
    struct job *jobs; struct late_draw *draws; struct counter_op *counter_ops;
    struct job_borrow *borrowed_jobs;
    struct command_chunk *chunks, *chunks_tail;
    struct wait_snapshot waits[16]; unsigned wait_count;
};
struct control { uint64_t tag, handle; size_t bytes; };
/* Exact last public handle argument is opaque CPU metadata. The separate high
 * word tracks SDK plural writes; a scalar write leaves that word unchanged.
 * Unknown initial high words stay undefined. No GPU CB2 upload is implied. */
struct texture_handle_record {
    uint64_t guest[2][32];
    uint32_t high[2][32], low_defined[2], high_defined[2], last_plural[2];
};
struct recording {
    struct nn_render_state render;
    ptrdiff_t copy_row_stride, copy_image_stride;unsigned copy_recording;
    struct object *pool; size_t offset, capacity, used, start;
    unsigned char *control; size_t control_capacity, control_used;
    struct handle *current; int active;
    PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC memory_callback;
    void *memory_callback_user; unsigned in_callback;
    size_t logical_bytes;
    uint32_t debug_domains[32]; unsigned debug_depth;
    struct object *program;
    struct bound ubos[16], ssbos[16];
    NVNimageHandle images[6][32];
    struct bound shader_scratch;
    struct object *graphics_program, *target;
    struct object *depth;struct public_depth_view depth_view;
    struct bound graphics_ubos[2][14],graphics_storage[2][16], vertex;
    uint32_t attributes[32]; struct vertex_stream stream;
    struct vertex_stream vertex_streams[32]; struct bound vertices[32];
    unsigned vertex_stream_count; int viewport[4],scissor[4]; float depth_range[2];
    struct object *texture_pool, *sampler_pool;
    /* Native descriptor lookup consumes only these normalized32-bit payloads.
     * Full guest identity/high-word provenance is kept separately, never used
     * as a descriptor index or GPUVA. Explicit unknown CB2 reads still reject. */
    uint64_t combined[2][32], separate_textures[2][32], separate_samplers[2][32];
    struct texture_handle_record handle_carriers[3];
    unsigned attributes_set, stream_set, viewport_set, scissor_set, depth_range_set;
};
struct window_builder_state {
    NVNnativeWindow native;
    NVNtexture *const *textures; /* borrowed through Initialize, SDK477620 */
    int count, interval;
};
struct window_state {
    nn_window_session *session;
    struct object *textures[3], *producer;
    struct nn_window_ticket tickets[3];
    unsigned pinned[3], count;
    unsigned source_display[3];
    nn_window_display *display[3];
    struct nn_external_image_view display_views[3];
    struct nn_tex_conversion conversions[3];
    nn_queue *conversion_queue;
    unsigned conversion_pending;
    uint32_t width,height;
    int closing;
};

static int counter_clone_metadata(struct handle *,const struct handle *,size_t);
static int counter_queue_release(struct object *);
static void counter_free_ops(struct counter_op *);
struct object {
    struct object *next; const void *key; enum kind kind;
    struct object *device, *pool;
    struct nn_command_state *call_queue_state;unsigned call_queue_active,call_queue_poisoned;
    /* Public builder keys are borrowed opaque identities only. Internal weak
     * nodes are cleared before owner destruction; keys are never dereferenced. */
    const NVNdevice *builder_device_key;
    const NVNmemoryPool *builder_pool_key;
    nn_device *gpu; nn_queue *queue; nn_bo *bo, *fs_bo;
    void *storage; size_t offset, size, refs; int flags;
    /* Immutable device-owned explicit source-kind policy; zero is invalid. */
    enum native_source_policy texture_source_policy;
    /* Non-NULL identifies canonical GOB source bytes, never native LINEAR. */
    struct nn_tex_source_layout *source_layout;
    const struct nn_tex_bridge_bundle *texture_bridge; /* device lifetime normal checked artifact */
    const struct nn_cs_image *block_copy; /* normal checked immutable raw-block kernel */
    const struct nn_cs_image *volume_copy; /* separate true3D GOB/Z shader ABI */
    const struct nn_cs_image *clear_color; /* optional checked one-texel fill CS */
    enum nn_nvn_gpu_pool_policy gpu_policy; /* GPU_UNCACHED CPU-only subset. */
    struct recording *record; struct counter_state *counter;
    struct cache *cache;
    const struct nn_nvn_compiled_cs *compiled;
    struct graphics_cache *graphics_cache;
    const struct nn_pipeline_archive *pipeline_archive;
    struct nn_shader_source_catalogue *shader_sources;
    const struct nn_pipeline_depth_archive *pipeline_depth;
    const struct nn_pipeline_storage_archive *pipeline_storage;
    const struct nn_pipeline_runtime_archive *pipeline_runtime;
    const struct nn_pipeline_typed_archive *pipeline_types; /* immutable normal actual-NIR facts */
    const struct nn_tex_native_view *pipeline_views; size_t pipeline_view_count; /* immutable Device catalogue */
    struct nn_pipeline_views *pipeline_runtime_views; /* owned CPU metadata, no eviction */
    nn_nvn_pipeline_input_reader pipeline_reader;void *pipeline_reader_user;
    struct nn_pipeline_source_program pipeline_source; /* Program identity only */
    const struct nn_nvn_compiled_graphics *graphics;
    struct texture_desc texture;
    unsigned texture_packaged; unsigned char packaged_layout[8];
    struct sampler_desc sampler;
    const NVNdevice *sampler_device_key; /* SB public weak key; never dereferenced. */
    struct descriptor_entry *slots; size_t slot_count;
    unsigned cpu_software_table; /* Only strict descriptor Initialize sets this role. */
    struct job *retired;
    nn_external_image *external; /* explicit host-only allocator owner */
    struct window_builder_state window_builder;
    struct window_state *window;
    struct object *window_owner; /* weak TEXTURE -> live Window while pinned */
    unsigned window_index, availability;
    struct queue_config queue_config;
    struct queue_runtime *queue_state;
    struct wait_snapshot waits[16]; unsigned wait_count;
    struct wait_snapshot fence; unsigned fenced;
    struct debug_hook debug_hooks[8];
    struct nn_gfx_draw_state graphics_state; /* Device-owned enum state. */
};
static struct object *objects;
static struct handle *handles;
static uint64_t next_handle = 1;
static _Thread_local int last_error;
static _Thread_local struct nn_nvn_pool_access_observation pool_access_observation;
int nn_nvn_error(void) { return last_error; }
void nn_nvn_clear_error(void)
{ last_error = 0; pool_access_observation = (struct nn_nvn_pool_access_observation){0}; }
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out)
{
    if (!out) return EINVAL;
    if (!pool_access_observation.consumer) return ENODATA;
    *out = pool_access_observation; return 0;
}
static _Thread_local struct object *debug_device;
static _Thread_local int debug_callback_active;
static void error(int e)
{
    if (!e || last_error) return;
    last_error=e;
    if (debug_callback_active) return;
    struct object *device=NULL;
    for(struct object *p=objects;p;p=p->next)if(p==debug_device&&p->kind==DEVICE){device=p;break;}
    if (!device) return;
    struct debug_hook callbacks[8];memcpy(callbacks,device->debug_hooks,sizeof(callbacks));
    debug_callback_active=1;
    for(unsigned i=0;i<8;i++)if(callbacks[i].callback)
        callbacks[i].callback(NVN_DEBUG_CALLBACK_SOURCE_API,NVN_DEBUG_CALLBACK_TYPE_API_ERROR,e,
            NVN_DEBUG_CALLBACK_SEVERITY_HIGH,(const unsigned char *)strerror(e),callbacks[i].user);
    debug_callback_active=0;
}
/* No GPU cache policy is inferred from CPU cachemode or IOCOHERENT. Exact
 * 0x14 currently implements real mapping/CPU/lifetime operations only. Every
 * potential consumer rejects before bytes, descriptors or refs are changed. */
static int gpu_pool_access(struct object *pool, size_t offset, size_t size, const char *consumer)
{
    if (!pool || pool->gpu_policy != NN_NVN_GPU_UNCACHED_UNQUALIFIED) return 0;
    if (!last_error) pool_access_observation = (struct nn_nvn_pool_access_observation){
        .pool = pool->key, .flags = pool->flags, .policy = pool->gpu_policy,
        .offset = offset, .size = size, .consumer = consumer};
    error(ENOTSUP); return ENOTSUP;
}
static int gpu_object_access(struct object *o, const char *consumer)
{
    if (!o) return 0;
    if (o->kind == TEXTURE && o->texture.target==NVN_TEXTURE_TARGET_BUFFER) { error(ENOTSUP);return ENOTSUP; }
    if (o->kind == TEXTURE && o->source_layout &&
        (!o->device->texture_bridge || nn_tex_bridge_source_validate(o->source_layout))) { error(ENOTSUP); return ENOTSUP; }
    return o->kind == POOL ? gpu_pool_access(o,0,o->size,consumer) :
           gpu_pool_access(o->pool,o->offset,o->size,consumer);
}
/* Typed color admission is performed by pipeline_target_admit/capture. This
 * target-only policy check must not apply the legacy RGBA8 sampled bridge gate
 * to an already typed MRT. It never relaxes the underlying pool GPU policy. */
static int gpu_render_target_access(struct object *o, const char *consumer)
{
#if NN_NVN_COLOR_TARGETS
    if(o && o->kind==TEXTURE && o->device->pipeline_archive)
        return gpu_pool_access(o->pool,o->offset,o->size,consumer);
#endif
    return gpu_object_access(o,consumer);
}
/* A role of the descriptor object, never a GPU-access exemption on its pool.
 * These native 32B slots are consumed only by CPU registration/materialization;
 * the encoder allocates separate immutable hardware tables in a private job. */
static int cpu_descriptor_access(const struct object *o)
{
    if (!o) return 0; /* NULL descriptor binding is a valid unbind. */
    if ((o->kind != TPOOL && o->kind != SPOOL) || !o->cpu_software_table ||
        !o->pool || o->pool->kind != POOL || o->pool->external ||
        (o->pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) ||
        !o->pool->bo || !nn_bo_map(o->pool->bo) || !o->slots ||
        o->slot_count <= RESERVED_DESCRIPTORS ||
        o->slot_count > (o->kind == TPOOL ? MAX_TEXTURE_DESCRIPTORS : MAX_SAMPLER_DESCRIPTORS) ||
        o->size != o->slot_count * DESCRIPTOR_BYTES ||
        o->offset > o->pool->size || o->size > o->pool->size-o->offset ||
        (nn_bo_address(o->pool->bo)+o->offset) % DESCRIPTOR_BYTES ||
        o->device != o->pool->device) { error(ENOTSUP); return ENOTSUP; }
    return 0;
}
static int gpu_bound_access(const struct bound *b, const char *consumer)
{ return gpu_pool_access(b->pool,b->offset,b->size,consumer); }
static int gpu_recording_access(const struct recording *r, const char *consumer)
{
    if (gpu_pool_access(r->pool,r->offset,r->capacity,consumer) ||
        gpu_object_access(r->target,consumer) || gpu_bound_access(&r->vertex,consumer) ||
        cpu_descriptor_access(r->texture_pool) || cpu_descriptor_access(r->sampler_pool)) return ENOTSUP;
    for (unsigned i=0;i<16;i++)
        if (gpu_bound_access(r->ubos+i,consumer) || gpu_bound_access(r->ssbos+i,consumer)) return ENOTSUP;
    for (unsigned stage=0;stage<2;stage++) for (unsigned i=0;i<14;i++)
        if (gpu_bound_access(&r->graphics_ubos[stage][i],consumer)) return ENOTSUP;
    return 0;
}
static int gpu_late_draw_access(const struct late_draw *d, const char *consumer)
{
    if(d->generic_pipeline) {
        if(gpu_render_target_access(d->target,consumer)||(d->depth&&gpu_pool_access(d->depth->pool,d->depth->offset,d->depth->size,consumer)))return ENOTSUP;
        /* Potential vertex/UBO owners are CPU metadata pins. Only the enqueue
         * recipe determines actual consumed GPU ranges. */
        return 0;
    }
    if (gpu_object_access(d->target,consumer) || gpu_bound_access(&d->vertex,consumer) ||
        gpu_bound_access(&d->matrix,consumer) || gpu_bound_access(&d->fragment,consumer) ||
        cpu_descriptor_access(d->texture_pool) || cpu_descriptor_access(d->sampler_pool)) return ENOTSUP;
    if (d->texture_pool && d->texture_id < d->texture_pool->slot_count) {
        const struct descriptor_entry *t=d->texture_pool->slots+d->texture_id;
        if(t->valid && (t->selection.subset || !texture_sampling_identity(&t->snapshot.texture) || t->snapshot.texture.target==NVN_TEXTURE_TARGET_BUFFER)){error(ENOTSUP);return ENOTSUP;}
        if (gpu_pool_access(t->pool,t->offset,t->size,consumer)) return ENOTSUP;
    }
    return 0;
}
static struct object *find(const void *key, enum kind kind)
{
    if(debug_callback_active){error(EDEADLK);return NULL;}
    for (struct object *o = objects; o; o = o->next) if (o->key == key && o->kind == kind) {
        if (kind == QUEUE && !o->queue) { error(EBUSY);return NULL; }
        if (kind == POOL && o->external && !nn_external_image_live(o->external)) { error(EBUSY); return NULL; }
        debug_device=kind==DEVICE?o:o->device;
        return o;
    }
    error(EINVAL); return NULL;
}
static int builder_kind(enum kind kind);
static void remove_object(struct object *o);
/* Builders have no public Finalize and carry only borrowed metadata. An
 * explicit resource Initialize may begin a new opaque lifetime at their old
 * key. This cannot establish validity for simultaneously live overlapping
 * C/C++ objects; live owners and any strong reference still refuse reuse. */
static int creation_key_error(const void *key,enum kind kind)
{
    if(debug_callback_active)return EDEADLK;
    if(!key)return EINVAL;
    for(struct object *o=objects;o;o=o->next)if(o->key==key){
        if(builder_kind(kind)||!builder_kind(o->kind)||o->refs)return EEXIST;
        break;
    }
    return 0;
}
static struct object *create(const void *key, enum kind kind)
{
    int key_error=creation_key_error(key,kind);if(key_error){error(key_error);return NULL;}
    struct object *o = calloc(1, sizeof(*o)); if (!o) { error(ENOMEM); return NULL; }
    /* Allocate first: allocation failure preserves the old builder. Each
     * builder-taking constructor copies all input used after this point. */
    for(struct object *old=objects;old;old=old->next)if(old->key==key){remove_object(old);break;}
    o->key = key; o->kind = kind; o->next = objects; objects = o; return o;
}
static void remove_object(struct object *o)
{
    /* Only builders carry weak references. Clear internal nodes before free,
     * preventing a new owner allocated at the same address from reviving one.
     * Public getter identities remain stable and do not pin an owner. */
    for(struct object *b=objects;b;b=b->next)if(builder_kind(b->kind)){
        if(b->device==o)b->device=NULL;
        if(b->pool==o)b->pool=NULL;
    }
    /* Queue continuation bindings are weak metadata, not a new resource
     * lifetime guarantee. Clear before free so allocator ABA cannot revive it. */
    for(struct object *q=objects;q;q=q->next)if(q->call_queue_state)
      for(unsigned i=0;i<NN_COMMAND_BINDING_COUNT;i++)if(q->call_queue_state->bindings[i].owner==o){
       memset(q->call_queue_state->bindings+i,0,sizeof(q->call_queue_state->bindings[i]));
       q->call_queue_state->binding_defined[i/64]&=~(UINT64_C(1)<<(i%64));
      }
    free(o->call_queue_state);
    struct object **link = &objects; while (*link != o) link = &(*link)->next;
    *link = o->next; free(o);
}
static int builder_kind(enum kind kind)
{ return kind==DB || kind==QB || kind==PB || kind==BB || kind==TB || kind==SB || kind==WB || kind==EVENT_BUILDER; }
static struct object *builder(const void *key, enum kind kind)
{
    if(debug_callback_active){error(EDEADLK);return NULL;}
    for (struct object *o=objects;o;o=o->next) if(o->key==key) {
        /* Defaults begins a new opaque-builder lifetime. C/C++ callers may
         * reuse the same stack storage for different builder types. Never
         * reclaim resource/command/program owners by mistaking them for one. */
        if(!builder_kind(o->kind) || o->refs) { error(EEXIST);return NULL; }
        if(o->kind!=kind) {
            struct object *next=o->next;
            *o=(struct object){.next=next,.key=key,.kind=kind};
        } else {
            o->device=NULL;o->pool=NULL;o->builder_device_key=NULL;o->builder_pool_key=NULL;
            o->storage=NULL;o->offset=o->size=0;o->flags=0;
        }
        return o;
    }
    return create(key,kind);
}
/* Original SDK value builders do not own resources. Defaults/SetDevice may
 * start a new builder lifetime at reused stack storage, while same-kind
 * metadata and its weak device survive until the API overwrites them. */
static struct object *value_builder(const void *key,enum kind kind)
{
    if(debug_callback_active){error(EDEADLK);return NULL;}
    for(struct object *o=objects;o;o=o->next)
        if(o->key==key && o->kind==kind)return o;
    return builder(key,kind); /* refuses live resource owners and strong refs */
}
static struct object *live_weak(struct object *weak,enum kind kind)
{ for(struct object *p=objects;p;p=p->next)if(p==weak&&p->kind==kind)return p;return NULL; }
static void reap_jobs(struct object *device)
{
    struct job **link = &device->retired;
    while (*link) {
        struct job *job = *link;
        /* Copied command streams retain original job GPUVAs. A retired owner
         * cannot destroy its private job while any independent copy needs it. */
        if(job->borrowers){link=&job->next;continue;}
#if NN_NVN_GRAPHICS_SOURCE_CACHE
        int e;
#if NN_NVN_DEPTH_TARGETS
        if(job->depth_owner){e=nn_depth_shadow_destroy(job->depth_owner);if(!e){free(job->depth_owner);job->depth_owner=NULL;}}
        else
#endif
#if NN_PIPELINE_SAMPLED_IMAGES
        if(job->sampled_image_owner)e=nn_sampled_image_shadow_destroy(&job->sampled_image_owner);
        else
#endif
#if NN_NVN_DEPTH_SAMPLED
        if(job->depth_sampled_owner){e=nn_depth_sampled_destroy(job->depth_sampled_owner);if(!e){free(job->depth_sampled_owner);job->depth_sampled_owner=NULL;}}
        else
#endif
        e = job->pipeline_owner ? nn_pipeline_program_destroy(job->pipeline_owner) : nn_bo_destroy(job->bo);
#else
        int e=nn_bo_destroy(job->bo);
#endif
        if (e == EBUSY) { link = &job->next; continue; }
        if (e) { error(e); return; }
        *link = job->next; pipeline_pins_release(job->resource_pins); free(job); device->refs--;
    }
}
static void retire_jobs(struct object *device, struct job *jobs)
{
    while (jobs) {
        struct job *job = jobs; jobs = job->next;
        job->owner_retired=1;
        job->next = device->retired; device->retired = job;
    }
    reap_jobs(device);
}
static void api_nvnDeviceBuilderSetDefaults(NVNdeviceBuilder *b) { builder(b, DB); }
static int device_flags_supported(int flags)
{
    return flags == 0 || flags == NVN_DEVICE_FLAG_ENABLE_SEPARATE_SAMPLER_TEXTURE_SUPPORT;
}
static void api_nvnDeviceBuilderSetFlags(NVNdeviceBuilder *b, int flags)
{
    struct object *o = find(b, DB); if (!o) return;
    if (!device_flags_supported(flags)) { error(ENOTSUP); return; }
    o->flags = flags;
}
static PFNNVNGENERICFUNCPTRPROC api_nvnDeviceGetProcAddress(const NVNdevice *d, const char *name)
{ (void)d; return nvnBootstrapLoader(name); }
static NVNboolean api_nvnDeviceInitialize(NVNdevice *key, const NVNdeviceBuilder *b)
{
    struct object *v = find(b, DB); if (!v) return 0;
    if (!device_flags_supported(v->flags)) { error(ENOTSUP); return 0; }
    const int flags=v->flags;
    struct object *o = create(key, DEVICE); if (!o) return 0;
    o->flags = flags;
    /* V1 chooses Generic16Bx2 source kind, with NVIDIA compression disabled.
     * This is a project device policy, not measured Tegra capabilities. */
    o->texture_source_policy = SOURCE_POLICY_GENERIC16BX2_NO_COMPRESSION_V1;
    /* Project profile defaults; independent of builder flags and later reuse. */
    o->graphics_state = (struct nn_gfx_draw_state){
        .window_origin=NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT, .depth_mode=NVN_DEPTH_MODE_NEAR_IS_MINUS_W};
    int e = nn_device_open(&o->gpu); if (e) { error(e); remove_object(o); return 0; } return 1;
}
static void api_nvnDeviceSetWindowOriginMode(NVNdevice *key, NVNwindowOriginMode mode)
{
    struct object *o = find(key, DEVICE); if (!o) return;
    if (mode != NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT && mode != NVN_WINDOW_ORIGIN_MODE_UPPER_LEFT) {
        error(ENOTSUP); return;
    }
    o->graphics_state.window_origin = (uint32_t)mode;
}
static void api_nvnDeviceSetDepthMode(NVNdevice *key, NVNdepthMode mode)
{
    struct object *o = find(key, DEVICE); if (!o) return;
    if (mode != NVN_DEPTH_MODE_NEAR_IS_MINUS_W && mode != NVN_DEPTH_MODE_NEAR_IS_ZERO) {
        error(ENOTSUP); return;
    }
    o->graphics_state.depth_mode = (uint32_t)mode;
}
static NVNwindowOriginMode api_nvnDeviceGetWindowOriginMode(const NVNdevice *key)
{
    struct object *o = find(key, DEVICE);
    return o ? (NVNwindowOriginMode)o->graphics_state.window_origin : NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT;
}
static NVNdepthMode api_nvnDeviceGetDepthMode(const NVNdevice *key)
{
    struct object *o = find(key, DEVICE);
    return o ? (NVNdepthMode)o->graphics_state.depth_mode : NVN_DEPTH_MODE_NEAR_IS_MINUS_W;
}
static void api_nvnDeviceInstallDebugCallback(NVNdevice *key,const PFNNVNDEBUGCALLBACKPROC callback,void *user,NVNboolean enable)
{
    struct object *d=find(key,DEVICE);if(!d)return;
    if(!callback || (enable!=0&&enable!=1)){error(EINVAL);return;}
    for(unsigned i=0;i<8;i++)if(d->debug_hooks[i].callback==callback&&d->debug_hooks[i].user==user){
        if(!enable)d->debug_hooks[i]=(struct debug_hook){0};
        return;
    }
    if(!enable)return;
    for(unsigned i=0;i<8;i++)if(!d->debug_hooks[i].callback){d->debug_hooks[i]=(struct debug_hook){callback,user};return;}
    error(ENOSPC);
}
static void api_nvnDeviceFinalize(NVNdevice *key)
{
    struct object *o = find(key, DEVICE); if (!o) return;
    reap_jobs(o);
    if (o->refs) { error(EBUSY); return; }
    int e = nn_device_close(o->gpu); if (e) error(e); else {
        while (o->cache) { struct cache *c = o->cache; o->cache = c->next; free(c); }
        while (o->graphics_cache) { struct graphics_cache *c = o->graphics_cache; o->graphics_cache = c->next; free(c); }
#if NN_PIPELINE_RUNTIME_VIEWS
        nn_pipeline_views_destroy(o->pipeline_runtime_views);
#endif
#if NN_NVN_SHADER_SOURCE_CATALOGUE
        nn_shader_source_catalogue_destroy(o->shader_sources);
#endif
        remove_object(o);
    }
}
int nn_nvn_register_texture_bridge(NVNdevice *key,const struct nn_tex_bridge_bundle *bundle)
{
    struct object *device=find(key,DEVICE);if(!device)return nn_nvn_error();
    int e=nn_tex_bridge_bundle_validate(bundle);if(e)return e;
    if(bundle->chip!=nn_device_chip(device->gpu))return ENOTSUP;
    if(device->texture_bridge && device->texture_bridge!=bundle)return EBUSY;
    device->texture_bridge=bundle;return 0;
}
static int register_raw_copy(NVNdevice *key,const struct nn_cs_image *image,int volume)
{
#if NN_NVN_BLOCK_COPY
    struct object *d=find(key,DEVICE);if(!d)return nn_nvn_error();
    if(!image||image->schema!=1||image->chip!=0x06050002||image->chip!=nn_device_chip(d->gpu)||
       image->local[0]!=8||image->local[1]!=8||image->local[2]!=1||image->ssbo_slots!=2||
       !image->code||!image->code_bytes||image->code_bytes>65536||!image->commands||
       !image->command_words||image->command_words>1024||!image->patches||!image->patch_count)return ENOTSUP;
    const struct nn_cs_image **slot=volume?&d->volume_copy:&d->block_copy;
    if(*slot&&*slot!=image)return EBUSY;
    *slot=image;return 0;
#else
    (void)key;(void)image;(void)volume;return ENOTSUP;
#endif
}
int nn_nvn_register_block_copy(NVNdevice *key,const struct nn_cs_image *image)
{return register_raw_copy(key,image,0);}
int nn_nvn_register_volume_copy(NVNdevice *key,const struct nn_cs_image *image)
{return register_raw_copy(key,image,1);}
int nn_nvn_register_clear_color(NVNdevice *key,const struct nn_cs_image *image)
{
#if NN_NVN_CLEAR_COLOR
    struct object *d=find(key,DEVICE);if(!d)return nn_nvn_error();
    if(!image||image->schema!=1||image->chip!=0x06050002||image->chip!=nn_device_chip(d->gpu)||
       image->local[0]!=8||image->local[1]!=8||image->local[2]!=1||image->ssbo_slots!=1||
       !image->code||!image->code_bytes||image->code_bytes>65536||!image->commands||
       !image->command_words||image->command_words>1024||!image->patches||!image->patch_count)return ENOTSUP;
    if(d->clear_color&&d->clear_color!=image)return EBUSY;
    d->clear_color=image;return 0;
#else
    (void)key;(void)image;return ENOTSUP;
#endif
}
static void api_nvnQueueBuilderSetDefaults(NVNqueueBuilder *b)
{
    struct object *old=NULL;
    for(struct object *p=objects;p;p=p->next)if(p->key==b&&p->kind==QB){old=p;break;}
    struct object *device=old?old->device:NULL;
    const NVNdevice *device_key=old?old->builder_device_key:NULL;
    struct object *v=builder(b,QB);if(!v)return;
    v->device=device;v->builder_device_key=device_key;
    v->queue_config=(struct queue_config){.command=65536,.compute=262144,.control=16384,.flush=4096};
}
static void api_nvnQueueBuilderSetDevice(NVNqueueBuilder *b,NVNdevice *d)
{
    struct object*device=find(d,DEVICE);if(!device)return;
    struct object*o=NULL;
    for(struct object*p=objects;p;p=p->next)if(p->key==b&&p->kind==QB){o=p;break;}
    if(!o){o=builder(b,QB);if(!o)return;
        o->queue_config=(struct queue_config){.command=65536,.compute=262144,.control=16384,.flush=4096};}
    o->device=device;o->builder_device_key=d;
}
static void api_nvnQueueBuilderSetFlags(NVNqueueBuilder *b,int flags)
{ struct object *o=find(b,QB);if(!o)return;if(flags&~3){error(ENOTSUP);return;}o->flags=flags; }
static void queue_budget(NVNqueueBuilder *b,size_t value,unsigned which)
{
    struct object *o=find(b,QB);if(!o)return;
    size_t minimum=which==1?256:which==2?0:256;
    if(value<minimum || value>INT_MAX || value%256){error(EINVAL);return;}
    if(which==1)o->queue_config.command=value;
    else if(which==2)o->queue_config.compute=value;
    else o->queue_config.control=value;
    o->queue_config.set|=which;
}
static void api_nvnQueueBuilderSetCommandMemorySize(NVNqueueBuilder*b,size_t n){queue_budget(b,n,1);}
static void api_nvnQueueBuilderSetComputeMemorySize(NVNqueueBuilder*b,size_t n){queue_budget(b,n,2);}
static void api_nvnQueueBuilderSetControlMemorySize(NVNqueueBuilder*b,size_t n){queue_budget(b,n,4);}
static void api_nvnQueueBuilderSetCommandFlushThreshold(NVNqueueBuilder*b,size_t n)
{struct object*o=find(b,QB);if(!o)return;if(!n||n>INT_MAX){error(EINVAL);return;}o->queue_config.flush=n;o->queue_config.set|=8;}
static size_t queue_memory_bytes(const struct queue_config *c)
{
    size_t n=4096;
    if(c->command>SIZE_MAX-n)return 0;
    n+=c->command;
    if(c->compute>SIZE_MAX-n)return 0;
    n+=c->compute;
    if(n>SIZE_MAX-4095 || c->control>SIZE_MAX-n-4095)return 0;
    n+=c->control;
    return (n+4095)&~(size_t)4095;
}
static size_t api_nvnQueueBuilderGetQueueMemorySize(const NVNqueueBuilder*b)
{
    struct object *o=find(b,QB);if(!o)return 0;
    if(!live_weak(o->device,DEVICE)){error(EINVAL);return 0;}
    size_t n=queue_memory_bytes(&o->queue_config);if(!n)error(EOVERFLOW);return n;
}
static void api_nvnQueueBuilderSetQueueMemory(NVNqueueBuilder*b,void*memory,size_t bytes)
{
    struct object*o=find(b,QB);if(!o)return;
    if((!memory)!=(!bytes) || (memory&&((uintptr_t)memory%4096||bytes%4096||bytes>INT_MAX))){error(EINVAL);return;}
    o->queue_config.memory=memory;o->queue_config.bytes=bytes;
}
static NVNboolean api_nvnQueueInitialize(NVNqueue*key,const NVNqueueBuilder*b)
{
    struct object*v=find(b,QB);if(!v)return 0;
    struct object*device=live_weak(v->device,DEVICE);
    if(!device){error(EINVAL);return 0;}
    if(v->flags&~3 || !v->queue_config.command || !v->queue_config.control ||
        !v->queue_config.flush || v->queue_config.flush>v->queue_config.command){error(EINVAL);return 0;}
    size_t bytes=queue_memory_bytes(&v->queue_config);if(!bytes){error(EOVERFLOW);return 0;}
    void*memory=v->queue_config.memory;int owned=!memory;
    if(memory && v->queue_config.bytes<bytes){error(ENOSPC);return 0;}
    if(memory)for(struct object*p=objects;p;p=p->next)if(p->kind==QUEUE&&p->queue_state){
        uintptr_t a=(uintptr_t)memory,z=(uintptr_t)p->queue_state;
        if(a<=z?z-a<bytes:a-z<p->queue_state->bytes){error(EBUSY);return 0;}
    }
    if(owned){memory=aligned_alloc(4096,bytes);if(!memory){error(ENOMEM);return 0;}}
    const struct queue_config config=v->queue_config;const int flags=v->flags;
    struct object*o=create(key,QUEUE);if(!o){if(owned)free(memory);return 0;}
    int e=nn_queue_create(device->gpu,&o->queue);
    if(e){if(owned)free(memory);remove_object(o);error(e);return 0;}
    memset(memory,0,bytes);o->queue_state=memory;
    *o->queue_state=(struct queue_runtime){.tag=UINT64_C(0x51554555454e4154),.owned=owned,.bytes=bytes};
    o->queue_config=config;o->flags=flags;
    o->device=device;device->refs++;return 1;
}
static void api_nvnQueueFlush(NVNqueue*key)
{struct object*o=find(key,QUEUE);if(o)error(nn_queue_flush(o->queue));}
static NVNqueueGetErrorResult api_nvnQueueGetError(NVNqueue*key,NVNqueueErrorInfo*info)
{
    struct object*o=find(key,QUEUE);if(!o)return NVN_QUEUE_GET_ERROR_RESULT_GPU_ERROR_UNKNOWN;
    int faults=0,e=nn_queue_faults(o->queue,&faults);
    if(e){error(e);return NVN_QUEUE_GET_ERROR_RESULT_GPU_ERROR_UNKNOWN;}
    if(!faults)return NVN_QUEUE_GET_ERROR_RESULT_GPU_NO_ERROR;
    /* The public reconstructed error-info type is explicitly TODO/dummy[1].
     * Do not write a fictional native/Maxwell layout into guest storage. */
    if(info)error(ENOTSUP);
    return NVN_QUEUE_GET_ERROR_RESULT_GPU_ERROR_UNKNOWN;
}
static void api_nvnQueueFinish(NVNqueue*key)
{struct object*o=find(key,QUEUE);if(o){int e=nn_queue_finish(o->queue,1000);if(e&&o->counter)o->counter->poisoned=1;error(e);reap_jobs(o->device);}}
static void api_nvnQueueFinalize(NVNqueue*key)
{
    struct object*o=find(key,QUEUE);if(!o)return;
    if(o->refs){error(EBUSY);return;}
    int e=nn_queue_destroy(o->queue);if(e){error(e);return;}
    o->queue=NULL;e=counter_queue_release(o);if(e){error(e);return;}
    for(unsigned i=0;i<o->wait_count;i++)o->waits[i].queue->refs--;
    if(o->queue_state->owned)free(o->queue_state);
    o->device->refs--;remove_object(o);
}
static void api_nvnMemoryPoolBuilderSetDefaults(NVNmemoryPoolBuilder *b)
{
    struct object *old = NULL;
    for (struct object *o = objects; o; o = o->next) if (o->key == b && o->kind == PB) { old = o; break; }
    struct object *device = old ? old->device : NULL;
    const NVNdevice *device_key=old?old->builder_device_key:NULL;
    struct object *o = builder(b, PB); if (!o) return;
    /* Original SDK defaults preserve the device and choose CPU_NO_ACCESS. */
    o->device = device;o->builder_device_key=device_key;
    o->flags = NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED;
}
static void api_nvnMemoryPoolBuilderSetDevice(NVNmemoryPoolBuilder *b, NVNdevice *d)
{ struct object *o=find(b,PB),*device=find(d,DEVICE);if(o&&device){o->device=device;o->builder_device_key=d;} }
static void api_nvnMemoryPoolBuilderSetStorage(NVNmemoryPoolBuilder *b, void *memory, size_t size)
{ struct object *o = find(b, PB); if (o) { o->storage = memory; o->size = size; } }
static void api_nvnMemoryPoolBuilderSetFlags(NVNmemoryPoolBuilder *b, int flags)
{ struct object *o = find(b, PB); if (o) o->flags = flags; }
static NVNboolean api_nvnMemoryPoolInitialize(NVNmemoryPool *key, const NVNmemoryPoolBuilder *b)
{
    struct object *v = find(b, PB); if (!v) return 0;
    /* Builder device is a weak reference; do not dereference a finalized
     * device retained by SetDefaults. Compare against live object nodes first. */
    struct object *device = NULL;
    for (struct object *p = objects; p; p = p->next)
        if (p == v->device && p->kind == DEVICE) { device = p; break; }
    if (!device) { error(EINVAL); return 0; }
    const int modifiers = NVN_MEMORY_POOL_FLAGS_SHADER_CODE | NVN_MEMORY_POOL_FLAGS_COMPRESSIBLE;
    int access = v->flags & ~modifiers;
    int unqualified = v->flags == (NVN_MEMORY_POOL_FLAGS_CPU_CACHED | NVN_MEMORY_POOL_FLAGS_GPU_UNCACHED) ||
                      v->flags == (NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_UNCACHED);
    if (!unqualified && access != (NVN_MEMORY_POOL_FLAGS_CPU_CACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED) &&
        access != (NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED) &&
        access != (NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED)) {
        error(ENOTSUP); return 0;
    }
    int key_error=creation_key_error(key,POOL);if(key_error){error(key_error);return 0;}
    /* Refuse known same-storage aliases involving an unqualified policy:
     * a second cached pool must not bypass the first-use gates. This checks
     * caller virtual ranges, not unrelated physical aliases. */
    uintptr_t start=(uintptr_t)v->storage;
#if defined(__aarch64__)
    start &= UINT64_C(0x00ffffffffffffff);
#endif
    if (unqualified && (!start || !v->size || start % nn_device_page(device->gpu) ||
        v->size % nn_device_page(device->gpu) || v->size > UINTPTR_MAX-start)) {
        error(EINVAL); return 0;
    }
    for (struct object *p=objects;p;p=p->next) if (p->kind == POOL && p->storage &&
        (unqualified || p->gpu_policy == NN_NVN_GPU_UNCACHED_UNQUALIFIED)) {
        uintptr_t previous=(uintptr_t)p->storage;
#if defined(__aarch64__)
        previous &= UINT64_C(0x00ffffffffffffff);
#endif
        if (start <= previous ? previous-start < v->size : start-previous < p->size) {
            error(ENOTSUP); return 0;
        }
    }
    /* Native-uncompressed policy: retain COMPRESSIBLE eligibility in flags,
     * using these same imported pages. This is not Maxwell compressed-byte
     * compatibility; each texture's strict layout/format qualification still
     * applies. Never turn this into a shadow allocation or a flag mask. */
    struct job *rollback = calloc(1, sizeof(*rollback));
    if (!rollback) { error(ENOMEM); return 0; }
    nn_bo *bo = NULL;
    /* CPU_UNCACHED visibility is provided by required hardware IO coherency
     * over the SAME caller WB pages, not a label change or NC-PTE claim.
     * Public cache operations stay legal NOPs; no noncoherent fallback. */
    int e = (access & NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED) ?
        nn_bo_import_coherent(device->gpu, v->storage, v->size, &bo) :
        nn_bo_import(device->gpu, v->storage, v->size, &bo);
    if (e) { free(rollback); error(e); return 0; }
    /* The caller may have dirtied cached backing before GPU-only ownership.
     * Public mapped cache calls are legal NOPs for NO_ACCESS; this private
     * ownership transfer must finish before publishing the pool. */
    if (access & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) e = nn_bo_cache(bo, 0, v->size, 0);
    const size_t bytes=v->size;void *storage=v->storage;const int flags=v->flags;
    struct object *o = e ? NULL : create(key, POOL);
    if (e || !o) {
        if (e) error(e);
        /* Preallocated cleanup metadata also retains a failed destroy safely:
         * no object is published; device Finalize/Finish can reap the BO later. */
        rollback->bo = bo; device->refs++;
        retire_jobs(device, rollback);
        return 0;
    }
    free(rollback);
    o->bo = bo; o->device = device; o->device->refs++;
    o->size = bytes; o->storage = storage; o->flags = flags;
    o->gpu_policy = unqualified ? NN_NVN_GPU_UNCACHED_UNQUALIFIED : NN_NVN_GPU_POOL_CACHED;
    return 1;
}
int nn_nvn_import_image_pool(NVNdevice *key, NVNmemoryPool *pool,
                              const struct nn_external_image_desc *desc)
{
    struct object *device = find(key, DEVICE);
    if (!device || !pool || !desc) return EINVAL;
    int key_error=creation_key_error(pool,POOL);if(key_error)return key_error;
    struct object *o = create(pool, POOL); if (!o) return nn_nvn_error();
    nn_external_image *external = NULL;
    int e = nn_external_image_import(device->gpu, desc, &external);
    if (e) { remove_object(o); return e; }
    struct nn_external_image_view view;
    (void)nn_external_image_view(external, &view); /* just-imported live contract */
    o->external = external; o->bo = view.bo; o->size = view.fd_bytes;
    o->flags = NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED;
    o->device = device; device->refs++; return 0;
}
int nn_nvn_image_pool_view(NVNmemoryPool *key, struct nn_external_image_view *out)
{
    if (!out) return EINVAL;
    memset(out, 0, sizeof(*out));
    struct object *o = find(key, POOL);
    return !o ? EINVAL : !o->external ? ENOTSUP : nn_external_image_view(o->external, out);
}
int nn_nvn_image_pool_pin(NVNmemoryPool *key)
{
    struct object *o = find(key, POOL); if (!o || !o->external) return EINVAL;
    int e = nn_external_image_pin(o->external); if (!e) o->refs++; return e;
}
int nn_nvn_image_pool_unpin(NVNmemoryPool *key)
{
    struct object *o = find(key, POOL); if (!o || !o->external) return EINVAL;
    int e = nn_external_image_unpin(o->external); if (!e) o->refs--; return e;
}
static void api_nvnMemoryPoolFinalize(NVNmemoryPool *key)
{
    struct object *o = find(key, POOL); if (!o) return;
    if (o->refs) { error(EBUSY); return; }
    int e = o->external ? nn_external_image_destroy(o->external) : nn_bo_destroy(o->bo);
    if (e) { error(e); return; }
    o->device->refs--; remove_object(o);
}
static void *api_nvnMemoryPoolMap(const NVNmemoryPool *key)
{ struct object *o = find(key, POOL); if (o && o->external) { error(ENOTSUP); return NULL; }
  return o && !(o->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) ? nn_bo_map(o->bo) : NULL; }
static NVNbufferAddress api_nvnMemoryPoolGetBufferAddress(const NVNmemoryPool *key)
{ struct object *o = find(key, POOL); return o ? nn_bo_address(o->bo) : 0; }
static void pool_cache(const NVNmemoryPool *key, ptrdiff_t offset, size_t size, int from)
{
    struct object *o = find(key, POOL); if (!o) return;
    if (o->external) { error(ENOTSUP); return; }
    if (offset < 0 || (size_t)offset > o->size || size > o->size - (size_t)offset) { error(EINVAL); return; }
    if (o->flags & NVN_MEMORY_POOL_FLAGS_CPU_CACHED) error(nn_bo_cache(o->bo, offset, size, from));
}
static void api_nvnMemoryPoolFlushMappedRange(const NVNmemoryPool *p, ptrdiff_t o, size_t n) { pool_cache(p,o,n,0); }
static void api_nvnMemoryPoolInvalidateMappedRange(const NVNmemoryPool *p, ptrdiff_t o, size_t n) { pool_cache(p,o,n,1); }
static void api_nvnBufferBuilderSetDefaults(NVNbufferBuilder *b) { builder(b, BB); }
static void api_nvnBufferBuilderSetDevice(NVNbufferBuilder *b, NVNdevice *d)
{ struct object *o=find(b,BB),*device=find(d,DEVICE);if(o&&device){o->device=device;o->builder_device_key=d;} }
static void api_nvnBufferBuilderSetStorage(NVNbufferBuilder *b, NVNmemoryPool *p, ptrdiff_t offset, size_t size)
{
    struct object *o = find(b, BB); if (!o) return;
    struct object *pool = find(p, POOL);
    if (pool && pool->external) { error(ENOTSUP); return; }
    if (!pool || offset < 0 || (size_t)offset > pool->size || size > pool->size - offset) { error(EINVAL); return; }
    o->pool = pool;o->builder_pool_key=p; o->offset = offset; o->size = size;
}
static NVNboolean api_nvnBufferInitialize(NVNbuffer *key, const NVNbufferBuilder *b)
{
    struct object *v = find(b, BB);
    if (!v || !v->pool || v->device != v->pool->device) { error(EINVAL); return 0; }
    struct object *pool=v->pool;const size_t offset=v->offset,bytes=v->size;
    struct object *o = create(key, BUFFER); if (!o) return 0;
    o->pool = pool; o->pool->refs++; o->offset = offset; o->size = bytes; return 1;
}
static void api_nvnBufferFinalize(NVNbuffer *key)
{ struct object *o = find(key, BUFFER); if (o) { o->pool->refs--; remove_object(o); } }
static void *api_nvnBufferMap(const NVNbuffer *key)
{
    struct object *o = find(key, BUFFER);
    if (!o || (o->pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS)) return NULL;
    void *map = nn_bo_map(o->pool->bo);
    return map ? (char *)map + o->offset : NULL;
}
static NVNbufferAddress api_nvnBufferGetAddress(const NVNbuffer *key)
{ struct object *o = find(key, BUFFER); return o ? nn_bo_address(o->pool->bo) + o->offset : 0; }
static void buffer_cache(const NVNbuffer *key, ptrdiff_t offset, size_t size, int from)
{
    struct object *o = find(key, BUFFER); if (!o) return;
    if (offset < 0 || (size_t)offset > o->size || size > o->size - offset) error(EINVAL);
    else if (o->pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_CACHED) error(nn_bo_cache(o->pool->bo, o->offset + offset, size, from));
}
static void api_nvnBufferFlushMappedRange(const NVNbuffer *p, ptrdiff_t o, size_t n) { buffer_cache(p,o,n,0); }
static void api_nvnBufferInvalidateMappedRange(const NVNbuffer *p, ptrdiff_t o, size_t n) { buffer_cache(p,o,n,1); }
enum { TEX_FLAGS = 1, TEX_TARGET = 2, TEX_WIDTH = 4, TEX_HEIGHT = 8,
       TEX_LEVELS = 16, TEX_SAMPLES = 32, TEX_FORMAT = 64, TEX_STRIDE = 128, TEX_COMPLETE = 255 };
#include "resource_layout.inc"
#include "texture_buffer_resource.inc"
#include "texture_source_resource.inc"
static int texture_profile(const struct texture_desc *t)
{
    /* Public flags: LINEAR=0x10, LINEAR_RENDER_TARGET=0x40. The latter
     * requires a separate layout qualification (the original reports 128B).
     * 64B here is this backend's conservative qualified LINEAR alignment,
     * not a claim that the original NVN's LINEAR minimum is 64B. */
    return t->set == TEX_COMPLETE && t->flags == NVN_TEXTURE_FLAGS_LINEAR &&
        t->target == NVN_TEXTURE_TARGET_2D &&
        ((t->width == 64 && t->height == 64) || (t->width == 2 && t->height == 2)) &&
        t->depth == 1 && t->levels == 1 && !t->samples &&
        t->format == NVN_FORMAT_RGBA8 && t->stride == 256;
}
static void api_nvnTextureBuilderSetDefaults(NVNtextureBuilder *key)
{
    struct object *old = NULL;
    for (struct object *o = objects; o; o = o->next) if (o->key == key && o->kind == TB) { old = o; break; }
    struct object *device = old ? old->device : NULL;
    const NVNdevice *device_key=old?old->builder_device_key:NULL;
    struct object *o = builder(key, TB); if (!o) return;
    o->device = device;o->builder_device_key=device_key;
    o->texture_packaged=0; memset(o->packaged_layout,0,8);
    /* SDK53.314 4752a0: device is preserved; dimensions/flags/format/
     * stride are zero, levels is one. Default flags select source block layout. */
    o->texture = (struct texture_desc){.target=NVN_TEXTURE_TARGET_2D,
        .levels=1, .set=TEX_COMPLETE};
}
static void api_nvnTextureBuilderSetDevice(NVNtextureBuilder *key, NVNdevice *d)
{
    struct object *o = value_builder(key,TB);
    struct object *device=find(d,DEVICE);
    if(o&&device){o->device=device;o->builder_device_key=d;}
}
/* SDK53.314 4752e0..4753d0: these are metadata stores, not allocation
 * or Adreno layout qualification. Preserve the declared scalar; unsupported
 * layout combinations still fail GetStorageSize/Alignment/Initialize without
 * publishing a native resource. This does not certify illegal NVN inputs. */
#define TEXTURE_SET(name, type, field, bit) \
    static void api_##name(NVNtextureBuilder *key, type value) { \
        struct object *o = find(key, TB); if (!o) return; \
        o->texture.field = value; o->texture.set |= bit; }
TEXTURE_SET(nvnTextureBuilderSetFlags, int, flags, TEX_FLAGS)
TEXTURE_SET(nvnTextureBuilderSetTarget, NVNtextureTarget, target, TEX_TARGET)
TEXTURE_SET(nvnTextureBuilderSetWidth, int, width, TEX_WIDTH)
TEXTURE_SET(nvnTextureBuilderSetHeight, int, height, TEX_HEIGHT)
TEXTURE_SET(nvnTextureBuilderSetDepth, int, depth, 0)
TEXTURE_SET(nvnTextureBuilderSetLevels, int, levels, TEX_LEVELS)
TEXTURE_SET(nvnTextureBuilderSetSamples, int, samples, TEX_SAMPLES)
TEXTURE_SET(nvnTextureBuilderSetFormat, NVNformat, format, TEX_FORMAT)
TEXTURE_SET(nvnTextureBuilderSetStride, ptrdiff_t, stride, TEX_STRIDE)
#undef TEXTURE_SET
static void api_nvnTextureBuilderSetSize2D(NVNtextureBuilder *key, int width, int height)
{
    struct object *o = find(key, TB); if (!o) return;
    o->texture.width = width; o->texture.height = height; o->texture.depth = 1;
    o->texture.set |= TEX_WIDTH | TEX_HEIGHT;
}
static void api_nvnTextureBuilderSetSize3D(NVNtextureBuilder *key,int width,int height,int depth)
{
    struct object *o=find(key,TB);if(!o)return;
    o->texture.width=width;o->texture.height=height;o->texture.depth=depth;
    o->texture.set|=TEX_WIDTH|TEX_HEIGHT;
}
/* Public scalar getters expose stored metadata, independently of native
 * layout support. Their PFN signatures are checked by the entry table. */
#define TEXTURE_BUILDER_GET(name,type,field) \
static type api_##name(const NVNtextureBuilder *key) { \
    struct object *o=find(key,TB);return o?(type)o->texture.field:(type)0; }
TEXTURE_BUILDER_GET(nvnTextureBuilderGetTarget,NVNtextureTarget,target)
TEXTURE_BUILDER_GET(nvnTextureBuilderGetWidth,int,width)
TEXTURE_BUILDER_GET(nvnTextureBuilderGetHeight,int,height)
TEXTURE_BUILDER_GET(nvnTextureBuilderGetDepth,int,depth)
TEXTURE_BUILDER_GET(nvnTextureBuilderGetLevels,int,levels)
TEXTURE_BUILDER_GET(nvnTextureBuilderGetSamples,int,samples)
TEXTURE_BUILDER_GET(nvnTextureBuilderGetFormat,NVNformat,format)
#undef TEXTURE_BUILDER_GET
static void api_nvnTextureBuilderSetStorage(NVNtextureBuilder *key, NVNmemoryPool *p, ptrdiff_t offset)
{
    struct object *o = find(key, TB), *pool = find(p, POOL); if (!o || !pool) return;
    if(o->texture.target==NVN_TEXTURE_TARGET_BUFFER){error(buffer_texture_set_storage(o,pool,offset));return;}
    size_t bytes=0; (void)linear_texture_bytes(&o->texture,&bytes);
    if (!(o->texture.flags & 0x50)) {
        struct nn_tex_source_layout payload;
        if (!source_builder_layout(o,&payload)) bytes=(size_t)payload.payload_bytes;
    }
    if (pool->external) {
        struct nn_external_image_view view;
        if (nn_external_image_view(pool->external, &view) || offset < 0 ||
            (size_t)offset != view.pixel_offset || (bytes && bytes != view.payload_bytes)) { error(ENOTSUP); return; }
    }
    if (pool->device != o->device || offset < 0 || (size_t)offset > pool->size ||
        bytes > pool->size - offset || (nn_bo_address(pool->bo) + offset) % 64) { error(EINVAL); return; }
    o->pool = pool;o->builder_pool_key=p; o->offset = offset;
}
static size_t texture_storage(const NVNtextureBuilder *key, size_t result)
{
    struct object *o = find(key, TB); if (!o) return 0;
    if(o->texture.target==NVN_TEXTURE_TARGET_BUFFER){
        size_t bytes=0;int e=o->texture_packaged?ENOTSUP:buffer_texture_bytes(&o->texture,&bytes);
        size_t alignment=0;if(!e && result)e=buffer_texture_alignment(o,&alignment);
        if(e){error(e);return 0;}
        return result?alignment:bytes;
    }
    if (!(o->texture.flags & 0x50)) {
        struct nn_tex_source_layout payload; int e=source_builder_layout(o,&payload);
        if(e){error(e);return 0;}
        return result ? SOURCE_KIND_ALIGNMENT : (size_t)payload.payload_bytes;
    }
    size_t bytes; int e=o->texture_packaged ? ENOTSUP : linear_texture_bytes(&o->texture,&bytes);
    if (e) { error(e); return 0; }
    return result ? result : bytes;
}
static size_t api_nvnTextureBuilderGetStorageSize(const NVNtextureBuilder *b) { return texture_storage(b, 0); }
static size_t api_nvnTextureBuilderGetStorageAlignment(const NVNtextureBuilder *b) { return texture_storage(b, 64); }
static ptrdiff_t api_nvnTextureBuilderGetStride(const NVNtextureBuilder *b)
{ struct object *o = find(b, TB); return o ? o->texture.stride : 0; }
static NVNtextureFlags api_nvnTextureBuilderGetFlags(const NVNtextureBuilder *b)
{ struct object *o = find(b, TB); return o ? (NVNtextureFlags)o->texture.flags : 0; }
static NVNboolean api_nvnTextureInitialize(NVNtexture *key, const NVNtextureBuilder *b)
{
    struct object *v = find(b, TB); if (!v) return 0;
    /* All three texture constructors consume this value snapshot. In
     * particular create(key,...) may retire the input builder at key==b. */
    struct object snapshot=*v;v=&snapshot;
    if(v->texture.target==NVN_TEXTURE_TARGET_BUFFER)return buffer_texture_initialize(key,v);
    if (!(v->texture.flags & 0x50)) return source_texture_initialize(key,v);
    size_t bytes; int layout_error=v->texture_packaged ? ENOTSUP : linear_texture_bytes(&v->texture,&bytes);
    if (layout_error) { error(layout_error); return 0; }
    if (!v->pool || !v->device || v->pool->device != v->device) { error(EINVAL); return 0; }
    if (gpu_pool_access(v->pool,v->offset,bytes,"nvnTextureInitialize")) return 0;
    if (v->pool->external && !nn_external_image_live(v->pool->external)) { error(EBUSY); return 0; }
    if (v->offset > v->pool->size || bytes > v->pool->size - v->offset ||
        (nn_bo_address(v->pool->bo) + v->offset) % 64) { error(EINVAL); return 0; }
    struct object *o = create(key, TEXTURE); if (!o) return 0;
    if (v->pool->external) {
        int e = nn_external_image_claim_texture(v->pool->external, v->offset, bytes);
        if (e) { error(e); remove_object(o); return 0; }
    }
    o->pool = v->pool; o->pool->refs++; o->device = v->device; o->bo = o->pool->bo;
    o->offset = v->offset; o->size = bytes; o->texture = v->texture; return 1;
}
static void api_nvnTextureFinalize(NVNtexture *key)
{
    struct object *o = find(key, TEXTURE); if (!o) return;
    if (o->refs) { error(EBUSY); return; }
    if (o->pool->external) {
        int e = nn_external_image_release_texture(o->pool->external);
        if (e) { error(e); return; }
    }
    o->pool->refs--; free(o->source_layout); remove_object(o);
}
static int api_nvnTextureGetStorageSize(const NVNtexture *key)
{ struct object *o = find(key, TEXTURE); return o ? (int)o->size : 0; }
static ptrdiff_t api_nvnTextureGetStride(const NVNtexture *key)
{ struct object *o = find(key, TEXTURE); return o ? o->texture.stride : 0; }
static NVNtextureFlags api_nvnTextureGetFlags(const NVNtexture *key)
{ struct object *o = find(key, TEXTURE); return o ? (NVNtextureFlags)o->texture.flags : 0; }
static NVNtextureTarget api_nvnTextureGetTarget(const NVNtexture *key)
{ struct object *o = find(key, TEXTURE); return o ? (NVNtextureTarget)o->texture.target : 0; }
/* Original SDK classifies either public LINEAR bit (flags&0x50) as 1.
 * We expose that class only for this backend's actual validated LINEAR layout;
 * this is not a Tegra memory-kind value, nor a promise of tiled/MSAA support. */
static NVNrawStorageClass api_nvnTextureGetRawStorageClass(const NVNtexture *key)
{
    struct object *o=find(key,TEXTURE);if(!o)return NVN_RAW_STORAGE_CLASS_NONE;
    if(o->source_layout){
        /* Original SDK53.314 477590..5e8, raw format-class table abf113.
         * Storage class is observable even with GPU depth consumers absent. */
        switch(o->texture.format){
        case NVN_FORMAT_STENCIL8:case NVN_FORMAT_DEPTH16:return NVN_RAW_STORAGE_CLASS_S8_AND_Z16;
        case NVN_FORMAT_DEPTH24:case NVN_FORMAT_DEPTH24_STENCIL8:return NVN_RAW_STORAGE_CLASS_Z24_AND_Z24S8;
        case NVN_FORMAT_DEPTH32F_STENCIL8:return NVN_RAW_STORAGE_CLASS_ZF32S8;
        default:return NVN_RAW_STORAGE_CLASS_COLOR_AND_ZF32;
        }
    }
    if(o->texture.target==NVN_TEXTURE_TARGET_BUFFER){
        size_t bytes=0;int e=buffer_texture_bytes(&o->texture,&bytes);
        if(e || bytes!=o->size){error(e?e:ENOTSUP);return NVN_RAW_STORAGE_CLASS_NONE;}
        return (o->texture.flags&0x50)?NVN_RAW_STORAGE_CLASS_LINEAR:NVN_RAW_STORAGE_CLASS_COLOR_AND_ZF32;
    }
    size_t bytes=0;int e=linear_texture_bytes(&o->texture,&bytes);
    if(e || bytes!=o->size){error(e?e:ENOTSUP);return NVN_RAW_STORAGE_CLASS_NONE;}
    return NVN_RAW_STORAGE_CLASS_LINEAR;
}
static NVNformat api_nvnTextureGetFormat(const NVNtexture *key)
{ struct object *o = find(key, TEXTURE); return o ? (NVNformat)o->texture.format : 0; }
#define TEXTURE_GET(name, field) \
    static int api_##name(const NVNtexture *key) { struct object *o = find(key, TEXTURE); return o ? o->texture.field : 0; }
TEXTURE_GET(nvnTextureGetWidth, width)
TEXTURE_GET(nvnTextureGetHeight, height)
TEXTURE_GET(nvnTextureGetDepth, depth)
TEXTURE_GET(nvnTextureGetLevels, levels)
TEXTURE_GET(nvnTextureGetSamples, samples)
#undef TEXTURE_GET
static NVNtextureAddress api_nvnTextureGetTextureAddress(const NVNtexture *key)
{ struct object *o = find(key, TEXTURE); return o ? nn_bo_address(o->bo) + o->offset : 0; }
#include "texture_view_offset.inc"
static ptrdiff_t api_nvnTextureGetViewOffset(const NVNtexture *key, const NVNtextureView *view)
{
    struct object *o=find(key,TEXTURE);if(!o)return 0;
    ptrdiff_t offset=0;int e=texture_view_offset(o,view,&offset);if(e){error(e);return 0;}
    return offset;
}

static void api_nvnDeviceGetInteger(const NVNdevice *key, NVNdeviceInfo what, int *out)
{
    if (!find(key, DEVICE)) return;
    if (!out) { error(EINVAL); return; }
    switch (what) {
    case NVN_DEVICE_INFO_API_MAJOR_VERSION: *out = NN_NVN_ABI_MAJOR; return;
    case NVN_DEVICE_INFO_API_MINOR_VERSION: *out = NN_NVN_ABI_MINOR; return;
    case NVN_DEVICE_INFO_TEXTURE_DESCRIPTOR_SIZE:
    case NVN_DEVICE_INFO_SAMPLER_DESCRIPTOR_SIZE: *out = DESCRIPTOR_BYTES; return;
    case NVN_DEVICE_INFO_RESERVED_TEXTURE_DESCRIPTORS:
    case NVN_DEVICE_INFO_RESERVED_SAMPLER_DESCRIPTORS: *out = RESERVED_DESCRIPTORS; return;
    case NVN_DEVICE_INFO_MAX_TEXTURE_POOL_SIZE: *out = MAX_TEXTURE_DESCRIPTORS; return;
    case NVN_DEVICE_INFO_MAX_SAMPLER_POOL_SIZE: *out = MAX_SAMPLER_DESCRIPTORS; return;
    default: error(ENOTSUP); return;
    }
}
static int descriptor_id(int id, enum kind kind)
{
    return id >= RESERVED_DESCRIPTORS && id < (kind == TPOOL ? MAX_TEXTURE_DESCRIPTORS : MAX_SAMPLER_DESCRIPTORS);
}
/* Original pinned SDK sdk+0x4660d0 returns the signed image index. The legal
 * application index policy below remains256..2^20-1. This CPU carrier does
 * not enable shader image access or require descriptor registration. */
static NVNimageHandle api_nvnDeviceGetImageHandle(const NVNdevice *key, int id)
{
    if (!find(key, DEVICE)) return 0;
    if (!descriptor_id(id, TPOOL)) { error(EINVAL); return 0; }
    return (NVNimageHandle)(uint64_t)id;
}
static NVNseparateTextureHandle api_nvnDeviceGetSeparateTextureHandle(const NVNdevice *key, int id)
{
    if (!find(key, DEVICE)) return (NVNseparateTextureHandle){0};
    if (!descriptor_id(id, TPOOL)) { error(EINVAL); return (NVNseparateTextureHandle){0}; }
    return (NVNseparateTextureHandle){UINT64_C(0x100000000) | (uint64_t)id};
}
static NVNseparateSamplerHandle api_nvnDeviceGetSeparateSamplerHandle(const NVNdevice *key, int id)
{
    if (!find(key, DEVICE)) return (NVNseparateSamplerHandle){0};
    if (!descriptor_id(id, SPOOL)) { error(EINVAL); return (NVNseparateSamplerHandle){0}; }
    return (NVNseparateSamplerHandle){(uint64_t)id << 20};
}
static NVNtextureHandle api_nvnDeviceGetTextureHandle(const NVNdevice *key, int texture, int sampler)
{
    if (!find(key, DEVICE)) return 0;
    if (!descriptor_id(texture, TPOOL) || !descriptor_id(sampler, SPOOL)) { error(EINVAL); return 0; }
    uint32_t payload = (uint32_t)texture | ((uint32_t)sampler << 20);
    /* sdk+0x466150: OR W registers, SXTW, then set bit32. Do not use
     * SeparateSampler's64-bit-shift carrier rule for this combined value. */
    uint64_t signed_payload = payload & UINT32_C(0x80000000) ?
        UINT64_C(0xffffffff00000000) | payload : payload;
    return signed_payload | UINT64_C(0x100000000);
}
/* sdk+0x466140 has exactly SeparateTexture's carrier encoding. This adds a
 * CPU handle constructor only; it does not enable a GPU texel-fetch pipeline. */
static NVNtextureHandle api_nvnDeviceGetTexelFetchHandle(const NVNdevice *key, int id)
{
    if (!find(key, DEVICE)) return 0;
    if (!descriptor_id(id, TPOOL)) { error(EINVAL); return 0; }
    return UINT64_C(0x100000000) | (uint64_t)id;
}
/* SDK scalar graphics Bind writes low32; plural graphics Bind writes both
 * words. This helper decodes only the native descriptor lookup payload.
 * Native policy accepts valid legacy raw32
 * values and the pinned SDK's exact decorated carriers, rejecting other upper
 * bits. Output is a pool-index payload, never a GPUVA or guest marker. */
static int texture_handle_payload(uint64_t carrier, int domain, uint32_t *out)
{
    if (!out || domain < 0 || domain > 2) return EINVAL;
    if (!carrier) { *out = 0; return 0; }
    uint32_t payload = (uint32_t)carrier;
    if (domain == 2) {
        /* SDK SeparateSampler: signextend the inputID, then shift64 by20.
         * Legal IDs256..4095 therefore have an unsigned32-bit payload. */
        if (carrier > UINT32_MAX || (payload & UINT32_C(0xfffff)) ||
            !descriptor_id((int)(payload >> 20), SPOOL)) return EINVAL;
    } else {
        uint64_t sdk = (payload & UINT32_C(0x80000000) ?
            UINT64_C(0xffffffff00000000) | payload : payload) | UINT64_C(0x100000000);
        if (carrier > UINT32_MAX && carrier != sdk) return EINVAL;
        if (!descriptor_id((int)(payload & UINT32_C(0xfffff)), TPOOL) ||
            (domain == 1 && (payload & UINT32_C(0xfff00000)))) return EINVAL;
        /* Sampler0 is the proven GetTexelFetchHandle carrier. Sampled Draw
         * consumers still separately require a valid registered sampler. */
        if (domain == 0 && payload >> 20 &&
            !descriptor_id((int)(payload >> 20), SPOOL)) return EINVAL;
    }
    *out = payload;
    return 0;
}
/* Semantic state is translated independently from the shader artifact.
 * The SW descriptor remains the existing32-byte publication/tamper audit;
 * immutable full sampler values live in its retained CPU metadata snapshot. */
static struct nn_sampler_state native_sampler_state(const struct sampler_desc *s)
{
    return (struct nn_sampler_state){.version=NN_SAMPLER_STATE_VERSION,
        .min_filter=s->min,.mag_filter=s->mag,.wrap={s->wrap[0],s->wrap[1],s->wrap[2]},
        .compare_mode=s->compare,.compare_func=s->compare_func,.reduction=s->reduction,
        .lod_min=s->lod_min,.lod_max=s->lod_max,.lod_bias=s->lod_bias,.anisotropy=s->anisotropy,
        .lod_snap=s->lod_snap,.border={s->border[0],s->border[1],s->border[2],s->border[3]}};
}
static int sampler_profile(const struct sampler_desc *s)
{ struct nn_sampler_state state=native_sampler_state(s);return !nn_sampler_state_validate(&state); }
/* SDK53.314 setters preserve scalar/float bits without native capability
 * checks. Metadata enum validity is separate from native GPU consumption;
 * float ranges/quantization are checked only by the actual native encoder. */
static int sampler_metadata(const struct sampler_desc *s)
{
    return (unsigned)s->min>NVN_MIN_FILTER_LINEAR_MIPMAP_LINEAR ||
        (unsigned)s->mag>NVN_MAG_FILTER_LINEAR ||
        (unsigned)s->wrap[0]>NVN_WRAP_MODE_CLAMP_TO_EDGE ||
        (unsigned)s->wrap[1]>NVN_WRAP_MODE_CLAMP_TO_EDGE ||
        (unsigned)s->wrap[2]>NVN_WRAP_MODE_CLAMP_TO_EDGE ||
        (unsigned)s->compare>NVN_COMPARE_MODE_COMPARE_R_TO_TEXTURE ||
        s->compare_func<NVN_COMPARE_FUNC_NEVER || s->compare_func>NVN_COMPARE_FUNC_ALWAYS ||
        (unsigned)s->reduction>NVN_SAMPLER_REDUCTION_MAX ? EINVAL : 0;
}
static void update_sampler(struct object *o,const struct sampler_desc *next)
{ o->sampler=*next; }
static struct object *sampler_builder(const NVNsamplerBuilder *key)
{ return value_builder(key,SB); }
static void api_nvnSamplerBuilderSetDevice(NVNsamplerBuilder *key, NVNdevice *device)
{
    struct object *d=find(device,DEVICE);if(!d)return;
    struct object *o=sampler_builder(key);if(!o)return;
    o->device=d;o->builder_device_key=device;o->sampler_device_key=device;
}
static const NVNdevice *api_nvnSamplerBuilderGetDevice(const NVNsamplerBuilder *key)
{ struct object *o=find(key,SB);return o?o->sampler_device_key:NULL; }
static void api_nvnSamplerBuilderSetDefaults(NVNsamplerBuilder *key)
{
    struct object *o = sampler_builder(key); if (!o) return;
    /* Original SDK dictionary355 -> sdk+0x473340; getter fields and constant
     * tables prove these defaults. Device is preserved. Native sampling translates
     * these semantics into independent job-local Adreno descriptors. */
    o->sampler = (struct sampler_desc){.min=NVN_MIN_FILTER_LINEAR,
        .mag=NVN_MAG_FILTER_LINEAR, .wrap={NVN_WRAP_MODE_REPEAT,NVN_WRAP_MODE_REPEAT,NVN_WRAP_MODE_REPEAT},
        .compare=NVN_COMPARE_MODE_NONE, .compare_func=NVN_COMPARE_FUNC_LESS,
        .lod_min=0, .lod_max=1000, .anisotropy=1, .reduction=NVN_SAMPLER_REDUCTION_AVERAGE};
}
static void api_nvnSamplerBuilderSetMinMagFilter(NVNsamplerBuilder *key,NVNminFilter min,NVNmagFilter mag)
{
    struct object *o=find(key,SB);if(!o)return;
    if((unsigned)min>NVN_MIN_FILTER_LINEAR_MIPMAP_LINEAR || (unsigned)mag>NVN_MAG_FILTER_LINEAR){error(EINVAL);return;}
    struct sampler_desc next=o->sampler;next.min=min;next.mag=mag;update_sampler(o,&next);
}
static void api_nvnSamplerBuilderSetWrapMode(NVNsamplerBuilder *key,NVNwrapMode x,NVNwrapMode y,NVNwrapMode z)
{
    struct object *o=find(key,SB);if(!o)return;
    if((unsigned)x>NVN_WRAP_MODE_CLAMP_TO_EDGE || (unsigned)y>NVN_WRAP_MODE_CLAMP_TO_EDGE || (unsigned)z>NVN_WRAP_MODE_CLAMP_TO_EDGE){error(EINVAL);return;}
    struct sampler_desc next=o->sampler;next.wrap[0]=x;next.wrap[1]=y;next.wrap[2]=z;update_sampler(o,&next);
}
static void api_nvnSamplerBuilderSetLodClamp(NVNsamplerBuilder *key,float min,float max)
{
    struct object *o=find(key,SB);if(!o)return;
    struct sampler_desc next=o->sampler;next.lod_min=min;next.lod_max=max;update_sampler(o,&next);
}
static void api_nvnSamplerBuilderSetCompare(NVNsamplerBuilder *key,NVNcompareMode mode,NVNcompareFunc func)
{
    struct object *o=find(key,SB);if(!o)return;
    if((unsigned)mode>NVN_COMPARE_MODE_COMPARE_R_TO_TEXTURE || func<NVN_COMPARE_FUNC_NEVER || func>NVN_COMPARE_FUNC_ALWAYS){error(EINVAL);return;}
    struct sampler_desc next=o->sampler;next.compare=mode;next.compare_func=func;update_sampler(o,&next);
}
/* SDK53.314 float/int/ui entry points all copy the same four32-bit words.
 * No float/int conversion and no tag changes the stored border representation. */
_Static_assert(sizeof(int)==4 && sizeof(float)==4,"NVN border word ABI");
static void sampler_border_store(NVNsamplerBuilder *key,const void *values)
{
    struct object *o=find(key,SB);if(!o)return;
    if(!values){error(EINVAL);return;}
    struct sampler_desc next=o->sampler;memcpy(next.border,values,sizeof(next.border));update_sampler(o,&next);
}
static void api_nvnSamplerBuilderSetBorderColor(NVNsamplerBuilder *key,const float *values)
{ sampler_border_store(key,values); }
static void api_nvnSamplerBuilderSetBorderColori(NVNsamplerBuilder *key,const int *values)
{ sampler_border_store(key,values); }
static void api_nvnSamplerBuilderSetBorderColorui(NVNsamplerBuilder *key,const uint32_t *values)
{ sampler_border_store(key,values); }
static void sampler_border_load(const void *key,enum kind kind,void *values)
{
    struct object *o=find(key,kind);if(!o)return;
    if(!values){error(EINVAL);return;}
    memcpy(values,o->sampler.border,sizeof(o->sampler.border));
}
#define SAMPLER_BORDER_GET(prefix,type,kind,suffix,word) \
    static void api_##prefix##GetBorderColor##suffix(const type *key,word *values) \
    { sampler_border_load(key,kind,values); }
SAMPLER_BORDER_GET(nvnSamplerBuilder,NVNsamplerBuilder,SB,i,int)
SAMPLER_BORDER_GET(nvnSamplerBuilder,NVNsamplerBuilder,SB,ui,uint32_t)
SAMPLER_BORDER_GET(nvnSampler,NVNsampler,SAMPLER,i,int)
SAMPLER_BORDER_GET(nvnSampler,NVNsampler,SAMPLER,ui,uint32_t)
#undef SAMPLER_BORDER_GET
#define SAMPLER_SET(name,type,field) \
    static void api_##name(NVNsamplerBuilder *key,type value) { \
        struct object *o=find(key,SB);if(!o)return;struct sampler_desc next=o->sampler; \
        next.field=value;update_sampler(o,&next); }
SAMPLER_SET(nvnSamplerBuilderSetLodBias,float,lod_bias)
SAMPLER_SET(nvnSamplerBuilderSetMaxAnisotropy,float,anisotropy)
static void api_nvnSamplerBuilderSetReductionFilter(NVNsamplerBuilder *key,NVNsamplerReduction value)
{
    struct object *o=find(key,SB);if(!o)return;
    if((unsigned)value>NVN_SAMPLER_REDUCTION_MAX){error(EINVAL);return;}
    o->sampler.reduction=value;
}
SAMPLER_SET(nvnSamplerBuilderSetLodSnap,float,lod_snap)
#undef SAMPLER_SET
#define SAMPLER_GETTERS(prefix, type, kind) \
    static void api_##prefix##GetMinMagFilter(const type *key, NVNminFilter *min, NVNmagFilter *mag) { \
        struct object *o = find(key, kind); if (!o) return; \
        if (!min || !mag) { error(EINVAL); return; } *min = o->sampler.min; *mag = o->sampler.mag; } \
    static void api_##prefix##GetWrapMode(const type *key, NVNwrapMode *s, NVNwrapMode *t, NVNwrapMode *r) { \
        struct object *o = find(key, kind); if (!o) return; \
        if (!s || !t || !r) { error(EINVAL); return; } *s=o->sampler.wrap[0]; *t=o->sampler.wrap[1]; *r=o->sampler.wrap[2]; } \
    static void api_##prefix##GetLodClamp(const type *key, float *min, float *max) { \
        struct object *o=find(key,kind); if (!o) return; \
        if (!min || !max) { error(EINVAL); return; } *min=o->sampler.lod_min; *max=o->sampler.lod_max; } \
    static void api_##prefix##GetCompare(const type *key, NVNcompareMode *mode, NVNcompareFunc *func) { \
        struct object *o=find(key,kind); if (!o) return; \
        if (!mode || !func) { error(EINVAL); return; } *mode=o->sampler.compare; *func=o->sampler.compare_func; } \
    static void api_##prefix##GetBorderColor(const type *key, float *values) { \
        struct object *o=find(key,kind); if (!o) return; \
        if (!values) { error(EINVAL); return; } memcpy(values,o->sampler.border,sizeof(o->sampler.border)); } \
    static float api_##prefix##GetLodBias(const type *key) { struct object *o=find(key,kind); return o?o->sampler.lod_bias:0; } \
    static float api_##prefix##GetMaxAnisotropy(const type *key) { struct object *o=find(key,kind); return o?o->sampler.anisotropy:0; } \
    static NVNsamplerReduction api_##prefix##GetReductionFilter(const type *key) { \
        struct object *o=find(key,kind); return o?o->sampler.reduction:NVN_SAMPLER_REDUCTION_AVERAGE; }
SAMPLER_GETTERS(nvnSamplerBuilder, NVNsamplerBuilder, SB)
SAMPLER_GETTERS(nvnSampler, NVNsampler, SAMPLER)
#undef SAMPLER_GETTERS
static float api_nvnSamplerBuilderGetLodSnap(const NVNsamplerBuilder *key)
{ struct object *o=find(key,SB); return o?o->sampler.lod_snap:0; }
static NVNboolean api_nvnSamplerInitialize(NVNsampler *key, const NVNsamplerBuilder *b)
{
    struct object *v = find(b, SB); if (!v) return 0;
    struct object *device=live_weak(v->device,DEVICE);
    if (!device || device->key!=v->sampler_device_key) { error(EINVAL); return 0; }
    int metadata_error=sampler_metadata(&v->sampler);if(metadata_error){error(metadata_error);return 0;}
    const struct sampler_desc sampler=v->sampler;
    struct object *o = create(key, SAMPLER); if (!o) return 0;
    o->device=device; o->device->refs++; o->sampler=sampler; return 1;
}
static void api_nvnSamplerFinalize(NVNsampler *key)
{
    struct object *o = find(key, SAMPLER); if (!o) return;
    if (o->refs) { error(EBUSY); return; } o->device->refs--; remove_object(o);
}
static NVNboolean descriptor_pool_initialize(void *key, const NVNmemoryPool *memory, ptrdiff_t offset, int count, enum kind kind)
{
    struct object *pool = find(memory, POOL); if (!pool) return 0;
    /* Only this software-table role is qualified. The backing pool retains
     * its original GPU policy and cannot become texture/code/command data. */
    if (pool->external || (pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) ||
        !pool->bo || !nn_bo_map(pool->bo)) { error(ENOTSUP); return 0; }
    size_t max = kind == TPOOL ? MAX_TEXTURE_DESCRIPTORS : MAX_SAMPLER_DESCRIPTORS;
    if (offset < 0 || (size_t)offset > pool->size || count <= RESERVED_DESCRIPTORS || (size_t)count > max ||
        (size_t)count > (pool->size - offset) / DESCRIPTOR_BYTES ||
        (nn_bo_address(pool->bo) + offset) % DESCRIPTOR_BYTES) { error(EINVAL); return 0; }
    size_t bytes = (size_t)count * DESCRIPTOR_BYTES;
    for (struct object *p = objects; p; p = p->next) if ((p->kind == TPOOL || p->kind == SPOOL) && p->pool == pool &&
        (size_t)offset < p->offset + p->size && p->offset < (size_t)offset + bytes) { error(ENOTSUP); return 0; }
    struct descriptor_entry *slots = calloc((size_t)count, sizeof(*slots));
    if (!slots) { error(ENOMEM); return 0; }
    struct object *o = create(key, kind); if (!o) { free(slots); return 0; }
    o->pool=pool; pool->refs++; o->device=pool->device; o->offset=offset; o->size=bytes;
    o->slots=slots; o->slot_count=count; o->cpu_software_table=1; return 1;
}
static void descriptor_pool_finalize(void *key, enum kind kind)
{
    struct object *o = find(key, kind); if (!o) return;
    if (o->refs) { error(EBUSY); return; }
    for (size_t i = 0; i < o->slot_count; ++i) {
        if (o->slots[i].pool) o->slots[i].pool->refs--;
        free(o->slots[i].source_layout);
    }
    o->pool->refs--; free(o->slots); remove_object(o);
}
static int descriptor_write(struct object *o, int id, const void *descriptor, struct descriptor_entry entry)
{
    if (cpu_descriptor_access(o)) return ENOTSUP;
    if (id < RESERVED_DESCRIPTORS || (size_t)id >= o->slot_count) return EINVAL;
    size_t offset = o->offset + (size_t)id * DESCRIPTOR_BYTES;
    if (gpu_pool_access(entry.pool,entry.offset,entry.size,"descriptor_write")) return ENOTSUP;
    unsigned char *map = (unsigned char *)nn_bo_map(o->pool->bo) + offset;
    unsigned char previous[DESCRIPTOR_BYTES]; memcpy(previous, map, sizeof(previous));
    memcpy(map, descriptor, DESCRIPTOR_BYTES);
    /* Preserve CPU_CACHED registration publication, including exact14.
     * Exact12 uses the qualified coherent CPU mapping; no GPU reads this table. */
    int e = o->pool->gpu_policy == NN_NVN_GPU_UNCACHED_UNQUALIFIED &&
            (o->pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED) ? 0 :
            nn_bo_cache(o->pool->bo, offset, DESCRIPTOR_BYTES, 0);
    if (e) { memcpy(map, previous, sizeof(previous)); return e; }
    if (entry.pool) entry.pool->refs++;
    if (o->slots[id].pool) o->slots[id].pool->refs--;
    free(o->slots[id].source_layout);
    o->slots[id] = entry; return 0;
}
#include "registered_texture_view.inc"
static void api_nvnTexturePoolRegisterTexture(const NVNtexturePool *key, int id, const NVNtexture *texture, const NVNtextureView *view)
{
    struct object *o=find(key,TPOOL), *t=find(texture,TEXTURE); if (!o || !t) return;
    if(t->texture.target==NVN_TEXTURE_TARGET_BUFFER){error(buffer_texture_register(o,id,t,view));return;}
    if (t->pool->external) { error(ENOTSUP); return; }
    struct texture_desc resolved=t->texture;struct descriptor_selection selection={0};
    int ve=t->source_layout?registered_texture_view(t,view,&selection):resolve_texture_view(t,view,&resolved);
    if(!ve)ve=texture_view_sampling_metadata(t,view,&resolved);
    if(ve){error(ve);return;}
    /* Registration publishes CPU-owned semantic metadata, including canonical
     * formats/mips/layers not yet admitted by a native GPU consumer. Preserve
     * the complete validated source layout; never demand a conversion recipe
     * merely to register it. Actual Draw/copy/target consumers retain their
     * own strict native-layout and conversion gates. */
    if (t->source_layout) {
        int e=nn_tex_source_validate(t->source_layout);if(e){error(e);return;}
    } else if (!texture_profile(&resolved)) { error(ENOTSUP); return; }
    if (o->device != t->device) { error(EINVAL); return; }
    if (gpu_pool_access(t->pool,t->offset,t->size,"nvnTexturePoolRegisterTexture")) return;
    struct texture_slot descriptor = {0x54584e4eu,(uint32_t)t->texture.format,(uint32_t)t->texture.width,
        (uint32_t)t->texture.height,(uint32_t)t->texture.stride,(uint32_t)t->size,nn_bo_address(t->bo)+t->offset};
    struct descriptor_entry entry = {.pool=t->pool,.offset=t->offset,.size=t->size,.valid=1};
    entry.snapshot.texture=resolved;entry.selection=selection;
    if(t->source_layout){entry.source_layout=malloc(sizeof(*entry.source_layout));
        if(!entry.source_layout){error(ENOMEM);return;}*entry.source_layout=*t->source_layout;}
    int e=descriptor_write(o,id,&descriptor,entry);if(e)free(entry.source_layout);error(e);
}
static void register_sampler(struct object *o, int id, struct object *s)
{
    int metadata_error=sampler_metadata(&s->sampler);if(metadata_error){error(metadata_error);return;}
    if (o->device != s->device) { error(EINVAL); return; }
    if (s->kind==SB && s->sampler_device_key!=o->device->key) { error(EINVAL); return; }
    struct sampler_slot descriptor = {0x534e4e53u,(uint32_t)s->sampler.min,(uint32_t)s->sampler.mag,
        (uint32_t)s->sampler.wrap[0] | ((uint32_t)s->sampler.wrap[1]<<8) | ((uint32_t)s->sampler.wrap[2]<<16),
        (uint32_t)s->sampler.compare | ((uint32_t)s->sampler.compare_func<<8),
        s->sampler.lod_min,s->sampler.lod_max,s->sampler.lod_bias};
    struct descriptor_entry entry = {.valid=1}; entry.snapshot.sampler=s->sampler;
    error(descriptor_write(o,id,&descriptor,entry));
}
static void api_nvnSamplerPoolRegisterSampler(const NVNsamplerPool *key, int id, const NVNsampler *sampler)
{ struct object *o=find(key,SPOOL), *s=find(sampler,SAMPLER); if (o && s) register_sampler(o,id,s); }
static void api_nvnSamplerPoolRegisterSamplerBuilder(const NVNsamplerPool *key, int id, const NVNsamplerBuilder *builder)
{ struct object *o=find(key,SPOOL), *s=find(builder,SB); if (o && s) register_sampler(o,id,s); }
#define DESCRIPTOR_POOL_APIS(prefix, type, kind) \
    static NVNboolean api_##prefix##Initialize(type *key, const NVNmemoryPool *p, ptrdiff_t off, int n) { \
        return descriptor_pool_initialize(key,p,off,n,kind); } \
    static void api_##prefix##Finalize(type *key) { descriptor_pool_finalize(key,kind); } \
    static const NVNmemoryPool *api_##prefix##GetMemoryPool(const type *key) { \
        struct object *o=find(key,kind); return o?o->pool->key:NULL; } \
    static ptrdiff_t api_##prefix##GetMemoryOffset(const type *key) { struct object *o=find(key,kind); return o?(ptrdiff_t)o->offset:0; } \
    static int api_##prefix##GetSize(const type *key) { struct object *o=find(key,kind); return o?(int)o->slot_count:0; }
DESCRIPTOR_POOL_APIS(nvnTexturePool, NVNtexturePool, TPOOL)
DESCRIPTOR_POOL_APIS(nvnSamplerPool, NVNsamplerPool, SPOOL)
#undef DESCRIPTOR_POOL_APIS

static struct object *address_pool(struct object *device, uint64_t address, size_t size)
{
    for (struct object *p = objects; p; p = p->next) if (p->kind == POOL && p->device == device) {
        if (p->external) continue; /* no raw address/UBO/SSBO/clear aliases */
        uint64_t base = nn_bo_address(p->bo);
        if (address >= base && address - base <= p->size && size <= p->size - (address - base)) return p;
    }
    return NULL;
}
int nn_nvn_get_program_source_info(const NVNprogram *key, NVNbufferAddress address,
                                  struct nn_nvn_program_source_info *out)
{
    if (!key || !out) return EINVAL;
    /* find() sets sticky diagnostics; a read-only observer must not call it. */
    struct object *program = NULL;
    for (struct object *o = objects; o; o = o->next)
        if (o->key == key && o->kind == PROGRAM) { program = o; break; }
    if (!program) return ENOENT;
    struct object *pool = address_pool(program->device, address, 1);
    if (!pool) return ENOENT;
    size_t offset = address - nn_bo_address(pool->bo);
    struct nn_nvn_program_source_info info = {
        .pool = pool->key, .flags = pool->flags, .policy = pool->gpu_policy,
        .offset = offset, .available = pool->size - offset,
        .cpu_readable = pool->gpu_policy == NN_NVN_GPU_POOL_CACHED &&
            !(pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) && nn_bo_map(pool->bo) != NULL
    };
    *out = info;
    return 0;
}
int nn_nvn_register_compute(NVNdevice *key, const struct nn_nvn_compiled_cs *compiled)
{
    struct object *device = find(key, DEVICE); if (!device) return EINVAL;
    if (!compiled || !compiled->image || compiled->image->schema != 1 ||
        compiled->image->chip != nn_device_chip(device->gpu) || !compiled->source_code ||
        !compiled->image->code || !compiled->image->code_bytes || compiled->image->code_bytes > 65536 ||
        compiled->image->code_bytes % 4 || !compiled->image->commands ||
        !compiled->image->command_words || compiled->image->command_words > 1024 ||
        (compiled->image->patch_count && !compiled->image->patches) ||
        compiled->image->ssbo_slots > 16 || compiled->image->height_shift >= 32 || compiled->image->offset_shift >= 32 ||
        !compiled->source_control || !compiled->source_bytes || compiled->source_bytes > 1048576 ||
        !compiled->control_bytes || compiled->control_bytes > 1048576 ||
        compiled->cbuf_count > 16 || compiled->sbuf_count > 16 ||
        (compiled->cbuf_count && !compiled->cbufs) || (compiled->sbuf_count && !compiled->sbufs)) return EINVAL;
    uint32_t cbuf_mask = 0, sbuf_mask = 0;
    for (size_t i = 0; i < compiled->cbuf_count; i++) {
        struct nn_nvn_cbuf map = compiled->cbufs[i];
        if (map.slot < 3 || map.slot > 16 || map.binding >= 16 || (cbuf_mask & (1u << map.binding))) return EINVAL;
        cbuf_mask |= 1u << map.binding;
    }
    for (size_t i = 0; i < compiled->sbuf_count; i++) {
        struct nn_nvn_sbuf map = compiled->sbufs[i];
        if (map.cbuf_slot >= 16 || map.binding >= compiled->image->ssbo_slots || map.cbuf_word > 16380 ||
            (sbuf_mask & (1u << map.binding))) return EINVAL;
        sbuf_mask |= 1u << map.binding;
    }
    struct cache *entry = calloc(1, sizeof(*entry)); if (!entry) return ENOMEM;
    entry->compiled = compiled; entry->next = device->cache; device->cache = entry; return 0;
}
static int graphics_profile(const struct nn_gfx_image *image)
{
    /* NVN modes require explicit schema3 scalar-state metadata. Legacy raw
     * images remain usable by the lower-level raw encoder, never as default
     * NVN0/0 state through this API. Full packet validation is in encode_state. */
    if (!image || image->schema != 3 || !image->state_patches ||
        image->state_patch_count != NN_GFX_STATE_KIND_COUNT ||
        image->width != 64 || image->height != 64 || image->target_pitch != 256 ||
        image->target_layer_bytes != 16384 || image->target_bytes != 16384 || image->target_alignment != 64 ||
        image->code_alignment != 128 || image->matrix_bytes != 64 || image->zero_bytes != 4096 ||
        !image->vs_code || !image->fs_code || !image->vs_code_bytes || !image->fs_code_bytes ||
        image->vs_code_bytes > 65536 || image->fs_code_bytes > 65536 ||
        image->vs_code_bytes % 4 || image->fs_code_bytes % 4 ||
        !image->commands || !image->command_words || image->command_words > 8192 ||
        !image->patches || !image->patch_count || image->patch_count > image->command_words / 2) return 0;
    uint32_t state_seen = 0;
    for (unsigned i = 0; i < image->state_patch_count; ++i) {
        const struct nn_gfx_state_patch *p = image->state_patches + i;
        if (p->kind >= NN_GFX_STATE_KIND_COUNT || (state_seen & (1u << p->kind)) ||
            !p->word || p->word >= image->command_words) return 0;
        state_seen |= 1u << p->kind;
    }
    if (state_seen != (1u << NN_GFX_STATE_KIND_COUNT) - 1u ||
        image->sequence_bytes != (image->chip == 0x06050002 ? 4u : 0u)) return 0;
    if (image->profile == NN_GFX_COLOR)
        return image->vertex_stride == 24 &&
            image->vertex_bytes == 72 && !image->fs_uniform_bytes && !image->fs_uniform_binding &&
            !image->texture_width && !image->texture_height && !image->texture_pitch &&
            !image->texture_layer_bytes && !image->texture_bytes && !image->texture_alignment &&
            !image->texture_slots && !image->texture_descriptors && !image->texture_descriptor_words &&
            !image->samplers && !image->sampler_words && !image->texture_patches &&
            !image->texture_patch_count && !image->texture_address_mask;
    return image->profile == NN_GFX_TEXTURE_2D_NORMALIZED &&
        image->vertex_stride == 32 && image->vertex_bytes == 96 &&
        image->fs_uniform_bytes == 64 && image->fs_uniform_binding == 2 &&
        image->texture_width == 2 && image->texture_height == 2 && image->texture_pitch == 256 &&
        image->texture_layer_bytes == 4096 && image->texture_bytes == 4096 && image->texture_alignment == 64 &&
        image->texture_slots == 2 && image->texture_descriptors && image->texture_descriptor_words == 32 &&
        image->samplers && image->sampler_words == 8 && image->texture_patches && image->texture_patch_count &&
        image->texture_patch_count <= 16 && image->texture_address_mask &&
        image->sequence_bytes == (image->chip == 0x06050002 ? 4u : 0u);
}
static int sampled_mapping(const struct nn_nvn_compiled_graphics *compiled)
{
    if (compiled->image->profile == NN_GFX_COLOR) return !compiled->sampled_count;
    if (compiled->sampled_count != 1 || !compiled->sampled) return 0;
    const struct nn_nvn_sampled *m = compiled->sampled;
    /* Qualified direct, statically resolved driver-CB2 operands. The domains,
     * API slots, pool IDs and native indices are four independent quantities. */
    return m->stage == NVN_SHADER_STAGE_FRAGMENT && m->native_texture_index == 1 && m->native_sampler_index == 1 &&
        m->texture_domain == NN_NVN_HANDLE_SEPARATE && m->sampler_domain == NN_NVN_HANDLE_SEPARATE &&
        m->texture_slot < 32 && m->sampler_slot < 32 && m->texture_cbuf == 2 && m->sampler_cbuf == 2 &&
        m->texture_word == (0x168u + 8u * m->texture_slot) / 4 &&
        m->sampler_word == (0x568u + 8u * m->sampler_slot) / 4;
}
static int graphics_source(const struct nn_nvn_graphics_stage *stage)
{
    return stage->source_code && stage->source_control && stage->source_bytes && stage->control_bytes &&
        stage->source_bytes <= 1048576 && stage->control_bytes <= 1048576;
}
int nn_nvn_register_graphics(NVNdevice *key, const struct nn_nvn_compiled_graphics *compiled)
{
    struct object *device = find(key, DEVICE); if (!device) return EINVAL;
    if (!compiled || !graphics_profile(compiled->image) ||
        compiled->image->chip != nn_device_chip(device->gpu) ||
        compiled->image->gmem_bytes != nn_device_gmem_bytes(device->gpu) ||
        !graphics_source(&compiled->vs) || !graphics_source(&compiled->fs) ||
        compiled->vs.cbuf_count != 1 || !compiled->vs.cbufs ||
        compiled->vs.cbufs[0].slot != 3 || compiled->vs.cbufs[0].binding != 1 ||
        !sampled_mapping(compiled)) return ENOTSUP;
    int textured = compiled->image->profile == NN_GFX_TEXTURE_2D_NORMALIZED;
    if (textured ? (compiled->fs.cbuf_count != 1 || !compiled->fs.cbufs ||
        compiled->fs.cbufs[0].slot != 3 || compiled->fs.cbufs[0].binding != 2) : !!compiled->fs.cbuf_count) return ENOTSUP;
    uint32_t seen = 0;
    const uint32_t sizes[NN_GFX_PATCH_KIND_COUNT] = {compiled->image->vs_code_bytes, compiled->image->fs_code_bytes,
        16384, compiled->image->vertex_bytes, 64, 4096, 64, 128, 32, 4096, 4};
    uint32_t required = (1u << NN_GFX_ZERO) - 1;
    if (textured) required |= (1u << NN_GFX_FS_UNIFORM) | (1u << NN_GFX_TEXTURE_DESCRIPTORS) |
        (1u << NN_GFX_SAMPLERS) | (1u << NN_GFX_TEXTURE);
    if (compiled->image->sequence_bytes) required |= 1u << NN_GFX_SEQUENCE;
    for (unsigned i = 0; i < compiled->image->patch_count; ++i) {
        const struct nn_gfx_patch *p = compiled->image->patches + i;
        if (p->kind >= NN_GFX_PATCH_KIND_COUNT ||
            (!textured && p->kind > NN_GFX_ZERO && p->kind != NN_GFX_SEQUENCE) ||
            p->word >= compiled->image->command_words - 1 || !p->bytes ||
            p->offset > sizes[p->kind] || p->bytes > sizes[p->kind] - p->offset) return EINVAL;
        if (p->kind == NN_GFX_SEQUENCE &&
            (compiled->image->sequence_bytes != 4 || p->offset || p->bytes != 4)) return EINVAL;
        seen |= 1u << p->kind;
    }
    if (textured) {
        for (unsigned i = 0; i < compiled->image->texture_patch_count; ++i) {
            const struct nn_gfx_patch *p = compiled->image->texture_patches + i;
            if (p->kind != NN_GFX_TEXTURE || p->word >= compiled->image->texture_descriptor_words - 1 ||
                !p->bytes || p->offset > sizes[p->kind] || p->bytes > sizes[p->kind] - p->offset) return EINVAL;
            seen |= 1u << NN_GFX_TEXTURE;
        }
    }
    if ((seen & required) != required || !!(seen & (1u << NN_GFX_SEQUENCE)) != !!compiled->image->sequence_bytes) return EINVAL;
    struct graphics_cache *entry = calloc(1, sizeof(*entry)); if (!entry) return ENOMEM;
    entry->compiled = compiled; entry->next = device->graphics_cache; device->graphics_cache = entry; return 0;
}
static int program_release_code(struct object *o)
{
    if (o->bo) { int e = nn_bo_destroy(o->bo); if (e) return e; o->bo = NULL; }
    if (o->fs_bo) { int e = nn_bo_destroy(o->fs_bo); if (e) return e; o->fs_bo = NULL; }
    return 0;
}
static int matches_shader(struct object *device, const NVNshaderData *shader,
                          const void *code, size_t code_bytes, const void *control, size_t control_bytes)
{
    if (!shader->control) return 0;
    struct object *pool = address_pool(device, shader->data, code_bytes);
    if (!pool) return 0;
    size_t offset = shader->data - nn_bo_address(pool->bo);
    if (gpu_pool_access(pool,offset,code_bytes,"nvnProgramSetShaders")) return 0;
    return !memcmp(shader->control, control, control_bytes) &&
        !memcmp((const char *)nn_bo_map(pool->bo) + offset, code, code_bytes);
}
#include "nvn_pipeline_source.inc"
static NVNboolean api_nvnProgramInitialize(NVNprogram *key, NVNdevice *d)
{
    struct object *device = find(d, DEVICE); if (!device) return 0;
    struct object *o = create(key, PROGRAM); if (!o) return 0;
    o->device = device; device->refs++; return 1;
}
static NVNboolean api_nvnProgramSetShaders(NVNprogram *key, int count, const NVNshaderData *shaders)
{
    struct object *o = find(key, PROGRAM); if (!o) return 0;
#if NN_NVN_GRAPHICS_SOURCE_CACHE
    if(count==2&&o->device->pipeline_archive) {
        if(o->refs){error(EBUSY);return 0;}
        struct nn_pipeline_source_program prepared;int e;
#if NN_NVN_SHADER_SOURCE_CATALOGUE
        e=o->device->shader_sources?pipeline_prepare_bound_source(o,shaders,&prepared):pipeline_prepare_program(o,shaders,&prepared);
#else
        e=pipeline_prepare_program(o,shaders,&prepared);
#endif
        if(e){error(e);return 0;}
        if((e=program_release_code(o))){
#if NN_NVN_SHADER_SOURCE_CATALOGUE
            nn_shader_source_program_destroy((struct nn_shader_source_program*)prepared.binding);
#endif
            error(e);return 0;
        }
#if NN_NVN_SHADER_SOURCE_CATALOGUE
        nn_shader_source_program_destroy((struct nn_shader_source_program*)o->pipeline_source.binding);
#endif
        o->compiled=NULL;o->graphics=NULL;o->pipeline_source=prepared;return 1;
    }
#endif
    if ((count != 1 && count != 2) || !shaders || !shaders[0].control ||
        (count == 2 && !shaders[1].control)) { error(ENOTSUP); return 0; }
    /* Source identity matching reads private CPU bytes. GPU-only sources
     * need a separately qualified wait/invalidate/read protocol; public
     * InvalidateMappedRange is intentionally a NOP for those pools. */
    for (int i = 0; i < count; ++i) {
        struct object *pool = address_pool(o->device, shaders[i].data, 1);
        if (pool && gpu_pool_access(pool,shaders[i].data-nn_bo_address(pool->bo),1,"nvnProgramSetShaders")) return 0;
        if (pool && (pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS)) { error(ENOTSUP); return 0; }
    }
    if (o->refs) { error(EBUSY); return 0; }
    const struct nn_nvn_compiled_cs *found = NULL;
    for (struct cache *c = count == 1 ? o->device->cache : NULL; c; c = c->next) {
        const struct nn_nvn_compiled_cs *compiled = c->compiled;
        if (matches_shader(o->device, shaders, compiled->source_code, compiled->source_bytes,
                           compiled->source_control, compiled->control_bytes)) {
            found = compiled; break;
        }
    }
    const struct nn_nvn_compiled_graphics *graphics = NULL;
    for (struct graphics_cache *c = count == 2 ? o->device->graphics_cache : NULL; c; c = c->next) {
        const struct nn_nvn_graphics_stage *vs = &c->compiled->vs, *fs = &c->compiled->fs;
        for (unsigned first = 0; first < 2; ++first)
            if (matches_shader(o->device, shaders + first, vs->source_code, vs->source_bytes,
                               vs->source_control, vs->control_bytes) &&
                matches_shader(o->device, shaders + (1 - first), fs->source_code, fs->source_bytes,
                               fs->source_control, fs->control_bytes)) { graphics = c->compiled; break; }
        if (graphics) break;
    }
    if (!found && !graphics) { error(ENOTSUP); return 0; }
    nn_bo *code = NULL, *fs_code = NULL;
    int e = nn_bo_alloc(o->device->gpu, found ? found->image->code_bytes : graphics->image->vs_code_bytes, &code);
    if (e) { error(e); return 0; }
    if (graphics && (e = nn_bo_alloc(o->device->gpu, graphics->image->fs_code_bytes, &fs_code))) {
        nn_bo_destroy(code); error(e); return 0;
    }
    if (found) memcpy(nn_bo_map(code), found->image->code, found->image->code_bytes);
    else {
        memcpy(nn_bo_map(code), graphics->image->vs_code, graphics->image->vs_code_bytes);
        memcpy(nn_bo_map(fs_code), graphics->image->fs_code, graphics->image->fs_code_bytes);
    }
    if ((e = program_release_code(o))) {
        nn_bo_destroy(code); if (fs_code) nn_bo_destroy(fs_code); error(e); return 0;
    }
#if NN_NVN_SHADER_SOURCE_CATALOGUE
    nn_shader_source_program_destroy((struct nn_shader_source_program*)o->pipeline_source.binding);
#endif
    o->bo = code; o->fs_bo = fs_code; o->compiled = found; o->graphics = graphics; o->pipeline_source=(struct nn_pipeline_source_program){0}; return 1;
}
static void api_nvnProgramFinalize(NVNprogram *key)
{
    struct object *o = find(key, PROGRAM); if (!o) return;
    if (o->refs) { error(EBUSY); return; }
    int e = program_release_code(o);
    if (e) { error(e); return; }
#if NN_NVN_SHADER_SOURCE_CATALOGUE
    nn_shader_source_program_destroy((struct nn_shader_source_program*)o->pipeline_source.binding);
#endif
    o->device->refs--; remove_object(o);
}
static NVNboolean api_nvnCommandBufferInitialize(NVNcommandBuffer *key, NVNdevice *d)
{
    struct object *dev = find(d, DEVICE); if (!dev) return 0;
    struct object *o = create(key, COMMAND); if (!o) return 0;
    o->record = calloc(1, sizeof(*o->record));
    if (!o->record) { error(ENOMEM); remove_object(o); return 0; }
    nn_render_state_defaults(&o->record->render);
    o->device = dev; dev->refs++; return 1;
}
static int add_resource(struct handle *,struct object *);
static void queue_family_handle_cleanup(struct handle *);
static void copy_commands_handle_cleanup(struct handle *);
static void free_chunks(struct command_chunk *c)
{while(c){struct command_chunk*n=c->next;c->pool->refs--;free(c);c=n;}}
static int command_close_chunk(struct object *o)
{
    struct recording*r=o->record;
    size_t bytes=r->used-r->start;if(!bytes)return 0;
    if(r->pool->refs==SIZE_MAX)return EOVERFLOW;
    struct command_chunk*c=calloc(1,sizeof(*c));if(!c)return ENOMEM;
    /* CPU command spans need their own pins; immutable parent staging means
     * they are not GPU resources to inherit into an independent Copy. */
    r->pool->refs++;
    *c=(struct command_chunk){.pool=r->pool,.offset=r->offset+r->start,.bytes=bytes};
    if(r->current->chunks_tail)r->current->chunks_tail->next=c;else r->current->chunks=c;
    r->current->chunks_tail=c;r->logical_bytes+=bytes;
    /* This span now belongs to chunks. Keep the remaining live arena span
     * empty so semantic offsets never count it again at EndRecording. */
    r->start=r->used;return 0;
}
static int command_reserve(struct object *o,size_t bytes)
{
    struct recording*r=o->record;
    if(!r->active || r->in_callback)return EINVAL;
    if(r->pool && r->used<=r->capacity && bytes<=r->capacity-r->used)return 0;
    if(!r->memory_callback)return ENOSPC;
    r->in_callback=1;
    r->memory_callback((NVNcommandBuffer*)o->key,NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY,bytes,r->memory_callback_user);
    r->in_callback=0;
    if(last_error)return last_error;
    return r->pool&&r->used<=r->capacity&&bytes<=r->capacity-r->used?0:ENOSPC;
}
static int control_reserve(struct object *o,size_t bytes)
{
    struct recording*r=o->record;
    if(r->control&&r->control_used<=r->control_capacity&&bytes<=r->control_capacity-r->control_used)return 0;
    if(!r->memory_callback || r->in_callback)return ENOSPC;
    r->in_callback=1;
    r->memory_callback((NVNcommandBuffer*)o->key,NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_CONTROL_MEMORY,bytes,r->memory_callback_user);
    r->in_callback=0;
    if(last_error)return last_error;
    return r->control&&r->control_used<=r->control_capacity&&bytes<=r->control_capacity-r->control_used?0:ENOSPC;
}
static void api_nvnCommandBufferSetMemoryCallback(NVNcommandBuffer*k,PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC cb)
{struct object*o=find(k,COMMAND);if(o)o->record->memory_callback=cb;}
static void api_nvnCommandBufferSetMemoryCallbackData(NVNcommandBuffer*k,void*user)
{struct object*o=find(k,COMMAND);if(o)o->record->memory_callback_user=user;}
static NVNboolean api_nvnCommandBufferIsRecording(const NVNcommandBuffer*k)
{struct object*o=find(k,COMMAND);return o?!!o->record->active:0;}
static void api_nvnCommandBufferAddCommandMemory(NVNcommandBuffer *key, const NVNmemoryPool *p, ptrdiff_t offset, size_t size)
{
    struct object *o = find(key, COMMAND), *pool = find(p, POOL); if (!o || !pool) return;
    /* Recording and enqueue copy/patch CPU command bytes. Keep GPU-only
     * pools restricted to resource use until an arena protocol is qualified. */
    if (pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) { error(ENOTSUP); return; }
    struct recording *r = o->record;
    if ((r->active && !r->in_callback) || pool->device != o->device || offset < 0 || offset % 4 || size % 4 ||
        (size_t)offset > pool->size || size > pool->size - offset) { error(EINVAL); return; }
    if (gpu_pool_access(pool,(size_t)offset,size,"nvnCommandBufferAddCommandMemory")) return;
    if(r->active){
        size_t begin=(size_t)offset,end=begin+size;
        for(struct command_chunk*c=r->current->chunks;c;c=c->next)
            if(c->pool==pool && begin<c->offset+c->bytes && c->offset<end){error(ENOTSUP);return;}
        if(r->pool==pool && begin<r->offset+r->used && r->offset+r->start<end && r->used!=r->start){error(ENOTSUP);return;}
        int e=command_close_chunk(o);if(e){error(e);return;}
    }
    if (r->pool) r->pool->refs--;
    r->pool = pool; pool->refs++; r->offset = offset; r->capacity = size; r->used = 0;r->start=0;
}
static void api_nvnCommandBufferAddControlMemory(NVNcommandBuffer *key, void *memory, size_t size)
{
    struct object *o = find(key, COMMAND); if (!o) return;
    if ((o->record->active && !o->record->in_callback) || !memory || (uintptr_t)memory % _Alignof(struct control)) { error(EINVAL); return; }
    o->record->control = memory; o->record->control_capacity = size; o->record->control_used = 0;
}
#define MEMORY_QUERY(name, expression) \
    static size_t api_##name(const NVNcommandBuffer *key) { \
        struct object *o = find(key, COMMAND); if (!o) return 0; \
        struct recording *r = o->record; return (expression); }
MEMORY_QUERY(nvnCommandBufferGetCommandMemorySize, r->capacity)
MEMORY_QUERY(nvnCommandBufferGetCommandMemoryUsed, r->used)
MEMORY_QUERY(nvnCommandBufferGetCommandMemoryFree, r->capacity - r->used)
MEMORY_QUERY(nvnCommandBufferGetControlMemorySize, r->control_capacity)
MEMORY_QUERY(nvnCommandBufferGetControlMemoryUsed, r->control_used)
MEMORY_QUERY(nvnCommandBufferGetControlMemoryFree, r->control_capacity - r->control_used)
#undef MEMORY_QUERY
static void command_copy_free(struct command_copy *);
#if NN_NVN_DEPTH_TARGETS
static int command_copy_depth_clone(struct handle *,const struct handle *,size_t);
static void command_copy_depth_transfer(struct handle *,struct handle *);
#endif
static int command_copy_job_known(const struct handle *,const struct job *);
static int command_copy_capture_kind(const struct handle *,size_t,size_t,unsigned *);
static int command_project_copy(struct handle *,const struct command_copy *,const struct nn_command_state *,size_t);
static int command_project_copies(struct handle *,const struct handle *,const struct nn_command_leaf *,const struct nn_command_state *);
#include "nvn_command_sequence.inc"
static void api_nvnCommandBufferBeginRecording(NVNcommandBuffer *key)
{
    struct object *o = find(key, COMMAND); if (!o) return;
    struct recording *r = o->record;
    if(r->active || r->in_callback){error(EINVAL);return;}
    int e=control_reserve(o,sizeof(struct control));if(e){error(e);return;}
    r->current = calloc(1, sizeof(*r->current)); if (!r->current) { error(ENOMEM); return; }
    /* The recording CommandBuffer owns the device until End/abort. Borrowed
     * jobs may retire during abort, before this becomes a published handle. */
    r->current->device=o->device;
    e=nn_command_sequence_create(r->current,&command_owner_ops,o->device,&r->current->call_sequence);
    if(e){free(r->current);r->current=NULL;error(e);return;}
    r->logical_bytes=0;r->debug_depth=0;r->start = r->used;r->active = 1;
    if((e=command_reserve(o,8))){nn_command_sequence_destroy(r->current->call_sequence);free(r->current);r->current=NULL;r->active=0;error(e);}
}
static int add_resource(struct handle *h, struct object *p)
{
    for (size_t i = 0; i < h->count; i++) if (h->resources[i] == p) return 0;
    void *new_list = realloc(h->resources, (h->count + 1) * sizeof(*h->resources));
    if (!new_list) return ENOMEM;
    h->resources = new_list; h->resources[h->count++] = p; p->refs++; return 0;
}
static int add_cpu_resource(struct handle *h, struct object *p)
{
    int e=cpu_descriptor_access(p); if (e) return e;
    if (!p) return EINVAL;
    for (size_t i=0;i<h->cpu_count;i++) if (h->cpu_resources[i]==p) return 0;
    if (h->cpu_count >= 65535) return E2BIG;
    void *list=realloc(h->cpu_resources,(h->cpu_count+1)*sizeof(*h->cpu_resources));
    if (!list) return ENOMEM;
    h->cpu_resources=list; h->cpu_resources[h->cpu_count++]=p; p->refs++; return 0;
}
static void api_nvnCommandBufferBindProgram(NVNcommandBuffer *key, const NVNprogram *p, int stages)
{
    struct object *o = find(key, COMMAND), *program = find(p, PROGRAM); if (!o || !program) return;
    if (!o->record->active || program->device != o->device || (!program->bo&&!program->pipeline_source.archive)) { error(ENOTSUP); return; }
    struct object **binding;
    if (stages == NVN_SHADER_STAGE_COMPUTE_BIT && program->compiled) binding = &o->record->program;
    else if (stages == (NVN_SHADER_STAGE_VERTEX_BIT | NVN_SHADER_STAGE_FRAGMENT_BIT) &&
             ((program->graphics && program->fs_bo)||program->pipeline_source.archive)) binding = &o->record->graphics_program;
    else { error(ENOTSUP); return; }
    if(command_capture_program(o,program,(unsigned)stages))return;
    if (*binding) (*binding)->refs--;
    *binding = program; program->refs++;
}
static void bind_buffers(NVNcommandBuffer *key, NVNshaderStage stage, int first, int count,
                         const NVNbufferRange *ranges, int storage)
{
    struct object *o = find(key, COMMAND); if (!o) return;
    int slots = storage ? 16 : 14;
    if (!o->record->active || (stage != NVN_SHADER_STAGE_COMPUTE &&
        ((storage&&!NN_NVN_GRAPHICS_STORAGE) || (stage != NVN_SHADER_STAGE_VERTEX && stage != NVN_SHADER_STAGE_FRAGMENT)))) {
        error(ENOTSUP); return;
    }
    if (first < 0 || count < 0 || first > slots || count > slots - first || (count && !ranges)) { error(EINVAL); return; }
    struct bound proposed[16] = {{0}};
    // Validate the complete range before replacing any live binding.
    for (int i = 0; i < count; i++) {
        uint64_t address = ranges[i].address; size_t size = ranges[i].size;
        if ((!address && !size) || (!storage && stage!=NVN_SHADER_STAGE_COMPUTE && !size)) continue;
        if (!address || !size) { error(EINVAL); return; }
        if (storage ? (size > UINT32_MAX || address % 4 || size % 4) :
                      (size > 65536 || address % 16 || size % 16)) { error(ENOTSUP); return; }
        struct object *pool = address_pool(o->device, address, size);
        if (!pool) { error(EINVAL); return; }
        /* Binding publishes CPU metadata. Only an actual Draw/Dispatch
         * consumer qualifies GPU cache/import policy for this range. */
        proposed[i] = (struct bound){.pool=pool, .offset=address-nn_bo_address(pool->bo), .size=size, .address=address};
    }
    struct bound *bindings = stage != NVN_SHADER_STAGE_COMPUTE ? (storage?o->record->graphics_storage[stage]:o->record->graphics_ubos[stage]) :
        storage ? o->record->ssbos : o->record->ubos;
    struct nn_command_binding values[16];for(int i=0;i<count;i++)values[i]=command_binding(proposed[i].pool,proposed[i].offset,proposed[i].size,proposed[i].address);
    if(count&&command_capture_bindings(o,storage?NN_COMMAND_SSBO:NN_COMMAND_UBO,(unsigned)stage,(unsigned)first,(unsigned)count,values))return;
    for (int i = 0; i < count; i++) {
        struct bound *bound = bindings + first + i;
        if (bound->pool) bound->pool->refs--;
        *bound = proposed[i]; if (bound->pool) bound->pool->refs++;
    }
}
static void api_nvnCommandBufferBindUniformBuffer(NVNcommandBuffer *key, NVNshaderStage stage, int index,
                                                NVNbufferAddress address, size_t size)
{ NVNbufferRange range = {address, size}; bind_buffers(key, stage, index, 1, &range, 0); }
static void api_nvnCommandBufferBindUniformBuffers(NVNcommandBuffer *key, NVNshaderStage stage, int first, int count,
                                                 const NVNbufferRange *ranges)
{ bind_buffers(key, stage, first, count, ranges, 0); }
static void api_nvnCommandBufferBindStorageBuffer(NVNcommandBuffer *key, NVNshaderStage stage, int index,
                                                NVNbufferAddress address, size_t size)
{ NVNbufferRange range = {address, size}; bind_buffers(key, stage, index, 1, &range, 1); }
static void api_nvnCommandBufferBindStorageBuffers(NVNcommandBuffer *key, NVNshaderStage stage, int first, int count,
                                                 const NVNbufferRange *ranges)
{ bind_buffers(key, stage, first, count, ranges, 1); }
/* Small NVN vertex states are copied by value by callers. Keep a bounded
 * inline encoding; recording snapshots these bytes, never a state pointer. */
_Static_assert(sizeof(NVNvertexAttribState) == sizeof(uint32_t), "vertex attribute ABI size");
_Static_assert(sizeof(NVNvertexStreamState) == sizeof(struct vertex_stream), "vertex stream ABI size");
static int pipeline_format_metadata(NVNformat);
static uint32_t attribute_read(const NVNvertexAttribState *key)
{ uint32_t value; memcpy(&value, key, sizeof(value)); return value; }
static void api_nvnVertexAttribStateSetDefaults(NVNvertexAttribState *key)
{ if (!key) { error(EINVAL); return; } memset(key, 0, sizeof(*key)); }
static void api_nvnVertexAttribStateSetFormat(NVNvertexAttribState *key, NVNformat format, ptrdiff_t offset)
{
    if (!key) { error(EINVAL); return; }
    if (!pipeline_format_metadata(format) || offset<0 || offset>65535) { error(EINVAL); return; }
    uint32_t value = (attribute_read(key) & 0xff000000u) | (uint32_t)format | ((uint32_t)offset << 8);
    memcpy(key, &value, sizeof(value));
}
static void api_nvnVertexAttribStateSetStreamIndex(NVNvertexAttribState *key, int index)
{
    if (!key) { error(EINVAL); return; }
    if(index<0||index>=16){error(EINVAL);return;}
    uint32_t value = (attribute_read(key) & 0x00ffffffu) | ((uint32_t)index<<24); memcpy(key, &value, sizeof(value));
}
static void api_nvnVertexStreamStateSetDefaults(NVNvertexStreamState *key)
{ if (!key) { error(EINVAL); return; } memset(key, 0, sizeof(*key)); }
static void api_nvnVertexStreamStateSetStride(NVNvertexStreamState *key, ptrdiff_t stride)
{
    if (!key) { error(EINVAL); return; }
    if(stride<0||(uint64_t)stride>UINT32_MAX){error(EINVAL);return;}
    struct vertex_stream state; memcpy(&state, key, sizeof(state)); state.stride = stride; memcpy(key, &state, sizeof(state));
}
static void api_nvnVertexStreamStateSetDivisor(NVNvertexStreamState *key, int divisor)
{
    if (!key) { error(EINVAL); return; }
    if(divisor<0){error(EINVAL);return;}
    struct vertex_stream state; memcpy(&state, key, sizeof(state)); state.divisor = divisor; memcpy(key, &state, sizeof(state));
}
static struct object *recording_command(NVNcommandBuffer *key)
{
    struct object *o = find(key, COMMAND); if (!o) return NULL;
    if (!o->record->active) { error(EINVAL); return NULL; }
    return o;
}
#include "nvn_render_state.inc"
static void set_descriptor_pool(NVNcommandBuffer *key, const void *p, enum kind kind)
{
    struct object *o = recording_command(key); if (!o) return;
    struct object *pool = p ? find(p, kind) : NULL;
    if (p && !pool) return;
    if (pool && pool->device != o->device) { error(EINVAL); return; }
    if (cpu_descriptor_access(pool)) return;
    struct object **binding = kind == TPOOL ? &o->record->texture_pool : &o->record->sampler_pool;
    struct nn_command_binding value=command_binding(pool,0,0,0);
    if(command_capture_bindings(o,kind==TPOOL?NN_COMMAND_TEXTURE_POOL:NN_COMMAND_SAMPLER_POOL,0,0,1,&value))return;
    if (*binding) (*binding)->refs--;
    *binding = pool; if (pool) pool->refs++;
}
static void api_nvnCommandBufferSetTexturePool(NVNcommandBuffer *key, const NVNtexturePool *pool)
{ set_descriptor_pool(key,pool,TPOOL); }
static void api_nvnCommandBufferSetSamplerPool(NVNcommandBuffer *key, const NVNsamplerPool *pool)
{ set_descriptor_pool(key,pool,SPOOL); }
static void bind_handles(NVNcommandBuffer *key, NVNshaderStage stage, int first, int count, const void *values, int domain, int plural)
{
    struct object *o=recording_command(key); if (!o) return;
    if ((stage != NVN_SHADER_STAGE_VERTEX && stage != NVN_SHADER_STAGE_FRAGMENT) ||
        first < 0 || count < 0 || first > 32 || count > 32-first || (count && !values)) { error(ENOTSUP); return; }
    uint64_t proposed[32] = {0}, guest[32] = {0};
    for (int i=0; i<count; ++i) {
        uint64_t v; memcpy(&v,(const char *)values + (size_t)i*sizeof(v),sizeof(v));
        uint32_t payload;
        int e = texture_handle_payload(v, domain, &payload);
        if (e) { error(e); return; }
        proposed[i] = payload;
        guest[i] = v;
    }
    uint64_t *binding = domain == 0 ? o->record->combined[stage] : domain == 1 ?
        o->record->separate_textures[stage] : o->record->separate_samplers[stage];
    struct nn_command_binding deltas[32]={{0}};for(int i=0;i<count;i++){deltas[i].address=proposed[i];deltas[i].guest=guest[i];deltas[i].offset=plural?(uint32_t)(guest[i]>>32):0;deltas[i].flags=plural?(NN_COMMAND_BINDING_HIGH_DEFINED|NN_COMMAND_BINDING_LAST_PLURAL):NN_COMMAND_BINDING_PRESERVE_HIGH;}
    enum nn_command_binding_domain state_domain=domain==0?NN_COMMAND_COMBINED_TEXTURE:domain==1?NN_COMMAND_SEPARATE_TEXTURE:NN_COMMAND_SEPARATE_SAMPLER;
    if(count&&command_capture_bindings(o,state_domain,(unsigned)stage,(unsigned)first,(unsigned)count,deltas))return;
    memcpy(binding+first,proposed,(size_t)count*sizeof(*proposed));
    struct texture_handle_record *carrier = &o->record->handle_carriers[domain];
    for (int i=0; i<count; ++i) {
        unsigned slot = (unsigned)(first+i);
        uint32_t bit = UINT32_C(1) << slot;
        carrier->guest[stage][slot] = guest[i];
        carrier->low_defined[stage] |= bit;
        if (plural) {
            carrier->high[stage][slot] = (uint32_t)(guest[i] >> 32);
            carrier->high_defined[stage] |= bit;
            carrier->last_plural[stage] |= bit;
        } else carrier->last_plural[stage] &= ~bit;
    }
}
static void api_nvnCommandBufferBindTextures(NVNcommandBuffer *key, NVNshaderStage stage, int first, int count, const NVNtextureHandle *values)
{ bind_handles(key,stage,first,count,values,0,1); }
static void api_nvnCommandBufferBindTexture(NVNcommandBuffer *key, NVNshaderStage stage, int index, NVNtextureHandle value)
{ bind_handles(key,stage,index,1,&value,0,0); }
static void api_nvnCommandBufferBindSeparateTextures(NVNcommandBuffer *key, NVNshaderStage stage, int first, int count, const NVNseparateTextureHandle *values)
{ bind_handles(key,stage,first,count,values,1,1); }
static void api_nvnCommandBufferBindSeparateTexture(NVNcommandBuffer *key, NVNshaderStage stage, int index, NVNseparateTextureHandle value)
{ bind_handles(key,stage,index,1,&value,1,0); }
static void api_nvnCommandBufferBindSeparateSamplers(NVNcommandBuffer *key, NVNshaderStage stage, int first, int count, const NVNseparateSamplerHandle *values)
{ bind_handles(key,stage,first,count,values,2,1); }
static void api_nvnCommandBufferBindSeparateSampler(NVNcommandBuffer *key, NVNshaderStage stage, int index, NVNseparateSamplerHandle value)
{ bind_handles(key,stage,index,1,&value,2,0); }
#include "nvn_depth_target.inc"
#include "nvn_pipeline_draw_record.inc"
static void api_nvnCommandBufferBindVertexAttribState(NVNcommandBuffer *key, int count, const NVNvertexAttribState *states)
{
    struct object *o = recording_command(key); if (!o) return;
    if(count<0||count>16||(count&&!states)){error(EINVAL);return;}
    uint32_t proposed[32]={0};for(int i=0;i<count;i++)proposed[i]=attribute_read(states+i);
    struct nn_command_binding values[32]={{0}};for(unsigned i=0;i<32;i++){values[i].address=proposed[i];values[i].flags=(uint32_t)count;}
    if(command_capture_bindings(o,NN_COMMAND_VERTEX_ATTRIB,0,0,32,values))return;
    memcpy(o->record->attributes,proposed,sizeof(proposed));o->record->attributes_set=count;
}
static void api_nvnCommandBufferBindVertexStreamState(NVNcommandBuffer *key, int count, const NVNvertexStreamState *states)
{
    struct object *o = recording_command(key); if (!o) return;
    if(count<0||count>16||(count&&!states)){error(EINVAL);return;}
    struct vertex_stream proposed[32]={{0}};if(count)memcpy(proposed,states,(size_t)count*sizeof(*proposed));
    struct nn_command_binding values[32]={{0}};for(unsigned i=0;i<32;i++){values[i].address=proposed[i].stride;values[i].offset=proposed[i].divisor;values[i].flags=(uint32_t)count;}
    if(command_capture_bindings(o,NN_COMMAND_VERTEX_STREAM,0,0,32,values))return;
    memcpy(o->record->vertex_streams,proposed,sizeof(proposed));o->record->vertex_stream_count=count;
    o->record->stream=proposed[0];o->record->stream_set=count;
}
static void api_nvnCommandBufferBindVertexBuffers(NVNcommandBuffer *key, int first, int count, const NVNbufferRange *ranges)
{
    struct object *o = recording_command(key); if (!o) return;
    if(first<0||count<0||first>16||count>16-first||(count&&!ranges)){error(EINVAL);return;}
    struct bound proposed[32]={{0}};
    for(int i=0;i<count;i++)if(ranges[i].address||ranges[i].size){
        if(!ranges[i].address||!ranges[i].size){error(EINVAL);return;}
        struct object *pool=address_pool(o->device,ranges[i].address,ranges[i].size);
        if(!pool){error(EINVAL);return;}
        proposed[i]=(struct bound){pool,ranges[i].address-nn_bo_address(pool->bo),ranges[i].size,ranges[i].address};
    }
    struct nn_command_binding values[32];for(int i=0;i<count;i++)values[i]=command_binding(proposed[i].pool,proposed[i].offset,proposed[i].size,proposed[i].address);
    if(count&&command_capture_bindings(o,NN_COMMAND_VERTEX_BUFFER,0,(unsigned)first,(unsigned)count,values))return;
    for(int i=0;i<count;i++){
        struct bound *old=o->record->vertices+first+i;if(old->pool)old->pool->refs--;
        *old=proposed[i];if(old->pool)old->pool->refs++;
    }
    /* Legacy stream0 aliases metadata only; refs belong to vertices[]. */
    o->record->vertex=o->record->vertices[0];
}
static void api_nvnCommandBufferBindVertexBuffer(NVNcommandBuffer *key, int index, NVNbufferAddress address, size_t size)
{ NVNbufferRange range = {address, size}; api_nvnCommandBufferBindVertexBuffers(key, index, 1, &range); }
static void api_nvnCommandBufferSetRenderTargets(NVNcommandBuffer *key, int count, const NVNtexture *const *colors,
                                                const NVNtextureView *const *views,
                                                const NVNtexture *depth, const NVNtextureView *depth_view)
{
    struct object *o = recording_command(key); if (!o) return;
#if NN_NVN_DEPTH_TARGETS
    if(o->device->pipeline_depth){int e=public_depth_set_targets(o,count,colors,views,depth,depth_view);if(e)error(e);return;}
#endif
    if (count != 1 || !colors || !colors[0] || depth || depth_view) { error(ENOTSUP); return; }
    struct object *target = find(colors[0], TEXTURE); if (!target) return;
    struct texture_desc resolved; size_t target_bytes=0;
    int view_error=resolve_texture_view(target,views ? views[0] : NULL,&resolved);
    if (view_error) { error(view_error); return; }
    int clear_target=0;
#if NN_NVN_CLEAR_COLOR
    /* Raw clear permission is separate from Draw or texture sampling. Its
     * typed source metadata proves canonical storage without a native RT. */
    if(o->device->clear_color&&!texture_sampling_identity(&resolved)){error(ENOTSUP);return;}
    clear_target=o->device->clear_color&&!clear_color_target_admit(target);
#endif
    if (target->device != o->device) { error(ENOTSUP); return; }
    if (!clear_target) {
#if NN_NVN_COLOR_TARGETS
        if(o->device->pipeline_archive){
            int admitted=pipeline_target_admit(o->device,target);if(admitted){error(admitted);return;}
        }else
#endif
        if (
        (!o->device->pipeline_archive&&(resolved.width!=64||resolved.height!=64)) ||
        (target->source_layout ? nn_tex_bridge_source_validate(target->source_layout) :
        (linear_texture_bytes(&resolved,&target_bytes) || target_bytes!=target->size))) { error(ENOTSUP); return; }
    }
    if(clear_target){if(gpu_pool_access(target->pool,target->offset,target->size,"nvnCommandBufferSetRenderTargets raw clear role"))return;}
    else if (gpu_render_target_access(target,"nvnCommandBufferSetRenderTargets")) return;
    struct nn_command_binding value=command_binding(target,target->offset,target->size,nn_bo_address(target->bo)+target->offset);
    if(command_capture_bindings(o,NN_COMMAND_TARGET,0,0,1,&value))return;
    if (o->record->target) o->record->target->refs--;
    o->record->target = target; target->refs++;
}
static void api_nvnCommandBufferSetViewport(NVNcommandBuffer *key, int x, int y, int width, int height)
{
    struct object *o = recording_command(key); if (!o) return;
    if(x<0||y<0||width<=0||height<=0||width>16384||height>16384){error(EINVAL);return;}
    struct nn_command_state delta={.viewport={(float)x,(float)y,(float)width,(float)height}};
    if(command_capture_state(o,NN_COMMAND_STATE_VIEWPORT,&delta))return;
    o->record->viewport[0]=x;o->record->viewport[1]=y;o->record->viewport[2]=width;o->record->viewport[3]=height;
    o->record->viewport_set = 1;
}
static void api_nvnCommandBufferSetViewports(NVNcommandBuffer *key, int first, int count, const float *values)
{
    struct object *o = recording_command(key); if (!o) return;
    if(first||count!=1||!values){error(ENOTSUP);return;}
    for(unsigned i=0;i<4;i++)if(!isfinite(values[i])||values[i]<0||values[i]>16384||values[i]!=(float)(int)values[i]){error(ENOTSUP);return;}
    if(values[2]<=0||values[3]<=0){error(EINVAL);return;}
    struct nn_command_state delta={0};memcpy(delta.viewport,values,sizeof(delta.viewport));
    if(command_capture_state(o,NN_COMMAND_STATE_VIEWPORT,&delta))return;
    for(unsigned i=0;i<4;i++)o->record->viewport[i]=(int)values[i];
    o->record->viewport_set=1;
}
static void api_nvnCommandBufferSetScissor(NVNcommandBuffer *key, int x, int y, int width, int height)
{
    struct object *o = recording_command(key); if (!o) return;
    if(x<0||y<0||width<=0||height<=0||width>16384||height>16384){error(EINVAL);return;}
    struct nn_command_state delta={.scissor={x,y,width,height}};
    if(command_capture_state(o,NN_COMMAND_STATE_SCISSOR,&delta))return;
    o->record->scissor[0]=x;o->record->scissor[1]=y;o->record->scissor[2]=width;o->record->scissor[3]=height;
    o->record->scissor_set = 1;
}
static void api_nvnCommandBufferSetDepthRange(NVNcommandBuffer *key, float near, float far)
{
    struct object *o = recording_command(key); if (!o) return;
    if(!isfinite(near)||!isfinite(far)||near<0||near>1||far<0||far>1){error(EINVAL);return;}
    struct nn_command_state delta={.depth_range={near,far}};
    if(command_capture_state(o,NN_COMMAND_STATE_DEPTH_RANGE,&delta))return;
    o->record->depth_range[0]=near;o->record->depth_range[1]=far;o->record->depth_range_set = 1;
}
#include "nvn_command_deferred_draw.inc"
static void record_graphics_draw(NVNcommandBuffer *key, NVNdrawPrimitive primitive, int first, int count,
                                 const struct nn_gfx_draw *parameters)
{
    struct object *o = recording_command(key); if (!o) return;
    struct recording *r = o->record;
    if (primitive < NVN_DRAW_PRIMITIVE_POINTS || primitive > NVN_DRAW_PRIMITIVE_TRIANGLE_FAN ||
        first < 0 || count < 0) { error(ENOTSUP); return; }
    if (!count || (parameters && !parameters->instances && parameters->mode==NN_GFX_DRAW_DIRECT)) return;
    if(command_capture_close(o))return;
    if(r->current->call_nodes){
        if(gpu_pool_access(r->pool,r->offset,r->capacity,"NVN deferred generic command arena"))return;
        error(command_record_deferred_draw(o,primitive,first,count,parameters));return;
    }
    /* Source registration/binding does not qualify runtime state/resources.
     * Reject before command reservation, resource refs or late-Draw mutation. */
    if(r->graphics_program&&r->graphics_program->pipeline_source.archive){
        if(gpu_pool_access(r->pool,r->offset,r->capacity,"NVN generic command arena")||
           gpu_render_target_access(r->target,"NVN generic render target")||
           (r->depth&&gpu_pool_access(r->depth->pool,r->depth->offset,r->depth->size,"NVN generic depth target")))return;
        error(pipeline_record_draw(o,primitive,first,count,parameters));return;
    }
    if (gpu_recording_access(r,"nvnCommandBufferDrawArrays")) return;
    if (!r->graphics_program || !r->graphics_program->graphics || !r->graphics_program->bo || !r->graphics_program->fs_bo ||
        !r->target || !r->vertex.pool || !r->graphics_ubos[NVN_SHADER_STAGE_VERTEX][0].pool ||
        !r->attributes_set || !r->stream_set || !r->viewport_set || !r->scissor_set || !r->depth_range_set) {
        error(EINVAL); return;
    }
    {int reserve_error=command_reserve(o,24);if(reserve_error){error(reserve_error);return;}}
    if(r->viewport[0]||r->viewport[1]||r->viewport[2]!=64||r->viewport[3]!=64||
       r->scissor[0]||r->scissor[1]||r->scissor[2]!=64||r->scissor[3]!=64||
       r->depth_range[0]!=0||r->depth_range[1]!=1){error(ENOTSUP);return;}
    if(r->attributes[0]!=NVN_FORMAT_RG32F||
       (r->attributes_set==2&&r->attributes[1]!=(NVN_FORMAT_RGBA32F|(8u<<8)))||
       (r->attributes_set==3&&(r->attributes[1]!=(NVN_FORMAT_RG32F|(8u<<8))||r->attributes[2]!=(NVN_FORMAT_RGBA32F|(16u<<8))))){error(ENOTSUP);return;}
    struct bound *matrix = &r->graphics_ubos[NVN_SHADER_STAGE_VERTEX][0];
    if (matrix->size < 64) { error(EINVAL); return; }
    const struct nn_gfx_image *image = r->graphics_program->graphics->image;
    if (r->target->texture.format!=NVN_FORMAT_RGBA8 ||
        (!r->target->source_layout && (r->target->texture.stride!=(ptrdiff_t)image->target_pitch ||
        r->target->size!=image->target_bytes))) { error(ENOTSUP); return; }
    struct nn_gfx_draw_state state = o->device->graphics_state;
    const struct nn_render_target_info target_info = {image->chip,state.window_origin,1,0,1};
    int state_error=nn_render_state_validate(&r->render,&target_info);
    if (state_error) { error(state_error); return; }
    if (!graphics_profile(image) || state.window_origin > 1 || state.depth_mode > 1) {
        error(ENOTSUP); return;
    }
    int textured = image->profile == NN_GFX_TEXTURE_2D_NORMALIZED;
    if (r->attributes_set != (textured ? 3u : 2u) || r->stream.stride != (textured ? 32u : 24u) ||
        r->vertex.size < image->vertex_bytes) { error(ENOTSUP); return; }
    struct object *arguments[3]={0};
    if (parameters) {
        const struct nn_gfx_buffer *extra[]={&parameters->index,&parameters->indirect,&parameters->indirect_count};
        for (unsigned i=0;i<3;i++) if (extra[i]->bo) {
            uint64_t address=nn_bo_address(extra[i]->bo)+extra[i]->offset;
            arguments[i]=address_pool(o->device,address,extra[i]->bytes);
            if (!arguments[i] || arguments[i]->bo!=extra[i]->bo ||
                gpu_pool_access(arguments[i],extra[i]->offset,extra[i]->bytes,"NVN draw arguments")) { error(EINVAL); return; }
        }
    }
    if (textured || r->target->source_layout) {
        const struct nn_nvn_sampled *map=r->graphics_program->graphics->sampled;
        struct bound *fragment=&r->graphics_ubos[NVN_SHADER_STAGE_FRAGMENT][0];
        if (textured && (!r->texture_pool || !r->sampler_pool || !fragment->pool || fragment->size < 64)) { error(EINVAL); return; }
        uint64_t texture=textured?r->separate_textures[map->stage][map->texture_slot]:0;
        uint64_t sampler=textured?r->separate_samplers[map->stage][map->sampler_slot]:0;
        if (textured && (!texture || !sampler || texture >= r->texture_pool->slot_count ||
            (sampler >> 20) >= r->sampler_pool->slot_count)) { error(EINVAL); return; }
        struct late_draw *draw=calloc(1,sizeof(*draw)); if (!draw) { error(ENOMEM); return; }
        *draw=(struct late_draw){.program=r->graphics_program,.target=r->target,
            .texture_pool=textured?r->texture_pool:NULL,.sampler_pool=textured?r->sampler_pool:NULL,
            .vertex=r->vertex,.matrix=*matrix,.fragment=*fragment,.state=state,.render=r->render,
            .texture_id=texture,.sampler_id=sampler >> 20,.command_offset=r->logical_bytes+r->used-r->start};
        if (parameters) { draw->draw_override=1; draw->draw=*parameters; }
        struct object *resources[]={r->graphics_program,r->target,r->target->pool,r->vertex.pool,matrix->pool,
            fragment->pool,arguments[0],arguments[1],arguments[2]};
        size_t previous=r->current->count, previous_cpu=r->current->cpu_count; int e=0;
        for (unsigned i=0;i<sizeof(resources)/sizeof(resources[0]);++i) if (resources[i] && (e=add_resource(r->current,resources[i]))) break;
        if (!e && textured) e=add_cpu_resource(r->current,r->texture_pool);
        if (!e && textured) e=add_cpu_resource(r->current,r->sampler_pool);
        if (e) {
            while (r->current->cpu_count>previous_cpu) r->current->cpu_resources[--r->current->cpu_count]->refs--;
            while (r->current->count>previous) r->current->resources[--r->current->count]->refs--;
            free(draw); error(e); return;
        }
        /* This is never submitted directly: enqueue fills a private parent IB.
         * Keeping the caller arena unchanged makes repeated/inflight handles
         * independent of later descriptor registration versions. */
        uint32_t *cmd=(uint32_t *)((char *)nn_bo_map(r->pool->bo)+r->offset+r->used);
        memset(cmd,0,16); r->used+=16;
        draw->next=r->current->draws; r->current->draws=draw; return;
    }
    struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT] = {0};
    buffers[NN_GFX_VS_CODE] = (struct nn_gfx_buffer){r->graphics_program->bo, 0, image->vs_code_bytes};
    buffers[NN_GFX_FS_CODE] = (struct nn_gfx_buffer){r->graphics_program->fs_bo, 0, image->fs_code_bytes};
    buffers[NN_GFX_TARGET] = (struct nn_gfx_buffer){r->target->bo, r->target->offset, r->target->size};
    buffers[NN_GFX_VERTICES] = (struct nn_gfx_buffer){r->vertex.pool->bo, r->vertex.offset, r->vertex.size};
    buffers[NN_GFX_MATRIX] = (struct nn_gfx_buffer){matrix->pool->bo, matrix->offset, matrix->size};
    struct job *job = calloc(1, sizeof(*job)); if (!job) { error(ENOMEM); return; }
    size_t bytes;
    int e = parameters ? nn_gfx_encode_draw_render(o->device->gpu,image,buffers,&state,parameters,&r->render,&job->bo,&bytes) :
        nn_gfx_encode_render(o->device->gpu, image, buffers, &state,&r->render, &job->bo, &bytes);
    if (e) { free(job); error(e); return; }
    size_t previous_resources = r->current->count;
    struct object *resources[] = {r->graphics_program, r->target, r->target->pool, r->vertex.pool, matrix->pool,arguments[0],arguments[1],arguments[2]};
    for (unsigned i = 0; i < sizeof(resources) / sizeof(resources[0]); ++i) {
        if (!resources[i] || !(e = add_resource(r->current, resources[i]))) continue;
        while (r->current->count > previous_resources) r->current->resources[--r->current->count]->refs--;
        nn_bo_destroy(job->bo); free(job); error(e); return;
    }
    uint64_t address = nn_bo_address(job->bo);
    uint32_t *cmd = (uint32_t *)((char *)nn_bo_map(r->pool->bo) + r->offset + r->used);
    cmd[0] = nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER, 3);
    cmd[1] = address; cmd[2] = address >> 32; cmd[3] = bytes / 4;
    r->used += 16;
    job->next = r->current->jobs; r->current->jobs = job; o->device->refs++;
}
#include "nvn_command_compute.inc"
static void record_compute_dispatch(NVNcommandBuffer *key,int x,int y,int z,NVNbufferAddress indirect)
{
 struct object *o=find(key,COMMAND);if(!o)return;struct recording *r=o->record;
 if(!r->active||x<=0||y<=0||z<=0){error(EINVAL);return;}
 uint32_t groups[3]={(uint32_t)x,(uint32_t)y,(uint32_t)z};
 if(r->current->call_nodes){
  if(gpu_pool_access(r->pool,r->offset,r->capacity,"nvnCommandBufferDispatchCompute"))return;
  error(command_record_deferred_compute(o,groups,indirect));return;
 }
 if(!r->program){error(EINVAL);return;}
 if(gpu_recording_access(r,"nvnCommandBufferDispatchCompute"))return;
 if(command_capture_close(o))return;
 int e=command_reserve(o,24);if(e){error(e);return;}
 struct command_dispatch *operation=calloc(1,sizeof(*operation));if(!operation){error(ENOMEM);return;}
 struct job *job=NULL;size_t bytes=0;
 e=command_compute_encode(o->device,r->program,r->ubos,r->ssbos,groups,indirect,r->current,&job,&bytes);
 if(e){free(operation);error(e);return;}
 operation->command_offset=r->logical_bytes+r->used-r->start;memcpy(operation->groups,groups,sizeof(groups));operation->indirect=indirect;operation->recorded_job=job;
 uint64_t address=nn_bo_address(job->bo);uint32_t *cmd=(uint32_t*)((char*)nn_bo_map(r->pool->bo)+r->offset+r->used);
 cmd[0]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);cmd[1]=(uint32_t)address;cmd[2]=(uint32_t)(address>>32);cmd[3]=bytes/4;r->used+=16;
 job->next=r->current->jobs;r->current->jobs=job;o->device->refs++;
 operation->next=r->current->dispatches;r->current->dispatches=operation;
}
#include "nvn_draw_dispatch.inc"
static void command_record_clear_buffer(struct object *,struct object *,uint64_t,size_t,uint32_t);
static void api_nvnCommandBufferClearBuffer(NVNcommandBuffer *key, NVNbufferAddress address, size_t size, uint32_t value)
{
    struct object *o = find(key, COMMAND); if (!o) return;
    struct recording *r = o->record;
    if (!r->active || address % 4 || size % 4 || !size || size / 4 > 1024) { error(EINVAL); return; }
    struct object *target = NULL;
    for (struct object *p = objects; p; p = p->next) if (p->kind == POOL && p->device == o->device) {
        if (p->external) continue; /* no raw address/UBO/SSBO/clear aliases */
        uint64_t base = nn_bo_address(p->bo);
        if (address >= base && address - base <= p->size && size <= p->size - (address - base)) { target = p; break; }
    }
    if(!target){error(EINVAL);return;}
    command_record_clear_buffer(o,target,address,size,value);
}
static NVNcommandHandle api_nvnCommandBufferEndRecording(NVNcommandBuffer *key)
{
    struct object *o = find(key, COMMAND); if (!o) return 0;
    struct recording *r = o->record;
    /* Begin/End is a valid empty command set, including state-only recordings.
     * Keep the ordinary terminal GPU ordering packets and handle ownership. */
    if(!r->active){error(EINVAL);return 0;}
    int reserve_error=command_reserve(o,8);if(reserve_error){error(reserve_error);return 0;}
    reserve_error=control_reserve(o,sizeof(struct control));if(reserve_error){error(reserve_error);return 0;}
    if(r->debug_depth){error(EINVAL);return 0;}
    uint32_t *cmd = (uint32_t *)((char *)nn_bo_map(r->pool->bo) + r->offset + r->used);
    cmd[0] = nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE, 0); cmd[1] = nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME, 0);
    r->used += 8;
    struct handle *h=r->current;
    size_t total=r->logical_bytes+r->used-r->start;
    if(r->logical_bytes){int e=command_close_chunk(o);if(e){error(e);return 0;}}
    int capture_error=command_capture_close(o);
    if(capture_error){error(capture_error);return 0;}
    if(h->wait_count){capture_error=nn_command_sequence_unsupported(h->call_sequence);if(capture_error){error(capture_error);return 0;}}
    capture_error=nn_command_sequence_seal(h->call_sequence);
    if(capture_error){error(capture_error);return 0;}
    h->commands=h->chunks?h->chunks->pool:r->pool;
    h->offset=h->chunks?h->chunks->offset:r->offset+r->start;
    h->commands->refs++;h->device=o->device;h->device->refs++;
    h->bytes=total;h->id=next_handle++;
    h->next = handles; handles = h;
    struct control c = {.tag = 0x4e564e434f4e5452ull, .handle = h->id, .bytes = h->bytes};
    memcpy(r->control + r->control_used, &c, sizeof(c)); r->control_used += sizeof(c);
    r->active = 0; r->current = NULL; return h->id;
}
static void free_draws(struct late_draw *draw)
{ while (draw) { struct late_draw *next=draw->next;
  if(draw->generic_pipeline)pipeline_record_release(&draw->pipeline);
  free(draw); draw=next; } }
static void api_nvnDeviceFinalizeCommandHandle(NVNdevice *key, NVNcommandHandle id)
{
    struct object *dev = find(key, DEVICE); if (!dev) return;
    struct handle *h=handles;while(h&&(h->id!=id||h->public_retired))h=h->next;
    if(!h||h->device!=dev){error(EINVAL);return;}
    h->public_retired=1;
    if(!h->call_references)command_body_destroy(h);
}
static void api_nvnCommandBufferFinalize(NVNcommandBuffer *key)
{
    struct object *o = find(key, COMMAND); if (!o) return;
    struct recording *r = o->record;
    if(r->in_callback){error(EBUSY);return;}
    if (r->current) {
        for (size_t i = 0; i < r->current->count; i++) r->current->resources[i]->refs--;
        for (size_t i = 0; i < r->current->cpu_count; i++) r->current->cpu_resources[i]->refs--;
        free(r->current->cpu_resources);
        retire_jobs(o->device, r->current->jobs); free_draws(r->current->draws);counter_free_ops(r->current->counter_ops);queue_family_handle_cleanup(r->current);
        command_dispatch_free(r->current->dispatches);command_copy_free(r->current->copies);nn_command_sequence_destroy(r->current->call_sequence);free(r->current->resources); free(r->current);
    }
    if (r->pool) r->pool->refs--;
    if (r->program) r->program->refs--;
    if (r->graphics_program) r->graphics_program->refs--;
    if (r->target) r->target->refs--;
    if (r->depth) r->depth->refs--;
    if (r->texture_pool) r->texture_pool->refs--;
    if (r->sampler_pool) r->sampler_pool->refs--;
    for(unsigned i=0;i<32;i++)if(r->vertices[i].pool)r->vertices[i].pool->refs--;
    if (r->shader_scratch.pool) r->shader_scratch.pool->refs--;
    for (unsigned i = 0; i < 16; i++) {
        if (r->ubos[i].pool) r->ubos[i].pool->refs--;
        if (r->ssbos[i].pool) r->ssbos[i].pool->refs--;
    }
    for (unsigned stage = 0; stage < 2; ++stage)
        for (unsigned i = 0; i < 14; ++i)
            if (r->graphics_ubos[stage][i].pool) r->graphics_ubos[stage][i].pool->refs--;
    for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<16;i++)if(r->graphics_storage[stage][i].pool)r->graphics_storage[stage][i].pool->refs--;
    o->device->refs--; free(r); remove_object(o);
}
static void queue_reference(nn_bo **refs, size_t *count, nn_bo *bo)
{
    if (!bo) return;
    for (size_t i = 0; i < *count; ++i) if (refs[i] == bo) return;
    refs[(*count)++] = bo;
}
#include "texture_bridge_core.inc"
#include "nvn_depth_draw.inc"
#include "nvn_depth_sampled.inc"
#include "nvn_pipeline_draw_prepare.inc"
#include "nvn_pipeline_draw_submit.inc"
/* Trusted native token; either original explicit64RGBA image pools or
 * canonical RGBA8 source textures with independent converted display owners.
 * 2/3 slots, interval0..5/full crop; native pacing is platform-qualified. Workers never enter core/BO/KGSL. */
static struct object *window_builder_object(const void *key)
{ return value_builder(key,WB); }
static void api_nvnWindowBuilderSetDefaults(NVNwindowBuilder *key)
{
    struct object *o=window_builder_object(key); if (!o) return;
    /* Keep the original weak device, including SetDevice-before-Defaults. */
    memset(&o->window_builder,0,sizeof(o->window_builder));
    o->window_builder.interval=1;
}
static void api_nvnWindowBuilderSetDevice(NVNwindowBuilder *key,NVNdevice *device)
{
    struct object *o=window_builder_object(key); if (!o) return;
    struct object *d=find(device,DEVICE);if(d){o->device=d;o->builder_device_key=device;}
}
static void api_nvnWindowBuilderSetNativeWindow(NVNwindowBuilder *key,NVNnativeWindow native)
{
    struct object *o=find(key,WB); if (!o) return;
    if (!native) { error(EINVAL); return; }
    o->window_builder.native=native; /* token identity only; never dereference */
}
static void api_nvnWindowBuilderSetTextures(NVNwindowBuilder *key,int count,NVNtexture *const *textures)
{
    struct object *o=find(key,WB); if (!o) return;
    if ((count!=2 && count!=3) || !textures) { error(ENOTSUP); return; }
    /* SDK477620 stores the caller array pointer. Initialize validates and
     * snapshots its then-current contents; it does not borrow them afterward. */
    o->window_builder.textures=textures;
    o->window_builder.count=count;
}
static void api_nvnWindowBuilderSetPresentInterval(NVNwindowBuilder *key,int interval)
{
    struct object *o=find(key,WB); if (!o) return;
    if (interval<0 || interval>5) { error(EINVAL); return; }
    o->window_builder.interval=interval;
}
static NVNnativeWindow api_nvnWindowBuilderGetNativeWindow(const NVNwindowBuilder *key)
{ struct object *o=find(key,WB); return o ? o->window_builder.native : NULL; }
static int api_nvnWindowBuilderGetPresentInterval(const NVNwindowBuilder *key)
{ struct object *o=find(key,WB); return o ? o->window_builder.interval : 0; }

/* Display allocations are independent owners. Canonical guest pool/address,
 * offset and source-layout metadata are never rebound to the display AHB. */
static int window_release_displays(struct object *window)
{
    struct window_state *state=window->window;
    for(unsigned i=0;i<state->count;i++)if(state->source_display[i]){
        int e=nn_tex_conversion_destroy(state->conversions+i);if(e)return e;
        if(state->display[i]){e=nn_window_display_destroy(state->display[i]);if(e)return e;state->display[i]=NULL;}
    }
    if(state->conversion_queue){int e=nn_queue_destroy(state->conversion_queue);if(e)return e;state->conversion_queue=NULL;}
    return 0;
}
static int window_release_pins(struct object *window)
{
    struct window_state *state=window->window;
    for(unsigned i=0;i<state->count;i++)if(state->pinned[i]){
        struct object *texture=state->textures[i];
        if(!state->source_display[i]){int e=nn_external_image_unpin(texture->pool->external);if(e)return e;}
        texture->pool->refs--;texture->refs--;state->pinned[i]=0;
        if(texture->window_owner==window)texture->window_owner=NULL;
    }
    return 0;
}
static int window_source_overlap(const struct object *a,const struct object *b)
{
    if(a->bo==b->bo && a->offset<b->offset+b->size && b->offset<a->offset+a->size)return 1;
    /* Same caller ADDR storage imported through distinct pool objects is also
     * a known alias; do not confuse different GPUVAs with independent pages. */
    if(a->pool->storage&&b->pool->storage){
        uintptr_t pa=(uintptr_t)a->pool->storage,pb=(uintptr_t)b->pool->storage;
#if defined(__aarch64__)
        pa&=UINT64_C(0x00ffffffffffffff);pb&=UINT64_C(0x00ffffffffffffff);
#endif
        if(a->offset>UINTPTR_MAX-pa||b->offset>UINTPTR_MAX-pb)return 1;
        pa+=a->offset;pb+=b->offset;
        if(a->size>UINTPTR_MAX-pa||b->size>UINTPTR_MAX-pb)return 1;
        if(pa<pb+b->size&&pb<pa+a->size)return 1;
    }
    return 0;
}
static NVNboolean api_nvnWindowInitialize(NVNwindow *key,const NVNwindowBuilder *builder_key)
{
    struct object *builder=find(builder_key,WB);if(!builder)return 0;
    struct object *device=live_weak(builder->device,DEVICE);
    if(!device){error(EINVAL);return 0;}
    const struct window_builder_state snapshot=builder->window_builder;
    const struct window_builder_state *desc=&snapshot;
    if(!desc->native||!desc->textures||(desc->count!=2&&desc->count!=3)||desc->interval<0||desc->interval>5){error(ENOTSUP);return 0;}
    int interval_error=nn_window_session_interval_supported(desc->interval);
    if(interval_error){error(interval_error);return 0;}
    struct object *textures[3]={0};struct nn_external_image_view views[3]={{0}};
    unsigned source[3]={0};uint32_t width=0,height=0;
    for(int i=0;i<desc->count;i++){
        for(struct object *o=objects;o;o=o->next)if(o->key==desc->textures[i]&&o->kind==TEXTURE){textures[i]=o;break;}
        struct object *texture=textures[i];
        if(!texture||texture->device!=device){error(EINVAL);return 0;}
        if(texture->window_owner||texture->refs==SIZE_MAX||texture->pool->refs>SIZE_MAX-3){error(EBUSY);return 0;}
        if(gpu_pool_access(texture->pool,texture->offset,texture->size,"nvnWindowInitialize"))return 0;
        if(texture->source_layout){
            source[i]=1;
            if(texture->pool->external || !(texture->texture.flags&NVN_TEXTURE_FLAGS_DISPLAY) ||
               nn_tex_bridge_source_validate(texture->source_layout) ||
               !device->texture_bridge || nn_tex_bridge_bundle_validate(device->texture_bridge)) {error(ENOTSUP);return 0;}
            if(texture->offset>nn_bo_size(texture->bo)||texture->size>nn_bo_size(texture->bo)-texture->offset){error(EINVAL);return 0;}
        }else{
            /* Preserve the separately qualified original explicit64RGBA path.
             * It is not a way to turn an ordinary pool into an AHB. */
            if(!texture->pool->external||!texture_profile(&texture->texture)||texture->texture.width!=64||texture->texture.height!=64){error(ENOTSUP);return 0;}
            int e=nn_external_image_view(texture->pool->external,views+i);if(e){error(e);return 0;}
            if(texture->offset!=views[i].pixel_offset||texture->size!=views[i].payload_bytes||views[i].width!=64||views[i].height!=64||views[i].pitch!=256||views[i].format!=NN_EXTERNAL_RGBA8||!views[i].display_buffer){error(ENOTSUP);return 0;}
        }
        if(i&&(texture->texture.width!=(int)width||texture->texture.height!=(int)height||source[i]!=source[0])){error(ENOTSUP);return 0;}
        width=(uint32_t)texture->texture.width;height=(uint32_t)texture->texture.height;
        for(int j=0;j<i;j++){
            if(texture==textures[j]||window_source_overlap(texture,textures[j])){error(EINVAL);return 0;}
            if(!source[i]&&(views[i].display_buffer==views[j].display_buffer ||
              (views[i].backing_inode==views[j].backing_inode&&views[i].backing_dev_major==views[j].backing_dev_major&&views[i].backing_dev_minor==views[j].backing_dev_minor&&
               views[i].pixel_offset<views[j].pixel_offset+views[j].payload_bytes&&views[j].pixel_offset<views[i].pixel_offset+views[i].payload_bytes))){error(EINVAL);return 0;}
        }
    }
    struct window_state *state=calloc(1,sizeof(*state));if(!state){error(ENOMEM);return 0;}
    struct object *window=create(key,WINDOW);if(!window){free(state);return 0;}
    window->window=state;window->device=device;device->refs++;
    state->count=(unsigned)desc->count;state->width=width;state->height=height;
    window->window_builder=*desc;window->window_builder.textures=NULL;
    memcpy(state->textures,textures,sizeof(textures));memcpy(state->source_display,source,sizeof(source));
    int e=0;
    for(unsigned i=0;i<state->count;i++){
        if(!source[i]){e=nn_external_image_pin(textures[i]->pool->external);if(e)break;}
        textures[i]->refs++;textures[i]->pool->refs++;state->pinned[i]=1;
    }
    if(!e&&source[0])e=nn_queue_create(device->gpu,&state->conversion_queue);
    struct nn_window_slot slots[3]={{0}};
    for(unsigned i=0;!e&&i<state->count;i++){
        if(source[i]){
            e=nn_window_display_create(device->gpu,width,height,state->display+i);if(e)break;
            e=nn_window_display_view(state->display[i],views+i);if(e)break;
            struct nn_display_rgba8_shape shape;
            e=nn_display_rgba8_shape(views[i].width,views[i].height,views[i].pitch,&shape);if(e)break;
            if(views[i].width!=width||views[i].height!=height||views[i].format!=NN_EXTERNAL_RGBA8||
               views[i].payload_bytes!=shape.storage_bytes||shape.storage_bytes>UINT32_MAX||
               nn_display_rgba8_span(&shape,views[i].pixel_offset,views[i].fd_bytes)){e=ENOTSUP;break;}
            struct nn_tex_native_layout native={width,height,views[i].pitch,(uint32_t)shape.storage_bytes,(uint32_t)shape.storage_bytes,64};
            e=nn_tex_conversion_create(device->gpu,device->texture_bridge,textures[i]->bo,textures[i]->offset,textures[i]->size,
                textures[i]->source_layout,&native,views[i].bo,views[i].pixel_offset,views[i].payload_bytes,0,state->conversions+i);
            if(e)break;
        }
        state->display_views[i]=views[i];
        for(unsigned j=0;j<i;j++)if(views[i].display_buffer==views[j].display_buffer ||
          (views[i].backing_inode==views[j].backing_inode&&views[i].backing_dev_major==views[j].backing_dev_major&&views[i].backing_dev_minor==views[j].backing_dev_minor&&
           views[i].pixel_offset<views[j].pixel_offset+views[j].payload_bytes&&views[j].pixel_offset<views[i].pixel_offset+views[i].payload_bytes)){e=EEXIST;break;}
        if(e)break;
        slots[i]=(struct nn_window_slot){.buffer=views[i].display_buffer,.owner=source[i]?(const void *)state->display[i]:(const void *)textures[i]->pool->external,
            .pixel_offset=views[i].pixel_offset,.payload_bytes=views[i].payload_bytes,.backing_inode=views[i].backing_inode,.backing_dev_major=views[i].backing_dev_major,.backing_dev_minor=views[i].backing_dev_minor,
            .width=width,.height=height,.pitch=views[i].pitch,.format=NN_EXTERNAL_RGBA8,.usage=819};
    }
    if(!e)e=nn_window_session_create_interval(desc->native,slots,state->count,desc->interval,&state->session);
    if(e){
        error(e);state->closing=1;
        /* create failure exposes no worker. Allocation/conversion cleanup may
         * retain an owner; keep the failed Window and all canonical pins. */
        if(state->session||window_release_displays(window)||window_release_pins(window))return 0;
        device->refs--;free(state);remove_object(window);return 0;
    }
    for(unsigned i=0;i<state->count;i++){textures[i]->window_owner=window;textures[i]->window_index=i;}
    return 1;
}

static int window_bind_producer(struct object *window,struct object *queue)
{
    struct window_state *state=window->window;
    if (state->closing) return EBUSY;
    if (queue->device!=window->device) return EINVAL;
    if (state->producer && state->producer!=queue) return EBUSY;
    if (!state->producer) { state->producer=queue; queue->refs++; }
    return 0;
}
static int window_target_writable(struct object *texture,struct object *queue,int bind)
{
    struct object *window=texture->window_owner;
    if (!window) return 0;
    struct window_state *state=window->window; unsigned i=texture->window_index;
    if (state->closing || i>=state->count) return EBUSY;
    int e=nn_window_session_check_acquired(state->session,state->tickets+i);
    if (e) return e;
    if (queue->device!=window->device) return EINVAL;
    if (state->producer && state->producer!=queue) return EBUSY;
    return bind ? window_bind_producer(window,queue) : 0;
}
static int window_acquire(struct object *window,int *index)
{
    if (!index) return EINVAL;
    *index=-1;
    struct window_state *state=window->window;
    if (state->closing) return EBUSY;
    struct nn_window_ticket ticket={0};
    /* Bounded first qualification: a platform failure/timeout is reported,
     * never converted to an invented free slot or an unsignaled success. */
    int e=nn_window_session_acquire(state->session,1500,&ticket); if (e) return e;
    if (ticket.index>=state->count) { state->closing=1; return EIO; }
    state->tickets[ticket.index]=ticket; *index=(int)ticket.index; return 0;
}
static NVNwindowAcquireTextureResult api_nvnWindowAcquireTexture(NVNwindow *key,NVNsync *sync_key,int *index)
{
    if (index) *index=-1;
    struct object *window=find(key,WINDOW); if (!window) return NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR;
    struct object *sync=NULL;
    if (sync_key) {
        sync=find(sync_key,SYNC);
        if (!sync || sync->device!=window->device) { error(EINVAL); return NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR; }
        sync->availability=0;
    }
    int e=window_acquire(window,index);
    if (e) { error(e); return NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR; }
    if(sync){
        if(sync->fenced&&sync->fence.queue)sync->fence.queue->refs--;
        sync->fence=(struct wait_snapshot){0};sync->fenced=0;sync->availability=1;
    } /* CPU already proved this slot reusable. */
    return NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_SUCCESS;
}
static NVNqueueAcquireTextureResult api_nvnQueueAcquireTexture(NVNqueue *queue_key,NVNwindow *window_key,int *index)
{
    if (index) *index=-1;
    struct object *queue=find(queue_key,QUEUE),*window=find(window_key,WINDOW);
    if (!queue || !window) return NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR;
    struct window_state *state=window->window;
    if (queue->device!=window->device || (state->producer && state->producer!=queue)) {
        error(EINVAL); return NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR;
    }
    int e=window_acquire(window,index);
    if (!e) e=window_bind_producer(window,queue);
    if (e) { error(e); return NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR; }
    return NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS;
}
static void api_nvnQueuePresentTexture(NVNqueue *queue_key,NVNwindow *window_key,int index)
{
    struct object *queue=find(queue_key,QUEUE),*window=find(window_key,WINDOW);if(!queue||!window)return;
    struct window_state *state=window->window;
    if(state->closing||index<0||(unsigned)index>=state->count){error(EINVAL);return;}
    int e=nn_window_session_check_acquired(state->session,state->tickets+index);
    if(!e)e=window_bind_producer(window,queue);
    if(e){error(e);return;}
    /* The producer may have written canonical source bytes via real rendering
     * or source conversion jobs. Finish proves those writes and their refs. */
    e=nn_queue_finish(queue->queue,1000);
    if(e){state->closing=1;error(e);return;}
    reap_jobs(queue->device);if(last_error)return;
    if(state->source_display[index]){
        const struct nn_tex_conversion *conversion=state->conversions+index;
        struct object *texture=state->textures[index];
        nn_bo *resources[2+NN_TEX_CONVERSION_BO_COUNT]={texture->bo,state->display_views[index].bo};
        for(unsigned i=0;i<NN_TEX_CONVERSION_BO_COUNT;i++)resources[2+i]=conversion->bos[i];
        /* Once mutation is attempted, no owner can be freed until a real
         * conversion Finish succeeds. Unknown submission is never replayed. */
        state->conversion_pending=1;uint32_t timestamp=0;
        e=nn_queue_submit(state->conversion_queue,conversion->bos[NN_TEX_CONVERSION_ENTRY],0,conversion->entry_bytes,
            resources,2+NN_TEX_CONVERSION_BO_COUNT,&timestamp);
        if(e){state->closing=1;error(e);return;}
        e=nn_queue_finish(state->conversion_queue,5000);
        if(e){state->closing=1;error(e);return;}
        state->conversion_pending=0;
    }
    e=nn_window_session_enqueue_ready(state->session,state->tickets+index);
    if(e)state->closing=1;
    error(e);
}
static void api_nvnWindowFinalize(NVNwindow *key)
{
    struct object *window=find(key,WINDOW);if(!window)return;
    struct window_state *state=window->window;state->closing=1;
    if(state->producer){
        int e=nn_queue_finish(state->producer->queue,1000);if(e){error(e);return;}
        reap_jobs(window->device);if(last_error)return;
    }
    if(state->conversion_queue){
        int e=nn_queue_finish(state->conversion_queue,5000);if(e){error(e);return;}
        state->conversion_pending=0;
    }
    if(state->session){
        int e=nn_window_session_retire(state->session,1500);if(e){error(e);return;}
        e=nn_window_session_destroy(state->session);if(e){error(e);return;}state->session=NULL;
    }
    int e=window_release_displays(window);if(e){error(e);return;}
    e=window_release_pins(window);if(e){error(e);return;}
    if(state->producer)state->producer->refs--;
    window->device->refs--;free(state);remove_object(window);
}

static NVNnativeWindow api_nvnWindowGetNativeWindow(const NVNwindow *key)
{ struct object *window=find(key,WINDOW); return window ? window->window_builder.native : NULL; }
static int api_nvnWindowGetPresentInterval(const NVNwindow *key)
{ struct object *window=find(key,WINDOW); return window ? window->window_builder.interval : 0; }
static void api_nvnWindowSetPresentInterval(NVNwindow *key,int interval)
{
    struct object *window=find(key,WINDOW); if (!window) return;
    if(window->window_builder.interval==interval)return; /* SDK skips unchanged callback */
    int e=nn_window_session_set_present_interval(window->window->session,interval);
    if(e){error(e);return;}
    window->window_builder.interval=interval;
}
static void api_nvnWindowSetCrop(NVNwindow *key,int x,int y,int width,int height)
{
    struct object *window=find(key,WINDOW); if(!window)return;
    if (x || y || width!=(int)window->window->width || height!=(int)window->window->height) error(ENOTSUP);
}
static void api_nvnWindowGetCrop(const NVNwindow *key,NVNrectangle *crop)
{
    struct object *window=find(key,WINDOW); if(!window)return;
    if (!crop) { error(EINVAL); return; }
    *crop=(NVNrectangle){0,0,(int)window->window->width,(int)window->window->height};
}
int nn_nvn_window_get_stats(NVNwindow *key,struct nn_window_session_stats *out)
{
    /* Owner-only inspection: never call find()/error() or change sticky state. */
    if (!out) return EINVAL;
    memset(out,0,sizeof(*out));
    for (struct object *window=objects;window;window=window->next)
        if (window->key==key && window->kind==WINDOW)
            return window->window->session ? nn_window_session_get_stats(window->window->session,out) : EBUSY;
    return EINVAL;
}

#include "nvn_queue_family.inc"
#include "nvn_copy_commands.inc"

static int queue_handle_waits(struct object*q,const struct handle*h,struct nn_queue_dependency deps[16],size_t*count)
{
    struct wait_snapshot list[16];unsigned n=0;int e=0;
    for(unsigned i=0;i<q->wait_count;i++)if(q->waits[i].queue!=q)
        if((e=append_wait(list,&n,q->waits[i])))return e;
    for(unsigned i=0;i<h->wait_count;i++)if(h->waits[i].queue!=q)
        if((e=append_wait(list,&n,h->waits[i])))return e;
    for(unsigned i=0;i<n;i++)deps[i]=(struct nn_queue_dependency){list[i].queue->queue,list[i].timestamp};
    *count=n;return 0;
}
static int queue_stage_commands(struct object*q,const struct handle*h,void**out)
{
    if(h->bytes>q->queue_config.command)return ENOSPC;
    unsigned char*dst=(unsigned char*)q->queue_state+4096;
    if(h->chunks){
        size_t used=0;
        for(struct command_chunk*c=h->chunks;c;c=c->next){
            if(c->offset>c->pool->size || c->bytes>c->pool->size-c->offset || c->bytes>h->bytes-used)return EINVAL;
            memcpy(dst+used,(const char*)nn_bo_map(c->pool->bo)+c->offset,c->bytes);used+=c->bytes;
        }
        if(used!=h->bytes)return EINVAL;
    }else{
        if(h->offset>h->commands->size || h->bytes>h->commands->size-h->offset)return EINVAL;
        memcpy(dst,(const char*)nn_bo_map(h->commands->bo)+h->offset,h->bytes);
    }
    for(const struct job *job=h->jobs;job;job=job->next)if(job->projected_dispatch){
        if(job->command_offset>h->bytes||h->bytes-job->command_offset<16||job->command_bytes%4||!job->command_bytes||job->command_bytes>nn_bo_size(job->bo))return EINVAL;
        if(job->projected_dispatch==2){
            if(!nn_bo_map(job->bo))return ENOTSUP;
            memcpy(dst+job->command_offset,nn_bo_map(job->bo),job->command_bytes);continue;
        }
        uint64_t address=nn_bo_address(job->bo);uint32_t *cmd=(uint32_t*)(dst+job->command_offset);
        cmd[0]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);cmd[1]=(uint32_t)address;cmd[2]=(uint32_t)(address>>32);cmd[3]=job->command_bytes/4;
    }
    *out=dst;return 0;
}
static int range_overlap(uint64_t,size_t,uint64_t,size_t);
#include "nvn_counter.inc"
static int copy_handle_resource_access(const struct handle *,struct object *);
static int copy_handle_preflight(const struct handle *);
static int queue_admit_storage(const struct object *q,const struct handle *h,
 const struct pipeline_batch *batch,int index,size_t counter_extra)
{
 size_t jobs=0,draws=0,refs=0,compute=0;
 for(const struct job *j=h->jobs;j;j=j->next){jobs++;compute+=j->compute!=0;}
 for(const struct job_borrow*b=h->borrowed_jobs;b;b=b->next){jobs++;compute+=b->job->compute!=0;}
 for(const struct late_draw*d=h->draws;d;d=d->next){
  draws++;size_t add=0;
#if NN_NVN_GRAPHICS_SOURCE_CACHE
  if(d->generic_pipeline){const struct pipeline_prepared_draw*p=pipeline_batch_find(batch,d,index);
   if(!p)return EINVAL;
   add=p->reference_capacity;}
  else
#endif
  add=draw_ref_capacity(d);
  if(add>SIZE_MAX-refs)return EOVERFLOW;
  refs+=add;
 }
#if !NN_NVN_GRAPHICS_SOURCE_CACHE
 (void)batch;(void)index;
#endif
 if(h->count>65535||jobs>65535||draws>65535)return E2BIG;
 if(compute>q->queue_config.compute/sizeof(nn_bo*))return ENOSPC;
 if(h->count>SIZE_MAX/2)return EOVERFLOW;
 size_t capacity=h->count*2;
 if(jobs>SIZE_MAX-capacity)return EOVERFLOW;
 capacity+=jobs;
 if(refs>SIZE_MAX-capacity)return EOVERFLOW;
 capacity+=refs;
 if(counter_extra>SIZE_MAX-capacity)return EOVERFLOW;
 capacity+=counter_extra;
 return capacity>q->queue_config.control/sizeof(nn_bo*)?ENOSPC:0;
}
static void queue_submit_prepared_selected(NVNqueue *key,int count,const NVNcommandHandle *ids,struct pipeline_batch *batch,struct handle *const *selected)
{
    struct object *q = find(key, QUEUE); if (!q) return;
    if (count < 0 || (count && !ids)) { error(EINVAL); return; }
    unsigned counter_valid=q->counter?q->counter->valid:0;
    /* Preflight every handle before any materialization, cache call or submit,
     * so a forbidden second handle cannot cause a partial submission. */
    for (int i=0;i<count;i++) {
        struct handle *h=selected?selected[i]:handles; if(!selected)while (h && (h->id != ids[i] || h->public_retired)) h=h->next;
        if (!h || h->device != q->device) { error(EINVAL); return; }
        struct nn_queue_dependency deps[16];size_t dep_count=0;
        int dependency_error=queue_handle_waits(q,h,deps,&dep_count);
        if(dependency_error){error(dependency_error);return;}
        if(h->bytes>q->queue_config.command){error(ENOSPC);return;}
        int copy_error=copy_handle_preflight(h);if(copy_error){error(copy_error);return;}
        size_t counter_extra=0;int counter_error=counter_preflight(q,h,&counter_valid,&counter_extra);
        if(counter_error){error(counter_error);return;}
        int storage_error=queue_admit_storage(q,h,batch,i,counter_extra);
        if(storage_error){error(storage_error);return;}
        if(h->chunks){for(const struct command_chunk *c=h->chunks;c;c=c->next)
            if(gpu_pool_access(c->pool,c->offset,c->bytes,"nvnQueueSubmitCommands"))return;}
        else if(gpu_pool_access(h->commands,h->offset,h->bytes,"nvnQueueSubmitCommands"))return;
        for (size_t j=0;j<h->count;j++) {
#if NN_NVN_COLOR_TARGETS
            /* The batch above has independently admitted every actual generic
             * target. Preserve that role across Copy/Call resource pin lists;
             * unrelated textures still use their original consumer gates. */
            int typed_target=0;
            for(const struct late_draw *d=h->draws;d;d=d->next)
                if(d->generic_pipeline&&(d->target==h->resources[j]||d->depth==h->resources[j])){typed_target=1;break;}
            if(typed_target){if(gpu_pool_access(h->resources[j]->pool,h->resources[j]->offset,h->resources[j]->size,"nvnQueueSubmitCommands typed attachment"))return;}
            else
#endif
            if (copy_handle_resource_access(h,h->resources[j])) return;
        }
        for (size_t j=0;j<h->cpu_count;j++)
            if (cpu_descriptor_access(h->cpu_resources[j])) return;
        for (const struct late_draw *d=h->draws;d;d=d->next) {
            if (gpu_late_draw_access(d,"nvnQueueSubmitCommands")) return;
            int e=sampler_draw_preflight(d);if(e){error(e);return;}
        }
        /* Preflight current Acquire ownership before any handle submits. */
        for (size_t j=0;j<h->count;j++) if (h->resources[j]->kind==TEXTURE) {
            int e=window_target_writable(h->resources[j],q,0);
            if (e) { error(e); return; }
        }
    }
    for (int i = 0; i < count; i++) {
        struct handle *h=selected?selected[i]:handles; if(!selected)while (h && (h->id != ids[i] || h->public_retired)) h=h->next;
        if (!h || h->device != q->device) { error(EINVAL); return; }
        /* Bind the producer only after every handle passed preflight. */
        for (size_t j=0;j<h->count;j++) if (h->resources[j]->kind==TEXTURE) {
            int e=window_target_writable(h->resources[j],q,1);
            if (e) { error(e); return; }
        }
        size_t job_count=0, draw_count=0,draw_refs=0;
        for (struct job *job=h->jobs;job;job=job->next) job_count++;
        for(struct job_borrow*b=h->borrowed_jobs;b;b=b->next)job_count++;
        for (struct late_draw *draw=h->draws;draw;draw=draw->next) {
            draw_count++;
#if NN_NVN_GRAPHICS_SOURCE_CACHE
            const struct pipeline_prepared_draw *prepared=pipeline_batch_find(batch,draw,i);
            if(draw->generic_pipeline){if(!prepared){error(EINVAL);return;}draw_refs+=prepared->reference_capacity;}
            else
#endif
            draw_refs+=draw_ref_capacity(draw);
        }
        if (h->count > 65535 || job_count > 65535 || draw_count > 65535) { error(E2BIG); return; }
        size_t counter_extra=0;unsigned counter_shadow=q->counter?q->counter->valid:0;
        int counter_error=counter_preflight(q,h,&counter_shadow,&counter_extra);if(counter_error){error(counter_error);return;}
        size_t ref_capacity=h->count*2+job_count+draw_refs+counter_extra;
        if(ref_capacity>q->queue_config.control/sizeof(nn_bo*)){error(ENOSPC);return;}
        nn_bo**refs=(nn_bo**)((char*)q->queue_state+4096+q->queue_config.command);
        nn_bo**compute_refs=(nn_bo**)((char*)refs+q->queue_config.control);
        size_t compute_count=0;
        for(struct job*job=h->jobs;job;job=job->next)if(job->compute){
            if(compute_count>=q->queue_config.compute/sizeof(nn_bo*)){error(ENOSPC);return;}
            compute_refs[compute_count++]=job->bo;
        }
        for(struct job_borrow*b=h->borrowed_jobs;b;b=b->next)if(b->job->compute){
            if(compute_count>=q->queue_config.compute/sizeof(nn_bo*)){error(ENOSPC);return;}
            compute_refs[compute_count++]=b->job->bo;
        }
        size_t cursor=0;
        for (size_t j=0;j<h->count;j++) {
            queue_reference(refs,&cursor,h->resources[j]->bo);
            queue_reference(refs,&cursor,h->resources[j]->fs_bo);
        }
        for(struct job*job=h->jobs;job;job=job->next)if(!job->compute)queue_reference(refs,&cursor,job->bo);
        for(struct job_borrow*b=h->borrowed_jobs;b;b=b->next)if(!b->job->compute)queue_reference(refs,&cursor,b->job->bo);
        for(size_t j=0;j<compute_count;j++)queue_reference(refs,&cursor,compute_refs[j]);
        size_t counter_shift=counter_bracket(q,h)?16:0;
        if(h->bytes>SIZE_MAX-2*counter_shift){error(EOVERFLOW);return;}
        size_t submit_bytes=h->bytes+2*counter_shift;
        int e=0;struct job*temporary=NULL;void*staged=NULL;
        e=queue_stage_commands(q,h,&staged);
        nn_bo*parent=NULL;size_t parent_offset=0;
        if (!e) {
            struct job *version=calloc(1,sizeof(*version));
            if (!version) e=ENOMEM;
            else if ((e=nn_bo_alloc(q->device->gpu,submit_bytes,&version->bo))) free(version);
            else {
                version->next=temporary; temporary=version; q->device->refs++;
#if NN_NVN_GRAPHICS_SOURCE_CACHE
                /* Transfer the admitted original owners to this immutable
                 * submission version. Unknown completion cannot drop them. */
                for(struct late_draw *d=h->draws;d;d=d->next)if(d->generic_pipeline){
                    struct pipeline_prepared_draw *p=(struct pipeline_prepared_draw *)pipeline_batch_find(batch,d,i);
                    if(p&&p->pins){p->pins->next=version->resource_pins;version->resource_pins=p->pins;p->pins=NULL;}
                }
#endif
                parent=version->bo; parent_offset=0;
                memcpy((char *)nn_bo_map(parent)+counter_shift,staged,h->bytes);
                for (struct late_draw *draw=h->draws;draw;draw=draw->next) {
                    if (draw->command_offset > h->bytes || h->bytes-draw->command_offset < 16) { e=EINVAL; break; }
                    struct job *child=calloc(1,sizeof(*child));
                    if (!child) { e=ENOMEM; break; }
                    size_t bytes=0; nn_bo *sampled=NULL;
                    struct job *auxiliary=NULL;
                    e=draw->generic_pipeline?pipeline_materialize(q->device,pipeline_batch_find(batch,draw,i),&child->bo,&bytes,&auxiliary,refs,&cursor,ref_capacity):
                      draw_needs_bridge(draw)?materialize_draw_bridge(q->device,draw,&child->bo,&bytes,&sampled,&auxiliary):
                        materialize_draw(q->device,draw,&child->bo,&bytes,&sampled);
                    while(auxiliary){struct job *next=auxiliary->next;auxiliary->next=temporary;temporary=auxiliary;
                        queue_reference(refs,&cursor,auxiliary->bo);auxiliary=next;}
                    if(e){free(child);break;}
                    child->next=temporary; temporary=child; q->device->refs++;
                    queue_reference(refs,&cursor,child->bo); queue_reference(refs,&cursor,sampled);
                    uint64_t address=nn_bo_address(child->bo);
                    uint32_t *cmd=(uint32_t *)((char *)nn_bo_map(parent)+counter_shift+draw->command_offset);
                    cmd[0]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);
                    cmd[1]=address; cmd[2]=address>>32; cmd[3]=bytes/4;
                }
            }
        }
        if(!e)e=counter_materialize(q,h,parent,counter_shift,refs,&cursor,q->queue_config.control/sizeof(nn_bo*),&temporary);
        if (!e) e=nn_bo_cache(parent,parent_offset,submit_bytes,0);
        uint32_t timestamp;
        if(!e){
            struct nn_queue_dependency deps[16];size_t dep_count=0;
            e=queue_handle_waits(q,h,deps,&dep_count);
            int counter_submitted=!e;
            if(!e)e=dep_count?nn_queue_submit_waits(q->queue,parent,parent_offset,submit_bytes,refs,cursor,deps,dep_count,&timestamp):
                nn_queue_submit(q->queue,parent,parent_offset,submit_bytes,refs,cursor,&timestamp);
            if(counter_submitted)counter_published(q,h,e);
            if(!e){q->queue_state->submissions++;q->queue_state->command_bytes+=h->bytes;
                q->queue_state->control_bytes+=cursor*sizeof(*refs);q->queue_state->compute_bytes+=compute_count*sizeof(*compute_refs);}
        }
        /* KGSL retains every BO even on an uncertain ioctl outcome. Busy
         * version BOs stay on the device retire list until a successful Finish.
         * Validation/materialization failures have no pending GPU work. */
        error(e); retire_jobs(q->device,temporary);
        if (e) return;
    }
}
static void queue_submit_prepared(NVNqueue *key,int count,const NVNcommandHandle *ids,struct pipeline_batch *batch)
{queue_submit_prepared_selected(key,count,ids,batch,NULL);}
#include "nvn_command_projection.inc"
static void api_nvnQueueSubmitCommands(NVNqueue *key,int count,const NVNcommandHandle *ids)
{
 struct object *q=find(key,QUEUE);if(!q)return;
 if(count<0||(count&&!ids)){error(EINVAL);return;}
 int projected=command_call_submit(key,q,count,ids);if(projected!=-1){error(projected);return;}
 for(int i=0;i<count;i++){
  struct handle *h=handles;while(h&&(h->id!=ids[i]||h->public_retired))h=h->next;
  if(!h||h->device!=q->device){error(EINVAL);return;}
  int call_error=command_call_admit(h);if(call_error){error(call_error);return;}
 }
 struct pipeline_batch batch={0};int e=pipeline_batch_prepare(q,count,ids,&batch);
 if(e){pipeline_batch_discard(&batch);error(e);return;}
 queue_submit_prepared(key,count,ids,&batch);pipeline_batch_discard(&batch);
 command_observe_submitted(q,count,ids,last_error);
}
#include "resource_copy.inc"
#include "resource_target_clear.inc"
#include "nvn_command_copy_projection.inc"
#include "metadata_getters.inc"
#include "texture_swizzle_metadata.inc"
#include "metadata_lifetime_getters.inc"
#define ENTRY(name, type) {#name, (PFNNVNGENERICFUNCPTRPROC)api_##name},
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
    static const struct { const char *name; PFNNVNGENERICFUNCPTRPROC function; } table[] = {
#include "nvn_entries.inc"
    };
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) if (!strcmp(name, table[i].name)) return table[i].function;
    return NULL;
}
#undef ENTRY
/* Compile-check every entry against the reconstructed ABI. */
#define ENTRY(name, type) static type const check_##name __attribute__((unused)) = api_##name;
#include "nvn_entries.inc"

static struct object *window_probe_lookup(NVNwindow *key)
{for(struct object *o=objects;o;o=o->next)if(o->kind==WINDOW&&o->key==key)return o;return NULL;}
int nn_nvn_window_target_info(NVNwindow *key,unsigned index,struct nn_external_image_view *out,struct nn_ahb_pixel *pixel)
{
    struct object *o=window_probe_lookup(key);if(!o||!out||!pixel)return EINVAL;
    if(index>=o->window->count||!o->window->source_display[index])return ENOTSUP;
    int e=nn_window_display_view(o->window->display[index],out);if(e)return e;
    return nn_window_display_pixel(o->window->display[index],pixel);
}
int nn_nvn_window_target_read_lock(NVNwindow *key,unsigned index,struct nn_ahb_view *view)
{
    struct object *o=window_probe_lookup(key);if(!o||!view||index>=o->window->count)return EINVAL;
    struct window_state *s=o->window;if(!s->source_display[index])return ENOTSUP;
    if(s->conversion_pending)return EBUSY; /* unknown GPU writes cannot be CPU-read */
    if(s->session){
        int e=nn_window_session_check_acquired(s->session,s->tickets+index);
        /* The real session returns EINVAL for a valid ticket in CURRENT or
         * QUEUED state. Native READ access reports that exclusion as busy. */
        if(e)return e==EINVAL?EBUSY:e;
    }
    else if(!s->closing)return EBUSY;
    return nn_window_display_read_lock(s->display[index],view);
}
int nn_nvn_window_target_read_unlock(NVNwindow *key,unsigned index)
{
    struct object *o=window_probe_lookup(key);if(!o||index>=o->window->count||!o->window->source_display[index])return EINVAL;
    return nn_window_display_read_unlock(o->window->display[index]);
}
int nn_nvn_window_retire_for_readback(NVNwindow *key)
{
    struct object *o=window_probe_lookup(key);if(!o)return EINVAL;struct window_state *s=o->window;s->closing=1;
    int e=0;if(s->producer){e=nn_queue_finish(s->producer->queue,1000);if(e)return e;reap_jobs(o->device);if(last_error)return last_error;}
    if(s->conversion_queue){e=nn_queue_finish(s->conversion_queue,5000);if(e)return e;s->conversion_pending=0;}
    if(s->session){e=nn_window_session_retire(s->session,1500);if(e)return e;e=nn_window_session_destroy(s->session);if(e)return e;s->session=NULL;}
    return 0;
}

int nn_nvn_window_source_identity(NVNwindow *key,unsigned index,struct nn_nvn_window_source_identity *out)
{
    struct object *o=window_probe_lookup(key);if(!o||!out||index>=o->window->count)return EINVAL;
    struct window_state *s=o->window;struct object *t=s->textures[index];if(!s->source_display[index])return ENOTSUP;
    *out=(struct nn_nvn_window_source_identity){.pool=(NVNmemoryPool *)t->pool->key,.address=nn_bo_address(t->bo)+t->offset,
        .entry_address=nn_bo_address(s->conversions[index].bos[NN_TEX_CONVERSION_ENTRY]),.offset=t->offset,.bytes=t->size,.entry_bytes=s->conversions[index].entry_bytes};return 0;
}

int nn_nvn_window_source_layout(NVNwindow *key,unsigned index,struct nn_tex_source_layout *out)
{
    struct object *o=window_probe_lookup(key);if(!o||!out||index>=o->window->count)return EINVAL;
    const struct nn_tex_source_layout *l=o->window->textures[index]->source_layout;if(!l)return ENOTSUP;
    *out=*l;return 0;
}
