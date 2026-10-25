/* SPDX-License-Identifier: MIT. Effective GPU-state -> existing native generic
 * pipeline capture. Native validation/compilation remains in the same source
 * pipeline; Call never copies a stale per-Draw recording snapshot as state. */
static int command_draw_render(const struct nn_command_state *state,struct nn_render_state *render)
{
 const uint64_t required=NN_COMMAND_STATE_COLOR|NN_COMMAND_STATE_MASK|NN_COMMAND_STATE_POLYGON|
  NN_COMMAND_STATE_DEPTH_STENCIL|NN_COMMAND_STATE_MULTISAMPLE|NN_COMMAND_STATE_DEPTH_BOUNDS|
  NN_COMMAND_STATE_VIEWPORT|NN_COMMAND_STATE_SCISSOR|NN_COMMAND_STATE_DEPTH_RANGE;
 if((state->defined&required)!=required)return ENOTSUP;
 struct nn_command_state normalized={0};nn_render_state_defaults(&normalized.render);
 int e=nn_command_state_overlay_render(&normalized,state);if(e)return e;
 /* Defaults are only internal values for inactive registers on this finite
  * one-color/no-Z/triangle profile. They never become explicitly defined in
  * the sequence or queue continuation. Active fields always need evidence. */
 const struct nn_render_state *r=&normalized.render;
 if(r->polygon_mode>2)return EINVAL;
 if((r->polygon_offset_enables&(1u<<r->polygon_mode))&&!(state->defined&NN_COMMAND_STATE_POLYGON_OFFSET))return ENOTSUP;
 if(r->logic_op==NVN_LOGIC_OP_COPY&&(r->blend_enable&1)){
  if(!(state->defined&NN_COMMAND_STATE_BLEND(0)))return ENOTSUP;
  const struct nn_render_blend *b=r->blend;const uint32_t factors[]={b->src_rgb,b->dst_rgb,b->src_alpha,b->dst_alpha};
  for(unsigned i=0;i<4;i++)if(factors[i]>=NVN_BLEND_FUNC_CONSTANT_COLOR&&factors[i]<=NVN_BLEND_FUNC_ONE_MINUS_CONSTANT_ALPHA&&!(state->defined&NN_COMMAND_STATE_BLEND_COLOR))return ENOTSUP;
 }
 *render=normalized.render;return 0;
}
static struct object *command_draw_object(const struct nn_command_binding *b,enum kind kind,struct object *device)
{
 struct object *o=b?live_weak(b->owner,kind):NULL;
 return o&&o->device==device?o:NULL;
}
static int command_draw_bound(const struct nn_command_binding *b,struct object *device,struct bound *out)
{
 *out=(struct bound){0};if(!b||!b->owner)return 0;
 struct object *pool=command_draw_object(b,POOL,device);if(!pool)return EINVAL;
 if(b->offset>pool->size||b->bytes>pool->size-b->offset||b->offset>UINT64_MAX-nn_bo_address(pool->bo)||b->address!=nn_bo_address(pool->bo)+b->offset)return EINVAL;
 *out=(struct bound){pool,b->offset,b->bytes,b->address};return 0;
}
static int command_draw_recording(const struct nn_command_state *state,struct object *device,struct recording *r)
{
 memset(r,0,sizeof(*r));int e=command_draw_render(state,&r->render);if(e)return e;
 const struct nn_command_binding *vs=nn_command_state_binding(state,NN_COMMAND_PROGRAM,NVN_SHADER_STAGE_VERTEX,0),*fs=nn_command_state_binding(state,NN_COMMAND_PROGRAM,NVN_SHADER_STAGE_FRAGMENT,0);
 struct object *program=command_draw_object(vs,PROGRAM,device);
 if(!program||command_draw_object(fs,PROGRAM,device)!=program||!program->pipeline_source.archive)return ENOTSUP;
 r->graphics_program=program;
 const struct nn_command_binding *target=nn_command_state_binding(state,NN_COMMAND_TARGET,0,0);
 r->target=command_draw_object(target,TEXTURE,device);
 const struct nn_command_binding *depth=nn_command_state_binding(state,NN_COMMAND_TARGET,0,8);
 if(depth&&depth->owner){
#if NN_NVN_DEPTH_TARGETS
  r->depth=command_draw_object(depth,TEXTURE,device);if(!r->depth||depth->flags!=1)return EINVAL;
  r->depth_view=(struct public_depth_view){(uint32_t)depth->guest,(uint32_t)(depth->guest>>32)};
  struct nn_depth_shadow_plan plan;int e=public_depth_plan(r->depth,r->depth_view,&plan);if(e)return e;
  if(depth->offset!=r->depth->offset||depth->bytes!=r->depth->size||depth->address!=nn_bo_address(r->depth->bo)+r->depth->offset)return EINVAL;
#else
  return ENOTSUP;
#endif
 }
 if(!r->target&&!r->depth)return ENOTSUP;
 for(unsigned i=0;i<4;i++){
  if(!isfinite(state->viewport[i])||state->viewport[i]<0||state->viewport[i]>16384||state->viewport[i]!=(float)(int)state->viewport[i])return ENOTSUP;
  r->viewport[i]=(int)state->viewport[i];r->scissor[i]=state->scissor[i];
 }
 r->viewport_set=r->scissor_set=r->depth_range_set=1;memcpy(r->depth_range,state->depth_range,sizeof(r->depth_range));
 const struct nn_command_binding *attrib=nn_command_state_binding(state,NN_COMMAND_VERTEX_ATTRIB,0,0),*stream=nn_command_state_binding(state,NN_COMMAND_VERTEX_STREAM,0,0);
 if(!attrib||!stream||!attrib->flags||attrib->flags>16||!stream->flags||stream->flags>16)return ENOTSUP;
 r->attributes_set=attrib->flags;r->vertex_stream_count=stream->flags;r->stream_set=stream->flags;
 for(unsigned i=0;i<32;i++){
  const struct nn_command_binding *a=nn_command_state_binding(state,NN_COMMAND_VERTEX_ATTRIB,0,i),*s=nn_command_state_binding(state,NN_COMMAND_VERTEX_STREAM,0,i);
  if(i<r->attributes_set&&(!a||a->flags!=r->attributes_set||a->address>UINT32_MAX))return ENOTSUP;
  if(i<r->vertex_stream_count&&(!s||s->flags!=r->vertex_stream_count||s->address>UINT32_MAX||s->offset>UINT32_MAX))return ENOTSUP;
  if(a)r->attributes[i]=(uint32_t)a->address;
  if(s)r->vertex_streams[i]=(struct vertex_stream){s->address,s->offset};
  if(i<16&&(e=command_draw_bound(nn_command_state_binding(state,NN_COMMAND_VERTEX_BUFFER,0,i),device,r->vertices+i)))return e;
 }
 for(unsigned i=0;i<r->attributes_set;i++)if(r->attributes[i]&255u){unsigned index=r->attributes[i]>>24;if(index>=r->vertex_stream_count||!r->vertices[index].pool)return ENOTSUP;}
 r->vertex=r->vertices[0];r->stream=r->vertex_streams[0];
 for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<14;i++)if((e=command_draw_bound(nn_command_state_binding(state,NN_COMMAND_UBO,stage,i),device,&r->graphics_ubos[stage][i])))return e;
#if NN_NVN_GRAPHICS_STORAGE
 for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<16;i++)if((e=command_draw_bound(nn_command_state_binding(state,NN_COMMAND_SSBO,stage,i),device,&r->graphics_storage[stage][i])))return e;
#endif
 r->texture_pool=command_draw_object(nn_command_state_binding(state,NN_COMMAND_TEXTURE_POOL,0,0),TPOOL,device);
 r->sampler_pool=command_draw_object(nn_command_state_binding(state,NN_COMMAND_SAMPLER_POOL,0,0),SPOOL,device);
 for(unsigned domain=0;domain<3;domain++)for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<32;i++){
  enum nn_command_binding_domain d=domain==0?NN_COMMAND_COMBINED_TEXTURE:domain==1?NN_COMMAND_SEPARATE_TEXTURE:NN_COMMAND_SEPARATE_SAMPLER;
  const struct nn_command_binding *b=nn_command_state_binding(state,d,stage,i);if(!b)continue;
  uint64_t *values=domain==0?r->combined[stage]:domain==1?r->separate_textures[stage]:r->separate_samplers[stage];values[i]=b->address;
  struct texture_handle_record *carrier=r->handle_carriers+domain;uint32_t bit=UINT32_C(1)<<i;
  carrier->guest[stage][i]=b->guest;carrier->low_defined[stage]|=bit;
  if(b->flags&NN_COMMAND_BINDING_HIGH_DEFINED){carrier->high[stage][i]=(uint32_t)b->offset;carrier->high_defined[stage]|=bit;}
  if(b->flags&NN_COMMAND_BINDING_LAST_PLURAL)carrier->last_plural[stage]|=bit;
 }
 return 0;
}
static int command_project_draw(struct handle *destination,const struct late_draw *source,const struct nn_command_state *state,size_t offset)
{
 if(!source->generic_pipeline)return ENOTSUP;
 struct recording recording;int e=command_draw_recording(state,destination->device,&recording);if(e)return e;
 struct object command={.kind=COMMAND,.device=destination->device,.record=&recording};struct pipeline_draw_capture capture={0};
 const struct nn_gfx_draw *draw=&source->pipeline.native.draw;
 e=pipeline_draw_capture_create(&command,&recording,(NVNdrawPrimitive)draw->primitive,(int)draw->first,(int)draw->count,draw,source->pipeline.queries.window_origin,source->pipeline.queries.depth_mode,&capture);
 if(e)return e;
 for(size_t i=0;i<capture.held;i++)if((e=add_resource(destination,capture.holds[i]))){pipeline_draw_capture_discard(&capture);return e;}
 while(capture.held){struct object *o=capture.holds[--capture.held];if(o)o->refs--;}
 struct late_draw *result=capture.draw;capture.draw=NULL;result->command_offset=offset;
 result->next=destination->draws;destination->draws=result;return 0;
}
