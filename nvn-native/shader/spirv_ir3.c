/* SPDX-License-Identifier: MIT */
/* Standalone compiler bridge. SPIR-V is only an intermediate format here. */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "nir.h"
#include "nir_builder.h"
#include "compiler/spirv/nir_spirv.h"
#include "ir3/ir3_compiler.h"
#include "ir3/ir3_nir.h"
#include "ir3/ir3_shader.h"
#include "nvn_nir_bridge.h"
extern int nvn_compute_artifact(const struct ir3_shader_variant *, const char *);

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: nvn-spirv-ir3 INPUT_SPV OUTPUT_PREFIX [CHIP_ID]\n");
        return 2;
    }
    uint32_t chip = 0x43030b00;
    if (argc == 4) {
        char *end;
        errno = 0;
        unsigned long long parsed = strtoull(argv[3], &end, 0);
        if (errno || end == argv[3] || *end || argv[3][0] == '-' || parsed > UINT32_MAX) return 2;
        chip = parsed;
    }
    if (chip != 0x43030b00 && chip != 0x06050002) {
        fprintf(stderr, "unsupported compute chip id: 0x%08x\n", chip);
        return 2;
    }
    const struct fd_dev_id id = {.chip_id = chip};
    const struct fd_dev_info *info = fd_dev_info_raw(&id);
    if (!info) { fprintf(stderr, "compute chip has no pinned device information\n"); return 2; }
    glsl_type_singleton_init_or_ref();
    struct ir3_compiler_options compiler_options = {.disable_cache = true};
    struct ir3_compiler *compiler = ir3_compiler_create(NULL, &id,
        info, &compiler_options);
    if (!compiler) return 3;
    nir_shader *nir = nvn_import_nir(argv[1], MESA_SHADER_COMPUTE, argv[2], compiler);
    if (!nir) return 3;
    unsigned ubos = nir->info.num_ubos, ssbos = nir->info.num_ssbos;
    struct ir3_shader_options shader_options = {
        .api_wavesize = IR3_SINGLE_ONLY, .real_wavesize = IR3_SINGLE_ONLY
    };
    ir3_nir_lower_io(nir);
    ir3_finalize_nir(compiler, &shader_options.nir_options, nir);
    struct ir3_shader *shader = ir3_shader_from_nir(compiler, nir, &shader_options);
    struct ir3_shader_key key = {0};
    struct ir3_shader_variant *variant = ir3_shader_create_variant(shader, &key, true);
    if (!variant || !variant->bin || !variant->disasm_info.disasm) return 6;
    char path[4096];
    snprintf(path, sizeof(path), "%s.bin", argv[2]);
    FILE *file = fopen(path, "wb");
    if (!file || fwrite(variant->bin, 1, variant->info.size, file) != variant->info.size || fclose(file)) return 5;
    snprintf(path, sizeof(path), "%s.asm", argv[2]);
    file = fopen(path, "w");
    if (!file || fputs(variant->disasm_info.disasm, file) < 0 || fclose(file)) return 5;
    snprintf(path, sizeof(path), "%s.json", argv[2]);
    file = fopen(path, "w");
    if (!file) return 5;
    fprintf(file, "{\"schema\":1,\"chip_id\":%u,\"binary_bytes\":%u,"
        "\"instrlen\":%u,\"constlen\":%u,\"max_reg\":%d,\"max_half_reg\":%d,"
        "\"branchstack\":%u,\"num_uavs\":%u,\"num_samplers\":%u,"
        "\"ubo_table_slots\":%u,\"ssbo_table_slots\":%u,"
        "\"local_size\":[%u,%u,%u],\"private_bytes\":%u,\"shared_bytes\":%u,"
        "\"double_threadsize\":%s,\"early_preamble\":%s,"
        "\"scope\":\"SPIR-V CS to IR3; host compilation only, no GPU execution\"",
        (unsigned)id.chip_id, variant->info.size, variant->instrlen, variant->constlen,
        variant->info.max_reg, variant->info.max_half_reg,
        ir3_shader_branchstack_hw(variant), ir3_shader_num_uavs(variant), variant->num_samp,
        ubos, ssbos, variant->local_size[0], variant->local_size[1], variant->local_size[2],
        variant->pvtmem_size, variant->shared_size,
        variant->info.double_threadsize ? "true" : "false", variant->early_preamble ? "true" : "false");
    const struct ir3_const_state *consts = ir3_const_state(variant);
    fprintf(file, ",\"merged_regs\":%s,\"constant_data_bytes\":%u,\"immediate_dwords\":%u,"
        "\"driver_params_dwords\":%u,\"driver_params_ubo\":{\"index\":%d,\"bytes\":%u},"
        "\"const_allocations\":[", variant->mergedregs ? "true" : "false",
        variant->constant_data_size, variant->imm_state.count, consts->num_driver_params,
        consts->driver_params_ubo.idx, consts->driver_params_ubo.size);
    for (unsigned i = 0; i < IR3_CONST_ALLOC_MAX; ++i) {
        const struct ir3_const_allocation *alloc = &consts->allocs.consts[i];
        fprintf(file, "%s{\"kind\":%u,\"offset_vec4\":%u,\"size_vec4\":%u}", i ? "," : "",
                i, alloc->offset_vec4, alloc->size_vec4);
    }
    fprintf(file, "],\"ubo_push_ranges\":[");
    for (unsigned i = 0; i < consts->ubo_state.num_enabled; ++i) {
        const struct ir3_ubo_range *range = &consts->ubo_state.range[i];
        fprintf(file, "%s{\"block\":%u,\"range_offset_bytes\":%u,\"start_bytes\":%u,\"end_bytes\":%u,"
            "\"bindless\":%s,\"global\":%s}", i ? "," : "", range->ubo.block, range->offset,
            range->start, range->end, range->ubo.bindless ? "true" : "false", range->ubo.global ? "true" : "false");
    }
    fprintf(file, "],\"inputs\":[");
    for (unsigned i = 0; i < variant->inputs_count; ++i) {
        fprintf(file, "%s{\"slot\":%u,\"regid\":%u,\"compmask\":%u,\"sysval\":%s,\"name\":\"%s\"}",
            i ? "," : "", variant->inputs[i].slot, variant->inputs[i].regid,
            variant->inputs[i].compmask, variant->inputs[i].sysval ? "true" : "false",
            variant->inputs[i].sysval ? gl_system_value_name(variant->inputs[i].slot) : "non-system");
    }
    fprintf(file, "]}\n");
    if (fclose(file)) return 5;
    int artifact_error = nvn_compute_artifact(variant, argv[2]);
    if (artifact_error) return artifact_error;
    ralloc_free(shader);
    ir3_compiler_destroy(compiler);
    glsl_type_singleton_decref();
    return 0;
}
