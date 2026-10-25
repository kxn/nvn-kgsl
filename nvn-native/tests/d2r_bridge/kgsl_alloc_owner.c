/* SPDX-License-Identifier: MIT. HOST syscall fault injection, never GPU/device.
 * The original kgsl.c executes; wrapped syscalls never reach a real kernel fd. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifndef NN_TEST_KGSL_SOURCE
#define NN_TEST_KGSL_SOURCE "../../src/kgsl.c"
#endif
#include NN_TEST_KGSL_SOURCE
static volatile unsigned checks;
static unsigned cases,total_allocs,total_shared,total_closes,poison_released;
static const int fd=424242;
static struct {unsigned allocs,shared,gpu_free,maps,unmaps,closes,contexts,context_frees;int metadata,map_error,free_failures,close_error;uint32_t chip;} mock;
#define C(x) do{checks++;assert(x);}while(0)
static void reset(void){memset(&mock,0,sizeof(mock));mock.chip=0x06050002;cases++;}
static nn_device *device(void){nn_device *d=calloc(1,sizeof(*d));C(d);d->fd=fd;d->page=4096;d->chip=mock.chip;return d;}
int __wrap_open(const char *path,int flags,...){(void)path;(void)flags;abort();}
int __wrap_close(int value){C(value==fd);mock.closes++;total_closes++;if(mock.close_error){errno=mock.close_error;return -1;}return 0;}
void *__wrap_mmap(void *address,size_t bytes,int prot,int flags,int value,off_t offset)
{
 C(!address&&bytes==8192&&prot==(PROT_READ|PROT_WRITE)&&flags==MAP_SHARED&&value==fd&&offset==(off_t)(100+mock.allocs)*4096);mock.maps++;
 if(mock.map_error){errno=mock.map_error;return MAP_FAILED;}
 void *p=calloc(1,bytes);C(p);return p;
}
int __wrap_munmap(void *p,size_t bytes){C(p&&bytes==8192);mock.unmaps++;free(p);return 0;}
int __wrap_ioctl(int value,unsigned long operation,...)
{
 C(value==fd);va_list args;va_start(args,operation);void *argument=va_arg(args,void*);va_end(args);
 if(operation==IOCTL_KGSL_GPUMEM_ALLOC_ID){
  struct kgsl_gpumem_alloc_id *a=argument;C(a->size==8192);uint32_t mode=mock.chip==0x06050002?KGSL_CACHEMODE_WRITECOMBINE:KGSL_CACHEMODE_UNCACHED;
  C(a->flags==(mode<<KGSL_CACHEMODE_SHIFT)&&a->id==0);mock.allocs++;total_allocs++;a->id=100+mock.allocs;a->gpuaddr=UINT64_C(0x900000)+mock.allocs*UINT64_C(0x10000);a->mmapsize=8192;
  switch(mock.metadata){case 1:a->id=0;break;case 2:a->gpuaddr=0;break;case 3:a->size=4096;break;case 4:a->mmapsize=4096;break;case 5:a->mmapsize=16384;break;case 6:a->flags^=KGSL_CACHEMODE_MASK;break;}
  return 0;
 }
 if(operation==IOCTL_KGSL_SHAREDMEM_FREE){
  const struct kgsl_sharedmem_free *p=argument;C(p->gpuaddr>UINT64_C(0x900000)&&p->gpuaddr<=UINT64_C(0x900000)+mock.allocs*UINT64_C(0x10000));mock.shared++;total_shared++;
  if(mock.free_failures){mock.free_failures--;errno=EIO;return -1;}return 0;
 }
 if(operation==IOCTL_KGSL_GPUOBJ_FREE){mock.gpu_free++;abort();}
 if(operation==IOCTL_KGSL_DRAWCTXT_CREATE){struct kgsl_drawctxt_create *p=argument;p->drawctxt_id=77;mock.contexts++;return 0;}
 if(operation==IOCTL_KGSL_DRAWCTXT_DESTROY){const struct kgsl_drawctxt_destroy *p=argument;C(p->drawctxt_id==77);mock.context_frees++;return 0;}
 fprintf(stderr,"unexpected ioctl %lx\n",operation);abort();
}
static int success(void)
{
 for(unsigned chip=0;chip<2;chip++){
  reset();mock.chip=chip?0x43030b00:0x06050002;nn_device *d=device();nn_bo *bo=(void*)1;
  C(!nn_bo_alloc(d,4097,&bo)&&bo);C(d->bos==1&&!d->failed_imports&&mock.allocs==1&&mock.maps==1&&bo->imported==0);C(nn_bo_size(bo)==8192&&nn_bo_map(bo));
  bo->busy=1;C(nn_bo_destroy(bo)==EBUSY&&!mock.shared&&!mock.unmaps);bo->busy=0;
  mock.free_failures=1;C(nn_bo_destroy(bo)==EIO&&d->bos==1&&mock.shared==1&&mock.unmaps==1&&!bo->map);
  C(!nn_bo_destroy(bo)&&!d->bos&&mock.shared==2&&mock.unmaps==1);C(!nn_device_close(d)&&mock.closes==1&&!mock.gpu_free);
 }
 return 0;
}
static int rejects(void)
{
 for(unsigned variant=3;variant<=7;variant++)for(unsigned failure=0;failure<2;failure++){
  reset();mock.metadata=variant==7?0:(int)variant;mock.map_error=variant==7?ENOMEM:0;mock.free_failures=failure?2:0;
  nn_device *d=device();nn_bo *bo=(void*)1;int expected=variant==7?ENOMEM:ENOTSUP;
  C(nn_bo_alloc(d,4097,&bo)==expected&&!bo);C(mock.allocs==1&&mock.shared==1&&mock.maps==(variant==7)&&!mock.unmaps&&!mock.gpu_free);
  if(failure){C(d->bos==1&&d->failed_imports&&d->failed_imports->imported==0&&!d->failed_imports->map);C(nn_device_close(d)==EIO&&d->bos==1&&mock.shared==2&&!mock.closes);}
  else C(!d->bos&&!d->failed_imports);
  C(!nn_device_close(d)&&mock.closes==1&&mock.allocs==1&&mock.shared==(failure?3u:1u));
 }
 return 0;
}
static int unidentified(void)
{
 for(unsigned variant=1;variant<=2;variant++){
  reset();nn_device *d=device();nn_bo *ordinary=NULL,*bo=(void*)1;C(!nn_bo_alloc(d,4097,&ordinary));mock.metadata=(int)variant;
  C(nn_bo_alloc(d,4097,&bo)==ENOTSUP&&!bo);C(d->bos==2&&d->failed_imports&&!mock.shared&&!mock.gpu_free);
  C(nn_device_close(d)==EBUSY&&!mock.closes&&!mock.shared);C(!nn_bo_destroy(ordinary));C(d->bos==1&&mock.shared==1);
  nn_queue *q=NULL;C(!nn_queue_create(d,&q)&&q);C(nn_device_close(d)==EBUSY&&!mock.closes);C(!nn_queue_destroy(q));C(mock.contexts==1&&mock.context_frees==1);
  C(!nn_device_close(d)&&mock.closes==1&&mock.shared==1&&!mock.gpu_free);
 }
 reset();nn_device *d=device();nn_bo *bo=(void*)1;mock.metadata=3;mock.free_failures=1;
 C(nn_bo_alloc(d,4097,&bo)==ENOTSUP&&!bo&&d->failed_imports&&d->bos==1);mock.metadata=1;
 C(nn_bo_alloc(d,4097,&bo)==ENOTSUP&&!bo&&d->failed_imports->failed_next&&d->bos==2);C(mock.shared==1);
 C(!nn_device_close(d)&&mock.closes==1&&mock.shared==1&&!mock.gpu_free);
 return 0;
}
static int poisoned_close(void)
{
 reset();nn_device *d=device();nn_bo *bo=(void*)1;mock.metadata=2;C(nn_bo_alloc(d,4097,&bo)==ENOTSUP&&!bo);
 mock.close_error=EINTR;C(nn_device_close(d)==EINTR&&d->close_error==EINTR&&d->fd==-1&&d->failed_imports&&d->bos==1&&mock.closes==1);
 C(nn_device_close(d)==EINTR&&mock.closes==1&&d->failed_imports);
 /* Only this fake-kernel fixture manually frees a deliberately poisoned
  * process-lifetime quarantine for leak checking; production must retain it. */
 while(d->failed_imports){nn_bo *b=d->failed_imports;d->failed_imports=b->failed_next;C(!b->map&&b->imported==0);free(b);poison_released++;}free(d);return 0;
}
int main(void)
{
 C(!success());C(!rejects());C(!unidentified());C(!poisoned_close());
 printf("{\"passed\":true,\"checks\":%u,\"cases\":%u,\"alloc_ioctl_calls\":%u,\"sharedmem_free_calls\":%u,\"close_calls\":%u,\"poisoned_fixture_owners_manually_released\":%u,\"gpu_execution\":false,\"scope\":\"HOST actual KGSL allocator successful-ioctl cleanup owner only\"}\n",checks,cases,total_allocs,total_shared,total_closes,poison_released);return 0;
}
