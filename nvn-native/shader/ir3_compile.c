/* SPDX-License-Identifier: MIT */
/* Host compiler qualification: controlled NIR compute input, no API device. */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "nir.h"
#include "nir_builder.h"
#include "ir3/ir3_compiler.h"
#include "ir3/ir3_nir.h"
#include "ir3/ir3_shader.h"
extern int nvn_cs_artifact(const struct ir3_shader_variant *, uint32_t, const char *);

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: nvn-ir3-compile OUTPUT_PREFIX [UINT32_VALUE]\n");
        return 2;
    }
    uint32_t value = 0x4e564e32;
    if (argc == 3) {
        char *end;
        errno = 0;
        unsigned long parsed = strtoul(argv[2], &end, 0);
        if (errno || end == argv[2] || *end || parsed > UINT32_MAX || argv[2][0] == '-') return 2;
        value = parsed;
    }
    const struct fd_dev_id id = {.chip_id = 0x43030b00};
    const struct fd_dev_info *info = fd_dev_info_raw(&id);
    if (!info) return 3;
    struct ir3_compiler_options compiler_options = {.disable_cache = true};
    struct ir3_compiler *compiler = ir3_compiler_create(NULL, &id, info, &compiler_options);
    nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_COMPUTE,
        ir3_get_compiler_options(compiler), "nvn-controlled-ssbo-write");
    b.shader->info.workgroup_size[0] = 1;
    b.shader->info.workgroup_size[1] = 1;
    b.shader->info.workgroup_size[2] = 1;
    b.shader->info.num_ssbos = 1;
    nir_store_ssbo(&b, nir_imm_int(&b, value), nir_imm_int(&b, 0),
                   nir_imm_int(&b, 0), .align_mul = 4, .write_mask = 1);
    nir_validate_shader(b.shader, "controlled NVN backend fixture");
    nir_shader_gather_info(b.shader, nir_shader_get_entrypoint(b.shader));
    char path[4096];
    if (snprintf(path, sizeof(path), "%s.nir", argv[1]) >= (int)sizeof(path)) return 2;
    FILE *file = fopen(path, "w");
    if (!file) return 5;
    nir_print_shader(b.shader, file);
    if (fclose(file)) return 7;
    struct ir3_shader_options options = {
        .api_wavesize = IR3_SINGLE_ONLY, .real_wavesize = IR3_SINGLE_ONLY
    };
    ir3_nir_lower_io(b.shader);
    ir3_finalize_nir(compiler, &options.nir_options, b.shader);
    struct ir3_shader *shader = ir3_shader_from_nir(compiler, b.shader, &options);
    struct ir3_shader_key key = {0};
    struct ir3_shader_variant *variant = ir3_shader_create_variant(shader, &key, true);
    if (!variant || !variant->bin) return 4;
    snprintf(path, sizeof(path), "%s.bin", argv[1]);
    file = fopen(path, "wb");
    if (!file) return 5;
    if (fwrite(variant->bin, 1, variant->info.size, file) != variant->info.size) return 6;
    if (fclose(file)) return 7;
    snprintf(path, sizeof(path), "%s.asm", argv[1]);
    file = fopen(path, "w");
    if (!file) return 5;
    /* create_variant captures disassembly before it releases temporary IR.
     * Calling ir3_shader_disasm afterward would dereference variant->ir=NULL. */
    if (!variant->disasm_info.disasm) return 8;
    fputs(variant->disasm_info.disasm, file);
    if (fclose(file)) return 7;
    snprintf(path, sizeof(path), "%s.json", argv[1]);
    file = fopen(path, "w");
    if (!file) return 5;
    fprintf(file, "{\"schema\":1,\"chip_id\":%u,\"binary_bytes\":%u,"
        "\"instrlen\":%u,\"constlen\":%u,\"max_reg\":%d,\"max_half_reg\":%d,"
        "\"branchstack\":%u,\"expected\":%u,\"num_uavs\":%u,\"num_samplers\":%u,"
        "\"local_size\":[%u,%u,%u],\"private_bytes\":%u,\"shared_bytes\":%u,"
        "\"double_threadsize\":%s,\"early_preamble\":%s,"
        "\"scope\":\"controlled NIR CS, not NVN input\"}\n",
        (unsigned)id.chip_id, variant->info.size, variant->instrlen, variant->constlen,
        variant->info.max_reg, variant->info.max_half_reg,
        ir3_shader_branchstack_hw(variant), value, ir3_shader_num_uavs(variant), variant->num_samp,
        variant->local_size[0], variant->local_size[1], variant->local_size[2],
        variant->pvtmem_size, variant->shared_size,
        variant->info.double_threadsize ? "true" : "false", variant->early_preamble ? "true" : "false");
    if (fclose(file)) return 7;
    int artifact = nvn_cs_artifact(variant, value, argv[1]);
    if (artifact) return artifact;
    ralloc_free(shader);
    ir3_compiler_destroy(compiler);
    return 0;
}
