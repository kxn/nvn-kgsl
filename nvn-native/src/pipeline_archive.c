/* SPDX-License-Identifier: MIT. No JSON parser, shader translator or default state. */
#include "nvn_native/pipeline_archive.h"
#include <errno.h>
#include <string.h>
int nn_pipeline_archive_validate(const struct nn_pipeline_archive *a)
{
 if(!a||!a->stages||!a->stages->stages||!a->stages->stage_count||a->stages->stage_count>128||
    a->requirement_count!=a->stages->stage_count||!a->requirements||a->pair_count>16384||
    (a->pair_count&&!a->pairs)||!a->platform||a->platform->version!=1||a->platform->chip!=0x06050002)return EINVAL;
 for(size_t i=0;i<a->requirement_count;i++) {
  const struct nn_pipeline_stage_requirements *r=a->requirements+i;
  if(r->stage_index!=i||!r->query_count||r->query_count>256||!r->queries)return EINVAL;
  for(size_t j=0;j<r->query_count;j++) {
   const struct nn_pipeline_query_requirement *q=r->queries+j;
   if(!q->query.name||!q->query.name[0]||q->query.arguments>15||q->query.type>NN_PIPELINE_QUERY_F32||!q->query.count||q->query.count>32)return EINVAL;
   for(unsigned k=0;k<4;k++)if(!(q->query.arguments&(1u<<k))&&q->query.args[k])return EINVAL;
   if(q->query.type==NN_PIPELINE_QUERY_BOOL)for(unsigned k=0;k<q->query.count;k++)if(q->values[k]>1)return EINVAL;
  }
 }
 return 0;
}
int nn_pipeline_select_stage(const struct nn_pipeline_archive *a,uint32_t stage,const void *source,size_t source_bytes,
 const void *control,size_t control_bytes,nn_pipeline_query_resolver resolve,void *user,const struct nn_offline_graphics_stage **out)
{
 if(!out)return EINVAL;
 *out=NULL;int e=nn_pipeline_archive_validate(a);if(e)return e;
 if(stage>1||!source||!source_bytes||source_bytes>1048576||!control||!control_bytes||control_bytes>1048576||!resolve)return EINVAL;
 const struct nn_offline_graphics_stage *found=NULL;int unknown=0;
 for(size_t i=0;i<a->stages->stage_count;i++) {
  const struct nn_offline_graphics_stage *s=a->stages->stages+i;
  if(s->stage!=stage||s->source_bytes!=source_bytes||s->control_bytes!=control_bytes||
     !s->source||!s->control||memcmp(source,s->source,source_bytes)||memcmp(control,s->control,control_bytes))continue;
  const struct nn_pipeline_stage_requirements *r=a->requirements+i;int matches=1,candidate_unknown=0;
  for(size_t j=0;j<r->query_count;j++) {
   const struct nn_pipeline_query_requirement *q=r->queries+j;uint32_t actual[32]={0};size_t count=0;
   e=resolve(user,stage,&q->query,actual,&count);
   if(e==ENOTSUP||e==ENODATA){candidate_unknown=1;continue;}
   if(e)return e;
   if(count>32)return EOVERFLOW;
   if(count!=q->query.count||memcmp(actual,q->values,count*4))matches=0;
  }
  if(matches&&candidate_unknown)unknown=1;
  if(matches&&!candidate_unknown){if(found)return EEXIST;found=s;}
 }
 if(unknown||!found)return ENOTSUP;
 *out=found;return 0;
}
int nn_pipeline_select_pair(const struct nn_pipeline_archive *a,const struct nn_offline_graphics_stage *vs,
 const struct nn_offline_graphics_stage *fs,const struct nn_pipeline_shader_recipe **out)
{
 if(!out)return EINVAL;
 *out=NULL;int e=nn_pipeline_archive_validate(a);if(e)return e;
 const struct nn_offline_graphics_pair *pair=NULL;
 e=nn_offline_graphics_find_pair(a->stages,a->platform->chip,vs,fs,&pair);if(e)return e;
 size_t vi=(size_t)(vs-a->stages->stages),fi=(size_t)(fs-a->stages->stages);const struct nn_pipeline_shader_recipe *found=NULL;
 for(size_t i=0;i<a->pair_count;i++) {
  const struct nn_pipeline_pair_recipe *p=a->pairs+i;
  if(p->vs_index>=a->stages->stage_count||p->fs_index>=a->stages->stage_count||!p->recipe)return EINVAL;
  if(p->vs_index!=vi||p->fs_index!=fi)continue;
  const struct nn_pipeline_shader_recipe *r=p->recipe;
  if(r->chip!=a->platform->chip||r->code_bytes[0]!=pair->vs_bytes||r->code_bytes[1]!=pair->fs_bytes||
     !r->code[0]||!r->code[1]||memcmp(r->code[0],pair->vs_code,pair->vs_bytes)||memcmp(r->code[1],pair->fs_code,pair->fs_bytes))return EINVAL;
  if(found)return EEXIST;
  found=r;
 }
 if(!found)return ENOTSUP;
 *out=found;return 0;
}

/* No native dimension is inferred from reflection's source sampler type:
 * frontend 1D may have become native 2D. Only normal actual-NIR tables qualify. */
static int pair_types(const struct nn_pipeline_archive *a,size_t index,
 const struct nn_pipeline_texture_types *t)
{
 const struct nn_pipeline_pair_recipe *p=a->pairs+index;
 if(p->vs_index>=a->stages->stage_count||p->fs_index>=a->stages->stage_count)return EINVAL;
 const struct nn_offline_graphics_stage *s[2]={a->stages->stages+p->vs_index,a->stages->stages+p->fs_index};
 const struct nn_pipeline_shader_recipe *r=NULL;
 int e=nn_pipeline_select_pair(a,s[0],s[1],&r);if(e)return e;
 if(!t||t->version!=1||t->recipe!=r||t->texture_mask!=r->texture_mask||
    t->sampler_mask!=r->sampler_mask||r->texture_mask>65535||r->sampler_mask>65535)return EINVAL;
 for(unsigned i=0;i<2;i++){
  uint32_t magic=0;
  if(!s[i]->specialization||!s[i]->spirv||s[i]->spirv_bytes<20||s[i]->spirv_bytes%4||
     s[i]->spirv_bytes>1048576||!t->spirv[i]||t->spirv_bytes[i]!=s[i]->spirv_bytes||
     memcmp(t->spirv[i],s[i]->spirv,t->spirv_bytes[i]))return EINVAL;
  memcpy(&magic,s[i]->spirv,4);if(magic!=0x07230203)return EINVAL;
 }
 for(unsigned i=0;i<16;i++){
  int valid=nn_pipeline_texture_type_validate(t,i);if(valid)return valid;
 }
 return 0;
}
int nn_pipeline_typed_archive_validate(const struct nn_pipeline_typed_archive *t)
{
 if(!t||t->version!=1)return EINVAL;
 int e=nn_pipeline_archive_validate(t->archive);if(e)return e;
 if(!t->pair_count||t->pair_count!=t->archive->pair_count||!t->pairs)return EINVAL;
 for(size_t i=0;i<t->pair_count;i++)if((e=pair_types(t->archive,i,t->pairs[i])))return e;
 return 0;
}
int nn_pipeline_select_texture_types(const struct nn_pipeline_typed_archive *t,
 const struct nn_offline_graphics_stage *vs,const struct nn_offline_graphics_stage *fs,
 const struct nn_pipeline_shader_recipe *recipe,const struct nn_pipeline_texture_types **out)
{
 if(!out)return EINVAL;
 *out=NULL;int e=nn_pipeline_typed_archive_validate(t);if(e)return e;
 const struct nn_pipeline_shader_recipe *r=NULL;
 e=nn_pipeline_select_pair(t->archive,vs,fs,&r);if(e)return e;
 if(r!=recipe)return EINVAL;
 for(size_t i=0;i<t->pair_count;i++)if(t->archive->pairs[i].recipe==r){*out=t->pairs[i];return 0;}
 return ENOTSUP;
}
