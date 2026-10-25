/* SPDX-License-Identifier: MIT
 * Copyright © 2016 Rob Clark <robclark@freedesktop.org>
 * Copyright © 2018 Google, Inc.
 * Bounded A6XX/A7XX native sysmem draw. State semantics follow pinned Mesa Gallium
 * fd6_emit/program/gmem/draw/rasterizer/blend/zsa; encoding and layout use its
 * generated register helpers and FDL. No API runtime or shader rewriting.
 */
#include <cstdio>
#include <cstring>
#include <vector>
#include "freedreno_pm4.h"
#include "fd6_hw.h"
#include "freedreno_layout.h"
#include "fd6_format_table.h"
#include "common/fd6_gmem_cache.h"
#include "common/freedreno_gpu_event.h"
#include "ir3/ir3_shader.h"
#include "nvn_graphics_image.h"

namespace {
constexpr uint32_t width = 64, height = 64;

bool supported(const ir3_shader_variant *vs, const ir3_shader_variant *fs,
               const ir3_shader_linkage *link, const fd6_gmem_config *sys,
               uint32_t gmem_bytes)
{
    if (!vs || !fs || !link || !sys || !vs->compiler || !fs->compiler ||
        vs->compiler != fs->compiler || (vs->compiler->gen != 6 && vs->compiler->gen != 7) ||
        vs->type != MESA_SHADER_VERTEX || fs->type != MESA_SHADER_FRAGMENT ||
        !gmem_bytes || gmem_bytes % 4096) return false;
    const bool textured = fs->num_samp != 0;
    const unsigned attrs = textured ? 3 : 2, varyings = textured ? 2 : 1;
    const auto *info = vs->compiler->info;
    if (!info->num_ccu || info->props.has_gmem_vpc_attr_buf ||
        info->props.load_shader_consts_via_preamble || sys->depth_ccu_offset != 0 ||
        sys->color_ccu_offset != info->num_ccu * info->props.sysmem_per_ccu_depth_cache_size ||
        uint64_t(sys->color_ccu_offset) + info->num_ccu * info->props.sysmem_per_ccu_color_cache_size > gmem_bytes ||
        sys->vpc_attr_buf_size || sys->vpc_pos_buf_size || sys->vpc_bv_pos_buf_size) return false;
    for (auto *v : {vs, fs}) {
        const auto *c = ir3_const_state(v);
        if (!v->bin || !v->info.size || v->info.size % 4 || v->info.size > 65536 ||
            v->pvtmem_size || v->shared_size || v->num_uavs || v->constant_data_size ||
            v->info.double_threadsize || v->info.max_reg < 0 || v->info.max_reg >= 48 ||
            v->info.max_half_reg != -1 || !v->mergedregs || v->constlen % 4 || v->constlen > 128 ||
            v->bindless_tex || v->bindless_samp || v->bindless_ibo || v->bindless_ubo ||
            v->clip_mask || v->cull_mask || v->stream_output.num_outputs || v->num_sampler_prefetch ||
            c->num_ubos || c->num_driver_params || c->driver_params_ubo.idx >= 0 ||
            c->consts_ubo.idx >= 0 || c->primitive_map_ubo.idx >= 0 || c->primitive_param_ubo.idx >= 0 ||
            v->imm_state.count > 16 || (v->imm_state.count && !v->imm_state.values)) return false;
        if (v->num_samp != (v == fs && textured ? 2 : 0)) return false;
        const unsigned ranges = v == vs || textured ? 1 : 0;
        if (c->ubo_state.num_enabled != ranges) return false;
        if (ranges) {
            const auto &r = c->ubo_state.range[0];
            if (r.ubo.bindless || r.ubo.global || r.ubo.block != (v == vs ? 1u : 2u) ||
                r.offset % 64 || r.start || r.end != 64 || r.offset / 16 + 4 > v->constlen) return false;
        }
        for (unsigned i = 0; i < IR3_CONST_ALLOC_MAX; i++) {
            if (i == IR3_CONST_ALLOC_UBO_RANGES || i == IR3_CONST_ALLOC_PREAMBLE ||
                (v == vs && i == IR3_CONST_ALLOC_PRIMITIVE_PARAM)) continue;
            if (c->allocs.consts[i].size_vec4) return false;
        }
        if (v->imm_state.count && c->allocs.max_const_offset_vec4 +
            (v->imm_state.count + 3) / 4 > v->constlen) return false;
    }
    if (vs->inputs_count != attrs || vs->total_in != (textured ? 8u : 6u) ||
        vs->imm_state.count) return false;
    for (unsigned i = 0; i < attrs; i++) {
        const auto &in = vs->inputs[i];
        const unsigned mask = i + 1 == attrs ? 15 : 3;
        if (in.slot != VERT_ATTRIB_GENERIC0 + i || in.compmask != mask ||
            in.sysval || in.bary || in.regid == INVALID_REG) return false;
    }
    if (fs->inputs_count != varyings + 1 || fs->total_in > 128 || !fs->total_in ||
        fs->sample_shading || fs->writes_pos || fs->writes_smask || fs->writes_stencilref ||
        fs->reads_primid || fs->frag_face || fs->fragcoord_compmask ||
        fs->dual_src_blend || fs->color0_mrt || fs->post_depth_coverage ||
        fs->outputs_count != 1 || fs->outputs[0].slot != FRAG_RESULT_DATA0 ||
        fs->outputs[0].regid == INVALID_REG || fs->outputs[0].half || fs->outputs[0].aliased_components ||
        (!textured && (fs->has_kill || fs->need_full_quad || fs->need_pixlod || fs->early_preamble || fs->imm_state.count)))
        return false;
    unsigned varying_end = 0;
    for (unsigned i = 0; i < varyings; i++) {
        const auto &in = fs->inputs[i];
        if (in.slot != VARYING_SLOT_VAR1 + i || in.compmask != (textured && i == 0 ? 3u : 15u) ||
            in.inloc + unsigned(__builtin_popcount(in.compmask)) > fs->total_in ||
            !in.bary || in.sysval || in.flat || in.rasterflat) return false;
        varying_end = MAX2(varying_end, in.inloc + unsigned(__builtin_popcount(in.compmask)));
    }
    const auto &bary = fs->inputs[varyings];
    if (bary.slot != SYSTEM_VALUE_BARYCENTRIC_PERSP_PIXEL || bary.regid == INVALID_REG ||
        bary.compmask != 3 || !bary.sysval || bary.bary) return false;
    if (link->cnt != varyings + 2 || link->max_loc != varying_end + 5 ||
        link->primid_loc != 255 || link->viewid_loc != 255 || link->clip0_loc != 255 || link->clip1_loc != 255)
        return false;
    for (unsigned i = 0; i < link->cnt; i++) {
        const unsigned slot = i < varyings ? VARYING_SLOT_VAR1 + i :
            i == varyings ? VARYING_SLOT_POS : VARYING_SLOT_PSIZ;
        const unsigned mask = i < varyings ? fs->inputs[i].compmask : i == varyings ? 15 : 1;
        if (link->var[i].slot != slot || link->var[i].regid != ir3_find_output_regid(vs, (gl_varying_slot)slot) ||
            link->var[i].regid == INVALID_REG || link->var[i].compmask != mask || link->var[i].loc !=
                (i < varyings ? fs->inputs[i].inloc : i == varyings ? varying_end : varying_end + 4))
            return false;
    }
    unsigned active_outputs = 0;
    for (unsigned i = 0; i < vs->outputs_count; i++) {
        const auto &out = vs->outputs[i];
        if (out.regid == INVALID_REG) continue;
        bool found = false;
        for (unsigned j = 0; j < link->cnt; j++) found |= out.slot == link->var[j].slot;
        if (out.half || out.aliased_components || !found) return false;
        active_outputs++;
    }
    return active_outputs == link->cnt;
}
}

static int graphics_artifact(const ir3_shader_variant *vs, const ir3_shader_variant *fs,
    const ir3_shader_linkage *link, const fd6_gmem_config *sys, uint32_t gmem_bytes, const char *prefix,
    bool mode_aware)
{
    if (!prefix || strlen(prefix) > 4000 || !supported(vs, fs, link, sys, gmem_bytes)) {
        fprintf(stderr, "native graphics: unsupported bounded A6XX/A7XX VS/FS, linkage or GMEM profile\n");
        return 9;
    }
    const bool textured = fs->num_samp != 0;
    const bool gen6 = vs->compiler->gen == 6;
    const uint32_t schema = mode_aware ? 3 : gen6 || textured ? 2 : 1;
    if (!vs->compiler->dev_id || vs->compiler->dev_id->chip_id > UINT32_MAX) return 9;
    const uint32_t chip_id = vs->compiler->dev_id->chip_id;
    if (mode_aware && chip_id != 0x06050002 && chip_id != 0x43030b00) return 9;
    const unsigned attrs = vs->inputs_count, vertex_stride = textured ? 32 : 24;
    const unsigned vertex_bytes = vertex_stride * 3, varying_count = fs->inputs_count - 1;
    const auto *info = vs->compiler->info;
    const fd_dev_id device_id = {.chip_id = chip_id};
    if (fd_dev_info_raw(&device_id) != info) return 9;
    fdl_layout layout = {};
    const fdl_image_params params = {.format = PIPE_FORMAT_R8G8B8A8_UNORM, .nr_samples = 1,
        .width0 = width, .height0 = height, .depth0 = 1, .mip_levels = 1, .array_size = 1,
        .tile_mode = TILE6_LINEAR, .ubwc = false};
    if (!fdl6_layout_image(&layout, info, &params, nullptr) || layout.ubwc ||
        fdl_tile_mode(&layout, 0) != TILE6_LINEAR || layout.size > UINT32_MAX) return 9;
    const uint32_t pitch = fdl_pitch(&layout, 0), layer = fdl_layer_stride(&layout, 0);
    fdl_layout texture_layout = {};
    uint32_t texture_descriptors[32] = {}, samplers[8] = {};
    std::vector<nn_gfx_patch> texture_patches;
    const uint64_t texture_address_mask = uint64_t(A6XX_TEX_MEMOBJ_4_BASE_LO__MASK) |
        uint64_t(A6XX_TEX_MEMOBJ_5_BASE_HI__MASK) << 32;
    if (textured) {
        fdl_image_params texture_params = params;
        texture_params.width0 = texture_params.height0 = 2;
        if (!fdl6_layout_image(&texture_layout, info, &texture_params, nullptr) || texture_layout.ubwc ||
            fdl_tile_mode(&texture_layout, 0) != TILE6_LINEAR || texture_layout.size > UINT32_MAX) return 9;
        const fdl_layout *layouts[3] = {&texture_layout, nullptr, nullptr};
        const fdl_view_args args = {.iova=0, .base_miplevel=0, .level_count=1,
            .base_array_layer=0, .layer_count=1, .min_lod_clamp=0,
            .swiz={PIPE_SWIZZLE_X,PIPE_SWIZZLE_Y,PIPE_SWIZZLE_Z,PIPE_SWIZZLE_W},
            .format=PIPE_FORMAT_R8G8B8A8_UNORM, .type=FDL_VIEW_TYPE_2D};
        fdl6_view view = {};
        if (gen6) fdl6_view_init<A6XX>(&view, layouts, &args, info->props.has_z24uint_s8uint);
        else fdl6_view_init<A7XX>(&view, layouts, &args, info->props.has_z24uint_s8uint);
        if (view.offset || view.ubwc_enabled) return 9;
        memcpy(texture_descriptors + 16, view.descriptor, sizeof(view.descriptor));
        texture_patches.push_back({20, NN_GFX_TEXTURE, view.offset, uint32_t(texture_layout.size)});
        // fd6_sampler_state_create<A7XX>: normalized, nearest, no mip filtering,
        // edge clamp, no comparison/aniso/LOD bias. Slot 0 remains the dummy hole.
        samplers[4] = A6XX_TEX_SAMP_0_XY_MAG(A6XX_TEX_NEAREST) |
            A6XX_TEX_SAMP_0_XY_MIN(A6XX_TEX_NEAREST) |
            A6XX_TEX_SAMP_0_WRAP_S(A6XX_TEX_CLAMP_TO_EDGE) |
            A6XX_TEX_SAMP_0_WRAP_T(A6XX_TEX_CLAMP_TO_EDGE) |
            A6XX_TEX_SAMP_0_WRAP_R(A6XX_TEX_CLAMP_TO_EDGE) | A6XX_TEX_SAMP_0_LOD_BIAS(0);
        samplers[5] = A6XX_TEX_SAMP_1_MIPFILTER_LINEAR_FAR | A6XX_TEX_SAMP_1_CUBEMAPSEAMLESSFILTOFF |
            A6XX_TEX_SAMP_1_MIN_LOD(0) | A6XX_TEX_SAMP_1_MAX_LOD(0);
        samplers[6] = A6XX_TEX_SAMP_2_REDUCTION_MODE(A6XX_REDUCTION_MODE_AVERAGE);
        // Qualify the runtime's generated address mask against full FDL views,
        // including high GPUVA bits and the adjacent DEPTH field in word 5.
        for (uint64_t iova : {UINT64_C(0), UINT64_C(64), UINT64_C(0x1234567800),
                             UINT64_C(0x10000000100), UINT64_C(0x1fffffffff000)}) {
            fdl_view_args expected_args = args;
            expected_args.iova = iova;
            fdl6_view expected = {};
            if (gen6) fdl6_view_init<A6XX>(&expected, layouts, &expected_args, info->props.has_z24uint_s8uint);
            else fdl6_view_init<A7XX>(&expected, layouts, &expected_args, info->props.has_z24uint_s8uint);
            uint32_t actual[16]; memcpy(actual, view.descriptor, sizeof(actual));
            uint64_t field = uint64_t(actual[4]) | uint64_t(actual[5]) << 32;
            field = (field & ~texture_address_mask) | (iova & texture_address_mask);
            actual[4] = field; actual[5] = field >> 32;
            if (memcmp(actual, expected.descriptor, sizeof(actual))) return 10;
        }
    }
    const auto color_format = fd6_color_format(params.format, TILE6_LINEAR);
    const auto color_swap = fd6_color_swap(params.format, TILE6_LINEAR, false);
    std::vector<uint32_t> cs;
    std::vector<nn_gfx_patch> patches;
    std::vector<nn_gfx_state_patch> state_patches;
    auto p7 = [&](uint32_t op, uint32_t count) { cs.push_back(pm4_pkt7_hdr(op, count)); };
    auto reg = [&](uint32_t r, uint32_t v = 0) { cs.push_back(pm4_pkt4_hdr(r, 1)); cs.push_back(v); };
    auto state_reg = [&](uint32_t r, uint32_t baseline, uint32_t kind,
                         uint32_t mask, uint32_t value0, uint32_t value1) {
        const uint32_t word = cs.size() + 1;
        reg(r, baseline);
        if (mode_aware) state_patches.push_back({word, r, pm4_pkt4_hdr(r, 1), baseline,
                                               mask, kind, {value0, value1}});
    };
    auto address = [&](uint32_t kind, uint32_t bytes) {
        patches.push_back({uint32_t(cs.size()), kind, 0, bytes}); cs.insert(cs.end(), {0, 0});
    };
    auto reg_addr = [&](uint32_t r, uint32_t kind, uint32_t bytes) {
        cs.push_back(pm4_pkt4_hdr(r, 2)); address(kind, bytes);
    };
    auto zero_addr = [&](uint32_t r) { cs.push_back(pm4_pkt4_hdr(r, 2)); cs.insert(cs.end(), {0, 0}); };
    uint32_t sequence = 0;
    auto event = [&](fd_gpu_event e) {
        const auto event_info = gen6 ? fd_gpu_events<A6XX>[e] : fd_gpu_events<A7XX>[e];
        p7(CP_EVENT_WRITE, event_info.needs_seqno ? 4 : 1);
        if (gen6) cs.push_back(CP_EVENT_WRITE_0_EVENT(event_info.raw_event) |
            (event_info.needs_seqno ? CP_EVENT_WRITE_0_TIMESTAMP : 0));
        else cs.push_back(CP_EVENT_WRITE7_0_EVENT(event_info.raw_event) |
            CP_EVENT_WRITE7_0_WRITE_SRC(EV_WRITE_USER_32B) | CP_EVENT_WRITE7_0_WRITE_DST(EV_DST_RAM));
        if (event_info.needs_seqno) { address(NN_GFX_SEQUENCE, 4); cs.push_back(++sequence); }
    };
    // Gallium fd6_emit_restore: never depend on an earlier submit's state.
    p7(CP_SET_MODE, 1); cs.push_back(0);
    if (!gen6) {
        p7(CP_THREAD_CONTROL, 1); cs.push_back(CP_THREAD_CONTROL_0_THREAD(CP_SET_THREAD_BR) |
            CP_THREAD_CONTROL_0_CONCURRENT_BIN_DISABLE);
    }
    event(FD_CCU_INVALIDATE_COLOR); event(FD_CCU_INVALIDATE_DEPTH);
    if (!gen6) event(FD_LRZ_INVALIDATE);
    event(FD_CACHE_INVALIDATE); if (!gen6) p7(CP_WAIT_FOR_IDLE, 0);
    if (gen6) reg(REG_A6XX_SP_UPDATE_CNTL, A6XX_SP_UPDATE_CNTL_VS_STATE | A6XX_SP_UPDATE_CNTL_HS_STATE |
        A6XX_SP_UPDATE_CNTL_DS_STATE | A6XX_SP_UPDATE_CNTL_GS_STATE | A6XX_SP_UPDATE_CNTL_FS_STATE |
        A6XX_SP_UPDATE_CNTL_CS_STATE | A6XX_SP_UPDATE_CNTL_CS_UAV | A6XX_SP_UPDATE_CNTL_GFX_UAV |
        A6XX_SP_UPDATE_CNTL_CS_SHARED_CONST | A6XX_SP_UPDATE_CNTL_GFX_SHARED_CONST |
        A6XX_SP_UPDATE_CNTL_CS_BINDLESS(0x1f) | A6XX_SP_UPDATE_CNTL_GFX_BINDLESS(0x1f));
    else reg(REG_A7XX_SP_UPDATE_CNTL, A7XX_SP_UPDATE_CNTL_VS_STATE | A7XX_SP_UPDATE_CNTL_HS_STATE |
        A7XX_SP_UPDATE_CNTL_DS_STATE | A7XX_SP_UPDATE_CNTL_GS_STATE | A7XX_SP_UPDATE_CNTL_FS_STATE |
        A7XX_SP_UPDATE_CNTL_CS_STATE | A7XX_SP_UPDATE_CNTL_CS_UAV | A7XX_SP_UPDATE_CNTL_GFX_UAV |
        A7XX_SP_UPDATE_CNTL_CS_BINDLESS(0xff) | A7XX_SP_UPDATE_CNTL_GFX_BINDLESS(0xff));
    p7(CP_WAIT_FOR_IDLE, 0);
    // Static non-context regs: public chip table, including its property masks.
    if (!gen6) reg(REG_A7XX_RB_CCU_CNTL, A7XX_RB_CCU_CNTL_GMEM_FAST_CLEAR_DISABLE);
    for (auto magic : info->magic_raw) {
        if (!magic.reg) break;
        uint32_t value = magic.value;
        if (magic.reg == REG_A6XX_TPL1_DBG_ECO_CNTL)
            value = (value & ~A6XX_TPL1_DBG_ECO_CNTL_LINEAR_MIPMAP_FALLBACK_IN_BLOCKS) |
                (info->props.supports_linear_mipmap_threshold_in_blocks ?
                 A6XX_TPL1_DBG_ECO_CNTL_LINEAR_MIPMAP_FALLBACK_IN_BLOCKS : 0);
        if (magic.reg == REG_A6XX_TPL1_DBG_ECO_CNTL1)
            value = (value & ~A6XX_TPL1_DBG_ECO_CNTL1_TP_UBWC_FLAG_HINT) |
                (info->props.enable_tp_ubwc_flag_hint ? A6XX_TPL1_DBG_ECO_CNTL1_TP_UBWC_FLAG_HINT : 0);
        if (magic.reg == REG_A6XX_SP_CHICKEN_BITS)
            value = (value & ~A6XX_SP_CHICKEN_BITS_EOLM_ENABLE) |
                (info->props.has_eolm_eogm ? A6XX_SP_CHICKEN_BITS_EOLM_ENABLE : 0);
        reg(magic.reg, value);
    }
    reg(REG_A6XX_RB_DBG_ECO_CNTL, info->magic.RB_DBG_ECO_CNTL);
    reg(REG_A6XX_SP_NC_MODE_CNTL_2); reg(REG_A6XX_VPC_LB_MODE_CNTL);
    reg(REG_A6XX_PC_CONTEXT_SWITCH_GFX_PREEMPTION_MODE); reg(REG_A6XX_SP_PERFCTR_SHADER_MASK, 0x3f);
    reg(REG_A6XX_GRAS_SC_SCREEN_SCISSOR_CNTL);
    if (!gen6) {
    const uint32_t bicubic[] = {0, 0x3fe05ff4, 0x3fa0ebee, 0x3f5193ed, 0x3f0243f0};
    for (unsigned i = 0; i < 5; i++) reg(REG_A7XX_TPL1_BICUBIC_WEIGHTS_TABLE(i), bicubic[i]);
    if (info->props.has_hw_bin_scaling) { reg(REG_A7XX_GRAS_BIN_FOVEAT); reg(REG_A7XX_RB_BIN_FOVEAT); }
    reg(REG_A7XX_RB_UNKNOWN_8E09, 7); reg(REG_A7XX_RB_A2D_DEST_BUFFER_ARRAY_PITCH);
    } else {
        reg(REG_A6XX_TPL1_UNKNOWN_B605, 0x44); reg(REG_A6XX_HLSQ_UNKNOWN_BE00, 0x80);
        reg(REG_A6XX_HLSQ_UNKNOWN_BE01); reg(REG_A6XX_HLSQ_SHARED_CONSTS); reg(REG_A6XX_VPC_UNKNOWN_9211);
    }
    // Static context regs, fd6_emit_static_context_regs. No context bunch for init registers.
    reg(REG_A6XX_SP_GFX_USIZE); reg(REG_A6XX_TPL1_PS_ROTATION_CNTL);
    reg(REG_A6XX_RB_RBP_CNTL, info->magic.RB_RBP_CNTL); reg(REG_A6XX_SP_UNKNOWN_A9A8);
    reg(REG_A6XX_VFD_MODE_CNTL, A6XX_VFD_MODE_CNTL_VERTEX | A6XX_VFD_MODE_CNTL_INSTANCE);
    if (gen6) reg(REG_A6XX_PC_RAST_STREAM_CNTL);
    reg(REG_A6XX_RB_MODE_CNTL, 0x10); reg(REG_A6XX_GRAS_MODE_CNTL, 2);
    reg(REG_A6XX_GRAS_LRZ_PS_INPUT_CNTL); reg(REG_A6XX_GRAS_LRZ_PS_SAMPLEFREQ_CNTL);
    reg(REG_A6XX_RB_UNKNOWN_8818);
    if (gen6) {
        reg(REG_A6XX_RB_UNKNOWN_8819); reg(REG_A6XX_RB_UNKNOWN_881A); reg(REG_A6XX_RB_UNKNOWN_881B);
        reg(REG_A6XX_RB_UNKNOWN_881C); reg(REG_A6XX_RB_UNKNOWN_881D); reg(REG_A6XX_RB_UNKNOWN_881E);
        reg(REG_A6XX_RB_UNKNOWN_88F0);
    } reg(REG_A6XX_VPC_REPLACE_MODE_CNTL); reg(REG_A6XX_VPC_ROTATION_CNTL);
    reg(REG_A6XX_VPC_SO_OVERRIDE, A6XX_VPC_SO_OVERRIDE_DISABLE);
    reg(REG_A6XX_VPC_RAST_STREAM_CNTL); if (!gen6) reg(REG_A7XX_VPC_RAST_STREAM_CNTL_V2);
    reg(REG_A6XX_PC_STEREO_RENDERING_CNTL); reg(REG_A6XX_TPL1_PS_SWIZZLE_CNTL);
    reg(REG_A6XX_GRAS_SU_CONSERVATIVE_RAS_CNTL); reg(REG_A6XX_GRAS_SU_VS_SIV_CNTL);
    if (gen6) reg(REG_A6XX_VPC_UNKNOWN_9210);
    reg(REG_A6XX_GRAS_SC_CNTL, A6XX_GRAS_SC_CNTL_CCUSINGLECACHELINESIZE(2));
    reg(REG_A6XX_VFD_RENDER_MODE, A6XX_VFD_RENDER_MODE_RENDER_MODE(RENDERING_PASS));
    reg(REG_A6XX_VFD_STEREO_RENDERING_CNTL); reg(REG_A6XX_VPC_SO_CNTL); reg(REG_A6XX_VPC_SO_MAPPING_WPTR);
    if (info->props.tess_use_shared) reg(REG_A6XX_PC_DGEN_SO_CNTL);
    reg(gen6 ? REG_A6XX_GRAS_LRZ_CNTL : REG_A7XX_GRAS_LRZ_CNTL);
    if (!gen6) reg(REG_A7XX_GRAS_LRZ_CNTL2);
    reg(REG_A6XX_RB_LRZ_CNTL);
    reg(REG_A6XX_RB_DEPTH_PLANE_CNTL, A6XX_RB_DEPTH_PLANE_CNTL_Z_MODE(A6XX_LATE_Z));
    reg(REG_A6XX_GRAS_SU_DEPTH_PLANE_CNTL, A6XX_GRAS_SU_DEPTH_PLANE_CNTL_Z_MODE(A6XX_LATE_Z));
    for (unsigned i = 0; i < 32; i++) reg(REG_A6XX_VFD_VERTEX_BUFFER_SIZE(i));
    reg_addr(REG_A6XX_TPL1_GFX_BORDER_COLOR_BASE, NN_GFX_ZERO, 4096);
    reg_addr(REG_A6XX_TPL1_CS_BORDER_COLOR_BASE, NN_GFX_ZERO, 4096);
    reg(REG_A6XX_PC_DGEN_SU_CONSERVATIVE_RAS_CNTL); if (!gen6) reg(REG_A7XX_VPC_UNKNOWN_CNTL);
    reg(REG_A6XX_SP_PS_CNTL_0);
    p7(CP_SET_DRAW_STATE, 3); cs.push_back(CP_SET_DRAW_STATE__0_DISABLE_ALL_GROUPS);
    cs.insert(cs.end(), {0, 0});
    if (gen6) {
        p7(CP_WAIT_FOR_IDLE, 0);
        reg(REG_A6XX_RB_CCU_CNTL,
            (!info->props.has_gmem_fast_clear ? A6XX_RB_CCU_CNTL_GMEM_FAST_CLEAR_DISABLE : 0) |
            (info->props.concurrent_resolve ? A6XX_RB_CCU_CNTL_CONCURRENT_RESOLVE : 0) |
            A6XX_RB_CCU_CNTL_DEPTH_OFFSET_HI(sys->depth_ccu_offset >> 21) |
            A6XX_RB_CCU_CNTL_COLOR_OFFSET_HI(sys->color_ccu_offset >> 21) |
            A6XX_RB_CCU_CNTL_DEPTH_CACHE_SIZE(CCU_CACHE_SIZE_FULL) |
            A6XX_RB_CCU_CNTL_COLOR_CACHE_SIZE(CCU_CACHE_SIZE_FULL) |
            A6XX_RB_CCU_CNTL_DEPTH_OFFSET(sys->depth_ccu_offset & 0x1fffff) |
            A6XX_RB_CCU_CNTL_COLOR_OFFSET(sys->color_ccu_offset & 0x1fffff));
    } else reg(REG_A7XX_RB_CCU_CACHE_CNTL,
        A7XX_RB_CCU_CACHE_CNTL_DEPTH_OFFSET_HI(sys->depth_ccu_offset >> 21) |
        A7XX_RB_CCU_CACHE_CNTL_COLOR_OFFSET_HI(sys->color_ccu_offset >> 21) |
        A7XX_RB_CCU_CACHE_CNTL_DEPTH_CACHE_SIZE(CCU_CACHE_SIZE_FULL) |
        A7XX_RB_CCU_CACHE_CNTL_COLOR_CACHE_SIZE(CCU_CACHE_SIZE_FULL) |
        A7XX_RB_CCU_CACHE_CNTL_DEPTH_OFFSET(sys->depth_ccu_offset & 0x1fffff) |
        A7XX_RB_CCU_CACHE_CNTL_COLOR_OFFSET(sys->color_ccu_offset & 0x1fffff));
    p7(CP_WAIT_FOR_IDLE, 0);
    // Direct sysmem render, fd6_emit_sysmem_prep/set_bin_size/update_render_cntl.
    p7(CP_SET_MARKER, 1); cs.push_back(A6XX_CP_SET_MARKER_0_MODE(RM6_DIRECT_RENDER));
    event(FD_LRZ_FLUSH);
    reg(REG_A6XX_GRAS_SC_WINDOW_SCISSOR_TL);
    reg(REG_A6XX_GRAS_SC_WINDOW_SCISSOR_BR, A6XX_GRAS_SC_WINDOW_SCISSOR_BR_X(width - 1) |
        A6XX_GRAS_SC_WINDOW_SCISSOR_BR_Y(height - 1));
    reg(REG_A6XX_RB_WINDOW_OFFSET); reg(REG_A6XX_RB_RESOLVE_WINDOW_OFFSET);
    reg(gen6 ? REG_A6XX_SP_WINDOW_OFFSET : REG_A7XX_SP_WINDOW_OFFSET);
    reg(REG_A6XX_TPL1_WINDOW_OFFSET);
    const auto feedback = info->props.has_lrz_feedback ? LRZ_FEEDBACK_EARLY_Z_LATE_Z : LRZ_FEEDBACK_NONE;
    reg(REG_A6XX_GRAS_SC_BIN_CNTL, A6XX_GRAS_SC_BIN_CNTL_RENDER_MODE(RENDERING_PASS) |
        A6XX_GRAS_SC_BIN_CNTL_BUFFERS_LOCATION(BUFFERS_IN_SYSMEM) |
        A6XX_GRAS_SC_BIN_CNTL_LRZ_FEEDBACK_ZMODE_MASK(feedback));
    reg(REG_A6XX_RB_CNTL, A6XX_RB_CNTL_RENDER_MODE(RENDERING_PASS) |
        A6XX_RB_CNTL_BUFFERS_LOCATION(BUFFERS_IN_SYSMEM) | A6XX_RB_CNTL_LRZ_FEEDBACK_ZMODE_MASK(feedback));
    reg(REG_A6XX_RB_RESOLVE_CNTL_3);
    if (!gen6) {
    reg(REG_A7XX_RB_BUFFER_CNTL, A7XX_RB_BUFFER_CNTL_Z_SYSMEM | A7XX_RB_BUFFER_CNTL_S_SYSMEM |
        A7XX_RB_BUFFER_CNTL_RT0_SYSMEM | A7XX_RB_BUFFER_CNTL_RT1_SYSMEM | A7XX_RB_BUFFER_CNTL_RT2_SYSMEM |
        A7XX_RB_BUFFER_CNTL_RT3_SYSMEM | A7XX_RB_BUFFER_CNTL_RT4_SYSMEM | A7XX_RB_BUFFER_CNTL_RT5_SYSMEM |
        A7XX_RB_BUFFER_CNTL_RT6_SYSMEM | A7XX_RB_BUFFER_CNTL_RT7_SYSMEM);
    reg(REG_A7XX_GRAS_LRZ_CB_CNTL);
    reg(REG_A7XX_RB_CLEAR_TARGET, A7XX_RB_CLEAR_TARGET_CLEAR_MODE(CLEAR_MODE_SYSMEM));
    }
    p7(CP_SKIP_IB2_ENABLE_GLOBAL, 1); cs.push_back(0);
    p7(CP_SKIP_IB2_ENABLE_LOCAL, 1); cs.push_back(1);
    p7(CP_SET_VISIBILITY_OVERRIDE, 1); cs.push_back(1);
    if (gen6) {
        const uint32_t value = A6XX_RB_RENDER_CNTL_CCUSINGLECACHELINESIZE(2);
        if (info->props.has_cp_reg_write) {
            p7(CP_REG_WRITE, 3); cs.push_back(CP_REG_WRITE_0_TRACKER(TRACK_RENDER_CNTL));
            cs.push_back(REG_A6XX_RB_RENDER_CNTL); cs.push_back(value);
        } else reg(REG_A6XX_RB_RENDER_CNTL, value);
    } else {
    reg(REG_A7XX_RB_RENDER_CNTL, A7XX_RB_RENDER_CNTL_RASTER_MODE(TYPE_TILED) |
        A7XX_RB_RENDER_CNTL_RASTER_DIRECTION(LR_TB));
    reg(REG_A7XX_GRAS_SU_RENDER_CNTL); reg(REG_A7XX_SP_RENDER_CNTL);
    }
    // MRT, depth/stencil and multisampling: fd6_gmem.cc emit_mrt/emit_zs/emit_msaa.
    reg(REG_A6XX_RB_MRT(0) + 2, gen6 ?
        A6XX_RB_MRT_BUF_INFO_COLOR_FORMAT(color_format) | A6XX_RB_MRT_BUF_INFO_COLOR_TILE_MODE(TILE6_LINEAR) |
            A6XX_RB_MRT_BUF_INFO_COLOR_SWAP(color_swap) :
        A7XX_RB_MRT_BUF_INFO_COLOR_FORMAT(color_format) | A7XX_RB_MRT_BUF_INFO_COLOR_TILE_MODE(TILE6_LINEAR) |
            A7XX_RB_MRT_BUF_INFO_COLOR_SWAP(color_swap));
    reg(REG_A6XX_RB_MRT_PITCH(0), A6XX_RB_MRT_PITCH(pitch));
    reg(REG_A6XX_RB_MRT_ARRAY_PITCH(0), A6XX_RB_MRT_ARRAY_PITCH(layer));
    reg_addr(REG_A6XX_RB_MRT_BASE(0), NN_GFX_TARGET, uint32_t(layout.size));
    reg(REG_A6XX_RB_MRT_BASE_GMEM(0));
    reg(REG_A6XX_SP_PS_MRT_REG(0), A6XX_SP_PS_MRT_REG_COLOR_FORMAT(color_format));
    reg_addr(REG_A6XX_RB_COLOR_FLAG_BUFFER_ADDR(0), NN_GFX_TARGET, uint32_t(layout.size));
    reg(REG_A6XX_RB_COLOR_FLAG_BUFFER_PITCH(0));
    reg(REG_A6XX_GRAS_LRZ_MRT_BUFFER_INFO_0, A6XX_GRAS_LRZ_MRT_BUFFER_INFO_0_COLOR_FORMAT(color_format));
    reg(REG_A6XX_RB_SRGB_CNTL); reg(REG_A6XX_SP_SRGB_CNTL); reg(REG_A6XX_GRAS_CL_ARRAY_SIZE);
    reg(gen6 ? REG_A6XX_RB_DEPTH_BUFFER_INFO : REG_A7XX_RB_DEPTH_BUFFER_INFO, gen6 ?
        A6XX_RB_DEPTH_BUFFER_INFO_DEPTH_FORMAT(DEPTH6_NONE) : A7XX_RB_DEPTH_BUFFER_INFO_DEPTH_FORMAT(DEPTH6_NONE));
    reg(REG_A6XX_RB_DEPTH_BUFFER_PITCH); reg(REG_A6XX_RB_DEPTH_BUFFER_ARRAY_PITCH);
    zero_addr(REG_A6XX_RB_DEPTH_BUFFER_BASE); reg(REG_A6XX_RB_DEPTH_GMEM_BASE);
    reg(REG_A6XX_GRAS_SU_DEPTH_BUFFER_INFO, A6XX_GRAS_SU_DEPTH_BUFFER_INFO_DEPTH_FORMAT(DEPTH6_NONE));
    reg(gen6 ? REG_A6XX_RB_STENCIL_BUFFER_INFO : REG_A7XX_RB_STENCIL_BUFFER_INFO); zero_addr(REG_A6XX_GRAS_LRZ_BUFFER_BASE);
    reg(REG_A6XX_GRAS_LRZ_BUFFER_PITCH); reg(REG_A6XX_GRAS_LRZ_VIEW_INFO);
    zero_addr(REG_A6XX_GRAS_LRZ_FAST_CLEAR_BUFFER_BASE); if (!gen6) reg(REG_A7XX_GRAS_LRZ_DEPTH_BUFFER_INFO);
    reg(REG_A6XX_TPL1_RAS_MSAA_CNTL, A6XX_TPL1_RAS_MSAA_CNTL_SAMPLES(MSAA_ONE));
    reg(REG_A6XX_TPL1_DEST_MSAA_CNTL, A6XX_TPL1_DEST_MSAA_CNTL_SAMPLES(MSAA_ONE) | A6XX_TPL1_DEST_MSAA_CNTL_MSAA_DISABLE);
    reg(REG_A6XX_GRAS_SC_RAS_MSAA_CNTL, A6XX_GRAS_SC_RAS_MSAA_CNTL_SAMPLES(MSAA_ONE));
    reg(REG_A6XX_GRAS_SC_DEST_MSAA_CNTL, A6XX_GRAS_SC_DEST_MSAA_CNTL_SAMPLES(MSAA_ONE) | A6XX_GRAS_SC_DEST_MSAA_CNTL_MSAA_DISABLE);
    reg(REG_A6XX_RB_RAS_MSAA_CNTL, A6XX_RB_RAS_MSAA_CNTL_SAMPLES(MSAA_ONE));
    reg(REG_A6XX_RB_DEST_MSAA_CNTL, A6XX_RB_DEST_MSAA_CNTL_SAMPLES(MSAA_ONE) | A6XX_RB_DEST_MSAA_CNTL_MSAA_DISABLE);
    reg(REG_A6XX_RB_RESOLVE_GMEM_BUFFER_INFO, A6XX_RB_RESOLVE_GMEM_BUFFER_INFO_SAMPLES(MSAA_ONE));
    reg(REG_A6XX_GRAS_SC_MSAA_SAMPLE_POS_CNTL); reg(REG_A6XX_RB_MSAA_SAMPLE_POS_CNTL);
    reg(REG_A6XX_TPL1_MSAA_SAMPLE_POS_CNTL);
    // Shader configuration and code pointers, fd6_program.cc emit_shader_regs.
    reg(REG_A6XX_SP_MODE_CNTL, A6XX_SP_MODE_CNTL_CONSTANT_DEMOTION_ENABLE | A6XX_SP_MODE_CNTL_ISAMMODE(ISAMMODE_GL));
    reg(REG_A6XX_TPL1_MODE_CNTL, A6XX_TPL1_MODE_CNTL_ISAMMODE(ISAMMODE_GL) |
        A6XX_TPL1_MODE_CNTL_TEXCOORDROUNDMODE(COORD_TRUNCATE) | A6XX_TPL1_MODE_CNTL_ARRAYCOORDROUNDMODE(COORD_TRUNCATE) |
        A6XX_TPL1_MODE_CNTL_NEARESTMIPSNAP(CLAMP_ROUND_TRUNCATE) | A6XX_TPL1_MODE_CNTL_DESTDATATYPEOVERRIDE);
    reg(gen6 ? REG_A6XX_SP_VS_CONST_CONFIG : REG_A7XX_SP_VS_CONST_CONFIG, A6XX_SP_VS_CONST_CONFIG_CONSTLEN(vs->constlen) | A6XX_SP_VS_CONST_CONFIG_ENABLED);
    reg(gen6 ? REG_A6XX_SP_HS_CONST_CONFIG : REG_A7XX_SP_HS_CONST_CONFIG); reg(gen6 ? REG_A6XX_SP_DS_CONST_CONFIG : REG_A7XX_SP_DS_CONST_CONFIG); reg(gen6 ? REG_A6XX_SP_GS_CONST_CONFIG : REG_A7XX_SP_GS_CONST_CONFIG);
    reg(gen6 ? REG_A6XX_SP_PS_CONST_CONFIG : REG_A7XX_SP_PS_CONST_CONFIG, A6XX_SP_PS_CONST_CONFIG_CONSTLEN(fs->constlen) | A6XX_SP_PS_CONST_CONFIG_ENABLED);
    reg(REG_A6XX_SP_VS_CONFIG, A6XX_SP_VS_CONFIG_ENABLED); reg(REG_A6XX_SP_HS_CONFIG);
    reg(REG_A6XX_SP_DS_CONFIG); reg(REG_A6XX_SP_GS_CONFIG); reg(REG_A6XX_SP_PS_CONFIG, A6XX_SP_PS_CONFIG_ENABLED |
        A6XX_SP_PS_CONFIG_NTEX(fs->num_samp) | A6XX_SP_PS_CONFIG_NSAMP(fs->num_samp));
    reg(REG_A6XX_SP_VS_CNTL_0, A6XX_SP_VS_CNTL_0_HALFREGFOOTPRINT(vs->info.max_half_reg + 1) |
        A6XX_SP_VS_CNTL_0_FULLREGFOOTPRINT(vs->info.max_reg + 1) |
        A6XX_SP_VS_CNTL_0_BRANCHSTACK(ir3_shader_branchstack_hw(vs)) |
        A6XX_SP_VS_CNTL_0_MERGEDREGS | (vs->early_preamble ? A6XX_SP_VS_CNTL_0_EARLYPREAMBLE : 0));
    reg(REG_A6XX_SP_VS_INSTR_SIZE, vs->instrlen); reg(REG_A6XX_SP_VS_PROGRAM_COUNTER_OFFSET);
    reg_addr(REG_A6XX_SP_VS_BASE, NN_GFX_VS_CODE, vs->info.size);
    reg(REG_A6XX_SP_VS_PVT_MEM_PARAM); zero_addr(REG_A6XX_SP_VS_PVT_MEM_BASE);
    reg(REG_A6XX_SP_VS_PVT_MEM_SIZE); reg(REG_A6XX_SP_VS_PVT_MEM_STACK_OFFSET);
    if (!gen6) reg(REG_A7XX_SP_VS_VGS_CNTL);
    else {
        p7(CP_LOAD_STATE6_GEOM, 3);
        cs.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_SHADER) | CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
            CP_LOAD_STATE6_0_STATE_BLOCK(SB6_VS_SHADER) |
            CP_LOAD_STATE6_0_NUM_UNIT(MIN2(vs->instrlen, info->props.instr_cache_size)));
        address(NN_GFX_VS_CODE, vs->info.size);
    }
    reg(REG_A6XX_SP_PS_CNTL_0, A6XX_SP_PS_CNTL_0_HALFREGFOOTPRINT(fs->info.max_half_reg + 1) |
        A6XX_SP_PS_CNTL_0_FULLREGFOOTPRINT(fs->info.max_reg + 1) |
        A6XX_SP_PS_CNTL_0_BRANCHSTACK(ir3_shader_branchstack_hw(fs)) | A6XX_SP_PS_CNTL_0_THREADSIZE(THREAD64) |
        A6XX_SP_PS_CNTL_0_VARYING | A6XX_SP_PS_CNTL_0_INOUTREGOVERLAP | A6XX_SP_PS_CNTL_0_MERGEDREGS |
        (fs->early_preamble ? A6XX_SP_PS_CNTL_0_EARLYPREAMBLE : 0) |
        (fs->need_full_quad ? A6XX_SP_PS_CNTL_0_LODPIXMASK : 0) |
        (fs->need_pixlod ? A6XX_SP_PS_CNTL_0_PIXLODENABLE : 0));
    reg(REG_A6XX_SP_PS_INSTR_SIZE, fs->instrlen); reg(REG_A6XX_SP_PS_PROGRAM_COUNTER_OFFSET);
    reg_addr(REG_A6XX_SP_PS_BASE, NN_GFX_FS_CODE, fs->info.size);
    reg(REG_A6XX_SP_PS_PVT_MEM_PARAM); zero_addr(REG_A6XX_SP_PS_PVT_MEM_BASE);
    reg(REG_A6XX_SP_PS_PVT_MEM_SIZE); reg(REG_A6XX_SP_PS_PVT_MEM_STACK_OFFSET);
    if (!gen6) reg(REG_A7XX_SP_PS_VGS_CNTL);
    else {
        p7(CP_LOAD_STATE6_FRAG, 3);
        cs.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_SHADER) | CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
            CP_LOAD_STATE6_0_STATE_BLOCK(SB6_FS_SHADER) |
            CP_LOAD_STATE6_0_NUM_UNIT(MIN2(fs->instrlen, info->props.instr_cache_size)));
        address(NN_GFX_FS_CODE, fs->info.size);
    }
    // Matrix bytes are supplied by caller. This aligned 4-vec4 indirect load is
    // exactly fd6_emit_const_bo's supported alignment, not a baked uniform.
    p7(CP_LOAD_STATE6_GEOM, 3);
    cs.push_back(CP_LOAD_STATE6_0_DST_OFF(ir3_const_state(vs)->ubo_state.range[0].offset / 16) | CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS) |
        CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) | CP_LOAD_STATE6_0_STATE_BLOCK(SB6_VS_SHADER) |
        CP_LOAD_STATE6_0_NUM_UNIT(4)); address(NN_GFX_MATRIX, 64);
    if (textured) {
        const auto *consts = ir3_const_state(fs);
        const auto &range = consts->ubo_state.range[0];
        p7(CP_LOAD_STATE6_FRAG, 3);
        cs.push_back(CP_LOAD_STATE6_0_DST_OFF(range.offset / 16) |
            CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS) | CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
            CP_LOAD_STATE6_0_STATE_BLOCK(SB6_FS_SHADER) | CP_LOAD_STATE6_0_NUM_UNIT((range.end-range.start)/16));
        address(NN_GFX_FS_UNIFORM, range.end - range.start);
        // Same placement and vec4 padding as public ir3_emit_immediates.
        if (fs->imm_state.count) {
            const unsigned count = (fs->imm_state.count + 3) & ~3u;
            if (fs->imm_state.size < count) return 9;
            p7(CP_LOAD_STATE6_FRAG, 3 + count);
            cs.push_back(CP_LOAD_STATE6_0_DST_OFF(consts->allocs.max_const_offset_vec4) |
                CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS) | CP_LOAD_STATE6_0_STATE_SRC(SS6_DIRECT) |
                CP_LOAD_STATE6_0_STATE_BLOCK(SB6_FS_SHADER) | CP_LOAD_STATE6_0_NUM_UNIT(count/4));
            cs.insert(cs.end(), {0,0});
            cs.insert(cs.end(), fs->imm_state.values, fs->imm_state.values + count);
        }
        // Public fd6_texture.cc build_texture_state: base registers and two
        // indirect uploads. Texture slots are not compacted across hole zero.
        reg_addr(REG_A6XX_SP_PS_SAMPLER_BASE, NN_GFX_SAMPLERS, sizeof(samplers));
        reg_addr(REG_A6XX_SP_PS_TEXMEMOBJ_BASE, NN_GFX_TEXTURE_DESCRIPTORS, sizeof(texture_descriptors));
        reg(REG_A6XX_SP_PS_TSIZE, 2);
        p7(CP_LOAD_STATE6_FRAG, 3);
        cs.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_SHADER) | CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
            CP_LOAD_STATE6_0_STATE_BLOCK(SB6_FS_TEX) | CP_LOAD_STATE6_0_NUM_UNIT(2));
        address(NN_GFX_SAMPLERS, sizeof(samplers));
        p7(CP_LOAD_STATE6_FRAG, 3);
        cs.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS) | CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
            CP_LOAD_STATE6_0_STATE_BLOCK(SB6_FS_TEX) | CP_LOAD_STATE6_0_NUM_UNIT(2));
        address(NN_GFX_TEXTURE_DESCRIPTORS, sizeof(texture_descriptors));
    }
    // Vertex fetch and VS->VPC link. link.varmask already includes pos/psize;
    // transfer mask must describe only the FS's four varying components.
    reg_addr(REG_A6XX_VFD_VERTEX_BUFFER_BASE(0), NN_GFX_VERTICES, vertex_bytes);
    reg(REG_A6XX_VFD_VERTEX_BUFFER_SIZE(0), vertex_bytes); reg(REG_A6XX_VFD_VERTEX_BUFFER_STRIDE(0), vertex_stride);
    for (unsigned i = 0; i < attrs; i++) {
        const auto format = i + 1 == attrs ? PIPE_FORMAT_R32G32B32A32_FLOAT : PIPE_FORMAT_R32G32_FLOAT;
        reg(REG_A6XX_VFD_FETCH_INSTR_INSTR(i), A6XX_VFD_FETCH_INSTR_INSTR_IDX(0) |
            A6XX_VFD_FETCH_INSTR_INSTR_OFFSET(i * 8) |
            A6XX_VFD_FETCH_INSTR_INSTR_FORMAT(fd6_vertex_format(format)) |
            A6XX_VFD_FETCH_INSTR_INSTR_SWAP(fd6_vertex_swap(format)) |
            A6XX_VFD_FETCH_INSTR_INSTR_UNK30 | A6XX_VFD_FETCH_INSTR_INSTR_FLOAT);
        reg(REG_A6XX_VFD_FETCH_INSTR_STEP_RATE(i), 1);
        reg(REG_A6XX_VFD_DEST_CNTL_INSTR(i), A6XX_VFD_DEST_CNTL_INSTR_WRITEMASK(vs->inputs[i].compmask) |
            A6XX_VFD_DEST_CNTL_INSTR_REGID(vs->inputs[i].regid));
    }
    reg(REG_A6XX_VFD_CNTL_0, A6XX_VFD_CNTL_0_FETCH_CNT(attrs) | A6XX_VFD_CNTL_0_DECODE_CNT(attrs));
    reg(REG_A6XX_VFD_CNTL_1, A6XX_VFD_CNTL_1_REGID4VTX(INVALID_REG) |
        A6XX_VFD_CNTL_1_REGID4INST(INVALID_REG) | A6XX_VFD_CNTL_1_REGID4PRIMID(INVALID_REG) |
        A6XX_VFD_CNTL_1_REGID4VIEWID(INVALID_REG));
    reg(REG_A6XX_VFD_CNTL_2, A6XX_VFD_CNTL_2_REGID_HSRELPATCHID(INVALID_REG) |
        A6XX_VFD_CNTL_2_REGID_INVOCATIONID(INVALID_REG));
    reg(REG_A6XX_VFD_CNTL_3, A6XX_VFD_CNTL_3_REGID_DSPRIMID(INVALID_REG) |
        A6XX_VFD_CNTL_3_REGID_DSRELPATCHID(INVALID_REG) | A6XX_VFD_CNTL_3_REGID_TESSX(INVALID_REG) |
        A6XX_VFD_CNTL_3_REGID_TESSY(INVALID_REG));
    reg(REG_A6XX_VFD_CNTL_4, A6XX_VFD_CNTL_4_REGID_DSVIEWID(INVALID_REG));
    reg(REG_A6XX_VFD_CNTL_5, A6XX_VFD_CNTL_5_REGID_GSHEADER(INVALID_REG) |
        A6XX_VFD_CNTL_5_REGID_GSVIEWID(INVALID_REG)); reg(REG_A6XX_VFD_CNTL_6);
    reg(REG_A6XX_VFD_INDEX_OFFSET); reg(REG_A6XX_VFD_INSTANCE_START_OFFSET); reg(REG_A6XX_PC_RESTART_INDEX, ~0u);
    uint32_t varying_mask[4] = {};
    for (unsigned i = 0; i < varying_count; i++) {
        unsigned location = fs->inputs[i].inloc;
        for (unsigned bit = 0; bit < 4; bit++) if (fs->inputs[i].compmask & (1u << bit)) {
            varying_mask[location / 32] |= 1u << (location % 32); location++;
        }
    }
    for (unsigned i = 0; i < 4; i++) reg(REG_A6XX_VPC_VARYING_LM_TRANSFER_CNTL(i), ~varying_mask[i]);
    uint16_t out[4] = {}; uint8_t dest[4] = {};
    for (unsigned i = 0; i < link->cnt; i++) {
        out[i] = A6XX_SP_VS_OUTPUT_REG_A_REGID(link->var[i].regid) |
            A6XX_SP_VS_OUTPUT_REG_A_COMPMASK(link->var[i].compmask);
        dest[i] = link->var[i].loc;
    }
    for (unsigned i = 0; i < 2; i++) reg(REG_A6XX_SP_VS_OUTPUT_REG(i), uint32_t(out[i*2]) | uint32_t(out[i*2+1]) << 16);
    reg(REG_A6XX_SP_VS_VPC_DEST_REG(0), uint32_t(dest[0]) | uint32_t(dest[1]) << 8 | uint32_t(dest[2]) << 16 | uint32_t(dest[3]) << 24);
    reg(REG_A6XX_SP_VS_OUTPUT_CNTL, A6XX_SP_VS_OUTPUT_CNTL_OUT(link->cnt));
    reg(REG_A6XX_VPC_VS_CNTL, A6XX_VPC_VS_CNTL_STRIDE_IN_VPC(link->max_loc) |
        A6XX_VPC_VS_CNTL_POSITIONLOC(link->var[varying_count].loc) | A6XX_VPC_VS_CNTL_PSIZELOC(link->var[varying_count+1].loc));
    reg(REG_A6XX_VPC_VS_CLIP_CULL_CNTL, A6XX_VPC_VS_CLIP_CULL_CNTL_CLIP_DIST_03_LOC(255) |
        A6XX_VPC_VS_CLIP_CULL_CNTL_CLIP_DIST_47_LOC(255));
    reg(REG_A6XX_VPC_VS_CLIP_CULL_CNTL_V2, A6XX_VPC_VS_CLIP_CULL_CNTL_V2_CLIP_DIST_03_LOC(255) |
        A6XX_VPC_VS_CLIP_CULL_CNTL_V2_CLIP_DIST_47_LOC(255));
    reg(REG_A6XX_GRAS_CL_VS_CLIP_CULL_DISTANCE);
    reg(REG_A6XX_PC_VS_CNTL, A6XX_PC_VS_CNTL_STRIDE_IN_VPC(link->max_loc) | A6XX_PC_VS_CNTL_PSIZE);
    reg(REG_A6XX_VPC_VS_SIV_CNTL, A6XX_VPC_VS_SIV_CNTL_LAYERLOC(255) | A6XX_VPC_VS_SIV_CNTL_VIEWLOC(255) |
        A6XX_VPC_VS_SIV_CNTL_SHADINGRATELOC(255));
    reg(REG_A6XX_VPC_VS_SIV_CNTL_V2, A6XX_VPC_VS_SIV_CNTL_V2_LAYERLOC(255) | A6XX_VPC_VS_SIV_CNTL_V2_VIEWLOC(255) |
        A6XX_VPC_VS_SIV_CNTL_V2_SHADINGRATELOC(255));
    reg(REG_A6XX_PC_PS_CNTL);
    reg(REG_A6XX_VPC_PS_CNTL, A6XX_VPC_PS_CNTL_NUMNONPOSVAR(fs->total_in) | A6XX_VPC_PS_CNTL_PRIMIDLOC(link->primid_loc) |
        A6XX_VPC_PS_CNTL_VIEWIDLOC(link->viewid_loc) | A6XX_VPC_PS_CNTL_VARYING);
    for (unsigned i = 0; i < 8; i++) {
        reg(REG_A6XX_VPC_VARYING_INTERP_MODE(i)); reg(REG_A6XX_VPC_VARYING_REPLACE_MODE(i));
    }
    // FS input and output mappings, emit_fs_inputs/emit_fs_outputs/build_prog_fb_rast.
    reg(REG_A6XX_SP_PS_INITIAL_TEX_LOAD_CNTL);
    if (gen6) reg(REG_A6XX_SP_LB_PARAM_LIMIT,
        A6XX_SP_LB_PARAM_LIMIT_PRIMALLOCTHRESHOLD(info->props.prim_alloc_threshold));
    reg(gen6 ? REG_A6XX_SP_REG_PROG_ID_0 : REG_A7XX_SP_REG_PROG_ID_0, A6XX_SP_REG_PROG_ID_0_FACEREGID(INVALID_REG) |
        A6XX_SP_REG_PROG_ID_0_SAMPLEID(INVALID_REG) | A6XX_SP_REG_PROG_ID_0_SAMPLEMASK(INVALID_REG) |
        A6XX_SP_REG_PROG_ID_0_CENTERRHW(INVALID_REG));
    reg(gen6 ? REG_A6XX_SP_REG_PROG_ID_1 : REG_A7XX_SP_REG_PROG_ID_1, A6XX_SP_REG_PROG_ID_1_IJ_PERSP_PIXEL(fs->inputs[varying_count].regid) |
        A6XX_SP_REG_PROG_ID_1_IJ_LINEAR_PIXEL(INVALID_REG) | A6XX_SP_REG_PROG_ID_1_IJ_PERSP_CENTROID(INVALID_REG) |
        A6XX_SP_REG_PROG_ID_1_IJ_LINEAR_CENTROID(INVALID_REG));
    reg(gen6 ? REG_A6XX_SP_REG_PROG_ID_2 : REG_A7XX_SP_REG_PROG_ID_2, A6XX_SP_REG_PROG_ID_2_IJ_PERSP_SAMPLE(INVALID_REG) |
        A6XX_SP_REG_PROG_ID_2_IJ_LINEAR_SAMPLE(INVALID_REG) | A6XX_SP_REG_PROG_ID_2_XYCOORDREGID(INVALID_REG) |
        A6XX_SP_REG_PROG_ID_2_ZWCOORDREGID(INVALID_REG));
    reg(gen6 ? REG_A6XX_SP_REG_PROG_ID_3 : REG_A7XX_SP_REG_PROG_ID_3, A6XX_SP_REG_PROG_ID_3_LINELENGTHREGID(INVALID_REG) |
        A6XX_SP_REG_PROG_ID_3_FOVEATIONQUALITYREGID(INVALID_REG));
    if (!gen6) reg(REG_A7XX_SP_PS_CNTL_1, A7XX_SP_PS_CNTL_1_SYSVAL_REGS_COUNT(2) |
        A7XX_SP_PS_CNTL_1_DEFER_WAVE_ALLOC_DIS | A7XX_SP_PS_CNTL_1_EVICT_BUF_MODE(1));
    reg(gen6 ? REG_A6XX_SP_PS_WAVE_CNTL : REG_A7XX_SP_PS_WAVE_CNTL,
        A6XX_SP_PS_WAVE_CNTL_THREADSIZE(THREAD64) | A6XX_SP_PS_WAVE_CNTL_VARYINGS);
    reg(REG_A6XX_GRAS_CL_INTERP_CNTL, A6XX_GRAS_CL_INTERP_CNTL_IJ_PERSP_PIXEL);
    reg(REG_A6XX_RB_INTERP_CNTL, A6XX_RB_INTERP_CNTL_IJ_PERSP_PIXEL | A6XX_RB_INTERP_CNTL_INTERP_EN);
    reg(REG_A6XX_RB_PS_INPUT_CNTL, A6XX_RB_PS_INPUT_CNTL_FRAGCOORDSAMPLEMODE(FRAGCOORD_CENTER));
    reg(REG_A6XX_RB_PS_SAMPLEFREQ_CNTL);
    reg(REG_A6XX_GRAS_LRZ_PS_INPUT_CNTL, A6XX_GRAS_LRZ_PS_INPUT_CNTL_FRAGCOORDSAMPLEMODE(FRAGCOORD_CENTER));
    reg(REG_A6XX_SP_PS_OUTPUT_CNTL, A6XX_SP_PS_OUTPUT_CNTL_DEPTH_REGID(INVALID_REG) |
        A6XX_SP_PS_OUTPUT_CNTL_SAMPMASK_REGID(INVALID_REG) | A6XX_SP_PS_OUTPUT_CNTL_STENCILREF_REGID(INVALID_REG));
    reg(REG_A6XX_SP_PS_OUTPUT_REG(0), A6XX_SP_PS_OUTPUT_REG_REGID(ir3_find_output_regid(fs, FRAG_RESULT_DATA0)));
    if (!gen6) { reg(REG_A7XX_SP_PS_OUTPUT_CONST_CNTL); reg(REG_A7XX_SP_PS_OUTPUT_CONST_MASK); }
    reg(REG_A6XX_RB_PS_OUTPUT_CNTL); reg(REG_A6XX_RB_PS_MRT_CNTL, A6XX_RB_PS_MRT_CNTL_MRT(1));
    reg(REG_A6XX_SP_PS_MRT_CNTL, A6XX_SP_PS_MRT_CNTL_MRT(1));
    reg(REG_A6XX_SP_PS_OUTPUT_MASK, 15); reg(REG_A6XX_RB_PS_OUTPUT_MASK, 15);
    // Viewport and conservative fixed raster state for this one viewport.
    reg(REG_A6XX_GRAS_CL_VIEWPORT(0) + 0, fui(32.0f)); reg(REG_A6XX_GRAS_CL_VIEWPORT(0) + 1, fui(32.0f));
    reg(REG_A6XX_GRAS_CL_VIEWPORT(0) + 2, fui(32.0f));
    state_reg(REG_A6XX_GRAS_CL_VIEWPORT(0) + 3, fui(32.0f), NN_GFX_STATE_VIEWPORT_YSCALE,
              UINT32_MAX, fui(32.0f), fui(-32.0f));
    state_reg(REG_A6XX_GRAS_CL_VIEWPORT(0) + 4, 0, NN_GFX_STATE_VIEWPORT_ZOFFSET,
              UINT32_MAX, fui(.5f), 0);
    state_reg(REG_A6XX_GRAS_CL_VIEWPORT(0) + 5, fui(1.0f), NN_GFX_STATE_VIEWPORT_ZSCALE,
              UINT32_MAX, fui(.5f), fui(1.0f));
    reg(REG_A6XX_GRAS_SC_VIEWPORT_SCISSOR(0) + 0);
    reg(REG_A6XX_GRAS_SC_VIEWPORT_SCISSOR(0) + 1, A6XX_GRAS_SC_VIEWPORT_SCISSOR_BR_X(63) | A6XX_GRAS_SC_VIEWPORT_SCISSOR_BR_Y(63));
    reg(REG_A6XX_GRAS_SC_SCREEN_SCISSOR(0) + 0);
    reg(REG_A6XX_GRAS_SC_SCREEN_SCISSOR(0) + 1, A6XX_GRAS_SC_SCREEN_SCISSOR_BR_X(63) | A6XX_GRAS_SC_SCREEN_SCISSOR_BR_Y(63));
    reg(REG_A6XX_GRAS_CL_GUARDBAND_CLIP_ADJ);
    state_reg(REG_A6XX_GRAS_CL_CNTL, (gen6 ? 0 : A6XX_GRAS_CL_CNTL_Z_CLAMP_ENABLE) |
              A6XX_GRAS_CL_CNTL_ZERO_GB_SCALE_Z | A6XX_GRAS_CL_CNTL_VP_CLIP_CODE_IGNORE,
              NN_GFX_STATE_CLIP_HALFZ, A6XX_GRAS_CL_CNTL_ZERO_GB_SCALE_Z,
              0, A6XX_GRAS_CL_CNTL_ZERO_GB_SCALE_Z);
    reg(REG_A6XX_GRAS_SU_CNTL, A6XX_GRAS_SU_CNTL_LINEHALFWIDTH(0.5f) | A6XX_GRAS_SU_CNTL_LINE_MODE(BRESENHAM));
    reg(REG_A6XX_GRAS_SU_POINT_MINMAX, A6XX_GRAS_SU_POINT_MINMAX_MIN(1.0f) | A6XX_GRAS_SU_POINT_MINMAX_MAX(1.0f));
    reg(REG_A6XX_GRAS_SU_POINT_SIZE, A6XX_GRAS_SU_POINT_SIZE(1.0f));
    reg(REG_A6XX_GRAS_SU_POLY_OFFSET_SCALE); reg(REG_A6XX_GRAS_SU_POLY_OFFSET_OFFSET); reg(REG_A6XX_GRAS_SU_POLY_OFFSET_OFFSET_CLAMP);
    reg(REG_A6XX_PC_CNTL, A6XX_PC_CNTL_PROVOKING_VTX_LAST);
    if (!gen6) reg(REG_A7XX_VPC_PC_CNTL, A7XX_VPC_PC_CNTL_PROVOKING_VTX_LAST);
    reg(REG_A6XX_VPC_RAST_CNTL, A6XX_VPC_RAST_CNTL_MODE(POLYMODE6_TRIANGLES));
    reg(REG_A6XX_PC_DGEN_RAST_CNTL, A6XX_PC_DGEN_RAST_CNTL_MODE(POLYMODE6_TRIANGLES));
    if (!gen6 || info->props.is_a702) reg(REG_A6XX_VPC_PS_RAST_CNTL, A6XX_VPC_PS_RAST_CNTL_MODE(POLYMODE6_TRIANGLES));
    if (!gen6) {
        reg(REG_A6XX_GRAS_CL_VIEWPORT_ZCLAMP(0)); reg(REG_A6XX_GRAS_CL_VIEWPORT_ZCLAMP(0) + 1, fui(1.0f));
        reg(REG_A6XX_RB_VIEWPORT_ZCLAMP_MIN); reg(REG_A6XX_RB_VIEWPORT_ZCLAMP_MAX, fui(1.0f));
    }
    reg(REG_A6XX_RB_MRT_BLEND_CONTROL(0), A6XX_RB_MRT_BLEND_CONTROL_RGB_SRC_FACTOR(FACTOR_ONE) |
        A6XX_RB_MRT_BLEND_CONTROL_RGB_DEST_FACTOR(FACTOR_ZERO) |
        A6XX_RB_MRT_BLEND_CONTROL_RGB_BLEND_OPCODE(BLEND_DST_PLUS_SRC) |
        A6XX_RB_MRT_BLEND_CONTROL_ALPHA_SRC_FACTOR(FACTOR_ONE) |
        A6XX_RB_MRT_BLEND_CONTROL_ALPHA_DEST_FACTOR(FACTOR_ZERO) |
        A6XX_RB_MRT_BLEND_CONTROL_ALPHA_BLEND_OPCODE(BLEND_DST_PLUS_SRC));
    reg(REG_A6XX_RB_MRT_CONTROL(0), A6XX_RB_MRT_CONTROL_ROP_CODE(ROP_COPY) | A6XX_RB_MRT_CONTROL_COMPONENT_ENABLE(15));
    reg(REG_A6XX_RB_DITHER_CNTL); reg(REG_A6XX_SP_BLEND_CNTL);
    reg(REG_A6XX_RB_BLEND_CNTL, A6XX_RB_BLEND_CNTL_SAMPLE_MASK(0xffff));
    reg(REG_A6XX_RB_ALPHA_TEST_CNTL); reg(REG_A6XX_RB_STENCIL_CNTL); reg(REG_A6XX_GRAS_SU_STENCIL_CNTL);
    reg(REG_A6XX_RB_STENCIL_MASK); reg(REG_A6XX_RB_STENCIL_WRITE_MASK); reg(REG_A6XX_RB_STENCIL_REF_CNTL);
    reg(REG_A6XX_RB_DEPTH_CNTL, gen6 ? 0 : A6XX_RB_DEPTH_CNTL_Z_CLAMP_ENABLE);
    reg(REG_A6XX_GRAS_SU_DEPTH_CNTL); reg(REG_A6XX_RB_DEPTH_BOUND_MIN); reg(REG_A6XX_RB_DEPTH_BOUND_MAX, fui(1.0f));
    // Non-indexed direct draw; visibility is explicitly overridden above.
    p7(CP_DRAW_INDX_OFFSET, 3);
    cs.push_back(CP_DRAW_INDX_OFFSET_0_PRIM_TYPE(DI_PT_TRILIST) |
        CP_DRAW_INDX_OFFSET_0_SOURCE_SELECT(DI_SRC_SEL_AUTO_INDEX) | CP_DRAW_INDX_OFFSET_0_VIS_CULL(USE_VISIBILITY));
    cs.push_back(1); cs.push_back(3);
    // fd6_emit_sysmem_fini + fd6_emit_flushes: CCU clean precedes invalidate.
    event(FD_CCU_CLEAN_COLOR); event(FD_CCU_CLEAN_DEPTH); event(FD_CCU_INVALIDATE_COLOR); event(FD_CCU_INVALIDATE_DEPTH);
    event(FD_CACHE_CLEAN); p7(CP_WAIT_FOR_IDLE, 0);
    if (!gen6) { p7(CP_CCHE_INVALIDATE, 0); p7(CP_WAIT_FOR_IDLE, 0); }
    p7(CP_SKIP_IB2_ENABLE_GLOBAL, 1); cs.push_back(0); event(FD_LRZ_FLUSH); p7(CP_WAIT_FOR_IDLE, 0);
    p7(CP_WAIT_FOR_ME, 0);
    // The normal KGSL IB's timestamp is the completion primitive. No conditional
    // kernel preamble flags or firmware preemption amble contract is introduced.
    if (mode_aware) {
        if (state_patches.size() != NN_GFX_STATE_KIND_COUNT) return 9;
        for (auto p : state_patches) {
            if (!p.word || p.word >= cs.size() || cs[p.word-1] != p.header || cs[p.word] != p.expected) return 9;
            for (auto a : patches)
                if (p.word-1 <= a.word+1 && a.word <= p.word) return 9;
        }
    }
    char path[4096]; snprintf(path, sizeof(path), "%s.graphics.h", prefix);
    FILE *file = fopen(path, "w"); if (!file) return 5;
    fprintf(file, gen6 ? "/* Generated native A6XX graphics artifact; see build record. */\n#pragma once\n#include \"nvn_native/graphics_image.h\"\n" :
        "/* Generated native A735 graphics artifact; see build record. */\n#pragma once\n#include \"nvn_native/graphics_image.h\"\n");
    auto array = [&](const char *name, const uint32_t *data, size_t count) {
        fprintf(file, "static const uint32_t %s[] = {\n", name);
        for (size_t i = 0; i < count; i++) fprintf(file, "0x%08xu,%s", data[i], i % 8 == 7 ? "\n" : "");
        fprintf(file, "\n};\n");
    };
    array("nn_gfx_vs_code", vs->bin, vs->info.size / 4); array("nn_gfx_fs_code", fs->bin, fs->info.size / 4);
    array("nn_gfx_commands", cs.data(), cs.size());
    if (textured) {
        array("nn_gfx_texture_descriptors", texture_descriptors, 32);
        array("nn_gfx_samplers", samplers, 8);
        fprintf(file, "static const struct nn_gfx_patch nn_gfx_texture_patches[] = {\n");
        for (auto p : texture_patches) fprintf(file, "{%uu,%uu,%uu,%uu},\n", p.word, p.kind, p.offset, p.bytes);
        fprintf(file, "};\n");
    }
    if (mode_aware) {
        fprintf(file, "static const struct nn_gfx_state_patch nn_gfx_state_patches[] = {\n");
        for (auto p : state_patches) fprintf(file, "{%uu,0x%xu,0x%08xu,0x%08xu,0x%08xu,%uu,{0x%08xu,0x%08xu}},\n",
            p.word, p.reg, p.header, p.expected, p.mask, p.kind, p.value[0], p.value[1]);
        fprintf(file, "};\n");
    }
    fprintf(file, "static const struct nn_gfx_patch nn_gfx_patches[] = {\n");
    for (auto p : patches) fprintf(file, "{%uu,%uu,%uu,%uu},\n", p.word, p.kind, p.offset, p.bytes);
    fprintf(file, "};\nstatic const struct nn_gfx_image nn_gfx_image = {\n"
        ".schema=%u,.chip=0x%08xu,.gmem_bytes=%uu,.width=%uu,.height=%uu,\n"
        ".target_pitch=%uu,.target_layer_bytes=%uu,.target_bytes=%uu,.target_alignment=%uu,\n"
        ".code_alignment=128,.vertex_bytes=%u,.matrix_bytes=64,.zero_bytes=4096,\n"
        ".vs_code=nn_gfx_vs_code,.vs_code_bytes=%uu,.fs_code=nn_gfx_fs_code,.fs_code_bytes=%uu,\n"
        ".commands=nn_gfx_commands,.command_words=%zuu,.patches=nn_gfx_patches,.patch_count=%zuu\n",
        schema, chip_id, gmem_bytes, width, height, pitch, layer, uint32_t(layout.size), layout.base_align,
        vertex_bytes, vs->info.size, fs->info.size, cs.size(), patches.size());
    if ((gen6 || mode_aware) && !textured) fprintf(file, ",.profile=0,.vertex_stride=%u\n", vertex_stride);
    if (textured) fprintf(file,
        ",.profile=1,.vertex_stride=%u,.fs_uniform_bytes=64,.fs_uniform_binding=%u,\n"
        ".texture_width=2,.texture_height=2,.texture_pitch=%u,.texture_layer_bytes=%u,\n"
        ".texture_bytes=%u,.texture_alignment=%u,.texture_slots=2,\n"
        ".texture_descriptors=nn_gfx_texture_descriptors,.texture_descriptor_words=32,\n"
        ".samplers=nn_gfx_samplers,.sampler_words=8,\n"
        ".texture_patches=nn_gfx_texture_patches,.texture_patch_count=%zu,\n"
        ".texture_address_mask=0x%016llxull\n", vertex_stride, ir3_const_state(fs)->ubo_state.range[0].ubo.block,
        fdl_pitch(&texture_layout,0), fdl_layer_stride(&texture_layout,0), uint32_t(texture_layout.size),
        texture_layout.base_align, texture_patches.size(), (unsigned long long)texture_address_mask);
    if (gen6) fprintf(file, ",.sequence_bytes=4\n");
    if (mode_aware) fprintf(file, ",.state_patches=nn_gfx_state_patches,.state_patch_count=%zu\n", state_patches.size());
    fprintf(file, "};\n#define NVN_GRAPHICS_IMAGE nn_gfx_image\n");
    if (fclose(file)) return 7;
    snprintf(path, sizeof(path), "%s.graphics.json", prefix); file = fopen(path, "w"); if (!file) return 5;
    fprintf(file, "{\"schema\":%u,\"chip\":%u,\"gmem_bytes\":%u,\"width\":%u,\"height\":%u,"
        "\"target_pitch\":%u,\"target_layer_bytes\":%u,\"target_bytes\":%u,\"target_alignment\":%u,"
        "\"vs_code_bytes\":%u,\"fs_code_bytes\":%u,\"command_words\":%zu,\"patches\":[",
        schema, chip_id, gmem_bytes, width, height, pitch, layer, uint32_t(layout.size), layout.base_align,
        vs->info.size, fs->info.size, cs.size());
    for (size_t i = 0; i < patches.size(); i++) {
        auto p = patches[i]; fprintf(file, "%s{\"word\":%u,\"kind\":%u,\"offset\":%u,\"bytes\":%u}",
            i ? "," : "", p.word, p.kind, p.offset, p.bytes);
    }
    if (textured) fprintf(file,
        "],\"profile\":1,\"vertex_stride\":%u,\"fs_uniform_binding\":%u,\"fs_uniform_bytes\":64,"
        "\"texture\":{\"width\":2,\"height\":2,\"pitch\":%u,\"layer_bytes\":%u,"
        "\"bytes\":%u,\"alignment\":%u,\"slots\":2,\"descriptor_words\":32,\"sampler_words\":8,"
        "\"normalized\":true,\"filter\":\"nearest\",\"wrap\":\"clamp_to_edge\",\"mips\":1},\"texture_patches\":[",
        vertex_stride, ir3_const_state(fs)->ubo_state.range[0].ubo.block, fdl_pitch(&texture_layout,0),
        fdl_layer_stride(&texture_layout,0), uint32_t(texture_layout.size), texture_layout.base_align);
    if (textured) for (size_t i=0;i<texture_patches.size();i++) {
        auto p=texture_patches[i]; fprintf(file,"%s{\"word\":%u,\"kind\":%u,\"offset\":%u,\"bytes\":%u}",
            i ? "," : "",p.word,p.kind,p.offset,p.bytes);
    }
    fprintf(file, "]");
    if (gen6) fprintf(file, ",\"sequence_bytes\":4,\"gpu_generation\":6");
    if (mode_aware) {
        if (!textured) fprintf(file, ",\"profile\":0,\"vertex_stride\":%u", vertex_stride);
        fprintf(file, ",\"scalar_state\":{\"snapshot_required\":true,\"full_viewport_scissor_only\":true,"
            "\"no_cull_depth_fragcoord\":true,\"patches\":[");
        for (size_t i = 0; i < state_patches.size(); ++i) {
            auto p = state_patches[i];
            fprintf(file, "%s{\"word\":%u,\"reg\":%u,\"header\":%u,\"expected\":%u,\"mask\":%u,"
                "\"kind\":%u,\"values\":[%u,%u]}", i ? "," : "", p.word, p.reg,
                p.header, p.expected, p.mask, p.kind, p.value[0], p.value[1]);
        }
        fprintf(file, "]}");
    }
    fprintf(file, ",\"execution\":\"native PM4 template; GPU execution requires KGSL submission\"}\n");
    return fclose(file) ? 7 : 0;
}

/* Preserve the legacy emitter and every command/code byte for existing callers.
 * The separate opt-in entry emits identical raw baseline words plus schema3
 * declarations; the runtime must receive an explicit recorded draw snapshot. */
extern "C" int nvn_graphics_artifact(const ir3_shader_variant *vs, const ir3_shader_variant *fs,
    const ir3_shader_linkage *link, const fd6_gmem_config *sys, uint32_t gmem_bytes, const char *prefix)
{ return graphics_artifact(vs, fs, link, sys, gmem_bytes, prefix, false); }

extern "C" int nvn_graphics_artifact_modes(const ir3_shader_variant *vs, const ir3_shader_variant *fs,
    const ir3_shader_linkage *link, const fd6_gmem_config *sys, uint32_t gmem_bytes, const char *prefix)
{ return graphics_artifact(vs, fs, link, sys, gmem_bytes, prefix, true); }
