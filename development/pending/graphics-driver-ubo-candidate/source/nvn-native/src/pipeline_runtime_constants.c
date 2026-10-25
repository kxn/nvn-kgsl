/* SPDX-License-Identifier: MIT. Actual Adreno register/descriptor contracts,
 * common IR3 allocation facts; no source-ID or query-based parameter guesses. */
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/pipeline_runtime_constants.h"
#include "nvn_native/pm4.h"
#include "nvn_native/pipeline_archive.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
static unsigned high(uint32_t n){unsigned b=0;while(n){b++;n>>=1;}return b;}
int nn_pipeline_runtime_constants_validate(const struct nn_pipeline_runtime_constants *t,
 const struct nn_pipeline_shader_recipe *r,const struct nn_offline_graphics_stage *const stages[2])
{
 if(!r||!r->words||!r->patches||r->word_count>4096||r->patch_count>128)return EINVAL;
 unsigned needs=0;for(size_t i=0;i<r->patch_count;i++)needs+=r->patches[i].role==NN_PIPELINE_DRIVER_PARAMS||r->patches[i].role==NN_PIPELINE_DYNAMIC_UBO;
 if(!t)return needs?ENOTSUP:0;
 if(t->version!=1||!t->layout||t->layout->version!=1||t->layout->recipe!=r||!stages||!needs)return EINVAL;
 const struct nn_pipeline_runtime_layout *l=t->layout;
 unsigned driver_seen[2]={0},ubo_seen[2]={0};uint32_t ubo_mask[2]={0};
 for(unsigned s=0;s<2;s++){
  const struct nn_offline_graphics_stage *v=stages[s];
  if(!v||v->stage!=s||!v->spirv||v->spirv_bytes<20||v->spirv_bytes%4||v->spirv_bytes>1048576||
     !t->spirv[s]||t->spirv_bytes[s]!=v->spirv_bytes||memcmp(t->spirv[s],v->spirv,v->spirv_bytes))return EINVAL;
  if(l->driver_dwords[s]){
   if(s||l->driver_dwords[s]!=4||!l->driver_mask[s]||(l->driver_mask[s]&~7u)||
      !l->driver_offset_vec4[s]||l->driver_offset_vec4[s]%4||l->driver_offset_vec4[s]>124||l->driver_upload_vec4[s]!=4)return ENOTSUP;
  }else if(l->driver_mask[s]||l->driver_offset_vec4[s]||l->driver_upload_vec4[s])return EINVAL;
  if(l->ubo_mask[s]>65535u||l->ubo_entries[s]!=high(l->ubo_mask[s]))return EINVAL;
 }
 for(size_t at=0;at<r->word_count;){
  uint32_t n=nvn_pm4_packet_words(r->words[at]);if(!n||n>r->word_count-at)return EINVAL;
  if((r->words[at]>>28)==7){
   unsigned op=(r->words[at]>>16)&127u;
   if(op==50||op==52){
    if(n<4)return EINVAL;
    unsigned s=op==52;uint32_t ctl=r->words[at+1],type=(ctl>>14)&3,src=(ctl>>16)&3,block=(ctl>>18)&15,units=ctl>>22;
    if(type==2){
     if(!l->ubo_entries[s]||src||block!=(s?12u:8u)||(ctl&16383u)||units!=l->ubo_entries[s]||
        n!=4u+2u*units||r->words[at+2]||r->words[at+3]||ubo_seen[s]++)return EINVAL;
     for(unsigned b=0;b<units;b++){
      unsigned match=0;
      for(size_t i=0;i<r->patch_count;i++){
       const struct nn_pipeline_address_patch*p=r->patches+i;
       if(p->word!=at+4+2*b)continue;
       if(p->role!=NN_PIPELINE_DYNAMIC_UBO||p->stage!=s||p->binding!=b||p->offset||p->bytes!=16)return EINVAL;
       match++;
      }
      if(match!=!!(l->ubo_mask[s]&(1u<<b))||r->words[at+4+2*b]||r->words[at+5+2*b])return EINVAL;
      if(match)ubo_mask[s]|=1u<<b;
     }
    }
   }
  }
  for(size_t i=0;i<r->patch_count;i++){
   const struct nn_pipeline_address_patch*p=r->patches+i;
   if(p->word<at||p->word>=at+n)continue;
   if(p->role==NN_PIPELINE_DRIVER_PARAMS){
    unsigned s=p->stage;if(s>1||!l->driver_dwords[s]||p->binding||p->offset||p->bytes!=64||p->word!=at+2||n!=4||
      r->words[at]!=nvn_pm4_packet7(s?52:50,3)||r->words[at+1]!=(l->driver_offset_vec4[s]|(1u<<14)|(2u<<16)|((s?12u:8u)<<18)|(4u<<22))||
      r->words[p->word]||r->words[p->word+1]||driver_seen[s]++)return EINVAL;
   }else if(p->role==NN_PIPELINE_DYNAMIC_UBO){
    if(p->stage>1||p->binding>=16||!(ubo_mask[p->stage]&(1u<<p->binding))||
       r->words[at]!=nvn_pm4_packet7(p->stage?52:50,3+2*l->ubo_entries[p->stage])||
       p->word!=at+4+2*p->binding||((r->words[at+1]>>14)&3)!=2)return EINVAL;
   }
  }
  at+=n;
 }
 for(unsigned s=0;s<2;s++)if(driver_seen[s]!=!!l->driver_dwords[s]||ubo_seen[s]!=!!l->ubo_entries[s]||ubo_mask[s]!=l->ubo_mask[s])return EINVAL;
 return 0;
}
int nn_pipeline_runtime_draw(const struct nn_pipeline_runtime_constants *t,const struct nn_gfx_draw*d,
 uint32_t draw_id,uint32_t words[16],uint32_t *dst)
{
 if(!words||!dst)return EINVAL;
 memset(words,0,64);*dst=0;if(!d)return EINVAL;
 if(!t)return draw_id?EINVAL:0;
 if(!t->layout||t->version!=1||t->layout->version!=1)return EINVAL;
 const struct nn_pipeline_runtime_layout*l=t->layout;
 if(d->mode>NN_GFX_DRAW_INDIRECT_COUNT||d->indexed>1)return EINVAL;
 if(!l->driver_dwords[0])return draw_id?EINVAL:0;
 if(l->driver_dwords[0]!=4||!l->driver_mask[0]||(l->driver_mask[0]&~7u)||!l->driver_offset_vec4[0]||l->driver_offset_vec4[0]>124||l->driver_offset_vec4[0]%4||l->driver_upload_vec4[0]!=4)return ENOTSUP;
 if(d->mode!=NN_GFX_DRAW_DIRECT){if(draw_id)return ENOTSUP;*dst=l->driver_offset_vec4[0];return 0;}
 words[0]=draw_id;words[1]=d->indexed?(uint32_t)d->base_vertex:d->first;words[2]=d->base_instance;
 /* vtxcnt_max is inactive: this profile independently excludes streamout. */
 return 0;
}
int nn_pipeline_runtime_ubo_descriptor(struct nn_pipeline_buffer b,uint32_t out[2])
{
 if(!out)return EINVAL;
 out[0]=out[1]=0;
 if(!b.bo||!b.bytes||b.bytes>65536||b.bytes%16||b.offset%16||b.offset>nn_bo_size(b.bo)||b.bytes>nn_bo_size(b.bo)-b.offset)return EINVAL;
 uint64_t a=nn_bo_address(b.bo);if(!a||b.offset>UINT64_MAX-a||b.bytes>UINT64_MAX-a-b.offset)return EOVERFLOW;
 a+=b.offset;if(a%16||a>>49||(a+b.bytes-1)>>49)return EOVERFLOW;
 out[0]=(uint32_t)a;out[1]=(uint32_t)(a>>32)|((uint32_t)(b.bytes/16)<<17);return 0;
}

int nn_pipeline_runtime_archive_validate(const struct nn_pipeline_runtime_archive*a)
{
 if(!a||a->version!=1||!a->typed||!a->pairs)return EINVAL;
 int e=nn_pipeline_typed_archive_validate(a->typed);if(e)return e;
 const struct nn_pipeline_archive*b=a->typed->archive;
 if(!a->pair_count||a->pair_count!=b->pair_count)return EINVAL;
 for(size_t i=0;i<a->pair_count;i++){
  const struct nn_pipeline_pair_recipe*p=b->pairs+i;
  if(p->vs_index>=b->stages->stage_count||p->fs_index>=b->stages->stage_count)return EINVAL;
  const struct nn_offline_graphics_stage*s[2]={b->stages->stages+p->vs_index,b->stages->stages+p->fs_index};
  if((e=nn_pipeline_runtime_constants_validate(a->pairs[i],p->recipe,s)))return e;
  if(a->pairs[i])for(unsigned stage=0;stage<2;stage++){
   uint32_t mask=a->pairs[i]->layout->ubo_mask[stage],seen=0;
   if(s[stage]->source_bytes<s[stage]->code_bytes)return EINVAL;
   if(s[stage]->cbuf_count>16||(s[stage]->cbuf_count&&!s[stage]->cbufs))return EINVAL;
   for(size_t j=0;j<s[stage]->cbuf_count;j++){
    const struct nn_offline_cbuf*r=s[stage]->cbufs+j;
    if(r->binding>=16||(seen&(1u<<r->binding)))return EINVAL;
    seen|=1u<<r->binding;
    if(mask&(1u<<r->binding)){
     if(r->slot==1){size_t bytes=s[stage]->source_bytes-s[stage]->code_bytes;if(!bytes||bytes>65536||bytes%16)return ENOTSUP;}
     else if(r->slot<3||r->slot>16)return ENOTSUP;
    }
   }
   if(mask&~seen)return EINVAL;
  }
 }
 return 0;
}
int nn_pipeline_select_runtime_constants(const struct nn_pipeline_runtime_archive*a,
 const struct nn_offline_graphics_stage*vs,const struct nn_offline_graphics_stage*fs,
 const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_runtime_constants**out)
{
 if(!out)return EINVAL;
 *out=NULL;int e=nn_pipeline_runtime_archive_validate(a);if(e)return e;
 const struct nn_pipeline_shader_recipe*selected=NULL;
 if((e=nn_pipeline_select_pair(a->typed->archive,vs,fs,&selected)))return e;
 if(selected!=r)return EINVAL;
 for(size_t i=0;i<a->pair_count;i++)if(a->typed->archive->pairs[i].recipe==r){*out=a->pairs[i];return 0;}
 return ENOTSUP;
}
