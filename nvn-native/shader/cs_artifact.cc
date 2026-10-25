/* SPDX-License-Identifier: MIT
 * State sequence adapted from Mesa computerator/a6xx.cc (Google, Inc., MIT).
 * Register encoding and the buffer descriptor use Mesa's existing generators/FDL.
 * This bounded fixture is not a general GPU replay implementation.
 */
#include <cstdio>
#include <cstring>
#include <vector>
#include "freedreno_pm4.h"
#include "fd6_hw.h"
#include "freedreno_layout.h"
#include "ir3/ir3_shader.h"

extern "C" int nvn_cs_artifact(const ir3_shader_variant *v, uint32_t expected, const char *prefix)
{
    constexpr uint32_t code_offset = 4096, desc_offset = 8192;
    constexpr uint32_t output_offset = 12288, seq_offset = 12352, bo_bytes = 16384;
    if (v->constlen || v->pvtmem_size || v->shared_size || v->num_samp ||
        v->num_uavs != 1 || v->local_size[0] != 1 || v->local_size[1] != 1 ||
        v->local_size[2] != 1 || v->info.size > 4096) return 9;
    std::vector<uint32_t> cs, reloc;
    auto p7 = [&](uint32_t op, uint32_t count) { cs.push_back(pm4_pkt7_hdr(op, count)); };
    auto reg = [&](uint32_t address, uint32_t value) {
        cs.push_back(pm4_pkt4_hdr(address, 1)); cs.push_back(value);
    };
    auto address = [&](uint32_t offset) {
        reloc.push_back(cs.size()); cs.push_back(offset); cs.push_back(0);
    };
    auto reg_address = [&](uint32_t r, uint32_t offset) {
        cs.push_back(pm4_pkt4_hdr(r, 2)); address(offset);
    };
    const auto *info = v->compiler->info;
    reg(REG_A6XX_SP_PERFCTR_SHADER_MASK, A6XX_SP_PERFCTR_SHADER_MASK_CS);
    reg(REG_A6XX_SP_NC_MODE_CNTL_2, 0);
    for (auto magic : info->magic_raw) { if (!magic.reg) break; reg(magic.reg, magic.value); }
    reg(REG_A6XX_SP_MODE_CNTL, A6XX_SP_MODE_CNTL_CONSTANT_DEMOTION_ENABLE |
        A6XX_SP_MODE_CNTL_ISAMMODE(ISAMMODE_GL));
    reg(REG_A7XX_SP_UPDATE_CNTL, A7XX_SP_UPDATE_CNTL_VS_STATE |
        A7XX_SP_UPDATE_CNTL_HS_STATE | A7XX_SP_UPDATE_CNTL_DS_STATE |
        A7XX_SP_UPDATE_CNTL_GS_STATE | A7XX_SP_UPDATE_CNTL_FS_STATE |
        A7XX_SP_UPDATE_CNTL_CS_STATE | A7XX_SP_UPDATE_CNTL_GFX_UAV);
    reg(REG_A7XX_SP_CS_CONST_CONFIG, A7XX_SP_CS_CONST_CONFIG_ENABLED);
    reg(REG_A6XX_SP_CS_CONFIG, A6XX_SP_CS_CONFIG_ENABLED | A6XX_SP_CS_CONFIG_NUAV(1));
    reg(REG_A6XX_SP_CS_INSTR_SIZE, v->instrlen);
    reg(REG_A6XX_SP_CS_CNTL_0,
        A6XX_SP_CS_CNTL_0_FULLREGFOOTPRINT(v->info.max_reg + 1) |
        A6XX_SP_CS_CNTL_0_HALFREGFOOTPRINT(v->info.max_half_reg + 1) |
        A6XX_SP_CS_CNTL_0_THREADSIZE(THREAD64) |
        (v->mergedregs ? A6XX_SP_CS_CNTL_0_MERGEDREGS : 0));
    reg(REG_A7XX_SP_PS_WAVE_CNTL, A7XX_SP_PS_WAVE_CNTL_THREADSIZE(THREAD64));
    reg(REG_A7XX_SP_REG_PROG_ID_0, 0xfcfcfcfc);
    reg(REG_A7XX_SP_REG_PROG_ID_1, 0xfcfcfcfc);
    reg(REG_A7XX_SP_REG_PROG_ID_2, 0xfcfcfcfc);
    reg(REG_A7XX_SP_REG_PROG_ID_3, 0x0000fc00);
    reg(REG_A6XX_SP_CS_CNTL_1, A6XX_SP_CS_CNTL_1_SHARED_SIZE(1) |
        A6XX_SP_CS_CNTL_1_CONSTANTRAMMODE(CONSTLEN_128));
    reg(REG_A7XX_SP_CS_WGE_CNTL, A7XX_SP_CS_WGE_CNTL_LINEARLOCALIDREGID(0xfc) |
        A7XX_SP_CS_WGE_CNTL_THREADSIZE(THREAD64) |
        A7XX_SP_CS_WGE_CNTL_WORKGROUPRASTORDERZFIRSTEN |
        A7XX_SP_CS_WGE_CNTL_WGTILEWIDTH(4) | A7XX_SP_CS_WGE_CNTL_WGTILEHEIGHT(17));
    reg(REG_A6XX_SP_CS_WIE_CNTL_0, 0xfcfcfcfc);
    reg(REG_A7XX_SP_CS_WIE_CNTL_1, A7XX_SP_CS_WIE_CNTL_1_LINEARLOCALIDREGID(0xfc) |
        A7XX_SP_CS_WIE_CNTL_1_THREADSIZE(THREAD64) |
        A7XX_SP_CS_WIE_CNTL_1_WORKITEMRASTORDER(WORKITEMRASTORDER_LINEAR));
    reg_address(REG_A6XX_SP_CS_BASE, code_offset);
    p7(CP_LOAD_STATE6_FRAG, 3);
    cs.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_SHADER) |
        CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
        CP_LOAD_STATE6_0_STATE_BLOCK(SB6_CS_SHADER) | CP_LOAD_STATE6_0_NUM_UNIT(v->instrlen));
    address(code_offset);
    p7(CP_LOAD_STATE6_FRAG, 3);
    cs.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_UAV) |
        CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
        CP_LOAD_STATE6_0_STATE_BLOCK(SB6_CS_SHADER) | CP_LOAD_STATE6_0_NUM_UNIT(1));
    address(desc_offset);
    reg_address(REG_A7XX_SP_CS_UAV_BASE, desc_offset);
    reg(REG_A6XX_SP_CS_USIZE, 1);
    p7(CP_SET_MARKER, 1); cs.push_back(A6XX_CP_SET_MARKER_0_MODE(RM6_COMPUTE));
    reg(REG_A7XX_SP_CS_NDRANGE_0, A7XX_SP_CS_NDRANGE_0_KERNELDIM(3));
    reg(REG_A7XX_SP_CS_NDRANGE_7, 0);
    reg(REG_A7XX_SP_CS_NDRANGE_1, 1); reg(REG_A7XX_SP_CS_NDRANGE_2, 0);
    reg(REG_A7XX_SP_CS_NDRANGE_3, 1); reg(REG_A7XX_SP_CS_NDRANGE_4, 0);
    reg(REG_A7XX_SP_CS_NDRANGE_5, 1); reg(REG_A7XX_SP_CS_NDRANGE_6, 0);
    reg(REG_A7XX_SP_CS_KERNEL_GROUP_X, 1); reg(REG_A7XX_SP_CS_KERNEL_GROUP_Y, 1);
    reg(REG_A7XX_SP_CS_KERNEL_GROUP_Z, 1);
    p7(CP_EXEC_CS, 4); cs.insert(cs.end(), {0, 1, 1, 1});
    p7(CP_WAIT_FOR_IDLE, 0);
    p7(CP_EVENT_WRITE, 4);
    cs.push_back(CP_EVENT_WRITE7_0_EVENT(RB_DONE_TS) |
        CP_EVENT_WRITE7_0_WRITE_SRC(EV_WRITE_USER_32B) | CP_EVENT_WRITE7_0_WRITE_ENABLED);
    address(seq_offset); cs.push_back(1);
    p7(CP_WAIT_REG_MEM, 6);
    cs.push_back(CP_WAIT_REG_MEM_0_FUNCTION(WRITE_EQ) | CP_WAIT_REG_MEM_0_POLL(POLL_MEMORY));
    address(seq_offset); cs.insert(cs.end(), {1, ~0u, 16});
    p7(CP_EVENT_WRITE, 1);
    cs.push_back(CP_EVENT_WRITE7_0_EVENT(CACHE_FLUSH7) | CP_EVENT_WRITE7_0_WRITE_SRC(EV_WRITE_USER_32B));
    p7(CP_WAIT_FOR_IDLE, 0); p7(CP_WAIT_FOR_ME, 0);
    if (cs.size() * 4 >= code_offset) return 9;
    std::vector<uint32_t> blob(bo_bytes / 4, 0);
    memcpy(blob.data(), cs.data(), cs.size() * 4);
    memcpy(blob.data() + code_offset / 4, v->bin, v->info.size);
    const uint8_t swiz[4] = {PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W};
    fdl6_buffer_view_init<A7XX>(blob.data() + desc_offset / 4, PIPE_FORMAT_R32_UINT, swiz,
                              output_offset, 4);
    reloc.push_back(desc_offset / 4 + 4);  // FDL A7XX base address, low/high pair.
    char path[4096];
    snprintf(path, sizeof(path), "%s.h", prefix);
    FILE *out = fopen(path, "w"); if (!out) return 5;
    fprintf(out, "/* Generated controlled CS artifact; source and hash in build record. */\n"
        "#define NVN_CS_BO_BYTES %uu\n#define NVN_CS_COMMAND_DWORDS %zuu\n"
        "#define NVN_CS_OUTPUT_OFFSET %uu\n#define NVN_CS_EXPECTED %uu\n"
        "static const uint32_t nvn_cs_relocs[] = {", bo_bytes, cs.size(), output_offset, expected);
    for (auto r : reloc) fprintf(out, "%uu,", r);
    fprintf(out, "};\nstatic const uint32_t nvn_cs_blob[] = {\n");
    // The result/sequence region must be initialized by the runtime, not this blob.
    for (size_t i = 0; i < (desc_offset + 64) / 4; i++) fprintf(out, "0x%08xu,%s", blob[i], i % 8 == 7 ? "\n" : "");
    fprintf(out, "};\n");
    return fclose(out) ? 7 : 0;
}
