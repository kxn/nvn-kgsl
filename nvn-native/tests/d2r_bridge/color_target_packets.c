/* HOST ONLY actual common encoder + existing RAM metadata fault backend.
 * Commands are inspected, never executed. Original source/recipe fully locked. */
#define main unused_program_owner_main
#include "program_owner.c"
#undef main
#include "nvn_native/pipeline.h"
#include "nvn_native/color_target.h"
#include "nvn_native/pm4.h"
#include "../color_target_cases.h"
#include NN_COLOR_QUALIFICATION_HEADER
int nn_pipeline_encode_legacy(nn_device*,const nn_pipeline_program*,const struct nn_pipeline_shader_recipe*,const struct nn_pipeline_platform*,const struct nn_pipeline_draw_state*,struct nn_pipeline_submission*,nn_bo**);
static void normalize(uint32_t*w,size_t n,nn_bo*job,const struct nn_pipeline_platform*p)
{
 const struct nn_pipeline_platform_segment*segments[]={&p->begin,&p->end};
 for(unsigned i=0;i<2;i++){
  const struct nn_pipeline_platform_segment*s=segments[i];size_t base=i?n-s->word_count:0;
  CHECK(s->word_count<=n);
  for(size_t j=0;j<s->patch_count;j++){
   const struct nn_pipeline_platform_patch*q=s->patches+j;size_t at=base+q->word;
   CHECK(q->kind==NN_PLATFORM_ZERO||q->kind==NN_PLATFORM_SEQUENCE);CHECK(at<n-1&&q->offset==0);
   uint64_t offset=q->kind==NN_PLATFORM_ZERO?32768:45056,actual=(uint64_t)w[at]|(uint64_t)w[at+1]<<32;
   CHECK(actual==nn_bo_address(job)+offset);w[at]=(uint32_t)offset;w[at+1]=0;
  }
 }
}
static uint32_t last(const uint32_t*w,size_t n,uint32_t reg)
{uint32_t v=UINT32_MAX;for(size_t at=0;at<n;){size_t len=nvn_pm4_packet_words(w[at]);CHECK(len&&len<=n-at);if(w[at]==nvn_pm4_packet4(reg,1))v=w[at+1];at+=len;}CHECK(v!=UINT32_MAX);return v;}
int main(void)
{
 const struct nn_pipeline_archive*a=&NN_GRAPHICS_PIPELINE_ARCHIVE;const struct nn_pipeline_pair_recipe*pair=a->pairs+NN_PIPELINE_QUAL_PAIR_INDEX;
 CHECK(pair->vs_index==2&&pair->fs_index==3&&!pair->recipe->texture_mask&&!pair->recipe->sampler_mask);
 for(size_t i=0;i<pair->recipe->patch_count;i++)CHECK(pair->recipe->patches[i].role==NN_PIPELINE_CODE||pair->recipe->patches[i].role==NN_PIPELINE_UBO);
 nn_device*d=NULL;CHECK(!nn_device_open(&d));const struct nn_offline_graphics_stage*vs=a->stages->stages+pair->vs_index,*fs=a->stages->stages+pair->fs_index;
 nn_pipeline_program*p=NULL,*retained=NULL;CHECK(!nn_pipeline_program_create(d,a->stages,vs,fs,vs->specialization,fs->specialization,&p,&retained)&&!retained);
 nn_bo*input=NULL;CHECK(!nn_bo_alloc(d,4096,&input));struct nn_pipeline_draw_state state={0};
 state.vertex.attribute_count=state.vertex.stream_count=32;state.vertex.streams[3]=(struct nn_pipeline_stream){24,0};state.vertex.buffers[3]=(struct nn_pipeline_buffer){input,256,72};
 state.vertex.attributes[0]=(struct nn_pipeline_attribute){NVN_FORMAT_RG32F,0,3};state.vertex.attributes[1]=(struct nn_pipeline_attribute){NVN_FORMAT_RGBA32F,8,3};state.ubo[0][1]=(struct nn_pipeline_buffer){input,64,64};
 state.draw=(struct nn_gfx_draw){.mode=NN_GFX_DRAW_DIRECT,.primitive=4,.count=3,.instances=1};CHECK(!nn_pipeline_full_raster(73,41,0,1,&state.raster));
 unsigned packets=0;for(unsigned k=0;k<23;k++)for(unsigned mode=0;mode<3;mode++){
  uint32_t f=k?nn_color_formats[k-1]:37;struct nn_tex_native_view view;struct nn_color_target_info info;CHECK(!nn_color_target_create(0x06050002,f,73,41,0,&view,&info));
  nn_bo*target=NULL;CHECK(!nn_bo_alloc(d,8192+view.required_span_bytes,&target));state.target=(struct nn_pipeline_image){&view,{target,4096,view.required_span_bytes}};
  nn_render_state_defaults(&state.render);if(mode==1){state.render.blend_enable=1;state.render.blend[0].src_rgb=NVN_BLEND_FUNC_SRC_ALPHA;state.render.blend[0].dst_rgb=NVN_BLEND_FUNC_ONE_MINUS_SRC_ALPHA;}if(mode==2)state.render.color_mask=(state.render.color_mask&~15u)|5u;
  struct nn_pipeline_submission output={0};nn_bo*job=NULL;CHECK(!nn_pipeline_encode(d,p,pair->recipe,a->platform,&state,&output,&job)&&!job);
  const uint32_t*w=nn_bo_map(output.job);size_t n=output.command_bytes/4;
  CHECK(last(w,n,0x8822)==view.rb_mrt_buf_info);CHECK(last(w,n,0xa996)==view.sp_ps_mrt_reg);CHECK(last(w,n,0x8102)==info.color_format);CHECK(last(w,n,0x880f)==info.srgb);CHECK(last(w,n,0xa98a)==info.srgb);
  CHECK(((last(w,n,0x8820)>>7)&15)==(mode==2?5u:15u));CHECK((last(w,n,0xa989)&1)==(mode==1));
  if(!k){
   struct nn_pipeline_submission legacy={0};CHECK(!nn_pipeline_encode_legacy(d,p,pair->recipe,a->platform,&state,&legacy,&job)&&!job);CHECK(legacy.command_bytes==output.command_bytes&&legacy.resource_count==output.resource_count);
   uint32_t x[8192],y[8192];CHECK(n<=8192);memcpy(x,w,n*4);memcpy(y,nn_bo_map(legacy.job),n*4);normalize(x,n,output.job,a->platform);normalize(y,n,legacy.job,a->platform);CHECK(!memcmp(x,y,n*4));CHECK(!nn_bo_destroy(legacy.job));
  }
  CHECK(!nn_bo_destroy(output.job));CHECK(!nn_bo_destroy(target));packets++;
 }
 CHECK(!nn_bo_destroy(input));CHECK(!nn_pipeline_program_destroy(p));CHECK(!d->bos);CHECK(!nn_device_close(d));
 printf("{\"passed\":true,\"host_only\":true,\"checks\":%u,\"encoded_states\":%u,\"default_rgba8_normalized_commands_equal\":true,\"gpu_execution\":false}\n",checks,packets);return 0;
}
