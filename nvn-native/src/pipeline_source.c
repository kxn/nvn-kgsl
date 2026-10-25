/* SPDX-License-Identifier: MIT. Full source identity is independent of variant. */
#include "nvn_native/pipeline_source.h"
#include <errno.h>
#include <string.h>
#ifndef NN_PIPELINE_SOURCE_CATALOGUE
#define NN_PIPELINE_SOURCE_CATALOGUE 0
#endif
#if NN_PIPELINE_SOURCE_CATALOGUE != 0 && NN_PIPELINE_SOURCE_CATALOGUE != 1
#error "NN_PIPELINE_SOURCE_CATALOGUE must be 0 or 1"
#endif
int nn_pipeline_source_prepare(const struct nn_pipeline_archive *a,const struct nn_pipeline_source_input *input,size_t count,struct nn_pipeline_source_program *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=nn_pipeline_archive_validate(a);if(e)return e;
 if(!input||count!=2)return ENOTSUP;
 struct nn_pipeline_source_program result={.archive=a};
 for(size_t i=0;i<count;i++) {
  const struct nn_pipeline_source_input *in=input+i;
  if(in->stage>1)return ENOTSUP;
  if(result.identity[in->stage]||!in->source||!in->control||!in->source_bytes||in->source_bytes>1048576||!in->control_bytes||in->control_bytes>1048576)return EINVAL;
  for(size_t j=0;j<a->stages->stage_count;j++) {
   const struct nn_offline_graphics_stage *s=a->stages->stages+j;
   if(s->stage!=in->stage||s->source_bytes!=in->source_bytes||s->control_bytes!=in->control_bytes)continue;
   if(!s->source||!s->control||!s->code_bytes||s->code_bytes>s->source_bytes)return EINVAL;
   if(memcmp(in->source,s->source,in->source_bytes)||memcmp(in->control,s->control,in->control_bytes))continue;
   /* Multiple specializations may share a full source group. Preserve only
    * identity, and reject contradictory code/C1 boundaries within that group. */
   const struct nn_offline_graphics_stage *old=result.identity[in->stage];
   if(old&&old->code_bytes!=s->code_bytes)return EINVAL;
   if(!old)result.identity[in->stage]=s;
  }
  if(!result.identity[in->stage])return ENOTSUP;
 }
 *out=result;return 0;
}
int nn_pipeline_source_prepare_bound(const struct nn_pipeline_archive*a,const struct nn_shader_source_program*b,struct nn_pipeline_source_program*out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));
#if NN_PIPELINE_SOURCE_CATALOGUE
 int e=nn_pipeline_archive_validate(a);if(e)return e;
 for(unsigned i=0;i<2;i++){const struct nn_shader_source_entry*s=nn_shader_source_program_stage(b,i);if(!s||s->stage!=i)return EINVAL;}
 *out=(struct nn_pipeline_source_program){.archive=a,.binding=b};return 0;
#else
 (void)a;(void)b;return ENOTSUP;
#endif
}
int nn_pipeline_source_identity(const struct nn_pipeline_source_program*p,uint32_t stage,struct nn_pipeline_source_input*out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!p||stage>1||!p->archive)return EINVAL;
 int e=nn_pipeline_archive_validate(p->archive);if(e)return e;
 if(p->binding){
#if NN_PIPELINE_SOURCE_CATALOGUE
  if(p->identity[0]||p->identity[1])return EINVAL;
  const struct nn_shader_source_entry*s=nn_shader_source_program_stage(p->binding,stage);if(!s||s->stage!=stage)return EINVAL;
  *out=(struct nn_pipeline_source_input){stage,s->source,s->source_bytes,s->control,s->control_bytes};return 0;
#else
  return ENOTSUP;
#endif
 }
 for(size_t i=0;i<p->archive->stages->stage_count;i++){
  const struct nn_offline_graphics_stage*s=p->archive->stages->stages+i;
  if(s==p->identity[stage]&&s->stage==stage){*out=(struct nn_pipeline_source_input){stage,s->source,s->source_bytes,s->control,s->control_bytes};return 0;}
 }
 return EINVAL;
}
int nn_pipeline_source_resolve(const struct nn_pipeline_source_program *p,nn_pipeline_query_resolver resolve,void *user,const struct nn_offline_graphics_stage **vs,const struct nn_offline_graphics_stage **fs,const struct nn_pipeline_shader_recipe **recipe)
{
 if(vs)*vs=NULL;
 if(fs)*fs=NULL;
 if(recipe)*recipe=NULL;
 if(!p||!vs||!fs||!recipe||!resolve)return EINVAL;
 int e=nn_pipeline_archive_validate(p->archive);if(e)return e;
 const struct nn_offline_graphics_stage *selected[2]={0};
 for(unsigned i=0;i<2;i++) {
  struct nn_pipeline_source_input identity;
  e=nn_pipeline_source_identity(p,i,&identity);if(e)return e;
  e=nn_pipeline_select_stage(p->archive,i,identity.source,identity.source_bytes,identity.control,identity.control_bytes,resolve,user,selected+i);
  if(e)return e;
 }
 const struct nn_pipeline_shader_recipe *r=NULL;e=nn_pipeline_select_pair(p->archive,selected[0],selected[1],&r);if(e)return e;
 *vs=selected[0];*fs=selected[1];*recipe=r;return 0;
}
