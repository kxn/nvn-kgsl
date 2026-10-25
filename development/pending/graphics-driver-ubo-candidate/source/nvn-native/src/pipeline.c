/* SPDX-License-Identifier: MIT. Native pipeline assembled from actual program,
 * device-only restore, actual FDL views and per-Draw snapshots. No fixed image. */
#include "nvn_native/pipeline.h"
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/pm4.h"
#include "nvn_native/depth_native.h"
#include "nvn_native/pipeline_storage.h"
#include "nvn_native/pipeline_runtime_constants.h"
#ifndef NN_PIPELINE_RUNTIME_CONSTANTS
#define NN_PIPELINE_RUNTIME_CONSTANTS 0
#endif
#ifndef NN_PIPELINE_GRAPHICS_STORAGE
#define NN_PIPELINE_GRAPHICS_STORAGE 0
#endif
#ifndef NN_PIPELINE_DEPTH_NATIVE
#define NN_PIPELINE_DEPTH_NATIVE 0
#endif
#include "nvn_native/texture_bc_shadow.h"
#include "nvn_native/texture_volume_sample.h"
#ifndef NN_PIPELINE_COLOR_TARGETS
#define NN_PIPELINE_COLOR_TARGETS 0
#endif
#if NN_PIPELINE_COLOR_TARGETS
#include "nvn_native/color_target.h"
#endif
#include <errno.h>
#include <math.h>
#include <string.h>
#ifndef NN_PIPELINE_SAMPLED_IMAGES
#define NN_PIPELINE_SAMPLED_IMAGES 0
#endif
#if NN_PIPELINE_SAMPLED_IMAGES
#include "nvn_native/sampled_image.h"
#endif
#ifndef NN_PIPELINE_GATHER
#define NN_PIPELINE_GATHER 0
#endif
#ifndef NN_PIPELINE_BUFFER_SAMPLED
#define NN_PIPELINE_BUFFER_SAMPLED 0
#endif
#ifndef NN_PIPELINE_VOLUME_SAMPLED
#define NN_PIPELINE_VOLUME_SAMPLED 0
#endif
#ifndef NN_PIPELINE_BC_SAMPLED
#define NN_PIPELINE_BC_SAMPLED 0
#endif
#ifndef NN_PIPELINE_DEPTH_SAMPLED
#define NN_PIPELINE_DEPTH_SAMPLED 0
#endif
#if NN_PIPELINE_DEPTH_SAMPLED
#include "nvn_native/depth_sampled_native.h"
#endif
enum {COMMAND_CAPACITY=8192,ZERO_OFFSET=32768,SAMPLER_OFFSET=36864,TEXTURE_OFFSET=40960,
      SEQUENCE_OFFSET=45056,STORAGE_OFFSET=45120,DRIVER_OFFSET=47168,JOB_BYTES=NN_PIPELINE_RUNTIME_CONSTANTS?47232:NN_PIPELINE_GRAPHICS_STORAGE?47168:45060};
struct builder {uint32_t words[COMMAND_CAPACITY];size_t count;};
static int append(struct builder *b,const uint32_t *words,size_t count)
{if(count>COMMAND_CAPACITY-b->count)return EOVERFLOW;memcpy(b->words+b->count,words,count*4);b->count+=count;return 0;}
static void reg(struct builder *b,uint32_t address,uint32_t value)
{b->words[b->count++]=nvn_pm4_packet4(address,1);b->words[b->count++]=value;}
static void reg_address(struct builder *b,uint32_t r,uint64_t value)
{b->words[b->count++]=nvn_pm4_packet4(r,2);b->words[b->count++]=(uint32_t)value;b->words[b->count++]=(uint32_t)(value>>32);}
static uint32_t bits(float f){uint32_t v;memcpy(&v,&f,4);return v;}
static int resource(struct nn_pipeline_submission *s,nn_bo *bo)
{if(!bo)return EINVAL;for(size_t i=0;i<s->resource_count;i++)if(s->resources[i]==bo)return 0;
 if(s->resource_count==128)return EOVERFLOW;
 s->resources[s->resource_count++]=bo;return 0;}
static int buffer(struct nn_pipeline_buffer b,size_t bytes,uint32_t alignment,uint64_t *address)
{
 if(!b.bo||!alignment||b.offset>nn_bo_size(b.bo)||b.bytes>nn_bo_size(b.bo)-b.offset||bytes>b.bytes)return EINVAL;
 uint64_t a=nn_bo_address(b.bo);if(!a||b.offset>UINT64_MAX-a||b.bytes>UINT64_MAX-a-b.offset)return EOVERFLOW;
 a+=b.offset;if(a%alignment)return EINVAL;*address=a;return 0;
}
static int image_layout(const struct nn_tex_native_view *v)
{
 if(!v)return EINVAL;
 if(v->schema!=1||v->chip!=0x06050002||v->format!=37||v->target!=1||v->levels!=1||v->layers!=1||
    v->samples!=1||v->tile_mode||v->ubwc||!v->width||v->width>16384||!v->height||v->height>16384||
    v->pitch<v->width*4u||v->pitch%64||v->layer_bytes%64||v->alignment!=64||
    (uint64_t)v->pitch*v->height>v->bytes||v->required_span_bytes<v->bytes||v->required_span_bytes<v->layer_bytes||
    v->descriptor_address_word!=4||v->descriptor_offset||v->descriptor_address_mask!=UINT64_C(0x1ffffffffffe0)||
    v->rb_mrt_buf_info!=48||v->sp_ps_mrt_reg!=48||v->rb_mrt_pitch!=v->pitch/64||v->rb_mrt_array_pitch!=v->layer_bytes/64)
    return ENOTSUP;
 uint64_t field=(uint64_t)v->texture_descriptor[4]|(uint64_t)v->texture_descriptor[5]<<32;
 if(field&v->descriptor_address_mask)return EINVAL;
 return 0;
}
static int image(const struct nn_pipeline_image *im,uint64_t *address)
{
 if(!im)return EINVAL;
 const struct nn_tex_native_view *v=im->layout;int e=image_layout(v);if(e)return e;
 e=buffer(im->buffer,v->required_span_bytes,v->alignment,address);if(e)return e;
 if(*address&~v->descriptor_address_mask)return EOVERFLOW;
 return 0;
}
/* Target storage and sampled-image domains are independent. The default
 * legacy path is unchanged; opt-in needs a verified actual runtime FDL lib. */
static int target_layout(const struct nn_tex_native_view *v)
{
#if NN_PIPELINE_COLOR_TARGETS
 return nn_color_target_validate(v,NULL);
#else
 return image_layout(v);
#endif
}
static int target_image(const struct nn_pipeline_image *im,uint64_t *address)
{
 if(!im)return EINVAL;
 const struct nn_tex_native_view *v=im->layout;int e=target_layout(v);if(e)return e;
 e=buffer(im->buffer,v->required_span_bytes,v->alignment,address);if(e)return e;
 if(*address&~v->descriptor_address_mask)return EOVERFLOW;
 return 0;
}
static int segment_validate(const struct nn_pipeline_platform_segment *s)
{
 if(!s->words||!s->word_count||s->word_count>2048||s->patch_count>32||(s->patch_count&&!s->patches))return EINVAL;
 for(size_t at=0;at<s->word_count;){size_t n=nvn_pm4_packet_words(s->words[at]);if(!n||n>s->word_count-at)return EINVAL;at+=n;}
 for(size_t i=0;i<s->patch_count;i++) {
  const struct nn_pipeline_platform_patch *p=s->patches+i;
  if(p->word>=s->word_count-1||p->offset||p->kind>NN_PLATFORM_SEQUENCE||
     p->bytes!=(p->kind==NN_PLATFORM_ZERO?4096u:4u)||s->words[p->word]||s->words[p->word+1])return EINVAL;
  for(size_t j=0;j<i;j++)if(p->word<s->patches[j].word+2&&s->patches[j].word<p->word+2)return EINVAL;
  int found=0;for(size_t at=0;at<s->word_count;){size_t n=nvn_pm4_packet_words(s->words[at]);uint32_t h=s->words[at];
   if(p->word==at+1&&(h>>28)==4&&n==3&&p->kind==NN_PLATFORM_ZERO)found=1;
   if(p->word==at+2&&h==nvn_pm4_packet7(0x46,4)&&p->kind==NN_PLATFORM_SEQUENCE)found=1;
   at+=n;}
  if(!found)return EINVAL;
 }
 return 0;
}
static int segment(struct builder *b,const struct nn_pipeline_platform_segment *s,nn_bo *job)
{
 int e=segment_validate(s);if(e)return e;
 size_t start=b->count;if((e=append(b,s->words,s->word_count)))return e;
 for(size_t i=0;i<s->patch_count;i++) {
  const struct nn_pipeline_platform_patch *p=s->patches+i;
  uint64_t a=nn_bo_address(job)+(p->kind==NN_PLATFORM_ZERO?ZERO_OFFSET:SEQUENCE_OFFSET);
  b->words[start+p->word]=(uint32_t)a;b->words[start+p->word+1]=(uint32_t)(a>>32);
 }
 return 0;
}
int nn_pipeline_full_raster(uint32_t width,uint32_t height,uint32_t origin,uint32_t depth,struct nn_pipeline_raster *out)
{
 if(!out||!width||width>16384||!height||height>16384||origin>1||depth>1)return EINVAL;
 *out=(struct nn_pipeline_raster){.viewport={width*.5f,width*.5f,height*.5f,(origin?-1.f:1.f)*height*.5f,depth?0.f:.5f,depth?1.f:.5f},
  .scissor_width=width,.scissor_height=height,.window_origin=origin,.depth_mode=depth};return 0;
}
/* Project's generic A650 Draw packet implementation (graphics.c), independent
 * from its legacy image scan/replacement. Native code here has no driverparams;
 * new typed runtime-constant profile supplies GPU-updated VS driver params. */
static int draw(struct builder *b,const struct nn_gfx_draw *d,struct nn_pipeline_submission *s,uint32_t driver_dst)
{
 static const uint32_t primitives[]={9,2,7,3,4,6,5};
 if(d->mode>NN_GFX_DRAW_INDIRECT_COUNT||d->indexed>1||d->index_type>2||d->primitive<4||d->primitive>=7)return ENOTSUP;
 if(d->mode==NN_GFX_DRAW_DIRECT&&(!d->count||!d->instances))return EINVAL;
 const struct nn_gfx_buffer *extra[]={&d->index,&d->indirect,&d->indirect_count};uint64_t addresses[3]={0};
 for(unsigned i=0;i<3;i++) {
  int needed=i==0?d->indexed:i==1?d->mode!=NN_GFX_DRAW_DIRECT:d->mode==NN_GFX_DRAW_INDIRECT_COUNT;
  const struct nn_gfx_buffer *v=extra[i];if(!needed){if(v->bo||v->offset||v->bytes)return EINVAL;continue;}
  int e=buffer((struct nn_pipeline_buffer){v->bo,v->offset,v->bytes},i==2?4:1,i==0?1u<<d->index_type:4,addresses+i);
  if(e)return e;
  if((e=resource(s,v->bo)))return e;
 }
 uint64_t indices=d->indexed?d->index.bytes>>d->index_type:0;
 if(indices>UINT32_MAX||(d->indexed&&d->mode==NN_GFX_DRAW_DIRECT&&d->count>indices))return EINVAL;
 if(d->mode!=NN_GFX_DRAW_DIRECT) {
  uint32_t record=d->indexed?20:16;
  if(!d->max_draws||d->stride<record||d->stride%4||(uint64_t)(d->max_draws-1)*d->stride+record>d->indirect.bytes)return EINVAL;
 }
 uint32_t ctl=primitives[d->primitive]|(1u<<8)|(d->indexed?d->index_type<<10:2u<<6);
 b->words[b->count++]=nvn_pm4_packet4(0xa00e,2);b->words[b->count++]=d->indexed?(uint32_t)d->base_vertex:d->first;b->words[b->count++]=d->base_instance;
 if(d->mode==NN_GFX_DRAW_DIRECT) {
  b->words[b->count++]=nvn_pm4_packet7(0x38,d->indexed?7:3);b->words[b->count++]=ctl;b->words[b->count++]=d->instances;b->words[b->count++]=d->count;
  if(d->indexed){b->words[b->count++]=0;b->words[b->count++]=(uint32_t)addresses[0];b->words[b->count++]=(uint32_t)(addresses[0]>>32);b->words[b->count++]=(uint32_t)indices;}
 } else {
  int counted=d->mode==NN_GFX_DRAW_INDIRECT_COUNT;
  b->words[b->count++]=nvn_pm4_packet7(0x2a,d->indexed?(counted?11:9):(counted?8:6));b->words[b->count++]=ctl;
  b->words[b->count++]=(d->indexed?(counted?7:4):(counted?6:2))|(driver_dst<<8);b->words[b->count++]=d->max_draws;
  if(d->indexed){b->words[b->count++]=(uint32_t)addresses[0];b->words[b->count++]=(uint32_t)(addresses[0]>>32);b->words[b->count++]=(uint32_t)indices;}
  b->words[b->count++]=(uint32_t)addresses[1];b->words[b->count++]=(uint32_t)(addresses[1]>>32);
  if(counted){b->words[b->count++]=(uint32_t)addresses[2];b->words[b->count++]=(uint32_t)(addresses[2]>>32);}
  b->words[b->count++]=d->stride;
 }
 return 0;
}
/* The table is a trusted immutable normal compiler artifact, never an NVN
 * caller tag. Exact recipe plus both actual SPIR-V stage identities prevent
 * transplanting dimension facts from another pair or specialization. */
static int sampler_encode(const struct nn_pipeline_draw_state *state,uint32_t chip,unsigned binding,uint32_t words[4])
{
#if NN_PIPELINE_DEPTH_SAMPLED
 if(state->sampled_depth[binding])return nn_depth_sampled_sampler(state->sampled_depth[binding],state->samplers+binding,state->sampler_layouts+binding,words);
#endif
#if NN_PIPELINE_SAMPLED_IMAGES
 if(state->sampled_color[binding])return nn_sampled_image_sampler(state->sampled_color[binding],state->samplers+binding,state->sampler_layouts+binding,words);
#endif
 return nn_sampler_state_encode(chip,state->samplers+binding,state->sampler_layouts+binding,words);
}
static int texture_types(const struct nn_offline_graphics_stage *const stages[2],
 const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_texture_types*t)
{
 if(!t)return 0;
 if(t->version!=1||t->recipe!=r||t->texture_mask!=r->texture_mask||t->sampler_mask!=r->sampler_mask)return EINVAL;
 for(unsigned i=0;i<2;i++){
  if(!stages||!stages[i]||!stages[i]->spirv||!t->spirv[i]||t->spirv_bytes[i]<20||t->spirv_bytes[i]%4||t->spirv_bytes[i]>1048576||
     stages[i]->spirv_bytes!=t->spirv_bytes[i]||memcmp(stages[i]->spirv,t->spirv[i],t->spirv_bytes[i]))return EINVAL;
 }
 for(unsigned i=0;i<16;i++){
  int valid=nn_pipeline_texture_type_validate(t,i);if(valid)return valid;
 }
 return 0;
}
/* planned_images bit0 is target, bit(1+i) native texture i. A planned
 * image must have no BO/address at all: this is explicit nonresident metadata,
 * not a canonical source BO disguised as a native allocation. */
#if NN_PIPELINE_DEPTH_NATIVE
static int depth_alias(const struct nn_depth_attachment *a,struct nn_pipeline_buffer b)
{
 if(!b.bo)return 0;
 if(b.offset>nn_bo_size(b.bo)||b.bytes>nn_bo_size(b.bo)-b.offset)return EINVAL;
 uint64_t x=nn_bo_address(a->storage),n=nn_bo_size(a->storage),y=nn_bo_address(b.bo);
 if(!y||b.offset>UINT64_MAX-y||b.bytes>UINT64_MAX-y-b.offset||n>UINT64_MAX-x)return EOVERFLOW;
 y+=b.offset;return x<y+b.bytes&&y<x+n?ENOTSUP:0;
}
static int depth_preflight(const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_depth_export*d,
 const struct nn_depth_attachment*a,const struct nn_depth_fdl_view*planned,const struct nn_depth_controls_desc*c,const struct nn_pipeline_draw_state*s)
{
 if(!d||d->recipe!=r||!c||(!a&&!planned)||(a&&planned))return EINVAL;
 size_t words=0;int e=planned?nn_depth_native_plan(planned,d,c,&words):nn_depth_native_words(a,d,c,NULL,0,&words);if(e)return e;
 const struct nn_depth_fdl_view *view=planned?planned:&a->view;
 if(s->target.layout){
  if(view->view_width!=s->target.layout->width||view->view_height!=s->target.layout->height)return ENOTSUP;
 }else if(d->color_outputs||s->target.buffer.bo||s->target.buffer.offset||s->target.buffer.bytes)return ENOTSUP;
 if(planned)return 0; /* Canonical aliases belong to the caller's CPU preflight; no resident depth BO exists. */
 if((e=depth_alias(a,s->target.buffer)))return e;
 for(unsigned i=0;i<16;i++)if(s->texture_bound&(1u<<i)){
  if((e=depth_alias(a,s->textures[i].buffer)))return e;
  if(s->sampled_buffer[i]){const struct nn_buffer_sampled_binding*b=s->sampled_buffer[i];if((e=depth_alias(a,(struct nn_pipeline_buffer){b->resource,(size_t)b->offset,(size_t)b->bytes})))return e;}
 }
 for(size_t i=0;i<r->input_count;i++){unsigned location=r->inputs[i].location;if(location>=32||s->vertex.attributes[location].stream>=32)return EINVAL;
  if((e=depth_alias(a,s->vertex.buffers[s->vertex.attributes[location].stream])))return e;}
 for(size_t i=0;i<r->patch_count;i++){const struct nn_pipeline_address_patch*q=r->patches+i;if(q->role==NN_PIPELINE_UBO||q->role==NN_PIPELINE_DYNAMIC_UBO){if(q->stage>=2||q->binding>=16)return EINVAL;if((e=depth_alias(a,s->ubo[q->stage][q->binding])))return e;}}
 struct builder command={0};struct nn_pipeline_submission refs={0};if((e=draw(&command,&s->draw,&refs,0)))return e;
 for(size_t i=0;i<refs.resource_count;i++)if((e=depth_alias(a,(struct nn_pipeline_buffer){refs.resources[i],0,nn_bo_size(refs.resources[i])})))return e;
 return 0;
}
#endif
static int pipeline_preflight(nn_device *device,const struct nn_offline_graphics_stage *const stages[2],
 const struct nn_pipeline_shader_recipe *recipe,const struct nn_pipeline_platform *platform,
 const struct nn_pipeline_draw_state *state,uint32_t planned_images,const struct nn_pipeline_depth_export*depth,
 const struct nn_depth_attachment*attachment,const struct nn_depth_controls_desc*controls,const struct nn_depth_fdl_view*planned_depth)
{
 if(depth){
#if NN_PIPELINE_DEPTH_NATIVE
  if(!state)return EINVAL;
  int e=depth_preflight(recipe,depth,attachment,planned_depth,controls,state);if(e)return e;
#else
  return ENOTSUP;
#endif
 }else if(attachment||controls||planned_depth)return EINVAL;
 if(!device||!recipe||!platform||!state||platform->version!=1||
    platform->chip!=nn_device_chip(device)||platform->chip!=0x06050002||
    platform->gmem_bytes!=nn_device_gmem_bytes(device)||(planned_images&~UINT32_C(0x1ffff)))return EINVAL;
 unsigned color=state->target.layout!=NULL;
 if(!color&&(!depth||depth->color_outputs||state->target.buffer.bo||state->target.buffer.offset||state->target.buffer.bytes))return ENOTSUP;
 const struct nn_pipeline_raster *r=&state->raster;
 if(r->window_origin>1||r->depth_mode>1)return EINVAL;
 for(unsigned i=0;i<6;i++)if(!isfinite(r->viewport[i]))return EINVAL;
 if(r->viewport[1]==0||r->viewport[3]==0||r->viewport[5]<=0)return ENOTSUP;
 if(state->texture_bound!=recipe->texture_mask||state->sampler_bound!=recipe->sampler_mask)return ENOTSUP;
 int types_error=texture_types(stages,recipe,state->texture_types);if(types_error)return types_error;
 for(unsigned i=0;i<16;i++)if((state->sampled_fdl[i]||state->sampled_volume[i]||state->sampled_buffer[i]||state->sampled_color[i]||state->sampled_depth[i])&&!(state->texture_bound&(1u<<i)))return EINVAL;
 for(unsigned bit=0;bit<=16;bit++) {
  if(!bit&&!color)continue;
  if(bit&&!(state->texture_bound&(1u<<(bit-1)))){if(planned_images&(1u<<bit))return EINVAL;continue;}
  const struct nn_pipeline_image *im=bit?state->textures+bit-1:&state->target;
  const struct nn_fdl_image_view *bc=bit?state->sampled_fdl[bit-1]:NULL;
  const struct nn_fdl_volume_view *volume=bit?state->sampled_volume[bit-1]:NULL;
  const struct nn_buffer_sampled_binding *buf=bit?state->sampled_buffer[bit-1]:NULL;
  const struct nn_fdl_image_view *color=bit?state->sampled_color[bit-1]:NULL;
  const struct nn_depth_fdl_view *sampled_depth=bit?state->sampled_depth[bit-1]:NULL;
  if((!!bc+!!volume+!!buf+!!color+!!sampled_depth)>1)return EINVAL;
  if(bit&&state->texture_types&&state->texture_types->dimensions[bit-1]!=(buf?1u:volume?3u:2u))return ENOTSUP;
  if(bit&&state->texture_types&&(state->texture_types->operations[bit-1]&NN_PIPELINE_TEX_GATHER)){
#if NN_PIPELINE_GATHER
   if(bc||volume||buf||(!color&&!sampled_depth&&!im->layout)||!(state->sampler_bound&(1u<<(bit-1)))||
      state->sampler_layouts[bit-1].normalized!=1)return ENOTSUP;
#else
   return ENOTSUP;
#endif
  }
  uint64_t required=0;int e;
  if(sampled_depth){
#if NN_PIPELINE_DEPTH_SAMPLED
   if(im->layout||!state->texture_types||sampled_depth->desc.address)return EINVAL;
   uint32_t operations;if((e=nn_depth_sampled_types(state->texture_types,bit-1,&operations))||
      (e=nn_depth_sampled_validate(sampled_depth)))return e;
   if(state->sampler_bound&(1u<<(bit-1))){uint32_t sampler[4];if((e=nn_depth_sampled_sampler(sampled_depth,state->samplers+bit-1,state->sampler_layouts+bit-1,sampler)))return e;}
   required=sampled_depth->required_span;
#else
   return ENOTSUP;
#endif
  }else if(color){
#if NN_PIPELINE_SAMPLED_IMAGES
   if(im->layout||!state->texture_types||state->texture_types->dimensions[bit-1]!=2)return ENOTSUP;
   if((e=nn_sampled_image_validate(color,NULL)))return e;
   uint32_t op=state->texture_types->operations[bit-1];
   if(!!(op&(NN_PIPELINE_TEX_SAMPLE|NN_PIPELINE_TEX_GATHER))!=!!(state->sampler_bound&(1u<<(bit-1))))return ENOTSUP;
   if(state->sampler_bound&(1u<<(bit-1))){uint32_t sampler[4];if((e=nn_sampled_image_sampler(color,state->samplers+bit-1,state->sampler_layouts+bit-1,sampler)))return e;}
   required=color->required_span;
#else
   return ENOTSUP;
#endif
  }else if(buf){
#if NN_PIPELINE_BUFFER_SAMPLED
   if(im->layout||im->buffer.bo||im->buffer.offset||im->buffer.bytes||(planned_images&(1u<<bit)))return EINVAL;
   struct nn_fdl_buffer_view actual;uint32_t result;
   if((e=nn_buffer_sampled_view(buf,&actual))||(e=nn_buffer_sampled_result(buf->format,&result)))return e;
   if(!state->texture_types||(state->sampler_bound&(1u<<(bit-1)))||
      !(state->texture_types->operations[bit-1]&(NN_PIPELINE_TEX_FETCH|NN_PIPELINE_TEX_SIZE))||
      (state->texture_types->operations[bit-1]&~(NN_PIPELINE_TEX_FETCH|NN_PIPELINE_TEX_SIZE)))return ENOTSUP;
   /* Existing normal compiler profile accepts floating sampled declarations
    * only. TXS is format-independent; integer TXF remains explicitly closed. */
   if((state->texture_types->operations[bit-1]&NN_PIPELINE_TEX_FETCH)&&result!=NN_BUFFER_FLOAT)return ENOTSUP;
   /* Same BO can be proven disjoint by exact native read span; imports in
    * different BOs still require caller physical-independence proof. */
   if(buf->resource==state->target.buffer.bo){
    const struct nn_pipeline_buffer *target=&state->target.buffer;
    uint64_t base=nn_bo_address(buf->resource),start=actual.base_address-base;
    if(start<target->offset+target->bytes&&target->offset<start+actual.required_span)return ENOTSUP;
   }
   continue;
#else
   return ENOTSUP;
#endif
  }else if(volume){
#if NN_PIPELINE_VOLUME_SAMPLED
   if(im->layout||!state->texture_types||state->texture_types->dimensions[bit-1]!=3)return ENOTSUP;
   if((e=nn_volume_sampled_validate(volume)))return e;
   required=volume->required_span;
#else
   return ENOTSUP;
#endif
  }else if(bc){
#if NN_PIPELINE_BC_SAMPLED
   if(im->layout)return ENOTSUP;
   if(state->sampler_bound&(1u<<(bit-1))){uint32_t sampler[4];
    if((e=nn_bc_sampled_sampler(bc,state->samplers+bit-1,state->sampler_layouts+bit-1,sampler)))return e;}
   if((e=nn_bc_sampled_validate(bc)))return e;
   required=bc->required_span;
#else
   return ENOTSUP;
#endif
  }else{if((e=bit?image_layout(im->layout):target_layout(im->layout)))return e;required=im->layout->required_span_bytes;}
  if(planned_images&(1u<<bit)) {
   if(im->buffer.bo||im->buffer.offset||im->buffer.bytes!=required)return EINVAL;
  }else if(sampled_depth){
#if NN_PIPELINE_DEPTH_SAMPLED
   uint32_t descriptor[16];if((e=nn_depth_sampled_descriptor(sampled_depth,&im->buffer,descriptor)))return e;
#else
   return ENOTSUP;
#endif
  }else if(color){
#if NN_PIPELINE_SAMPLED_IMAGES
   uint32_t descriptor[16];struct nn_sampled_image_binding binding={im->buffer.bo,im->buffer.offset,im->buffer.bytes,im->buffer.offset,im->buffer.bytes};
   if((e=nn_sampled_image_descriptor(color,&binding,descriptor)))return e;
#else
   return ENOTSUP;
#endif
  }else if(volume){
#if NN_PIPELINE_VOLUME_SAMPLED
   uint32_t descriptor[16];if((e=nn_volume_sampled_descriptor(volume,im->buffer.bo,im->buffer.offset,im->buffer.bytes,descriptor)))return e;
#else
   return ENOTSUP;
#endif
  }else if(bc){
#if NN_PIPELINE_BC_SAMPLED
   uint32_t descriptor[16];if((e=nn_bc_sampled_descriptor(bc,im->buffer.bo,im->buffer.offset,im->buffer.bytes,descriptor)))return e;
#else
   return ENOTSUP;
#endif
  }else{uint64_t address;if((e=bit?image(im,&address):target_image(im,&address)))return e;}
  if(bit && !(planned_images&1u) && !(planned_images&(1u<<bit))&&
     im->buffer.bo==state->target.buffer.bo)return ENOTSUP;
 }
 const struct nn_tex_native_view *v=state->target.layout;
 uint32_t width=color?v->width:(planned_depth?planned_depth->view_width:attachment->view.view_width),height=color?v->height:(planned_depth?planned_depth->view_height:attachment->view.view_height);
 if(!r->scissor_width||!r->scissor_height||r->scissor_x>=width||r->scissor_width>width-r->scissor_x||
    r->scissor_y>=height||r->scissor_height>height-r->scissor_y)return EINVAL;
 uint32_t vertex[512],render[128];size_t vertex_words=0,render_words=0;
 int e=(depth?nn_pipeline_vertex_encode_depth(depth,&state->vertex,vertex,512,&vertex_words):nn_pipeline_vertex_encode(recipe,&state->vertex,vertex,512,&vertex_words));if(e)return e;
 struct nn_render_target_info rt={platform->chip,r->window_origin,color,depth?1u:0u,1};
 struct nn_render_state color_render=state->render;
#if NN_PIPELINE_DEPTH_NATIVE
 if(depth&&(e=nn_depth_native_render(&state->render,controls,&color_render)))return e;
#endif
 if((e=depth?nn_render_state_encode_depth_raster(&color_render,&rt,render,128,&render_words):nn_render_state_encode(&color_render,&rt,render,128,&render_words)))return e;
 for(unsigned i=0;i<16;i++)if(state->sampler_bound&(1u<<i)){
  uint32_t words[4];if((e=sampler_encode(state,platform->chip,i,words)))return e;
 }
 uint32_t storage_entries=0;
#if NN_PIPELINE_GRAPHICS_STORAGE
 if((e=nn_pipeline_storage_types_validate(state->storage_types,recipe,stages,&storage_entries)))return e;
 for(unsigned stage=0;stage<2;stage++)for(unsigned binding=0;binding<16;binding++){
  struct nn_pipeline_buffer v=state->storage[stage][binding];
  if(!state->storage_types||!(state->storage_types->mask[stage]&(1u<<binding))){if(v.bo||v.offset||v.bytes)return EINVAL;continue;}
  uint32_t desc[16];if((e=nn_pipeline_storage_descriptor(v,desc)))return e;
  /* No read-feedback or writable cross-role alias protocol is assumed. */
  if(v.bo==state->target.buffer.bo)return ENOTSUP;
#if NN_PIPELINE_DEPTH_NATIVE
  if(depth&&attachment&&v.bo==attachment->storage)return ENOTSUP;
#endif
  if(state->storage_types->operations[stage][binding]&NN_PIPELINE_STORAGE_WRITE){
   for(unsigned st=0;st<2;st++)for(unsigned i=0;i<16;i++)if((st!=stage||i!=binding)&&v.bo==state->storage[st][i].bo)return ENOTSUP;
   for(unsigned i=0;i<16;i++)if((state->texture_bound&(1u<<i))&&
      (v.bo==state->textures[i].buffer.bo||(state->sampled_buffer[i]&&v.bo==state->sampled_buffer[i]->resource)))return ENOTSUP;
   for(unsigned st=0;st<2;st++)for(unsigned i=0;i<16;i++)if(v.bo==state->ubo[st][i].bo)return ENOTSUP;
   for(unsigned i=0;i<32;i++)if(v.bo==state->vertex.buffers[i].bo)return ENOTSUP;
   if(v.bo==state->draw.index.bo||v.bo==state->draw.indirect.bo||v.bo==state->draw.indirect_count.bo)return ENOTSUP;
  }
 }
#else
 if(state->storage_types)return ENOTSUP;
 for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<16;i++)if(state->storage[stage][i].bo||state->storage[stage][i].offset||state->storage[stage][i].bytes)return ENOTSUP;
#endif
 (void)storage_entries;
 uint32_t driver_words[16]={0},driver_dst=0;
#if NN_PIPELINE_RUNTIME_CONSTANTS
 if((e=nn_pipeline_runtime_constants_validate(state->runtime_constants,recipe,stages))||
    (e=nn_pipeline_runtime_draw(state->runtime_constants,&state->draw,state->runtime_draw_id,driver_words,&driver_dst)))return e;
#else
 if(state->runtime_constants||state->runtime_draw_id)return ENOTSUP;
#endif
 (void)driver_words;
#if NN_PIPELINE_RUNTIME_CONSTANTS
 if(state->runtime_constants)for(unsigned stage=0;stage<2;stage++)for(unsigned binding=0;binding<16;binding++)
  if((state->runtime_constants->layout->ubo_mask[stage]&(1u<<binding))&&
     state->ubo[stage][binding].bo&&state->ubo[stage][binding].bo==state->target.buffer.bo)return ENOTSUP;
#endif
 struct builder builder={0};struct nn_pipeline_submission resources={0};
 if((e=draw(&builder,&state->draw,&resources,driver_dst)))return e;
 if((e=segment_validate(&platform->begin))||(e=segment_validate(&platform->end)))return e;
#if NN_PIPELINE_RUNTIME_CONSTANTS
 if((e=nn_pipeline_shader_preflight_runtime(stages,recipe,depth,state->runtime_constants,state->ubo)))return e;
#else
 if((e=(depth?nn_pipeline_shader_preflight_depth(stages,depth,state->ubo):nn_pipeline_shader_preflight(stages,recipe,state->ubo))))return e;
#endif
 size_t depth_count=0;
#if NN_PIPELINE_DEPTH_NATIVE
 if(depth&&(e=planned_depth?nn_depth_native_plan(planned_depth,depth,controls,&depth_count):nn_depth_native_words(attachment,depth,controls,NULL,0,&depth_count)))return e;
#endif
 size_t total=depth_count+platform->begin.word_count+platform->end.word_count+recipe->word_count+vertex_words+render_words+builder.count;
 /* Fixed target/raster state below emits fewer than128 DWORDs. This is a
  * conservative storage admission bound, not a per-Draw control-ref charge. */
 if(total>COMMAND_CAPACITY-128)return ENOSPC;
 return 0;
}
static int pipeline_encode(nn_device *device,const nn_pipeline_program *program,const struct nn_pipeline_shader_recipe *recipe,
 const struct nn_pipeline_platform *platform,const struct nn_pipeline_draw_state *state,struct nn_pipeline_submission *out,nn_bo **retained,
 const struct nn_pipeline_depth_export*depth,const struct nn_depth_attachment*attachment,const struct nn_depth_controls_desc*controls)
{
 if(out)memset(out,0,sizeof(*out));
 if(retained)*retained=NULL;
 if(!out||!retained||!device||!program||!recipe||!platform||!state||platform->version!=1||
    nn_pipeline_program_device(program)!=device||platform->chip!=nn_device_chip(device)||platform->chip!=0x06050002||platform->gmem_bytes!=nn_device_gmem_bytes(device))return EINVAL;
 /* Normal compiler facts are bound to the immutable program owner, never a
  * free-form Draw label or an expected archive variant used as actual state. */
 if(state->texture_types!=nn_pipeline_program_texture_types(program))return EINVAL;
 if(state->storage_types!=nn_pipeline_program_storage_types(program))return EINVAL;
 if(state->runtime_constants!=nn_pipeline_program_runtime_constants(program))return EINVAL;
#if NN_PIPELINE_DEPTH_NATIVE
 if(nn_pipeline_program_depth_export(program)!=depth)return ENOTSUP;
#else
 if(depth||attachment||controls)return ENOTSUP;
#endif
 const struct nn_offline_graphics_stage *source[2]={0};
 struct nn_pipeline_draw_state validation=*state;
 for(unsigned i=0;i<2;i++){
  struct nn_pipeline_stage_view view;int e=nn_pipeline_program_stage(program,i,&view);if(e)return e;source[i]=view.source;
#if NN_PIPELINE_RUNTIME_CONSTANTS
  if(state->runtime_constants&&state->target.buffer.bo&&
     (state->target.buffer.bo==view.code||state->target.buffer.bo==view.c1))return ENOTSUP;
#endif
#if NN_PIPELINE_GRAPHICS_STORAGE
  if(state->storage_types)for(unsigned st=0;st<2;st++)for(unsigned b=0;b<16;b++)if((state->storage_types->mask[st]&(1u<<b))&&(state->storage_types->operations[st][b]&NN_PIPELINE_STORAGE_WRITE)&&
     (state->storage[st][b].bo==view.code||state->storage[st][b].bo==view.c1))return ENOTSUP;
#endif
  /* The pure nonresident preflight has no program BO owner. Normalize only
   * an actual immutable-C1 view whose owner was independently verified here;
   * the final shader resolver still checks the original binding/address. */
  if(view.c1&&view.c1_native_binding<16){
   struct nn_pipeline_buffer u=state->ubo[i][view.c1_native_binding];
   if(u.bo&&(u.bo!=view.c1||u.offset||u.bytes!=view.c1_bytes))return EINVAL;
   validation.ubo[i][view.c1_native_binding]=(struct nn_pipeline_buffer){0};
  }
 }
 int preflight=pipeline_preflight(device,source,recipe,platform,&validation,0,depth,attachment,controls,NULL);if(preflight)return preflight;
 unsigned color=state->target.layout!=NULL;
 if(!color&&(!depth||depth->color_outputs||state->target.buffer.bo||state->target.buffer.offset||state->target.buffer.bytes))return ENOTSUP;
 const struct nn_pipeline_raster *r=&state->raster;
 if(r->window_origin>1||r->depth_mode>1)return EINVAL;
 for(unsigned i=0;i<6;i++)if(!isfinite(r->viewport[i]))return EINVAL;
 if(r->viewport[1]==0||r->viewport[3]==0||r->viewport[5]<=0)return ENOTSUP;
 uint64_t target=0;int e=0;if(color&&(e=target_image(&state->target,&target)))return e;
 const struct nn_tex_native_view *v=state->target.layout;
 uint32_t width=color?v->width:attachment->view.view_width,height=color?v->height:attachment->view.view_height;
 if(!r->scissor_width||!r->scissor_height||r->scissor_x>=width||r->scissor_width>width-r->scissor_x||
    r->scissor_y>=height||r->scissor_height>height-r->scissor_y)return EINVAL;
 if(state->texture_bound!=recipe->texture_mask||state->sampler_bound!=recipe->sampler_mask)return ENOTSUP;
 uint32_t vertex[512],render[128];size_t vertex_words=0,render_words=0;
 if((e=(depth?nn_pipeline_vertex_encode_depth(depth,&state->vertex,vertex,512,&vertex_words):nn_pipeline_vertex_encode(recipe,&state->vertex,vertex,512,&vertex_words))))return e;
 struct nn_render_target_info rt={platform->chip,r->window_origin,color,depth?1u:0u,1};
 struct nn_render_state color_render=state->render;
#if NN_PIPELINE_DEPTH_NATIVE
 if(depth&&(e=nn_depth_native_render(&state->render,controls,&color_render)))return e;
#endif
 if((e=depth?nn_render_state_encode_depth_raster(&color_render,&rt,render,128,&render_words):nn_render_state_encode(&color_render,&rt,render,128,&render_words)))return e;
 struct nn_pipeline_submission result={0};if(color&&(e=resource(&result,state->target.buffer.bo)))return e;
#if NN_PIPELINE_DEPTH_NATIVE
 if(depth){
  nn_bo*program_bos[4];size_t count=0;
  if((e=nn_pipeline_program_resources(program,program_bos,4,&count)))return e;
  for(size_t i=0;i<count;i++)if((e=depth_alias(attachment,(struct nn_pipeline_buffer){program_bos[i],0,nn_bo_size(program_bos[i])})))return e;
  if((e=resource(&result,attachment->storage)))return e;
 }
#endif
 for(size_t i=0;i<recipe->input_count;i++){
  const struct nn_pipeline_attribute *a=state->vertex.attributes+recipe->inputs[i].location;
  if((e=resource(&result,state->vertex.buffers[a->stream].bo)))return e;
 }
 uint32_t storage_descriptors[32*16]={0};
#if NN_PIPELINE_GRAPHICS_STORAGE
 if(state->storage_types){
  nn_bo *program_bos[4];size_t n=0;if((e=nn_pipeline_program_resources(program,program_bos,4,&n)))return e;
  for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<16;i++)if(state->storage_types->mask[stage]&(1u<<i)){
   struct nn_pipeline_buffer v=state->storage[stage][i];
   for(size_t j=0;j<n;j++)if(v.bo==program_bos[j])return ENOTSUP;
   if((e=nn_pipeline_storage_descriptor(v,storage_descriptors+(stage*16+i)*16))||(e=resource(&result,v.bo)))return e;
  }
 }
#endif
 uint32_t descriptors[16*16]={0},samplers[16*4]={0};
 for(unsigned i=0;i<16;i++) {
  if(state->texture_bound&(1u<<i)) {
   uint64_t a=0;const struct nn_fdl_image_view *bc=state->sampled_fdl[i];
   const struct nn_fdl_volume_view *volume=state->sampled_volume[i];
   const struct nn_buffer_sampled_binding *buf=state->sampled_buffer[i];
   const struct nn_fdl_image_view *color=state->sampled_color[i];
   const struct nn_depth_fdl_view *sampled_depth=state->sampled_depth[i];
   if(buf){
#if NN_PIPELINE_BUFFER_SAMPLED
    if((e=nn_buffer_sampled_descriptor(buf,descriptors+i*16))||(e=resource(&result,buf->resource)))return e;
    continue;
#else
    return ENOTSUP;
#endif
   }
   if(!bc&&!volume&&!color&&!sampled_depth&&(e=image(state->textures+i,&a)))return e;
   const struct nn_tex_native_view *tv=state->textures[i].layout;
   if(state->textures[i].buffer.bo==state->target.buffer.bo)return ENOTSUP; /* no feedback protocol */
   if(sampled_depth){
#if NN_PIPELINE_DEPTH_SAMPLED
    uint32_t operations;if((e=nn_depth_sampled_program(program,recipe,i,&operations))||
      (e=nn_depth_sampled_descriptor(sampled_depth,&state->textures[i].buffer,descriptors+i*16)))return e;
#else
    return ENOTSUP;
#endif
   }else if(color){
#if NN_PIPELINE_SAMPLED_IMAGES
    uint32_t operations;const struct nn_pipeline_buffer *b=&state->textures[i].buffer;
    struct nn_sampled_image_binding binding={b->bo,b->offset,b->bytes,b->offset,b->bytes};
    if((e=nn_sampled_image_program(program,recipe,i,&operations))||(e=nn_sampled_image_descriptor(color,&binding,descriptors+i*16)))return e;
#else
    return ENOTSUP;
#endif
   }else if(volume){
#if NN_PIPELINE_VOLUME_SAMPLED
    if((e=nn_volume_sampled_descriptor(volume,state->textures[i].buffer.bo,state->textures[i].buffer.offset,state->textures[i].buffer.bytes,descriptors+i*16)))return e;
#else
    return ENOTSUP;
#endif
   }else if(bc){
#if NN_PIPELINE_BC_SAMPLED
    if((e=nn_bc_sampled_descriptor(bc,state->textures[i].buffer.bo,state->textures[i].buffer.offset,state->textures[i].buffer.bytes,descriptors+i*16)))return e;
#else
    return ENOTSUP;
#endif
   }
   else{
   memcpy(descriptors+i*16,tv->texture_descriptor,64);
   uint64_t field=(uint64_t)descriptors[i*16+4]|(uint64_t)descriptors[i*16+5]<<32;
   field=(field&~tv->descriptor_address_mask)|(a&tv->descriptor_address_mask);descriptors[i*16+4]=(uint32_t)field;descriptors[i*16+5]=(uint32_t)(field>>32);}
   if((e=resource(&result,state->textures[i].buffer.bo)))return e;
  }
  if(state->sampler_bound&(1u<<i))if((e=sampler_encode(state,platform->chip,i,samplers+i*4)))return e;
 }
 struct builder builder={0};
 /* Preflight Draw before allocation; no CPU reads from indirect/index data. */
 uint32_t driver_words[16]={0},driver_dst=0;
#if NN_PIPELINE_RUNTIME_CONSTANTS
 if((e=nn_pipeline_runtime_draw(state->runtime_constants,&state->draw,state->runtime_draw_id,driver_words,&driver_dst)))return e;
#endif
 if((e=draw(&builder,&state->draw,&result,driver_dst)))return e;
 uint32_t draw_words[24];size_t draw_count=builder.count;if(draw_count>24)return EOVERFLOW;
 memcpy(draw_words,builder.words,draw_count*4);builder.count=0;
 nn_bo *job=NULL;if((e=nn_bo_alloc(device,JOB_BYTES,&job))){*retained=job;return e;}
 if(!nn_bo_map(job)||nn_bo_size(job)<JOB_BYTES||!nn_bo_address(job)||nn_bo_address(job)%128||nn_bo_address(job)>UINT64_MAX-nn_bo_size(job)){e=EOVERFLOW;goto failed;}
 memset(nn_bo_map(job),0,nn_bo_size(job));
 memcpy((unsigned char *)nn_bo_map(job)+SAMPLER_OFFSET,samplers,sizeof(samplers));
 memcpy((unsigned char *)nn_bo_map(job)+TEXTURE_OFFSET,descriptors,sizeof(descriptors));
#if NN_PIPELINE_RUNTIME_CONSTANTS
 memcpy((unsigned char *)nn_bo_map(job)+DRIVER_OFFSET,driver_words,sizeof(driver_words));
#else
 (void)driver_words;
#endif
#if NN_PIPELINE_GRAPHICS_STORAGE
 memcpy((unsigned char *)nn_bo_map(job)+STORAGE_OFFSET,storage_descriptors,sizeof(storage_descriptors));
#else
 (void)storage_descriptors;
#endif
 if((e=segment(&builder,&platform->begin,job)))goto failed;
 if(color){
#if NN_PIPELINE_COLOR_TARGETS
 if(v->format==37){
 reg(&builder,0x8822,v->rb_mrt_buf_info);reg(&builder,0x8823,v->rb_mrt_pitch);reg(&builder,0x8824,v->rb_mrt_array_pitch);
 reg_address(&builder,0x8825,target);reg(&builder,0x8827,0);reg(&builder,0xa996,v->sp_ps_mrt_reg);
 reg_address(&builder,0x8903,target);reg(&builder,0x8905,0);reg(&builder,0x8102,v->sp_ps_mrt_reg);
 }else{
 uint32_t color_words[14];size_t color_count=0;
 if((e=nn_color_target_words(v,color_words,14,&color_count))||
    (e=append(&builder,color_words,color_count)))goto failed;
 reg_address(&builder,0x8825,target);reg(&builder,0x8827,0);
 reg_address(&builder,0x8903,target);reg(&builder,0x8905,0);
 }
#else
 reg(&builder,0x8822,v->rb_mrt_buf_info);reg(&builder,0x8823,v->rb_mrt_pitch);reg(&builder,0x8824,v->rb_mrt_array_pitch);
 reg_address(&builder,0x8825,target);reg(&builder,0x8827,0);reg(&builder,0xa996,v->sp_ps_mrt_reg);
 reg_address(&builder,0x8903,target);reg(&builder,0x8905,0);reg(&builder,0x8102,v->sp_ps_mrt_reg);
#endif
 }
 reg(&builder,0x80f0,0);reg(&builder,0x80f1,(width-1)|((height-1)<<16));
 struct nn_pipeline_shader_bindings binding={.job=job,.command_offset=builder.count*4,.sampler_offset=SAMPLER_OFFSET,.texture_offset=TEXTURE_OFFSET,.storage_offset=STORAGE_OFFSET,.driver_params_offset=DRIVER_OFFSET};
 memcpy(binding.ubo,state->ubo,sizeof(binding.ubo));size_t shader_words=0;
 if((e=(depth?nn_pipeline_shader_encode_depth(program,depth,&binding,COMMAND_CAPACITY-builder.count,&shader_words):nn_pipeline_shader_encode(program,recipe,&binding,COMMAND_CAPACITY-builder.count,&shader_words))))goto failed;
 if((e=append(&builder,(const uint32_t *)((const unsigned char *)nn_bo_map(job)+binding.command_offset),shader_words)))goto failed;
 nn_bo *shader_bos[64];size_t shader_count=0;
 if((e=(depth?nn_pipeline_shader_resources_depth(program,depth,&binding,shader_bos,64,&shader_count):nn_pipeline_shader_resources(program,recipe,&binding,shader_bos,64,&shader_count))))goto failed;
 for(size_t i=0;i<shader_count;i++)if((e=resource(&result,shader_bos[i])))goto failed;
 if((e=append(&builder,vertex,vertex_words)))goto failed;
 for(unsigned i=0;i<6;i++)reg(&builder,0x8010+i,bits(r->viewport[i]));
 uint32_t tl=r->scissor_x|(r->scissor_y<<16),br=(r->scissor_x+r->scissor_width-1)|((r->scissor_y+r->scissor_height-1)<<16);
 reg(&builder,0x80d0,tl);reg(&builder,0x80d1,br);reg(&builder,0x80b0,tl);reg(&builder,0x80b1,br);reg(&builder,0x8006,0);
 reg(&builder,0x8000,0x80u|(r->depth_mode?0x40u:0));
 if((e=append(&builder,render,render_words)))goto failed;
#if NN_PIPELINE_DEPTH_NATIVE
 if(depth){uint32_t depth_words[96];size_t count=0;
  if((e=nn_depth_native_words(attachment,depth,controls,depth_words,96,&count))||(e=append(&builder,depth_words,count)))goto failed;}
#endif
 if((e=append(&builder,draw_words,draw_count))||(e=segment(&builder,&platform->end,job)))goto failed;
 memcpy(nn_bo_map(job),builder.words,builder.count*4);
 if((e=nn_bo_cache(job,0,nn_bo_size(job),0)))goto failed;
 result.job=job;result.command_bytes=builder.count*4;*out=result;return 0;
failed:
 {int cleanup=nn_bo_destroy(job);if(cleanup)*retained=job;}
 return e;
}

int nn_pipeline_preflight(nn_device*d,const struct nn_offline_graphics_stage*const stages[2],const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_platform*p,const struct nn_pipeline_draw_state*s,uint32_t planned){return pipeline_preflight(d,stages,r,p,s,planned,NULL,NULL,NULL,NULL);}
int nn_pipeline_preflight_depth(nn_device*d,const struct nn_offline_graphics_stage*const stages[2],const struct nn_pipeline_depth_export*depth,const struct nn_pipeline_platform*p,const struct nn_pipeline_draw_state*s,const struct nn_depth_attachment*a,const struct nn_depth_controls_desc*c){return !depth?EINVAL:pipeline_preflight(d,stages,depth->recipe,p,s,0,depth,a,c,NULL);}
int nn_pipeline_encode(nn_device*d,const nn_pipeline_program*program,const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_platform*p,const struct nn_pipeline_draw_state*s,struct nn_pipeline_submission*out,nn_bo**retained){return pipeline_encode(d,program,r,p,s,out,retained,NULL,NULL,NULL);}
int nn_pipeline_encode_depth(nn_device*d,const nn_pipeline_program*program,const struct nn_pipeline_depth_export*depth,const struct nn_pipeline_platform*p,const struct nn_pipeline_draw_state*s,const struct nn_depth_attachment*a,const struct nn_depth_controls_desc*c,struct nn_pipeline_submission*out,nn_bo**retained){if(!depth){if(out)memset(out,0,sizeof(*out));if(retained)*retained=NULL;return EINVAL;}return pipeline_encode(d,program,depth->recipe,p,s,out,retained,depth,a,c);}

int nn_pipeline_preflight_depth_planned(nn_device*d,const struct nn_offline_graphics_stage*const stages[2],const struct nn_pipeline_depth_export*depth,const struct nn_pipeline_platform*p,const struct nn_pipeline_draw_state*s,uint32_t images,const struct nn_depth_fdl_view*v,const struct nn_depth_controls_desc*c)
{return !depth?EINVAL:pipeline_preflight(d,stages,depth->recipe,p,s,images,depth,NULL,c,v);}
