/* SPDX-License-Identifier: MIT. Actual native handle/body ownership bridge.
 * Public lookup lifetime is separate from Call owner lifetime. Retired child
 * bodies remain in the existing all-body alias ledger until their last Call
 * drops them; no dangling source arena or hidden alias interval is created. */
static void free_draws(struct late_draw *);
static void command_dispatch_free(struct command_dispatch *);
static struct object *recording_command(NVNcommandBuffer *);
static void command_body_destroy(struct handle *h)
{
 struct object *dev=h->device;
 struct handle **at=&handles;while(*at&&*at!=h)at=&(*at)->next;
 if(*at==h)*at=h->next;
 for(size_t i=0;i<h->count;i++)h->resources[i]->refs--;
 for(size_t i=0;i<h->cpu_count;i++)h->cpu_resources[i]->refs--;
 free(h->cpu_resources);retire_jobs(dev,h->jobs);free_draws(h->draws);
 counter_free_ops(h->counter_ops);queue_family_handle_cleanup(h);
 command_dispatch_free(h->dispatches);command_copy_free(h->copies);nn_command_sequence_destroy(h->call_sequence);
 h->commands->refs--;dev->refs--;free(h->resources);free(h);
}
static int command_owner_hold(void *key)
{
 struct handle *h=key;
 if(!h||!h->commands||!h->id||h->call_references==SIZE_MAX)return EOVERFLOW;
 h->call_references++;return 0;
}
static void command_owner_drop(void *key)
{
 struct handle *h=key;
 if(!h||!h->call_references){error(EPROTO);return;}
 h->call_references--;
 if(!h->call_references&&h->public_retired)command_body_destroy(h);
}
static int command_owner_span(const void *key,size_t offset,size_t bytes,unsigned kind)
{
 const struct handle *h=key;(void)kind;
 if(!h||!h->device||offset>h->call_limit||bytes>h->call_limit-offset)return EINVAL;
 return 0;
}
static const struct nn_command_owner_ops command_owner_ops={command_owner_hold,command_owner_drop,command_owner_span};
static int command_capture_close(struct object *o)
{
 struct recording *r=o->record;struct handle *h=r->current;
 if(!h||!h->call_sequence)return EINVAL;
 if(r->used<r->start||r->logical_bytes>SIZE_MAX-(r->used-r->start))return EOVERFLOW;
 size_t end=r->logical_bytes+r->used-r->start;
 if(end<h->call_cursor)return EINVAL;
 if(end==h->call_cursor)return 0;
 unsigned kind=NN_COMMAND_RAW;
 for(struct late_draw *d=h->draws;d;d=d->next)if(!d->bridge_clear&&!d->depth_clear&&d->command_offset>=h->call_cursor&&d->command_offset<end)kind=NN_COMMAND_DRAW;
 /* Only semantic Dispatch jobs can be projected. Copy/private legacy jobs
  * remain visibly unsupported instead of being passed through as raw bytes. */
 unsigned unknown_jobs=h->borrowed_jobs!=NULL;
 for(const struct job *job=h->jobs;job;job=job->next){
  unsigned found=0;for(const struct command_dispatch *d=h->dispatches;d;d=d->next)if(d->recorded_job==job){found=1;break;}
  if(!found&&!command_copy_job_known(h,job))unknown_jobs=1;
 }
 if(unknown_jobs){int e=nn_command_sequence_unsupported(h->call_sequence);if(e)return e;}
 for(const struct command_dispatch *d=h->dispatches;d;d=d->next)if(d->command_offset>=h->call_cursor&&d->command_offset<end){
  if(kind!=NN_COMMAND_RAW&&kind!=NN_COMMAND_COMPUTE){int e=nn_command_sequence_unsupported(h->call_sequence);if(e)return e;}
  kind=NN_COMMAND_COMPUTE;
 }
 for(struct counter_op *c=h->counter_ops;c;c=c->next)if(c->command_offset>=h->call_cursor&&c->command_offset<end){
  if(kind!=NN_COMMAND_RAW&&kind!=NN_COMMAND_COUNTER){int e=nn_command_sequence_unsupported(h->call_sequence);if(e)return e;}
  kind=NN_COMMAND_COUNTER;
 }
 int copy_error=command_copy_capture_kind(h,h->call_cursor,end,&kind);if(copy_error)return copy_error;
 h->call_limit=end;
 struct nn_command_leaf leaf={.offset=h->call_cursor,.bytes=end-h->call_cursor,.semantic_id=h->call_cursor,.kind=kind};
 int e=nn_command_sequence_native(h->call_sequence,&leaf);if(!e)h->call_cursor=end;
 return e;
}
static int command_capture_state(struct object *o,uint64_t mask,const struct nn_command_state *value)
{
 int e=command_capture_close(o);
 if(!e)e=nn_command_sequence_state(o->record->current->call_sequence,mask,value);
 if(e)error(e);
 return e;
}
static int command_capture_render(struct object *o,uint64_t mask,const struct nn_render_state *render)
{
 struct nn_command_state value={.render=*render};
 return command_capture_state(o,mask,&value);
}
/* Until all resource/program/descriptor families have typed ordered deltas,
 * they must remain visibly unqualified when referenced. The original direct
 * non-Call path continues its existing native qualification. */
static int command_capture_unknown(struct object *o)
{
 int e=command_capture_close(o);
 if(!e)e=nn_command_sequence_unsupported(o->record->current->call_sequence);
 if(e)error(e);
 return e;
}
static int command_reference_hold(void *key)
{struct object *o=key;if(!o||o->refs==SIZE_MAX)return EOVERFLOW;o->refs++;return 0;}
static void command_reference_drop(void *key)
{struct object *o=key;if(!o||!o->refs){error(EPROTO);return;}o->refs--;}
static const struct nn_command_reference_ops command_reference_ops={command_reference_hold,command_reference_drop};
static struct nn_command_binding command_binding(struct object *owner,size_t offset,size_t bytes,uint64_t address)
{return (struct nn_command_binding){.owner=owner,.ops=owner?&command_reference_ops:NULL,.offset=offset,.bytes=bytes,.address=address};}
static int command_capture_bindings(struct object *o,enum nn_command_binding_domain domain,unsigned stage,unsigned first,unsigned count,const struct nn_command_binding *values)
{
 int e=command_capture_close(o);
 if(!e)e=nn_command_sequence_bindings(o->record->current->call_sequence,domain,stage,first,count,values);
 if(e)error(e);
 return e;
}
static int command_capture_program(struct object *o,struct object *program,unsigned stages)
{
 struct nn_command_binding_write writes[6];unsigned count=0;int e=0;
 for(unsigned stage=0;stage<6;stage++)if(stages&(1u<<stage)){
  writes[count].index=stage;writes[count].value=command_binding(program,0,0,0);count++;
 }
 if((e=command_capture_close(o)))return e;
 e=nn_command_sequence_binding_writes(o->record->current->call_sequence,count,writes);
 if(e)error(e);
 return e;
}
static void api_nvnCommandBufferCallCommands(NVNcommandBuffer *key,int count,const NVNcommandHandle *ids)
{
 struct object *o=recording_command(key);if(!o)return;
 if(count<0||count>65535||(count&&!ids)){error(EINVAL);return;}
 if(o->record->in_callback){error(EDEADLK);return;}
 if(!count)return;
 const struct nn_command_sequence **sequences=calloc((size_t)count,sizeof(*sequences));if(!sequences){error(ENOMEM);return;}
 int e=0;
 for(int i=0;i<count;i++){
  struct handle *h=handles;while(h&&(h->id!=ids[i]||h->public_retired))h=h->next;
  if(!h||h->device!=o->device){e=EINVAL;break;}
  sequences[i]=h->call_sequence;
 }
 if(!e)e=command_capture_close(o);
 if(!e)e=nn_command_sequence_calls(o->record->current->call_sequence,(unsigned)count,sequences);
 if(!e)o->record->current->call_nodes=1;
 free(sequences);if(e)error(e);
}
/* Real queue/pipeline admission invokes this guard before any resources are
 * pinned. Direct nonprojected callers never
 * submit a parent containing only its own bytes while skipping child calls. */
static int command_call_admit(const struct handle *h)
{
 if(!h->call_nodes)return 0;
 struct nn_command_state state={0},final;size_t count=0;
 int e=nn_command_sequence_visit(h->call_sequence,1u<<NN_COMMAND_RAW,&state,NULL,NULL,&final,&count);
 return e?e:ENOTSUP;
}
