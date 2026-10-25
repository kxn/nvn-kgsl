/* Bound-target clear is a GPU-state consumer. Its target/scissor are resolved
 * at the Call execution point, unlike explicit Texture/Buffer copy operands. */
static void clear_bound_color(NVNcommandBuffer *key,int index,const void *color,int mask,int type)
{
 struct object *command=recording_command(key);if(!command)return;
 if(index<0||index>=8||mask<0||(mask&~15)||!color){error(EINVAL);return;}
 if(!mask)return;
 struct command_copy *operation=calloc(1,sizeof(*operation));if(!operation){error(ENOMEM);return;}
 operation->kind=COMMAND_COPY_BOUND_CLEAR;operation->clear_index=index;operation->clear_mask=mask;operation->clear_type=type;
 memcpy(operation->color,color,16);
 /* Existing ordinary recording needs the cached target to admit this clear.
  * Retain it across the memory callback without encoding it as inherited GPU state. */
 operation->textures[0]=command->record->target;
 int e=command_copy_begin(command,operation);if(e){command_copy_free(operation);error(e);return;}
 struct job *old=command->record->current->jobs;
 if(command->record->current->call_nodes){
  /* As with a deferred Draw/Dispatch, resolve state after the actual Call.
   * Keep the same ordinary finite target/scissor qualification at enqueue. */
  e=command_reserve(command,24);
  if(!e)e=gpu_pool_access(command->record->pool,command->record->offset,command->record->capacity,"deferred bound clear");
  if(!e){void *map=nn_bo_map(command->record->pool->bo);if(!map)e=ENOTSUP;
   else{memset((char*)map+command->record->offset+command->record->used,0,16);command->record->used+=16;}}
 }else{union command_copy_color value;clear_bound_color_native(command,index,command_copy_color_value(operation,&value),mask,type);}

 e=command_copy_finish(command,operation,old,16,e);if(e)error(e);
}
