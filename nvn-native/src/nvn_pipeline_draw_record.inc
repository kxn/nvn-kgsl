static int pipeline_format_metadata(NVNformat f)
{switch(f){
case NVN_FORMAT_NONE:
case NVN_FORMAT_R8:
case NVN_FORMAT_R8SN:
case NVN_FORMAT_R8UI:
case NVN_FORMAT_R8I:
case NVN_FORMAT_R16F:
case NVN_FORMAT_R16:
case NVN_FORMAT_R16SN:
case NVN_FORMAT_R16UI:
case NVN_FORMAT_R16I:
case NVN_FORMAT_R32F:
case NVN_FORMAT_R32UI:
case NVN_FORMAT_R32I:
case NVN_FORMAT_RG8:
case NVN_FORMAT_RG8SN:
case NVN_FORMAT_RG8UI:
case NVN_FORMAT_RG8I:
case NVN_FORMAT_RG16F:
case NVN_FORMAT_RG16:
case NVN_FORMAT_RG16SN:
case NVN_FORMAT_RG16UI:
case NVN_FORMAT_RG16I:
case NVN_FORMAT_RG32F:
case NVN_FORMAT_RG32UI:
case NVN_FORMAT_RG32I:
case NVN_FORMAT_RGB8:
case NVN_FORMAT_RGB8SN:
case NVN_FORMAT_RGB8UI:
case NVN_FORMAT_RGB8I:
case NVN_FORMAT_RGB16F:
case NVN_FORMAT_RGB16:
case NVN_FORMAT_RGB16SN:
case NVN_FORMAT_RGB16UI:
case NVN_FORMAT_RGB16I:
case NVN_FORMAT_RGB32F:
case NVN_FORMAT_RGB32UI:
case NVN_FORMAT_RGB32I:
case NVN_FORMAT_RGBA8:
case NVN_FORMAT_RGBA8SN:
case NVN_FORMAT_RGBA8UI:
case NVN_FORMAT_RGBA8I:
case NVN_FORMAT_RGBA16F:
case NVN_FORMAT_RGBA16:
case NVN_FORMAT_RGBA16SN:
case NVN_FORMAT_RGBA16UI:
case NVN_FORMAT_RGBA16I:
case NVN_FORMAT_RGBA32F:
case NVN_FORMAT_RGBA32UI:
case NVN_FORMAT_RGBA32I:
case NVN_FORMAT_STENCIL8:
case NVN_FORMAT_DEPTH16:
case NVN_FORMAT_DEPTH24:
case NVN_FORMAT_DEPTH32F:
case NVN_FORMAT_DEPTH24_STENCIL8:
case NVN_FORMAT_DEPTH32F_STENCIL8:
case NVN_FORMAT_RGBX8_SRGB:
case NVN_FORMAT_RGBA8_SRGB:
case NVN_FORMAT_RGBA4:
case NVN_FORMAT_RGB5:
case NVN_FORMAT_RGB5A1:
case NVN_FORMAT_RGB565:
case NVN_FORMAT_RGB10A2:
case NVN_FORMAT_RGB10A2UI:
case NVN_FORMAT_R11G11B10F:
case NVN_FORMAT_RGB9E5F:
case NVN_FORMAT_RGB_DXT1:
case NVN_FORMAT_RGBA_DXT1:
case NVN_FORMAT_RGBA_DXT3:
case NVN_FORMAT_RGBA_DXT5:
case NVN_FORMAT_RGB_DXT1_SRGB:
case NVN_FORMAT_RGBA_DXT1_SRGB:
case NVN_FORMAT_RGBA_DXT3_SRGB:
case NVN_FORMAT_RGBA_DXT5_SRGB:
case NVN_FORMAT_RGTC1_UNORM:
case NVN_FORMAT_RGTC1_SNORM:
case NVN_FORMAT_RGTC2_UNORM:
case NVN_FORMAT_RGTC2_SNORM:
case NVN_FORMAT_BPTC_UNORM:
case NVN_FORMAT_BPTC_UNORM_SRGB:
case NVN_FORMAT_BPTC_SFLOAT:
case NVN_FORMAT_BPTC_UFLOAT:
case NVN_FORMAT_R8_UI2F:
case NVN_FORMAT_R8_I2F:
case NVN_FORMAT_R16_UI2F:
case NVN_FORMAT_R16_I2F:
case NVN_FORMAT_R32_UI2F:
case NVN_FORMAT_R32_I2F:
case NVN_FORMAT_RG8_UI2F:
case NVN_FORMAT_RG8_I2F:
case NVN_FORMAT_RG16_UI2F:
case NVN_FORMAT_RG16_I2F:
case NVN_FORMAT_RG32_UI2F:
case NVN_FORMAT_RG32_I2F:
case NVN_FORMAT_RGB8_UI2F:
case NVN_FORMAT_RGB8_I2F:
case NVN_FORMAT_RGB16_UI2F:
case NVN_FORMAT_RGB16_I2F:
case NVN_FORMAT_RGB32_UI2F:
case NVN_FORMAT_RGB32_I2F:
case NVN_FORMAT_RGBA8_UI2F:
case NVN_FORMAT_RGBA8_I2F:
case NVN_FORMAT_RGBA16_UI2F:
case NVN_FORMAT_RGBA16_I2F:
case NVN_FORMAT_RGBA32_UI2F:
case NVN_FORMAT_RGBA32_I2F:
case NVN_FORMAT_RGB10A2SN:
case NVN_FORMAT_RGB10A2I:
case NVN_FORMAT_RGB10A2_UI2F:
case NVN_FORMAT_RGB10A2_I2F:
case NVN_FORMAT_RGBX8:
case NVN_FORMAT_RGBX8SN:
case NVN_FORMAT_RGBX8UI:
case NVN_FORMAT_RGBX8I:
case NVN_FORMAT_RGBX16F:
case NVN_FORMAT_RGBX16:
case NVN_FORMAT_RGBX16SN:
case NVN_FORMAT_RGBX16UI:
case NVN_FORMAT_RGBX16I:
case NVN_FORMAT_RGBX32F:
case NVN_FORMAT_RGBX32UI:
case NVN_FORMAT_RGBX32I:
case NVN_FORMAT_RGBA_ASTC_4x4:
case NVN_FORMAT_RGBA_ASTC_5x4:
case NVN_FORMAT_RGBA_ASTC_5x5:
case NVN_FORMAT_RGBA_ASTC_6x5:
case NVN_FORMAT_RGBA_ASTC_6x6:
case NVN_FORMAT_RGBA_ASTC_8x5:
case NVN_FORMAT_RGBA_ASTC_8x6:
case NVN_FORMAT_RGBA_ASTC_8x8:
case NVN_FORMAT_RGBA_ASTC_10x5:
case NVN_FORMAT_RGBA_ASTC_10x6:
case NVN_FORMAT_RGBA_ASTC_10x8:
case NVN_FORMAT_RGBA_ASTC_10x10:
case NVN_FORMAT_RGBA_ASTC_12x10:
case NVN_FORMAT_RGBA_ASTC_12x12:
case NVN_FORMAT_RGBA_ASTC_4x4_SRGB:
case NVN_FORMAT_RGBA_ASTC_5x4_SRGB:
case NVN_FORMAT_RGBA_ASTC_5x5_SRGB:
case NVN_FORMAT_RGBA_ASTC_6x5_SRGB:
case NVN_FORMAT_RGBA_ASTC_6x6_SRGB:
case NVN_FORMAT_RGBA_ASTC_8x5_SRGB:
case NVN_FORMAT_RGBA_ASTC_8x6_SRGB:
case NVN_FORMAT_RGBA_ASTC_8x8_SRGB:
case NVN_FORMAT_RGBA_ASTC_10x5_SRGB:
case NVN_FORMAT_RGBA_ASTC_10x6_SRGB:
case NVN_FORMAT_RGBA_ASTC_10x8_SRGB:
case NVN_FORMAT_RGBA_ASTC_10x10_SRGB:
case NVN_FORMAT_RGBA_ASTC_12x10_SRGB:
case NVN_FORMAT_RGBA_ASTC_12x12_SRGB:
case NVN_FORMAT_BGR565:
case NVN_FORMAT_BGR5:
case NVN_FORMAT_BGR5A1:
case NVN_FORMAT_A1BGR5:
case NVN_FORMAT_BGRX8:
case NVN_FORMAT_BGRA8:
case NVN_FORMAT_BGRX8_SRGB:
case NVN_FORMAT_BGRA8_SRGB:
return 1;default:return 0;}}
struct pipeline_draw_capture {struct late_draw *draw;struct object *holds[8];size_t held;};
#if NN_NVN_GRAPHICS_SOURCE_CACHE
int nn_nvn_register_pipeline_views(NVNdevice *key,const struct nn_tex_native_view *views,size_t count)
{
 struct object *d=find(key,DEVICE);if(!d)return nn_nvn_error();
 if(!d->pipeline_archive||!views||!count||count>128)return EINVAL;
 if(d->pipeline_views)return EEXIST;
 for(struct object *o=objects;o;o=o->next)if(o->kind==PROGRAM&&o->device==d)return EBUSY;
 for(size_t i=0;i<count;i++) {
  const struct nn_tex_native_view *v=views+i;
  if(v->schema!=1||v->chip!=nn_device_chip(d->gpu)||v->format!=37||v->target!=1||
     v->levels!=1||v->layers!=1||v->samples!=1||v->tile_mode||v->ubwc||
     !v->width||v->width>16384||!v->height||v->height>16384||v->pitch<v->width*4u||
     v->required_span_bytes<v->bytes||v->required_span_bytes<v->layer_bytes)return ENOTSUP;
  for(size_t j=0;j<i;j++)if(v->width==views[j].width&&v->height==views[j].height&&v->pitch==views[j].pitch)return EEXIST;
 }
 d->pipeline_views=views;d->pipeline_view_count=count;return 0;
}
int nn_nvn_enable_pipeline_runtime_views(NVNdevice *key)
{
#if NN_PIPELINE_RUNTIME_VIEWS
 struct object *d=find(key,DEVICE);if(!d)return nn_nvn_error();
 if(!d->pipeline_archive)return EINVAL;
 if(d->pipeline_runtime_views)return EEXIST;
 for(struct object *o=objects;o;o=o->next)if(o->kind==PROGRAM&&o->device==d)return EBUSY;
 return nn_pipeline_views_create(nn_device_chip(d->gpu),&d->pipeline_runtime_views);
#else
 (void)key;return ENOTSUP;
#endif
}
static int pipeline_target_view(struct object *d,const struct object *target,const struct nn_tex_native_view **out)
{
 *out=NULL;
#if NN_NVN_COLOR_TARGETS
 if(d->pipeline_runtime_views){
  if(target->texture.target!=NVN_TEXTURE_TARGET_2D||target->texture.levels!=1||target->texture.depth!=1||target->texture.samples||
     target->texture.width<1||target->texture.height<1||(!target->source_layout&&(target->texture.stride<0||(uint64_t)target->texture.stride>UINT32_MAX)))return ENOTSUP;
  const struct nn_tex_native_view *view=NULL;
  int e=nn_pipeline_views_get_format(d->pipeline_runtime_views,(uint32_t)target->texture.format,(uint32_t)target->texture.width,
    (uint32_t)target->texture.height,target->source_layout?0:(uint32_t)target->texture.stride,&view);if(e)return e;
  if(!target->source_layout&&target->size<view->required_span_bytes)return EINVAL;
  *out=view;return 0;
 }
 if(target->texture.format!=NVN_FORMAT_RGBA8)return ENOTSUP;
#endif
#if NN_PIPELINE_RUNTIME_VIEWS
 if(d->pipeline_runtime_views){
  if(target->texture.format!=NVN_FORMAT_RGBA8||target->texture.target!=NVN_TEXTURE_TARGET_2D||
     target->texture.levels!=1||target->texture.depth!=1||target->texture.samples||
     target->texture.width<1||target->texture.height<1)return ENOTSUP;
  if(!target->source_layout&&(target->texture.stride<0||(uint64_t)target->texture.stride>UINT32_MAX))return ENOTSUP;
  const struct nn_tex_native_view *view=NULL;
  int e=nn_pipeline_views_get(d->pipeline_runtime_views,(uint32_t)target->texture.width,
   (uint32_t)target->texture.height,target->source_layout?0:(uint32_t)target->texture.stride,&view);if(e)return e;
  if(!target->source_layout&&target->size<view->required_span_bytes)return EINVAL;
  *out=view;return 0;
 }
#endif
 const struct nn_tex_native_view *found=NULL;
 for(size_t i=0;i<d->pipeline_view_count;i++) {
  const struct nn_tex_native_view *v=d->pipeline_views+i;
  if(v->width!=(unsigned)target->texture.width||v->height!=(unsigned)target->texture.height)continue;
  /* Existing finite qualification policy is unchanged when runtime is off. */
  if(!target->source_layout&&(target->texture.stride!=(ptrdiff_t)v->pitch||target->size<v->required_span_bytes))continue;
  if(found)return ENOTSUP;
  found=v;
 }
 if(!found)return ENOTSUP;
 *out=found;return 0;
}
/* A recorded generic Draw owns only immutable CPU values and lifetime pins.
 * It does not select a specialization or consume a descriptor entry. */
static void pipeline_record_release(struct pipeline_draw_snapshot *p)
{
 struct pipeline_record_owner *o=p->owner;p->owner=NULL;
 if(!o)return;
 if(!o->references){error(EPROTO);return;}
 if(--o->references)return;
 while(o->count)o->pins[--o->count]->refs--;
 free(o);
}
static int pipeline_record_retain(struct pipeline_draw_snapshot *p)
{
 if(!p->owner||!p->owner->references)return EINVAL;
 if(p->owner->references==SIZE_MAX)return EOVERFLOW;
 p->owner->references++;return 0;
}
static int pipeline_record_pin(struct pipeline_record_owner *owner,struct object *o)
{
 if(!o)return 0;
 for(size_t i=0;i<owner->count;i++)if(owner->pins[i]==o)return 0;
 if(owner->count>=sizeof(owner->pins)/sizeof(*owner->pins))return EOVERFLOW;
 if(o->refs==SIZE_MAX)return EOVERFLOW;
 owner->pins[owner->count++]=o;o->refs++;return 0;
}
#if NN_NVN_COLOR_TARGETS
static int pipeline_target_admit(struct object *device,const struct object *target)
{
 const struct texture_desc *t=&target->texture;
 if(t->target!=NVN_TEXTURE_TARGET_2D||t->levels!=1||t->depth!=1||t->samples)return ENOTSUP;
 const struct nn_tex_native_view *view=NULL;int e=pipeline_target_view(device,target,&view);if(e)return e;
 if(target->source_layout){
  if(!device->block_copy)return ENOTSUP;
  struct nn_color_target_shadow_plan plan;return nn_color_target_shadow_plan(target->source_layout,view,&plan);
 }
 size_t bytes=0;if((e=linear_texture_bytes(t,&bytes)))return e;
 return bytes==target->size?0:EINVAL;
}
#endif
static int pipeline_draw_capture_create(struct object *o,struct recording *r,NVNdrawPrimitive primitive,int first,int count,const struct nn_gfx_draw *parameters,unsigned window_origin,unsigned depth_mode,struct pipeline_draw_capture *capture)
{
 *capture=(struct pipeline_draw_capture){0};
 struct object *program=r->graphics_program,*target=r->target;
 if(!program||!program->pipeline_source.archive||(!target&&!r->depth)||!r->attributes_set||!r->stream_set||
    !r->viewport_set||!r->scissor_set||!r->depth_range_set)return EINVAL;
 const struct nn_tex_native_view *view=NULL;uint32_t width=0,height=0;
 if(target){
  if(!o->device->pipeline_views&&!o->device->pipeline_runtime_views)return ENOTSUP;
  if(target->texture.target!=NVN_TEXTURE_TARGET_2D||target->texture.levels!=1||target->texture.depth!=1||target->texture.samples)return ENOTSUP;
#if !NN_NVN_COLOR_TARGETS
  if(target->texture.format!=NVN_FORMAT_RGBA8)return ENOTSUP;
#else
  int admitted=pipeline_target_admit(o->device,target);if(admitted)return admitted;
#endif
  int view_error=pipeline_target_view(o->device,target,&view);if(view_error)return view_error;
#if !NN_NVN_COLOR_TARGETS
  if(target->source_layout&&(!o->device->texture_bridge||nn_tex_bridge_source_validate(target->source_layout)))return ENOTSUP;
#endif
  width=view->width;height=view->height;
 }
 if(r->depth){
#if NN_NVN_DEPTH_TARGETS
  if(!o->device->pipeline_depth)return ENOTSUP;
  struct nn_depth_shadow_plan z;int e=public_depth_plan(r->depth,r->depth_view,&z);if(e)return e;
  if(width&&(width!=z.aliases.tiled.width||height!=z.aliases.tiled.height))return ENOTSUP;
  width=z.aliases.tiled.width;height=z.aliases.tiled.height;
#else
  return ENOTSUP;
#endif
 }
 if(r->viewport[0]||r->viewport[1]||r->viewport[2]!=(int)width||r->viewport[3]!=(int)height||
    r->scissor[0]<0||r->scissor[1]<0||r->scissor[2]<=0||r->scissor[3]<=0||
    (unsigned)r->scissor[0]>=width||(unsigned)r->scissor[2]>width-(unsigned)r->scissor[0]||
    (unsigned)r->scissor[1]>=height||(unsigned)r->scissor[3]>height-(unsigned)r->scissor[1]||
    r->depth_range[0]!=0||r->depth_range[1]!=1)return ENOTSUP;
 if(!r->depth&&(r->scissor[0]||r->scissor[1]||r->scissor[2]!=(int)width||r->scissor[3]!=(int)height))return ENOTSUP;
 struct nn_pipeline_frontend_queries queries={.version=1,.chip=nn_device_chip(o->device->gpu),.primitive=primitive,
  .depth_mode=depth_mode,.window_origin=window_origin,
  .render=r->render,.point_size=1};
 for(unsigned i=0;i<r->attributes_set;i++) {
  unsigned f=r->attributes[i]&255u;
  if(f&&f!=NVN_FORMAT_R32F&&f!=NVN_FORMAT_RG32F&&f!=NVN_FORMAT_RGB32F&&f!=NVN_FORMAT_RGBA32F&&
     f!=NVN_FORMAT_RGBA8&&f!=NVN_FORMAT_RGBA8SN)return ENOTSUP;
 }
 const struct nn_render_target_info target_info={queries.chip,queries.window_origin,target?1u:0u,r->depth?1u:0u,1};
 int e;
 if(r->depth){
#if NN_NVN_DEPTH_TARGETS
  struct nn_depth_controls_desc c={1,queries.chip,r->render.depth.depth_test,r->render.depth.depth_test&&r->render.depth.depth_write,r->render.depth.depth_func,0,252,0};
  struct nn_render_state color;size_t words=0;
  if((e=nn_depth_native_render(&r->render,&c,&color))||(e=nn_render_state_encode_depth_raster(&color,&target_info,NULL,0,&words)))return e;
#else
  return ENOTSUP;
#endif
 }else if((e=nn_render_state_validate(&r->render,&target_info)))return e;
 struct late_draw *draw=calloc(1,sizeof(*draw));if(!draw)return ENOMEM;
 draw->generic_pipeline=1;draw->program=program;draw->target=target;draw->depth=r->depth;draw->depth_view=r->depth_view;
 struct pipeline_draw_snapshot *p=&draw->pipeline;
 struct object **holds=capture->holds;size_t held=0;
 p->owner=calloc(1,sizeof(*p->owner));if(!p->owner){free(draw);return ENOMEM;}
 p->owner->references=1;p->source=program->pipeline_source;p->queries=queries;p->texture_pool=r->texture_pool;
 if((e=pipeline_record_pin(p->owner,r->texture_pool)))goto fail;
 p->sampler_pool=r->sampler_pool;
 if((e=pipeline_record_pin(p->owner,r->sampler_pool)))goto fail;
 memcpy(p->combined,r->combined,sizeof(p->combined));memcpy(p->samplers,r->separate_samplers,sizeof(p->samplers));
 memcpy(p->combined_guest,r->handle_carriers[0].guest,sizeof(p->combined_guest));
 memcpy(p->sampler_guest,r->handle_carriers[2].guest,sizeof(p->sampler_guest));
 memcpy(p->combined_defined,r->handle_carriers[0].low_defined,sizeof(p->combined_defined));
 memcpy(p->sampler_defined,r->handle_carriers[2].low_defined,sizeof(p->sampler_defined));
 memcpy(p->separate,r->separate_textures,sizeof(p->separate));
 memcpy(p->guest,r->handle_carriers[1].guest,sizeof(p->guest));
 memcpy(p->defined,r->handle_carriers[1].low_defined,sizeof(p->defined));
 struct nn_pipeline_draw_state *n=&p->native;n->render=r->render;
 e=nn_pipeline_full_raster(width,height,queries.window_origin,queries.depth_mode,&n->raster);if(e)goto fail;
 n->raster.scissor_x=(unsigned)r->scissor[0];n->raster.scissor_y=(unsigned)r->scissor[1];n->raster.scissor_width=(unsigned)r->scissor[2];n->raster.scissor_height=(unsigned)r->scissor[3];
 if(target)n->target=(struct nn_pipeline_image){view,{target->bo,target->offset,target->size}};
 n->vertex.attribute_count=r->attributes_set;n->vertex.stream_count=r->vertex_stream_count;
 for(unsigned i=0;i<32;i++){
  n->vertex.attributes[i]=(struct nn_pipeline_attribute){r->attributes[i]&255u,(r->attributes[i]>>8)&65535u,r->attributes[i]>>24};
  n->vertex.streams[i]=(struct nn_pipeline_stream){r->vertex_streams[i].stride,r->vertex_streams[i].divisor};
  p->vertices[i]=r->vertices[i];
  if(r->vertices[i].pool){
   n->vertex.buffers[i]=(struct nn_pipeline_buffer){r->vertices[i].pool->bo,r->vertices[i].offset,r->vertices[i].size};
   if((e=pipeline_record_pin(p->owner,r->vertices[i].pool)))goto fail;
  }
 }
 for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<14;i++) {
  p->uniforms[stage][i]=r->graphics_ubos[stage][i];
  if((e=pipeline_record_pin(p->owner,p->uniforms[stage][i].pool)))goto fail;
 }
 for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<16;i++){
  p->storage[stage][i]=r->graphics_storage[stage][i];
  if((e=pipeline_record_pin(p->owner,p->storage[stage][i].pool)))goto fail;
 }
 n->draw=parameters?*parameters:(struct nn_gfx_draw){.primitive=primitive,.first=(unsigned)first,.count=(unsigned)count,.instances=1};
 struct object *resources[8]={program,target,target?target->pool:NULL,r->depth,r->depth?r->depth->pool:NULL};size_t rc=5;
 const struct nn_gfx_buffer *extra[]={&n->draw.index,&n->draw.indirect,&n->draw.indirect_count};
 for(unsigned i=0;i<3;i++)if(extra[i]->bo){
  uint64_t base=nn_bo_address(extra[i]->bo);if(extra[i]->offset>UINT64_MAX-base){e=EOVERFLOW;goto fail;}
  struct object *pool=address_pool(o->device,base+extra[i]->offset,extra[i]->bytes);
  if(!pool||pool->bo!=extra[i]->bo){e=EINVAL;goto fail;}
  if(gpu_pool_access(pool,extra[i]->offset,extra[i]->bytes,"NVN generic draw arguments")){e=ENOTSUP;goto fail;}
  resources[rc++]=pool;
 }
 /* A refill callback may rebind and finalize the previous public objects.
  * Pin the exact recorded owners BEFORE invoking it; potential CPU bindings
  * already have their independent p->owner lifetime pins above. */
 for(size_t i=0;i<rc;i++)if(resources[i]){
  size_t j=0;while(j<held&&holds[j]!=resources[i])j++;
  if(j<held)continue;
  if(resources[i]->refs==SIZE_MAX){e=EOVERFLOW;goto fail;}
  holds[held++]=resources[i];resources[i]->refs++;
 }
 capture->draw=draw;capture->held=held;return 0;
 fail:
 while(held){struct object *hold=holds[--held];if(hold)hold->refs--;}
 pipeline_record_release(p);free(draw);*capture=(struct pipeline_draw_capture){0};return e;
}
static int pipeline_record_draw(struct object *o,NVNdrawPrimitive primitive,int first,int count,const struct nn_gfx_draw *parameters)
{
 struct recording *r=o->record;struct pipeline_draw_capture capture;
 int e=pipeline_draw_capture_create(o,r,primitive,first,count,parameters,o->device->graphics_state.window_origin,o->device->graphics_state.depth_mode,&capture);
 if(e)return e;
 struct late_draw *draw=capture.draw;struct pipeline_draw_snapshot *p=&draw->pipeline;
 struct object **holds=capture.holds;size_t held=capture.held;
 e=command_reserve(o,16);if(e)goto fail;
 size_t previous=r->current->count;
 for(size_t i=0;i<held;i++){
  size_t j=0;while(j<r->current->count&&r->current->resources[j]!=holds[i])j++;
  if(j<r->current->count)continue;
  if(r->current->count>=SIZE_MAX/sizeof(*r->current->resources)){e=EOVERFLOW;goto rollback;}
  void *list=realloc(r->current->resources,(r->current->count+1)*sizeof(*r->current->resources));
  if(!list){e=ENOMEM;goto rollback;}
  r->current->resources=list;r->current->resources[r->current->count++]=holds[i];
  holds[i]=NULL; /* transfer the hold, never increment the ref a second time */
 }
 while(held){struct object *hold=holds[--held];if(hold)hold->refs--;}
 draw->command_offset=r->logical_bytes+r->used-r->start;
 memset((char*)nn_bo_map(r->pool->bo)+r->offset+r->used,0,16);r->used+=16;
 draw->next=r->current->draws;r->current->draws=draw;return 0;
 rollback:
 while(r->current->count>previous)r->current->resources[--r->current->count]->refs--;
 fail:
 while(held){struct object *hold=holds[--held];if(hold)hold->refs--;}
 pipeline_record_release(p);free(draw);return e;
}

#else
static int pipeline_draw_capture_create(struct object *o,struct recording *r,NVNdrawPrimitive primitive,int first,int count,const struct nn_gfx_draw *parameters,unsigned window_origin,unsigned depth_mode,struct pipeline_draw_capture *capture)
{(void)o;(void)r;(void)primitive;(void)first;(void)count;(void)parameters;(void)window_origin;(void)depth_mode;(void)capture;return ENOTSUP;}
static void pipeline_record_release(struct pipeline_draw_snapshot *p){(void)p;}
static int pipeline_record_retain(struct pipeline_draw_snapshot *p){(void)p;return ENOTSUP;}
int nn_nvn_enable_pipeline_runtime_views(NVNdevice *key){(void)key;return ENOTSUP;}
int nn_nvn_register_pipeline_views(NVNdevice *key,const struct nn_tex_native_view *views,size_t count)
{(void)key;(void)views;(void)count;return ENOTSUP;}
static int pipeline_record_draw(struct object *o,NVNdrawPrimitive p,int first,int count,const struct nn_gfx_draw *d)
{(void)o;(void)p;(void)first;(void)count;(void)d;return ENOTSUP;}
#endif

static void pipeline_draw_capture_discard(struct pipeline_draw_capture *capture)
{
 while(capture->held){struct object *o=capture->holds[--capture->held];if(o)o->refs--;}
 if(capture->draw){pipeline_record_release(&capture->draw->pipeline);free(capture->draw);}
 *capture=(struct pipeline_draw_capture){0};
}
