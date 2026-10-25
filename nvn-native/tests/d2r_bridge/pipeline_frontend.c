/* HOST ONLY namespace/selection regression. Never executes shader code or GPU. */
#define main program_owner_fixture
#include NN_PROGRAM_OWNER_FIXTURE
#undef main
#include "nvn_native/pipeline_frontend.h"
#include "nvn_native/pipeline_source.h"
#include NN_PIPELINE_ARCHIVE
#include "pipeline_fixture_pair.h"
int main(void)
{
 nn_device *d=NULL;CHECK(!nn_device_open(&d));nn_bo *bo=NULL;CHECK(!nn_bo_alloc(d,131072,&bo));
 struct nn_pipeline_buffer api[14]={0},native[16];
 for(unsigned i=0;i<14;i++)api[i]=(struct nn_pipeline_buffer){bo,i*256,65536};
 unsigned char original[512]={0};struct nn_offline_cbuf map[2]={{3,7},{16,2}};
 struct nn_offline_graphics_stage s={.stage=0,.source=original,.source_bytes=256,.code_bytes=256,.cbufs=map,.cbuf_count=2};
 struct nn_pipeline_address_patch patches[2]={{.role=NN_PIPELINE_UBO,.stage=0,.binding=7,.offset=16,.bytes=64},
  {.role=NN_PIPELINE_UBO,.stage=0,.binding=2,.offset=32,.bytes=96}};
 struct nn_pipeline_shader_recipe r={.version=1,.chip=0x06050002,.patches=patches,.patch_count=2};
 CHECK(!nn_pipeline_frontend_uniforms(&s,&r,api,native));
 CHECK(native[7].bo==bo&&native[7].offset==0&&native[2].offset==13*256);
 for(unsigned i=0;i<16;i++)if(i!=2&&i!=7)CHECK(!native[i].bo&&!native[i].bytes&&!native[i].offset);
 /* Every legal APIindex is independent of every native binding. */
 for(unsigned slot=3;slot<=16;slot++)for(unsigned binding=0;binding<16;binding++) {
  map[0]=(struct nn_offline_cbuf){slot,binding};s.cbuf_count=1;r.patch_count=1;patches[0].binding=binding;
  CHECK(!nn_pipeline_frontend_uniforms(&s,&r,api,native));CHECK(native[binding].offset==(slot-3)*256);
 }
 for(unsigned slot=0;slot<32;slot++)if(slot<3||slot>16) {
  map[0]=(struct nn_offline_cbuf){slot,7};patches[0].binding=7;
  memset(native,0xa5,sizeof(native));CHECK(nn_pipeline_frontend_uniforms(&s,&r,api,native)==ENOTSUP);
  for(unsigned i=0;i<16;i++)CHECK(!native[i].bo&&!native[i].offset&&!native[i].bytes);
 }
 /* C1 never aliases API0/native1. It is validated and supplied by owner. */
 map[0]=(struct nn_offline_cbuf){1,7};s.source_bytes=512;
 CHECK(!nn_pipeline_frontend_uniforms(&s,&r,api,native));CHECK(!native[7].bo);
 patches[0].offset=240;patches[0].bytes=64;
 CHECK(nn_pipeline_frontend_uniforms(&s,&r,api,native)==ENOTSUP);
 patches[0].offset=16;patches[0].bytes=64;s.source_bytes=256;map[0]=(struct nn_offline_cbuf){3,7};
 struct nn_pipeline_buffer saved=api[0];api[0].offset=4;
 CHECK(nn_pipeline_frontend_uniforms(&s,&r,api,native)==EINVAL);api[0]=saved;api[0].bytes=79;
 CHECK(nn_pipeline_frontend_uniforms(&s,&r,api,native)==EINVAL);api[0]=saved;
 s.cbuf_count=2;map[1]=map[0];CHECK(nn_pipeline_frontend_uniforms(&s,&r,api,native)==EINVAL);
 map[1]=(struct nn_offline_cbuf){4,7};CHECK(nn_pipeline_frontend_uniforms(&s,&r,api,native)==EINVAL);s.cbuf_count=1;
 /* Unconsumed API slots are not made required or resources. */
 memset(api,0,sizeof(api));r.patch_count=0;CHECK(!nn_pipeline_frontend_uniforms(&s,&r,api,native));
 struct nn_pipeline_frontend_queries state={.version=1,.chip=0x06050002,.primitive=4,.point_size=1};
 nn_render_state_defaults(&state.render);CHECK(state.render.multisample.alpha_to_coverage_dither==1);
 const struct nn_offline_graphics_archive *a=NN_GRAPHICS_PIPELINE_ARCHIVE.stages;
 const struct nn_pipeline_pair_recipe *pair=pipeline_fixture_color_pair(&NN_GRAPHICS_PIPELINE_ARCHIVE,&state);
 CHECK(pair); const struct nn_offline_graphics_stage *vs=a->stages+pair->vs_index,*fs=a->stages+pair->fs_index;
 struct nn_pipeline_source_input input[2]={{0,vs->source,vs->source_bytes,vs->control,vs->control_bytes},
  {1,fs->source,fs->source_bytes,fs->control,fs->control_bytes}};struct nn_pipeline_source_program program;
 CHECK(!nn_pipeline_source_prepare(&NN_GRAPHICS_PIPELINE_ARCHIVE,input,2,&program));
 const struct nn_offline_graphics_stage *selected_vs=NULL,*selected_fs=NULL;const struct nn_pipeline_shader_recipe *recipe=NULL;
 CHECK(!nn_pipeline_source_resolve(&program,nn_pipeline_frontend_query,&state,&selected_vs,&selected_fs,&recipe));
 CHECK(selected_vs==vs&&selected_fs==fs&&recipe==pair->recipe);
 struct nn_pipeline_vertex_state vertex={.attribute_count=32,.stream_count=1};
 vertex.buffers[0]=(struct nn_pipeline_buffer){bo,0,65536};
 for(size_t i=0;i<recipe->input_count;i++)vertex.attributes[recipe->inputs[i].location]=(struct nn_pipeline_attribute){0x2e,(uint32_t)i*16,0};
 size_t nwords=0;vertex.streams[0].stride=4092;CHECK(!nn_pipeline_vertex_encode(recipe,&vertex,NULL,0,&nwords)&&nwords);
 uint32_t words[512];memset(words,0xa5,sizeof(words));vertex.streams[0].stride=4095;
 CHECK(nn_pipeline_vertex_encode(recipe,&vertex,words,512,&nwords)==EINVAL&&!nwords);
 vertex.streams[0].stride=4096;CHECK(nn_pipeline_vertex_encode(recipe,&vertex,words,512,&nwords)==ENOTSUP&&!nwords);
 for(size_t i=0;i<512;i++)CHECK(words[i]==0xa5a5a5a5u);
 api[0]=(struct nn_pipeline_buffer){bo,0,256};CHECK(!nn_pipeline_frontend_uniforms(vs,recipe,api,native));
 CHECK(native[vs->cbufs[0].binding].bo==bo);CHECK(!nn_pipeline_frontend_uniforms(fs,recipe,api,native));
 state.depth_mode=1;const struct nn_pipeline_pair_recipe *depth1=pipeline_fixture_color_pair(&NN_GRAPHICS_PIPELINE_ARCHIVE,&state);CHECK(depth1&&depth1!=pair);
 CHECK(!nn_pipeline_source_resolve(&program,nn_pipeline_frontend_query,&state,&selected_vs,&selected_fs,&recipe));
 CHECK(selected_vs==a->stages+depth1->vs_index&&selected_fs==a->stages+depth1->fs_index&&recipe==depth1->recipe);
 state.depth_mode=0;
 state.attribute_types[0]=1;CHECK(nn_pipeline_source_resolve(&program,nn_pipeline_frontend_query,&state,&selected_vs,&selected_fs,&recipe)==ENOTSUP);state.attribute_types[0]=0;
 state.render.multisample.alpha_to_coverage=1;CHECK(nn_pipeline_source_resolve(&program,nn_pipeline_frontend_query,&state,&selected_vs,&selected_fs,&recipe)==ENOTSUP);state.render.multisample.alpha_to_coverage=0;
 uint32_t values[32];size_t count=99;struct nn_pipeline_query unknown={"QuerySamplerType",3,1,1,{110,-1,0,0}};
 CHECK(nn_pipeline_frontend_query(&state,1,&unknown,values,&count)==ENOTSUP&&count==0);
 unknown=(struct nn_pipeline_query){"graphics/attribute_types",0,1,32,{0}};state.attribute_types[3]=2;
 CHECK(!nn_pipeline_frontend_query(&state,0,&unknown,values,&count)&&count==32&&values[3]==2);
 unknown.arguments=1;CHECK(nn_pipeline_frontend_query(&state,0,&unknown,values,&count)==EINVAL&&count==0);
 CHECK(!nn_bo_destroy(bo));CHECK(!d->bos);CHECK(!nn_device_close(d));
 printf("HOST ONLY pipeline frontend checks=%u; shader/GPU execution=false\n",checks);return 0;
}
