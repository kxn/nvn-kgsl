/* HOST metadata/owner qualification only. This RAM backend executes no GPU. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_alloc ram_alloc
#define nn_bo_map ram_map
#define nn_bo_destroy ram_destroy
#include NN_MEMORY_BACKEND
#undef nn_bo_alloc
#undef nn_bo_map
#undef nn_bo_destroy
#include "nvn_native/pipeline_storage.h"
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/pipeline.h"
#include "nvn_native/pm4.h"
#include NN_STORAGE_NATIVE_ARTIFACT
#include NN_STORAGE_PLATFORM
static unsigned checks,allocs,destroys,fail_alloc_at,fail_destroy_at,map_null_new,jobs;
static nn_bo*last_alloc;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%u: %s\n",__LINE__,#x);abort();}}while(0)
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){++allocs;*out=NULL;if(allocs==fail_alloc_at)return ENOMEM;int e=ram_alloc(d,n,out);last_alloc=*out;return e;}
void*nn_bo_map(const nn_bo*b){return map_null_new&&b==last_alloc?NULL:ram_map(b);}
int nn_bo_destroy(nn_bo*b){if(++destroys==fail_destroy_at)return EIO;return ram_destroy(b);}
#ifndef NN_TEST_STORAGE_OFF
static struct nn_tex_native_view target_view(void){
 struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=73,.height=41,.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=NN_FDL_SAMPLED|NN_FDL_COLOR_TARGET};struct nn_fdl_image_view a;CHECK(!nn_fdl_image(&d,&a));
 struct nn_tex_native_view v={.schema=1,.chip=a.chip,.format=a.format,.target=1,.levels=1,.layers=1,.samples=1,.width=a.width,.height=a.height,.pitch=a.level[0].pitch,.layer_bytes=a.level[0].layer_stride,.bytes=a.bytes,.alignment=a.alignment,.required_span_bytes=a.required_span,.descriptor_address_word=a.descriptor_address_word,.descriptor_address_mask=a.descriptor_address_mask,.rb_mrt_buf_info=a.rb_mrt_buf_info,.rb_mrt_pitch=a.rb_mrt_pitch,.rb_mrt_array_pitch=a.rb_mrt_array_pitch,.sp_ps_mrt_reg=a.sp_ps_mrt_reg};memcpy(v.texture_descriptor,a.descriptor,64);return v;
}
static void reject(nn_device*d,nn_pipeline_program*p,const struct nn_pipeline_draw_state*s){
 struct nn_pipeline_submission out;nn_bo*kept=(void*)1;unsigned before=allocs;
 CHECK(nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,s,&out,&kept)!=0&&!out.job&&!kept&&!out.resource_count&&allocs==before);
}
static void inspect(const struct nn_pipeline_submission*s,const struct nn_pipeline_draw_state*state){
 const uint32_t*w=ram_map(s->job);unsigned shared=0,reg=0,draws=0;uint64_t table=0;
 for(size_t i=0;i<s->command_bytes/4;){size_t n=nvn_pm4_packet_words(w[i]);CHECK(n&&n<=s->command_bytes/4-i);
  if(w[i]==nvn_pm4_packet4(0xab1a,2)){table=(uint64_t)w[i+1]|(uint64_t)w[i+2]<<32;reg++;}
  if(w[i]==nvn_pm4_packet7(0x36,3)){
   uint32_t ctl=w[i+1];if(((ctl>>18)&15)==14){shared++;CHECK((ctl&0x3ffffu)==(2u<<16));CHECK(table==((uint64_t)w[i+2]|(uint64_t)w[i+3]<<32));}
  }
  if((w[i]>>28)==7&&((w[i]>>16)&127)==0x38)draws++;
  i+=n;
 }CHECK(shared==1&&reg==1&&draws==1&&table>=nn_bo_address(s->job)&&table-nn_bo_address(s->job)<=nn_bo_size(s->job)-2048);
 const uint32_t*t=(const uint32_t*)((char*)ram_map(s->job)+(size_t)(table-nn_bo_address(s->job)));
 for(unsigned stage=0;stage<2;stage++)for(unsigned b=0;b<16;b++){
  const uint32_t*desc=t+(16*stage+b)*16;
  if(!(state->storage_types->mask[stage]&(1u<<b))){for(unsigned i=0;i<16;i++)CHECK(desc[i]==0);continue;}
  struct nn_pipeline_buffer v=state->storage[stage][b];uint64_t address=nn_bo_address(v.bo)+v.offset;
  CHECK(((uint64_t)desc[4]|(uint64_t)desc[5]<<32)==(address&~UINT64_C(63)));
  unsigned refs=0;for(size_t i=0;i<s->resource_count;i++)refs+=s->resources[i]==v.bo;CHECK(refs==1);
 }
}
#endif
int main(void){
 nn_device*d=NULL;CHECK(!nn_device_open(&d));nn_pipeline_program*p=NULL,*kept=NULL;unsigned before=allocs;
 CHECK(nn_pipeline_program_create_native(d,&NN_STORAGE_ARTIFACT,&p,&kept)==ENOTSUP&&!p&&!kept&&allocs==before);
#ifdef NN_TEST_STORAGE_OFF
 CHECK(nn_pipeline_program_create_native_storage(d,&NN_STORAGE_ARTIFACT,NULL,&nn_storage_types,&p,&kept)==ENOTSUP&&!p&&!kept&&allocs==before);
#else
 struct nn_pipeline_storage_types wrong=nn_storage_types;wrong.operations[0][0]=8;
 CHECK(nn_pipeline_program_create_native_storage(d,&NN_STORAGE_ARTIFACT,NULL,&wrong,&p,&kept)==EINVAL&&!p&&!kept&&allocs==before);
 CHECK(!nn_pipeline_program_create_native_storage(d,&NN_STORAGE_ARTIFACT,NULL,&nn_storage_types,&p,&kept)&&p&&!kept&&nn_pipeline_program_storage_types(p)==&nn_storage_types);
 struct nn_tex_native_view view=target_view();nn_bo*target=NULL,*vertices=NULL,*storage[2]={0};CHECK(!nn_bo_alloc(d,view.required_span_bytes,&target));CHECK(!nn_bo_alloc(d,4096,&vertices));for(unsigned i=0;i<2;i++){CHECK(!nn_bo_alloc(d,4096,storage+i));memset(ram_map(storage[i]),0x31+i,4096);}
 struct nn_pipeline_draw_state s={.target={&view,{target,0,view.required_span_bytes}},.texture_types=NN_STORAGE_ARTIFACT.textures,.storage_types=&nn_storage_types};
 CHECK(!nn_pipeline_full_raster(73,41,0,1,&s.raster));nn_render_state_defaults(&s.render);s.vertex.attribute_count=1;s.vertex.stream_count=1;s.vertex.streams[0]=(struct nn_pipeline_stream){8,0};s.vertex.buffers[0]=(struct nn_pipeline_buffer){vertices,0,4096};s.vertex.attributes[0]=(struct nn_pipeline_attribute){0x16,0,0};s.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
 for(unsigned stage=0;stage<2;stage++)for(unsigned b=0;b<16;b++)if(nn_storage_types.mask[stage]&(1u<<b))s.storage[stage][b]=(struct nn_pipeline_buffer){storage[stage],4+stage*4,1024};
 const struct nn_offline_graphics_stage*stages[2];for(unsigned i=0;i<2;i++){struct nn_pipeline_stage_view v;CHECK(!nn_pipeline_program_stage(p,i,&v));stages[i]=v.source;}
 CHECK(!nn_pipeline_preflight(d,stages,&nn_pipeline_recipe,&nn_pipeline_device,&s,0));
 uint32_t words[4096];CHECK(nn_pipeline_recipe.word_count<=4096);memcpy(words,nn_pipeline_recipe.words,nn_pipeline_recipe.word_count*4);struct nn_pipeline_shader_recipe malformed=nn_pipeline_recipe;malformed.words=words;
 unsigned at=0;for(;at<malformed.word_count;){if(words[at]==nvn_pm4_packet7(0x36,3))break;at+=nvn_pm4_packet_words(words[at]);}CHECK(at<malformed.word_count);words[at+1]&=~(3u<<16);CHECK(nn_pipeline_shader_preflight(stages,&malformed,s.ubo)==EINVAL);words[at+1]=nn_pipeline_recipe.words[at+1];words[at+1]|=1;CHECK(nn_pipeline_shader_preflight(stages,&malformed,s.ubo)==EINVAL);
 struct nn_pipeline_submission first={0};
 for(unsigned replay=0;replay<2;replay++){
  struct nn_pipeline_submission out;nn_bo*keep=NULL;CHECK(!nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,&s,&out,&keep)&&out.job&&!keep);jobs++;inspect(&out,&s);
  for(size_t i=0;i<out.resource_count;i++)out.resources[i]->busy++;
  CHECK(nn_pipeline_program_destroy(p)==EBUSY&&nn_bo_destroy(storage[0])==EBUSY&&nn_bo_destroy(storage[1])==EBUSY);
  for(size_t i=0;i<out.resource_count;i++)out.resources[i]->busy--;
  if(!replay)first=out;else{CHECK(first.job!=out.job);CHECK(!nn_bo_destroy(first.job));CHECK(!nn_bo_destroy(out.job));}
 }
 for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<4096;i++)CHECK(((unsigned char*)ram_map(storage[stage]))[i]==0x31+stage);
 struct nn_pipeline_draw_state bad=s;bad.storage_types=NULL;reject(d,p,&bad);bad=s;bad.storage_types=&wrong;reject(d,p,&bad);
 for(unsigned stage=0;stage<2;stage++)for(unsigned b=0;b<16;b++)if(nn_storage_types.mask[stage]&(1u<<b)){
  bad=s;bad.storage[stage][b].bytes=0;reject(d,p,&bad);bad=s;bad.storage[stage][b].offset=4092;reject(d,p,&bad);bad=s;bad.storage[stage][b].offset=1;reject(d,p,&bad);
  bad=s;bad.storage[stage][b].bo=target;reject(d,p,&bad);
  if(nn_storage_types.operations[stage][b]&2){for(unsigned st=0;st<2;st++){struct nn_pipeline_stage_view v;CHECK(!nn_pipeline_program_stage(p,st,&v));bad=s;bad.storage[stage][b]=(struct nn_pipeline_buffer){v.code,0,4};reject(d,p,&bad);if(v.c1){bad=s;bad.storage[stage][b]=(struct nn_pipeline_buffer){v.c1,0,4};reject(d,p,&bad);}}bad=s;bad.vertex.buffers[0].bo=storage[stage];reject(d,p,&bad);for(unsigned st=0;st<2;st++)for(unsigned i=0;i<16;i++)if((st!=stage||i!=b)&&(nn_storage_types.mask[st]&(1u<<i))){bad=s;bad.storage[st][i].bo=storage[stage];reject(d,p,&bad);}}
 }
 unsigned n=d->bos;fail_alloc_at=allocs+1;struct nn_pipeline_submission out;nn_bo*job_kept=NULL;CHECK(nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,&s,&out,&job_kept)==ENOMEM&&!out.job&&!job_kept&&d->bos==n);fail_alloc_at=0;
 map_null_new=1;fail_destroy_at=destroys+1;CHECK(nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,&s,&out,&job_kept)!=0&&!out.job&&job_kept&&d->bos==n+1);map_null_new=0;fail_destroy_at=0;CHECK(!nn_bo_destroy(job_kept)&&d->bos==n);
 CHECK(!nn_pipeline_program_destroy(p));CHECK(!nn_bo_destroy(target)&&!nn_bo_destroy(vertices));for(unsigned i=0;i<2;i++)CHECK(!nn_bo_destroy(storage[i]));
#endif
 CHECK(!d->bos);CHECK(!nn_device_close(d));printf("{\"checks\":%u,\"jobs\":%u,\"gpu_execution\":false}\n",checks,jobs);return 0;
}
