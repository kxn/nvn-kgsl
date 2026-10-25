/* SPDX-License-Identifier: MIT
 * Shader-dependent hardware recipe from actual common IR3 variants. Register
 * semantics: pinned Gallium a6xx/fd6_program.cc emit_shader_regs/emit_fs_inputs/
 * emit_fs_outputs and generated Qualcomm register definitions. No API runtime,
 * target size, vertex stride, draw command, application slot or game identity.
 */
#include <cstdio>
#include <cstring>
#include <vector>
#include "freedreno_pm4.h"
#include "fd6_hw.h"
#include "ir3/ir3_shader.h"
#include "nvn_pipeline_recipe.h"
namespace {
static bool supported(const ir3_shader_variant *vs,const ir3_shader_variant *fs,
                      const ir3_shader_linkage *link,bool runtime_constants)
{
    if(!vs||!fs||!link||vs->compiler!=fs->compiler||vs->compiler->gen!=6||
       vs->compiler->dev_id->chip_id!=0x06050002||link->cnt>32||link->max_loc>128||
       link->primid_loc!=255||link->viewid_loc!=255||link->clip0_loc!=255||link->clip1_loc!=255)
        return false;
    for(auto *v:{vs,fs}) {
        auto *c=ir3_const_state(v);
        if(!v->bin||!v->info.size||v->info.size%4||v->info.size>65536||v->pvtmem_size||
           v->shared_size||v->num_uavs>32||v->constant_data_size||v->info.double_threadsize||
           v->info.max_reg<0||v->info.max_reg>=48||v->info.max_half_reg!=-1||!v->mergedregs||
           v->constlen%4||v->constlen>128||v->bindless_tex||v->bindless_samp||v->bindless_ibo||
           v->bindless_ubo||v->clip_mask||v->cull_mask||v->stream_output.num_outputs||
           v->num_sampler_prefetch||v->num_samp>16||(!runtime_constants&&(c->num_ubos||c->num_driver_params))||
           c->driver_params_ubo.idx>=0||c->consts_ubo.idx>=0||c->primitive_map_ubo.idx>=0||
           c->primitive_param_ubo.idx>=0||v->imm_state.count>128||
           (v->imm_state.count&&(!v->imm_state.values||v->imm_state.size<align(v->imm_state.count,4))))
            return false;
        if(runtime_constants) {
            const auto &a=c->allocs.consts[IR3_CONST_ALLOC_DRIVER_PARAMS];
            if(c->nvn_driver_unknown||c->nvn_remaining_ubo_unknown||c->num_ubos>16||
               c->nvn_remaining_ubo_mask>65535u||
               (c->num_ubos?(!c->nvn_remaining_ubo_mask||unsigned(32-__builtin_clz(c->nvn_remaining_ubo_mask))!=c->num_ubos):c->nvn_remaining_ubo_mask))return false;
            if(c->num_driver_params) {
                if(v!=vs||c->num_driver_params!=4||!c->nvn_driver_word_mask||
                   c->nvn_driver_word_mask&~7u||!a.offset_vec4||a.size_vec4!=4||
                   a.offset_vec4%4||a.offset_vec4+a.size_vec4>v->constlen)return false;
            }else if(c->nvn_driver_word_mask||a.size_vec4)return false;
        }
        if(v->imm_state.count&&c->allocs.max_const_offset_vec4+DIV_ROUND_UP(v->imm_state.count,4)>v->constlen)
            return false;
        for(unsigned i=0;i<c->ubo_state.num_enabled;i++) {
            const auto &r=c->ubo_state.range[i];
            if(r.ubo.bindless||r.ubo.global||r.ubo.block>=16||r.offset%64||r.start%64||
               r.end<=r.start||r.end%64||r.offset/16+(r.end-r.start)/16>v->constlen)
                return false;
        }
    }
    if(vs->num_samp||vs->inputs_count>34||vs->outputs_count>32||fs->total_in>128||
       fs->sample_shading||fs->writes_smask||fs->writes_stencilref||
       fs->reads_primid||fs->dual_src_blend||fs->color0_mrt||
       fs->post_depth_coverage)
        return false;
    /* Fullprecision scalar depth export, optionally with DATA0. No fake
     * DATA0 is synthesized for a genuine depth-only fragment shader. */
    unsigned depth_outputs=0,color_outputs=0;
    for(unsigned i=0;i<fs->outputs_count;i++) {
        const auto &out=fs->outputs[i];
        if(out.regid==INVALID_REG||out.regid>=192||out.half||out.aliased_components)return false;
        if(out.slot==FRAG_RESULT_DATA0)color_outputs++;
        else if(out.slot==FRAG_RESULT_DEPTH)depth_outputs++;
        else return false;
    }
    if(fs->writes_pos) {
        if(depth_outputs!=1||color_outputs>1||fs->fs.early_fragment_tests)return false;
    } else if(depth_outputs||color_outputs!=1||fs->outputs_count!=1)return false;
    for(unsigned i=0;i<vs->inputs_count;i++) {
        const auto &in=vs->inputs[i];
        if(in.regid==INVALID_REG||in.bary||!in.compmask||in.compmask>15)return false;
        if(in.sysval) {
            if(in.slot!=SYSTEM_VALUE_VERTEX_ID&&in.slot!=SYSTEM_VALUE_INSTANCE_ID)return false;
        } else if(in.slot<VERT_ATTRIB_GENERIC0||in.slot>=VERT_ATTRIB_GENERIC0+32)return false;
    }
    unsigned face_inputs=0;
    for(unsigned i=0;i<fs->inputs_count;i++) {
        const auto &in=fs->inputs[i];
        if(!in.compmask||in.compmask>15||in.flat||in.rasterflat)return false;
        if(in.sysval) {
            /* INVALID_REG is an unused recognized sysval after IR3 DCE;
             * live input writes must remain inside the thread register file. */
            if(in.regid!=INVALID_REG&&in.regid>=192)return false;
            if(in.slot==SYSTEM_VALUE_FRONT_FACE) {
                face_inputs++;
                if(!fs->frag_face||!in.half||in.compmask!=1||in.bary)return false;
                /* ir3.c accounts merged half scalar indices in max_reg using
                 * regid>>3; do not invent a separate half-bank allocation. */
                if(in.regid!=INVALID_REG&&in.regid/8u>unsigned(fs->info.max_reg))return false;
                continue;
            }
            if(in.half)return false;
            if(in.slot==SYSTEM_VALUE_BARYCENTRIC_PERSP_CENTER_RHW)return false;
            if(in.slot!=SYSTEM_VALUE_FRAG_COORD &&
               (in.slot<SYSTEM_VALUE_BARYCENTRIC_PERSP_PIXEL||in.slot>SYSTEM_VALUE_BARYCENTRIC_LINEAR_SAMPLE))
                return false;
        } else if(!in.bary||in.inloc+util_bitcount(in.compmask)>fs->total_in)return false;
    }
    if(face_inputs>1||bool(face_inputs)!=fs->frag_face)return false;
    for(unsigned i=0;i<vs->outputs_count;i++)
        if(vs->outputs[i].half||vs->outputs[i].aliased_components)return false;
    return true;
}
}
extern "C" int nvn_pipeline_artifact(const ir3_shader_variant *vs,const ir3_shader_variant *fs,
 const ir3_shader_linkage *link,uint32_t texture_mask,uint32_t sampler_mask,const uint32_t storage_masks[2],bool runtime_constants,const char *prefix)
{
    if(!prefix||!storage_masks||strlen(prefix)>4000||!supported(vs,fs,link,runtime_constants)||
       (storage_masks[0]|storage_masks[1])>65535u||
       (vs->num_uavs&&!storage_masks[0])||(fs->num_uavs&&!storage_masks[1])) {
        fprintf(stderr,"pipeline recipe: unsupported A650 shader/linkage feature\n");return 9;
    }
    std::vector<uint32_t> words;
    std::vector<nn_pipeline_address_patch> patches;
    std::vector<nn_pipeline_vertex_input> inputs;
    auto reg=[&](uint32_t r,uint32_t v=0){words.push_back(pm4_pkt4_hdr(r,1));words.push_back(v);};
    auto zero_addr=[&](uint32_t r){words.push_back(pm4_pkt4_hdr(r,2));words.insert(words.end(),{0,0});};
    auto p7=[&](uint32_t op,uint32_t count){words.push_back(pm4_pkt7_hdr(op,count));};
    auto address=[&](uint32_t role,uint32_t stage,uint32_t binding,uint32_t offset,uint32_t bytes){
        patches.push_back({uint32_t(words.size()),role,stage,binding,offset,bytes});words.insert(words.end(),{0,0});};
    auto reg_addr=[&](uint32_t r,uint32_t role,uint32_t stage,uint32_t binding,uint32_t bytes){
        words.push_back(pm4_pkt4_hdr(r,2));address(role,stage,binding,0,bytes);};
    const auto *info=vs->compiler->info;
    const bool writes_depth=fs->writes_pos;
    const uint32_t depth_regid=writes_depth?ir3_find_output_regid(fs,FRAG_RESULT_DEPTH):INVALID_REG;
    const uint32_t color_outputs=ir3_find_output_regid(fs,FRAG_RESULT_DATA0)!=INVALID_REG;
    reg(REG_A6XX_SP_MODE_CNTL,A6XX_SP_MODE_CNTL_CONSTANT_DEMOTION_ENABLE|A6XX_SP_MODE_CNTL_ISAMMODE(ISAMMODE_GL));
    reg(REG_A6XX_TPL1_MODE_CNTL,A6XX_TPL1_MODE_CNTL_ISAMMODE(ISAMMODE_GL)|
        A6XX_TPL1_MODE_CNTL_TEXCOORDROUNDMODE(COORD_TRUNCATE)|A6XX_TPL1_MODE_CNTL_ARRAYCOORDROUNDMODE(COORD_TRUNCATE)|
        A6XX_TPL1_MODE_CNTL_NEARESTMIPSNAP(CLAMP_ROUND_TRUNCATE)|A6XX_TPL1_MODE_CNTL_DESTDATATYPEOVERRIDE);
    reg(REG_A6XX_SP_VS_CONST_CONFIG,A6XX_SP_VS_CONST_CONFIG_CONSTLEN(vs->constlen)|A6XX_SP_VS_CONST_CONFIG_ENABLED);
    reg(REG_A6XX_SP_HS_CONST_CONFIG);reg(REG_A6XX_SP_DS_CONST_CONFIG);reg(REG_A6XX_SP_GS_CONST_CONFIG);
    reg(REG_A6XX_SP_PS_CONST_CONFIG,A6XX_SP_PS_CONST_CONFIG_CONSTLEN(fs->constlen)|A6XX_SP_PS_CONST_CONFIG_ENABLED);
    reg(REG_A6XX_SP_VS_CONFIG,A6XX_SP_VS_CONFIG_ENABLED|A6XX_SP_VS_CONFIG_NUAV(vs->num_uavs));reg(REG_A6XX_SP_HS_CONFIG);reg(REG_A6XX_SP_DS_CONFIG);reg(REG_A6XX_SP_GS_CONFIG);
    reg(REG_A6XX_SP_PS_CONFIG,A6XX_SP_PS_CONFIG_ENABLED|A6XX_SP_PS_CONFIG_NUAV(fs->num_uavs)|A6XX_SP_PS_CONFIG_NTEX(fs->num_samp)|A6XX_SP_PS_CONFIG_NSAMP(fs->num_samp));
    for(unsigned stage=0;stage<2;stage++) {
        const auto *v=stage?fs:vs;const auto *c=ir3_const_state(v);
        reg(stage?REG_A6XX_SP_PS_CNTL_0:REG_A6XX_SP_VS_CNTL_0,stage?
            A6XX_SP_PS_CNTL_0_HALFREGFOOTPRINT(v->info.max_half_reg+1)|A6XX_SP_PS_CNTL_0_FULLREGFOOTPRINT(v->info.max_reg+1)|
            A6XX_SP_PS_CNTL_0_BRANCHSTACK(ir3_shader_branchstack_hw(v))|A6XX_SP_PS_CNTL_0_THREADSIZE(THREAD64)|
            (v->total_in?A6XX_SP_PS_CNTL_0_VARYING:0)|A6XX_SP_PS_CNTL_0_INOUTREGOVERLAP|A6XX_SP_PS_CNTL_0_MERGEDREGS|
            (v->early_preamble?A6XX_SP_PS_CNTL_0_EARLYPREAMBLE:0)|(v->need_full_quad?A6XX_SP_PS_CNTL_0_LODPIXMASK:0)|
            (v->need_pixlod?A6XX_SP_PS_CNTL_0_PIXLODENABLE:0):
            A6XX_SP_VS_CNTL_0_HALFREGFOOTPRINT(v->info.max_half_reg+1)|A6XX_SP_VS_CNTL_0_FULLREGFOOTPRINT(v->info.max_reg+1)|
            A6XX_SP_VS_CNTL_0_BRANCHSTACK(ir3_shader_branchstack_hw(v))|A6XX_SP_VS_CNTL_0_MERGEDREGS|
            (v->early_preamble?A6XX_SP_VS_CNTL_0_EARLYPREAMBLE:0));
        reg(stage?REG_A6XX_SP_PS_INSTR_SIZE:REG_A6XX_SP_VS_INSTR_SIZE,v->instrlen);
        reg(stage?REG_A6XX_SP_PS_PROGRAM_COUNTER_OFFSET:REG_A6XX_SP_VS_PROGRAM_COUNTER_OFFSET);
        reg_addr(stage?REG_A6XX_SP_PS_BASE:REG_A6XX_SP_VS_BASE,NN_PIPELINE_CODE,stage,0,v->info.size);
        reg(stage?REG_A6XX_SP_PS_PVT_MEM_PARAM:REG_A6XX_SP_VS_PVT_MEM_PARAM);
        zero_addr(stage?REG_A6XX_SP_PS_PVT_MEM_BASE:REG_A6XX_SP_VS_PVT_MEM_BASE);
        reg(stage?REG_A6XX_SP_PS_PVT_MEM_SIZE:REG_A6XX_SP_VS_PVT_MEM_SIZE);
        reg(stage?REG_A6XX_SP_PS_PVT_MEM_STACK_OFFSET:REG_A6XX_SP_VS_PVT_MEM_STACK_OFFSET);
        const auto block=stage?SB6_FS_SHADER:SB6_VS_SHADER;const auto op=stage?CP_LOAD_STATE6_FRAG:CP_LOAD_STATE6_GEOM;
        p7(op,3);words.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_SHADER)|CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT)|
            CP_LOAD_STATE6_0_STATE_BLOCK(block)|CP_LOAD_STATE6_0_NUM_UNIT(MIN2(v->instrlen,info->props.instr_cache_size)));
        address(NN_PIPELINE_CODE,stage,0,0,v->info.size);
        for(unsigned i=0;i<c->ubo_state.num_enabled;i++) {
            const auto &r=c->ubo_state.range[i];
            p7(op,3);words.push_back(CP_LOAD_STATE6_0_DST_OFF(r.offset/16)|CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS)|
                CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT)|CP_LOAD_STATE6_0_STATE_BLOCK(block)|CP_LOAD_STATE6_0_NUM_UNIT((r.end-r.start)/16));
            address(NN_PIPELINE_UBO,stage,r.ubo.block,r.start,r.end-r.start);
        }
        if(runtime_constants&&c->num_driver_params) {
            const auto &a=c->allocs.consts[IR3_CONST_ALLOC_DRIVER_PARAMS];
            p7(op,3);words.push_back(CP_LOAD_STATE6_0_DST_OFF(a.offset_vec4)|CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS)|
                CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT)|CP_LOAD_STATE6_0_STATE_BLOCK(block)|CP_LOAD_STATE6_0_NUM_UNIT(a.size_vec4));
            address(NN_PIPELINE_DRIVER_PARAMS,stage,0,0,a.size_vec4*16);
        }
        if(runtime_constants&&c->num_ubos) {
            /* Native 64-bit UBO descriptors, exactly the Gallium ST6_UBO table
             * contract. Holes are zero; runtime resolves each used full range. */
            p7(op,3+2*c->num_ubos);words.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_UBO)|
                CP_LOAD_STATE6_0_STATE_SRC(SS6_DIRECT)|CP_LOAD_STATE6_0_STATE_BLOCK(block)|CP_LOAD_STATE6_0_NUM_UNIT(c->num_ubos));
            words.insert(words.end(),{0,0});
            for(unsigned b=0;b<c->num_ubos;b++) {
                if(c->nvn_remaining_ubo_mask&(1u<<b))address(NN_PIPELINE_DYNAMIC_UBO,stage,b,0,16);
                else words.insert(words.end(),{0,0});
            }
        }
        if(v->imm_state.count) {
            unsigned count=align(v->imm_state.count,4);p7(op,3+count);
            words.push_back(CP_LOAD_STATE6_0_DST_OFF(c->allocs.max_const_offset_vec4)|CP_LOAD_STATE6_0_STATE_TYPE(ST6_CONSTANTS)|
                CP_LOAD_STATE6_0_STATE_SRC(SS6_DIRECT)|CP_LOAD_STATE6_0_STATE_BLOCK(block)|CP_LOAD_STATE6_0_NUM_UNIT(count/4));
            words.insert(words.end(),{0,0});words.insert(words.end(),v->imm_state.values,v->imm_state.values+count);
        }
    }
    if(storage_masks[0]||storage_masks[1]) {
        const uint32_t entries=MAX2(vs->num_uavs,fs->num_uavs);
        /* Actual register XML: the graphics IBO cache is shared across stages.
         * Gallium fd6_image.cc uses ST6_SHADER/SB6_UAV for graphics storage. */
        reg(REG_A6XX_SP_GFX_USIZE,entries);
        reg_addr(REG_A6XX_SP_GFX_UAV_BASE,NN_PIPELINE_STORAGE,0,0,entries*64);
        p7(CP_LOAD_STATE6,3);words.push_back(CP_LOAD_STATE6_0_STATE_TYPE(ST6_SHADER)|
            CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT)|CP_LOAD_STATE6_0_STATE_BLOCK(SB6_UAV)|CP_LOAD_STATE6_0_NUM_UNIT(entries));
        address(NN_PIPELINE_STORAGE,0,0,0,entries*64);
    }
    if(fs->num_samp) {
        reg_addr(REG_A6XX_SP_PS_SAMPLER_BASE,NN_PIPELINE_SAMPLERS,1,0,fs->num_samp*16);
        reg_addr(REG_A6XX_SP_PS_TEXMEMOBJ_BASE,NN_PIPELINE_TEXTURES,1,0,fs->num_samp*64);reg(REG_A6XX_SP_PS_TSIZE,fs->num_samp);
        for(unsigned role:{NN_PIPELINE_SAMPLERS,NN_PIPELINE_TEXTURES}) {
            p7(CP_LOAD_STATE6_FRAG,3);words.push_back(CP_LOAD_STATE6_0_STATE_TYPE(role==NN_PIPELINE_SAMPLERS?ST6_SHADER:ST6_CONSTANTS)|
                CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT)|CP_LOAD_STATE6_0_STATE_BLOCK(SB6_FS_TEX)|CP_LOAD_STATE6_0_NUM_UNIT(fs->num_samp));
            address(role,1,0,0,fs->num_samp*(role==NN_PIPELINE_SAMPLERS?16:64));
        }
    } else if(texture_mask||sampler_mask)return 9;
    if((texture_mask|sampler_mask)&~((1u<<fs->num_samp)-1u))return 9;
    uint32_t mask[4]={};
    for(unsigned i=0;i<fs->inputs_count;i++)if(fs->inputs[i].bary) {
        unsigned loc=fs->inputs[i].inloc;
        for(unsigned bit=0;bit<4;bit++)if(fs->inputs[i].compmask&(1u<<bit)){mask[loc/32]|=1u<<(loc%32);loc++;}
    }
    for(unsigned i=0;i<4;i++)reg(REG_A6XX_VPC_VARYING_LM_TRANSFER_CNTL(i),~mask[i]);
    uint16_t out[32]={};uint8_t dest[32]={};unsigned pos=255,psize=255;
    for(unsigned i=0;i<link->cnt;i++) {
        out[i]=A6XX_SP_VS_OUTPUT_REG_A_REGID(link->var[i].regid)|A6XX_SP_VS_OUTPUT_REG_A_COMPMASK(link->var[i].compmask);
        dest[i]=link->var[i].loc;if(link->var[i].slot==VARYING_SLOT_POS)pos=dest[i];if(link->var[i].slot==VARYING_SLOT_PSIZ)psize=dest[i];
    }
    if(pos==255)return 9;
    for(unsigned i=0;i<unsigned(DIV_ROUND_UP(link->cnt,2));i++)reg(REG_A6XX_SP_VS_OUTPUT_REG(i),out[2*i]|uint32_t(out[2*i+1])<<16);
    for(unsigned i=0;i<unsigned(DIV_ROUND_UP(link->cnt,4));i++)reg(REG_A6XX_SP_VS_VPC_DEST_REG(i),dest[4*i]|uint32_t(dest[4*i+1])<<8|uint32_t(dest[4*i+2])<<16|uint32_t(dest[4*i+3])<<24);
    reg(REG_A6XX_SP_VS_OUTPUT_CNTL,A6XX_SP_VS_OUTPUT_CNTL_OUT(link->cnt));
    reg(REG_A6XX_VPC_VS_CNTL,A6XX_VPC_VS_CNTL_STRIDE_IN_VPC(link->max_loc)|A6XX_VPC_VS_CNTL_POSITIONLOC(pos)|A6XX_VPC_VS_CNTL_PSIZELOC(psize));
    reg(REG_A6XX_VPC_VS_CLIP_CULL_CNTL,A6XX_VPC_VS_CLIP_CULL_CNTL_CLIP_DIST_03_LOC(255)|A6XX_VPC_VS_CLIP_CULL_CNTL_CLIP_DIST_47_LOC(255));
    reg(REG_A6XX_VPC_VS_CLIP_CULL_CNTL_V2,A6XX_VPC_VS_CLIP_CULL_CNTL_V2_CLIP_DIST_03_LOC(255)|A6XX_VPC_VS_CLIP_CULL_CNTL_V2_CLIP_DIST_47_LOC(255));
    reg(REG_A6XX_GRAS_CL_VS_CLIP_CULL_DISTANCE);
    reg(REG_A6XX_PC_VS_CNTL,A6XX_PC_VS_CNTL_STRIDE_IN_VPC(link->max_loc)|(psize!=255?A6XX_PC_VS_CNTL_PSIZE:0));
    reg(REG_A6XX_VPC_VS_SIV_CNTL,A6XX_VPC_VS_SIV_CNTL_LAYERLOC(255)|A6XX_VPC_VS_SIV_CNTL_VIEWLOC(255)|A6XX_VPC_VS_SIV_CNTL_SHADINGRATELOC(255));
    reg(REG_A6XX_VPC_VS_SIV_CNTL_V2,A6XX_VPC_VS_SIV_CNTL_V2_LAYERLOC(255)|A6XX_VPC_VS_SIV_CNTL_V2_VIEWLOC(255)|A6XX_VPC_VS_SIV_CNTL_V2_SHADINGRATELOC(255));
    reg(REG_A6XX_PC_PS_CNTL);
    reg(REG_A6XX_VPC_PS_CNTL,A6XX_VPC_PS_CNTL_NUMNONPOSVAR(fs->total_in)|A6XX_VPC_PS_CNTL_PRIMIDLOC(255)|A6XX_VPC_PS_CNTL_VIEWIDLOC(255)|(fs->total_in?A6XX_VPC_PS_CNTL_VARYING:0));
    for(unsigned i=0;i<8;i++){reg(REG_A6XX_VPC_VARYING_INTERP_MODE(i));reg(REG_A6XX_VPC_VARYING_REPLACE_MODE(i));}
    uint32_t ij[IJ_COUNT];for(unsigned i=0;i<IJ_COUNT;i++)ij[i]=ir3_find_sysval_regid(fs,(gl_system_value)(SYSTEM_VALUE_BARYCENTRIC_PERSP_PIXEL+i));
    auto valid=[](uint32_t id){return id!=INVALID_REG;};
    uint32_t face=ir3_find_sysval_regid(fs,SYSTEM_VALUE_FRONT_FACE);
    uint32_t coord=ir3_find_sysval_regid(fs,SYSTEM_VALUE_FRAG_COORD),zw=valid(coord)?coord+2:INVALID_REG;
    reg(REG_A6XX_SP_PS_INITIAL_TEX_LOAD_CNTL,!valid(ij[IJ_PERSP_PIXEL])?A6XX_SP_PS_INITIAL_TEX_LOAD_CNTL_IJ_WRITE_DISABLE:0);
    reg(REG_A6XX_SP_LB_PARAM_LIMIT,A6XX_SP_LB_PARAM_LIMIT_PRIMALLOCTHRESHOLD(info->props.prim_alloc_threshold));
    reg(REG_A6XX_SP_REG_PROG_ID_0,A6XX_SP_REG_PROG_ID_0_FACEREGID(face)|A6XX_SP_REG_PROG_ID_0_SAMPLEID(INVALID_REG)|A6XX_SP_REG_PROG_ID_0_SAMPLEMASK(INVALID_REG)|A6XX_SP_REG_PROG_ID_0_CENTERRHW(INVALID_REG));
    reg(REG_A6XX_SP_REG_PROG_ID_1,A6XX_SP_REG_PROG_ID_1_IJ_PERSP_PIXEL(ij[IJ_PERSP_PIXEL])|A6XX_SP_REG_PROG_ID_1_IJ_LINEAR_PIXEL(ij[IJ_LINEAR_PIXEL])|A6XX_SP_REG_PROG_ID_1_IJ_PERSP_CENTROID(ij[IJ_PERSP_CENTROID])|A6XX_SP_REG_PROG_ID_1_IJ_LINEAR_CENTROID(ij[IJ_LINEAR_CENTROID]));
    reg(REG_A6XX_SP_REG_PROG_ID_2,A6XX_SP_REG_PROG_ID_2_IJ_PERSP_SAMPLE(ij[IJ_PERSP_SAMPLE])|A6XX_SP_REG_PROG_ID_2_IJ_LINEAR_SAMPLE(ij[IJ_LINEAR_SAMPLE])|A6XX_SP_REG_PROG_ID_2_XYCOORDREGID(coord)|A6XX_SP_REG_PROG_ID_2_ZWCOORDREGID(zw));
    reg(REG_A6XX_SP_REG_PROG_ID_3,A6XX_SP_REG_PROG_ID_3_LINELENGTHREGID(INVALID_REG)|A6XX_SP_REG_PROG_ID_3_FOVEATIONQUALITYREGID(INVALID_REG));
    reg(REG_A6XX_SP_PS_WAVE_CNTL,A6XX_SP_PS_WAVE_CNTL_THREADSIZE(THREAD64)|(fs->total_in?A6XX_SP_PS_WAVE_CNTL_VARYINGS:0));
    const bool size=!info->props.has_implicit_fragface_fragcoord_ij_linear&&(fs->frag_face||fs->fragcoord_compmask);
    uint32_t cl=0,rb=0;
#define IJ(NAME,IDX) do {if(valid(ij[IDX])){cl|=A6XX_GRAS_CL_INTERP_CNTL_##NAME;rb|=A6XX_RB_INTERP_CNTL_##NAME;}}while(0)
    IJ(IJ_PERSP_PIXEL,IJ_PERSP_PIXEL);IJ(IJ_PERSP_CENTROID,IJ_PERSP_CENTROID);IJ(IJ_PERSP_SAMPLE,IJ_PERSP_SAMPLE);IJ(IJ_LINEAR_CENTROID,IJ_LINEAR_CENTROID);IJ(IJ_LINEAR_SAMPLE,IJ_LINEAR_SAMPLE);
    if(valid(ij[IJ_LINEAR_PIXEL])||size){cl|=A6XX_GRAS_CL_INTERP_CNTL_IJ_LINEAR_PIXEL;rb|=A6XX_RB_INTERP_CNTL_IJ_LINEAR_PIXEL;}
#undef IJ
    cl|=(fs->frag_face?A6XX_GRAS_CL_INTERP_CNTL_FACENESS:0);
    cl|=A6XX_GRAS_CL_INTERP_CNTL_COORD_MASK(fs->fragcoord_compmask);rb|=A6XX_RB_INTERP_CNTL_COORD_MASK(fs->fragcoord_compmask);
    reg(REG_A6XX_GRAS_CL_INTERP_CNTL,cl);reg(REG_A6XX_RB_INTERP_CNTL,rb|(fs->total_in?A6XX_RB_INTERP_CNTL_INTERP_EN:0));
    reg(REG_A6XX_RB_PS_INPUT_CNTL,A6XX_RB_PS_INPUT_CNTL_FRAGCOORDSAMPLEMODE(FRAGCOORD_CENTER)|(fs->frag_face?A6XX_RB_PS_INPUT_CNTL_FACENESS:0));
    reg(REG_A6XX_RB_PS_SAMPLEFREQ_CNTL);reg(REG_A6XX_GRAS_LRZ_PS_INPUT_CNTL,A6XX_GRAS_LRZ_PS_INPUT_CNTL_FRAGCOORDSAMPLEMODE(FRAGCOORD_CENTER));reg(REG_A6XX_GRAS_LRZ_PS_SAMPLEFREQ_CNTL);
    reg(REG_A6XX_SP_PS_OUTPUT_CNTL,A6XX_SP_PS_OUTPUT_CNTL_DEPTH_REGID(depth_regid)|A6XX_SP_PS_OUTPUT_CNTL_SAMPMASK_REGID(INVALID_REG)|A6XX_SP_PS_OUTPUT_CNTL_STENCILREF_REGID(INVALID_REG));
    reg(REG_A6XX_SP_PS_OUTPUT_REG(0),A6XX_SP_PS_OUTPUT_REG_REGID(ir3_find_output_regid(fs,FRAG_RESULT_DATA0)));
    reg(REG_A6XX_RB_PS_OUTPUT_CNTL,writes_depth?A6XX_RB_PS_OUTPUT_CNTL_FRAG_WRITES_Z:0);reg(REG_A6XX_RB_PS_MRT_CNTL,A6XX_RB_PS_MRT_CNTL_MRT(color_outputs));reg(REG_A6XX_SP_PS_MRT_CNTL,A6XX_SP_PS_MRT_CNTL_MRT(color_outputs));
    reg(REG_A6XX_SP_PS_OUTPUT_MASK,color_outputs?15:0);reg(REG_A6XX_RB_PS_OUTPUT_MASK,color_outputs?15:0);
    for(unsigned i=0;i<vs->inputs_count;i++)if(!vs->inputs[i].sysval)
        inputs.push_back({uint32_t(vs->inputs[i].slot-VERT_ATTRIB_GENERIC0),vs->inputs[i].regid,vs->inputs[i].compmask});
    if(words.size()>4096||patches.size()>128||inputs.size()>32)return 9;
    char path[4096];snprintf(path,sizeof(path),"%s.pipeline.h",prefix);FILE *f=fopen(path,"w");if(!f)return 5;
    fprintf(f,"/* Generated shader-dependent A650 recipe; derived game bytes stay private. */\n#pragma once\n#include \"nvn_native/pipeline_recipe.h\"\n");
    for(unsigned stage=0;stage<2;stage++){
        const auto *v=stage?fs:vs;fprintf(f,"static const uint32_t nn_pipeline_code_%u[]={",stage);
        for(unsigned i=0;i<v->info.size/4;i++)fprintf(f,"0x%08xu,",v->bin[i]);
        fprintf(f,"};\n");
    }
    fprintf(f,"static const uint32_t nn_pipeline_words[]={");
    for(auto w:words)fprintf(f,"0x%08xu,",w);
    fprintf(f,"};\nstatic const struct nn_pipeline_address_patch nn_pipeline_patches[]={");
    for(auto &p:patches)fprintf(f,"{%u,%u,%u,%u,%u,%u},",p.word,p.role,p.stage,p.binding,p.offset,p.bytes);
    fprintf(f,"};\nstatic const struct nn_pipeline_vertex_input nn_pipeline_inputs[]={");
    for(auto &in:inputs)fprintf(f,"{%u,%u,%u},",in.location,in.regid,in.compmask);
    fprintf(f,"};\nstatic const struct nn_pipeline_shader_recipe nn_pipeline_recipe={%u,0x06050002,{%u,%u},{nn_pipeline_code_0,nn_pipeline_code_1},nn_pipeline_words,%zu,nn_pipeline_patches,%zu,nn_pipeline_inputs,%zu,%u,%u,%u,%u,%u,%u,%u};\n",
        writes_depth?NN_PIPELINE_DEPTH_RECIPE_VERSION:NN_PIPELINE_RECIPE_VERSION,vs->info.size,fs->info.size,words.size(),patches.size(),inputs.size(),
        ir3_find_sysval_regid(vs,SYSTEM_VALUE_VERTEX_ID),ir3_find_sysval_regid(vs,SYSTEM_VALUE_INSTANCE_ID),texture_mask,sampler_mask,fs->num_samp,fs->fragcoord_compmask,fs->has_kill);
    if(runtime_constants&&(ir3_const_state(vs)->num_driver_params||ir3_const_state(fs)->num_driver_params||ir3_const_state(vs)->num_ubos||ir3_const_state(fs)->num_ubos)) {
        fprintf(f,"static const struct nn_pipeline_runtime_layout nn_pipeline_runtime={.version=1,.recipe=&nn_pipeline_recipe");
        const char *names[]={"driver_dwords","driver_mask","driver_offset_vec4","driver_upload_vec4","ubo_mask","ubo_entries"};
        for(unsigned field=0;field<6;field++) {
            fprintf(f,",.%s={",names[field]);
            for(unsigned stage=0;stage<2;stage++) {
                const auto *c=ir3_const_state(stage?fs:vs);const auto &a=c->allocs.consts[IR3_CONST_ALLOC_DRIVER_PARAMS];
                uint32_t values[]={c->num_driver_params,c->nvn_driver_word_mask,a.offset_vec4,a.size_vec4,c->nvn_remaining_ubo_mask,c->num_ubos};
                fprintf(f,"%s%u",stage?",":"",values[field]);
            }
            fprintf(f,"}");
        }
        fprintf(f,"};\n");
    }
    if(writes_depth)fprintf(f,"static const struct nn_pipeline_depth_export nn_pipeline_depth={1,&nn_pipeline_recipe,1,%u,%u,1,1};\n",depth_regid,color_outputs);
    if(fclose(f))return 5;
    snprintf(path,sizeof(path),"%s.pipeline.json",prefix);f=fopen(path,"w");if(!f)return 5;
    fprintf(f,"{\"schema\":1,\"chip_id\":100990978,\"scope\":\"shader-dependent state only; no target/VFD/viewport/render/draw or GPU execution\",\"word_count\":%zu,\"address_patch_count\":%zu,\"vertex_inputs\":%zu,\"texture_mask\":%u,\"semantic_sampler_mask\":%u,\"table_entries\":%u,\"fragcoord_mask\":%u,\"has_kill\":%s,\"stages\":[",words.size(),patches.size(),inputs.size(),texture_mask,sampler_mask,fs->num_samp,fs->fragcoord_compmask,fs->has_kill?"true":"false");
    for(unsigned stage=0;stage<2;stage++){
        const auto *v=stage?fs:vs;const auto *c=ir3_const_state(v);
        fprintf(f,"%s{\"code_bytes\":%u,\"immediate_destination_vec4\":%u,\"immediate_values\":[",stage?",":"",v->info.size,c->allocs.max_const_offset_vec4);
        for(unsigned i=0;i<align(v->imm_state.count,4);i++)fprintf(f,"%s%u",i?",":"",v->imm_state.values[i]);
        fprintf(f,"]}");
    }
    fprintf(f,"]");
    if(writes_depth)fprintf(f,",\"depth_export\":{\"schema\":1,\"recipe_version\":2,\"writes_depth\":true,\"regid\":%u,\"color_outputs\":%u,\"late_z_required\":true,\"lrz_disabled_required\":true}",depth_regid,color_outputs);
    if(fs->frag_face)fprintf(f,",\"front_facing\":{\"schema\":1,\"half_scalar\":true,\"merged_registers\":true,\"max_full_register\":%d,\"max_separate_half_register\":%d,\"regid\":%u,\"live\":%s,\"need_size\":%s}",fs->info.max_reg,fs->info.max_half_reg,face,valid(face)?"true":"false",size?"true":"false");
    fprintf(f,"}\n");if(fclose(f))return 5;
    snprintf(path,sizeof(path),"%s.pipeline.runtime.json",prefix);f=fopen(path,"w");if(!f)return 5;
    fprintf(f,"{\"schema\":1,\"basis\":\"actual common IR3 const-state allocation and post-push NIR UBO lowering\",\"enabled\":%s,\"stages\":[",runtime_constants?"true":"false");
    for(unsigned stage=0;stage<2;stage++) {
        const auto *c=ir3_const_state(stage?fs:vs);const auto &a=c->allocs.consts[IR3_CONST_ALLOC_DRIVER_PARAMS];
        fprintf(f,"%s{\"driver_dwords\":%u,\"driver_mask\":%u,\"driver_offset_vec4\":%u,\"driver_upload_vec4\":%u,\"ubo_mask\":%u,\"ubo_entries\":%u,\"driver_unknown\":%s,\"ubo_unknown\":%s}",stage?",":"",c->num_driver_params,c->nvn_driver_word_mask,a.offset_vec4,a.size_vec4,c->nvn_remaining_ubo_mask,c->num_ubos,c->nvn_driver_unknown?"true":"false",c->nvn_remaining_ubo_unknown?"true":"false");
    }
    fprintf(f,"]}\n");return fclose(f)?5:0;
}
