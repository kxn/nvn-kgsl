/* SPDX-License-Identifier: MIT. Actual NVN APIs, nonexecuting HOST backend.
 * Mock queues retain refs only. They never execute image/CP instructions. */
#define _POSIX_C_SOURCE 200809L
#define nn_device_open raw_open
#define nn_device_close raw_close
#define nn_bo_alloc raw_alloc
#define nn_bo_destroy raw_destroy
#define nn_bo_cache raw_cache
#define nn_queue_submit raw_submit
#define nn_queue_finish raw_finish
#include NN_PUBLIC_MEMORY_BACKEND
#undef nn_device_open
#undef nn_device_close
#undef nn_bo_alloc
#undef nn_bo_destroy
#undef nn_bo_cache
#undef nn_queue_submit
#undef nn_queue_finish
static unsigned fault,allocs,destroys,submissions,finishes,device_closes,checks;
int nn_device_inspect(struct nn_device_identity *i)
{if(!i)return EINVAL;*i=(struct nn_device_identity){.chip_id=0x06050002,.page_bytes=4096,.gmem_bytes=1179648};return 0;}
int nn_device_open(nn_device **d){if(fault==1){*d=NULL;return EACCES;}return raw_open(d);}
int nn_device_close(nn_device *d){device_closes++;return raw_close(d);}
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **b){allocs++;if(fault==2){*b=NULL;return ENOMEM;}return raw_alloc(d,n,b);}
int nn_bo_destroy(nn_bo *b){int e=raw_destroy(b);if(!e)destroys++;return e;}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int from){return fault==3?EIO:raw_cache(b,o,n,from);}
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
 (void)offset;assert(bytes&&count&&count+1<=64-q->count);submissions++;
 if(fault==4)return EIO;
 q->pending[q->count++]=ib;ib->busy++;
 for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}
 *timestamp=submissions;return 0;
}
int nn_queue_finish(nn_queue *q,uint32_t timeout)
{finishes++;if(fault==5)return ETIMEDOUT;return raw_finish(q,timeout);}
#include NN_PUBLIC_PROBE_SOURCE
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"HOST failure line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int admission(void)
{
 struct npc_owner o={0};CK(!api_load(&o.api));struct npc_api *p=&o.api;NVNdeviceBuilder db={0};NVNqueueBuilder qb={0};nn_nvn_clear_error();fault=7;
 p->nvnDeviceBuilderSetDefaults(&db);CK(p->nvnDeviceInitialize(&o.device,&db));o.device_ready=1;CK(!NN_PUBLIC_COPY_REGISTER(&o.device));
 p->nvnQueueBuilderSetDefaults(&qb);p->nvnQueueBuilderSetDevice(&qb,&o.device);CK(p->nvnQueueInitialize(&o.queue,&qb));o.queue_ready=1;
 for(unsigned n=0;n<NN_PUBLIC_COPY_CASES;n++){
  const struct nn_public_copy_case *c=nn_public_copy_cases+n;o.active=calloc(1,sizeof(*o.active));CK(o.active);unsigned clones=0,pins=0;
  CK(!pool_create(&o,0,c->pool_bytes[0]));if(c->pool_bytes[1])CK(!pool_create(&o,1,c->pool_bytes[1]));CK(!pool_create(&o,2,65536));
  CK(!texture_create(&o,c,0,0,c->canonical_offset));if(c->family==2)CK(!texture_create(&o,c,1,c->shared?0:1,c->destination_offset));
  CK(!buffer_create(&o,0,0,c->canonical_offset,c->source.payload_bytes));CK(!buffer_create(&o,1,c->shared?0:1,c->family==2?c->destination_offset:c->linear_offset,c->family==2?c->source.payload_bytes:c->linear_bytes));
  int error=record_copy(&o,c,&clones,&pins);if(error)fprintf(stderr,"admission case%u error%d\n",n,error);CK(!error);CK(clones==1&&pins==(c->family==2?2u:1u));
  for(unsigned replay=0;replay<2;replay++){
   p->nvnQueueSubmitCommands(&o.queue,1,o.active->handles+1);CK(!nn_nvn_error());
   p->nvnQueueFinish(&o.queue);CK(!nn_nvn_error());
  }
  CK(!destroy_active(&o));
 }
 p->nvnQueueFinalize(&o.queue);CK(!nn_nvn_error());p->nvnDeviceFinalize(&o.device);CK(!nn_nvn_error());return 0;
}
int main(int argc,char **argv)
{
 CK(argc==2);CK(nn_public_copy_probe(NULL,NULL,0)==-1);
 for(fault=1;fault<=6;fault++){
  unsigned s=submissions,f=finishes,closed=device_closes;struct npc_owner *prior=retained;char text[8192];
  CK(nn_public_copy_probe(argv[1],text,sizeof(text))==-1);
  CK(strstr(text,"\"passed\":false"));CK(strstr(text,"\"completed\":0"));
  if(fault==4||fault==5){
   CK(retained&&retained!=prior&&retained->next==prior);CK(retained->device_ready&&retained->queue_ready&&retained->active);
   CK(retained->active->handles[1]&&!retained->active->handles[0]);
   CK(!retained->active->commands_ready[0]&&!retained->active->commands_ready[1]);
   CK(retained->active->textures_ready[0]&&retained->active->buffers_ready[0]&&retained->active->buffers_ready[1]);
   CK(retained->active->pools_ready[0]&&retained->active->pools_ready[1]&&retained->active->pools_ready[2]);
   CK(strstr(text,"\"resources_retained\":true")&&strstr(text,"\"cleanup_errno\":16"));CK(device_closes==closed);
   CK(submissions==s+1&&finishes==f+(fault==5));
   #if NVN_PUBLIC_COPY_CALL
   CK(strstr(text,"\"CopyCommands_calls\":0")&&strstr(text,"\"CallCommands_calls\":1")&&strstr(text,"\"call_commands\":true"));
#else
   CK(strstr(text,"\"CopyCommands_calls\":1"));
#endif
   CK(strstr(text,"\"recorded_texture_finalize_rejections\":1"));
  }else{
   CK(prior==retained);CK(strstr(text,"\"resources_retained\":false")&&strstr(text,"\"cleanup_errno\":0"));
   if(fault<4){CK(submissions==s&&finishes==f);}
   else{CK(submissions==s+1&&finishes==f+1);CK(strstr(text,"\"files\":1"));CK(strstr(text,"\"stage\":\"all-bytes\""));}
   if(fault>1)CK(device_closes==closed+1);
  }
 }
 CK(!admission());
 printf("{\"passed\":true,\"HOST_failure_checks\":%u,\"HOST_public_copy_admission_cases\":%u,\"HOST_nonexecuting_submits\":%u,\"actual_GPU_submits\":0,\"ambiguous_owners_retained\":2,\"nonexecuting_queue_rejected_by_byte_oracle\":true}\n",checks,NN_PUBLIC_COPY_CASES,NN_PUBLIC_COPY_CASES*2);return 0;
}
