/* SPDX-License-Identifier: MIT. HOST packets/identity/owners only, no GPU oracle. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_cache ram_cache
#define nn_bo_destroy ram_destroy
#include NN_MEMORY_BACKEND
#undef nn_bo_cache
#undef nn_bo_destroy
static unsigned cache_calls,destroy_calls,fail_cache_at,fail_destroy_at;
int nn_bo_cache(nn_bo*b,size_t o,size_t n,int f){if(++cache_calls==fail_cache_at)return EIO;return ram_cache(b,o,n,f);}
int nn_bo_destroy(nn_bo*b){if(++destroy_calls==fail_destroy_at)return EIO;return ram_destroy(b);}
#include "nvn_native/pipeline.h"
#include "nvn_native/pm4.h"
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/texture_volume_sample.h"
#include NN_VOLUME_FETCH_ARTIFACT
static unsigned checks,jobs;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"failure line %u: %s\n",__LINE__,#v);abort();}checks++;}while(0)
static struct nn_tex_native_view target_view(void)
{
 struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=127,.height=73,.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=NN_FDL_SAMPLED|NN_FDL_COLOR_TARGET};
 struct nn_fdl_image_view a;CHECK(!nn_fdl_image(&d,&a));
 struct nn_tex_native_view v={.schema=1,.chip=a.chip,.format=a.format,.target=1,.levels=1,.layers=1,.samples=1,.width=a.width,.height=a.height,.pitch=a.level[0].pitch,.layer_bytes=a.level[0].layer_stride,.bytes=a.bytes,.alignment=a.alignment,.required_span_bytes=a.required_span,.descriptor_address_word=a.descriptor_address_word,.descriptor_address_mask=a.descriptor_address_mask,.rb_mrt_buf_info=a.rb_mrt_buf_info,.rb_mrt_pitch=a.rb_mrt_pitch,.rb_mrt_array_pitch=a.rb_mrt_array_pitch,.sp_ps_mrt_reg=a.sp_ps_mrt_reg};
 memcpy(v.texture_descriptor,a.descriptor,64);return v;
}
static void reject(nn_device*d,nn_pipeline_program*p,const struct nn_pipeline_native_artifact*a,const struct nn_pipeline_draw_state*s)
{struct nn_pipeline_submission out;nn_bo*q=(void*)1;unsigned before=d->bos;CHECK(nn_pipeline_encode(d,p,a->recipe,&nn_pipeline_device,s,&out,&q)!=0);CHECK(!out.job&&!out.resource_count&&!q&&d->bos==before);}
int main(void)
{
 nn_device*d=NULL;CHECK(!nn_device_open(&d));struct nn_tex_native_view tv=target_view();
 nn_bo*target=NULL,*uniform=NULL,*vertices=NULL;CHECK(!nn_bo_alloc(d,tv.required_span_bytes,&target));CHECK(!nn_bo_alloc(d,1024,&uniform));CHECK(!nn_bo_alloc(d,4096,&vertices));
 uint32_t formats[]={37,0x29,0x41,0x42,0x43,0x44};
 const struct nn_pipeline_native_artifact*artifacts[]={&volume_fetch_artifact,&volume_size_artifact};
 for(unsigned mode=0;mode<2;mode++){
  const struct nn_pipeline_native_artifact*a=artifacts[mode];nn_pipeline_program*p=NULL,*retained=NULL;
  CHECK(!nn_pipeline_program_create_native(d,a,&p,&retained)&&p&&!retained);CHECK(nn_pipeline_program_texture_types(p)==a->textures);
  struct nn_pipeline_stage_view stages[2];const struct nn_offline_graphics_stage*stageptr[2];
  for(unsigned i=0;i<2;i++){CHECK(!nn_pipeline_program_stage(p,i,stages+i));stageptr[i]=stages[i].source;CHECK(!stages[i].c1&&!stages[i].source->control);CHECK(!memcmp(nn_bo_map(stages[i].code),a->recipe->code[i],a->recipe->code_bytes[i]));}
  for(unsigned f=0;f<6;f++)for(unsigned base=0;base<3;base++){
   struct nn_fdl_volume_desc desc={.schema=1,.chip=0x06050002,.format=formats[f],.width=35,.height=19,.depth=9,.levels=4,.base_level=base,.level_count=4-base,.usage=NN_FDL_SAMPLED};
   struct nn_fdl_volume_view view;CHECK(!nn_fdl_volume(&desc,&view));CHECK(!nn_volume_sampled_validate(&view));nn_bo*texture=NULL;CHECK(!nn_bo_alloc(d,4096+view.required_span,&texture));
   struct nn_pipeline_draw_state s={.target={&tv,{target,0,tv.required_span_bytes}},.texture_bound=2,.texture_types=a->textures};
   s.textures[1].buffer=(struct nn_pipeline_buffer){texture,4096,view.required_span};s.sampled_volume[1]=&view;
   CHECK(!nn_pipeline_full_raster(tv.width,tv.height,0,1,&s.raster));nn_render_state_defaults(&s.render);
   s.vertex.attribute_count=32;s.vertex.stream_count=32;s.vertex.streams[0]=(struct nn_pipeline_stream){32,0};s.vertex.buffers[0]=(struct nn_pipeline_buffer){vertices,0,4096};
   for(size_t i=0;i<a->recipe->input_count;i++)s.vertex.attributes[a->recipe->inputs[i].location]=(struct nn_pipeline_attribute){0x2e,(uint32_t)i*16,0};
   s.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
   for(size_t i=0;i<a->recipe->patch_count;i++){const struct nn_pipeline_address_patch*u=a->recipe->patches+i;if(u->role==NN_PIPELINE_UBO)s.ubo[u->stage][u->binding]=(struct nn_pipeline_buffer){uniform,0,1024};}
   struct nn_pipeline_submission result;nn_bo*q=NULL;
#ifdef NN_TEST_VOLUME_OFF
   CHECK(nn_pipeline_encode(d,p,a->recipe,&nn_pipeline_device,&s,&result,&q)==ENOTSUP&&!result.job&&!q);CHECK(!nn_bo_destroy(texture));continue;
#endif
   CHECK(!nn_pipeline_encode(d,p,a->recipe,&nn_pipeline_device,&s,&result,&q)&&result.job&&!q);jobs++;
   uint32_t expected[16];CHECK(!nn_volume_sampled_descriptor(&view,texture,4096,view.required_span,expected));
   uint32_t*table=(uint32_t*)((unsigned char*)nn_bo_map(result.job)+40960);for(unsigned i=0;i<16;i++)for(unsigned j=0;j<16;j++)CHECK(table[i*16+j]==(i==1?expected[j]:0));
   unsigned found=0;for(size_t i=0;i<result.resource_count;i++){found+=result.resources[i]==texture;result.resources[i]->busy++;}CHECK(found==1);CHECK(nn_pipeline_program_destroy(p)==EBUSY&&nn_bo_destroy(texture)==EBUSY);for(size_t i=0;i<result.resource_count;i++)result.resources[i]->busy--;
   size_t at=0;uint32_t*w=nn_bo_map(result.job);while(at<result.command_bytes/4){size_t n=nvn_pm4_packet_words(w[at]);CHECK(n&&n<=result.command_bytes/4-at);at+=n;}CHECK(at==result.command_bytes/4);CHECK(!nn_bo_destroy(result.job));
   struct nn_pipeline_draw_state bad=s;bad.texture_types=NULL;reject(d,p,a,&bad);
   bad=s;bad.sampled_volume[2]=&view;reject(d,p,a,&bad);
   bad=s;bad.textures[1].layout=&tv;reject(d,p,a,&bad);
   bad=s;bad.textures[1].buffer.bytes=view.required_span-1;reject(d,p,a,&bad);
   bad=s;bad.textures[1].buffer.offset=4097;reject(d,p,a,&bad);
   bad=s;bad.textures[1].buffer.bo=target;reject(d,p,a,&bad);
   struct nn_fdl_volume_view broken=view;broken.level[base].slice_stride++;bad=s;bad.sampled_volume[1]=&broken;reject(d,p,a,&bad);
   broken=view;broken.descriptor[0]^=1;bad=s;bad.sampled_volume[1]=&broken;reject(d,p,a,&bad);
   struct nn_pipeline_texture_types types=*a->textures;bad=s;bad.texture_types=&types;reject(d,p,a,&bad); /* same values, not Program-bound identity */
   bad=s;bad.textures[1].buffer=(struct nn_pipeline_buffer){.bytes=view.required_span};CHECK(!nn_pipeline_preflight(d,stageptr,a->recipe,&nn_pipeline_device,&bad,1u<<2));
   types=*a->textures;types.dimensions[1]=2;bad.texture_types=&types;CHECK(nn_pipeline_preflight(d,stageptr,a->recipe,&nn_pipeline_device,&bad,1u<<2)==ENOTSUP);
   types=*a->textures;types.operations[1]=NN_PIPELINE_TEX_SAMPLE;CHECK(nn_pipeline_preflight(d,stageptr,a->recipe,&nn_pipeline_device,&bad,1u<<2)==ENOTSUP);
   types=*a->textures;types.dimensions[0]=3;CHECK(nn_pipeline_preflight(d,stageptr,a->recipe,&nn_pipeline_device,&bad,1u<<2)==EINVAL);
   types=*a->textures;types.recipe=artifacts[1-mode]->recipe;CHECK(nn_pipeline_preflight(d,stageptr,a->recipe,&nn_pipeline_device,&bad,1u<<2)==EINVAL);
   types=*a->textures;types.spirv[1]=artifacts[1-mode]->spirv[1];types.spirv_bytes[1]=artifacts[1-mode]->spirv_bytes[1];CHECK(nn_pipeline_preflight(d,stageptr,a->recipe,&nn_pipeline_device,&bad,1u<<2)==EINVAL);
   uint32_t zero[16]={0},out[16];memset(out,0xff,64);CHECK(nn_volume_sampled_descriptor(&view,texture,SIZE_MAX,view.required_span,out)==EINVAL&&!memcmp(out,zero,64));
   CHECK(!nn_bo_destroy(texture));
  }
  CHECK(!nn_pipeline_program_destroy(p));
  fail_cache_at=cache_calls+1;fail_destroy_at=destroy_calls+1;CHECK(nn_pipeline_program_create_native(d,a,&p,&retained)==EIO&&!p&&retained);CHECK(nn_pipeline_program_cleanup_error(retained)==EIO);CHECK(!nn_pipeline_program_destroy(retained));fail_cache_at=fail_destroy_at=0;
  struct nn_pipeline_native_artifact bad=*a;bad.spirv_bytes[0]-=4;CHECK(nn_pipeline_program_create_native(d,&bad,&p,&retained)==EINVAL&&!p&&!retained);
 }
 CHECK(!nn_bo_destroy(vertices));CHECK(!nn_bo_destroy(uniform));CHECK(!nn_bo_destroy(target));CHECK(!d->bos);CHECK(!nn_device_close(d));printf("{\"passed\":true,\"checks\":%u,\"encoded_jobs\":%u,\"gpu_execution\":false,\"android_execution\":false}\n",checks,jobs);return 0;
}
