#ifndef NN_PIPELINE_GATHER
#define NN_PIPELINE_GATHER 0
#endif
#ifndef NN_NVN_BUFFER_SAMPLED
#define NN_NVN_BUFFER_SAMPLED 0
#endif
/* One immutable CPU snapshot per enqueue; no GPU alloc/cache/submit. */
struct pipeline_resource_pins {
 struct pipeline_resource_pins *next;size_t count;struct object *objects[256];
};
static void pipeline_pins_release(struct pipeline_resource_pins *p)
{
 while(p){struct pipeline_resource_pins *next=p->next;
  while(p->count)p->objects[--p->count]->refs--;
  free(p);p=next;}
}
#if NN_NVN_GRAPHICS_SOURCE_CACHE
struct pipeline_prepared_draw {
 struct pipeline_prepared_draw *next;const struct late_draw *recorded;int handle_index;
 struct late_draw draw;
 struct nn_pipeline_frontend_resource_queries queries;
 struct nn_pipeline_resource_binding bindings[192];
 struct descriptor_entry entries[192];struct nn_tex_source_layout layouts[192];
 unsigned stage[192],selector[192],domain[192];size_t entry_count;
 struct pipeline_resource_pins *pins;
 const struct descriptor_entry *native_entries[16];
 /* Enqueue-owned values; no pointer into mutable descriptor slots. */
 struct nn_buffer_sampled_binding buffers[16];
#if NN_NVN_DEPTH_SAMPLED
 struct nn_depth_sampled_plan sampled_depth_plan[16];uint32_t depth_images;
#endif
#if NN_NVN_SAMPLED_IMAGES
 struct nn_sampled_image_plan color_plan[16];uint32_t color_images;
#endif
#if NN_PIPELINE_BC_SAMPLED
 struct nn_bc_shadow_plan bc_plan[16];uint32_t bc_images;
#endif
 uint32_t sampler_ids[16];struct nn_sampler_state sampler_values[16];size_t sampler_count;
#if NN_NVN_COLOR_TARGETS
 struct nn_color_target_shadow_plan target_plan;
#endif
#if NN_NVN_DEPTH_TARGETS
 struct nn_depth_shadow_plan depth_plan;const struct nn_pipeline_depth_export *depth_export;
 struct nn_depth_controls_desc depth_controls;
#endif
 uint32_t planned_images;size_t reference_capacity;
};
struct pipeline_batch {struct pipeline_prepared_draw *draws;};
static void pipeline_batch_discard(struct pipeline_batch *batch)
{
 while(batch->draws){struct pipeline_prepared_draw *p=batch->draws;batch->draws=p->next;
  pipeline_pins_release(p->pins);free(p);}
}
static int pipeline_entry_pin(struct pipeline_prepared_draw *p,struct object *o)
{
 if(!o)return EINVAL;
 for(size_t i=0;i<p->pins->count;i++)if(p->pins->objects[i]==o)return 0;
 if(p->pins->count>=256||o->refs==SIZE_MAX)return EOVERFLOW;
 p->pins->objects[p->pins->count++]=o;o->refs++;return 0;
}
#include "nvn_depth_clear_prepare.inc"
#if NN_PIPELINE_BC_SAMPLED
static unsigned pipeline_bc_private_nodes(unsigned mips)
{return mips&&mips<=NN_FDL_LEVELS?2+2*mips+(mips>1):0;}
/* This128-entry array belongs only to direct graphics resources, NOT the
 * per-mip private CS jobs. All32 raw VFD streams,32 UBOs,16 native textures,
 * target, stage code/C1 and draw args fit independently of mip count. */
_Static_assert(32+32+16+1+4+3+1<=sizeof(((struct nn_pipeline_submission*)0)->resources)/sizeof(nn_bo*),
 "direct graphics resource upper bound");
#endif
/* A selected mip view changes logical query dimensions, never the full
 * canonical backing window or the software descriptor integrity encoding. */
static int pipeline_bc_mip_view(const struct descriptor_entry *entry,uint32_t *base,uint32_t *count)
{
#if NN_PIPELINE_BC_SAMPLED
 const struct texture_desc *t=&entry->snapshot.texture;
 const struct nn_tex_source_layout *l=entry->source_layout;
 if(!entry->valid||!l||t->target!=NVN_TEXTURE_TARGET_2D||t->depth!=1||t->samples||
    t->format<NVN_FORMAT_RGB_DXT1||t->format>NVN_FORMAT_RGBA_DXT5_SRGB||
    !texture_sampling_identity(t)||l->target!=1||l->layers!=1)return ENOTSUP;
 int e=nn_tex_source_validate(l);if(e)return e;
 if(t->levels<1||(unsigned)t->levels!=l->levels||l->levels>NN_FDL_LEVELS||
    (unsigned)t->format!=l->format||(unsigned)t->width!=l->level[0].width||
    (unsigned)t->height!=l->level[0].height||entry->size!=l->payload_bytes)return EINVAL;
 const struct descriptor_selection *v=&entry->selection;
 if(v->base_layer||v->layer_count!=1||!v->level_count||v->base_level>=l->levels||
    v->level_count>l->levels-v->base_level||v->source_offset!=l->level[v->base_level].offset||
    v->subset!=!!(v->base_level||v->level_count!=l->levels))return EINVAL;
 *base=v->base_level;*count=v->level_count;return 0;
#else
 (void)entry;(void)base;(void)count;return ENOTSUP;
#endif
}
/* Legal selected FLOAT2D view over the original full canonical source.
 * This is resource metadata only; operation/sampler/Gather admission follows. */
static int pipeline_color_mip_view(const struct descriptor_entry *entry,uint32_t *base,uint32_t *count)
{
#if NN_NVN_SAMPLED_IMAGES
 const struct texture_desc *t=&entry->snapshot.texture;const struct nn_tex_source_layout *l=entry->source_layout;
 if(!entry->valid||!l||t->target!=NVN_TEXTURE_TARGET_2D||t->depth!=1||t->samples||!texture_sampling_identity(t)||l->target!=1||l->layers!=1||l->block_w!=1||l->block_h!=1)return ENOTSUP;
 int e=nn_tex_source_validate(l);if(e)return e;struct nn_fdl_format_info info;
 if((e=nn_fdl_image_format_info(0x06050002,t->format,&info)))return e;
 if(info.result_type!=NN_FDL_RESULT_FLOAT||info.block_width!=1||info.block_height!=1)return ENOTSUP;
 if(t->levels<1||(unsigned)t->levels!=l->levels||l->levels>NN_FDL_LEVELS||(unsigned)t->format!=l->format||(unsigned)t->width!=l->level[0].width||(unsigned)t->height!=l->level[0].height||entry->size!=l->payload_bytes||info.bytes_per_block!=l->bytes_per_block)return EINVAL;
 const struct descriptor_selection *v=&entry->selection;
 if(v->base_layer||v->layer_count!=1||!v->level_count||v->base_level>=l->levels||v->level_count>l->levels-v->base_level||v->source_offset!=l->level[v->base_level].offset||v->subset!=!!(v->base_level||v->level_count!=l->levels))return EINVAL;
 *base=v->base_level;*count=v->level_count;return 0;
#else
 (void)entry;(void)base;(void)count;return ENOTSUP;
#endif
}
static int pipeline_capture_domain(struct pipeline_prepared_draw *p,unsigned stage,unsigned domain,unsigned selector,size_t *index)
{
 for(size_t i=0;i<p->entry_count;i++)if(p->stage[i]==stage&&p->domain[i]==domain&&p->selector[i]==selector){*index=i;return 0;}
 if(stage>1||domain>2||selector>=32||p->entry_count==192)return EINVAL;
 const struct pipeline_draw_snapshot *record=&p->recorded->pipeline;
 size_t i=p->entry_count;struct object *pool=domain==2?record->sampler_pool:record->texture_pool;
 const uint64_t (*payload)[32]=domain==0?record->combined:domain==2?record->samplers:record->separate;
 const uint64_t (*guest)[32]=domain==0?record->combined_guest:domain==2?record->sampler_guest:record->guest;
 const uint32_t *defined=domain==0?record->combined_defined:domain==2?record->sampler_defined:record->defined;
 struct nn_pipeline_resource_binding row={.stage=stage,.domain=domain,.selector=selector,
  .low_word_defined=(defined[stage]>>selector)&1u,.payload=(uint32_t)payload[stage][selector],
  .guest_carrier=guest[stage][selector]};
 if(row.low_word_defined&&pool) {
  int e=cpu_descriptor_access(pool);if(e)return e;
  if(pool->device!=p->draw.program->device)return EINVAL;
  uint32_t id=domain==2?row.payload>>20:row.payload&0xfffffu;
  if(domain!=2)for(size_t j=0;j<p->entry_count;j++)if(p->bindings[j].texture.registered&&
     p->bindings[j].texture.descriptor_pool==pool&&p->bindings[j].texture.descriptor_id==id){
   p->entries[i]=p->entries[j];row.texture=p->bindings[j].texture;
   if(p->entries[j].source_layout){p->layouts[i]=*p->entries[j].source_layout;p->entries[i].source_layout=p->layouts+i;}
   goto captured;
  }
  if(id>=RESERVED_DESCRIPTORS&&id<pool->slot_count&&pool->slots[id].valid) {
   const struct descriptor_entry *entry=pool->slots+id;
   if(domain==2){
    const struct sampler_desc *a=&entry->snapshot.sampler;
    struct sampler_slot expected={0x534e4e53u,(uint32_t)a->min,(uint32_t)a->mag,
     (uint32_t)a->wrap[0]|((uint32_t)a->wrap[1]<<8)|((uint32_t)a->wrap[2]<<16),
     (uint32_t)a->compare|((uint32_t)a->compare_func<<8),a->lod_min,a->lod_max,a->lod_bias};
    size_t offset=pool->offset+(size_t)id*DESCRIPTOR_BYTES;
    if(offset>pool->pool->size||sizeof(expected)>pool->pool->size-offset||!nn_bo_map(pool->pool->bo)||
       memcmp((const char*)nn_bo_map(pool->pool->bo)+offset,&expected,sizeof(expected)))return EINVAL;
    p->entries[i]=*entry;goto captured;
   }
   if(!entry->pool||entry->pool->device!=pool->device)return EINVAL;
   /* Registration can describe a legal canonical subresource without a
    * native consumer. Never query/convert the original full source as if it
    * were that view. This check precedes resource pins or GPU allocation. */
   uint32_t view_base=0,view_count=0;int bc_view=!pipeline_bc_mip_view(entry,&view_base,&view_count);
   int color_view=bc_view?0:!pipeline_color_mip_view(entry,&view_base,&view_count);
   int depth_view=0;
#if NN_NVN_DEPTH_SAMPLED
   depth_view=!pipeline_depth_view(entry,&view_base);
#endif
   if((entry->selection.subset&&!bc_view&&!color_view&&!depth_view)||(!texture_sampling_identity(&entry->snapshot.texture)&&!depth_view))return ENOTSUP;
   const struct texture_desc *t=&entry->snapshot.texture;
   if(entry->offset>entry->pool->size||entry->size>entry->pool->size-entry->offset||entry->size>UINT32_MAX)return EINVAL;
   uint64_t address=nn_bo_address(entry->pool->bo);if(entry->offset>UINT64_MAX-address)return EOVERFLOW;
   struct texture_slot expected={t->target==NVN_TEXTURE_TARGET_BUFFER?0x42584e4eu:0x54584e4eu,
    (uint32_t)t->format,(uint32_t)t->width,(uint32_t)t->height,(uint32_t)t->stride,(uint32_t)entry->size,address+entry->offset};
   size_t offset=pool->offset+(size_t)id*DESCRIPTOR_BYTES;
   if(offset>pool->pool->size||sizeof(expected)>pool->pool->size-offset||!nn_bo_map(pool->pool->bo))return EINVAL;
   if(memcmp((const char *)nn_bo_map(pool->pool->bo)+offset,&expected,sizeof(expected)))return EINVAL;
   if((e=pipeline_entry_pin(p,entry->pool)))return e;
   p->entries[i]=*entry;
   if(entry->source_layout){p->layouts[i]=*entry->source_layout;p->entries[i].source_layout=p->layouts+i;}
   row.texture=(struct nn_pipeline_registered_texture){pool,entry->pool,1,id,(uint32_t)t->target,
    (uint32_t)t->format,(uint32_t)t->width,(uint32_t)t->height,(uint32_t)t->depth,(uint32_t)t->levels,
    (uint32_t)t->samples,entry->offset,entry->size};
   if(bc_view||color_view){row.texture.width=entry->source_layout->level[view_base].width;
    row.texture.height=entry->source_layout->level[view_base].height;row.texture.levels=view_count;}
  }
 }
 captured:
 p->bindings[i]=row;p->stage[i]=stage;p->domain[i]=domain;p->selector[i]=selector;p->entry_count++;
 p->queries.resources.binding_count=p->entry_count;*index=i;return 0;
}
/* Compatibility helper is intentionally the separate texture domain. */
static int pipeline_capture_selector(struct pipeline_prepared_draw *p,unsigned stage,unsigned selector,size_t *index)
{return pipeline_capture_domain(p,stage,NN_PIPELINE_SEPARATE_TEXTURE,selector,index);}
static int pipeline_capture_reference(struct pipeline_prepared_draw *p,unsigned stage,int32_t word,int32_t cb,
 struct nn_pipeline_resource_reference *r,size_t *texture)
{
 int e=nn_pipeline_resource_reference(word,cb,r);if(e)return e;
 return r->texture_domain==NN_PIPELINE_SEPARATE_TEXTURE?pipeline_capture_selector(p,stage,r->texture_selector,texture):
  pipeline_capture_domain(p,stage,r->texture_domain,r->texture_selector,texture);
}
static int pipeline_capture_sampler(struct pipeline_prepared_draw *p,unsigned stage,
 const struct nn_pipeline_resource_reference *r,size_t texture,struct nn_sampler_state *out)
{
 if(!r->has_sampler)return ENOTSUP;
 const struct pipeline_draw_snapshot *record=&p->recorded->pipeline;
 struct object *pool=record->sampler_pool;if(!pool)return ENOTSUP;
 int e=cpu_descriptor_access(pool);if(e)return e;
 uint32_t id;const struct descriptor_entry *entry;
 if(r->sampler_domain==NN_PIPELINE_COMBINED)id=p->bindings[texture].payload>>20;
 else{
  const struct pipeline_draw_snapshot *record=&p->recorded->pipeline;
  uint32_t payload=(uint32_t)record->samplers[stage][r->sampler_selector];
  if(!((record->sampler_defined[stage]>>r->sampler_selector)&1u)||payload&0xfffffu||
     (uint32_t)record->sampler_guest[stage][r->sampler_selector]!=payload)return ENOTSUP;
  id=payload>>20;
 }
 /* One descriptor ID denotes one immutable enqueue version, even when read
  * through several combined/separate selectors. No later mutable slot lookup. */
 for(size_t j=0;j<p->sampler_count;j++)if(p->sampler_ids[j]==id){*out=p->sampler_values[j];return 0;}
 if(id<RESERVED_DESCRIPTORS||id>=pool->slot_count||!pool->slots[id].valid)return ENOTSUP;
 if(p->sampler_count>=16)return E2BIG;
 if(r->sampler_domain==NN_PIPELINE_COMBINED){
  entry=pool->slots+id;const struct sampler_desc *a=&entry->snapshot.sampler;
  struct sampler_slot expected={0x534e4e53u,(uint32_t)a->min,(uint32_t)a->mag,
   (uint32_t)a->wrap[0]|((uint32_t)a->wrap[1]<<8)|((uint32_t)a->wrap[2]<<16),
   (uint32_t)a->compare|((uint32_t)a->compare_func<<8),a->lod_min,a->lod_max,a->lod_bias};
  size_t offset=pool->offset+(size_t)id*DESCRIPTOR_BYTES;
  if(pool->device!=p->draw.program->device||offset>pool->pool->size||sizeof(expected)>pool->pool->size-offset||
     !nn_bo_map(pool->pool->bo)||memcmp((char*)nn_bo_map(pool->pool->bo)+offset,&expected,sizeof(expected)))return EINVAL;
 }else{
  size_t i;if((e=pipeline_capture_domain(p,stage,NN_PIPELINE_SEPARATE_SAMPLER,r->sampler_selector,&i)))return e;
  entry=p->entries+i;if(!entry->valid)return ENOTSUP;
 }
 *out=native_sampler_state(&entry->snapshot.sampler);
 p->sampler_ids[p->sampler_count]=id;p->sampler_values[p->sampler_count++]=*out;return 0;
}
#if NN_NVN_GRAPHICS_STORAGE
static int pipeline_storage_alignment_query(const struct pipeline_prepared_draw*p,unsigned stage,uint32_t*out)
{
 if(stage>1||!out)return EINVAL;
 *out=0;const struct nn_pipeline_source_program*source=&p->draw.pipeline.source;
 struct nn_pipeline_source_input identity;int error=nn_pipeline_source_identity(source,stage,&identity);if(error)return error;
 const struct nn_pipeline_source_input*id=&identity;unsigned matched=0;
 /* Source identity is already a trusted complete code/control/C1 snapshot.
  * Union its reflected source references across all compiled specializations;
  * no expected query result or native binding selects an API slot. */
 for(size_t i=0;i<source->archive->stages->stage_count;i++){
  const struct nn_offline_graphics_stage*v=source->archive->stages->stages+i;
  if(v->stage!=stage||v->source_bytes!=id->source_bytes||v->control_bytes!=id->control_bytes||
     memcmp(v->source,id->source,id->source_bytes)||memcmp(v->control,id->control,id->control_bytes))continue;
  matched=1;
  if(v->storage_count>16||(v->storage_count&&!v->storage))return ENOTSUP;
  for(size_t j=0;j<v->storage_count;j++){
   unsigned api=0;int e=nn_pipeline_storage_source_selector(stage,v->storage[j].cbuf_slot,v->storage[j].cbuf_word,&api);if(e)return e;
   const struct bound*b=p->draw.pipeline.storage[stage]+api;if(!b->pool||!b->address||!b->size)return ENODATA;
   if(b->address&15u)*out=1;
  }
 }
 return matched?0:ENOTSUP;
}
#endif
static int pipeline_actual_query(void *user,uint32_t stage,const struct nn_pipeline_query *q,uint32_t values[32],size_t *count)
{
 struct pipeline_prepared_draw *p=user;
#if NN_NVN_GRAPHICS_STORAGE
 if(q&&q->name&&!strcmp(q->name,"QueryHasUnalignedStorageBuffer")){
  if(!count||!values)return EINVAL;
  *count=0;memset(values,0,32*sizeof(*values));
  if(stage>1||q->arguments||q->type!=NN_PIPELINE_QUERY_BOOL||q->count!=1||q->args[0]||q->args[1]||q->args[2]||q->args[3])return EINVAL;
  int e=pipeline_storage_alignment_query(p,stage,values);if(e)return e;
  *count=1;return 0;
 }
#endif
 if(q&&q->name&&!strcmp(q->name,"backend/unit_render_scale")) {
  if(!count||!values)return EINVAL;
  *count=0;memset(values,0,32*sizeof(*values));
  if(stage>1||q->arguments||q->type!=NN_PIPELINE_QUERY_BOOL||q->count!=1||
     q->args[0]||q->args[1]||q->args[2]||q->args[3])return EINVAL;
  /* Actual frontend policy: full pixel viewport/target and native views retain
   * source dimensions exactly. Every selected sampled view must pass the same
   * dimension-preserving admission below before any GPU allocation. This is
   * not a guest NVN scale property or a value copied from requirements. */
  const struct nn_tex_native_view *v=p->draw.pipeline.native.target.layout;
  if(p->draw.target){if(!v||v->width!=(uint32_t)p->draw.target->texture.width||v->height!=(uint32_t)p->draw.target->texture.height)return ENOTSUP;}
  else{
#if NN_NVN_DEPTH_TARGETS
   if(!p->draw.depth||v||p->depth_plan.aliases.tiled.width!=p->draw.depth->source_layout->level[p->draw.depth_view.level].width||p->depth_plan.aliases.tiled.height!=p->draw.depth->source_layout->level[p->draw.depth_view.level].height)return ENOTSUP;
#else
   return ENOTSUP;
#endif
  }
  values[0]=1;*count=1;return 0;
 }
 if(q&&q->name&&(!strcmp(q->name,"QuerySamplerType")||!strcmp(q->name,"QueryTextureFormat")||
    !strcmp(q->name,"QueryTextureCoordNormalized"))&&q->arguments==3) {
  size_t i;struct nn_pipeline_resource_reference ref;
  int e=pipeline_capture_reference(p,stage,q->args[0],q->args[1],&ref,&i);if(e)return e;
 }
 return nn_pipeline_frontend_resource_query(&p->queries,stage,q,values,count);
}
static int pipeline_native_sample(struct object *device,struct pipeline_prepared_draw *p,unsigned binding,
 const struct nn_offline_texture_binding *map)
{
 if(binding>=16||!map||map->set||map->array_length!=1||(map->type!=3&&map->type!=2)||map->use_known!=1||
    (!map->samples&&!map->fetches&&!map->size_queries&&!map->other))return ENOTSUP;
 const struct nn_pipeline_texture_types *types=p->draw.pipeline.native.texture_types;
 unsigned gather=0;
 if(types&&(types->operations[binding]&NN_PIPELINE_TEX_GATHER)){
#if NN_PIPELINE_GATHER
  if(nn_pipeline_texture_type_validate(types,binding)||map->type!=3||map->offsets||
     !!map->samples!=!!(types->operations[binding]&NN_PIPELINE_TEX_SAMPLE)||
     !!map->fetches!=!!(types->operations[binding]&NN_PIPELINE_TEX_FETCH)||
     !!map->size_queries!=!!(types->operations[binding]&NN_PIPELINE_TEX_SIZE)||
     !map->other||map->other!=types->gathers[binding].count)return ENOTSUP;
  gather=1;
#else
  return ENOTSUP;
#endif
 }else if(map->other)return ENOTSUP;
 unsigned semantic_sampler=!!map->samples||gather;
 size_t i;struct nn_pipeline_resource_reference ref;
 int e=pipeline_capture_reference(p,1,map->handle_word,map->cbuf_slot,&ref,&i);if(e)return e;
 struct nn_pipeline_resolved_texture resolved;
 if((e=nn_pipeline_resource_texture(&p->queries.resources,1,map->handle_word,map->cbuf_slot,&resolved)))return e;
 struct descriptor_entry *entry=p->entries+i;const struct texture_desc *t=&entry->snapshot.texture;
 if(map->type==2){
#if NN_NVN_BUFFER_SAMPLED && NN_PIPELINE_BUFFER_SAMPLED
  struct nn_pipeline_draw_state *state=&p->draw.pipeline.native;
  if(!entry->valid||t->target!=NVN_TEXTURE_TARGET_BUFFER||resolved.sampler_type!=2||
     t->height!=1||t->depth!=1||t->levels!=1||t->samples||entry->source_layout||entry->selection.subset||
     map->samples||map->offsets||(p->draw.pipeline.recipe->sampler_mask&(1u<<binding))||
     !state->texture_types||state->texture_types->dimensions[binding]!=1||
     (state->texture_types->operations[binding]&~(NN_PIPELINE_TEX_FETCH|NN_PIPELINE_TEX_SIZE)))return ENOTSUP;
  if((e=gpu_pool_access(entry->pool,entry->offset,entry->size,"NVN pipeline BUFFER source")))return e;
  /* Public capabilityfalse makes the translator rewrite SNORM fetches through
   * a SINT carrier. This profile binds genuine float-result native FDL views;
   * no silent integer alias descriptor or double normalization is supplied. */
  if(map->fetches&&(t->format==NVN_FORMAT_R8SN||t->format==NVN_FORMAT_R16SN||
     t->format==NVN_FORMAT_RG8SN||t->format==NVN_FORMAT_RG16SN||
     t->format==NVN_FORMAT_RGBA8SN||t->format==NVN_FORMAT_RGBA16SN))return ENOTSUP;
  struct nn_buffer_sampled_binding proposed={.version=1,.format=t->format,.resource=entry->pool->bo,
   .owner_offset=0,.owner_bytes=entry->pool->size,.offset=entry->offset,.bytes=entry->size};
  struct nn_fdl_buffer_view native;uint32_t result;
  if((e=nn_buffer_sampled_view(&proposed,&native))||(e=nn_buffer_sampled_result(t->format,&result)))return e;
  if(map->fetches&&result!=NN_BUFFER_FLOAT)return ENOTSUP;
  /* Prefix is part of the admitted owner/read range and all alias checks,
   * including distinct imports of the same caller CPU storage/TBI alias. */
  uint64_t base=nn_bo_address(entry->pool->bo);
  if(native.base_address<base||native.base_address-base>SIZE_MAX||native.required_span>SIZE_MAX)return EINVAL;
  struct descriptor_entry read_span=*entry;read_span.offset=(size_t)(native.base_address-base);read_span.size=(size_t)native.required_span;
  if((e=bridge_feedback(p->draw.target,&read_span))||(e=bridge_feedback(p->draw.depth,&read_span)))return e;
  p->buffers[binding]=proposed;state->sampled_buffer[binding]=p->buffers+binding;
  p->native_entries[binding]=entry;return 0;
#else
  return ENOTSUP;
#endif
 }
 if(!entry->valid||t->target!=NVN_TEXTURE_TARGET_2D||t->depth!=1||t->levels<1||t->samples)return ENOTSUP;
 if(gather&&!(NN_NVN_SAMPLED_IMAGES&&entry->source_layout)&&!(NN_NVN_DEPTH_SAMPLED&&entry->source_layout&&t->format>=NVN_FORMAT_DEPTH16&&t->format<=NVN_FORMAT_DEPTH32F)&&(t->format!=NVN_FORMAT_RGBA8||t->levels!=1||entry->selection.subset))return ENOTSUP;
 if((e=gpu_pool_access(entry->pool,entry->offset,entry->size,"NVN pipeline sampled resource")))return e;
 if((e=bridge_feedback(p->draw.target,entry))||(e=bridge_feedback(p->draw.depth,entry)))return e;
 struct nn_pipeline_draw_state *state=&p->draw.pipeline.native;
 if(semantic_sampler!=!!(p->draw.pipeline.recipe->sampler_mask&(1u<<binding)))return ENOTSUP;
 if(semantic_sampler){
  if((e=pipeline_capture_sampler(p,1,&ref,i,state->samplers+binding)))return e;
 }
 struct nn_pipeline_image *image=state->textures+binding;
#if NN_NVN_DEPTH_SAMPLED
 if(t->format>=NVN_FORMAT_DEPTH16&&t->format<=NVN_FORMAT_DEPTH32F){
  if(!device->block_copy||!entry->source_layout)return ENOTSUP;
  struct nn_depth_sampled_plan*plan=p->sampled_depth_plan+binding;
  if((e=pipeline_depth_sample_plan(entry,state->texture_types,binding,plan)))return e;
  state->sampler_layouts[binding]=(struct nn_sampler_layout){.width=plan->native.view_width,.height=plan->native.view_height,.levels=1,.samples=1,.dimensions=2,.normalized=1,.depth=1};
  if(semantic_sampler){uint32_t sampler[4];if((e=nn_depth_sampled_sampler(&plan->native,state->samplers+binding,state->sampler_layouts+binding,sampler)))return e;}
  image->layout=NULL;image->buffer=(struct nn_pipeline_buffer){NULL,0,plan->native.required_span};state->sampled_depth[binding]=&plan->native;
  p->depth_images|=1u<<binding;p->planned_images|=1u<<(binding+1);p->native_entries[binding]=entry;return 0;
 }
#endif
#if NN_NVN_SAMPLED_IMAGES
 if(entry->source_layout&&entry->source_layout->block_w==1&&entry->source_layout->block_h==1){
  if(!device->block_copy||!state->texture_types||state->texture_types->dimensions[binding]!=2)return ENOTSUP;
  int valid=nn_pipeline_texture_type_validate(state->texture_types,binding);if(valid)return valid;
  uint32_t base=0,count=0;if((e=pipeline_color_mip_view(entry,&base,&count)))return e;
  struct nn_sampled_image_plan *plan=p->color_plan+binding;
  if((e=nn_sampled_image_plan(entry->source_layout,base,count,plan)))return e;
  state->sampler_layouts[binding]=(struct nn_sampler_layout){.width=plan->native.view_width,.height=plan->native.view_height,.levels=count,.samples=1,.dimensions=2,.normalized=1};
  if(semantic_sampler){uint32_t sampler[4];if((e=nn_sampled_image_sampler(&plan->native,state->samplers+binding,state->sampler_layouts+binding,sampler)))return e;}
  image->layout=NULL;image->buffer=(struct nn_pipeline_buffer){NULL,0,plan->native.required_span};
  state->sampled_color[binding]=&plan->native;p->color_images|=1u<<binding;p->planned_images|=1u<<(binding+1);
  p->native_entries[binding]=entry;return 0;
 }
#endif
 if(t->format>=NVN_FORMAT_RGB_DXT1&&t->format<=NVN_FORMAT_RGBA_DXT5_SRGB){
#if NN_PIPELINE_BC_SAMPLED
  if(!entry->source_layout||!device->block_copy)return ENOTSUP;
  struct nn_bc_shadow_plan *plan=p->bc_plan+binding;
  uint32_t base=0,count=0;if((e=pipeline_bc_mip_view(entry,&base,&count)))return e;
  if((e=nn_bc_shadow_plan(entry->source_layout,base,count,0,1,plan)))return e;
  if(plan->count!=(uint32_t)t->levels||plan->native.width!=(uint32_t)t->width||plan->native.height!=(uint32_t)t->height||
     plan->native.view_width!=entry->source_layout->level[base].width||plan->native.view_height!=entry->source_layout->level[base].height)return EINVAL;
  state->sampler_layouts[binding]=(struct nn_sampler_layout){.width=plan->native.view_width,.height=plan->native.view_height,
   .levels=count,.samples=1,.dimensions=2,.normalized=1};
  if((e=nn_bc_sampled_validate(&plan->native)))return e;
  if(semantic_sampler){uint32_t sampler[4];
   if((e=nn_bc_sampled_sampler(&plan->native,state->samplers+binding,state->sampler_layouts+binding,sampler)))return e;}
  image->layout=NULL;image->buffer=(struct nn_pipeline_buffer){NULL,0,plan->native.required_span};
  state->sampled_fdl[binding]=&plan->native;p->bc_images|=1u<<binding;p->planned_images|=1u<<(binding+1);
#else
  return ENOTSUP;
#endif
 }else{
  if(t->format!=NVN_FORMAT_RGBA8||t->levels!=1||entry->selection.subset)return ENOTSUP;
  state->sampler_layouts[binding]=(struct nn_sampler_layout){.width=t->width,.height=t->height,
   .levels=1,.samples=1,.dimensions=2,.normalized=1};
  struct object shape={.texture=*t,.size=entry->size,.source_layout=entry->source_layout};
  const struct nn_tex_native_view *v=NULL;if((e=pipeline_target_view(device,&shape,&v)))return e;
  image->layout=v;
  if(entry->source_layout){
   if(!device->texture_bridge||(e=nn_tex_bridge_source_validate(entry->source_layout)))return ENOTSUP;
   image->buffer=(struct nn_pipeline_buffer){NULL,0,v->required_span_bytes};p->planned_images|=1u<<(binding+1);
  }else image->buffer=(struct nn_pipeline_buffer){entry->pool->bo,entry->offset,entry->size};
 }
 p->native_entries[binding]=entry;return 0;
}
#if NN_NVN_COLOR_TARGETS
static int pipeline_target_bound_alias(const struct object *target,const struct bound *b)
{
 if(!target||!target->source_layout||!b->pool)return 0;
 struct descriptor_entry read={.pool=b->pool,.offset=b->offset,.size=b->size};
 return bridge_feedback(target,&read);
}
#endif
#if NN_NVN_GRAPHICS_STORAGE
static int pipeline_storage_bound_alias(const struct bound*a,const struct bound*b)
{
 if(!a->pool||!b->pool)return 0;
 struct object range={.pool=a->pool,.bo=a->pool->bo,.offset=a->offset,.size=a->size};
 struct descriptor_entry other={.pool=b->pool,.offset=b->offset,.size=b->size};return bridge_feedback(&range,&other);
}
static int pipeline_prepare_storage(struct object*device,struct pipeline_prepared_draw*p)
{
 struct pipeline_draw_snapshot*s=&p->draw.pipeline;
 if(!device->pipeline_storage){
  for(size_t i=0;i<s->recipe->patch_count;i++)if(s->recipe->patches[i].role==NN_PIPELINE_STORAGE)return ENOTSUP;
  for(unsigned stage=0;stage<2;stage++)if(s->stages[stage]->storage_count)return ENOTSUP;
  return 0;
 }
 const struct nn_pipeline_storage_types*t=NULL;
 int e=nn_pipeline_select_storage_types(device->pipeline_storage,s->stages[0],s->stages[1],s->recipe,&t);if(e)return e;
 s->native.storage_types=t;if(!t)return 0;
 for(unsigned stage=0;stage<2;stage++)for(unsigned native=0;native<16;native++)if(t->mask[stage]&(1u<<native)){
  const struct nn_offline_storage_binding*map=NULL;
  for(size_t i=0;i<s->stages[stage]->storage_count;i++)if(s->stages[stage]->storage[i].binding==native){if(map)return EINVAL;map=s->stages[stage]->storage+i;}
  if(!map)return EINVAL;
  unsigned api=0;if((e=nn_pipeline_storage_source_selector(stage,map->cbuf_slot,map->cbuf_word,&api)))return e;
  const struct bound*b=s->storage[stage]+api;
  if(!b->pool)return ENOTSUP;
  /* Existing ADDR-import interval/TBI alias proof only. Unknown external
   * physical aliases are not established by BO/GPUVA inequality. */
  if(b->pool->external||!b->pool->storage)return ENOTSUP;
  if((e=gpu_bound_access(b,"NVN graphics storage")))return e;
  struct descriptor_entry span={.pool=b->pool,.offset=b->offset,.size=b->size};
  if((e=bridge_feedback(p->draw.target,&span))||(e=bridge_feedback(p->draw.depth,&span)))return e;
  struct object writer={.pool=b->pool,.bo=b->pool->bo,.offset=b->offset,.size=b->size};
  if(t->operations[stage][native]&NN_PIPELINE_STORAGE_WRITE){
   if((e=bridge_target_arena_alias(&writer)))return e;
   for(unsigned st=0;st<2;st++)for(unsigned i=0;i<14;i++)if((e=pipeline_storage_bound_alias(b,s->uniforms[st]+i)))return e;
   for(unsigned i=0;i<32;i++)if((e=pipeline_storage_bound_alias(b,s->vertices+i)))return e;
   for(unsigned i=0;i<16;i++)if(p->native_entries[i]&&(e=bridge_feedback(&writer,p->native_entries[i])))return e;
  }
  if(t->operations[stage][native]&NN_PIPELINE_STORAGE_WRITE){
   const struct nn_gfx_buffer*args[]={&s->native.draw.index,&s->native.draw.indirect,&s->native.draw.indirect_count};
   for(unsigned i=0;i<3;i++)if(args[i]->bo){
    uint64_t base=nn_bo_address(args[i]->bo);if(args[i]->offset>UINT64_MAX-base)return EOVERFLOW;
    struct object*pool=address_pool(device,base+args[i]->offset,args[i]->bytes);if(!pool||pool->bo!=args[i]->bo)return EINVAL;
    struct bound arg={pool,args[i]->offset,args[i]->bytes,base+args[i]->offset};if((e=pipeline_storage_bound_alias(b,&arg)))return e;
   }
   for(unsigned st=0;st<2;st++)for(size_t j=0;j<s->stages[st]->storage_count;j++){
    const struct nn_offline_storage_binding*other=s->stages[st]->storage+j;
    if(!(t->mask[st]&(1u<<other->binding))||(st==stage&&other->binding==native))continue;
    unsigned api=0;if((e=nn_pipeline_storage_source_selector(st,other->cbuf_slot,other->cbuf_word,&api)))return e;
    if((e=pipeline_storage_bound_alias(b,s->storage[st]+api)))return e;
   }
  }
  if((e=pipeline_entry_pin(p,b->pool)))return e;
  s->native.storage[stage][native]=(struct nn_pipeline_buffer){b->pool->bo,b->offset,b->size};
 }
 return 0;
}
#endif
static int pipeline_prepare_draw(struct object *device,const struct late_draw *recorded,struct pipeline_prepared_draw **out)
{
 *out=NULL;
 if(!recorded->generic_pipeline||recorded->pipeline.source.archive!=device->pipeline_archive)return EINVAL;
 struct pipeline_prepared_draw *p=calloc(1,sizeof(*p));if(!p)return ENOMEM;
 p->pins=calloc(1,sizeof(*p->pins));if(!p->pins){free(p);return ENOMEM;}
 p->draw=*recorded;p->recorded=recorded;
#if NN_NVN_DEPTH_TARGETS
 if(recorded->depth_clear){int e=public_depth_clear_prepare(device,recorded,p);if(e){pipeline_pins_release(p->pins);free(p);return e;}*out=p;return 0;}
#endif
 int initial=pipeline_entry_pin(p,recorded->program);
 if(!initial&&recorded->target)initial=pipeline_entry_pin(p,recorded->target);
 if(!initial&&recorded->target)initial=pipeline_entry_pin(p,recorded->target->pool);
 if(!initial&&recorded->depth)initial=pipeline_entry_pin(p,recorded->depth);
 if(!initial&&recorded->depth)initial=pipeline_entry_pin(p,recorded->depth->pool);
 if(!initial&&recorded->pipeline.texture_pool)initial=pipeline_entry_pin(p,recorded->pipeline.texture_pool);
 if(!initial&&recorded->pipeline.sampler_pool)initial=pipeline_entry_pin(p,recorded->pipeline.sampler_pool);
 if(initial){pipeline_pins_release(p->pins);free(p);return initial;}
#if NN_NVN_DEPTH_TARGETS
 if(recorded->depth){int e=public_depth_plan(recorded->depth,recorded->depth_view,&p->depth_plan);if(e){pipeline_pins_release(p->pins);free(p);return e;}}
#endif
 p->queries.graphics=recorded->pipeline.queries;
 p->queries.resources=(struct nn_pipeline_resource_queries){.version=1,.graphics_texture_cb_known=1,
  .graphics_texture_cb=2,.selected_texture_pool=recorded->pipeline.texture_pool,.bindings=p->bindings};
 struct pipeline_draw_snapshot *s=&p->draw.pipeline;const struct nn_pipeline_shader_recipe *recipe=NULL;
 int e=nn_pipeline_source_resolve(&s->source,pipeline_actual_query,p,s->stages,s->stages+1,&recipe);if(e)goto fail;
 s->recipe=recipe;
 if(device->pipeline_types){
  if(device->pipeline_types->archive!=device->pipeline_archive){e=EINVAL;goto fail;}
  e=nn_pipeline_select_texture_types(device->pipeline_types,s->stages[0],s->stages[1],recipe,&s->native.texture_types);if(e)goto fail;
 }
#if NN_NVN_DEPTH_TARGETS
 if(recorded->depth){
  if(!device->pipeline_depth){e=ENOTSUP;goto fail;}
  e=nn_pipeline_select_depth_export(device->pipeline_depth,s->stages[0],s->stages[1],recipe,&p->depth_export);if(e)goto fail;
  p->depth_controls=(struct nn_depth_controls_desc){1,nn_device_chip(device->gpu),s->native.render.depth.depth_test,
   s->native.render.depth.depth_test&&s->native.render.depth.depth_write,s->native.render.depth.depth_func,
   p->depth_export->writes_depth,p->depth_export->depth_regid,0};
 }else if(recipe->version!=NN_PIPELINE_RECIPE_VERSION){e=ENOTSUP;goto fail;}
#endif
 if(recipe->sampler_mask&~recipe->texture_mask){e=ENOTSUP;goto fail;}
 /* Native texture tables are currently fragment-only. No VS texture binding
  * is silently routed into the FS table, even if native indices coincide. */
 if(s->stages[0]->texture_count){e=ENOTSUP;goto fail;}
 if(s->stages[1]->texture_count>16||(s->stages[1]->texture_count&&!s->stages[1]->textures)){e=EINVAL;goto fail;}
 for(unsigned binding=0;binding<16;binding++)if(recipe->texture_mask&(1u<<binding)) {
  const struct nn_offline_texture_binding *map=NULL;
  for(size_t j=0;j<s->stages[1]->texture_count;j++)if(s->stages[1]->textures[j].binding==binding){
   if(map){e=EINVAL;goto fail;}map=s->stages[1]->textures+j;
  }
  if((e=pipeline_native_sample(device,p,binding,map)))goto fail;
 }
 s->native.texture_bound=recipe->texture_mask;s->native.sampler_bound=recipe->sampler_mask;
#if NN_NVN_GRAPHICS_RUNTIME_CONSTANTS
 if(device->pipeline_runtime){
  const struct nn_pipeline_runtime_constants*t=NULL;
  if((e=nn_pipeline_select_runtime_constants(device->pipeline_runtime,s->stages[0],s->stages[1],recipe,&t)))goto fail;
  /* NVN DrawID -> source builtin mapping is unproved. The original twelve
   * BaseInstance-only recipes do not consume word0. Never infer DrawID zero. */
  if(t&&((t->layout->driver_mask[0]|t->layout->driver_mask[1])&1u)){e=ENOTSUP;goto fail;}
  s->native.runtime_constants=t;s->native.runtime_draw_id=0;
 }else for(size_t i=0;i<recipe->patch_count;i++)if(recipe->patches[i].role==NN_PIPELINE_DRIVER_PARAMS||recipe->patches[i].role==NN_PIPELINE_DYNAMIC_UBO){e=ENOTSUP;goto fail;}
#endif
#if NN_NVN_GRAPHICS_STORAGE
 if((e=pipeline_prepare_storage(device,p)))goto fail;
#endif
 for(unsigned stage=0;stage<2;stage++) {
  struct nn_pipeline_buffer api[14]={0};
  for(unsigned i=0;i<14;i++)if(s->uniforms[stage][i].pool){
   struct bound b=s->uniforms[stage][i];api[i]=(struct nn_pipeline_buffer){b.pool->bo,b.offset,b.size};
  }
#if NN_NVN_DEPTH_TARGETS
  if(recorded->depth)e=nn_pipeline_frontend_uniforms_depth(s->stages[stage],p->depth_export,api,s->native.ubo[stage]);
  else
#endif
  e=nn_pipeline_frontend_uniforms(s->stages[stage],recipe,api,s->native.ubo[stage]);
  if(e)goto fail;
  for(unsigned i=0;i<14;i++)if(s->uniforms[stage][i].pool)for(unsigned native=0;native<16;native++) {
   const struct nn_pipeline_buffer *b=s->native.ubo[stage]+native;const struct bound *actual=s->uniforms[stage]+i;
   if(b->bo==actual->pool->bo&&b->offset==actual->offset&&b->bytes==actual->size){
    if((e=gpu_bound_access(actual,"NVN pipeline UBO")))goto fail;
#if NN_NVN_GRAPHICS_RUNTIME_CONSTANTS
    if(s->native.runtime_constants){struct descriptor_entry span={.pool=actual->pool,.offset=actual->offset,.size=actual->size};
     if((e=bridge_feedback(recorded->target,&span))||(e=bridge_feedback(recorded->depth,&span)))goto fail;}
#endif
#if NN_NVN_COLOR_TARGETS
    if((e=pipeline_target_bound_alias(recorded->target,actual)))goto fail;
#endif
    if((e=pipeline_entry_pin(p,actual->pool)))goto fail;
    break;
   }
  }
 }
 for(size_t i=0;i<recipe->input_count;i++) {
  unsigned location=recipe->inputs[i].location;if(location>=32){e=EINVAL;goto fail;}
  unsigned stream=s->native.vertex.attributes[location].stream;if(stream>=32){e=EINVAL;goto fail;}
  if((e=gpu_bound_access(s->vertices+stream,"NVN pipeline vertex")))goto fail;
#if NN_NVN_COLOR_TARGETS
  if((e=pipeline_target_bound_alias(recorded->target,s->vertices+stream)))goto fail;
#endif
  if((e=pipeline_entry_pin(p,s->vertices[stream].pool)))goto fail;
  if(s->native.vertex.streams[stream].stride>4095){e=ENOTSUP;goto fail;}
 }
 if(recorded->target&&recorded->target->source_layout){
  if((e=bridge_target_arena_alias(recorded->target)))goto fail;
#if NN_NVN_COLOR_TARGETS
  if((e=nn_color_target_shadow_plan(recorded->target->source_layout,s->native.target.layout,&p->target_plan)))goto fail;
  const struct nn_gfx_buffer *arguments[]={&s->native.draw.index,&s->native.draw.indirect,&s->native.draw.indirect_count};
  for(unsigned i=0;i<3;i++)if(arguments[i]->bo){
   const struct nn_gfx_buffer *b=arguments[i];uint64_t a=nn_bo_address(b->bo);
   if(b->offset>UINT64_MAX-a){e=EOVERFLOW;goto fail;}
   struct object *pool=address_pool(device,a+b->offset,b->bytes);
   if(!pool||pool->bo!=b->bo){e=EINVAL;goto fail;}
   struct bound read={pool,b->offset,b->bytes,a+b->offset};
   if((e=pipeline_target_bound_alias(recorded->target,&read)))goto fail;
  }
#endif
  s->native.target.buffer=(struct nn_pipeline_buffer){NULL,0,s->native.target.layout->required_span_bytes};p->planned_images|=1;
 }
#if NN_NVN_DEPTH_TARGETS
 if(recorded->depth){
  if((e=public_depth_preflight_aliases(recorded,s,recipe,p->native_entries)))goto fail;
  struct nn_depth_fdl_view zero;if((e=nn_depth_fdl(&p->depth_plan.attachment,&zero)))goto fail;
  if((e=nn_pipeline_preflight_depth_planned(device->gpu,s->stages,p->depth_export,device->pipeline_archive->platform,&s->native,p->planned_images,&zero,&p->depth_controls)))goto fail;
 }else
#endif
 if((e=nn_pipeline_preflight(device->gpu,s->stages,recipe,device->pipeline_archive->platform,&s->native,p->planned_images)))goto fail;
 p->reference_capacity=draw_ref_capacity(&p->draw);
#if NN_NVN_DEPTH_TARGETS
 if(recorded->depth){if(p->reference_capacity>SIZE_MAX-12){e=EOVERFLOW;goto fail;}p->reference_capacity+=12; /* canonical+9 private+2 joined-chain BOs */}
#endif
 unsigned converted=recorded->target&&recorded->target->source_layout?1u:0u;
 for(unsigned i=0;i<16;i++)if(p->native_entries[i]){
  unsigned shadow_nodes=p->native_entries[i]->source_layout?NN_TEX_SHADOW_BO_COUNT:0;
#if NN_PIPELINE_BC_SAMPLED
  if(p->bc_images&(1u<<i)){unsigned mips=p->bc_plan[i].count;
   shadow_nodes=pipeline_bc_private_nodes(mips);
   if(!shadow_nodes){e=EINVAL;goto fail;}}
#endif
#if NN_NVN_SAMPLED_IMAGES
  if(p->color_images&(1u<<i)){
   unsigned mips=p->color_plan[i].count;if(!mips||mips>NN_FDL_LEVELS){e=EINVAL;goto fail;}
   shadow_nodes=2+2*mips+(mips>1);
  }
#endif
#if NN_NVN_DEPTH_SAMPLED
  if(p->depth_images&(1u<<i))shadow_nodes=7; /* 6 private BOs plus joined load wrapper */
#endif
  p->reference_capacity+=1+shadow_nodes;
  converted+=p->native_entries[i]->source_layout!=NULL;
 }
 if(converted>NN_TEX_SHADOW_CHAIN_MAX_INPUTS){e=E2BIG;goto fail;}
 if(converted&&(!recorded->target||!recorded->target->source_layout))p->reference_capacity++; /* flatten wrapper */
 *out=p;return 0;
 fail:pipeline_pins_release(p->pins);free(p);return e;
}
static const struct pipeline_prepared_draw *pipeline_batch_find(const struct pipeline_batch *b,const struct late_draw *draw,int index)
{for(const struct pipeline_prepared_draw*p=b->draws;p;p=p->next)if(p->recorded==draw&&p->handle_index==index)return p;return NULL;}
static int pipeline_batch_prepare_owned(struct object *queue,int count,struct handle *const *selected,struct pipeline_batch *batch)
{
 memset(batch,0,sizeof(*batch));
 if(count<0||(count&&!selected))return EINVAL;
 for(int i=0;i<count;i++){
  struct handle *h=selected[i];if(!h||h->device!=queue->device||h->call_nodes)return EINVAL;
  for(const struct late_draw *d=h->draws;d;d=d->next){
   if(d->bridge_clear)continue; /* Existing copy clear materializer owns this leaf. */
   if(!d->generic_pipeline)return ENOTSUP;
   struct pipeline_prepared_draw *p=NULL;int e=pipeline_prepare_draw(queue->device,d,&p);if(e)return e;
   p->handle_index=i;p->next=batch->draws;batch->draws=p;
  }
 }
 return 0;
}
static int pipeline_batch_prepare(struct object *queue,int count,const NVNcommandHandle *ids,struct pipeline_batch *batch)
{
 memset(batch,0,sizeof(*batch));
 if(count<0||(count&&!ids))return EINVAL;
 for(int i=0;i<count;i++) {
  struct handle *h=handles;while(h&&(h->id!=ids[i]||h->public_retired))h=h->next;
  if(h){int call_error=command_call_admit(h);if(call_error)return call_error;}
  if(!h||h->device!=queue->device)return EINVAL;
  for(const struct late_draw*d=h->draws;d;d=d->next)if(d->generic_pipeline) {
   struct pipeline_prepared_draw *p=NULL;int e=pipeline_prepare_draw(queue->device,d,&p);if(e)return e;
   p->handle_index=i;p->next=batch->draws;batch->draws=p;
  }
 }
 return 0;
}
#else
struct pipeline_batch{unsigned unused;};
struct pipeline_prepared_draw{unsigned unused;};
static int pipeline_batch_prepare_owned(struct object *q,int count,struct handle *const *h,struct pipeline_batch *b)
{(void)q;(void)b;for(int i=0;i<count;i++)for(const struct late_draw *d=h[i]->draws;d;d=d->next)if(!d->bridge_clear)return ENOTSUP;return 0;}

static void pipeline_batch_discard(struct pipeline_batch *b){(void)b;}
static int pipeline_batch_prepare(struct object *q,int count,const NVNcommandHandle *ids,struct pipeline_batch *b)
{(void)q;(void)count;(void)ids;(void)b;return 0;}
static const struct pipeline_prepared_draw *pipeline_batch_find(const struct pipeline_batch *b,const struct late_draw *d,int index)
{(void)b;(void)d;(void)index;return NULL;}
#endif
