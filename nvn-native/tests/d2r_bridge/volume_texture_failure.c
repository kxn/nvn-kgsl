/* SPDX-License-Identifier: MIT. Actual probe error/retained owners, NO GPU execution. */
#define _POSIX_C_SOURCE 200809L
#define nn_queue_submit ram_submit
#define nn_queue_finish ram_finish
#define nn_bo_alloc ram_alloc
#include NN_MEMORY_BACKEND
#undef nn_queue_submit
#undef nn_queue_finish
#undef nn_bo_alloc
static unsigned submits,finishes,allocations,allocation_failure,mode;
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){*out=NULL;if(++allocations==allocation_failure)return ENOMEM;return ram_alloc(d,n,out);}
int nn_queue_submit(nn_queue*q,nn_bo*b,size_t o,size_t n,nn_bo*const*r,size_t count,uint32_t*t)
{(void)o;(void)n;if(!q||!b||!t||(count&&!r))return EINVAL;submits++;if(mode==1)return EIO;*t=submits;for(size_t i=0;i<=count;i++){nn_bo*x=i==count?b:r[i];unsigned j=0;while(j<q->count&&q->pending[j]!=x)j++;if(j==q->count){assert(q->count<64);q->pending[q->count++]=x;x->busy++;}}return 0;}
int nn_queue_finish(nn_queue*q,uint32_t t){finishes++;if(mode==2)return EIO;return ram_finish(q,t);}
#include "../volume_texture_probe.c"
static unsigned checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%u %s\n",__LINE__,#x);abort();}checks++;}while(0)
int main(void)
{
 char result[8192];
 for(unsigned test=0;test<4;test++){
  mode=test==0?1:test==1?2:0;submits=finishes=allocations=0;allocation_failure=test==3?5:0;
  CHECK(nn_public_copy_probe("/nonexistent-HOST-no-readback",result,sizeof(result))!=0);CHECK(strstr(result,"\"passed\":false"));CHECK(!strstr(result,"\"stage\":\"capture\""));
  if(test<2){CHECK(quarantine&&quarantine->d&&quarantine->a&&quarantine->q);CHECK(quarantine->d->bos>=12);CHECK(strstr(result,"\"resources_retained\":true"));CHECK(strstr(result,"\"cleanup_errno\":16"));
   if(test==0){CHECK(submits==1&&!finishes);CHECK(strstr(result,"\"stage\":\"materialize\""));}else{CHECK(submits==2&&finishes==1);CHECK(strstr(result,"\"stage\":\"finish\""));CHECK(nn_bo_destroy(quarantine->a->owner.storage)==EBUSY);}
   /* HOST RAM never executed hardware. Explicitly drain fake busy ledger,
    * then release inspected owners; this is NOT a production unknown recovery. */
   struct process*o=quarantine;quarantine=o->next;while(o->q->count)o->q->pending[--o->q->count]->busy--;CHECK(!release(o));free(o);
  }else{CHECK(!quarantine);CHECK(strstr(result,"\"resources_retained\":false"));CHECK(strstr(result,"\"cleanup_errno\":0"));if(test==2){CHECK(submits==2&&finishes==1);CHECK(strstr(result,"\"stage\":\"metadata-readonly\""));}else CHECK(!submits&&!finishes);}
 }
 /* The PM4-declared timestamp destination is writable; adjacent command bytes remain immutable. */
 allocation_failure=allocations=mode=0;nn_device*d=NULL;nn_bo*b=NULL;CHECK(!nn_device_open(&d));CHECK(!nn_bo_alloc(d,4096,&b));memset(nn_bo_map(b),0xa5,4096);struct shot watch={0};size_t watches=0;CHECK(!take(&watch,&watches,1,b));watch.gpu_timestamp_offset=4092;memset((char*)nn_bo_map(b)+4092,0x73,4);uint64_t compared=0;CHECK(!check_shots(&watch,1,&compared,2));CHECK(compared==4096);((unsigned char*)nn_bo_map(b))[4091]^=1;CHECK(check_shots(&watch,1,&compared,2)==EIO);CHECK(metadata_failure.group==2&&metadata_failure.offset==4091);free_shots(&watch,&watches);CHECK(!nn_bo_destroy(b));CHECK(!nn_device_close(d));
 printf("{\"passed\":true,\"checks\":%u,\"cases\":4,\"gpu_execution\":false,\"no_op_GPU_rejected\":true,\"unknown_owners_retained\":true}\n",checks);return 0;
}
