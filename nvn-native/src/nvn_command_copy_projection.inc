/* Fresh native copy jobs per expansion/replay. Private scratch is never NVN
 * Program/UBO/SSBO state and never reused from the source command body. */
static int command_project_copy(struct handle *destination,const struct command_copy *operation,
 const struct nn_command_state *state,size_t offset)
{
#if NN_NVN_DEPTH_TARGETS
 if(operation->kind==COMMAND_COPY_BOUND_DEPTH_CLEAR)return project_public_depth_clear(destination,operation,state,offset);
#endif
 if(operation->kind==COMMAND_COPY_RAW_CLEAR){
  if(operation->command_bytes<16||operation->command_bytes>4108||operation->command_bytes%4)return EINVAL;
  int e=canonical_copy_arena(&operation->destination);if(e)return e;
  /* Fixed CP destination is an actual RAW consumer, unlike source cached
   * bindings. Typed projection must retain it explicitly. */
  return add_resource(destination,operation->destination.pool);
 }
 if(operation->command_bytes!=16&&operation->command_bytes!=32)return EINVAL;
 struct job *patch=calloc(1,sizeof(*patch));if(!patch)return ENOMEM;
 int e=nn_bo_alloc(destination->device->gpu,64,&patch->bo);
 if(e){free(patch);return e;}
 struct object pool={.kind=POOL,.device=destination->device,.bo=patch->bo,.size=nn_bo_size(patch->bo),.flags=0x24};
 struct recording recording={.pool=&pool,.capacity=64,.current=destination,.active=1,.copy_recording=1,
  .copy_row_stride=operation->row_stride,.copy_image_stride=operation->image_stride};
 struct object command={.kind=COMMAND,.device=destination->device,.record=&recording};
 struct late_draw *old_draws=destination->draws;
 patch->next=destination->jobs;destination->jobs=patch;destination->device->refs++;
 switch(operation->kind){
 case COMMAND_COPY_CP:
  if((e=canonical_copy_arena(&operation->destination)))break;
  record_resource_job_native(&command,operation->has_source?&operation->source:NULL,&operation->destination,
   operation->pattern,operation->pattern_words,operation->direct);e=last_error;break;
#if NN_NVN_BLOCK_COPY
 case COMMAND_COPY_CANONICAL_BUFFER:
  e=record_canonical_copy_native(&command,operation->textures[0],operation->view_present[0]?operation->views:NULL,
   operation->regions,operation->address,operation->direction);break;
 case COMMAND_COPY_CANONICAL_TEXTURE:
  e=record_canonical_texture_copy_native(&command,operation->textures[0],operation->view_present[0]?operation->views:NULL,operation->regions,
   operation->textures[1],operation->view_present[1]?operation->views+1:NULL,operation->regions+1);break;
#endif
 case COMMAND_COPY_BRIDGE_CLEAR:
  bridge_record_clear_native(&command,operation->textures[0],operation->regions,operation->clear_word);e=last_error;break;
 case COMMAND_COPY_BOUND_CLEAR:{
  const struct nn_command_binding *binding=nn_command_state_binding(state,NN_COMMAND_TARGET,0,0);
  recording.target=command_draw_object(binding,TEXTURE,destination->device);
  if(!recording.target||!(state->defined&NN_COMMAND_STATE_SCISSOR)){e=ENOTSUP;break;}
  memcpy(recording.scissor,state->scissor,sizeof(recording.scissor));recording.scissor_set=1;
  union command_copy_color value;clear_bound_color_native(&command,operation->clear_index,command_copy_color_value(operation,&value),operation->clear_mask,operation->clear_type);e=last_error;break;
 }
 default:e=ENOTSUP;break;
 }
 if(e)return e;
 if(recording.used!=operation->command_bytes)return EINVAL;
 /* Bridge clear snapshots are materialized by the existing shadow path,
  * whose private native target/code/scratch is fresh at actual enqueue. */
 for(struct late_draw *draw=destination->draws;draw!=old_draws;draw=draw->next){
  if(!draw||!draw->bridge_clear||draw->command_offset>=recording.used)return EINVAL;
  draw->command_offset+=offset;
 }
 patch->projected_dispatch=2;patch->command_offset=offset;patch->command_bytes=recording.used;return 0;
}
static int command_project_copies(struct handle *destination,const struct handle *source,
 const struct nn_command_leaf *leaf,const struct nn_command_state *state)
{
 unsigned found=0;
 for(const struct command_copy *operation=source->copies;operation;operation=operation->next)
  if(operation->command_offset>=leaf->offset&&operation->command_offset-leaf->offset<leaf->bytes){
   size_t relative=operation->command_offset-leaf->offset;
   if(operation->command_bytes>leaf->bytes-relative||relative>SIZE_MAX-destination->bytes)return EINVAL;
   int e=command_project_copy(destination,operation,state,destination->bytes+relative);if(e)return e;found++;
  }
 return found?0:ENOTSUP;
}
