/* Exact public depth attachment snapshots. No native BO or driver default is
 * inferred from a canonical source pointer. Included after view/Call helpers. */
#if NN_NVN_DEPTH_TARGETS
#if !NN_NVN_COLOR_TARGETS
#error "public depth requires typed COLOR target admission"
#endif
static int pipeline_target_admit(struct object *,const struct object *);
static int public_depth_plan(const struct object*t,struct public_depth_view selection,struct nn_depth_shadow_plan*out)
{
 if(!t||t->kind!=TEXTURE||!t->source_layout||!t->pool||t->pool->external||!t->bo||
    t->texture.samples||t->texture.format<NVN_FORMAT_DEPTH16||t->texture.format>NVN_FORMAT_DEPTH32F||
    (t->texture.target!=NVN_TEXTURE_TARGET_2D&&t->texture.target!=NVN_TEXTURE_TARGET_2D_ARRAY))return ENOTSUP;
 const struct nn_tex_source_layout*s=t->source_layout;
 if(s->format!=(uint32_t)t->texture.format||s->target!=(uint32_t)t->texture.target||s->payload_bytes!=t->size||
    s->level[0].width!=(uint32_t)t->texture.width||s->level[0].height!=(uint32_t)t->texture.height||
    s->levels!=(uint32_t)t->texture.levels||s->layers!=(uint32_t)(t->texture.target==NVN_TEXTURE_TARGET_2D_ARRAY?t->texture.depth:1))return EINVAL;
 if(t->offset>t->pool->size||t->size>t->pool->size-t->offset||t->bo!=t->pool->bo)return EINVAL;
 return nn_depth_shadow_plan(s,selection.level,selection.layer,out);
}
static int public_depth_view_read(const struct object*t,const NVNtextureView*key,struct public_depth_view*out)
{
 *out=(struct public_depth_view){0};struct resource_view v;int e=resource_view_read(key,&v);if(e)return e;
 if(!t)return EINVAL;
 if((v.format&&v.format_value!=t->texture.format)||(v.target&&v.target_value!=t->texture.target)||
    (v.mode&&v.mode_value!=NVN_TEXTURE_DEPTH_STENCIL_MODE_DEPTH)||v.swizzle)return ENOTSUP;
 /* Render attachment uses one explicitly selected mip/layer, not a sampled
  * mip chain. Count0/default addresses the base; count>1 has no native role. */
 if((v.levels&&(v.base_level<0||v.level_count<0||v.level_count>1))||
    (v.layers&&(v.base_layer<0||v.layer_count<0||v.layer_count>1)))return ENOTSUP;
 out->level=v.levels?(uint32_t)v.base_level:0;out->layer=v.layers?(uint32_t)v.base_layer:0;
 struct nn_depth_shadow_plan p;return public_depth_plan(t,*out,&p);
}
static int public_depth_set_targets(struct object*o,int count,const NVNtexture*const*colors,const NVNtextureView*const*views,
 const NVNtexture*depth,const NVNtextureView*depth_view)
{
 if(count<0||count>8)return EINVAL;
 if(count>1)return ENOTSUP;
 if((count&&(!colors||!colors[0]))||(!depth&&depth_view))return EINVAL;
 struct object*color=count?find(colors[0],TEXTURE):NULL,*z=depth?find(depth,TEXTURE):NULL;
 if((count&&!color)||(depth&&!z))return last_error?last_error:EINVAL;
 struct public_depth_view selection={0};struct nn_depth_shadow_plan plan;
 if(color){
  if(color->device!=o->device)return EINVAL;
  struct texture_desc resolved;int e=resolve_texture_view(color,views?views[0]:NULL,&resolved);if(e)return e;
  if((e=pipeline_target_admit(o->device,color)))return e;
  if(gpu_pool_access(color->pool,color->offset,color->size,"nvnCommandBufferSetRenderTargets COLOR"))return ENOTSUP;
 }
 if(z){
  if(z->device!=o->device)return EINVAL;
  int e=public_depth_view_read(z,depth_view,&selection);if(e)return e;
  if((e=public_depth_plan(z,selection,&plan)))return e;
  if(!o->device->block_copy)return ENOTSUP;
  if(gpu_pool_access(z->pool,z->offset,z->size,"nvnCommandBufferSetRenderTargets DEPTH"))return ENOTSUP;
  if(color&&((uint32_t)color->texture.width!=plan.aliases.tiled.width||(uint32_t)color->texture.height!=plan.aliases.tiled.height))return ENOTSUP;
 }
 if((color&&color->refs==SIZE_MAX)||(z&&z->refs==SIZE_MAX))return EOVERFLOW;
 struct nn_command_binding values[9]={{0}};
 if(color)values[0]=command_binding(color,color->offset,color->size,nn_bo_address(color->bo)+color->offset);
 if(z){values[8]=command_binding(z,z->offset,z->size,nn_bo_address(z->bo)+z->offset);values[8].guest=(uint64_t)selection.level|((uint64_t)selection.layer<<32);values[8].flags=1;}
 int e=command_capture_bindings(o,NN_COMMAND_TARGET,0,0,9,values);if(e)return e;
 if(o->record->target)o->record->target->refs--;
 if(o->record->depth)o->record->depth->refs--;
 o->record->target=color;if(color)color->refs++;
 o->record->depth=z;o->record->depth_view=selection;if(z)z->refs++;
 return 0;
}
#endif
