#if NN_NVN_BLOCK_COPY
static int record_canonical_copy(struct object *command,struct object *texture,const NVNtextureView *view,
 const NVNcopyRegion *region,uint64_t address,int to_texture)
{
 if(command->record->copy_recording)return record_canonical_copy_native(command,texture,view,region,address,to_texture);
 if(!region)return EINVAL;
 struct command_copy *operation=calloc(1,sizeof(*operation));if(!operation)return ENOMEM;
 operation->kind=COMMAND_COPY_CANONICAL_BUFFER;operation->textures[0]=texture;operation->regions[0]=*region;
 operation->view_present[0]=view!=NULL;if(view)memcpy(operation->views,view,sizeof(*view));operation->address=address;operation->direction=!!to_texture;
 int e=command_copy_begin(command,operation);if(e){command_copy_free(operation);return e;}
 struct job *old=command->record->current->jobs;
 e=record_canonical_copy_native(command,texture,view?operation->views:NULL,operation->regions,address,to_texture);
 return command_copy_finish(command,operation,old,16,e);
}
static int record_canonical_texture_copy(struct object *command,struct object *source,const NVNtextureView *sv,
 const NVNcopyRegion *sr,struct object *destination,const NVNtextureView *dv,const NVNcopyRegion *dr)
{
 if(command->record->copy_recording)return record_canonical_texture_copy_native(command,source,sv,sr,destination,dv,dr);
 if(!sr||!dr)return EINVAL;
 struct command_copy *operation=calloc(1,sizeof(*operation));if(!operation)return ENOMEM;
 operation->kind=COMMAND_COPY_CANONICAL_TEXTURE;operation->textures[0]=source;operation->textures[1]=destination;
 operation->regions[0]=*sr;operation->regions[1]=*dr;operation->view_present[0]=sv!=NULL;operation->view_present[1]=dv!=NULL;
 if(sv)memcpy(operation->views,sv,sizeof(*sv));
 if(dv)memcpy(operation->views+1,dv,sizeof(*dv));
 int e=command_copy_begin(command,operation);if(e){command_copy_free(operation);return e;}
 struct job *old=command->record->current->jobs;
 e=record_canonical_texture_copy_native(command,source,sv?operation->views:NULL,operation->regions,destination,dv?operation->views+1:NULL,operation->regions+1);
 return command_copy_finish(command,operation,old,32,e);
}
#endif
