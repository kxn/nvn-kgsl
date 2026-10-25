/* SPDX-License-Identifier: MIT
 * Hardware sequence follows Mesa computerator/a6xx.cc (Google, Inc., MIT).
 * Dynamic state is emitted from real IR3 metadata, generated registers and FDL.
 */
#include <cstdio>
#include <cstring>
#include <vector>
#include "freedreno_pm4.h"
#include "fd6_hw.h"
#include "freedreno_layout.h"
#include "ir3/ir3_shader.h"
#include "nvn_compute_image.h"

extern "C" int nvn_compute_artifact(const ir3_shader_variant *v, const char *prefix)
{
    if (!v || !v->compiler || !v->compiler->dev_id || !v->compiler->info) return 9;
    const uint64_t actual_chip = v->compiler->dev_id->chip_id;
    const bool a6 = actual_chip == 0x06050002 && v->compiler->gen == 6;
    const bool a7 = actual_chip == 0x43030b00 && v->compiler->gen == 7;
    if (!a6 && !a7) return 9;
    const uint32_t chip = actual_chip;
    const auto *consts = ir3_const_state(v);
    if (v->pvtmem_size || v->shared_size || v->num_samp || !v->num_uavs || v->num_uavs > 16 ||
        v->constlen > 128 || v->info.double_threadsize || v->early_preamble ||
        v->constant_data_size || consts->num_ubos ||
        consts->driver_params_ubo.idx >= 0 || v->info.size > 65536) return 9;
    // Gallium ir3_emit_immediates uploads the compiler-owned padded vec4s at
    // max_const_offset_vec4. Preserve those bytes, including allocator padding.
    if (v->imm_state.count > v->constlen * 4u) return 9;
    const unsigned imm_count = align(v->imm_state.count, 4);
    const unsigned imm_base = consts->allocs.max_const_offset_vec4;
    if (v->imm_state.count && (!v->imm_state.values ||
          v->imm_state.size < imm_count || imm_base > v->constlen ||
          imm_count / 4 > v->constlen - imm_base)) return 9;
    std::vector<uint32_t> cs;
    std::vector<nn_cs_patch> patches;
    auto p7 = [&](uint32_t op, uint32_t count) { cs.push_back(pm4_pkt7_hdr(op, count)); };
    auto reg = [&](uint32_t address, uint32_t value) {
        cs.push_back(pm4_pkt4_hdr(address, 1)); cs.push_back(value);
    };
    auto address = [&](uint32_t kind, uint32_t index = 0, uint32_t offset = 0, uint32_t bytes = 0) {
        patches.push_back({(uint32_t)cs.size(), kind, index, offset}); cs.push_back(bytes); cs.push_back(0);
    };
    auto reg_address = [&](uint32_t r, uint32_t kind) {
        cs.push_back(pm4_pkt4_hdr(r, 2)); address(kind);
    };
    auto reg_dynamic = [&](uint32_t r, uint32_t kind, uint32_t axis) {
        cs.push_back(pm4_pkt4_hdr(r, 1));
        patches.push_back({(uint32_t)cs.size(), kind, axis, 0}); cs.push_back(0);
    };
    auto emit_constants = [&]() {
        auto alloc = consts->allocs.consts[IR3_CONST_ALLOC_DRIVER_PARAMS];
        if (alloc.size_vec4) {
            uint32_t count = alloc.size_vec4 * 4;
            if (count > sizeof(ir3_driver_params_cs) / 4) return false;
            p7(CP_LOAD_STATE6_FRAG, 3 + count);
            cs.push_back(CP_LOAD_STATE6_0_DST_OFF(alloc.offset_vec4) |
                CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS) | CP_LOAD_STATE6_0_STATE_SRC(SS6_DIRECT) |
                CP_LOAD_STATE6_0_STATE_BLOCK(SB6_CS_SHADER) | CP_LOAD_STATE6_0_NUM_UNIT(alloc.size_vec4));
            cs.insert(cs.end(), {0, 0});
            ir3_driver_params_cs params = {};
            params.work_dim = 3; params.subgroup_size = 64;
            params.local_group_size_x = v->local_size[0]; params.local_group_size_y = v->local_size[1];
            params.local_group_size_z = v->local_size[2];
            for (unsigned i = 0; i < count; i++) {
                if (i < 3) patches.push_back({(uint32_t)cs.size(), NN_CS_GROUP_COUNT, i, 0});
                uint32_t value; memcpy(&value, (const char *)&params + i * 4, 4); cs.push_back(value);
            }
        }
        for (unsigned i = 0; i < consts->ubo_state.num_enabled; i++) {
            const auto &range = consts->ubo_state.range[i];
            if (range.ubo.bindless || range.ubo.global || range.ubo.block >= 16 ||
                (range.offset | range.start | range.end) % 16 || range.end <= range.start) return false;
            p7(CP_LOAD_STATE6_FRAG, 3);
            cs.push_back(CP_LOAD_STATE6_0_DST_OFF(range.offset / 16) |
                CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS) | CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
                CP_LOAD_STATE6_0_STATE_BLOCK(SB6_CS_SHADER) | CP_LOAD_STATE6_0_NUM_UNIT((range.end-range.start)/16));
            address(NN_CS_UBO, range.ubo.block, range.start, range.end - range.start);
        }
        if (imm_count) {
            p7(CP_LOAD_STATE6_FRAG, 3 + imm_count);
            cs.push_back(CP_LOAD_STATE6_0_DST_OFF(imm_base) |
                CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS) |
                CP_LOAD_STATE6_0_STATE_SRC(SS6_DIRECT) |
                CP_LOAD_STATE6_0_STATE_BLOCK(SB6_CS_SHADER) |
                CP_LOAD_STATE6_0_NUM_UNIT(imm_count / 4));
            cs.insert(cs.end(), {0, 0});
            cs.insert(cs.end(), v->imm_state.values, v->imm_state.values + imm_count);
        }
        return true;
    };
    // Reset each execution, including repeated submission of the same handle.
    p7(CP_MEM_WRITE, 3); address(NN_CS_SEQUENCE); cs.push_back(0);
    // A6 CP writes must retire before a later event overwrites this sentinel
    // and the CP polls it. WFI/WFM do not replace WAIT_MEM_WRITES: the pinned
    // Gallium fd6_query.cc uses this explicit MEM_WRITE -> wait -> event/poll
    // contract. Keep it in the normal producer, never patch generated words.
    if (a6) p7(CP_WAIT_MEM_WRITES, 0);
    p7(CP_WAIT_FOR_IDLE, 0); p7(CP_WAIT_FOR_ME, 0);
    if (a6) {
        // CPU cache maintenance does not invalidate UCHE. Invalidate before
        // indirect constant/resource loads on every execution, including a
        // replay after host writes to the same buffer. CACHE_FLUSH_TS below
        // only cleans dirty UCHE data; it does not discard cached input.
        // Same A6 event as fd6_cache_inv; wait before issuing new state loads.
        p7(CP_EVENT_WRITE, 1); cs.push_back(CP_EVENT_WRITE_0_EVENT(CACHE_INVALIDATE));
        p7(CP_WAIT_FOR_IDLE, 0); p7(CP_WAIT_FOR_ME, 0);
    }
    const auto *info = v->compiler->info;
    reg(REG_A6XX_SP_PERFCTR_SHADER_MASK, A6XX_SP_PERFCTR_SHADER_MASK_CS);
    reg(REG_A6XX_SP_NC_MODE_CNTL_2, 0);
    for (auto magic : info->magic_raw) { if (!magic.reg) break; reg(magic.reg, magic.value); }
    reg(REG_A6XX_SP_MODE_CNTL, A6XX_SP_MODE_CNTL_CONSTANT_DEMOTION_ENABLE |
        A6XX_SP_MODE_CNTL_ISAMMODE(ISAMMODE_GL));
    if (a6) {
        reg(REG_A6XX_SP_UPDATE_CNTL, A6XX_SP_UPDATE_CNTL_VS_STATE |
            A6XX_SP_UPDATE_CNTL_HS_STATE | A6XX_SP_UPDATE_CNTL_DS_STATE |
            A6XX_SP_UPDATE_CNTL_GS_STATE | A6XX_SP_UPDATE_CNTL_FS_STATE |
            A6XX_SP_UPDATE_CNTL_CS_STATE | A6XX_SP_UPDATE_CNTL_CS_UAV | A6XX_SP_UPDATE_CNTL_GFX_UAV);
        reg(REG_A6XX_SP_CS_CONST_CONFIG, A6XX_SP_CS_CONST_CONFIG_ENABLED |
            A6XX_SP_CS_CONST_CONFIG_CONSTLEN((v->constlen + 3) & ~3u));
    } else {
        reg(REG_A7XX_SP_UPDATE_CNTL, A7XX_SP_UPDATE_CNTL_VS_STATE |
            A7XX_SP_UPDATE_CNTL_HS_STATE | A7XX_SP_UPDATE_CNTL_DS_STATE |
            A7XX_SP_UPDATE_CNTL_GS_STATE | A7XX_SP_UPDATE_CNTL_FS_STATE |
            A7XX_SP_UPDATE_CNTL_CS_STATE | A7XX_SP_UPDATE_CNTL_GFX_UAV);
        reg(REG_A7XX_SP_CS_CONST_CONFIG, A7XX_SP_CS_CONST_CONFIG_ENABLED |
            A7XX_SP_CS_CONST_CONFIG_CONSTLEN((v->constlen + 3) & ~3u));
    }
    reg(REG_A6XX_SP_CS_CONFIG, A6XX_SP_CS_CONFIG_ENABLED | A6XX_SP_CS_CONFIG_NUAV(v->num_uavs));
    reg(REG_A6XX_SP_CS_INSTR_SIZE, v->instrlen);
    reg(REG_A6XX_SP_CS_CNTL_0,
        A6XX_SP_CS_CNTL_0_FULLREGFOOTPRINT(v->info.max_reg + 1) |
        A6XX_SP_CS_CNTL_0_HALFREGFOOTPRINT(v->info.max_half_reg + 1) |
        A6XX_SP_CS_CNTL_0_THREADSIZE(THREAD64) |
        A6XX_SP_CS_CNTL_0_BRANCHSTACK(ir3_shader_branchstack_hw(v)) |
        (v->mergedregs ? A6XX_SP_CS_CNTL_0_MERGEDREGS : 0));
    if (a7) {
        reg(REG_A7XX_SP_PS_WAVE_CNTL, A7XX_SP_PS_WAVE_CNTL_THREADSIZE(THREAD64));
        reg(REG_A7XX_SP_REG_PROG_ID_0, 0xfcfcfcfc);
        reg(REG_A7XX_SP_REG_PROG_ID_1, 0xfcfcfcfc);
        reg(REG_A7XX_SP_REG_PROG_ID_2, 0xfcfcfcfc);
        reg(REG_A7XX_SP_REG_PROG_ID_3, 0x0000fc00);
    }
    reg(REG_A6XX_SP_CS_CNTL_1, A6XX_SP_CS_CNTL_1_SHARED_SIZE(1) |
        A6XX_SP_CS_CNTL_1_CONSTANTRAMMODE(CONSTLEN_128));
    if (a6) {
        // A650 has the original HLSQ workgroup configuration, not A7's
        // SP tile/raster state. IDs come from this target's real IR3 inputs.
        if (info->props.has_lpac)
            reg(REG_A6XX_HLSQ_CS_CTRL_REG1, A6XX_HLSQ_CS_CTRL_REG1_SHARED_SIZE(1) |
                A6XX_HLSQ_CS_CTRL_REG1_CONSTANTRAMMODE(CONSTLEN_128));
        reg(REG_A6XX_SP_CS_CONST_CONFIG_0,
            A6XX_SP_CS_CONST_CONFIG_0_WGIDCONSTID(ir3_find_sysval_regid(v, SYSTEM_VALUE_WORKGROUP_ID)) |
            A6XX_SP_CS_CONST_CONFIG_0_WGSIZECONSTID(0xfc) |
            A6XX_SP_CS_CONST_CONFIG_0_WGOFFSETCONSTID(0xfc) |
            A6XX_SP_CS_CONST_CONFIG_0_LOCALIDREGID(ir3_find_sysval_regid(v, SYSTEM_VALUE_LOCAL_INVOCATION_ID)));
        reg(REG_A6XX_SP_CS_WGE_CNTL, A6XX_SP_CS_WGE_CNTL_LINEARLOCALIDREGID(0xfc) |
            A6XX_SP_CS_WGE_CNTL_THREADSIZE(THREAD64));
    } else {
        reg(REG_A7XX_SP_CS_WGE_CNTL, A7XX_SP_CS_WGE_CNTL_LINEARLOCALIDREGID(0xfc) |
            A7XX_SP_CS_WGE_CNTL_THREADSIZE(THREAD64) |
            A7XX_SP_CS_WGE_CNTL_WORKGROUPRASTORDERZFIRSTEN |
            A7XX_SP_CS_WGE_CNTL_WGTILEWIDTH(4) | A7XX_SP_CS_WGE_CNTL_WGTILEHEIGHT(17));
    }
    if (a7 || info->props.has_lpac) {
        reg(REG_A6XX_SP_CS_WIE_CNTL_0,
            A6XX_SP_CS_WIE_CNTL_0_WGIDCONSTID(ir3_find_sysval_regid(v, SYSTEM_VALUE_WORKGROUP_ID)) |
            A6XX_SP_CS_WIE_CNTL_0_WGSIZECONSTID(0xfc) |
            A6XX_SP_CS_WIE_CNTL_0_WGOFFSETCONSTID(0xfc) |
            A6XX_SP_CS_WIE_CNTL_0_LOCALIDREGID(ir3_find_sysval_regid(v, SYSTEM_VALUE_LOCAL_INVOCATION_ID)));
        if (a6) {
            reg(REG_A6XX_SP_CS_WIE_CNTL_1, A6XX_SP_CS_WIE_CNTL_1_LINEARLOCALIDREGID(0xfc) |
                A6XX_SP_CS_WIE_CNTL_1_THREADSIZE(THREAD64));
        } else {
            reg(REG_A7XX_SP_CS_WIE_CNTL_1, A7XX_SP_CS_WIE_CNTL_1_LINEARLOCALIDREGID(0xfc) |
                A7XX_SP_CS_WIE_CNTL_1_THREADSIZE(THREAD64) |
                A7XX_SP_CS_WIE_CNTL_1_WORKITEMRASTORDER(WORKITEMRASTORDER_LINEAR));
        }
    }
    reg_address(REG_A6XX_SP_CS_BASE, NN_CS_CODE);
    p7(CP_LOAD_STATE6_FRAG, 3);
    cs.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_SHADER) |
        CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
        CP_LOAD_STATE6_0_STATE_BLOCK(SB6_CS_SHADER) |
        CP_LOAD_STATE6_0_NUM_UNIT(a6 ? MIN2(v->instrlen, info->props.instr_cache_size) : v->instrlen));
    address(NN_CS_CODE);
    p7(CP_LOAD_STATE6_FRAG, 3);
    cs.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_UAV) |
        CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
        CP_LOAD_STATE6_0_STATE_BLOCK(SB6_CS_SHADER) | CP_LOAD_STATE6_0_NUM_UNIT(v->num_uavs));
    address(NN_CS_DESCRIPTORS);
    reg_address(a6 ? REG_A6XX_SP_CS_UAV_BASE : REG_A7XX_SP_CS_UAV_BASE, NN_CS_DESCRIPTORS);
    reg(REG_A6XX_SP_CS_USIZE, v->num_uavs);
    p7(CP_SET_MARKER, 1); cs.push_back(A6XX_CP_SET_MARKER_0_MODE(RM6_COMPUTE));
    if (a6) {
        reg(REG_A6XX_SP_CS_NDRANGE_0, A6XX_SP_CS_NDRANGE_0_KERNELDIM(3) |
            A6XX_SP_CS_NDRANGE_0_LOCALSIZEX(v->local_size[0] - 1) |
            A6XX_SP_CS_NDRANGE_0_LOCALSIZEY(v->local_size[1] - 1) |
            A6XX_SP_CS_NDRANGE_0_LOCALSIZEZ(v->local_size[2] - 1));
        reg_dynamic(REG_A6XX_SP_CS_NDRANGE_1, NN_CS_GLOBAL_SIZE, 0); reg(REG_A6XX_SP_CS_NDRANGE_2, 0);
        reg_dynamic(REG_A6XX_SP_CS_NDRANGE_3, NN_CS_GLOBAL_SIZE, 1); reg(REG_A6XX_SP_CS_NDRANGE_4, 0);
        reg_dynamic(REG_A6XX_SP_CS_NDRANGE_5, NN_CS_GLOBAL_SIZE, 2); reg(REG_A6XX_SP_CS_NDRANGE_6, 0);
        reg(REG_A6XX_SP_CS_KERNEL_GROUP_X, 1); reg(REG_A6XX_SP_CS_KERNEL_GROUP_Y, 1);
        reg(REG_A6XX_SP_CS_KERNEL_GROUP_Z, 1);
    } else {
    reg(REG_A7XX_SP_CS_NDRANGE_0, A7XX_SP_CS_NDRANGE_0_KERNELDIM(3) |
        A7XX_SP_CS_NDRANGE_0_LOCALSIZEX(v->local_size[0] - 1) |
        A7XX_SP_CS_NDRANGE_0_LOCALSIZEY(v->local_size[1] - 1) |
        A7XX_SP_CS_NDRANGE_0_LOCALSIZEZ(v->local_size[2] - 1));
    reg(REG_A7XX_SP_CS_NDRANGE_7,
        A7XX_SP_CS_NDRANGE_7_LOCALSIZEX(v->local_size[0] - 1) |
        A7XX_SP_CS_NDRANGE_7_LOCALSIZEY(v->local_size[1] - 1) |
        A7XX_SP_CS_NDRANGE_7_LOCALSIZEZ(v->local_size[2] - 1));
    reg_dynamic(REG_A7XX_SP_CS_NDRANGE_1, NN_CS_GLOBAL_SIZE, 0); reg(REG_A7XX_SP_CS_NDRANGE_2, 0);
    reg_dynamic(REG_A7XX_SP_CS_NDRANGE_3, NN_CS_GLOBAL_SIZE, 1); reg(REG_A7XX_SP_CS_NDRANGE_4, 0);
    reg_dynamic(REG_A7XX_SP_CS_NDRANGE_5, NN_CS_GLOBAL_SIZE, 2); reg(REG_A7XX_SP_CS_NDRANGE_6, 0);
    reg(REG_A7XX_SP_CS_KERNEL_GROUP_X, 1); reg(REG_A7XX_SP_CS_KERNEL_GROUP_Y, 1);
    reg(REG_A7XX_SP_CS_KERNEL_GROUP_Z, 1);
    }
    if (!emit_constants()) return 9;
    p7(CP_EXEC_CS, 4); cs.push_back(0);
    for (unsigned axis = 0; axis < 3; axis++) {
        patches.push_back({(uint32_t)cs.size(), NN_CS_GROUP_COUNT, axis, 0}); cs.push_back(0);
    }
    p7(CP_WAIT_FOR_IDLE, 0);
    p7(CP_EVENT_WRITE, 4);
    cs.push_back(a6 ? CP_EVENT_WRITE_0_EVENT(RB_DONE_TS) : (CP_EVENT_WRITE7_0_EVENT(RB_DONE_TS) |
        CP_EVENT_WRITE7_0_WRITE_SRC(EV_WRITE_USER_32B) | CP_EVENT_WRITE7_0_WRITE_ENABLED));
    address(NN_CS_SEQUENCE); cs.push_back(1);
    p7(CP_WAIT_REG_MEM, 6);
    cs.push_back(CP_WAIT_REG_MEM_0_FUNCTION(WRITE_EQ) | CP_WAIT_REG_MEM_0_POLL(POLL_MEMORY));
    address(NN_CS_SEQUENCE); cs.insert(cs.end(), {1, ~0u, 16});
    if (a6) {
        // Follow computerator's A6 cache completion: a second timestamp and
        // WAIT_MEM_GTE, reusing the existing sequence patch/address ABI.
        p7(CP_EVENT_WRITE, 4); cs.push_back(CP_EVENT_WRITE_0_EVENT(CACHE_FLUSH_TS));
        address(NN_CS_SEQUENCE); cs.push_back(2);
        p7(CP_WAIT_MEM_GTE, 4); cs.push_back(0);
        address(NN_CS_SEQUENCE); cs.push_back(2);
    } else {
        p7(CP_EVENT_WRITE, 1);
        cs.push_back(CP_EVENT_WRITE7_0_EVENT(CACHE_FLUSH7) | CP_EVENT_WRITE7_0_WRITE_SRC(EV_WRITE_USER_32B));
    }
    p7(CP_WAIT_FOR_IDLE, 0); p7(CP_WAIT_FOR_ME, 0);
    if (cs.size() > 1024) return 9;
    nn_cs_image image = {};
    const uint8_t swiz[4] = {PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W};
    auto descriptor = [&](uint32_t *words, uint64_t addr, uint32_t bytes) {
        if (a6) fdl6_buffer_view_init<A6XX>(words, PIPE_FORMAT_R32_UINT, swiz, addr, bytes);
        else fdl6_buffer_view_init<A7XX>(words, PIPE_FORMAT_R32_UINT, swiz, addr, bytes);
    };
    descriptor(image.descriptor, 0, 0);
    image.width_mask = A6XX_TEX_MEMOBJ_1_WIDTH(~0u);
    image.height_mask = A6XX_TEX_MEMOBJ_1_HEIGHT(~0u);
    image.height_shift = __builtin_ctz(image.height_mask);
    image.offset_mask = A6XX_TEX_MEMOBJ_2_STARTOFFSETTEXELS(~0u);
    image.offset_shift = __builtin_ctz(image.offset_mask);
    // Independently compare dynamic patching with the full existing FDL tool.
    for (uint64_t addr : {0ull, 4ull, 60ull, 0x1234567844ull}) {
        for (uint32_t size : {4u, 64u, 131068u, 131072u, 1048580u}) {
            uint32_t actual[16], expected[16];
            descriptor(expected, addr, size);
            if (nn_cs_buffer_descriptor(&image, actual, addr, size) || memcmp(actual, expected, sizeof(actual))) return 10;
        }
    }
    char path[4096]; snprintf(path, sizeof(path), "%s.native.h", prefix);
    FILE *out = fopen(path, "w"); if (!out) return 5;
    const char *symbol = v->blake3_str;
    fprintf(out, "/* Generated from real IR3 compiler metadata, registers and FDL. */\n");
    auto array = [&](const char *name, const uint32_t *data, size_t count) {
        fprintf(out, "static const uint32_t nn_%s_%s[] = {", name, symbol);
        for (size_t i = 0; i < count; i++) fprintf(out, "0x%08xu,", data[i]);
        fprintf(out, "};\n");
    };
    array("code", v->bin, v->info.size / 4); array("commands", cs.data(), cs.size());
    fprintf(out, "static const struct nn_cs_patch nn_patches_%s[] = {", symbol);
    for (auto p : patches) fprintf(out, "{%uu,%uu,%uu,%uu},", p.word, p.kind, p.index, p.offset);
    fprintf(out, "};\nstatic const struct nn_cs_image nn_image_%s = {\n"
        ".schema=1,.chip=0x%08xu,.local={%u,%u,%u},.ssbo_slots=%u,\n"
        ".code=nn_code_%s,.code_bytes=%u,.commands=nn_commands_%s,.command_words=%zu,\n"
        ".patches=nn_patches_%s,.patch_count=%zu,.descriptor={",
        symbol, chip, v->local_size[0], v->local_size[1], v->local_size[2], v->num_uavs,
        symbol, v->info.size, symbol, cs.size(), symbol, patches.size());
    for (auto word : image.descriptor) fprintf(out, "0x%08xu,", word);
    fprintf(out, "},.width_mask=%uu,.height_mask=%uu,.height_shift=%uu,.offset_mask=%uu,.offset_shift=%uu};\n"
        "#define NVN_COMPILED_IMAGE nn_image_%s\n", image.width_mask, image.height_mask,
        image.height_shift, image.offset_mask, image.offset_shift, symbol);
    if (fclose(out)) return 7;
    snprintf(path, sizeof(path), "%s.native.json", prefix); out = fopen(path, "w"); if (!out) return 5;
    fprintf(out, "{\"schema\":1,\"descriptor_fdl_checks\":20,\"command_words\":%zu,\"patches\":%zu}\n", cs.size(), patches.size());
    return fclose(out) ? 7 : 0;
}
