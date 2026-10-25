/* HOST ONLY. Real stage/C1 upload and BO ownership; never executes GPU. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_alloc ram_alloc
#define nn_bo_cache ram_cache
#define nn_bo_destroy ram_destroy
#include NN_PROGRAM_MEMORY_BACKEND
#undef nn_bo_alloc
#undef nn_bo_cache
#undef nn_bo_destroy
static unsigned alloc_calls,cache_calls,destroy_calls;static unsigned fail_alloc_at,fail_cache_at,fail_destroy_at;
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **o){alloc_calls++;if(alloc_calls==fail_alloc_at){*o=NULL;return ENOMEM;}return ram_alloc(d,n,o);}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int f){cache_calls++;if(cache_calls==fail_cache_at)return EIO;return ram_cache(b,o,n,f);}
int nn_bo_destroy(nn_bo *b){destroy_calls++;if(destroy_calls==fail_destroy_at)return EIO;return ram_destroy(b);}
#include "nvn_native/pipeline_program.h"
#include NN_PROGRAM_ARCHIVE
static unsigned checks;
#define CHECK(v) do{assert(v);checks++;}while(0)
int main(void)
{
 nn_device *d=NULL;CHECK(!nn_device_open(&d));const struct nn_offline_graphics_archive *a=&nn_offline_graphics;CHECK(a->stage_count>0&&a->pair_count>0);
 for(size_t i=0;i<a->pair_count;i++) {
  const struct nn_offline_graphics_pair *pair=a->pairs+i;const struct nn_offline_graphics_stage *vs=a->stages+pair->vs_index,*fs=a->stages+pair->fs_index;
  nn_pipeline_program *p=(void*)1,*q=(void*)1;unsigned char bad[32];memcpy(bad,vs->specialization,32);bad[0]^=1;
  CHECK(nn_pipeline_program_create(d,a,vs,fs,bad,fs->specialization,&p,&q)==ENOTSUP&&p==NULL&&q==NULL&&d->bos==0);
  CHECK(!nn_pipeline_program_create(d,a,vs,fs,vs->specialization,fs->specialization,&p,&q)&&p&&q==NULL);
  struct nn_pipeline_stage_view stages[2];CHECK(!nn_pipeline_program_stage(p,0,stages));CHECK(!nn_pipeline_program_stage(p,1,stages+1));
  for(unsigned stage=0;stage<2;stage++) {
   const struct nn_offline_graphics_stage *s=stage?fs:vs;const unsigned char *code=stage?pair->fs_code:pair->vs_code;size_t bytes=stage?pair->fs_bytes:pair->vs_bytes;
   CHECK(stages[stage].source==s&&stages[stage].pair==pair&&stages[stage].code_bytes==bytes);CHECK(!memcmp(nn_bo_map(stages[stage].code),code,bytes));
   for(size_t j=bytes;j<nn_bo_size(stages[stage].code);j++)CHECK(!((unsigned char *)nn_bo_map(stages[stage].code))[j]);
   CHECK(stages[stage].c1_bytes==s->source_bytes-s->code_bytes);
   if(stages[stage].c1_bytes){CHECK(stages[stage].c1&&stages[stage].c1_native_binding<16);CHECK(!memcmp(nn_bo_map(stages[stage].c1),s->source+s->code_bytes,stages[stage].c1_bytes));}
   else CHECK(!stages[stage].c1&&stages[stage].c1_native_binding==UINT32_MAX);
  }
  nn_bo *bos[4]={(void*)1,(void*)1,(void*)1,(void*)1};size_t count=0;CHECK(nn_pipeline_program_resources(p,NULL,0,&count)==ENOSPC&&count==d->bos);CHECK(!nn_pipeline_program_resources(p,bos,4,&count)&&count==d->bos);
  bos[0]->busy++;CHECK(nn_pipeline_program_destroy(p)==EBUSY);CHECK(!nn_pipeline_program_stage(p,0,stages));bos[0]->busy--;CHECK(!nn_pipeline_program_destroy(p));CHECK(!d->bos);
  fail_alloc_at=alloc_calls+2;CHECK(nn_pipeline_program_create(d,a,vs,fs,vs->specialization,fs->specialization,&p,&q)==ENOMEM&&!p&&!q&&!d->bos);fail_alloc_at=0;
  fail_cache_at=cache_calls+1;fail_destroy_at=destroy_calls+1;CHECK(nn_pipeline_program_create(d,a,vs,fs,vs->specialization,fs->specialization,&p,&q)==EIO&&!p&&q&&d->bos==1);CHECK(nn_pipeline_program_cleanup_error(q)==EIO);CHECK(nn_pipeline_program_stage(q,0,stages)==EPIPE);CHECK(!nn_pipeline_program_destroy(q)&&!d->bos);fail_cache_at=fail_destroy_at=0;
 }
 CHECK(!nn_device_close(d));printf("HOST ONLY generic program owner checks=%u; GPU execution=false\n",checks);return 0;
}
