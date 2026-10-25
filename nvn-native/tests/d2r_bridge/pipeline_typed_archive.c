/* HOST ONLY. Actual normal Maxwell archive + actual lowered-NIR type facts.
 * RAM BO backend tests upload/retirement, never GPU execution or resource facts. */
#define main legacy_program_owner_main
#include "program_owner.c"
#undef main
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/pipeline_archive.h"
#include "nvn_native/pipeline_resource_queries.h"
int main(void)
{
 CHECK(!legacy_program_owner_main());
 const struct nn_pipeline_typed_archive *a=&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE;
 CHECK(!nn_pipeline_typed_archive_validate(a));CHECK(a->pair_count==a->archive->pair_count);
 nn_device *device=NULL;CHECK(!nn_device_open(&device));
 const struct nn_pipeline_texture_types **tables=calloc(a->pair_count,sizeof(*tables));CHECK(tables);
 memcpy(tables,a->pairs,a->pair_count*sizeof(*tables));
 for(size_t i=0;i<a->pair_count;i++){
  const struct nn_pipeline_pair_recipe *pair=a->archive->pairs+i;
  const struct nn_offline_graphics_stage *vs=a->archive->stages->stages+pair->vs_index,*fs=a->archive->stages->stages+pair->fs_index;
  const struct nn_pipeline_texture_types *types=(void*)1;
  CHECK(!nn_pipeline_select_texture_types(a,vs,fs,pair->recipe,&types)&&types==a->pairs[i]);
  CHECK(types->recipe==pair->recipe&&types->spirv_bytes[0]==vs->spirv_bytes&&types->spirv_bytes[1]==fs->spirv_bytes);
  CHECK(!memcmp(types->spirv[0],vs->spirv,vs->spirv_bytes)&&!memcmp(types->spirv[1],fs->spirv,fs->spirv_bytes));
  nn_pipeline_program *p=(void*)1,*q=(void*)1;
  CHECK(!nn_pipeline_program_create_typed(device,a->archive->stages,vs,fs,a->pairs[i],&p,&q)&&p&&!q);
  CHECK(nn_pipeline_program_texture_types(p)==types);
  struct nn_pipeline_stage_view stages[2];CHECK(!nn_pipeline_program_stage(p,0,stages));CHECK(!nn_pipeline_program_stage(p,1,stages+1));
  CHECK(stages[0].source==vs&&stages[1].source==fs);
  nn_bo *refs[4];size_t count=0;CHECK(!nn_pipeline_program_resources(p,refs,4,&count)&&count>=2);
  refs[0]->busy++;CHECK(nn_pipeline_program_destroy(p)==EBUSY&&nn_pipeline_program_texture_types(p)==types);refs[0]->busy--;
  CHECK(!nn_pipeline_program_destroy(p)&&!device->bos);
  CHECK(!nn_pipeline_program_create(device,a->archive->stages,vs,fs,vs->specialization,fs->specialization,&p,&q));
  CHECK(!nn_pipeline_program_texture_types(p));CHECK(!nn_pipeline_program_destroy(p));
  struct nn_pipeline_shader_recipe wrong=*pair->recipe;
  CHECK(nn_pipeline_select_texture_types(a,vs,fs,&wrong,&types)==EINVAL&&!types);
  struct nn_offline_graphics_stage clone=*fs;
  CHECK(nn_pipeline_program_create_typed(device,a->archive->stages,vs,&clone,a->pairs[i],&p,&q)!=0&&!p&&!q&&!device->bos);
  struct nn_pipeline_typed_archive bad=*a;bad.pairs=tables;
  struct nn_pipeline_texture_types wrong_types=*tables[i];tables[i]=&wrong_types;
  wrong_types.recipe=&wrong;CHECK(nn_pipeline_typed_archive_validate(&bad)==EINVAL);wrong_types=*a->pairs[i];
  wrong_types.texture_mask^=1;CHECK(nn_pipeline_typed_archive_validate(&bad)==EINVAL);wrong_types=*a->pairs[i];
  wrong_types.spirv_bytes[1]-=4;CHECK(nn_pipeline_typed_archive_validate(&bad)==EINVAL);wrong_types=*a->pairs[i];
  unsigned char *spv=malloc(fs->spirv_bytes);CHECK(spv);memcpy(spv,fs->spirv,fs->spirv_bytes);spv[fs->spirv_bytes-1]^=1;
  wrong_types.spirv[1]=(const uint32_t*)spv;CHECK(nn_pipeline_typed_archive_validate(&bad)==EINVAL);free(spv);wrong_types=*a->pairs[i];
  unsigned inactive=15;while(inactive&&(wrong_types.texture_mask&(1u<<inactive)))inactive--;
  CHECK(!(wrong_types.texture_mask&(1u<<inactive)));wrong_types.dimensions[inactive]=3;
  CHECK(nn_pipeline_program_create_typed(device,a->archive->stages,vs,fs,&wrong_types,&p,&q)==EINVAL&&!p&&!q&&!device->bos);
  wrong_types=*a->pairs[i];tables[i]=a->pairs[i];
  fail_cache_at=cache_calls+1;fail_destroy_at=destroy_calls+1;
  CHECK(nn_pipeline_program_create_typed(device,a->archive->stages,vs,fs,a->pairs[i],&p,&q)==EIO&&!p&&q&&device->bos==1);
  CHECK(!nn_pipeline_program_texture_types(q));CHECK(!nn_pipeline_program_destroy(q)&&!device->bos);fail_cache_at=fail_destroy_at=0;
 }
 struct nn_pipeline_typed_archive bad=*a;bad.pair_count--;CHECK(nn_pipeline_typed_archive_validate(&bad)==EINVAL);
 bad=*a;bad.version=2;CHECK(nn_pipeline_typed_archive_validate(&bad)==EINVAL);
 /* Actual 3D resource is still unsupported, even though package metadata is
  * now propagated. No archived expected 2D query is supplied as actual state. */
 struct nn_pipeline_resource_binding binding={.stage=1,.domain=NN_PIPELINE_SEPARATE_TEXTURE,.selector=10,.low_word_defined=1,.payload=256,.guest_carrier=256};
 binding.texture=(struct nn_pipeline_registered_texture){.registered=1,.descriptor_pool=a,.descriptor_id=256,.backing_pool=device,.bytes=16384,.width=32,.height=32,.depth=32,.levels=1,.format=37,.target=2};
 struct nn_pipeline_resource_queries resources={.version=1,.graphics_texture_cb_known=1,.graphics_texture_cb=2,.selected_texture_pool=a,.bindings=&binding,.binding_count=1};
 struct nn_pipeline_resolved_texture resolved;CHECK(nn_pipeline_resource_texture(&resources,1,110,-1,&resolved)==ENOTSUP);
 free(tables);CHECK(!nn_device_close(device));printf("HOST ONLY typed Maxwell archive checks=%u; GPU execution=false; guest3D remains unsupported\n",checks);return 0;
}
