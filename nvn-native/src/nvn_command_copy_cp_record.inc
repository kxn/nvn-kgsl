/* Existing planners/encoders remain the only native producer. These wrappers
 * snapshot semantic copy data and identify exactly the jobs they produced. */
static void record_resource_job(struct object *command,const struct resource_span *s,const struct resource_span *d,
 const uint32_t *pattern,unsigned pattern_words,int direct)
{
 if(command->record->copy_recording){record_resource_job_native(command,s,d,pattern,pattern_words,direct);return;}
 struct command_copy *operation=calloc(1,sizeof(*operation));if(!operation){error(ENOMEM);return;}
 operation->kind=COMMAND_COPY_CP;operation->has_source=s!=NULL;if(s)operation->source=*s;operation->destination=*d;
 operation->pattern_words=pattern_words;operation->direct=!!direct;
 if(!s){
  operation->pattern_bytes=direct?d->row_bytes:(size_t)pattern_words*4;
  if(!pattern||!operation->pattern_bytes||operation->pattern_bytes>65536){command_copy_free(operation);error(EINVAL);return;}
  operation->pattern=malloc(operation->pattern_bytes);if(!operation->pattern){command_copy_free(operation);error(ENOMEM);return;}
  for(size_t i=0;i<operation->pattern_bytes/4;i++){
   uint32_t value;memcpy(&value,(const unsigned char*)pattern+i*4,4);operation->pattern[i]=value;
  }
 }
 int e=command_copy_begin(command,operation);if(e){command_copy_free(operation);error(e);return;}
 struct job *old=command->record->current->jobs;
 record_resource_job_native(command,s,d,operation->pattern,pattern_words,direct);
 e=command_copy_finish(command,operation,old,16,0);if(e)error(e);
}
static void bridge_record_clear(struct object *command,struct object *target,const NVNcopyRegion *region,uint32_t word)
{
 if(command->record->copy_recording){bridge_record_clear_native(command,target,region,word);return;}
 if(!region){error(EINVAL);return;}
 struct command_copy *operation=calloc(1,sizeof(*operation));if(!operation){error(ENOMEM);return;}
 operation->kind=COMMAND_COPY_BRIDGE_CLEAR;operation->textures[0]=target;operation->regions[0]=*region;operation->clear_word=word;
 int e=command_copy_begin(command,operation);if(e){command_copy_free(operation);error(e);return;}
 struct job *old=command->record->current->jobs;bridge_record_clear_native(command,target,region,word);
 e=command_copy_finish(command,operation,old,16,0);if(e)error(e);
}

/* Fixed-operand CP_MEM_WRITE has no driver-private shader/scratch. Record the
 * original native words, retain its destination, and type its arena check. */
static void command_record_clear_buffer(struct object *command,struct object *target,uint64_t address,size_t size,uint32_t value)
{
 struct recording *r=command->record;struct command_copy *operation=calloc(1,sizeof(*operation));
 if(!operation){error(ENOMEM);return;}operation->kind=COMMAND_COPY_RAW_CLEAR;
 operation->destination=(struct resource_span){.pool=target,.offset=(size_t)(address-nn_bo_address(target->bo)),.pitch=size,.row_bytes=size,.rows=1};
 operation->clear_word=value;int e=command_copy_begin(command,operation);if(e){command_copy_free(operation);error(e);return;}
 struct job *old=r->current->jobs;size_t bytes=(size/4+3)*4;
 e=command_reserve(command,bytes+8);
 if(!e)e=gpu_pool_access(target,operation->destination.offset,size,"nvnCommandBufferClearBuffer");
 if(!e)e=gpu_pool_access(r->pool,r->offset,r->capacity,"nvnCommandBufferClearBuffer");
 if(!e)e=canonical_copy_arena(&operation->destination);
 if(!e)e=add_resource(r->current,target);
 if(!e){
  void *map=nn_bo_map(r->pool->bo);
  if(!map)e=ENOTSUP;
  else{uint32_t *words=(uint32_t*)((char*)map+r->offset+r->used);words[0]=nvn_pm4_packet7(NVN_CP_MEM_WRITE,(uint16_t)(size/4+2));words[1]=(uint32_t)address;words[2]=(uint32_t)(address>>32);
   for(size_t i=0;i<size/4;i++)words[i+3]=value;
   r->used+=bytes;}
 }
 e=command_copy_finish(command,operation,old,bytes,e);if(e)error(e);
}
