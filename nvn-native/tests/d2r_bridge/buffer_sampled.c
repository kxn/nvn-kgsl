/* SPDX-License-Identifier: MIT. HOST packets/identity/owners only, no GPU oracle. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_map ram_map
#define nn_bo_cache ram_cache
#define nn_bo_destroy ram_destroy
#include NN_MEMORY_BACKEND
#undef nn_bo_map
#undef nn_bo_cache
#undef nn_bo_destroy
static nn_bo *forbidden_source_map;static unsigned source_map_attempts;
void *nn_bo_map(const nn_bo*b){if(b==forbidden_source_map){source_map_attempts++;abort();}return ram_map(b);}
static unsigned cache_calls,destroy_calls,fail_cache_at,fail_destroy_at;
int nn_bo_cache(nn_bo*b,size_t o,size_t n,int f){if(++cache_calls==fail_cache_at)return EIO;return ram_cache(b,o,n,f);}
int nn_bo_destroy(nn_bo*b){if(++destroy_calls==fail_destroy_at)return EIO;return ram_destroy(b);}
#include "nvn_native/pipeline.h"
#include "nvn_native/pm4.h"
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/texture_buffer_sample.h"
#include "nvn_native/texture_buffer_format.h"
#include NN_BUFFER_FETCH_ARTIFACT
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
 nn_bo*target=NULL,*uniform=NULL,*vertices=NULL;CHECK(!nn_bo_alloc(d,tv.required_span_bytes+4096,&target));CHECK(!nn_bo_alloc(d,1024,&uniform));CHECK(!nn_bo_alloc(d,4096,&vertices));
 uint32_t formats[]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,154};
 const struct nn_pipeline_native_artifact*artifacts[]={&buffer_float_artifact,&buffer_size_artifact};
 for(unsigned mode=0;mode<2;mode++){
  const struct nn_pipeline_native_artifact*a=artifacts[mode];nn_pipeline_program*p=NULL,*retained=NULL;
  CHECK(!nn_pipeline_program_create_native(d,a,&p,&retained)&&p&&!retained);
  struct nn_pipeline_stage_view stages[2];const struct nn_offline_graphics_stage*stageptr[2];
  for(unsigned i=0;i<2;i++){CHECK(!nn_pipeline_program_stage(p,i,stages+i));stageptr[i]=stages[i].source;}
  for(unsigned f=0;f<40;f++)for(unsigned prefix=0;prefix<3;prefix++){
   struct nn_fdl_buffer_desc zero={.schema=1,.chip=0x06050002,.format=formats[f],.bytes=16};
   uint32_t cpp;CHECK(!nn_tex_buffer_element_bytes(formats[f],&cpp));zero.bytes=13*cpp;
   struct nn_fdl_buffer_view z;CHECK(!nn_fdl_buffer(&zero,&z));nn_bo*texture=NULL;CHECK(!nn_bo_alloc(d,8192,&texture));
   struct nn_buffer_sampled_binding view={.version=1,.format=formats[f],.resource=texture,.owner_offset=4096,.owner_bytes=4096,.offset=4096+prefix*cpp,.bytes=13*cpp};
   struct nn_fdl_buffer_view native;CHECK(!nn_buffer_sampled_view(&view,&native));
   CHECK(native.elements==13&&native.element_bytes==cpp&&native.required_prefix==prefix*cpp&&native.required_span==prefix*cpp+13*cpp);
   struct nn_fdl_buffer_desc direct=zero;direct.address=nn_bo_address(texture)+view.offset;
   CHECK(!nn_fdl_buffer(&direct,&z)&&!memcmp(&native,&z,sizeof(z)));
   memset(ram_map(texture),0xa5,nn_bo_size(texture));forbidden_source_map=texture;
   struct nn_pipeline_draw_state s={.target={&tv,{target,0,tv.required_span_bytes}},.texture_bound=2,.texture_types=a->textures};s.sampled_buffer[1]=&view;
   CHECK(!nn_pipeline_full_raster(tv.width,tv.height,0,1,&s.raster));nn_render_state_defaults(&s.render);
   s.vertex.attribute_count=32;s.vertex.stream_count=32;s.vertex.streams[0]=(struct nn_pipeline_stream){32,0};s.vertex.buffers[0]=(struct nn_pipeline_buffer){vertices,0,4096};
   for(size_t i=0;i<a->recipe->input_count;i++)s.vertex.attributes[a->recipe->inputs[i].location]=(struct nn_pipeline_attribute){0x2e,(uint32_t)i*16,0};
   s.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
   for(size_t i=0;i<a->recipe->patch_count;i++){const struct nn_pipeline_address_patch*u=a->recipe->patches+i;if(u->role==NN_PIPELINE_UBO)s.ubo[u->stage][u->binding]=(struct nn_pipeline_buffer){uniform,0,1024};}
   struct nn_pipeline_submission result;nn_bo*q=NULL;uint32_t numeric;CHECK(!nn_buffer_sampled_result(view.format,&numeric));
#ifdef NN_TEST_BUFFER_OFF
   CHECK(nn_pipeline_encode(d,p,a->recipe,&nn_pipeline_device,&s,&result,&q)==ENOTSUP&&!result.job&&!q);forbidden_source_map=NULL;CHECK(!nn_bo_destroy(texture));continue;
#endif
   if(!mode&&numeric!=NN_BUFFER_FLOAT){reject(d,p,a,&s);forbidden_source_map=NULL;CHECK(!nn_bo_destroy(texture));continue;}
   CHECK(!nn_pipeline_encode(d,p,a->recipe,&nn_pipeline_device,&s,&result,&q)&&result.job&&!q);jobs++;
   uint32_t expected[16];CHECK(!nn_buffer_sampled_descriptor(&view,expected));uint32_t*table=(uint32_t*)((unsigned char*)nn_bo_map(result.job)+40960);
   for(unsigned i=0;i<16;i++)for(unsigned j=0;j<16;j++)CHECK(table[i*16+j]==(i==1?expected[j]:0));
   unsigned found=0;for(size_t i=0;i<result.resource_count;i++){found+=result.resources[i]==texture;result.resources[i]->busy++;}
   CHECK(found==1);CHECK(nn_pipeline_program_destroy(p)==EBUSY&&nn_bo_destroy(texture)==EBUSY);
   for(size_t i=0;i<result.resource_count;i++)result.resources[i]->busy--;
   size_t at=0;uint32_t*w=nn_bo_map(result.job);while(at<result.command_bytes/4){size_t n=nvn_pm4_packet_words(w[at]);CHECK(n&&n<=result.command_bytes/4-at);at+=n;}
   CHECK(at==result.command_bytes/4);CHECK(!nn_bo_destroy(result.job));
   struct nn_pipeline_draw_state bad=s;bad.texture_types=NULL;reject(d,p,a,&bad);
   bad=s;bad.sampled_buffer[2]=&view;reject(d,p,a,&bad);
   bad=s;bad.textures[1].layout=&tv;reject(d,p,a,&bad);
   bad=s;bad.textures[1].buffer=(struct nn_pipeline_buffer){texture,view.offset,view.bytes};reject(d,p,a,&bad);
   struct nn_pipeline_texture_types types=*a->textures;bad=s;bad.texture_types=&types;reject(d,p,a,&bad);
   types.dimensions[1]=2;CHECK(nn_pipeline_preflight(d,stageptr,a->recipe,&nn_pipeline_device,&bad,0)==ENOTSUP);
   types=*a->textures;types.operations[1]=NN_PIPELINE_TEX_SAMPLE;CHECK(nn_pipeline_preflight(d,stageptr,a->recipe,&nn_pipeline_device,&bad,0)==ENOTSUP);
   CHECK(nn_pipeline_preflight(d,stageptr,a->recipe,&nn_pipeline_device,&s,1u<<2)==EINVAL);
   struct nn_buffer_sampled_binding broken=view;broken.owner_bytes=1;bad=s;bad.sampled_buffer[1]=&broken;reject(d,p,a,&bad);
   broken=view;broken.offset=SIZE_MAX;bad.sampled_buffer[1]=&broken;reject(d,p,a,&bad);
   broken=view;broken.owner_offset=view.offset;
   if(prefix){bad=s;bad.sampled_buffer[1]=&broken;reject(d,p,a,&bad);}
   broken=view;broken.resource=target;broken.owner_offset=0;broken.owner_bytes=nn_bo_size(target);broken.offset=0;bad=s;bad.sampled_buffer[1]=&broken;reject(d,p,a,&bad);
   /* Exact same BO disjoint native read and target write ranges are legal. */
   broken.offset=(tv.required_span_bytes+63)&~(size_t)63;bad=s;bad.sampled_buffer[1]=&broken;
   CHECK(!nn_pipeline_encode(d,p,a->recipe,&nn_pipeline_device,&bad,&result,&q));CHECK(!nn_bo_destroy(result.job));
   /* Unknown caller submit/completion retains ALL returned resources; simulate
    * backend busy ownership only, never fake completion or execute pixels. */
   fail_cache_at=cache_calls+1;fail_destroy_at=destroy_calls+1;
   CHECK(nn_pipeline_encode(d,p,a->recipe,&nn_pipeline_device,&s,&result,&q)==EIO&&!result.job&&!result.resource_count&&q);
   CHECK(nn_bo_idle(texture)==0);CHECK(!nn_bo_destroy(q));fail_cache_at=fail_destroy_at=0;
   int untouched=1;for(size_t i=0;i<nn_bo_size(texture);i++)if(((unsigned char*)ram_map(texture))[i]!=0xa5)untouched=0;
   CHECK(untouched&&!source_map_attempts);forbidden_source_map=NULL;
   CHECK(!nn_bo_destroy(texture));
  }
  CHECK(!nn_pipeline_program_destroy(p));
 }
 /* Independent pure-owner negatives: zero length, bad format, incomplete
  * element, prefix escapes, address overflow and native capacity limits. */
 nn_bo*b=NULL;CHECK(!nn_bo_alloc(d,8192,&b));struct nn_buffer_sampled_binding good={1,46,b,0,8192,16,13*16},bad=good;
 struct nn_fdl_buffer_view v;uint32_t out[16],zeros[16]={0};CHECK(!nn_buffer_sampled_view(&good,&v)&&v.required_prefix==16&&v.elements==13);
 bad.format=65;CHECK(nn_buffer_sampled_descriptor(&bad,out)==ENOTSUP&&!memcmp(out,zeros,64));bad=good;bad.bytes=13*16-1;CHECK(nn_buffer_sampled_descriptor(&bad,out)==EINVAL&&!memcmp(out,zeros,64));
 bad=good;bad.owner_offset=16;CHECK(nn_buffer_sampled_descriptor(&bad,out)==EINVAL&&!memcmp(out,zeros,64));
 bad=good;bad.bytes=0;CHECK(nn_buffer_sampled_descriptor(&bad,out)==EINVAL&&!memcmp(out,zeros,64));
 bad=good;bad.owner_bytes=SIZE_MAX;CHECK(nn_buffer_sampled_descriptor(&bad,out)==EINVAL&&!memcmp(out,zeros,64));
 CHECK(!nn_bo_destroy(b));CHECK(!nn_bo_destroy(vertices));CHECK(!nn_bo_destroy(uniform));CHECK(!nn_bo_destroy(target));CHECK(!d->bos);CHECK(!nn_device_close(d));
 printf("{\"passed\":true,\"checks\":%u,\"encoded_jobs\":%u,\"gpu_execution\":false,\"android_execution\":false}\n",checks,jobs);return 0;
}
