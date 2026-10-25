/* Typed immutable GPU-copy operations. CPU recording strides are snapshots,
 * never writes to the inherited NVN GPU binding frame. */
enum command_copy_kind { COMMAND_COPY_CP,COMMAND_COPY_CANONICAL_BUFFER,
 COMMAND_COPY_CANONICAL_TEXTURE,COMMAND_COPY_BRIDGE_CLEAR,COMMAND_COPY_BOUND_CLEAR,COMMAND_COPY_RAW_CLEAR,COMMAND_COPY_BOUND_DEPTH_CLEAR };
struct command_copy {
 struct command_copy *next;enum command_copy_kind kind;size_t command_offset,command_bytes;
 struct job *jobs[6];unsigned job_count;
 struct object *pins[4];unsigned pin_count;
 struct resource_span source,destination;unsigned has_source,pattern_words,direct;
 uint32_t *pattern;size_t pattern_bytes;
 struct object *textures[2];NVNtextureView views[2];unsigned view_present[2];NVNcopyRegion regions[2];
 uint64_t address;ptrdiff_t row_stride,image_stride;int direction;
 uint32_t clear_word;unsigned char color[16];int clear_index,clear_mask,clear_type;
};
union command_copy_color {float f[4];uint32_t u[4];int i[4];};
static const void *command_copy_color_value(const struct command_copy *operation,union command_copy_color *value)
{
 if(operation->clear_type==0){memcpy(value->f,operation->color,16);return value->f;}
 if(operation->clear_type==1){memcpy(value->u,operation->color,16);return value->u;}
 memcpy(value->i,operation->color,16);return value->i;
}
static int canonical_copy_arena(const struct resource_span *);
static void command_copy_unpin(struct command_copy *operation)
{while(operation->pin_count)operation->pins[--operation->pin_count]->refs--;}
static void command_copy_free(struct command_copy *operation)
{while(operation){struct command_copy *next=operation->next;command_copy_unpin(operation);free(operation->pattern);free(operation);operation=next;}}
static int command_copy_pin(struct command_copy *operation,struct object *owner)
{
 if(!owner)return 0;
 for(unsigned i=0;i<operation->pin_count;i++)if(operation->pins[i]==owner)return 0;
 if(operation->pin_count==4||owner->refs==SIZE_MAX)return EOVERFLOW;
 operation->pins[operation->pin_count++]=owner;owner->refs++;return 0;
}
static int command_copy_begin(struct object *command,struct command_copy *operation)
{
 int e=command_capture_close(command);if(e)return e;
 struct object *owners[]={operation->has_source?operation->source.pool:NULL,
  operation->has_source?operation->source.texture:NULL,operation->destination.pool,operation->destination.texture};
 for(unsigned i=0;i<4;i++)if((e=command_copy_pin(operation,owners[i])))return e;
 for(unsigned i=0;i<2;i++)if((e=command_copy_pin(operation,operation->textures[i])))return e;
 operation->row_stride=command->record->copy_row_stride;
 operation->image_stride=command->record->copy_image_stride;
 command->record->copy_recording=1;return 0;
}
static int command_copy_finish(struct object *command,struct command_copy *operation,struct job *old_jobs,size_t bytes,int e)
{
 struct recording *r=command->record;struct handle *h=r->current;r->copy_recording=0;
 if(!e)e=last_error;
 if(!e&&r->used>=r->start&&r->logical_bytes<=SIZE_MAX-(r->used-r->start)){
  size_t end=r->logical_bytes+r->used-r->start;
  if(bytes>end||end-bytes<h->call_cursor)e=EINVAL;
  else{operation->command_offset=end-bytes;operation->command_bytes=bytes;}
 }else if(!e)e=EOVERFLOW;
 if(!e)for(struct job *job=h->jobs;job!=old_jobs;job=job->next){
  if(!job||operation->job_count==6){e=E2BIG;break;}
  operation->jobs[operation->job_count++]=job;
 }
 if(e){command_copy_free(operation);return e;}
 for(unsigned i=0;i<operation->job_count;i++)operation->jobs[i]->copy_owner=operation;
 command_copy_unpin(operation);operation->next=h->copies;h->copies=operation;return 0;
}
static int command_copy_job_known(const struct handle *source,const struct job *job)
{
 if(!job->copy_owner)return 0;
 for(const struct command_copy *operation=source->copies;operation;operation=operation->next)if(operation==job->copy_owner)
  for(unsigned i=0;i<operation->job_count;i++)if(operation->jobs[i]==job)return 1;
 return 0;
}
static int command_copy_capture_kind(const struct handle *h,size_t begin,size_t end,unsigned *kind)
{
 for(const struct command_copy *operation=h->copies;operation;operation=operation->next)
  if(operation->command_offset>=begin&&operation->command_offset<end){
   if(operation->command_bytes>end-operation->command_offset)return EINVAL;
   if(*kind!=NN_COMMAND_RAW&&*kind!=NN_COMMAND_COPY){int e=nn_command_sequence_unsupported(h->call_sequence);if(e)return e;}
   *kind=NN_COMMAND_COPY;
  }
 return 0;
}

#if NN_NVN_DEPTH_TARGETS
static int command_copy_depth_clone(struct handle*plan,const struct handle*src,size_t adjustment)
{
 for(const struct command_copy*c=src->copies;c;c=c->next)if(c->kind==COMMAND_COPY_BOUND_DEPTH_CLEAR){
  if(c->job_count||c->pattern||c->pin_count||c->command_offset>SIZE_MAX-adjustment)return EINVAL;
  struct command_copy*clone=malloc(sizeof(*clone));if(!clone)return ENOMEM;
  *clone=*c;clone->command_offset+=adjustment;clone->next=plan->copies;plan->copies=clone;
 }
 return 0;
}
static void command_copy_depth_transfer(struct handle*dst,struct handle*plan)
{while(plan->copies){struct command_copy*c=plan->copies;plan->copies=c->next;c->next=dst->copies;dst->copies=c;}}
#endif
