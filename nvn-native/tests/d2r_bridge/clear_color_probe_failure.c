/* SPDX-License-Identifier: MIT. Mock owns jobs; never executes GPU instructions. */
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
 (void)offset;assert(bytes&&count&&count+1<=64-q->count);submissions++;if(fault==4)return EIO;
 q->pending[q->count++]=ib;ib->busy++;for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}*timestamp=submissions;return 0;
}
int nn_queue_finish(nn_queue *q,uint32_t timeout)
{finishes++;if(fault==5)return ETIMEDOUT;return raw_finish(q,timeout);}
#include NN_CLEAR_PROBE_SOURCE
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"clear HOST failure line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
 CK(argc==2);CK(nn_public_copy_probe(NULL,NULL,0)==-1);
 for(fault=1;fault<=5;fault++){
  unsigned s=submissions,f=finishes,closed=device_closes;struct ncc_owner *prior=retained;char text[8192];
  CK(nn_public_copy_probe(argv[1],text,sizeof(text))==-1);CK(strstr(text,"\"passed\":false"));CK(strstr(text,"\"completed\":0"));
  if(fault==4||fault==5){
   CK(retained&&retained!=prior&&retained->next==prior);CK(retained->device_ready&&retained->queue_ready&&retained->active);
   CK(retained->active->handles[2]&&retained->active->handles[3]&&!retained->active->handles[0]&&!retained->active->handles[1]);
   for(unsigned i=0;i<3;i++)CK(!retained->active->commands_ready[i]);
   for(unsigned i=0;i<2;i++)CK(retained->active->textures_ready[i]&&retained->active->pools_ready[i]);
   CK(strstr(text,"\"resources_retained\":true")&&strstr(text,"\"cleanup_errno\":16"));CK(device_closes==closed);CK(submissions==s+1&&finishes==f+(fault==5));
  }else{CK(prior==retained);CK(strstr(text,"\"resources_retained\":false")&&strstr(text,"\"cleanup_errno\":0"));CK(submissions==s&&finishes==f);if(fault>1)CK(device_closes==closed+1);}
 }
 /* Full ordinary + Call constructor/queue/lifetime matrix using a RAM queue.
  * Raw exports remain their input seeds. The independent HOST oracle MUST
  * reject them; the checker records this as zero actual GPU submissions. */
 fault=6;unsigned before=submissions;char text[8192];CK(!nn_public_copy_probe(argv[1],text,sizeof(text)));CK(submissions-before==NN_CLEAR_PROBE_FILES);CK(strstr(text,"\"completed\":536"));CK(strstr(text,"\"resources_retained\":false"));
 printf("{\"passed\":true,\"HOST_failure_checks\":%u,\"HOST_nonexecuting_submits\":%u,\"actual_GPU_submits\":0,\"ambiguous_owners_retained\":2,\"HOST_constructor_cases\":134}\n",checks,NN_CLEAR_PROBE_FILES);return 0;
}
