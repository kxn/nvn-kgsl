/* HOST ownership/failure test. The backend only tracks references, never runs PM4. */
#include "nvn_native/copy_job.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include NVN_COPY_GRAPHICS_HEADER
struct nn_device { unsigned chip; };
struct nn_queue { unsigned timestamp,busy; };
struct nn_bo { void *map; size_t bytes; uint64_t address; unsigned flags,busy; };
struct fixture { nn_device dev;nn_queue queue;nn_bo src,dst;unsigned pins,pincalls,unpins,validates,proofs,allocs,submits,finishes,destroys;
 int pin_error,validate_error,proof_error,unpin_error,alloc_error,submit_error,finish_error,destroy_error,map_null;nn_bo *job_bo; };
static struct fixture *f;static unsigned checks;
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"check line%d %s\n",__LINE__,#x);abort();}}while(0)
uint32_t nn_device_chip(const nn_device *d){return d->chip;}
size_t nn_device_page(const nn_device *d){(void)d;return 4096;}
size_t nn_device_gmem_bytes(const nn_device *d){(void)d;return 1179648;}
uint32_t nn_queue_timestamp(const nn_queue *q){return q->timestamp;}
uint64_t nn_bo_address(const nn_bo *b){return b->address;}
size_t nn_bo_size(const nn_bo *b){return b->bytes;}
uint32_t nn_bo_flags(const nn_bo *b){return b->flags;}
void *nn_bo_map(const nn_bo *b){C(b!=&f->src && b!=&f->dst);return f->map_null?NULL:b->map;}
int nn_bo_alloc(nn_device *d,size_t bytes,nn_bo **out){C(d==&f->dev);++f->allocs;*out=NULL;if(f->alloc_error)return f->alloc_error;
 nn_bo *b=calloc(1,sizeof(*b));C(b);b->bytes=(bytes+4095)&~(size_t)4095;b->address=0x33000000;b->map=malloc(b->bytes);C(b->map);f->job_bo=b;*out=b;return 0;}
int nn_bo_destroy(nn_bo *b){++f->destroys;if(f->destroy_error)return f->destroy_error;if(b->busy)return EBUSY;C(b==f->job_bo);free(b->map);free(b);f->job_bo=NULL;return 0;}
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t off,size_t bytes,nn_bo *const *rs,size_t n,uint32_t *ts){
 C(q==&f->queue&&ib==f->job_bo&&!off&&bytes==98324&&n==2&&rs[0]==&f->src&&rs[1]==&f->dst);
 C(f->pins==1);++f->submits;ib->busy++;f->src.busy++;f->dst.busy++;q->busy++;if(f->submit_error)return f->submit_error;
 *ts=++q->timestamp;return 0;}
int nn_queue_finish(nn_queue *q,uint32_t ms){C(q==&f->queue&&ms&&ms<=1500);++f->finishes;if(f->finish_error)return f->finish_error;
 if(q->busy){C(f->job_bo&&f->job_bo->busy);f->job_bo->busy--;f->src.busy--;f->dst.busy--;q->busy--;}return 0;}
static int pin(void *p){C(p==f);++f->pincalls;if(f->pin_error)return f->pin_error;f->pins++;return 0;}
static int live(void *p){C(p==f);++f->validates;return f->validate_error;}
static int prove(void *p){C(p==f);++f->proofs;return f->proof_error;}
static int unpin(void *p){C(p==f&&f->pins==1);++f->unpins;if(f->unpin_error)return f->unpin_error;f->pins--;return 0;}
static struct nn_copy_desc init(void){
 f=calloc(1,sizeof(*f));C(f);f->dev.chip=0x06050002;f->queue.timestamp=9;
 f->src=(nn_bo){.address=0x11000000,.bytes=24576};f->dst=(nn_bo){.address=0x22000000,.bytes=16384};
 return(struct nn_copy_desc){.device=&f->dev,.producer=&f->queue,.source=&f->src,.target=&f->dst,
 .source_offset=4096,.source_bytes=16384,.target_bytes=16384,.source_gpu_address=0x11001000,
 .producer_timestamp=9,.cpu_no_access=1,.alias_proof=NN_COPY_FRESH_PRIVATE_PAGES_AND_FRESH_AHB,
 .source_image=&NVN_GRAPHICS_IMAGE,.owner={f,pin,live,prove,unpin}};
}
static void clean(void){C(!f->pins&&!f->job_bo);free(f);f=NULL;}
static void retained(nn_copy_job *j,int error){struct nn_copy_stats st={0};C(j);C(!nn_copy_job_stats(j,&st));
 C(st.state==NN_COPY_RETAINED&&st.resources_retained&&st.owner_pinned&&st.first_error==error);
 unsigned submissions=f->submits,finishes=f->finishes,destroys=f->destroys;
 C(nn_copy_job_submit(j,NN_COPY_SAME_QUEUE)==EBUSY);C(nn_copy_job_finish(j,1000)==EBUSY);C(nn_copy_job_destroy(j)==EBUSY);
 C(f->pins==1&&f->submits==submissions&&f->finishes==finishes&&f->destroys==destroys);
 /* Deliberately retained until process exit: never simulate safe cleanup. */
}
int main(void){
 unsigned cases=0;nn_copy_job *j=NULL;
 for(int order=NN_COPY_SAME_QUEUE;order<=NN_COPY_AFTER_FINISH;order++){
  struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));C(f->pins==1);C(!nn_copy_job_submit(j,order));
  C(nn_copy_job_destroy(j)==EBUSY);C(f->submits==1&&f->finishes==(unsigned)(order==NN_COPY_AFTER_FINISH));
  C(!nn_copy_job_finish(j,1000));struct nn_copy_stats st;C(!nn_copy_job_stats(j,&st));
  C(st.state==NN_COPY_FINISHED&&st.copy_submissions==1&&st.command_readonly_checks==98324&&st.command_guard_checks==4076);
  C(!nn_copy_job_destroy(j));clean();cases++;
 }
 {struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));C(!nn_copy_job_destroy(j));clean();cases++;}
 for(unsigned kind=0;kind<11;kind++){
  struct nn_copy_desc d=init();struct nn_gfx_image bad=*d.source_image;uint32_t words[8192];
  if(kind==0)d.cpu_no_access=0;
  if(kind==1)d.alias_proof=NN_COPY_ALIAS_UNKNOWN;
  if(kind==2)d.producer_timestamp=8;
  if(kind==3)d.source_gpu_address++;
  if(kind==4)d.source_bytes--;
  if(kind==5)d.source_offset++;
  if(kind==6)d.target=d.source;
  if(kind==7)d.owner.prove_disjoint=NULL;
  if(kind==8){bad.schema=2;d.source_image=&bad;}
  if(kind==9){memcpy(words,bad.commands,bad.command_words*4);words[bad.command_words-26+1]=0;bad.commands=words;d.source_image=&bad;}
  if(kind==10){f->dst.address=f->src.address;d.source_offset=0;d.source_gpu_address=f->src.address;}
  j=(nn_copy_job *)1;C(nn_copy_job_create(&d,&j)!=0&&!j);C(!f->allocs&&!f->pincalls&&!f->submits);clean();cases++;
 }
 for(unsigned kind=0;kind<5;kind++){
  struct nn_copy_desc d=init();
  if(kind==0)f->validate_error=EBUSY;
  if(kind==1)f->proof_error=ENOTSUP;
  if(kind==2)f->pin_error=ENOMEM;
  if(kind==3)f->alloc_error=ENOMEM;
  if(kind==4)f->map_null=1;
  C(nn_copy_job_create(&d,&j)!=0&&!j);C(!f->submits);clean();cases++;
 }
 {struct nn_copy_desc d=init();f->map_null=1;f->destroy_error=EIO;C(nn_copy_job_create(&d,&j)==EFAULT);retained(j,EIO);cases++;}
 {struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));f->proof_error=ENOTSUP;C(nn_copy_job_submit(j,NN_COPY_SAME_QUEUE)==ENOTSUP);
  C(!f->submits);C(!nn_copy_job_destroy(j));clean();cases++;}
 {struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));f->queue.timestamp++;C(nn_copy_job_submit(j,NN_COPY_SAME_QUEUE)==ESTALE);
  C(!f->submits);C(!nn_copy_job_destroy(j));clean();cases++;}
 {struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));f->submit_error=EIO;C(nn_copy_job_submit(j,NN_COPY_SAME_QUEUE)==EIO);retained(j,EIO);cases++;}
 {struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));f->finish_error=ETIMEDOUT;
  C(nn_copy_job_submit(j,NN_COPY_AFTER_FINISH)==ETIMEDOUT);C(!f->submits);retained(j,ETIMEDOUT);cases++;}
 {struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));C(!nn_copy_job_submit(j,NN_COPY_SAME_QUEUE));f->finish_error=ETIMEDOUT;
  C(nn_copy_job_finish(j,1000)==ETIMEDOUT);retained(j,ETIMEDOUT);cases++;}
 {struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));C(!nn_copy_job_submit(j,NN_COPY_SAME_QUEUE));
  ((char *)f->job_bo->map)[98324]^=1;C(nn_copy_job_finish(j,1000)==EIO);retained(j,EIO);cases++;}
 {struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));C(!nn_copy_job_submit(j,NN_COPY_SAME_QUEUE));C(!nn_copy_job_finish(j,1000));
  f->destroy_error=EIO;C(nn_copy_job_destroy(j)==EIO);retained(j,EIO);cases++;}
 {struct nn_copy_desc d=init();C(!nn_copy_job_create(&d,&j));f->unpin_error=EIO;C(nn_copy_job_destroy(j)==EIO);retained(j,EIO);C(!f->job_bo);cases++;}
 printf("{\"passed\":true,\"cases\":%u,\"checks\":%u,\"gpu_executes\":0,\"unknown_owners_retained\":true}\n",cases,checks);return 0;
}
