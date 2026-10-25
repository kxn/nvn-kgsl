/* SPDX-License-Identifier: MIT. Enqueue expansion over original command pool
 * chunks. This qualified internal path handles RAW native packet spans and generic Draw
 * with explicitly resolved GPU state. Compute dispatches rebuild through the existing native encoder; counter remains ENOTSUP. No
 * guest CPU instruction or GPU command is executed by this CPU metadata walk.
 * The existing queue_stage_commands/private-parent/KGSL retirement path owns
 * the actual command-buffer staging and native submission. */
#include "nvn_command_draw_projection.inc"
#include "nvn_command_compute_projection.inc"
struct command_projection {struct object *queue;struct handle *handle;};
static int command_project_chunk(struct handle *dst,struct object *pool,size_t offset,size_t bytes)
{
 if(!pool||!pool->bo||!nn_bo_map(pool->bo)||offset>pool->size||bytes>pool->size-offset)return EINVAL;
 if(pool->refs==SIZE_MAX)return EOVERFLOW;
 struct command_chunk *chunk=calloc(1,sizeof(*chunk));if(!chunk)return ENOMEM;
 *chunk=(struct command_chunk){.pool=pool,.offset=offset,.bytes=bytes};pool->refs++;
 if(dst->chunks_tail)dst->chunks_tail->next=chunk;else dst->chunks=chunk;dst->chunks_tail=chunk;
 return 0;
}
static int command_project_range(struct handle *dst,const struct handle *src,size_t offset,size_t bytes)
{
 if(offset>src->bytes||bytes>src->bytes-offset)return EINVAL;
 if(!src->chunks)return command_project_chunk(dst,src->commands,src->offset+offset,bytes);
 size_t position=0,remaining=bytes,skip=offset;
 for(const struct command_chunk *c=src->chunks;c&&remaining;c=c->next){
  if(c->bytes>src->bytes-position)return EINVAL;
  position+=c->bytes;
  if(skip>=c->bytes){skip-=c->bytes;continue;}
  size_t n=c->bytes-skip;if(n>remaining)n=remaining;
  if(skip>SIZE_MAX-c->offset)return EOVERFLOW;
  int e=command_project_chunk(dst,c->pool,c->offset+skip,n);if(e)return e;
  remaining-=n;skip=0;
 }
 return remaining?EINVAL:0;
}
static int command_project_terminal(const struct handle *source,const struct nn_command_leaf *leaf)
{
 if(leaf->kind!=NN_COMMAND_RAW||leaf->bytes!=8||source->bytes<8||leaf->offset!=source->bytes-8)return 0;
 const struct object *pool;size_t offset;
 if(source->chunks){
  const struct command_chunk *tail=source->chunks_tail;if(!tail||tail->bytes<8)return 0;
  pool=tail->pool;offset=tail->offset+tail->bytes-8;
 }else{pool=source->commands;offset=source->offset+source->bytes-8;}
 if(!pool||!pool->bo||offset>pool->size||8>pool->size-offset)return 0;
 const void *map=nn_bo_map(pool->bo);if(!map)return 0;
 const uint32_t expected[]={nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0),nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0)};
 return !memcmp((const char*)map+offset,expected,8);
}
static int command_project_native(void *key,const void *source,const struct nn_command_leaf *leaf,const struct nn_command_state *state)
{
 struct command_projection *p=key;struct handle *h=p->handle;const struct handle *src=source;
 if((leaf->kind!=NN_COMMAND_RAW&&leaf->kind!=NN_COMMAND_DRAW&&leaf->kind!=NN_COMMAND_COMPUTE&&leaf->kind!=NN_COMMAND_COPY)||src->wait_count||command_source_jobs_known(src))return ENOTSUP;
 if(src->count>65535||src->cpu_count>65535)return E2BIG;
 if(h->bytes>p->queue->queue_config.command||leaf->bytes>p->queue->queue_config.command-h->bytes)return ENOSPC;
 /* Typed encoders below register every effective GPU consumer. The source
  * DAG retains its old recorded CPU owners, even after public Finalize; an
  * unused old binding must not become a fake GPU access. Only the verified
  * resource-free terminal WFI/WFM is exempted from conservative RAW ownership.
  * Other RAW families preserve their existing resource admission. */
 if(leaf->kind==NN_COMMAND_RAW&&!command_project_terminal(src,leaf))
  for(size_t i=0;i<src->count;i++){
   if(src->resources[i]->refs==SIZE_MAX)return EOVERFLOW;
   int e=add_resource(h,src->resources[i]);if(e)return e;
  }
 for(size_t i=0;i<src->cpu_count;i++){
  if(src->cpu_resources[i]->refs==SIZE_MAX)return EOVERFLOW;
  int e=add_cpu_resource(h,src->cpu_resources[i]);if(e)return e;
 }
 if(leaf->kind==NN_COMMAND_DRAW){
  for(const struct late_draw *draw=src->draws;draw;draw=draw->next)if(draw->command_offset>=leaf->offset&&draw->command_offset-leaf->offset<leaf->bytes){
   size_t relative=draw->command_offset-leaf->offset;if(leaf->bytes-relative<16||relative>SIZE_MAX-h->bytes)return EINVAL;
   int e=command_project_draw(h,draw,state,h->bytes+relative);if(e)return e;
  }
 }
 if(leaf->kind==NN_COMMAND_COMPUTE){
  unsigned found=0;
  for(const struct command_dispatch *d=src->dispatches;d;d=d->next)if(d->command_offset>=leaf->offset&&d->command_offset-leaf->offset<leaf->bytes){
   size_t relative=d->command_offset-leaf->offset;if(leaf->bytes-relative<16||relative>SIZE_MAX-h->bytes)return EINVAL;
   int e=command_project_compute(h,d,state,h->bytes+relative);if(e)return e;found++;
  }
  if(!found)return ENOTSUP;
 }
 if(leaf->kind==NN_COMMAND_COPY){int e=command_project_copies(h,src,leaf,state);if(e)return e;}
 int e=command_project_range(h,src,leaf->offset,leaf->bytes);if(e)return e;
 h->bytes+=leaf->bytes;return 0;
}
static void command_projection_discard(struct handle *h)
{
 if(!h)return;
 for(size_t i=0;i<h->count;i++)h->resources[i]->refs--;
 for(size_t i=0;i<h->cpu_count;i++)h->cpu_resources[i]->refs--;
 free(h->resources);free(h->cpu_resources);free_draws(h->draws);retire_jobs(h->device,h->jobs);free_chunks(h->chunks);free(h);
}
static int command_project_sequence(struct object *queue,const struct handle *source,const struct nn_command_state *initial,struct nn_command_state *final,struct handle **out)
{
 *out=NULL;struct handle *h=calloc(1,sizeof(*h));if(!h)return ENOMEM;
 h->device=queue->device;h->public_retired=1;
 struct command_projection p={queue,h};size_t native_count=0;
 int e=nn_command_sequence_visit(source->call_sequence,(1u<<NN_COMMAND_RAW)|(1u<<NN_COMMAND_DRAW)|(1u<<NN_COMMAND_COMPUTE)|(1u<<NN_COMMAND_COPY),initial,command_project_native,&p,final,&native_count);
 if(e||!native_count||!h->chunks){command_projection_discard(h);return e?e:EINVAL;}
 /* The projection is a temporary body whose arena belongs to original
  * resource owners. It enters no public handle lookup or all-body ledger. */
 h->commands=h->chunks->pool;h->offset=h->chunks->offset;
 if(h->commands->refs==SIZE_MAX||h->device->refs==SIZE_MAX){command_projection_discard(h);return EOVERFLOW;}
 h->commands->refs++;h->device->refs++;*out=h;return 0;
}
/* Returns -1 when the unchanged direct native path applies. All projection
 * and state preparation precedes GPU submission. A failed submission poisons
 * this experimental Call state rather than guessing which GPU writes landed.
 * The qualified RAW/generic Draw/native compute Call profile is exported. */
static int command_call_submit(NVNqueue *key,struct object *queue,int count,const NVNcommandHandle *ids)
{
 if(!count)return -1;
 unsigned applicable=queue->call_queue_active;
 for(int i=0;i<count;i++){
  struct handle *h=handles;while(h&&(h->id!=ids[i]||h->public_retired))h=h->next;
  if(!h||h->device!=queue->device)return EINVAL;
  applicable|=h->call_nodes;
 }
 if(!applicable)return -1;
 if(queue->call_queue_poisoned)return ENOTSUP;
 if((size_t)count>SIZE_MAX/sizeof(struct handle *))return EOVERFLOW;
 struct handle **projected=calloc((size_t)count+1,sizeof(*projected));if(!projected)return ENOMEM;
 struct nn_command_state *state=calloc(1,sizeof(*state));if(!state){free(projected);return ENOMEM;}
 if(queue->call_queue_state)*state=*queue->call_queue_state;
 int e=0,submitted=0;
 for(int i=0;i<count;i++){
  struct handle *h=handles;while(h&&(h->id!=ids[i]||h->public_retired))h=h->next;
  e=command_project_sequence(queue,h,state,state,projected+i);if(e)goto finish;
 }
 {struct pipeline_batch batch={0};e=pipeline_batch_prepare_owned(queue,count,projected,&batch);
  if(e){pipeline_batch_discard(&batch);goto finish;}
  submitted=1;queue_submit_prepared_selected(key,count,ids,&batch,projected);e=last_error;
  pipeline_batch_discard(&batch);
 }
 if(!e){free(queue->call_queue_state);queue->call_queue_state=state;state=NULL;queue->call_queue_active=1;}
 else if(submitted){queue->call_queue_active=1;queue->call_queue_poisoned=1;}
 finish:
 for(int i=0;i<count;i++)if(projected[i])command_body_destroy(projected[i]);
 free(state);free(projected);return e;
}

/* The ordinary path may execute before the first Call. Observe only after
 * the actual native queue returned success; failed/uncertain submission or
 * observation failure invalidates continuation instead of publishing writes
 * that may not have executed. Unknown nodes reset the frame, while later
 * explicit writes can re-establish individual fields. This weak metadata
 * never extends public resource lifetime or changes the submission result. */
static void command_observe_submitted(struct object *queue,int count,const NVNcommandHandle *ids,int submission_error)
{
 if(!count)return;
 if(submission_error){free(queue->call_queue_state);queue->call_queue_state=NULL;return;}
 struct nn_command_state *state=calloc(1,sizeof(*state));
 if(state&&queue->call_queue_state)*state=*queue->call_queue_state;
 int e=state?0:ENOMEM;
 for(int i=0;i<count&&!e;i++){
  struct handle *h=handles;while(h&&(h->id!=ids[i]||h->public_retired))h=h->next;
  e=h?nn_command_sequence_observe(h->call_sequence,state,state):EINVAL;
 }
 free(queue->call_queue_state);queue->call_queue_state=e?NULL:state;if(e)free(state);
}
