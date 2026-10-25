/* HOST only: real production metadata/encoders, RAM never executes a shader. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_alloc ram_alloc
#define nn_bo_map ram_map
#define nn_bo_destroy ram_destroy
#include NN_MEMORY_BACKEND
#undef nn_bo_alloc
#undef nn_bo_map
#undef nn_bo_destroy
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/pipeline_runtime_constants.h"
#include "nvn_native/pipeline.h"
#include "nvn_native/pm4.h"
#include NN_RUNTIME_NATIVE_ARTIFACT
#include NN_RUNTIME_PLATFORM
static unsigned checks,allocs,destroys,fail_alloc_at,fail_destroy_at,map_null_new,jobs;
static nn_bo*last_alloc,*deny_map[3];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%u: %s\n",__LINE__,#x);abort();}}while(0)
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){++allocs;*out=NULL;if(allocs==fail_alloc_at)return ENOMEM;int e=ram_alloc(d,n,out);last_alloc=*out;return e;}
void*nn_bo_map(const nn_bo*b){for(unsigned i=0;i<3;i++)CHECK(!b||b!=deny_map[i]);return map_null_new&&b==last_alloc?NULL:ram_map(b);}
int nn_bo_destroy(nn_bo*b){if(++destroys==fail_destroy_at)return EIO;return ram_destroy(b);}
#ifndef NN_TEST_RUNTIME_OFF
static struct nn_tex_native_view target_view(void){
 struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=73,.height=41,.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=NN_FDL_SAMPLED|NN_FDL_COLOR_TARGET};struct nn_fdl_image_view a;CHECK(!nn_fdl_image(&d,&a));
 struct nn_tex_native_view v={.schema=1,.chip=a.chip,.format=a.format,.target=1,.levels=1,.layers=1,.samples=1,.width=a.width,.height=a.height,.pitch=a.level[0].pitch,.layer_bytes=a.level[0].layer_stride,.bytes=a.bytes,.alignment=a.alignment,.required_span_bytes=a.required_span,.descriptor_address_word=a.descriptor_address_word,.descriptor_address_mask=a.descriptor_address_mask,.rb_mrt_buf_info=a.rb_mrt_buf_info,.rb_mrt_pitch=a.rb_mrt_pitch,.rb_mrt_array_pitch=a.rb_mrt_array_pitch,.sp_ps_mrt_reg=a.sp_ps_mrt_reg};memcpy(v.texture_descriptor,a.descriptor,64);return v;
}
static void reject(nn_device*d,nn_pipeline_program*p,const struct nn_pipeline_draw_state*s){struct nn_pipeline_submission out;nn_bo*kept=(void*)1;unsigned before=allocs;CHECK(nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,s,&out,&kept)!=0&&!out.job&&!kept&&!out.resource_count&&allocs==before);}
static void inspect(const struct nn_pipeline_submission*out,const struct nn_pipeline_draw_state*s){
 const uint32_t*w=ram_map(out->job);unsigned params=0,tables[2]={0},draws=0;const struct nn_pipeline_runtime_layout*l=nn_runtime_constants.layout;
 for(size_t at=0;at<out->command_bytes/4;){unsigned n=nvn_pm4_packet_words(w[at]);CHECK(n&&n<=out->command_bytes/4-at);unsigned op=(w[at]>>16)&127u;
  if((w[at]>>28)==7&&(op==50||op==52)){
   unsigned stage=op==52;uint32_t ctl=w[at+1];unsigned type=(ctl>>14)&3;
   if(type==2){tables[stage]++;CHECK(n==4+2*l->ubo_entries[stage]&&(ctl>>22)==l->ubo_entries[stage]);
    for(unsigned b=0;b<l->ubo_entries[stage];b++){
     uint64_t desc=(uint64_t)w[at+4+2*b]|(uint64_t)w[at+5+2*b]<<32;
     if(!(l->ubo_mask[stage]&(1u<<b))){CHECK(!desc);continue;}
     const struct nn_pipeline_buffer*v=&s->ubo[stage][b];uint64_t address=nn_bo_address(v->bo)+v->offset;
     CHECK(desc==(address|((uint64_t)(v->bytes/16)<<49)));
     unsigned refs=0;for(size_t i=0;i<out->resource_count;i++)refs+=out->resources[i]==v->bo;CHECK(refs==1);
    }
   }else if(!stage&&l->driver_dwords[0]&&type==1&&(ctl&16383u)==l->driver_offset_vec4[0]){
    params++;CHECK(n==4&&((ctl>>16)&3)==2&&(ctl>>22)==4);uint64_t a=(uint64_t)w[at+2]|(uint64_t)w[at+3]<<32;
    CHECK(a>=nn_bo_address(out->job)&&a-nn_bo_address(out->job)<=nn_bo_size(out->job)-64);
    const uint32_t*p=(const uint32_t*)((const char*)ram_map(out->job)+(size_t)(a-nn_bo_address(out->job)));
    for(unsigned k=0;k<16;k++){uint32_t expected=0;if(s->draw.mode==NN_GFX_DRAW_DIRECT){if(k==0)expected=s->runtime_draw_id;if(k==1)expected=s->draw.indexed?(uint32_t)s->draw.base_vertex:s->draw.first;if(k==2)expected=s->draw.base_instance;}CHECK(p[k]==expected);}
   }
  }
  if((w[at]>>28)==7&&(op==0x38||op==0x2a)){draws++;CHECK((op==0x38)==(s->draw.mode==NN_GFX_DRAW_DIRECT));if(op==0x2a)CHECK(((w[at+2]>>8)&16383u)==(l->driver_dwords[0]?l->driver_offset_vec4[0]:0));}
  at+=n;
 }CHECK(params==!!l->driver_dwords[0]&&tables[0]==!!l->ubo_entries[0]&&tables[1]==!!l->ubo_entries[1]&&draws==1);
}
#endif
int main(void){
 nn_device*d=NULL;CHECK(!nn_device_open(&d));nn_pipeline_program*p=NULL,*kept=NULL;unsigned before=allocs;
 CHECK(nn_pipeline_program_create_native(d,&NN_RUNTIME_ARTIFACT,&p,&kept)==ENOTSUP&&!p&&!kept&&allocs==before);
#ifdef NN_TEST_RUNTIME_OFF
 CHECK(nn_pipeline_program_create_native_runtime(d,&NN_RUNTIME_ARTIFACT,NULL,NULL,&nn_runtime_constants,&p,&kept)==ENOTSUP&&!p&&!kept&&allocs==before);
#else
 struct nn_pipeline_runtime_constants wrong=nn_runtime_constants;wrong.spirv_bytes[0]-=4;
 CHECK(nn_pipeline_program_create_native_runtime(d,&NN_RUNTIME_ARTIFACT,NULL,NULL,&wrong,&p,&kept)==EINVAL&&!p&&!kept&&allocs==before);
 struct nn_pipeline_runtime_layout badlayout=*nn_runtime_constants.layout;wrong=nn_runtime_constants;wrong.layout=&badlayout;
 if(badlayout.driver_dwords[0])badlayout.driver_mask[0]|=16;else badlayout.ubo_mask[0]|=1;
 CHECK(nn_pipeline_program_create_native_runtime(d,&NN_RUNTIME_ARTIFACT,NULL,NULL,&wrong,&p,&kept)!=0&&!p&&!kept&&allocs==before);
 CHECK(!nn_pipeline_program_create_native_runtime(d,&NN_RUNTIME_ARTIFACT,NULL,NULL,&nn_runtime_constants,&p,&kept)&&p&&!kept&&nn_pipeline_program_runtime_constants(p)==&nn_runtime_constants);
 struct nn_tex_native_view view=target_view();nn_bo*target=NULL,*vertices=NULL,*ubos[2]={0},*args[3]={0};CHECK(!nn_bo_alloc(d,view.required_span_bytes,&target));CHECK(!nn_bo_alloc(d,4096,&vertices));
 for(unsigned i=0;i<2;i++){CHECK(!nn_bo_alloc(d,32768,ubos+i));memset(ram_map(ubos[i]),0x31+i,32768);}
 for(unsigned i=0;i<3;i++){CHECK(!nn_bo_alloc(d,4096,args+i));memset(ram_map(args[i]),0xa1+i,4096);deny_map[i]=args[i];}
 struct nn_pipeline_draw_state s={.target={&view,{target,0,view.required_span_bytes}},.texture_types=NN_RUNTIME_ARTIFACT.textures,.runtime_constants=&nn_runtime_constants};
 CHECK(!nn_pipeline_full_raster(73,41,0,1,&s.raster));nn_render_state_defaults(&s.render);s.vertex.attribute_count=1;s.vertex.stream_count=1;s.vertex.streams[0]=(struct nn_pipeline_stream){8,0};s.vertex.buffers[0]=(struct nn_pipeline_buffer){vertices,0,4096};s.vertex.attributes[0]=(struct nn_pipeline_attribute){0x16,0,0};
 for(unsigned stage=0;stage<2;stage++)for(unsigned b=0;b<16;b++)if(nn_runtime_constants.layout->ubo_mask[stage]&(1u<<b))s.ubo[stage][b]=(struct nn_pipeline_buffer){ubos[stage],0,16384};
 const struct nn_offline_graphics_stage*stages[2];for(unsigned i=0;i<2;i++){struct nn_pipeline_stage_view v;CHECK(!nn_pipeline_program_stage(p,i,&v));stages[i]=v.source;}
 const unsigned offsets[]={0,16,256,4096};
 for(unsigned mode=0;mode<3;mode++)for(unsigned indexed=0;indexed<2;indexed++)for(unsigned scenario=0;scenario<4;scenario++)for(unsigned replay=0;replay<2;replay++){
  s.draw=(struct nn_gfx_draw){.mode=mode,.primitive=4,.indexed=indexed,.index_type=1,.count=3,.instances=2,.first=scenario,.base_vertex=-(int32_t)(scenario*7),.base_instance=13*scenario};
  if(indexed)s.draw.index=(struct nn_gfx_buffer){args[0],0,4096};
  if(mode){s.draw.indirect=(struct nn_gfx_buffer){args[1],0,4096};s.draw.max_draws=3;s.draw.stride=32;if(mode==2)s.draw.indirect_count=(struct nn_gfx_buffer){args[2],0,4};}
  s.runtime_draw_id=mode||!nn_runtime_constants.layout->driver_dwords[0]?0:scenario*5;
  for(unsigned stage=0;stage<2;stage++)for(unsigned b=0;b<16;b++)if(s.ubo[stage][b].bo)s.ubo[stage][b].offset=offsets[scenario];
  CHECK(!nn_pipeline_preflight(d,stages,&nn_pipeline_recipe,&nn_pipeline_device,&s,0));
  struct nn_pipeline_submission out;nn_bo*job_kept=NULL;CHECK(!nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,&s,&out,&job_kept)&&out.job&&!job_kept);jobs++;inspect(&out,&s);
  /* Simulated unknown submit ownership: no RAM execution or fake completion. */
  for(size_t i=0;i<out.resource_count;i++)out.resources[i]->busy++;
  CHECK(nn_pipeline_program_destroy(p)==EBUSY&&nn_bo_destroy(out.job)==EBUSY);
  for(unsigned stage=0;stage<2;stage++)if(nn_runtime_constants.layout->ubo_mask[stage])CHECK(nn_bo_destroy(ubos[stage])==EBUSY);
  if(mode){CHECK(nn_bo_destroy(args[1])==EBUSY);}
  if(mode==2){CHECK(nn_bo_destroy(args[2])==EBUSY);}
  if(indexed){CHECK(nn_bo_destroy(args[0])==EBUSY);}
  for(size_t i=0;i<out.resource_count;i++)out.resources[i]->busy--;
  CHECK(!nn_bo_destroy(out.job));
 }
 struct nn_pipeline_draw_state bad=s;bad.runtime_constants=NULL;reject(d,p,&bad);bad=s;bad.runtime_constants=&wrong;reject(d,p,&bad);bad=s;bad.runtime_draw_id=1;reject(d,p,&bad);
 for(unsigned stage=0;stage<2;stage++)for(unsigned b=0;b<16;b++)if(s.ubo[stage][b].bo){
  bad=s;bad.ubo[stage][b]=(struct nn_pipeline_buffer){target,0,16384};reject(d,p,&bad);
  bad=s;bad.ubo[stage][b].bytes=0;reject(d,p,&bad);bad=s;bad.ubo[stage][b].bytes=65552;reject(d,p,&bad);bad=s;bad.ubo[stage][b].bytes=16383;reject(d,p,&bad);bad=s;bad.ubo[stage][b].offset=4;reject(d,p,&bad);bad=s;bad.ubo[stage][b].offset=32768;reject(d,p,&bad);
 }
 unsigned n=d->bos;struct nn_pipeline_submission out;nn_bo*job_kept=NULL;fail_alloc_at=allocs+1;CHECK(nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,&s,&out,&job_kept)==ENOMEM&&!out.job&&!job_kept&&d->bos==n);fail_alloc_at=0;
 map_null_new=1;fail_destroy_at=destroys+1;CHECK(nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,&s,&out,&job_kept)!=0&&!out.job&&job_kept&&d->bos==n+1);map_null_new=0;fail_destroy_at=0;CHECK(!nn_bo_destroy(job_kept)&&d->bos==n);
 for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<32768;i++)CHECK(((unsigned char*)ram_map(ubos[stage]))[i]==0x31+stage);
 for(unsigned a=0;a<3;a++)for(unsigned i=0;i<4096;i++)CHECK(((unsigned char*)ram_map(args[a]))[i]==0xa1+a);
 CHECK(!nn_pipeline_program_destroy(p));CHECK(!nn_bo_destroy(target)&&!nn_bo_destroy(vertices));for(unsigned i=0;i<2;i++)CHECK(!nn_bo_destroy(ubos[i]));for(unsigned i=0;i<3;i++)CHECK(!nn_bo_destroy(args[i]));
#endif
 CHECK(!d->bos);CHECK(!nn_device_close(d));printf("{\"checks\":%u,\"jobs\":%u,\"gpu_execution\":false}\n",checks,jobs);return 0;
}
