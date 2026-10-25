/* SPDX-License-Identifier: MIT. Dynamic finite A650 VS/FS pipeline. */
#pragma once
#include "pipeline_vertex.h"
#include "pipeline_platform.h"
#include "texture_native_view.h"
#include "runtime_fdl.h"
#include "pipeline_texture_types.h"
#include "texture_buffer_sample.h"
#include "nvn_native/graphics.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_pipeline_image {const struct nn_tex_native_view *layout;struct nn_pipeline_buffer buffer;};
struct nn_pipeline_raster {
    /* Native viewport xoffset,xscale,yoffset,yscale,zoffset,zscale. Mapping NVN
     * viewport semantics to these fields is the frontend's explicit contract. */
    float viewport[6];uint32_t scissor_x,scissor_y,scissor_width,scissor_height;
    uint32_t window_origin,depth_mode;
};
struct nn_depth_fdl_view;
struct nn_pipeline_draw_state {
    struct nn_pipeline_image target,textures[16];
    struct nn_sampler_state samplers[16];struct nn_sampler_layout sampler_layouts[16];
    uint32_t texture_bound,sampler_bound;
    struct nn_pipeline_buffer ubo[2][16];
    struct nn_pipeline_vertex_state vertex;
    struct nn_pipeline_raster raster;struct nn_render_state render;
    struct nn_gfx_draw draw;
    /* Optional exact zero-address FDL BC views for TXF/TXS or explicit nearest/clamp2D.
     * Corresponding textures[i].layout must beNULL; targets stay RGBA8. */
    const struct nn_fdl_image_view *sampled_fdl[16];
    /* Append-only native true3D TXF/TXS. Corresponding legacy and2D FDL views
     * must beNULL; types is actual compiler proof for these exact stages. */
    const struct nn_fdl_volume_view *sampled_volume[16];
    const struct nn_pipeline_texture_types *texture_types;
    /* Default-OFF raw-native BUFFER-only field. Legacy image/FDL/volume and
     * textures[i] must all be zero for this binding. No public NVN admission. */
    const struct nn_buffer_sampled_binding *sampled_buffer[16];
    /* Distinct sampled-only actual FDL FLOAT2D role, never a BC/MRT view.
     * Corresponding legacy layout, BC/volume/BUFFER roles must be NULL. */
    const struct nn_fdl_image_view *sampled_color[16];
    /* Distinct default-OFF noncompare depth role, actual zero-address TILE6_3
     * FDL. No COLOR/BC/volume/BUFFER/legacy view at the same binding. */
    const struct nn_depth_fdl_view *sampled_depth[16];
    /* Actual compiler-logical bindings, separate from NVN API slots. */
    const struct nn_pipeline_storage_types *storage_types;
    struct nn_pipeline_buffer storage[2][16];
    /* Explicit actual compiler facts; pointer must equal Program-owned proof.
     * Raw native DrawID is caller data. No NVN DrawID mapping is inferred. */
    const struct nn_pipeline_runtime_constants *runtime_constants;
    uint32_t runtime_draw_id;
};
struct nn_pipeline_submission {nn_bo *job;size_t command_bytes;nn_bo *resources[128];size_t resource_count;};
/* Full-target viewport helper has unambiguous qualified0/1 origin+depth modes;
 * arbitrary native viewport/scissor are separately validated by the encoder.
 * No device state is read later: caller copies this value at Draw recording. */
int nn_pipeline_full_raster(uint32_t width,uint32_t height,uint32_t origin,uint32_t depth,
                            struct nn_pipeline_raster *out);
/* No fixture/nn_gfx_image, no 64x64/2x2 gate. Actual common FDL view artifact is
 * mandatory for every native target/sample. Only RGBA8, 2D, one mip/layer/sample,
 * triangle/list/strip/fan topology only, no UBWC, depth/stencil, feedback loops
 * or unknown shader features. Resource
 * mappings and specialization must have been resolved by caller, not selected
 * from archive order. Images supplied here are native shadows, never canonical
 * source GOB allocations. Caller retains immutable Program/FDL/artifact values
 * and ALL returned BOs through actual completion. Same-device object proof is
 * supplied by frontend and independently enforced by KGSL submit.
 * On error out is zero; retained is nonNULL only if rollback destroy failed.
 * That BO and its device must be quarantined. No GPU submit is performed. */
/* Pure whole-batch admission. planned_images explicitly marks nonresident
 * future native shadows (bit0 target,bit1..16 textures); their buffer must be
 * {NULL,0,required_span}. All layout/state/recipe and existing BO ranges are
 * validated without allocating, mapping, caching or submitting. Only future
 * address properties remain for encode's shared second validation. */
int nn_pipeline_preflight(nn_device *,const struct nn_offline_graphics_stage *const [2],
 const struct nn_pipeline_shader_recipe *,const struct nn_pipeline_platform *,
 const struct nn_pipeline_draw_state *,uint32_t planned_images);
int nn_pipeline_encode(nn_device *,const nn_pipeline_program *,const struct nn_pipeline_shader_recipe *,
    const struct nn_pipeline_platform *,const struct nn_pipeline_draw_state *,
    struct nn_pipeline_submission *out,nn_bo **retained);
struct nn_depth_attachment;struct nn_depth_controls_desc;struct nn_depth_fdl_view;
/* Nonresident metadata only, for whole-batch validation BEFORE image BOs
 * exist. All planned image buffers must beNULL and carry exact actual FDL
 * metadata. The NVN caller separately verifies original canonical windows;
 * encode_depth repeats full resident/program aliases after materialization. */
int nn_pipeline_preflight_depth_planned(nn_device *,const struct nn_offline_graphics_stage *const [2],
 const struct nn_pipeline_depth_export *,const struct nn_pipeline_platform *,const struct nn_pipeline_draw_state *,
 uint32_t planned_images,const struct nn_depth_fdl_view *,const struct nn_depth_controls_desc *);
int nn_pipeline_preflight_depth(nn_device *,const struct nn_offline_graphics_stage *const [2],const struct nn_pipeline_depth_export *,const struct nn_pipeline_platform *,const struct nn_pipeline_draw_state *,const struct nn_depth_attachment *,const struct nn_depth_controls_desc *);
int nn_pipeline_encode_depth(nn_device *,const nn_pipeline_program *,const struct nn_pipeline_depth_export *,const struct nn_pipeline_platform *,const struct nn_pipeline_draw_state *,const struct nn_depth_attachment *,const struct nn_depth_controls_desc *,struct nn_pipeline_submission *,nn_bo **retained);
#ifdef __cplusplus
}
#endif
