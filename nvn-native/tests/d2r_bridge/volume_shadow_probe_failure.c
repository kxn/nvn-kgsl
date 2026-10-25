/* SPDX-License-Identifier: MIT. No GPU emulation; only failure/retention paths. */
#include NN_VOLUME_SHADOW_PROBE_SOURCE
#include <assert.h>
struct nn_device{int live;};struct nn_queue{int live;};struct nn_bo{uint64_t address;size_t bytes;void*map;int busy;struct nn_bo*next;};static struct nn_bo*all;
static unsigned allocations,destroyed,dcalls,qclose,dclose,submits,finishes,checks;static int fault;
#define CK(x) do{checks++;assert(x);}while(0)
int nn_device_open(nn_device**d){*d=NULL;if(fault==1)return EACCES;*d=calloc(1,sizeof(**d));return*d?0:ENOMEM;}
int nn_device_close(nn_device*d){dclose++;free(d);return 0;}
uint32_t nn_device_chip(const nn_device*d){(void)d;return 0x06050002;}
size_t nn_device_page(const nn_device*d){(void)d;return 4096;}
int nn_queue_create(nn_device*d,nn_queue**q){(void)d;*q=calloc(1,sizeof(**q));return*q?0:ENOMEM;}
int nn_queue_destroy(nn_queue*q){qclose++;free(q);return 0;}
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){(void)d;*out=NULL;allocations++;if(fault==2)return ENOMEM;struct nn_bo*b=calloc(1,sizeof(*b));if(!b)return ENOMEM;b->bytes=(n+4095)&~(size_t)4095;b->map=calloc(1,b->bytes);b->address=0x1000000+(uint64_t)allocations*0x100000;b->next=all;all=b;*out=b;return b->map?0:ENOMEM;}
int nn_bo_destroy(nn_bo*b){dcalls++;if(b->busy)return EBUSY;struct nn_bo**p=&all;while(*p&&*p!=b)p=&(*p)->next;CK(*p==b);*p=b->next;free(b->map);free(b);destroyed++;return 0;}
int nn_bo_idle(const nn_bo*b){return !b?EINVAL:b->busy?EBUSY:0;}
size_t nn_bo_size(const nn_bo*b){return b->bytes;}
uint64_t nn_bo_address(const nn_bo*b){return b->address;}
void*nn_bo_map(const nn_bo*b){return b->map;}
int nn_bo_cache(nn_bo*b,size_t o,size_t n,int f){(void)b;(void)o;(void)n;(void)f;return fault==3?EIO:0;}
int nn_queue_submit(nn_queue*q,nn_bo*b,size_t o,size_t n,nn_bo*const*refs,size_t count,uint32_t*ts){(void)q;CK(!o&&n&&count>=5);submits++;if(fault==4)return EIO;b->busy=1;for(size_t i=0;i<count;i++)refs[i]->busy=1;*ts=submits;return 0;}
int nn_queue_finish(nn_queue*q,uint32_t timeout){(void)q;CK(timeout==1000);finishes++;if(fault==5)return ETIMEDOUT;for(nn_bo*b=all;b;b=b->next)b->busy=0;return 0;}
int main(int argc,char**argv)
{
 CK(argc==2);for(fault=1;fault<=6;fault++){
  char json[8192];unsigned a=allocations,d=destroyed,dc=dcalls,q=qclose,v=dclose,s=submits,f=finishes;struct shadow_process*prior=retained;
  CK(nn_public_copy_probe(argv[1],json,sizeof(json))==-1);CK(strstr(json,"\"passed\":false"));CK(strstr(json,"\"completed\":0"));
  if(fault<=3||fault==6){CK(retained==prior&&strstr(json,"\"resources_retained\":false"));if(fault>1)CK(qclose==q+1&&dclose==v+1);if(fault==3||fault==6)CK(allocations-a==destroyed-d);}
  else{CK(retained&&retained!=prior&&retained->next==prior&&retained->d&&retained->q&&retained->a&&retained->a->canonical&&retained->a->owner.storage&&retained->a->owner.code&&retained->a->owner.dispatch[0]);CK(strstr(json,"\"resources_retained\":true"));CK(strstr(json,"\"cleanup_errno\":16"));CK(qclose==q&&dclose==v&&destroyed==d&&submits==s+1&&finishes==f+(fault==5));CK(dcalls==dc+(fault==5?2:0));}
  if(fault==6){CK(strstr(json,"\"stage\":\"readonly\""));CK(strstr(json,"\"files\":1"));}
 }
 printf("{\"passed\":true,\"HOST_failure_checks\":%u,\"actual_GPU_submits\":0,\"ambiguous_owners_retained\":2,\"nonexecuting_queue_rejected\":true}\n",checks);return 0;
}
