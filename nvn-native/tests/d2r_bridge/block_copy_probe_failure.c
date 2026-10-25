/* SPDX-License-Identifier: MIT. HOST failure/owner checks, no GPU simulation. */
#include NN_BLOCK_PROBE_SOURCE
#include <assert.h>
struct nn_device {int live;};struct nn_queue {int live;};struct nn_bo {uint64_t address;size_t bytes;void *map;int busy;};
static unsigned allocations,destroyed,destroy_calls,queue_closes,device_closes,submits,finishes,checks;
static int fault;
#define CK(x) do{checks++;assert(x);}while(0)
int nn_device_open(nn_device **d){*d=NULL;if(fault==1)return EACCES;*d=calloc(1,sizeof(**d));return *d?0:ENOMEM;}
int nn_device_close(nn_device *d){device_closes++;free(d);return 0;}
uint32_t nn_device_chip(const nn_device *d){(void)d;return 0x06050002;}
size_t nn_device_page(const nn_device *d){(void)d;return 4096;}
int nn_queue_create(nn_device *d,nn_queue **q){(void)d;*q=calloc(1,sizeof(**q));return *q?0:ENOMEM;}
int nn_queue_destroy(nn_queue *q){queue_closes++;free(q);return 0;}
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **b){(void)d;*b=NULL;allocations++;if(fault==2)return ENOMEM;*b=calloc(1,sizeof(**b));if(!*b)return ENOMEM;(*b)->bytes=(n+4095)&~(size_t)4095;(*b)->map=calloc(1,(*b)->bytes);(*b)->address=0x1000000+allocations*0x100000;return (*b)->map?0:ENOMEM;}
int nn_bo_destroy(nn_bo *b){destroy_calls++;if(b->busy)return EBUSY;destroyed++;free(b->map);free(b);return 0;}
size_t nn_bo_size(const nn_bo *b){return b->bytes;}
uint64_t nn_bo_address(const nn_bo *b){return b->address;}
void *nn_bo_map(const nn_bo *b){return b->map;}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int from){(void)b;(void)o;(void)n;(void)from;return fault==3?EIO:0;}
int nn_queue_submit(nn_queue *q,nn_bo *b,size_t o,size_t n,nn_bo *const *refs,size_t count,uint32_t *timestamp){(void)q;CK(!o&&n&&count>=3);submits++;if(fault==4)return EIO;*timestamp=1;b->busy=1;for(size_t i=0;i<count;i++)refs[i]->busy=1;return 0;}
int nn_queue_finish(nn_queue *q,uint32_t timeout){(void)q;CK(timeout==1000);finishes++;return ETIMEDOUT;}
int main(void)
{
 for(fault=1;fault<=5;fault++){
  char text[8192];unsigned a=allocations,d=destroyed,dc=destroy_calls,q=queue_closes,v=device_closes,s=submits,f=finishes;struct block_owner *prior=retained;
  CK(nn_block_copy_probe("/must-not-write-any-file",text,sizeof(text))==-1);CK(strstr(text,"\"passed\":false"));CK(strstr(text,"\"completed\":0"));
  if(fault<=3){CK(retained==prior);CK(strstr(text,"\"resources_retained\":false"));CK(submits==s&&finishes==f);if(fault==3)CK(allocations-a==destroyed-d);if(fault>1)CK(queue_closes==q+1&&device_closes==v+1);}
  else {CK(retained&&retained!=prior&&retained->next==prior);CK(retained->device&&retained->queue&&retained->code&&retained->active&&retained->active->job&&retained->active->ubo&&retained->active->data[0]&&retained->active->data[1]);CK(strstr(text,"\"resources_retained\":true"));CK(strstr(text,"\"cleanup_errno\":16"));CK(queue_closes==q&&device_closes==v&&destroyed==d&&submits==s+1);CK(destroy_calls==dc+(fault==5));CK(finishes==f+(fault==5));}
 }
 printf("{\"passed\":true,\"HOST_failure_checks\":%u,\"actual_GPU_submits\":0,\"ambiguous_owners_retained\":2}\n",checks);return 0;
}
