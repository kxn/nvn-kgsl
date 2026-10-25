/* SPDX-License-Identifier: MIT. A650 shader recipe validation and relocation. */
#include "nvn_native/pipeline_shader.h"
#include "nvn_native/pm4.h"
#include "nvn_native/depth_native.h"
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/pipeline_storage.h"
#include "nvn_native/pipeline_runtime_constants.h"
#ifndef NN_PIPELINE_RUNTIME_CONSTANTS
#define NN_PIPELINE_RUNTIME_CONSTANTS 0
#endif
#ifndef NN_PIPELINE_GRAPHICS_STORAGE
#define NN_PIPELINE_GRAPHICS_STORAGE 0
#endif
#ifndef NN_PIPELINE_DEPTH_NATIVE
#define NN_PIPELINE_DEPTH_NATIVE 0
#endif
#include <errno.h>
#include <string.h>
#include <limits.h>
/* Qualcomm adreno_pm4.xml/a6xx.xml, pinned Mesa26.2.5. Producer uses generated
 * packers; HOST fixture cross-checks these fields against real produced words. */
enum {LOAD_GEOM=50,LOAD_FRAG=52,VS_BASE=0xa81c,FS_BASE=0xa983,
    SAMPLER_BASE=0xa9e0,TEXTURE_BASE=0xa9e4,STORAGE_BASE=0xab1a,LOAD_SHARED=0x36};
struct resolved {uint64_t address[128];nn_bo *bos[64];size_t count;};
static int add(struct resolved *r,nn_bo *bo)
{for(size_t i=0;i<r->count;i++)if(r->bos[i]==bo)return 0;if(r->count==64)return EOVERFLOW;r->bos[r->count++]=bo;return 0;}
static int overlap(size_t a,size_t n,size_t b,size_t m)
{return a<b+m&&b<a+n;}
static int view_address(struct nn_pipeline_buffer b,size_t offset,size_t bytes,uint64_t *out)
{
    if(!b.bo||b.offset>nn_bo_size(b.bo)||b.bytes>nn_bo_size(b.bo)-b.offset||
       offset>b.bytes||bytes>b.bytes-offset)return EINVAL;
    uint64_t address=nn_bo_address(b.bo);
    if(!address||b.offset>UINT64_MAX-address||offset>UINT64_MAX-address-b.offset||
       bytes>UINT64_MAX-address-b.offset-offset)return EOVERFLOW;
    address+=b.offset+offset;if(address%64)return EINVAL;*out=address;return 0;
}
static int recipe_validate(const struct nn_pipeline_shader_recipe *r,const struct nn_pipeline_depth_export *depth)
{
    if(depth){
#if NN_PIPELINE_DEPTH_NATIVE
        int e=nn_depth_native_recipe(depth);if(e)return e;
        if(depth->recipe!=r)return EINVAL;
#else
        return ENOTSUP;
#endif
    }
    if(!r||r->version!=(depth&&depth->writes_depth?NN_PIPELINE_DEPTH_RECIPE_VERSION:NN_PIPELINE_RECIPE_VERSION)||r->chip!=0x06050002||
       !r->words||!r->word_count||r->word_count>4096||!r->patches||r->patch_count>128||
       r->patch_count<4||r->input_count>32||(r->input_count&&!r->inputs)||r->texture_table_entries>16||
       r->fragcoord_mask>15||r->has_kill>1)return EINVAL;
    uint32_t validmask=(1u<<r->texture_table_entries)-1u;
    if((r->texture_mask|r->sampler_mask)&~validmask)return EINVAL;
    uint32_t locations=0;
    for(size_t i=0;i<r->input_count;i++) {
        const struct nn_pipeline_vertex_input *in=r->inputs+i;
        if(in->location>=32||in->regid>=252||!in->compmask||in->compmask>15||
           (locations&(1u<<in->location)))return EINVAL;
        locations|=1u<<in->location;
    }
    if((r->vertex_id_regid!=252&&r->vertex_id_regid>=252)||
       (r->instance_id_regid!=252&&r->instance_id_regid>=252))return EINVAL;
    unsigned char found[128]={0};unsigned code_count[2]={0},table_count[2]={0},storage_count=0,storage_bytes=0;
    for(size_t at=0;at<r->word_count;) {
        uint32_t header=r->words[at],length=nvn_pm4_packet_words(header);
        if(!length||length>r->word_count-at)return EINVAL;
        const int is_reg=(header>>28)==4;unsigned opcode=(header>>16)&127;
        if(!is_reg&&opcode!=LOAD_GEOM&&opcode!=LOAD_FRAG&&opcode!=LOAD_SHARED)return ENOTSUP;
        if(!is_reg&&opcode==LOAD_SHARED){
#if NN_PIPELINE_GRAPHICS_STORAGE
            if(length!=4||((r->words[at+1]>>14)&255u)!=(2u<<2|14u<<4)||
               (r->words[at+1]&0x3fffu)||!(r->words[at+1]>>22)||r->words[at+1]>>22>32)return EINVAL;
            unsigned matches=0;for(size_t j=0;j<r->patch_count;j++)matches+=r->patches[j].role==NN_PIPELINE_STORAGE&&r->patches[j].word==at+2;
            if(matches!=1)return EINVAL;
#else
            return ENOTSUP;
#endif
        }
        for(size_t j=0;j<r->patch_count;j++) {
            const struct nn_pipeline_address_patch *p=r->patches+j;
            if(p->word<at||p->word>=at+length)continue;
            if(found[j]||p->word+1>=at+length||p->stage>1||p->binding>=16||!p->bytes||
               r->words[p->word]||r->words[p->word+1])return EINVAL;
            found[j]=1;
            for(size_t k=0;k<j;k++)if(p->word<r->patches[k].word+2&&r->patches[k].word<p->word+2)return EINVAL;
            if(is_reg) {
                uint32_t reg=(header>>8)&0x7ffffu;
                uint32_t expected=p->role==NN_PIPELINE_CODE?(p->stage?FS_BASE:VS_BASE):
                    p->role==NN_PIPELINE_SAMPLERS?SAMPLER_BASE:p->role==NN_PIPELINE_TEXTURES?TEXTURE_BASE:p->role==NN_PIPELINE_STORAGE?STORAGE_BASE:0;
                if(length!=3||p->word!=at+1||!expected||reg!=expected)return EINVAL;
            } else {
                uint32_t control=r->words[at+1],type=(control>>14)&3,src=(control>>16)&3,
                    block=(control>>18)&15,units=control>>22;
                if(p->role==NN_PIPELINE_DYNAMIC_UBO) {
#if NN_PIPELINE_RUNTIME_CONSTANTS
                    if(type!=2||src||!units||units>16||length!=4+2*units||p->binding>=units||p->word!=at+4+2*p->binding||
                       block!=(p->stage?12u:8u)||opcode!=(p->stage?LOAD_FRAG:LOAD_GEOM))return EINVAL;
#else
                    return ENOTSUP;
#endif
                }else if(length!=4||p->word!=at+2||src!=2||!units||opcode!=(p->role==NN_PIPELINE_STORAGE?LOAD_SHARED:p->stage?LOAD_FRAG:LOAD_GEOM))return EINVAL;
                if(p->role==NN_PIPELINE_DYNAMIC_UBO) {
                    if(p->offset||p->bytes!=16)return EINVAL;
                }else if(p->role==NN_PIPELINE_CODE) {
                    if(type||block!=(p->stage?12u:8u)||units>(p->bytes+127u)/128u)return EINVAL;
                } else if(p->role==NN_PIPELINE_UBO||p->role==NN_PIPELINE_DRIVER_PARAMS) {
                    if(type!=1||block!=(p->stage?12u:8u)||p->bytes!=units*16u)return EINVAL;
                } else if(p->role==NN_PIPELINE_SAMPLERS||p->role==NN_PIPELINE_TEXTURES) {
                    if(block!=4||type!=(p->role==NN_PIPELINE_SAMPLERS?0u:1u)||units!=r->texture_table_entries)return EINVAL;
                } else if(p->role==NN_PIPELINE_STORAGE){if(type||block!=14||units>32||p->bytes!=units*64u)return EINVAL;}
                else return EINVAL;
            }
            if(p->role==NN_PIPELINE_CODE) {
                if(p->binding||p->offset||p->bytes!=r->code_bytes[p->stage])return EINVAL;
                code_count[p->stage]++;
            } else if(p->role==NN_PIPELINE_DRIVER_PARAMS||p->role==NN_PIPELINE_DYNAMIC_UBO) {
#if NN_PIPELINE_RUNTIME_CONSTANTS
                if(p->role==NN_PIPELINE_DRIVER_PARAMS&&(p->stage||p->binding||p->offset||p->bytes!=64))return EINVAL;
                if(p->role==NN_PIPELINE_DYNAMIC_UBO&&(p->offset||p->bytes!=16))return EINVAL;
#else
                return ENOTSUP;
#endif
            } else if(p->role==NN_PIPELINE_UBO) {
                if(p->offset%64||p->bytes%64)return EINVAL;
            } else if(p->role==NN_PIPELINE_SAMPLERS||p->role==NN_PIPELINE_TEXTURES) {
                size_t bytes=(size_t)r->texture_table_entries*(p->role==NN_PIPELINE_SAMPLERS?16u:64u);
                if(!r->texture_table_entries||p->stage!=1||p->binding||p->offset||p->bytes!=bytes)return EINVAL;
                table_count[p->role==NN_PIPELINE_TEXTURES]++;
            } else if(p->role==NN_PIPELINE_STORAGE) {
#if NN_PIPELINE_GRAPHICS_STORAGE
                if(p->stage||p->binding||p->offset||p->bytes%64||p->bytes>32*64||!p->bytes||
                   (storage_count&&storage_bytes!=p->bytes))return EINVAL;
                storage_bytes=p->bytes;storage_count++;
#else
                return ENOTSUP;
#endif
            } else return EINVAL;
        }
        /* No unaccounted indirect LOAD_STATE address may survive. */
        if(!is_reg&&((r->words[at+1]>>16)&3)==2) {
            int has=0;for(size_t j=0;j<r->patch_count;j++)has|=r->patches[j].word==at+2;
            if(!has)return EINVAL;
        }
        at+=length;
    }
    for(size_t i=0;i<r->patch_count;i++)if(!found[i])return EINVAL;
    if(code_count[0]!=2||code_count[1]!=2||table_count[0]!=(r->texture_table_entries?2u:0u)||
       table_count[1]!=(r->texture_table_entries?2u:0u)||(storage_count&&storage_count!=2))return EINVAL;
    (void)storage_bytes;
    return 0;
}
static int shader_preflight(const struct nn_offline_graphics_stage *const stages[2],
 const struct nn_pipeline_shader_recipe *r,const struct nn_pipeline_depth_export *depth,const struct nn_pipeline_runtime_constants *runtime,const struct nn_pipeline_buffer ubo[2][16])
{
 int e=recipe_validate(r,depth);if(e)return e;
 if(!stages||!ubo)return EINVAL;
#if NN_PIPELINE_RUNTIME_CONSTANTS
 if((e=nn_pipeline_runtime_constants_validate(runtime,r,stages)))return e;
#else
 if(runtime)return ENOTSUP;
#endif
 size_t c1[2]={0};uint32_t c1_binding[2]={UINT32_MAX,UINT32_MAX};
 for(unsigned i=0;i<2;i++) {
  const struct nn_offline_graphics_stage *s=stages[i];
  if(!s||s->stage!=i||!s->source||!s->code_bytes||s->source_bytes<s->code_bytes||
     s->source_bytes>1048576||s->cbuf_count>16||(s->cbuf_count&&!s->cbufs)||
     !r->code[i]||!r->code_bytes[i]||r->code_bytes[i]>65536)return EINVAL;
  c1[i]=s->source_bytes-s->code_bytes;
  if(c1[i])for(size_t j=0;j<s->cbuf_count;j++)if(s->cbufs[j].slot==1) {
   if(s->cbufs[j].binding>=16||c1_binding[i]!=UINT32_MAX)return EINVAL;
   c1_binding[i]=s->cbufs[j].binding;
  }
 }
 for(size_t j=0;j<r->patch_count;j++) {
  const struct nn_pipeline_address_patch *p=r->patches+j;
  if(p->role!=NN_PIPELINE_UBO&&p->role!=NN_PIPELINE_DYNAMIC_UBO)continue;
  struct nn_pipeline_buffer v=ubo[p->stage][p->binding];
  if(c1[p->stage]&&p->binding==c1_binding[p->stage]) {
   if(v.bo||v.offset||v.bytes||p->offset>c1[p->stage]||p->bytes>c1[p->stage]-p->offset||
      (p->role==NN_PIPELINE_DYNAMIC_UBO&&(c1[p->stage]>65536||c1[p->stage]%16)))return EINVAL;
  }else if(p->role==NN_PIPELINE_DYNAMIC_UBO){
#if NN_PIPELINE_RUNTIME_CONSTANTS
   uint32_t descriptor[2];if((e=nn_pipeline_runtime_ubo_descriptor(v,descriptor)))return e;
#else
   return ENOTSUP;
#endif
  }else{uint64_t address;if((e=view_address(v,p->offset,p->bytes,&address)))return e;}
 }
 return 0;
}
static int resolve(const nn_pipeline_program *program,const struct nn_pipeline_shader_recipe *r,
                   const struct nn_pipeline_shader_bindings *b,const struct nn_pipeline_depth_export *depth,struct resolved *out)
{
    memset(out,0,sizeof(*out));
    if(!program||!b)return EINVAL;
#if NN_PIPELINE_DEPTH_NATIVE
    if(nn_pipeline_program_depth_export(program)!=depth)return ENOTSUP;
#else
    if(depth)return ENOTSUP;
#endif
    int validation=recipe_validate(r,depth);if(validation)return validation;
    struct nn_pipeline_stage_view stage[2];
    for(unsigned s=0;s<2;s++) {
        int e=nn_pipeline_program_stage(program,s,stage+s);if(e)return e;
        const struct nn_offline_graphics_pair *pair=stage[s].pair;
        const unsigned char *expected=s?pair->fs_code:pair->vs_code;
        if(!r->code[s]||r->code_bytes[s]!=stage[s].code_bytes||
           memcmp(r->code[s],expected,r->code_bytes[s]))return ENOTSUP;
    }
    size_t commands=r->word_count*4,samplers=r->texture_table_entries*16,textures=r->texture_table_entries*64;
    uint32_t storage_entries=0;
#if NN_PIPELINE_GRAPHICS_STORAGE
    const struct nn_offline_graphics_stage *sources[2]={stage[0].source,stage[1].source};
    int se=nn_pipeline_storage_types_validate(nn_pipeline_program_storage_types(program),r,sources,&storage_entries);if(se)return se;
#else
    if(nn_pipeline_program_storage_types(program))return ENOTSUP;
#endif
    size_t storage_bytes=(size_t)storage_entries*64;
    size_t driver_bytes=0;
#if NN_PIPELINE_RUNTIME_CONSTANTS
    const struct nn_pipeline_runtime_constants *runtime=nn_pipeline_program_runtime_constants(program);
    const struct nn_offline_graphics_stage *runtime_sources[2]={stage[0].source,stage[1].source};
    int re=nn_pipeline_runtime_constants_validate(runtime,r,runtime_sources);if(re)return re;
    if(runtime&&runtime->layout->driver_dwords[0])driver_bytes=64;
#else
    if(nn_pipeline_program_runtime_constants(program))return ENOTSUP;
#endif


    if(!b->job||!nn_bo_map(b->job)||b->command_offset%4||b->command_offset>nn_bo_size(b->job)||
       commands>nn_bo_size(b->job)-b->command_offset)return EINVAL;
    if(r->texture_table_entries && (b->sampler_offset%64||b->texture_offset%64||
       b->sampler_offset>nn_bo_size(b->job)||samplers>nn_bo_size(b->job)-b->sampler_offset||
       b->texture_offset>nn_bo_size(b->job)||textures>nn_bo_size(b->job)-b->texture_offset||
       overlap(b->command_offset,commands,b->sampler_offset,samplers)||
       overlap(b->command_offset,commands,b->texture_offset,textures)||
       overlap(b->sampler_offset,samplers,b->texture_offset,textures)))return EINVAL;
    if(storage_bytes&&(b->storage_offset%64||b->storage_offset>nn_bo_size(b->job)||storage_bytes>nn_bo_size(b->job)-b->storage_offset||
       overlap(b->command_offset,commands,b->storage_offset,storage_bytes)||
       (samplers&&overlap(b->sampler_offset,samplers,b->storage_offset,storage_bytes))||
       (textures&&overlap(b->texture_offset,textures,b->storage_offset,storage_bytes))))return EINVAL;
    if(driver_bytes&&(b->driver_params_offset%64||b->driver_params_offset>nn_bo_size(b->job)||driver_bytes>nn_bo_size(b->job)-b->driver_params_offset||
       overlap(b->command_offset,commands,b->driver_params_offset,driver_bytes)||
       (samplers&&overlap(b->sampler_offset,samplers,b->driver_params_offset,driver_bytes))||
       (textures&&overlap(b->texture_offset,textures,b->driver_params_offset,driver_bytes))||
       (storage_bytes&&overlap(b->storage_offset,storage_bytes,b->driver_params_offset,driver_bytes))))return EINVAL;
    for(size_t j=0;j<r->patch_count;j++) {
        const struct nn_pipeline_address_patch *p=r->patches+j;
        struct nn_pipeline_buffer view={0};size_t offset=p->offset;
        if(p->role==NN_PIPELINE_CODE) {
            if(p->bytes!=stage[p->stage].code_bytes)return EINVAL;
            view=(struct nn_pipeline_buffer){stage[p->stage].code,0,stage[p->stage].code_bytes};
        }else if(p->role==NN_PIPELINE_UBO||p->role==NN_PIPELINE_DYNAMIC_UBO) {
            view=b->ubo[p->stage][p->binding];if(view.bo==b->job)return EINVAL;
            if(stage[p->stage].c1&&p->binding==stage[p->stage].c1_native_binding) {
                if(view.bo&&(view.bo!=stage[p->stage].c1||view.offset||view.bytes!=stage[p->stage].c1_bytes))return EINVAL;
                view=(struct nn_pipeline_buffer){stage[p->stage].c1,0,stage[p->stage].c1_bytes};
            }
        }else if(p->role==NN_PIPELINE_DRIVER_PARAMS){
            view=(struct nn_pipeline_buffer){b->job,b->driver_params_offset,driver_bytes};
        }else if(p->role==NN_PIPELINE_STORAGE){
            view=(struct nn_pipeline_buffer){b->job,b->storage_offset,storage_bytes};
        }else {
            size_t bytes=p->role==NN_PIPELINE_SAMPLERS?samplers:textures;
            view=(struct nn_pipeline_buffer){b->job,p->role==NN_PIPELINE_SAMPLERS?b->sampler_offset:b->texture_offset,bytes};
        }
        int e=0;
        if(p->role==NN_PIPELINE_DYNAMIC_UBO){
#if NN_PIPELINE_RUNTIME_CONSTANTS
            uint32_t descriptor[2];if((e=nn_pipeline_runtime_ubo_descriptor(view,descriptor)))return e;
            out->address[j]=(uint64_t)descriptor[0]|(uint64_t)descriptor[1]<<32;
#else
            return ENOTSUP;
#endif
        }else if((e=view_address(view,offset,p->bytes,out->address+j)))return e;
        if((e=add(out,view.bo)))return e;
    }
    return add(out,b->job);
}

static int shader_encode(const nn_pipeline_program *p,const struct nn_pipeline_shader_recipe *r,
    const struct nn_pipeline_shader_bindings *b,const struct nn_pipeline_depth_export *depth,size_t capacity,size_t *words)
{
    if(!words)return EINVAL;
    *words=0;struct resolved values;int e=resolve(p,r,b,depth,&values);if(e)return e;
    if(capacity<r->word_count)return ENOSPC;
    uint32_t *out=(uint32_t *)((unsigned char *)nn_bo_map(b->job)+b->command_offset);
    memcpy(out,r->words,r->word_count*4);
    for(size_t i=0;i<r->patch_count;i++) {out[r->patches[i].word]=(uint32_t)values.address[i];out[r->patches[i].word+1]=(uint32_t)(values.address[i]>>32);}
    *words=r->word_count;return 0;
}
static int shader_resources(const nn_pipeline_program *p,const struct nn_pipeline_shader_recipe *r,
    const struct nn_pipeline_shader_bindings *b,const struct nn_pipeline_depth_export *depth,nn_bo **bos,size_t capacity,size_t *count)
{
    if(!count)return EINVAL;
    *count=0;struct resolved values;int e=resolve(p,r,b,depth,&values);if(e)return e;
    *count=values.count;if(capacity<values.count)return ENOSPC;if(values.count&&!bos)return EINVAL;
    memcpy(bos,values.bos,values.count*sizeof(*bos));return 0;
}

int nn_pipeline_shader_preflight(const struct nn_offline_graphics_stage *const s[2],const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_buffer u[2][16]){return shader_preflight(s,r,NULL,NULL,u);}
int nn_pipeline_shader_preflight_depth(const struct nn_offline_graphics_stage *const s[2],const struct nn_pipeline_depth_export*d,const struct nn_pipeline_buffer u[2][16]){return !d?EINVAL:shader_preflight(s,d->recipe,d,NULL,u);}
int nn_pipeline_shader_encode(const nn_pipeline_program*p,const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_shader_bindings*b,size_t n,size_t*out){return shader_encode(p,r,b,NULL,n,out);}
int nn_pipeline_shader_encode_depth(const nn_pipeline_program*p,const struct nn_pipeline_depth_export*d,const struct nn_pipeline_shader_bindings*b,size_t n,size_t*out){if(!d){if(out)*out=0;return EINVAL;}return shader_encode(p,d->recipe,b,d,n,out);}
int nn_pipeline_shader_resources(const nn_pipeline_program*p,const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_shader_bindings*b,nn_bo**bos,size_t n,size_t*out){return shader_resources(p,r,b,NULL,bos,n,out);}
int nn_pipeline_shader_resources_depth(const nn_pipeline_program*p,const struct nn_pipeline_depth_export*d,const struct nn_pipeline_shader_bindings*b,nn_bo**bos,size_t n,size_t*out){if(!d){if(out)*out=0;return EINVAL;}return shader_resources(p,d->recipe,b,d,bos,n,out);}

int nn_pipeline_shader_preflight_runtime(const struct nn_offline_graphics_stage *const s[2],const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_depth_export*d,const struct nn_pipeline_runtime_constants*t,const struct nn_pipeline_buffer u[2][16])
{return shader_preflight(s,r,d,t,u);}
