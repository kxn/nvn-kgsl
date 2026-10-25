/* SPDX-License-Identifier: MIT. Actual Mesa FDL and typed NIR facts only. */
#include "nvn_native/pipeline_storage.h"
#include "nvn_native/pm4.h"
#include "nvn_native/pipeline_archive.h"
#include <errno.h>
#include <string.h>
static unsigned high(uint32_t v){unsigned n=0;while(v){n++;v>>=1;}return n;}
int nn_pipeline_storage_types_validate(const struct nn_pipeline_storage_types *t,
 const struct nn_pipeline_shader_recipe *r,const struct nn_offline_graphics_stage *const stages[2],uint32_t *entries)
{
 if(!entries)return EINVAL;
 *entries=0;if(!r||!r->words||!r->patches||r->word_count>4096||r->patch_count>128)return EINVAL;
 unsigned patches=0;for(size_t i=0;i<r->patch_count;i++)patches+=r->patches[i].role==NN_PIPELINE_STORAGE;
 if(!t)return patches?ENOTSUP:0;
 if(t->version!=1||t->recipe!=r||!stages||(t->mask[0]|t->mask[1])>65535u||!(t->mask[0]|t->mask[1]))return EINVAL;
 unsigned n=t->mask[1]?16+high(t->mask[1]):high(t->mask[0]);
 for(unsigned s=0;s<2;s++){
  const struct nn_offline_graphics_stage *v=stages[s];
  if(!v||v->stage!=s||!v->spirv||v->spirv_bytes<20||v->spirv_bytes>1048576||v->spirv_bytes%4||
     !t->spirv[s]||t->spirv_bytes[s]!=v->spirv_bytes||memcmp(t->spirv[s],v->spirv,v->spirv_bytes))return EINVAL;
  for(unsigned b=0;b<16;b++)if((t->operations[s][b]&~7u)||!!t->operations[s][b]!=!!(t->mask[s]&(1u<<b)))return EINVAL;
 }
 if(patches!=2)return EINVAL;
 unsigned reg=0,load=0;
 for(size_t i=0;i<r->patch_count;i++){
  const struct nn_pipeline_address_patch *p=r->patches+i;if(p->role!=NN_PIPELINE_STORAGE)continue;
  if(p->stage||p->binding||p->offset||p->bytes!=n*64u||p->word>=r->word_count||r->word_count-p->word<2||r->words[p->word]||r->words[p->word+1])return EINVAL;
  if(p->word&&r->words[p->word-1]==nvn_pm4_packet4(0xab1a,2))reg++;
  else if(p->word>=2&&r->words[p->word-2]==nvn_pm4_packet7(0x36,3)&&r->words[p->word-1]==((2u<<16)|(14u<<18)|(n<<22)))load++;
  else return EINVAL;
 }
 if(reg!=1||load!=1)return EINVAL;
 *entries=n;return 0;
}
int nn_pipeline_storage_descriptor(struct nn_pipeline_buffer b,uint32_t out[16])
{
 if(!out)return EINVAL;
 memset(out,0,64);
 if(!b.bo||!b.bytes||b.bytes>UINT32_MAX||b.bytes%4||b.offset%4||b.offset>nn_bo_size(b.bo)||b.bytes>nn_bo_size(b.bo)-b.offset)return EINVAL;
 uint64_t a=nn_bo_address(b.bo);if(!a||b.offset>UINT64_MAX-a||b.bytes>UINT64_MAX-a-b.offset)return EOVERFLOW;
 struct nn_fdl_buffer_desc d={1,0x06050002,11,(uint32_t)b.bytes,a+b.offset};struct nn_fdl_buffer_view v;
 int e=nn_fdl_buffer(&d,&v);if(e)return e;
 if(v.base_address<a||v.base_address-a>nn_bo_size(b.bo)||v.required_span>nn_bo_size(b.bo)-(size_t)(v.base_address-a))return EINVAL;
 memcpy(out,v.descriptor,64);return 0;
}

int nn_pipeline_storage_source_selector(unsigned stage,uint32_t cb,uint32_t word,unsigned *out)
{
 if(!out)return EINVAL;
 *out=0;if(stage>1||cb)return ENOTSUP;
 uint32_t base=stage?0x510u/4u:0x110u/4u;
 if(word<base||(word-base)%4||(word-base)/4>=16)return ENOTSUP;
 *out=(word-base)/4;return 0;
}
int nn_pipeline_storage_archive_validate(const struct nn_pipeline_storage_archive*a)
{
 if(!a||a->version!=1||!a->typed||!a->pairs)return EINVAL;
 int e=nn_pipeline_typed_archive_validate(a->typed);if(e)return e;
 const struct nn_pipeline_archive*archive=a->typed->archive;
 if(!a->pair_count||a->pair_count!=archive->pair_count)return EINVAL;
 for(size_t i=0;i<a->pair_count;i++){
  const struct nn_pipeline_pair_recipe *p=archive->pairs+i;
  const struct nn_offline_graphics_stage *stages[2]={archive->stages->stages+p->vs_index,archive->stages->stages+p->fs_index};
  uint32_t entries=0;if((e=nn_pipeline_storage_types_validate(a->pairs[i],p->recipe,stages,&entries)))return e;
  for(unsigned stage=0;stage<2;stage++){
   const struct nn_offline_graphics_stage*s=stages[stage];
   if(s->storage_count>16||(s->storage_count&&!s->storage))return EINVAL;
   uint32_t seen=0;
   for(size_t j=0;j<s->storage_count;j++){
    const struct nn_offline_storage_binding*b=s->storage+j;unsigned slot=0;
    if(b->slot>=16||b->binding>=16||(b->flags&~1u)||(seen&(1u<<b->binding)))return EINVAL;
    if((e=nn_pipeline_storage_source_selector(stage,b->cbuf_slot,b->cbuf_word,&slot)))return e;
    seen|=1u<<b->binding;
    if(a->pairs[i]&&(a->pairs[i]->operations[stage][b->binding]&2)&&!(b->flags&1))return EINVAL;
   }
   if(a->pairs[i]&&(a->pairs[i]->mask[stage]&~seen))return EINVAL;
   if(!a->pairs[i]&&s->storage_count)return ENOTSUP;
  }
 }
 return 0;
}
int nn_pipeline_select_storage_types(const struct nn_pipeline_storage_archive*a,
 const struct nn_offline_graphics_stage*vs,const struct nn_offline_graphics_stage*fs,
 const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_storage_types**out)
{
 if(!out)return EINVAL;
 *out=NULL;int e=nn_pipeline_storage_archive_validate(a);if(e)return e;
 const struct nn_pipeline_shader_recipe*selected=NULL;
 if((e=nn_pipeline_select_pair(a->typed->archive,vs,fs,&selected)))return e;
 if(selected!=r)return EINVAL;
 for(size_t i=0;i<a->pair_count;i++)if(a->typed->archive->pairs[i].recipe==r){*out=a->pairs[i];return 0;}
 return ENOTSUP;
}
