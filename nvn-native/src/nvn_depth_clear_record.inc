#if NN_NVN_DEPTH_TARGETS
static int public_depth_clear_capture(struct object*device,const struct recording*r,float depth,struct late_draw**out)
{
 *out=NULL;if(!device->pipeline_depth||!r->depth||!r->scissor_set)return ENOTSUP;
 struct nn_depth_shadow_plan plan;int e=public_depth_plan(r->depth,r->depth_view,&plan);if(e)return e;
 const int*s=r->scissor;uint32_t w=plan.aliases.tiled.width,h=plan.aliases.tiled.height;
 if(s[0]<0||s[1]<0||s[2]<=0||s[3]<=0||(unsigned)s[0]>=w||(unsigned)s[2]>w-(unsigned)s[0]||(unsigned)s[1]>=h||(unsigned)s[3]>h-(unsigned)s[1])return EINVAL;
 if(!(depth>=0&&depth<=1))return EINVAL;
 if(gpu_pool_access(r->depth->pool,r->depth->offset,r->depth->size,"NVN depth clear target"))return ENOTSUP;
 struct late_draw*draw=calloc(1,sizeof(*draw));if(!draw)return ENOMEM;
 draw->pipeline.owner=calloc(1,sizeof(*draw->pipeline.owner));
 if(!draw->pipeline.owner){free(draw);return ENOMEM;}
 draw->pipeline.owner->references=1;
 if((e=pipeline_record_pin(draw->pipeline.owner,r->depth))||(e=pipeline_record_pin(draw->pipeline.owner,r->depth->pool))){pipeline_record_release(&draw->pipeline);free(draw);return e;}
 draw->generic_pipeline=1;draw->depth_clear=1;draw->depth=r->depth;draw->depth_view=r->depth_view;draw->pipeline.source.archive=device->pipeline_archive;
 draw->clear_region=(NVNcopyRegion){.xoffset=s[0],.yoffset=s[1],.width=s[2],.height=s[3],.depth=1};memcpy(&draw->clear_word,&depth,4);
 *out=draw;return 0;
}
static int record_public_depth_clear(struct object*command,float depth)
{
 struct command_copy*operation=calloc(1,sizeof(*operation));if(!operation)return ENOMEM;
 operation->kind=COMMAND_COPY_BOUND_DEPTH_CLEAR;operation->textures[0]=command->record->depth;memcpy(operation->color,&depth,4);
 struct late_draw*draw=NULL;int e=0;
 if(!command->record->current->call_nodes){e=public_depth_clear_capture(command->device,command->record,depth,&draw);if(e){free(operation);return e;}}
 e=command_copy_begin(command,operation);if(e){if(draw)pipeline_record_release(&draw->pipeline);free(draw);command_copy_free(operation);return e;}
 struct job*old=command->record->current->jobs;
 size_t resource_count=command->record->current->count;
 if(draw){if((e=add_resource(command->record->current,draw->depth))||(e=add_resource(command->record->current,draw->depth->pool)))goto finish;}
 if((e=command_reserve(command,16)))goto finish;
 if(gpu_pool_access(command->record->pool,command->record->offset,command->record->capacity,"NVN depth clear command arena")){e=ENOTSUP;goto finish;}
 void*map=nn_bo_map(command->record->pool->bo);if(!map){e=ENOTSUP;goto finish;}
 size_t offset=command->record->logical_bytes+command->record->used-command->record->start;
 memset((char*)map+command->record->offset+command->record->used,0,16);command->record->used+=16;
 if(draw){draw->command_offset=offset;draw->next=command->record->current->draws;command->record->current->draws=draw;draw=NULL;}
 finish:if(e){struct handle*h=command->record->current;while(h->count>resource_count)h->resources[--h->count]->refs--;}
 if(draw){pipeline_record_release(&draw->pipeline);}free(draw);return command_copy_finish(command,operation,old,16,e);
}
static int project_public_depth_clear(struct handle*destination,const struct command_copy*operation,const struct nn_command_state*state,size_t offset)
{
 const struct nn_command_binding*b=nn_command_state_binding(state,NN_COMMAND_TARGET,0,8);
 struct object*z=command_draw_object(b,TEXTURE,destination->device);
 if(!z||!b||b->flags!=1||!(state->defined&NN_COMMAND_STATE_SCISSOR)||b->offset!=z->offset||b->bytes!=z->size||b->address!=nn_bo_address(z->bo)+z->offset)return ENOTSUP;
 struct recording r={.depth=z,.depth_view={(uint32_t)b->guest,(uint32_t)(b->guest>>32)},.scissor_set=1};memcpy(r.scissor,state->scissor,sizeof(r.scissor));float depth;memcpy(&depth,operation->color,4);
 struct late_draw*draw=NULL;int e=public_depth_clear_capture(destination->device,&r,depth,&draw);if(e)return e;
 if((e=add_resource(destination,z))||(e=add_resource(destination,z->pool))){pipeline_record_release(&draw->pipeline);free(draw);return e;}
 draw->command_offset=offset;draw->next=destination->draws;destination->draws=draw;return 0;
}
#endif
