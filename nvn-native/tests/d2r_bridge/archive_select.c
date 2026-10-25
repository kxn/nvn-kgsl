/* HOST ONLY. Qualification query mock; never used by production NVN. */
#define _POSIX_C_SOURCE 200809L
#include NN_PROGRAM_MEMORY_BACKEND
#include "nvn_native/pipeline_archive.h"
#include NN_PIPELINE_ARCHIVE
#include "nvn_native/pipeline_source.h"
static unsigned checks,calls,dither_calls;
#define CHECK(v) do{assert(v);checks++;}while(0)
struct context {const struct nn_pipeline_stage_requirements *stage;int unknown,short_count,hard_error,mismatch;};
static int query(void *user,uint32_t stage,const struct nn_pipeline_query *key,uint32_t values[32],size_t *count)
{
 (void)stage;struct context *c=user;calls++;
 if(!strcmp(key->name,"graphics/alpha_to_coverage_dither_enable"))dither_calls++;
 if(c->hard_error)return EIO;
 if(c->unknown)return ENOTSUP;
 for(size_t i=0;i<c->stage->query_count;i++) {
  const struct nn_pipeline_query_requirement *q=c->stage->queries+i;
  if(strcmp(key->name,q->query.name)||key->arguments!=q->query.arguments||memcmp(key->args,q->query.args,sizeof(key->args)))continue;
  CHECK(key->type==q->query.type&&key->count==q->query.count);memcpy(values,q->values,key->count*4);*count=c->short_count?0:key->count;
  if(c->mismatch)values[0]^=1;
  return 0;
 }
 return ENODATA;
}
static int program_query(void *user,uint32_t stage,const struct nn_pipeline_query *key,uint32_t values[32],size_t *count)
{
 const struct nn_pipeline_stage_requirements *const *desired=user;
 if(stage>1||!desired[stage])return ENODATA;
 /* Authored runtime state is independent of Program's source identity. More
  * than one specialization may share those bytes; Prepare chooses no variant. */
 struct context c={.stage=desired[stage]};
 return query(&c,stage,key,values,count);
}
int main(void)
{
 const struct nn_pipeline_archive *a=&NN_GRAPHICS_PIPELINE_ARCHIVE;CHECK(a->stages->stage_count>0&&a->stages->stage_count<128&&a->pair_count>0);
 for(size_t i=0;i<a->stages->stage_count;i++) {
  const struct nn_offline_graphics_stage *s=a->stages->stages+i,*found=(void *)1;struct context c={a->requirements+i,0,0,0,0};
  CHECK(!nn_pipeline_select_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,query,&c,&found)&&found==s);
  CHECK(!dither_calls); /* ATOC=false is retained; raw dither1 is irrelevant by audited compiler condition. */
  c.unknown=1;CHECK(nn_pipeline_select_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,query,&c,&found)==ENOTSUP&&!found);c.unknown=0;
  c.short_count=1;CHECK(nn_pipeline_select_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,query,&c,&found)==ENOTSUP&&!found);c.short_count=0;
  c.hard_error=1;CHECK(nn_pipeline_select_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,query,&c,&found)==EIO&&!found);c.hard_error=0;
  c.mismatch=1;CHECK(nn_pipeline_select_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,query,&c,&found)==ENOTSUP&&!found);c.mismatch=0;
  unsigned char *copy=malloc(s->source_bytes);CHECK(copy);memcpy(copy,s->source,s->source_bytes);copy[s->source_bytes-1]^=1;unsigned before=calls;
  CHECK(nn_pipeline_select_stage(a,s->stage,copy,s->source_bytes,s->control,s->control_bytes,query,&c,&found)==ENOTSUP&&!found&&calls==before);free(copy);
  copy=malloc(s->control_bytes);CHECK(copy);memcpy(copy,s->control,s->control_bytes);copy[s->control_bytes-1]^=1;
  CHECK(nn_pipeline_select_stage(a,s->stage,s->source,s->source_bytes,copy,s->control_bytes,query,&c,&found)==ENOTSUP&&!found&&calls==before);free(copy);
 }
 for(size_t i=0;i<a->pair_count;i++) {
  const struct nn_pipeline_pair_recipe *p=a->pairs+i;const struct nn_pipeline_shader_recipe *r=NULL;
  CHECK(!nn_pipeline_select_pair(a,a->stages->stages+p->vs_index,a->stages->stages+p->fs_index,&r)&&r==p->recipe);
 }
 /* ProgramSetShaders-style preparation snapshots full archive-owned source
  * identity without making a specialization choice or borrowing input bytes. */
 for(size_t i=0;i<a->pair_count;i++) {
  const struct nn_pipeline_pair_recipe *pair=a->pairs+i;const struct nn_offline_graphics_stage *stages[]={a->stages->stages+pair->vs_index,a->stages->stages+pair->fs_index};
  struct nn_pipeline_source_input inputs[2];unsigned char *copies[2];
  for(unsigned j=0;j<2;j++){copies[j]=malloc(stages[j]->source_bytes);CHECK(copies[j]);memcpy(copies[j],stages[j]->source,stages[j]->source_bytes);inputs[j]=(struct nn_pipeline_source_input){j,copies[j],stages[j]->source_bytes,stages[j]->control,stages[j]->control_bytes};}
  struct nn_pipeline_source_program p;unsigned before=calls;
  copies[0][inputs[0].source_bytes-1]^=1;CHECK(nn_pipeline_source_prepare(a,inputs,2,&p)==ENOTSUP&&!p.archive&&calls==before);copies[0][inputs[0].source_bytes-1]^=1;
  CHECK(!nn_pipeline_source_prepare(a,inputs,2,&p)&&calls==before);
  struct context unavailable={a->requirements+pair->vs_index,1,0,0,0};const struct nn_offline_graphics_stage *uv=(void *)1,*uf=(void *)1;const struct nn_pipeline_shader_recipe *ur=(void *)1;
  CHECK(nn_pipeline_source_resolve(&p,query,&unavailable,&uv,&uf,&ur)==ENOTSUP&&!uv&&!uf&&!ur);
  for(unsigned j=0;j<2;j++){memset(copies[j],0xa5,stages[j]->source_bytes);free(copies[j]);}
  const struct nn_offline_graphics_stage *v=(void *)1,*f=(void *)1;const struct nn_pipeline_shader_recipe *r=(void *)1;
  const struct nn_pipeline_stage_requirements *desired[]={a->requirements+pair->vs_index,a->requirements+pair->fs_index};
  CHECK(!nn_pipeline_source_resolve(&p,program_query,desired,&v,&f,&r)&&v==stages[0]&&f==stages[1]&&r==pair->recipe);
  inputs[0].source=NULL;inputs[0].stage=2;CHECK(nn_pipeline_source_prepare(a,inputs,2,&p)==ENOTSUP&&!p.archive&&!p.identity[0]&&!p.identity[1]);
  CHECK(nn_pipeline_source_resolve(&p,program_query,desired,&v,&f,&r)==EINVAL&&!v&&!f&&!r);
  inputs[0].stage=0;CHECK(nn_pipeline_source_prepare(a,inputs,1,&p)==ENOTSUP&&!p.archive);
 }
 struct nn_pipeline_archive bad=*a;struct nn_pipeline_stage_requirements records[128];memcpy(records,a->requirements,a->requirement_count*sizeof(*records));records[0].stage_index=1;bad.requirements=records;
 const struct nn_offline_graphics_stage *s=a->stages->stages,*found=(void *)1;struct context c={a->requirements,0,0,0,0};
 CHECK(nn_pipeline_select_stage(&bad,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,query,&c,&found)==EINVAL&&!found);
 /* Add a second full-identity variant with identical requirements: selection
  * must reject ambiguity instead of picking archive order. */
 struct nn_offline_graphics_stage stages[128];memcpy(stages,a->stages->stages,a->stages->stage_count*sizeof(*stages));size_t extra=a->stages->stage_count;stages[extra]=stages[0];
 struct nn_offline_graphics_archive source=*a->stages;source.stages=stages;source.stage_count++;
 memcpy(records,a->requirements,a->requirement_count*sizeof(*records));records[extra]=records[0];records[extra].stage_index=(uint32_t)extra;
 bad=*a;bad.stages=&source;bad.requirements=records;bad.requirement_count++;
 CHECK(nn_pipeline_select_stage(&bad,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,query,&c,&found)==EEXIST&&!found);
 printf("HOST ONLY pipeline runtime-query selection checks=%u; GPU execution=false\n",checks);return 0;
}
