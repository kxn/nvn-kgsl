/* HOST ONLY generated recipe ABI/patch ownership; no GPU or NVN Draw. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include NN_UNIT_SCALE_ARCHIVE
int main(void)
{
 const struct nn_pipeline_archive *a=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 unsigned checks=0,ubo_patches=0;
 assert(a->pair_count==3);
 for(size_t i=0;i<a->pair_count;i++){
  const struct nn_pipeline_shader_recipe *r=a->pairs[i].recipe;
  unsigned fs_ubos=0;
  assert(r->version==1&&r->texture_mask==6&&!r->sampler_mask);checks++;
  for(size_t j=0;j<r->patch_count;j++){
   const struct nn_pipeline_address_patch *p=r->patches+j;
   if(p->role!=NN_PIPELINE_UBO)continue;
   assert(p->binding!=0);checks++;ubo_patches++;
   if(p->stage==1){assert(p->binding==3||p->binding==4);fs_ubos|=1u<<p->binding;checks++;}
  }
  assert(fs_ubos==((1u<<3)|(1u<<4)));checks++;
 }
 for(size_t i=0;i<a->requirement_count;i++){
  const struct nn_pipeline_stage_requirements *s=a->requirements+i;
  int unit=0;
  for(size_t j=0;j<s->query_count;j++){
   const struct nn_pipeline_query_requirement *q=s->queries+j;
   assert(strcmp(q->query.name,"QueryTextureCoordNormalized"));checks++;
   if(!strcmp(q->query.name,"backend/unit_render_scale")){
    assert(!q->query.arguments&&q->query.type==NN_PIPELINE_QUERY_BOOL&&q->query.count==1&&q->values[0]==1);unit++;checks++;
   }
  }
  assert(unit==(a->stages->stages[s->stage_index].stage==1));checks++;
 }
 printf("{\"checks\":%u,\"ubo_patches\":%u,\"pairs\":3,\"ubo0_patches\":0,\"gpu_submits\":0}\n",checks,ubo_patches);
 return 0;
}
