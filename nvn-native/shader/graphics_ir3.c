/* SPDX-License-Identifier: MIT */
#include <string.h>
#include "nvn_nir_bridge.h"
#include "common/fd6_gmem_cache.h"

int nvn_graphics_artifact(const struct ir3_shader_variant *, const struct ir3_shader_variant *,
                         const struct ir3_shader_linkage *, const struct fd6_gmem_config *, uint32_t, const char *);
int nvn_graphics_artifact_modes(const struct ir3_shader_variant *, const struct ir3_shader_variant *,
                         const struct ir3_shader_linkage *, const struct fd6_gmem_config *, uint32_t, const char *);

/* NIR exposes both component write masks and undef SSA values. These are
 * conservative may-write facts, never an all-path initialization proof. */
struct nvn_output_usage {uint8_t may_write_mask,store_mask,direct_undef_mask;bool known;};
struct nvn_io_decl {bool present,known;unsigned base_type,bits,components,component,interpolation;bool centroid,sample;};
struct nvn_stage_usage {
    struct nvn_io_decl inputs[64],output_decls[64];
    struct nvn_output_usage outputs[64];
    uint32_t texture_mask,sampler_mask,implicit_samples,fetches,size_queries,constant_offset_fetches;
    uint32_t texture_dimensions[16],texture_operations[16];
    struct {uint32_t fetches,sizes,samples,offsets,other;} texture_resources[16];
    uint32_t gathers;
    struct {uint32_t count,component_mask,sampler_mask,offsets;} gather_resources[16];
    uint32_t storage_mask,storage_operations[16];
    struct nvn_unit_scale_usage unit_scale;
};
/* Inspect existing lowered NIR, never decode or execute SPIR-V here. A6xx
 * has one graphics IBO table. Reserve independent halves for the two stages.
 * Logical source/native/API bindings stay distinct in the typed sidecar. */
struct storage_remap {struct nvn_stage_usage *usage;unsigned bias;};
static bool remap_graphics_storage(nir_builder *b,nir_intrinsic_instr *intr,void *data)
{
 struct storage_remap *r=data;unsigned index=0,op=0;
 switch(intr->intrinsic) {
 case nir_intrinsic_load_ssbo:
  if(intr->def.bit_size!=32||intr->num_components>4||nir_intrinsic_align_mul(intr)<4||nir_intrinsic_align_offset(intr)%4)reject("graphics storage load type/alignment");
  op=1;break;
 case nir_intrinsic_store_ssbo:
  index=1;op=2;
  if(intr->src[0].ssa->bit_size!=32||intr->num_components>4||nir_intrinsic_align_mul(intr)<4||nir_intrinsic_align_offset(intr)%4)reject("graphics storage store type/alignment");
  break;
 case nir_intrinsic_get_ssbo_size:op=4;break;
 case nir_intrinsic_ssbo_atomic:
 case nir_intrinsic_ssbo_atomic_swap:reject("graphics storage atomics");return false;
 default:return false;
 }
 if(!nir_src_is_const(intr->src[index])||nir_src_as_uint(intr->src[index])>=16)reject("graphics storage dynamic binding/range");
 unsigned binding=nir_src_as_uint(intr->src[index]);
 r->usage->storage_mask|=1u<<binding;r->usage->storage_operations[binding]|=op;
 if(!r->bias)return false;
 b->cursor=nir_before_instr(&intr->instr);
 nir_src_rewrite(&intr->src[index],nir_imm_int(b,binding+r->bias));return true;
}
static void gather_io_declarations(nir_shader *nir,struct nvn_stage_usage *out)
{
    nir_foreach_variable_with_modes(var,nir,nir_var_shader_in|nir_var_shader_out) {
        unsigned slot=var->data.location;if(slot>=64)continue;
        struct nvn_io_decl *d=(var->data.mode==nir_var_shader_in?out->inputs:out->output_decls)+slot;
        if(d->present){d->known=false;continue;}
        d->present=true;d->known=(glsl_type_is_scalar(var->type)||glsl_type_is_vector(var->type)) &&
            glsl_get_bit_size(var->type)==32;
        if(!d->known)continue;
        d->base_type=glsl_get_base_type(var->type);d->bits=glsl_get_bit_size(var->type);
        d->components=glsl_get_vector_elements(var->type);d->component=var->data.location_frac;
        d->interpolation=var->data.interpolation;d->centroid=var->data.centroid;d->sample=var->data.sample;
    }
}
static void write_io_declarations(FILE *file,const struct nvn_io_decl *d)
{
    fprintf(file,"[");bool first=true;
    for(unsigned i=0;i<64;i++)if(d[i].present) {
        fprintf(file,"%s{\"slot\":%u,\"known\":%s,\"base_type\":%u,\"bits\":%u,\"components\":%u,"
            "\"component\":%u,\"interpolation\":%u,\"centroid\":%s,\"sample\":%s}",first?"":",",i,d[i].known?"true":"false",
            d[i].base_type,d[i].bits,d[i].components,d[i].component,d[i].interpolation,d[i].centroid?"true":"false",d[i].sample?"true":"false");first=false;
    }
    fprintf(file,"]");
}
static void gather_stage_usage(nir_shader *original,struct nvn_stage_usage *out)
{
    /* IO lowering creates temporaries after initial SSA lowering. Analyze a
     * clone with Mesa SSA/copy passes so partial vectors reveal undef lanes;
     * do not perturb optimizer ordering or native code in the real shader. */
    nir_shader *nir=nir_shader_clone(NULL,original);
    NIR_PASS(_,nir,nir_lower_vars_to_ssa);
    NIR_PASS(_,nir,nir_opt_copy_prop);
    NIR_PASS(_,nir,nir_opt_constant_folding);
    for(unsigned i=0;i<64;i++)out->outputs[i].known=true;
    nir_foreach_function_impl(impl,nir) nir_foreach_block(block,impl) nir_foreach_instr(instr,block) {
        if(instr->type==nir_instr_type_tex) {
            nir_tex_instr *tex=nir_instr_as_tex(instr);
            if(tex->texture_index>=16)reject("resource proof texture binding range");
            out->texture_mask|=1u<<tex->texture_index;
            unsigned ti=tex->texture_index;
            unsigned dimensions=tex->sampler_dim==GLSL_SAMPLER_DIM_BUF?1:
                tex->sampler_dim==GLSL_SAMPLER_DIM_2D?2:tex->sampler_dim==GLSL_SAMPLER_DIM_3D?3:0;
            if(!dimensions||(out->texture_dimensions[ti]&&out->texture_dimensions[ti]!=dimensions))
                reject("conflicting native NIR resource dimensions");
            out->texture_dimensions[ti]=dimensions;
            out->texture_operations[ti]|=tex->op==nir_texop_tex?1u:tex->op==nir_texop_txf?2u:tex->op==nir_texop_txs?4u:tex->op==nir_texop_tg4?16u:8u;
            if(tex->op==nir_texop_txf)out->texture_resources[ti].fetches++;
            else if(tex->op==nir_texop_txs)out->texture_resources[ti].sizes++;
            else if(tex->op==nir_texop_tex)out->texture_resources[ti].samples++;
            else out->texture_resources[ti].other++;
            if(nir_tex_instr_src_index(tex,nir_tex_src_offset)>=0)out->texture_resources[ti].offsets++;
            /* Keep gather distinct from ordinary implicit sampling. The old
             * conservative other count also stays nonzero, so consumers which
             * have not admitted gather cannot accidentally treat it as sample. */
            if(tex->op==nir_texop_tg4){
                out->gathers++;out->gather_resources[ti].count++;
                out->gather_resources[ti].component_mask|=1u<<tex->component;
                out->gather_resources[ti].sampler_mask|=1u<<tex->sampler_index;
                if(nir_tex_instr_src_index(tex,nir_tex_src_offset)>=0)out->gather_resources[ti].offsets++;
            }
            if(nir_tex_instr_need_sampler(tex))out->sampler_mask|=1u<<tex->sampler_index;
            if(tex->op==nir_texop_tex)out->implicit_samples++;
            if(tex->op==nir_texop_txf) {
                out->fetches++;
                if(nir_tex_instr_src_index(tex,nir_tex_src_offset)>=0)out->constant_offset_fetches++;
            }
            if(tex->op==nir_texop_txs)out->size_queries++;
        }
        if(instr->type!=nir_instr_type_intrinsic)continue;
        nir_intrinsic_instr *intr=nir_instr_as_intrinsic(instr);
        if(intr->intrinsic!=nir_intrinsic_store_output)continue;
        nir_io_semantics io=nir_intrinsic_io_semantics(intr);
        unsigned component=nir_intrinsic_component(intr),mask=nir_intrinsic_write_mask(intr);
        if(io.location>=64 || !nir_src_is_const(intr->src[1]) ||
           nir_src_as_uint(intr->src[1])>=64-io.location || component+intr->num_components>4 ||
           intr->src[0].ssa->bit_size!=32) {
            for(unsigned i=0;i<64;i++)out->outputs[i].known=false;
            continue;
        }
        unsigned slot=io.location+nir_src_as_uint(intr->src[1]);
        struct nvn_output_usage *u=out->outputs+slot;
        for(unsigned i=0;i<intr->num_components;i++)if(mask&(1u<<i)) {
            unsigned bit=1u<<(component+i);u->store_mask|=bit;
            nir_scalar value=nir_scalar_resolved(intr->src[0].ssa,i);
            if(nir_scalar_is_undef(value))u->direct_undef_mask|=bit;
            else u->may_write_mask|=bit;
        }
    }
    ralloc_free(nir);
}
static void write_stage_usage(FILE *file,const struct nvn_stage_usage *u)
{
    fprintf(file,"{\"schema\":1,\"scope\":\"NIR may-write and direct-undef facts; not all-path initialization\","
        "\"all_paths_initialized_proven\":false,\"texture_usage\":{\"texture_mask\":%u,\"semantic_sampler_mask\":%u,"
        "\"implicit_samples\":%u,\"texel_fetches\":%u,\"size_lod_queries\":%u,\"constant_offset_fetches\":%u},\"outputs\":[",
        u->texture_mask,u->sampler_mask,u->implicit_samples,u->fetches,u->size_queries,u->constant_offset_fetches);
    bool first=true;
    for(unsigned i=0;i<64;i++)if(u->outputs[i].store_mask || !u->outputs[i].known) {
        const struct nvn_output_usage *v=u->outputs+i;
        fprintf(file,"%s{\"slot\":%u,\"known\":%s,\"may_write_mask\":%u,\"store_mask\":%u,\"direct_undef_mask\":%u}",
            first?"":",",i,v->known?"true":"false",v->may_write_mask,v->store_mask,v->direct_undef_mask);first=false;
    }
    fprintf(file,"],\"resource_semantics\":{\"schema\":1,\"basis\":\"actual common NIR texture instructions\",\"bindings\":[");
    first=true;
    for(unsigned i=0;i<16;i++)if(u->texture_mask&(1u<<i)){
        fprintf(file,"%s{\"binding\":%u,\"fetches\":%u,\"size_queries\":%u,\"samples\":%u,\"offsets\":%u,\"other\":%u}",
         first?"":",",i,u->texture_resources[i].fetches,u->texture_resources[i].sizes,u->texture_resources[i].samples,u->texture_resources[i].offsets,u->texture_resources[i].other);first=false;
    }
    if(u->gathers){
        fprintf(file,"]},\"gather_usage\":{\"schema\":1,\"basis\":\"actual common NIR tg4 instructions\",\"count\":%u,\"coordinate_domain\":\"normalized 2D texture operation\",\"lod_mode\":\"level_zero\",\"bindings\":[",u->gathers);
        bool first_gather=true;
        for(unsigned i=0;i<16;i++)if(u->gather_resources[i].count){
            fprintf(file,"%s{\"binding\":%u,\"count\":%u,\"component_mask\":%u,\"semantic_sampler_mask\":%u,\"offsets\":%u}",
                first_gather?"":",",i,u->gather_resources[i].count,u->gather_resources[i].component_mask,u->gather_resources[i].sampler_mask,u->gather_resources[i].offsets);first_gather=false;
        }
    }
    fprintf(file,"]},\"unit_render_scale\":{\"requested\":%s,\"replaced_scale_loads\":%u,\"remaining_support_loads\":%u},\"interfaces\":{\"inputs\":",
      nvn_unit_render_scale_requested?"true":"false",u->unit_scale.replaced_scale_loads,u->unit_scale.remaining_support_loads);write_io_declarations(file,u->inputs);
    fprintf(file,",\"outputs\":");write_io_declarations(file,u->output_decls);fprintf(file,"}");
    if(u->storage_mask){
      fprintf(file,",\"storage_usage\":{\"schema\":1,\"basis\":\"actual common NIR storage instructions\",\"mask\":%u,\"operations\":[",u->storage_mask);
      for(unsigned i=0;i<16;i++)fprintf(file,"%s%u",i?",":"",u->storage_operations[i]);
      fprintf(file,"]}");
    }
    fprintf(file,"}");
}

static struct ir3_shader_variant *compile_stage(struct ir3_compiler *compiler, const char *input,
                                               const char *prefix, mesa_shader_stage stage, struct nvn_stage_usage *usage)
{
    memset(usage,0,sizeof(*usage));
    nir_shader *nir = nvn_import_nir(input, stage, prefix, compiler);
    if (!nir) return NULL;
    usage->unit_scale=nvn_unit_scale_last;
    struct storage_remap storage={usage,stage==MESA_SHADER_FRAGMENT?16u:0u};
    nir_shader_intrinsics_pass(nir,remap_graphics_storage,nir_metadata_control_flow,&storage);
    if(usage->storage_mask&&storage.bias)nir->info.num_ssbos+=storage.bias;
    NIR_PASS(_, nir, nir_split_per_member_structs);
    nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));
    nvn_update_sampled_info(nir);
    nir_foreach_variable_with_modes(var, nir, nir_var_shader_out) {
        unsigned slot = var->data.location;
        if (slot >= 64 || !(nir->info.outputs_written & (UINT64_C(1) << slot))) continue;
        if ((!glsl_type_is_scalar(var->type) && !glsl_type_is_vector(var->type)) || glsl_get_bit_size(var->type) != 32)
            reject("only scalar/vector 32-bit producer outputs");
        /* Component coverage is gathered from lowered stores below. */
    }
    gather_io_declarations(nir,usage);
    nir_assign_io_var_locations(nir, nir_var_shader_in);
    nir_assign_io_var_locations(nir, nir_var_shader_out);
    ir3_nir_lower_io_vars_to_temporaries(nir);
    ir3_nir_lower_io(nir);
    gather_stage_usage(nir,usage);
    char usage_path[4096];snprintf(usage_path,sizeof(usage_path),"%s.io.json",prefix);
    FILE *usage_file=fopen(usage_path,"w");if(!usage_file)return NULL;
    write_stage_usage(usage_file,usage);fprintf(usage_file,"\n");if(fclose(usage_file))return NULL;
    struct ir3_shader_options options = {.api_wavesize=IR3_SINGLE_ONLY, .real_wavesize=IR3_SINGLE_ONLY};
    ir3_finalize_nir(compiler, &options.nir_options, nir);
    struct ir3_shader *shader = ir3_shader_from_nir(compiler, nir, &options);
    struct ir3_shader_key key = {0};
    struct ir3_shader_variant *variant = ir3_shader_create_variant(shader, &key, true);
    if (!variant || !variant->bin || !variant->disasm_info.disasm) return NULL;
    char path[4096];
    snprintf(path, sizeof(path), "%s.bin", prefix); FILE *file = fopen(path, "wb");
    if (!file || fwrite(variant->bin, 1, variant->info.size, file) != variant->info.size || fclose(file)) return NULL;
    snprintf(path, sizeof(path), "%s.asm", prefix); file = fopen(path, "w");
    if (!file || fputs(variant->disasm_info.disasm, file) < 0 || fclose(file)) return NULL;
    return variant;
}

static void describe(FILE *file, const struct ir3_shader_variant *v, const struct nvn_stage_usage *usage)
{
    fprintf(file, "{\"stage\":\"%s\",\"binary_bytes\":%u,\"instrlen\":%u,\"constlen\":%u,"
        "\"max_reg\":%d,\"max_half_reg\":%d,\"branchstack\":%u,\"private_bytes\":%u,"
        "\"num_samplers\":%u,\"num_uavs\":%u,\"total_in\":%u,\"early_preamble\":%s,"
        "\"merged_regs\":%s,\"double_threadsize\":%s,\"immediate_dwords\":%u,\"constant_data_bytes\":%u,\"inputs\":[",
        v->type == MESA_SHADER_VERTEX ? "vs" : "fs", v->info.size, v->instrlen, v->constlen,
        v->info.max_reg, v->info.max_half_reg, ir3_shader_branchstack_hw(v), v->pvtmem_size,
        v->num_samp, ir3_shader_num_uavs(v), v->total_in, v->early_preamble ? "true" : "false",
        v->mergedregs ? "true" : "false", v->info.double_threadsize ? "true" : "false",
        v->imm_state.count, v->constant_data_size);
    for (unsigned i = 0; i < v->inputs_count; i++)
        fprintf(file, "%s{\"slot\":%u,\"regid\":%u,\"compmask\":%u,\"inloc\":%u,\"sysval\":%s,\"bary\":%s}",
            i ? "," : "", v->inputs[i].slot, v->inputs[i].regid, v->inputs[i].compmask,
            v->inputs[i].inloc, v->inputs[i].sysval ? "true" : "false", v->inputs[i].bary ? "true" : "false");
    fprintf(file, "],\"outputs\":[");
    for (unsigned i = 0; i < v->outputs_count; i++)
        fprintf(file, "%s{\"slot\":%u,\"regid\":%u,\"half\":%s}",
            i ? "," : "", v->outputs[i].slot, v->outputs[i].regid,
            v->outputs[i].half ? "true" : "false");
    if(v->type==MESA_SHADER_FRAGMENT && v->writes_pos){
        unsigned depth_regid=ir3_find_output_regid(v,FRAG_RESULT_DEPTH);
        fprintf(file,"],\"depth_export\":{\"writes_depth\":true,\"slot\":%u,\"regid\":%u,\"requires_depth_attachment_contract\":true},\"const_allocations\":[",FRAG_RESULT_DEPTH,depth_regid);
    }
    const struct ir3_const_state *consts = ir3_const_state(v);
    if(!(v->type==MESA_SHADER_FRAGMENT && v->writes_pos))fprintf(file, "],\"const_allocations\":[");
    for (unsigned i = 0; i < IR3_CONST_ALLOC_MAX; ++i) {
        const struct ir3_const_allocation *alloc = &consts->allocs.consts[i];
        fprintf(file, "%s{\"kind\":%u,\"offset_vec4\":%u,\"size_vec4\":%u}", i ? "," : "", i, alloc->offset_vec4, alloc->size_vec4);
    }
    fprintf(file, "],\"ubo_push_ranges\":[");
    for (unsigned i = 0; i < consts->ubo_state.num_enabled; i++) {
        const struct ir3_ubo_range *range = &consts->ubo_state.range[i];
        fprintf(file, "%s{\"block\":%u,\"range_offset_bytes\":%u,\"start_bytes\":%u,\"end_bytes\":%u}",
            i ? "," : "", range->ubo.block, range->offset, range->start, range->end);
    }
    fprintf(file, "],\"sampler_prefetch\":{\"count\":%u,\"bary_type\":%u,\"end_of_quad\":%s,\"entries\":[",
        v->num_sampler_prefetch, v->prefetch_bary_type, v->prefetch_end_of_quad ? "true" : "false");
    for (unsigned i = 0; i < v->num_sampler_prefetch; ++i) {
        const struct ir3_sampler_prefetch *p = v->sampler_prefetch + i;
        fprintf(file, "%s{\"src\":%u,\"bindless\":%s,\"samp_id\":%u,\"tex_id\":%u,"
            "\"samp_bindless_id\":%u,\"tex_bindless_id\":%u,\"dst\":%u,\"wrmask\":%u,"
            "\"half_precision\":%u,\"tex_opc\":%u}", i ? "," : "", p->src, p->bindless ? "true" : "false",
            p->samp_id, p->tex_id, p->samp_bindless_id, p->tex_bindless_id, p->dst, p->wrmask, p->half_precision, p->tex_opc);
    }
    fprintf(file, "]},\"nir_usage\":");write_stage_usage(file,usage);fprintf(file,"}");
}

#ifdef NVN_PIPELINE_RECIPE_MAIN
int nvn_pipeline_artifact(const struct ir3_shader_variant *, const struct ir3_shader_variant *,
 const struct ir3_shader_linkage *,uint32_t,uint32_t,const uint32_t[2],const char *);
#endif

int main(int argc, char **argv)
{
    bool modes = false;
    int positional = 1;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--unit-render-scale")) {
            if(nvn_unit_render_scale_requested)return 2;
            nvn_unit_render_scale_requested=true;
        } else if (!strcmp(argv[i], "--modes")) {
            if (modes) { fprintf(stderr, "duplicate --modes\n"); return 2; }
            modes = true;
        } else argv[positional++] = argv[i];
    }
    argc = positional;
    if (argc < 4 || argc > 6) { fprintf(stderr, "usage: nvn-graphics-ir3 [--modes] [--unit-render-scale] VERTEX_SPV FRAGMENT_SPV OUTPUT_PREFIX [GMEM_BYTES [CHIP_ID]]\n"); return 2; }
    if (strlen(argv[3]) > 4000) return 2;
    uint32_t gmem_bytes = 0;
    if (argc >= 5) {
        char *end;
        unsigned long long size = strtoull(argv[4], &end, 0);
        if (*end || end == argv[4] || argv[4][0] == '-' || size > UINT32_MAX || size % 4096) return 2;
        gmem_bytes = size;
    }
    if(nvn_unit_render_scale_requested&&gmem_bytes){fprintf(stderr,"unit scale requires shader-only GMEM0 contract\n");return 2;}
    if (modes && !gmem_bytes) { fprintf(stderr, "--modes requires nonzero GMEM_BYTES\n"); return 2; }
#ifdef NVN_PIPELINE_RECIPE_MAIN
    if (gmem_bytes || modes) { fprintf(stderr,"shader recipe takes GMEM=0 and no --modes; target state is runtime-owned\n");return 2; }
#endif
    glsl_type_singleton_init_or_ref();
    struct fd_dev_id id = {.chip_id=0x43030b00};
    if (argc == 6) {
        char *end;
        unsigned long long chip = strtoull(argv[5], &end, 0);
        if (*end || end == argv[5] || argv[5][0] == '-' ||
            (chip != 0x43030b00 && chip != 0x06050002)) return 2;
        id.chip_id = chip;
    }
    const struct fd_dev_info *device_info = fd_dev_info_raw(&id);
    if (!device_info) return 2;
    struct ir3_compiler_options options = {.disable_cache=true};
    struct ir3_compiler *compiler = ir3_compiler_create(NULL, &id, device_info, &options);
    char prefix[4096];
    struct nvn_stage_usage vs_usage,fs_usage;
    snprintf(prefix, sizeof(prefix), "%s.vs", argv[3]);
    struct ir3_shader_variant *vs = compile_stage(compiler, argv[1], prefix, MESA_SHADER_VERTEX, &vs_usage);
    snprintf(prefix, sizeof(prefix), "%s.fs", argv[3]);
    struct ir3_shader_variant *fs = compile_stage(compiler, argv[2], prefix, MESA_SHADER_FRAGMENT, &fs_usage);
    if (!vs || !fs) return 3;
    for (unsigned i = 0; i < fs->inputs_count; i++) {
        if (!fs->inputs[i].bary || !fs->inputs[i].compmask) continue;
        int output = ir3_find_output(vs, fs->inputs[i].slot);
        if (output < 0 || fs->inputs[i].slot >= 64 || (!vs_usage.outputs[fs->inputs[i].slot].known ||
            (fs->inputs[i].compmask & ~vs_usage.outputs[fs->inputs[i].slot].may_write_mask)))
            reject("fragment input lacks a known potentially-defined vertex producer component");
    }
    if (ir3_find_output_regid(vs, VARYING_SLOT_POS) == INVALID_REG)
        reject("vertex position output missing");
    struct ir3_shader_linkage linkage = {0};
    ir3_link_shaders(&linkage, vs, fs, true);
    unsigned fragment_varying_count = linkage.cnt;
    uint32_t varying_mask[4]; memcpy(varying_mask, linkage.varmask, sizeof(varying_mask));
    // Position and point size follow fragment varyings in the a6xx/a7xx VPC.
    unsigned position_loc = linkage.max_loc, pointsize_loc = 0xff;
    ir3_link_add(&linkage, VARYING_SLOT_POS, ir3_find_output_regid(vs, VARYING_SLOT_POS), 15, linkage.max_loc);
    unsigned pointsize_regid = ir3_find_output_regid(vs, VARYING_SLOT_PSIZ);
    if (pointsize_regid != INVALID_REG) {
        pointsize_loc = linkage.max_loc;
        ir3_link_add(&linkage, VARYING_SLOT_PSIZ, pointsize_regid, 1, linkage.max_loc);
    }
#ifdef NVN_PIPELINE_RECIPE_MAIN
    for(unsigned i=0;i<vs->inputs_count;i++)if(!vs->inputs[i].sysval) {
        const struct nvn_io_decl *d=vs_usage.inputs+vs->inputs[i].slot;
        if(!d->present||!d->known||d->base_type!=GLSL_TYPE_FLOAT||d->bits!=32||d->component)
            reject("pipeline recipe currently requires float32 vertex input declarations");
    }
    const struct nvn_io_decl *color=fs_usage.output_decls+FRAG_RESULT_DATA0;
    if(color->present) {
        if(!color->known||color->base_type!=GLSL_TYPE_FLOAT||color->bits!=32||color->components!=4||color->component)
            reject("pipeline recipe requires float32 vec4 DATA0 when present");
    } else if(!fs->writes_pos)reject("pipeline recipe requires a real color or depth output");
    if(fs->writes_pos) {
        const struct nvn_io_decl *depth=fs_usage.output_decls+FRAG_RESULT_DEPTH;
        if(!depth->present||!depth->known||depth->base_type!=GLSL_TYPE_FLOAT||depth->bits!=32||depth->components!=1||depth->component)
            reject("depth recipe requires actual float32 scalar FragDepth declaration");
    }
    const uint32_t storage_masks[2]={vs_usage.storage_mask,fs_usage.storage_mask};
    int pipeline_error=nvn_pipeline_artifact(vs,fs,&linkage,fs_usage.texture_mask,fs_usage.sampler_mask,storage_masks,argv[3]);
    if(pipeline_error)return pipeline_error;
    if(storage_masks[0]||storage_masks[1]) {
      snprintf(prefix,sizeof(prefix),"%s.pipeline.storage.json",argv[3]);
      FILE *storage=fopen(prefix,"w");if(!storage)return 5;
      fprintf(storage,"{\"schema\":1,\"basis\":\"actual common NIR storage instructions\",\"hardware_mapping\":\"VS binding; FS binding+16\",\"masks\":[%u,%u],\"operations\":[",storage_masks[0],storage_masks[1]);
      for(unsigned stage=0;stage<2;stage++) {
        const struct nvn_stage_usage *u=stage?&fs_usage:&vs_usage;
        fprintf(storage,"%s[",stage?",":"");for(unsigned i=0;i<16;i++)fprintf(storage,"%s%u",i?",":"",u->storage_operations[i]);fprintf(storage,"]");
      }
      fprintf(storage,"]}\n");if(fclose(storage))return 5;
    }
    /* Independent append-only typed fact artifact. Existing recipe ABI, code,
     * PM4 and legacy JSON resource-semantic fields remain unchanged. */
    snprintf(prefix,sizeof(prefix),"%s.pipeline.types.json",argv[3]);
    FILE *types=fopen(prefix,"w");if(!types)return 5;
    fprintf(types,"{\"schema\":1,\"basis\":\"actual common NIR texture instructions\",\"texture_mask\":%u,\"sampler_mask\":%u,\"code_bytes\":[%u,%u],\"dimensions\":[",fs_usage.texture_mask,fs_usage.sampler_mask,vs->info.size,fs->info.size);
    for(unsigned i=0;i<16;i++)fprintf(types,"%s%u",i?",":"",fs_usage.texture_dimensions[i]);
    fprintf(types,"],\"operations\":[");
    for(unsigned i=0;i<16;i++)fprintf(types,"%s%u",i?",":"",fs_usage.texture_operations[i]);
    fprintf(types,"]}\n");if(fclose(types))return 5;
#endif
    snprintf(prefix, sizeof(prefix), "%s.link.json", argv[3]); FILE *file = fopen(prefix, "w");
    if (!file) return 5;
    fprintf(file, "{\"schema\":1,\"chip_id\":%u,\"scope\":\"host graphics compilation/link only; no GPU draw\",\"vs\":", (unsigned)id.chip_id);
    describe(file, vs, &vs_usage); fprintf(file, ",\"fs\":"); describe(file, fs, &fs_usage);
    fprintf(file, ",\"linkage\":{\"max_loc\":%u,\"fragment_varying_count\":%u,\"position_loc\":%u,\"pointsize_loc\":%u,"
        "\"varying_mask\":[%u,%u,%u,%u],\"entries\":[", linkage.max_loc, fragment_varying_count, position_loc, pointsize_loc,
        varying_mask[0], varying_mask[1], varying_mask[2], varying_mask[3]);
    for (unsigned i = 0; i < linkage.cnt; i++)
        fprintf(file, "%s{\"slot\":%u,\"regid\":%u,\"compmask\":%u,\"loc\":%u}", i ? "," : "",
            linkage.var[i].slot, linkage.var[i].regid, linkage.var[i].compmask, linkage.var[i].loc);
    fprintf(file, "]}");
#ifndef NVN_PIPELINE_RECIPE_MAIN
    if (gmem_bytes) {
        const struct fd_dev_info *info = compiler->info;
        uint64_t required = (uint64_t)info->num_ccu *
            (info->props.sysmem_per_ccu_depth_cache_size + info->props.sysmem_per_ccu_color_cache_size +
             MAX2(info->props.sysmem_vpc_attr_buf_size, info->props.gmem_vpc_attr_buf_size));
        if (required > gmem_bytes) reject("device GMEM is smaller than the common cache layout");
        struct fd6_gmem_config gm = {0}, sys = {0};
        fd6_calc_gmem_cache_offsets(info, gmem_bytes, &gm, &sys);
        int artifact_error = modes ?
            nvn_graphics_artifact_modes(vs, fs, &linkage, &sys, gmem_bytes, argv[3]) :
            nvn_graphics_artifact(vs, fs, &linkage, &sys, gmem_bytes, argv[3]);
        if (artifact_error) return artifact_error;
        fprintf(file, ",\"device_cache\":{\"gmem_bytes\":%u,\"num_ccu\":%u,\"sysmem_depth_offset\":%u,"
            "\"sysmem_color_offset\":%u,\"sysmem_vpc_attr_offset\":%u,\"sysmem_vpc_attr_size\":%u}",
            gmem_bytes, info->num_ccu, sys.depth_ccu_offset, sys.color_ccu_offset, sys.vpc_attr_buf_offset, sys.vpc_attr_buf_size);
    }
#endif
    if (modes) fprintf(file, ",\"graphics_modes\":true");
    fprintf(file, "}\n"); if (fclose(file)) return 5;
    ralloc_free(vs->shader); ralloc_free(fs->shader);
    ir3_compiler_destroy(compiler); glsl_type_singleton_decref();
    return 0;
}
