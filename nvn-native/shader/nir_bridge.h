/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "nir.h"
#include "nir_builder.h"
#include "compiler/spirv/nir_spirv.h"
#include "ir3/ir3_compiler.h"
#include "ir3/ir3_nir.h"
#include "ir3/ir3_shader.h"
static void reject(const char *message)
{
    fprintf(stderr, "unqualified SPIR-V profile: %s\n", message);
    exit(4);
}

/* Set zero has separate bindful UBO/SSBO tables. A SPIR-V binding number
 * is its table index; no descriptor heap or graphics API object is involved.
 * Arrays and reindexing need an explicit layout contract and are rejected. */
static bool lower_resource(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
    (void)data;
    nir_def *result;
    switch (intr->intrinsic) {
    case nir_intrinsic_vulkan_resource_index:
        if (nir_intrinsic_desc_set(intr) != 0 || nir_intrinsic_binding(intr) >= 16 ||
            !nir_src_is_const(intr->src[0]) || nir_src_as_uint(intr->src[0]) != 0)
            reject("only scalar buffers in set zero, bindings 0..15");
        if (nir_intrinsic_desc_type(intr) != nir_descriptor_type_uniform_buffer &&
            nir_intrinsic_desc_type(intr) != nir_descriptor_type_storage_buffer)
            reject("only UBO/SSBO resources");
        if (intr->def.num_components != 2 || intr->def.bit_size != 32)
            reject("unexpected buffer address format");
        b->cursor = nir_before_instr(&intr->instr);
        result = nir_vec2(b, nir_imm_int(b, nir_intrinsic_binding(intr)), nir_imm_int(b, 0));
        break;
    case nir_intrinsic_load_vulkan_descriptor:
        if ((nir_intrinsic_desc_type(intr) != nir_descriptor_type_uniform_buffer &&
             nir_intrinsic_desc_type(intr) != nir_descriptor_type_storage_buffer) ||
            intr->def.num_components != 2 || intr->def.bit_size != 32)
            reject("unexpected buffer descriptor load");
        result = intr->src[0].ssa;
        break;
    case nir_intrinsic_vulkan_resource_reindex:
        reject("descriptor arrays");
        return false;
    default:
        return false;
    }
    nir_def_replace(&intr->def, result);
    return true;
}

static void dump(nir_shader *nir, const char *prefix, const char *suffix)
{
    char path[4096];
    if (snprintf(path, sizeof(path), "%s%s", prefix, suffix) >= sizeof(path)) exit(2);
    FILE *file = fopen(path, "w");
    if (!file) exit(5);
    nir_print_shader(nir, file);
    if (fclose(file)) exit(5);
}

/* First sampled-image profile: set zero, scalar bindings 0..15, floating
 * non-array 2D implicit-LOD sampling. Texture and sampler tables are separate
 * native domains; their indices are the corresponding SPIR-V bindings. */
static void validate_sampled_variable(const nir_variable *var, bool sampler_source)
{
    if (var->data.mode != nir_var_uniform || var->data.descriptor_set != 0 || var->data.binding >= 16)
        reject("sampled resource declaration layout: scalar set-zero bindings 0..15 required");
    const struct glsl_type *type = var->type;
    if (glsl_type_is_bare_sampler(type)) {
        if (!sampler_source) reject("bare sampler used as a texture");
        return;
    }
    if ((!glsl_type_is_sampler(type) && !glsl_type_is_texture(type)) ||
        (glsl_get_sampler_dim(type) != GLSL_SAMPLER_DIM_2D &&
         glsl_get_sampler_dim(type) != GLSL_SAMPLER_DIM_3D &&
         glsl_get_sampler_dim(type) != GLSL_SAMPLER_DIM_BUF) ||
        glsl_sampler_type_is_array(type) ||
        (glsl_type_is_sampler(type) && glsl_sampler_type_is_shadow(type)) ||
        glsl_get_sampler_result_type(type) != GLSL_TYPE_FLOAT)
        reject("only scalar floating non-array non-shadow 2D/3D or buffer texture declarations");
    if (sampler_source && !glsl_type_is_sampler(type))
        reject("texture-only declaration used as a sampler");
}

/* Accept only pure SPIR-V image/sampler type splits (including redundant
 * same-texture casts from OpImage). No pointer arithmetic/indexing is folded
 * into legality: validate every cast before nir_opt_deref can remove it. */
static void validate_sampled_deref(nir_src src, bool sampler, bool casts_allowed)
{
    nir_deref_instr *deref=nir_src_as_deref(src), *casts[8];unsigned count=0;
    while(deref && deref->deref_type==nir_deref_type_cast) {
        if(!casts_allowed || count==ARRAY_SIZE(casts))
            reject("sampled resource cast depth or lowered cast");
        nir_deref_instr *parent=nir_src_as_deref(deref->parent);
        if(!parent || deref->modes!=nir_var_uniform || parent->modes!=nir_var_uniform ||
           deref->cast.ptr_stride || deref->cast.align_mul || deref->cast.align_offset ||
           deref->def.bit_size!=parent->def.bit_size ||
           deref->def.num_components!=parent->def.num_components)
            reject("sampled resource cast has pointer arithmetic or mode mismatch");
        casts[count++]=deref;deref=parent;
    }
    if(!deref || deref->deref_type!=nir_deref_type_var)
        reject("sampled resource indexing or indirect dereference");
    validate_sampled_variable(deref->var,sampler);
    if(count) {
        const struct glsl_type *expected;
        if(sampler)expected=glsl_bare_sampler_type();
        else expected=glsl_type_is_sampler(deref->type)?
            glsl_sampler_type_to_texture(deref->type):deref->type;
        for(unsigned i=0;i<count;i++)if(casts[i]->type!=expected)
            reject("sampled resource cast target differs from declared source type");
    }
}
static bool validate_sampled_instructions(nir_shader *nir, bool lowered, bool sampler_casts_allowed)
{
    bool found=false;
    nir_foreach_function_impl(impl,nir) nir_foreach_block(block,impl) nir_foreach_instr(instr,block) {
        if(instr->type!=nir_instr_type_tex)continue;
        nir_tex_instr *tex=nir_instr_as_tex(instr);found=true;
        bool sample=tex->op==nir_texop_tex, fetch=tex->op==nir_texop_txf, size=tex->op==nir_texop_txs;
        bool gather=tex->op==nir_texop_tg4, semantic_sampler=sample||gather;
        /* Standard SPIR-V OpImageGather has level-zero semantics in the
         * existing SPIR-V frontend. Bias/LOD-extension and offset variants
         * require independent contracts; never silently lower them to sample. */
        if(gather && (tex->is_gather_implicit_lod || nir_tex_instr_has_explicit_tg4_offsets(tex)))
            reject("gather bias/implicit LOD or four-offset operation not qualified");
        bool buffer=tex->sampler_dim==GLSL_SAMPLER_DIM_BUF, volume=tex->sampler_dim==GLSL_SAMPLER_DIM_3D;
        unsigned dimensions=buffer?1:volume?3:2;
        if((!sample&&!fetch&&!size&&!gather) || (buffer?semantic_sampler:volume?semantic_sampler:tex->sampler_dim!=GLSL_SAMPLER_DIM_2D) || tex->is_array ||
           tex->is_shadow || tex->is_sparse || tex->texture_non_uniform || tex->sampler_non_uniform ||
           tex->coord_components!=(size?0:dimensions) || tex->def.bit_size!=32 ||
           (size?(tex->dest_type!=nir_type_int32 && tex->dest_type!=nir_type_uint32):tex->dest_type!=nir_type_float32) ||
           tex->def.num_components!=(size?dimensions:4))
            reject("only floating scalar 2D sample/gather-level-zero/fetch/size, 3D fetch/size, buffer fetch/size without LOD");
        unsigned textures=0,samplers=0,coords=0,lods=0,offsets=0;
        for(unsigned i=0;i<tex->num_srcs;i++) {
            nir_tex_src *src=tex->src+i;
            switch(src->src_type) {
            case nir_tex_src_coord:
                if(size || src->src.ssa->num_components!=dimensions || src->src.ssa->bit_size!=32)
                    reject("texture coordinates must match the typed dimension and use 32-bit components");
                coords++;continue;
            case nir_tex_src_lod:
                if(semantic_sampler || buffer || src->src.ssa->num_components!=1 || src->src.ssa->bit_size!=32)
                    reject("fetch/size LOD must be scalar 32-bit");
                lods++;continue;
            case nir_tex_src_offset:
                if(gather)reject("gather offset not qualified");
                if(buffer || volume || !fetch || !nir_src_is_const(src->src) || src->src.ssa->num_components!=2 ||
                   src->src.ssa->bit_size!=32)
                    reject("only constant two-component 32-bit texel-fetch offsets");
                offsets++;continue;
            case nir_tex_src_texture_deref:case nir_tex_src_sampler_deref:
                if(lowered)reject("unlowered sampled resource dereference");
                if(src->src_type==nir_tex_src_sampler_deref) {
                    if(!semantic_sampler)reject("fetch/size must not have a semantic sampler source");
                    validate_sampled_deref(src->src,true,sampler_casts_allowed);samplers++;
                } else {validate_sampled_deref(src->src,false,sampler_casts_allowed);textures++;}
                continue;
            default:reject("texture handle, array offset or unsupported operand");
            }
        }
        if(coords!=(size?0:1) || lods!=((semantic_sampler||buffer)?0:1) || offsets>1 ||
           (!lowered && (textures!=1 || samplers!=(semantic_sampler?1:0))))
            reject("invalid typed scalar texture operation source shape");
        if(lowered && (tex->texture_index>=16 || tex->sampler_index>=16 ||
                      (!semantic_sampler && tex->sampler_index!=0)))
            reject("native texture/sampler index exceeds profile or unused sampler is nonzero");
    }
    return found;
}

/* nir_shader_gather_info counts declarations, not sparse bindful indices.
 * Restore this native table contract after the caller's last gather as well. */
static void nvn_update_sampled_info(nir_shader *nir)
{
    if (!validate_sampled_instructions(nir, true, false)) return;
    BITSET_ZERO(nir->info.textures_used);
    BITSET_ZERO(nir->info.samplers_used);
    unsigned texture_slots = 0;
    nir_foreach_function_impl(impl, nir) {
        nir_foreach_block(block, impl) {
            nir_foreach_instr(instr, block) {
                if (instr->type != nir_instr_type_tex) continue;
                nir_tex_instr *tex = nir_instr_as_tex(instr);
                BITSET_SET(nir->info.textures_used, tex->texture_index);
                if(nir_tex_instr_need_sampler(tex)) BITSET_SET(nir->info.samplers_used, tex->sampler_index);
                texture_slots = MAX2(texture_slots, tex->texture_index + 1);
            }
        }
    }
    nir->info.num_textures = texture_slots;
}


/* Explicit native backend contract, never a replacement for guest CB0.
 * Only translator support_buffer.RenderScale[i] with its complete known type
 * and an in-range constant scalar index is folded. Other support fields stay
 * real UBO reads and continue to require independent runtime providers. */
struct nvn_unit_scale_usage { unsigned replaced_scale_loads,remaining_support_loads; };
static bool nvn_unit_render_scale_requested;
static struct nvn_unit_scale_usage nvn_unit_scale_last;
static bool nvn_scale_scalar(const struct glsl_type *t,enum glsl_base_type base)
{return glsl_type_is_scalar(t)&&glsl_get_base_type(t)==base&&glsl_get_bit_size(t)==32;}
static bool nvn_scale_vector(const struct glsl_type *t,enum glsl_base_type base)
{return glsl_type_is_vector(t)&&glsl_get_vector_elements(t)==4&&glsl_get_base_type(t)==base&&glsl_get_bit_size(t)==32;}
static bool nvn_support_type(const struct glsl_type *t)
{
 if((!glsl_type_is_struct(t)&&!glsl_type_is_interface(t))||glsl_get_length(t)!=8)return false;
 const unsigned offsets[8]={0,16,144,160,176,192,1360,1376};
 for(unsigned i=0;i<8;i++)if(glsl_get_struct_field_offset(t,i)!=(int)offsets[i])return false;
 const struct glsl_type *a=glsl_get_struct_field(t,1),*scale=glsl_get_struct_field(t,5);
 return nvn_scale_scalar(glsl_get_struct_field(t,0),GLSL_TYPE_UINT)&&
  glsl_type_is_array(a)&&glsl_get_length(a)==8&&glsl_get_explicit_stride(a)==16&&
  nvn_scale_scalar(glsl_get_array_element(a),GLSL_TYPE_UINT)&&
  nvn_scale_vector(glsl_get_struct_field(t,2),GLSL_TYPE_FLOAT)&&
  nvn_scale_vector(glsl_get_struct_field(t,3),GLSL_TYPE_FLOAT)&&
  nvn_scale_scalar(glsl_get_struct_field(t,4),GLSL_TYPE_INT)&&
  glsl_type_is_array(scale)&&glsl_get_length(scale)==73&&glsl_get_explicit_stride(scale)==16&&
  nvn_scale_scalar(glsl_get_array_element(scale),GLSL_TYPE_FLOAT)&&
  nvn_scale_vector(glsl_get_struct_field(t,6),GLSL_TYPE_INT)&&
  nvn_scale_scalar(glsl_get_struct_field(t,7),GLSL_TYPE_INT);
}
static bool nvn_support_descriptor(nir_src src)
{
 nir_instr *ins=nir_def_instr(src.ssa);if(ins->type!=nir_instr_type_intrinsic)return false;
 nir_intrinsic_instr *load=nir_instr_as_intrinsic(ins);
 if(load->intrinsic!=nir_intrinsic_load_vulkan_descriptor||nir_intrinsic_desc_type(load)!=nir_descriptor_type_uniform_buffer)return false;
 ins=nir_def_instr(load->src[0].ssa);if(ins->type!=nir_instr_type_intrinsic)return false;
 nir_intrinsic_instr *index=nir_instr_as_intrinsic(ins);
 return index->intrinsic==nir_intrinsic_vulkan_resource_index&&
  nir_intrinsic_desc_type(index)==nir_descriptor_type_uniform_buffer&&
  nir_intrinsic_desc_set(index)==0&&nir_intrinsic_binding(index)==0&&
  nir_src_is_const(index->src[0])&&nir_src_as_uint(index->src[0])==0;
}
static bool nvn_fold_unit_scale(nir_builder *b,nir_intrinsic_instr *load,void *unused)
{
 (void)unused;
 if(load->intrinsic!=nir_intrinsic_load_deref)return false;
 nir_deref_instr *leaf=nir_src_as_deref(load->src[0]),*root=leaf;
 while(root&&root->deref_type!=nir_deref_type_cast&&root->deref_type!=nir_deref_type_var)root=nir_deref_instr_parent(root);
 if(!root||root->deref_type!=nir_deref_type_cast||!nvn_support_descriptor(root->parent))return false;
 bool exact=root->modes==nir_var_mem_ubo&&nvn_support_type(root->type)&&
  !root->cast.ptr_stride&&!root->cast.align_mul&&!root->cast.align_offset;
 nir_deref_instr *field=leaf&&leaf->deref_type==nir_deref_type_array?nir_deref_instr_parent(leaf):NULL;
 exact=exact&&leaf&&leaf->deref_type==nir_deref_type_array&&leaf->arr.in_bounds&&
  leaf->arr.index.ssa->num_components==1&&leaf->arr.index.ssa->bit_size==32&&
  nir_src_is_const(leaf->arr.index)&&nir_src_as_uint(leaf->arr.index)<73&&
  field&&field->deref_type==nir_deref_type_struct&&field->strct.index==5&&nir_deref_instr_parent(field)==root&&
  load->def.num_components==1&&load->def.bit_size==32&&nvn_scale_scalar(leaf->type,GLSL_TYPE_FLOAT);
 if(!exact){nvn_unit_scale_last.remaining_support_loads++;return false;}
 b->cursor=nir_before_instr(&load->instr);
 nir_def_replace(&load->def,nir_imm_float(b,1.0f));nvn_unit_scale_last.replaced_scale_loads++;
 return true;
}
static void nvn_apply_unit_scale(nir_shader *nir)
{
 memset(&nvn_unit_scale_last,0,sizeof(nvn_unit_scale_last));
 if(!nvn_unit_render_scale_requested)return;
 const nir_variable *support=NULL;
 nir_foreach_variable_with_modes(v,nir,nir_var_mem_ubo)if(v->data.descriptor_set==0&&v->data.binding==0){
  if(support)reject("unit scale: duplicate UBO0 declarations");
  support=v;
 }
 if(!support)return;
 if(!support->name||strcmp(support->name,"support_buffer")||!nvn_support_type(support->type))
  reject("unit scale: binding0 is not the exact translator support_buffer type");
 nir_shader_intrinsics_pass(nir,nvn_fold_unit_scale,nir_metadata_control_flow,NULL);
}

static nir_shader *nvn_import_nir(const char *input_path, mesa_shader_stage stage,
                                  const char *prefix, struct ir3_compiler *compiler)
{
    FILE *input = fopen(input_path, "rb");
    if (!input || fseek(input, 0, SEEK_END)) return NULL;
    long length = ftell(input);
    if (length < 20 || length > 16 * 1024 * 1024 || length % 4 || fseek(input, 0, SEEK_SET)) return NULL;
    uint32_t *words = malloc(length);
    if (!words || fread(words, 1, length, input) != length || fclose(input)) return NULL;
    const struct spirv_to_nir_options options = {
        .environment = NIR_SPIRV_VULKAN,
        .ubo_addr_format = nir_address_format_32bit_index_offset,
        .ssbo_addr_format = nir_address_format_32bit_index_offset,
        .shared_addr_format = nir_address_format_32bit_offset,
        .temp_addr_format = nir_address_format_32bit_offset,
        .ignore_replacement = true,
        .skip_os_break_in_debug_build = true,
    };
    nir_shader *nir = spirv_to_nir(words, length / 4, NULL, stage,
                                  "main", &options, ir3_get_compiler_options(compiler));
    free(words);
    if (!nir) return NULL;
    dump(nir, prefix, ".import.nir");
    nir_foreach_variable_with_modes(var, nir, nir_var_uniform | nir_var_image) {
        // Bare samplers are valid only as the sampler side of a sampled pair.
        // Storage images, aggregates and all other uniform forms reject here.
        validate_sampled_variable(var, glsl_type_is_bare_sampler(var->type));
    }
    unsigned ubos = 0, ssbos = 0;
    nir_foreach_variable_with_modes(var, nir, nir_var_mem_ubo | nir_var_mem_ssbo) {
        if (glsl_type_is_array(var->type) || var->data.descriptor_set != 0 || var->data.binding >= 16)
            reject("buffer declaration layout");
        if (var->data.mode == nir_var_mem_ubo) ubos = MAX2(ubos, var->data.binding + 1);
        else ssbos = MAX2(ssbos, var->data.binding + 1);
    }
    if (stage == MESA_SHADER_COMPUTE && (nir->info.shared_size || nir->info.workgroup_size_variable ||
        (uint64_t)nir->info.workgroup_size[0] * nir->info.workgroup_size[1] * nir->info.workgroup_size[2] > 1024))
        reject("shared memory or workgroup size");
    NIR_PASS(_, nir, nir_lower_variable_initializers, nir_var_all);
    NIR_PASS(_, nir, nir_lower_returns);
    NIR_PASS(_, nir, nir_inline_functions);
    nir_remove_non_entrypoints(nir);
    NIR_PASS(_, nir, nir_split_var_copies);
    NIR_PASS(_, nir, nir_lower_var_copies);
    NIR_PASS(_, nir, nir_lower_vars_to_ssa);
    NIR_PASS(_, nir, nir_lower_system_values);
    if(nvn_unit_render_scale_requested){
        /* Validate texture pointer splits BEFORE generic deref optimization;
         * it may otherwise hide an illegal resource cast. Existing Mesa deref
         * and SSA passes expose the inlined helper scalar arguments. */
        validate_sampled_instructions(nir,false,true);
        NIR_PASS(_, nir, nir_opt_deref);
        NIR_PASS(_, nir, nir_lower_vars_to_ssa);
        bool progress;do{progress=false;
            NIR_PASS(progress, nir, nir_opt_remove_phis);
            NIR_PASS(progress, nir, nir_opt_copy_prop);
            NIR_PASS(progress, nir, nir_opt_constant_folding);
        }while(progress);
    }
    if(nvn_unit_render_scale_requested)dump(nir,prefix,".unit-before.nir");
    nvn_apply_unit_scale(nir);
    if (validate_sampled_instructions(nir, false, true)) {
        // This existing Mesa pass removes the strictly checked combined-image
        // type splits; validation below still requires plain scalar variables.
        NIR_PASS(_, nir, nir_opt_deref);
        validate_sampled_instructions(nir, false, false);
        NIR_PASS(_, nir, nir_lower_samplers);
        validate_sampled_instructions(nir, true, false);
    }
    NIR_PASS(_, nir, nir_lower_explicit_io, nir_var_mem_ubo | nir_var_mem_ssbo,
             nir_address_format_32bit_index_offset);
    nir_shader_intrinsics_pass(nir, lower_resource, nir_metadata_control_flow, NULL);
    NIR_PASS(_, nir, nir_opt_copy_prop);
    NIR_PASS(_, nir, nir_opt_constant_folding);
    NIR_PASS(_, nir, nir_opt_dce);
    nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));
    nvn_update_sampled_info(nir);
    nir->info.num_ubos = ubos;
    nir->info.num_ssbos = ssbos;
    nir_validate_shader(nir, "native bindful buffers");
    dump(nir, prefix, ".nir");
    return nir;
}
