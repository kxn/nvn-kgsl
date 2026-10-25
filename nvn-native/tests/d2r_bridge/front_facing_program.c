/* SPDX-License-Identifier: MIT. Actual native Program/relocation/owner, HOST no GPU. */
#define _POSIX_C_SOURCE 200809L
#include NN_MEMORY_BACKEND
#include "nvn_native/pipeline.h"
#include "nvn_native/pm4.h"
#include NN_FRONT_NATIVE_VIEWS
#include NN_FRONT_NATIVE_ARTIFACT
static unsigned checks;
#define CHECK(c)do{checks++;if(!(c)){fprintf(stderr,"face-owner:%u %s\n",__LINE__,#c);return 1;}}while(0)
/* Complete actual encoder matrix. RAM is only a BO owner/packet sink: it
 * never executes packets, rasterizes, evaluates FACE, or fabricates pixels. */
static uint32_t reg_last(const uint32_t *w,size_t words,unsigned reg,unsigned *hits)
{
 uint32_t value=0;*hits=0;
 for(size_t at=0;at<words;){size_t n=nvn_pm4_packet_words(w[at]);
  if(!n||n>words-at)return UINT32_MAX;
  if((w[at]>>28)==4){unsigned base=(w[at]>>8)&0x7ffffu;
   if(reg>=base&&(size_t)(reg-base)+1<n){value=w[at+1+reg-base];(*hits)++;}}
  at+=n;
 }
 return value;
}
static int matrix(nn_device*d,nn_pipeline_program*p)
{
 const struct nn_tex_native_view *v=NN_TEXTURE_NATIVE_VIEWS;
 CHECK(v->format==37&&v->width==64&&v->height==64);
 nn_bo *target=NULL,*vertex=NULL,*index=NULL;
 CHECK(!nn_bo_alloc(d,v->required_span_bytes+8192,&target));
 CHECK(!nn_bo_alloc(d,4096,&vertex));CHECK(!nn_bo_alloc(d,4096,&index));
 memset(nn_bo_map(target),0xa5,nn_bo_size(target));memset(nn_bo_map(vertex),0x76,4096);memset(nn_bo_map(index),0xc7,4096);
 const uint16_t indices[]={0,1,2};memcpy((unsigned char*)nn_bo_map(index)+64,indices,sizeof(indices));
 struct nn_pipeline_stage_view stages[2];const struct nn_offline_graphics_stage *sources[2];
 for(unsigned i=0;i<2;i++){CHECK(!nn_pipeline_program_stage(p,i,stages+i));sources[i]=stages[i].source;}
 unsigned jobs=0;
 for(unsigned origin=0;origin<2;origin++)for(unsigned front=0;front<2;front++)for(unsigned cull=0;cull<4;cull++)
 for(unsigned winding=0;winding<2;winding++)for(unsigned indexed=0;indexed<2;indexed++)for(unsigned replay=0;replay<2;replay++){
  const float ccw[]={-1,-1,3,-1,-1,3};float vertices[6];memcpy(vertices,ccw,sizeof(vertices));
  if(winding^replay){memcpy(vertices+2,ccw+4,8);memcpy(vertices+4,ccw+2,8);}
  memcpy((unsigned char*)nn_bo_map(vertex)+256,vertices,sizeof(vertices));
  struct nn_pipeline_draw_state state={.target={v,{target,4096,v->required_span_bytes}},.texture_types=&nn_front_types};
  CHECK(!nn_pipeline_full_raster(v->width,v->height,origin,0,&state.raster));nn_render_state_defaults(&state.render);
  state.render.front_face=front;state.render.cull_face=cull;
  state.vertex.attribute_count=1;state.vertex.stream_count=1;
  state.vertex.attributes[0]=(struct nn_pipeline_attribute){0x16,0,0}; /* public NVN RG32F */
  state.vertex.streams[0]=(struct nn_pipeline_stream){8,0};state.vertex.buffers[0]=(struct nn_pipeline_buffer){vertex,256,sizeof(vertices)};
  state.draw=(struct nn_gfx_draw){.mode=NN_GFX_DRAW_DIRECT,.primitive=4,.indexed=indexed,.index_type=1,.count=3,.instances=1};
  if(indexed)state.draw.index=(struct nn_gfx_buffer){index,64,sizeof(indices)};
  unsigned before=d->bos;CHECK(!nn_pipeline_preflight(d,sources,&nn_pipeline_recipe,&nn_pipeline_device,&state,0)&&d->bos==before);
  struct nn_pipeline_submission out={0};nn_bo*retained=(void*)1;
  CHECK(!nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,&state,&out,&retained)&&out.job&&!retained);
  CHECK(out.command_bytes&&out.command_bytes<32768&&out.resource_count==5+indexed);
  uint32_t*words=nn_bo_map(out.job);unsigned draws=0;
  for(size_t at=0;at<out.command_bytes/4;){size_t n=nvn_pm4_packet_words(words[at]);CHECK(n&&n<=out.command_bytes/4-at);
   if((words[at]>>28)==7&&((words[at]>>16)&127)==0x38){CHECK(n==(indexed?8u:4u)&&words[at+2]==1&&words[at+3]==3);draws++;}
   at+=n;
  }
  CHECK(draws==1);unsigned hits=0;
  CHECK((reg_last(words,out.command_bytes/4,0x8090,&hits)&7)==(cull|((front==NVN_FRONT_FACE_CW)<<2))&&hits);
  CHECK((reg_last(words,out.command_bytes/4,0xa98c,&hits)&255)==0&&hits); /* actual FACE half0 */
  CHECK((reg_last(words,out.command_bytes/4,0x8005,&hits)&1024)&&hits);
  CHECK((reg_last(words,out.command_bytes/4,0x8809,&hits)&8)&&hits);
  CHECK((reg_last(words,out.command_bytes/4,0x880a,&hits)&4)&&hits); /* need_size */
  unsigned required[4]={0};
  for(size_t i=0;i<out.resource_count;i++){
   nn_bo*b=out.resources[i];CHECK(b);for(size_t j=0;j<i;j++)CHECK(b!=out.resources[j]);
   required[0]+=b==target;required[1]+=b==vertex;required[2]+=b==index;required[3]+=b==out.job;b->busy++;
  }
  CHECK(required[0]==1&&required[1]==1&&required[2]==indexed&&required[3]==1);
  CHECK(nn_pipeline_program_destroy(p)==EBUSY&&nn_bo_destroy(target)==EBUSY&&nn_bo_destroy(vertex)==EBUSY&&nn_bo_destroy(out.job)==EBUSY);
  if(indexed)CHECK(nn_bo_destroy(index)==EBUSY);
  /* Recorded commands are independent from later raster/frontend values. */
  uint32_t saved[8192];memcpy(saved,words,out.command_bytes);state.render.front_face^=1;state.render.cull_face^=3;state.raster.window_origin^=1;
  CHECK(!memcmp(saved,words,out.command_bytes));
  for(size_t i=0;i<out.resource_count;i++)out.resources[i]->busy--;
  CHECK(!nn_bo_destroy(out.job));
  for(size_t i=0;i<nn_bo_size(target);i++)CHECK(((unsigned char*)nn_bo_map(target))[i]==0xa5);
  CHECK(!memcmp((unsigned char*)nn_bo_map(vertex)+256,vertices,sizeof(vertices)));
  CHECK(!memcmp((unsigned char*)nn_bo_map(index)+64,indices,sizeof(indices)));
  /* Fail before allocation: actual enum bounds, target range and typed identity. */
  state.raster.window_origin=2;out=(struct nn_pipeline_submission){0};retained=NULL;before=d->bos;
  CHECK(nn_pipeline_encode(d,p,&nn_pipeline_recipe,&nn_pipeline_device,&state,&out,&retained)==EINVAL&&!out.job&&!retained&&d->bos==before);
  jobs++;
 }
 CHECK(jobs==128);CHECK(!nn_bo_destroy(index));CHECK(!nn_bo_destroy(vertex));CHECK(!nn_bo_destroy(target));return 0;
}
int main(void)
{
 nn_device*d=NULL;CHECK(!nn_device_open(&d));nn_pipeline_program*p=NULL,*retained=NULL;
 CHECK(!nn_pipeline_program_create_native(d,&NN_FRONT_FACING_ARTIFACT,&p,&retained)&&p&&!retained);
 nn_bo*bos[4]={0};size_t count=0;CHECK(!nn_pipeline_program_resources(p,bos,4,&count)&&count==2&&bos[0]!=bos[1]);
 CHECK(!memcmp(nn_bo_map(bos[0]),nn_pipeline_recipe.code[0],nn_pipeline_recipe.code_bytes[0]));CHECK(!memcmp(nn_bo_map(bos[1]),nn_pipeline_recipe.code[1],nn_pipeline_recipe.code_bytes[1]));
 CHECK(nn_pipeline_program_texture_types(p)==&nn_front_types);
 nn_bo*job=NULL;CHECK(!nn_bo_alloc(d,4096,&job));struct nn_pipeline_shader_bindings binding={.job=job};size_t words=99;
 CHECK(!nn_pipeline_shader_encode(p,&nn_pipeline_recipe,&binding,1024,&words)&&words==nn_pipeline_recipe.word_count);
 nn_bo*refs[4]={0};CHECK(!nn_pipeline_shader_resources(p,&nn_pipeline_recipe,&binding,refs,4,&count)&&count==3);
 for(unsigned j=0;j<2;j++){unsigned found=0;for(unsigned i=0;i<count;i++)found+=refs[i]==bos[j];CHECK(found==1);}
 unsigned found=0;for(unsigned i=0;i<count;i++)found+=refs[i]==job;CHECK(found==1);
 for(size_t i=0;i<nn_pipeline_recipe.patch_count;i++){
  const struct nn_pipeline_address_patch*patch=nn_pipeline_recipe.patches+i;CHECK(patch->role==NN_PIPELINE_CODE);uint32_t*w=nn_bo_map(job);uint64_t address=(uint64_t)w[patch->word]|((uint64_t)w[patch->word+1]<<32);CHECK(address==nn_bo_address(bos[patch->stage]));
 }
 uint32_t before[1024];memcpy(before,nn_bo_map(job),sizeof(before));words=99;CHECK(nn_pipeline_shader_encode(p,&nn_pipeline_recipe,&binding,1,&words)==ENOSPC&&!words&&!memcmp(before,nn_bo_map(job),sizeof(before)));
 bos[0]->busy++;CHECK(nn_pipeline_program_destroy(p)==EBUSY);CHECK(nn_pipeline_program_device(p)==d);bos[0]->busy--;
 struct nn_pipeline_native_artifact bad=NN_FRONT_FACING_ARTIFACT;bad.spirv[1]=bad.spirv[0];nn_pipeline_program*invalid=NULL;CHECK(nn_pipeline_program_create_native(d,&bad,&invalid,&retained)==EINVAL&&!invalid&&!retained);
 CHECK(!nn_bo_destroy(job));CHECK(!matrix(d,p));CHECK(!nn_pipeline_program_destroy(p));CHECK(!d->bos);CHECK(!nn_device_close(d));
 printf("{\"passed\":true,\"checks\":%u,\"shader_resources\":3,\"gpu_execution\":false,\"actual_native_program_owner\":true,\"matrix_cases\":64,\"encoded_jobs\":128,\"replays_per_case\":2,\"packet_execution\":false}\n",checks);return 0;
}
