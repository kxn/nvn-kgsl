#if NN_NVN_GRAPHICS_SOURCE_CACHE
#include "nvn_depth_clear_submit.inc"
static int pipeline_materialize(struct object *device,const struct pipeline_prepared_draw *prepared,
 nn_bo **bo,size_t *bytes,struct job **owned,nn_bo **refs,size_t *cursor,size_t capacity)
{
 *bo=NULL;*bytes=0;*owned=NULL;
 if(!prepared||*cursor>capacity)return EINVAL;
#if NN_NVN_DEPTH_TARGETS
 if(prepared->draw.depth_clear)return public_depth_clear_materialize(device,prepared,bo,bytes,owned,refs,cursor,capacity);
#endif
 const struct late_draw *draw=&prepared->draw;const struct pipeline_draw_snapshot *p=&draw->pipeline;
 const struct nn_pipeline_archive *archive=p->source.archive;
 if(!draw->generic_pipeline||archive!=device->pipeline_archive||!p->recipe)return EINVAL;
 if(prepared->reference_capacity>capacity-*cursor)return ENOSPC;
 /* No lookup of mutable registrations is allowed here. These are the exact
  * entries and variant admitted by the first all-handle preflight pass. */
 unsigned node_count=3+((draw->target&&draw->target->source_layout)?NN_TEX_SHADOW_BO_COUNT:0)+(draw->depth?3u:0u);
 for(unsigned i=0;i<16;i++)if(prepared->native_entries[i]&&prepared->native_entries[i]->source_layout){
#if NN_NVN_DEPTH_SAMPLED
  if(prepared->depth_images&(1u<<i)){node_count+=2;continue;}
#endif
#if NN_NVN_SAMPLED_IMAGES
  if(prepared->color_images&(1u<<i)){unsigned mips=prepared->color_plan[i].count;if(!mips||mips>NN_FDL_LEVELS)return EINVAL;node_count+=1+(mips>1);continue;}
#endif
#if NN_PIPELINE_BC_SAMPLED
  if(prepared->bc_images&(1u<<i)){unsigned mips=prepared->bc_plan[i].count;
   unsigned add=pipeline_bc_private_nodes(mips);if(!add)return EINVAL;node_count+=add;
  }else node_count+=NN_TEX_SHADOW_BO_COUNT;
#else
  node_count+=NN_TEX_SHADOW_BO_COUNT;
#endif
 }
 /* Sixteen actual sampled resources, up to15 mip dispatches each, plus
  * target/program/graphics/flatten owners. No constant per-draw ref charge. */
 struct job *nodes[16*(2+2*NN_FDL_LEVELS+1)+NN_TEX_SHADOW_BO_COUNT+6]={0};
 if(node_count>sizeof(nodes)/sizeof(*nodes))return E2BIG;
 if(node_count>SIZE_MAX-device->refs)return EOVERFLOW;
 for(unsigned i=0;i<node_count;i++)if(!(nodes[i]=calloc(1,sizeof(*nodes[i])))){
  for(unsigned j=0;j<i;j++)free(nodes[j]);
  return ENOMEM;
 }
 unsigned node=0;int e=0;nn_pipeline_program *program=NULL,*retained=NULL;
 if(device->pipeline_types){
  const struct nn_pipeline_texture_types *types=NULL;
  e=nn_pipeline_select_texture_types(device->pipeline_types,p->stages[0],p->stages[1],p->recipe,&types);
  if(!e&&types!=p->native.texture_types)e=EINVAL;
  if(!e){
#if NN_NVN_GRAPHICS_STORAGE
   if(p->native.storage_types){
    const struct nn_pipeline_depth_export*depth=NULL;
#if NN_NVN_DEPTH_TARGETS
    if(draw->depth)depth=prepared->depth_export;
#endif
    e=nn_pipeline_program_create_typed_storage(device->gpu,archive->stages,p->stages[0],p->stages[1],types,depth,p->native.storage_types,&program,&retained);
   }else
#endif
#if NN_NVN_DEPTH_TARGETS
   if(draw->depth)e=nn_pipeline_program_create_typed_depth(device->gpu,archive->stages,p->stages[0],p->stages[1],types,prepared->depth_export,&program,&retained);
   else
#endif
   e=nn_pipeline_program_create_typed(device->gpu,archive->stages,p->stages[0],p->stages[1],types,&program,&retained);
  }
 }
 else e=nn_pipeline_program_create(device->gpu,archive->stages,p->stages[0],p->stages[1],
  p->stages[0]->specialization,p->stages[1]->specialization,&program,&retained);
 nodes[node++]->pipeline_owner=program?program:retained;if(e)goto done;
 struct nn_pipeline_draw_state state=p->native;
#if NN_NVN_DEPTH_TARGETS
 struct nn_depth_shadow *depth_owner=NULL;
 if(draw->depth){
  struct job *owner=nodes[node++];owner->depth_owner=calloc(1,sizeof(*owner->depth_owner));if(!owner->depth_owner){e=ENOMEM;goto done;}
  struct pipeline_resource_pins *pins=calloc(1,sizeof(*pins));if(!pins){e=ENOMEM;goto done;}
  if(draw->depth->pool->refs==SIZE_MAX){free(pins);e=EOVERFLOW;goto done;}
  pins->objects[0]=draw->depth->pool;pins->count=1;draw->depth->pool->refs++;owner->resource_pins=pins;
  struct nn_block_copy_range canonical={draw->depth->bo,draw->depth->offset,draw->depth->size,draw->depth->offset};
  e=nn_depth_shadow_create(device->gpu,device->block_copy,archive->platform,draw->depth->source_layout,&prepared->depth_plan,&canonical,capacity-*cursor,owner->depth_owner);if(e)goto done;
  depth_owner=owner->depth_owner;
  nn_bo *resources[NN_DEPTH_SHADOW_RESOURCES];size_t count=0;
  if((e=nn_depth_shadow_resources(depth_owner,resources,NN_DEPTH_SHADOW_RESOURCES,&count)))goto done;
  if(count>capacity-*cursor){e=ENOSPC;goto done;}
  for(size_t i=0;i<count;i++)queue_reference(refs,cursor,resources[i]);
 }
#endif
 struct nn_tex_shadow samples[16]={0},target={0};const struct nn_tex_shadow *inputs[17];size_t input_count=0;
 for(unsigned i=0;i<16;i++)if(prepared->native_entries[i]){
  const struct descriptor_entry *entry=prepared->native_entries[i];
  queue_reference(refs,cursor,entry->pool->bo);
  if(!entry->source_layout)continue;
#if NN_NVN_DEPTH_SAMPLED
  if(prepared->depth_images&(1u<<i)){
   struct job*owner=nodes[node++];owner->depth_sampled_owner=calloc(1,sizeof(*owner->depth_sampled_owner));if(!owner->depth_sampled_owner){e=ENOMEM;goto done;}
   struct pipeline_resource_pins*pin=calloc(1,sizeof(*pin));if(!pin){e=ENOMEM;goto done;}
   if(entry->pool->refs==SIZE_MAX){free(pin);e=EOVERFLOW;goto done;}
   pin->objects[0]=entry->pool;pin->count=1;entry->pool->refs++;owner->resource_pins=pin;
   struct nn_block_copy_range canonical={entry->pool->bo,entry->offset,entry->size,entry->offset};
   e=nn_depth_sampled_create(device->gpu,device->block_copy,archive->platform,entry->source_layout,prepared->sampled_depth_plan+i,&canonical,capacity-*cursor,owner->depth_sampled_owner);if(e)goto done;
   nn_bo*resources[7];size_t count=0;e=nn_depth_sampled_resources(owner->depth_sampled_owner,resources,7,&count);if(e)goto done;
   if(count>capacity-*cursor){e=ENOSPC;goto done;}for(size_t k=0;k<count;k++)queue_reference(refs,cursor,resources[k]);
   struct nn_depth_attachment binding;if((e=nn_depth_sampled_binding(owner->depth_sampled_owner,&binding)))goto done;
   state.textures[i].buffer=(struct nn_pipeline_buffer){binding.storage,0,binding.view.required_span};
   struct nn_depth_shadow_step load[2];if((e=nn_depth_sampled_sequence(owner->depth_sampled_owner,load)))goto done;
   struct nn_tex_shadow first={0};first.bos[NN_TEX_SHADOW_PRE]=load[0].job;first.pre_bytes=load[0].bytes;const struct nn_tex_shadow*list[]={&first};
   struct job*join=nodes[node++];e=nn_tex_shadow_chain(device->gpu,list,1,load[1].job,load[1].bytes,NULL,&join->bo,&samples[i].pre_bytes);if(e)goto done;
   samples[i].bos[NN_TEX_SHADOW_PRE]=join->bo;inputs[input_count++]=samples+i;continue;
  }
#endif
#if NN_NVN_SAMPLED_IMAGES
  if(prepared->color_images&(1u<<i)){
   const struct nn_sampled_image_plan *plan=prepared->color_plan+i;
   struct nn_block_copy_range range={entry->pool->bo,entry->offset,entry->size,entry->offset};
   struct job *owner=nodes[node++];
   if(entry->pool->refs==SIZE_MAX){e=EOVERFLOW;goto done;}
   struct pipeline_resource_pins *pin=calloc(1,sizeof(*pin));if(!pin){e=ENOMEM;goto done;}
   pin->objects[0]=entry->pool;pin->count=1;entry->pool->refs++;
   e=nn_sampled_image_shadow_create(device->gpu,device->block_copy,entry->source_layout,plan,&range,&owner->sampled_image_owner);
   /* A retained failed-cleanup owner keeps its borrowed canonical object
    * strongly alive even after the parent version BO/pins have retired. */
   if(owner->sampled_image_owner)owner->resource_pins=pin;else pipeline_pins_release(pin);
   if(e)goto done;
   nn_bo *resources[2+2*NN_FDL_LEVELS+1];size_t resource_count=0;
   e=nn_sampled_image_shadow_resources(owner->sampled_image_owner,resources,sizeof(resources)/sizeof(*resources),&resource_count);if(e)goto done;
   if(resource_count!=3+2*plan->count||resource_count>capacity-*cursor){e=ENOSPC;goto done;}
   for(size_t k=0;k<resource_count;k++)queue_reference(refs,cursor,resources[k]);
   /* Resources API order is not a storage accessor. The actual owner supplies
    * its native storage binding independently from dispatch/code ledgers. */
   struct nn_sampled_image_binding binding;
   e=nn_sampled_image_shadow_binding(owner->sampled_image_owner,&binding);if(e)goto done;
   state.textures[i].buffer=(struct nn_pipeline_buffer){binding.bo,binding.offset,binding.bytes};
   struct nn_tex_shadow steps[NN_FDL_LEVELS]={0};const struct nn_tex_shadow *list[NN_FDL_LEVELS];
   for(unsigned mip=0;mip<plan->count;mip++){
    e=nn_sampled_image_shadow_dispatch(owner->sampled_image_owner,mip,&steps[mip].bos[NN_TEX_SHADOW_PRE],&steps[mip].pre_bytes);if(e)goto done;list[mip]=steps+mip;
   }
   if(plan->count>1){struct job *joined=nodes[node++];
    e=nn_tex_shadow_chain(device->gpu,list,plan->count-1,steps[plan->count-1].bos[NN_TEX_SHADOW_PRE],steps[plan->count-1].pre_bytes,NULL,&joined->bo,&samples[i].pre_bytes);if(e)goto done;
    samples[i].bos[NN_TEX_SHADOW_PRE]=joined->bo;
   }else{samples[i].bos[NN_TEX_SHADOW_PRE]=steps[0].bos[NN_TEX_SHADOW_PRE];samples[i].pre_bytes=steps[0].pre_bytes;}
   inputs[input_count++]=samples+i;continue;
  }
#endif
#if NN_PIPELINE_BC_SAMPLED
  if(prepared->bc_images&(1u<<i)){
   struct nn_bc_shadow bc={0};
   struct nn_block_copy_range range={.bo=entry->pool->bo,.owner_offset=entry->offset,.owner_bytes=entry->size,.offset=entry->offset};
   e=nn_bc_shadow_create(device->gpu,device->block_copy,entry->source_layout,prepared->bc_plan+i,&range,&bc);
   /* Transfer ALL returned owners, including failed-cleanup remnants. No
    * metadata/local owner is dropped on allocation/map/encode failure. */
   nodes[node++]->bo=bc.storage;nodes[node++]->bo=bc.code;
   for(unsigned mip=0;mip<prepared->bc_plan[i].count;mip++){
    nodes[node++]->bo=bc.uniform[mip];nodes[node++]->bo=bc.dispatch[mip];}
   if(e)goto done;
   state.textures[i].buffer=(struct nn_pipeline_buffer){bc.storage,0,prepared->bc_plan[i].native.required_span};
   /* Each original normal CS keeps its own descriptor/sequence pointers.
    * Flatten every mip's complete immutable command range in order. The
    * existing chain supplies full cache boundaries; no pixel/ISA conversion. */
   if(bc.count>1){
    struct nn_tex_shadow steps[NN_FDL_LEVELS]={0};const struct nn_tex_shadow *list[NN_FDL_LEVELS];
    for(unsigned mip=0;mip<bc.count-1;mip++){steps[mip].bos[NN_TEX_SHADOW_PRE]=bc.dispatch[mip];
     steps[mip].pre_bytes=bc.dispatch_bytes[mip];list[mip]=steps+mip;}
    struct job *joined=nodes[node++];
    e=nn_tex_shadow_chain(device->gpu,list,bc.count-1,bc.dispatch[bc.count-1],bc.dispatch_bytes[bc.count-1],NULL,
     &joined->bo,&samples[i].pre_bytes);
    if(e)goto done;
    samples[i].bos[NN_TEX_SHADOW_PRE]=joined->bo;
   }else{samples[i].bos[NN_TEX_SHADOW_PRE]=bc.dispatch[0];samples[i].pre_bytes=bc.dispatch_bytes[0];}
   inputs[input_count++]=samples+i;continue;
  }
#endif
  const struct nn_tex_native_view *v=state.textures[i].layout;
  struct nn_tex_native_layout layout={v->width,v->height,v->pitch,v->layer_bytes,v->required_span_bytes,v->alignment};
  e=nn_tex_shadow_create(device->gpu,device->texture_bridge,entry->pool->bo,entry->offset,entry->size,
   entry->source_layout,&layout,samples+i);
  for(unsigned j=0;j<NN_TEX_SHADOW_BO_COUNT;j++)nodes[node++]->bo=samples[i].bos[j];
  if(e)goto done;
  state.textures[i].buffer=(struct nn_pipeline_buffer){samples[i].bos[NN_TEX_SHADOW_STORAGE],0,v->required_span_bytes};
  inputs[input_count++]=samples+i;
 }
 if((draw->target&&draw->target->source_layout)){
  const struct nn_tex_native_view *v=state.target.layout;
  struct nn_tex_native_layout layout={v->width,v->height,v->pitch,v->layer_bytes,v->required_span_bytes,v->alignment};
#if NN_NVN_COLOR_TARGETS
  (void)layout;
  struct nn_block_copy_range canonical={draw->target->bo,draw->target->offset,draw->target->size,draw->target->offset};
  e=nn_color_target_shadow_create(device->gpu,device->block_copy,draw->target->source_layout,&prepared->target_plan,&canonical,&target);
#else
  e=nn_tex_shadow_create(device->gpu,device->texture_bridge,draw->target->bo,draw->target->offset,
   draw->target->size,draw->target->source_layout,&layout,&target);
#endif
  for(unsigned j=0;j<NN_TEX_SHADOW_BO_COUNT;j++)nodes[node++]->bo=target.bos[j];
  if(e)goto done;
  state.target.buffer=(struct nn_pipeline_buffer){target.bos[NN_TEX_SHADOW_STORAGE],0,v->required_span_bytes};
  inputs[input_count++]=&target;
 }
 struct nn_pipeline_submission submission;nn_bo *failed=NULL;
 #if NN_NVN_DEPTH_TARGETS
 if(draw->depth)e=nn_pipeline_encode_depth(device->gpu,program,prepared->depth_export,archive->platform,&state,&depth_owner->attachment,&prepared->depth_controls,&submission,&failed);
 else
#endif
 e=nn_pipeline_encode(device->gpu,program,p->recipe,archive->platform,&state,&submission,&failed);
 struct job *encoded=nodes[node++];encoded->bo=submission.job?submission.job:failed;if(e)goto done;
 if(submission.resource_count>capacity-*cursor){e=ENOSPC;goto done;}
 for(size_t i=0;i<submission.resource_count;i++)queue_reference(refs,cursor,submission.resources[i]);
 struct job *graphics_owner=encoded;
 if(input_count){
  struct job *chain=nodes[node++];graphics_owner=chain;
  e=nn_tex_shadow_chain(device->gpu,inputs,input_count,encoded->bo,submission.command_bytes,
   target.bos[0]?&target:NULL,&chain->bo,bytes);if(e)goto done;
  *bo=chain->bo;chain->bo=NULL;
 }else{*bo=encoded->bo;encoded->bo=NULL;*bytes=submission.command_bytes;}
#if NN_NVN_DEPTH_TARGETS
 if(draw->depth){
  /* Preserve the original complete graphics child owner when it becomes a
   * referenced child of the outer depth sequence. No literal GPU address in
   * any original code/descriptor/sequence BO is relocated. */
  graphics_owner->bo=*bo;*bo=NULL;
  struct nn_depth_shadow_step load[2],store[2];if((e=nn_depth_shadow_sequence(depth_owner,load,store)))goto done;
  struct nn_tex_shadow steps[2]={0};const struct nn_tex_shadow *list[2]={steps,steps+1};
  for(unsigned i=0;i<2;i++){steps[i].bos[NN_TEX_SHADOW_PRE]=load[i].job;steps[i].pre_bytes=load[i].bytes;}
  struct job *before=nodes[node++];size_t before_bytes=0;
  e=nn_tex_shadow_chain(device->gpu,list,2,graphics_owner->bo,*bytes,NULL,&before->bo,&before_bytes);if(e)goto done;
  steps[0].bos[NN_TEX_SHADOW_PRE]=before->bo;steps[0].pre_bytes=before_bytes;
  steps[1].bos[NN_TEX_SHADOW_PRE]=store[0].job;steps[1].pre_bytes=store[0].bytes;
  struct job *after=nodes[node++];e=nn_tex_shadow_chain(device->gpu,list,2,store[1].job,store[1].bytes,NULL,&after->bo,bytes);if(e)goto done;
  *bo=after->bo;after->bo=NULL;
 }
#else
 (void)graphics_owner;
#endif
 done:
 for(unsigned i=0;i<node_count;i++)if(nodes[i]){
  if(nodes[i]->bo||nodes[i]->pipeline_owner||nodes[i]->sampled_image_owner||nodes[i]->depth_owner||nodes[i]->depth_sampled_owner){nodes[i]->next=*owned;*owned=nodes[i];device->refs++;}else free(nodes[i]);
 }
 return e;
}
#else
static int pipeline_materialize(struct object *d,const struct pipeline_prepared_draw *s,nn_bo **bo,size_t *bytes,
 struct job **owned,nn_bo **refs,size_t *cursor,size_t capacity)
{(void)d;(void)s;(void)refs;(void)cursor;(void)capacity;*bo=NULL;*bytes=0;*owned=NULL;return ENOTSUP;}
#endif
