/* SPDX-License-Identifier: MIT
 * Device-only A650 sysmem restore/fini. State semantics and register packers:
 * pinned Qualcomm/Freedreno common device table and Gallium fd6_emit/gmem.
 * Shares qualified project's encoding provenance, not its dimension/profile.
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include "freedreno_pm4.h"
#include "fd6_hw.h"
#include "common/fd6_gmem_cache.h"
#include "common/freedreno_gpu_event.h"
#include "nvn_pipeline_platform.h"
int main(int argc,char **argv)
{
    if(argc!=4||strlen(argv[3])>4000)return 2;
    char *end=nullptr;unsigned long long chip=strtoull(argv[1],&end,0);
    if(*end||argv[1][0]=='-'||chip!=0x06050002)return 2;
    unsigned long long bytes=strtoull(argv[2],&end,0);
    if(*end||argv[2][0]=='-'||!bytes||bytes>UINT32_MAX||bytes%4096)return 2;
    const uint32_t gmem_bytes=bytes;const bool gen6=true;
    const fd_dev_id id={.chip_id=chip};const auto *info=fd_dev_info_raw(&id);if(!info)return 2;
    const uint64_t required=uint64_t(info->num_ccu)*(info->props.sysmem_per_ccu_depth_cache_size+
        info->props.sysmem_per_ccu_color_cache_size+MAX2(info->props.sysmem_vpc_attr_buf_size,info->props.gmem_vpc_attr_buf_size));
    if(required>gmem_bytes||info->props.has_gmem_vpc_attr_buf||info->props.load_shader_consts_via_preamble)return 9;
    fd6_gmem_config gm={},sys_value={};fd6_calc_gmem_cache_offsets(info,gmem_bytes,&gm,&sys_value);
    const auto *sys=&sys_value;
    std::vector<uint32_t> cs;std::vector<nn_pipeline_platform_patch> patches;
    auto p7=[&](uint32_t op,uint32_t count){cs.push_back(pm4_pkt7_hdr(op,count));};
    auto reg=[&](uint32_t r,uint32_t v=0){cs.push_back(pm4_pkt4_hdr(r,1));cs.push_back(v);};
    auto address=[&](uint32_t kind,uint32_t size){patches.push_back({uint32_t(cs.size()),kind,0,size});cs.insert(cs.end(),{0,0});};
    auto reg_addr=[&](uint32_t r,uint32_t kind,uint32_t size){cs.push_back(pm4_pkt4_hdr(r,2));address(kind,size);};
    auto zero_addr=[&](uint32_t r){cs.push_back(pm4_pkt4_hdr(r,2));cs.insert(cs.end(),{0,0});};
    uint32_t sequence=0;
    auto event=[&](fd_gpu_event e){const auto ev=fd_gpu_events<A6XX>[e];p7(CP_EVENT_WRITE,ev.needs_seqno?4:1);
        cs.push_back(CP_EVENT_WRITE_0_EVENT(ev.raw_event)|(ev.needs_seqno?CP_EVENT_WRITE_0_TIMESTAMP:0));
        if(ev.needs_seqno){address(NN_PLATFORM_SEQUENCE,4);cs.push_back(++sequence);}};
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
    reg_addr(REG_A6XX_TPL1_GFX_BORDER_COLOR_BASE, NN_PLATFORM_ZERO, 4096);
    reg_addr(REG_A6XX_TPL1_CS_BORDER_COLOR_BASE, NN_PLATFORM_ZERO, 4096);
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

    p7(CP_SET_MARKER,1);cs.push_back(A6XX_CP_SET_MARKER_0_MODE(RM6_DIRECT_RENDER));
    event(FD_LRZ_FLUSH);
    reg(REG_A6XX_RB_WINDOW_OFFSET);reg(REG_A6XX_RB_RESOLVE_WINDOW_OFFSET);
    reg(REG_A6XX_SP_WINDOW_OFFSET);reg(REG_A6XX_TPL1_WINDOW_OFFSET);
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

    reg(REG_A6XX_PC_CNTL,A6XX_PC_CNTL_PROVOKING_VTX_LAST);
    reg(REG_A6XX_VPC_RAST_CNTL,A6XX_VPC_RAST_CNTL_MODE(POLYMODE6_TRIANGLES));
    reg(REG_A6XX_PC_DGEN_RAST_CNTL,A6XX_PC_DGEN_RAST_CNTL_MODE(POLYMODE6_TRIANGLES));
    reg(REG_A6XX_PC_RESTART_INDEX,~0u);
    reg(REG_A6XX_RB_SRGB_CNTL);reg(REG_A6XX_SP_SRGB_CNTL);reg(REG_A6XX_GRAS_CL_ARRAY_SIZE);
    const auto begin=cs;const auto begin_patches=patches;cs.clear();patches.clear();
    // fd6_emit_sysmem_fini + fd6_emit_flushes: CCU clean precedes invalidate.
    event(FD_CCU_CLEAN_COLOR); event(FD_CCU_CLEAN_DEPTH); event(FD_CCU_INVALIDATE_COLOR); event(FD_CCU_INVALIDATE_DEPTH);
    event(FD_CACHE_CLEAN); p7(CP_WAIT_FOR_IDLE, 0);
    if (!gen6) { p7(CP_CCHE_INVALIDATE, 0); p7(CP_WAIT_FOR_IDLE, 0); }
    p7(CP_SKIP_IB2_ENABLE_GLOBAL, 1); cs.push_back(0); event(FD_LRZ_FLUSH); p7(CP_WAIT_FOR_IDLE, 0);
    p7(CP_WAIT_FOR_ME, 0);

    const auto finish=cs;const auto end_patches=patches;
    char path[4096];snprintf(path,sizeof(path),"%s.platform.h",argv[3]);FILE *f=fopen(path,"w");if(!f)return 5;
    fprintf(f,"/* Generated device-only restore; no shader or draw data. */\n#pragma once\n#include \"nvn_native/pipeline_platform.h\"\n");
    auto array=[&](const char *name,const std::vector<uint32_t> &v){fprintf(f,"static const uint32_t %s[]={",name);for(auto w:v)fprintf(f,"0x%08xu,",w);fprintf(f,"};\n");};
    auto patch_array=[&](const char *name,const std::vector<nn_pipeline_platform_patch> &v){fprintf(f,"static const struct nn_pipeline_platform_patch %s[]={",name);
        if(v.empty())fprintf(f,"{0,0,0,0}");
        for(const auto &p:v)fprintf(f,"{%u,%u,%u,%u},",p.word,p.kind,p.offset,p.bytes);fprintf(f,"};\n");};
    array("nn_platform_begin",begin);array("nn_platform_end",finish);patch_array("nn_platform_begin_patches",begin_patches);patch_array("nn_platform_end_patches",end_patches);
    fprintf(f,"static const struct nn_pipeline_platform nn_pipeline_device={1,0x06050002,%u,{nn_platform_begin,%zu,nn_platform_begin_patches,%zu},{nn_platform_end,%zu,nn_platform_end_patches,%zu}};\n",
        gmem_bytes,begin.size(),begin_patches.size(),finish.size(),end_patches.size());
    return fclose(f)?5:0;
}
