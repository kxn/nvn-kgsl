/* Host-only ownership/recording check. The memory backend DOES NOT execute GPU work. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "nvn_native/nvn.h"
#include "nvn_native/graphics.h"
#define __user
#include "../../third_party/kgsl/msm_kgsl.h"
#undef __user

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
struct nn_device { unsigned bos; };
struct nn_bo { nn_device *device; void *map; size_t bytes; unsigned imported, busy; uint32_t flags; };
struct nn_queue { nn_device *device; nn_bo *pending[64]; unsigned count; };

int nn_device_open(nn_device **out) { *out=calloc(1,sizeof(**out)); return *out?0:ENOMEM; }
int nn_device_close(nn_device *d) { assert(!d->bos); free(d); return 0; }
uint32_t nn_device_chip(const nn_device *d) { (void)d; return 0x06050002; }
size_t nn_device_gmem_bytes(const nn_device *d) { (void)d; return 1179648; }
size_t nn_device_page(const nn_device *d) { (void)d; return 4096; }
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **out) { nn_bo *b=calloc(1,sizeof(*b)); if(!b)return ENOMEM; b->bytes=(n+4095)&~(size_t)4095; if(posix_memalign(&b->map,4096,b->bytes)){free(b);return ENOMEM;} memset(b->map,0,b->bytes);b->device=d;d->bos++;*out=b;return 0; }
static int import_flags(nn_device *d,void *p,size_t n,uint32_t flags,nn_bo **out) { *out=NULL;nn_bo *b=calloc(1,sizeof(*b));if(!b)return ENOMEM;b->map=p;b->bytes=n;b->device=d;b->imported=1;b->flags=flags;d->bos++;*out=b;return 0; }
int nn_bo_import(nn_device *d,void *p,size_t n,nn_bo **out) { return import_flags(d,p,n,KGSL_CACHEMODE_WRITEBACK<<KGSL_CACHEMODE_SHIFT,out); }
/* This RAM backend does not implement Android dma-bufs. Specialized host
 * ownership fixtures may override this weak failure boundary explicitly. */
__attribute__((weak)) int nn_bo_import_dmabuf(nn_device *d,int fd,size_t bytes,nn_bo **out)
{ (void)d;(void)fd;(void)bytes;if(out)*out=NULL;return ENOTSUP; }
/* Synthetic coherent import only. Flags describe the mock route, and do not
 * establish CPU/GPU coherence or execute an Android kernel ioctl. */
int nn_bo_import_coherent(nn_device *d,void *p,size_t n,nn_bo **out) { *out=NULL;if(!d||!p||!n||(uintptr_t)p%4096||n%4096)return EINVAL;return import_flags(d,p,n,(uint32_t)(KGSL_CACHEMODE_WRITEBACK<<KGSL_CACHEMODE_SHIFT)|KGSL_MEMFLAGS_IOCOHERENT,out); }
int nn_bo_destroy(nn_bo *b) { if(b->busy)return EBUSY;if(!b->imported)free(b->map);b->device->bos--;free(b);return 0; }
int nn_bo_idle(const nn_bo *b) { return !b?EINVAL:b->busy?EBUSY:0; }
void *nn_bo_map(const nn_bo *b) { return b->map; }
uint64_t nn_bo_address(const nn_bo *b) { return (uintptr_t)b->map; }
size_t nn_bo_size(const nn_bo *b) { return b->bytes; }
uint32_t nn_bo_flags(const nn_bo *b) { return b->flags; }
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int from) { (void)from;return o<=b->bytes&&n<=b->bytes-o?0:EINVAL; }
int nn_queue_create(nn_device *d,nn_queue **out) { *out=calloc(1,sizeof(**out));if(!*out)return ENOMEM;(*out)->device=d;return 0; }
int nn_queue_destroy(nn_queue *q) { if(q->count)return EBUSY;free(q);return 0; }
int nn_queue_finish(nn_queue *q,uint32_t timeout) { (void)timeout;while(q->count)q->pending[--q->count]->busy--;return 0; }
uint32_t nn_queue_timestamp(const nn_queue *q) { (void)q;return 0; }
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp) {(void)q;(void)ib;(void)offset;(void)bytes;(void)refs;(void)count;(void)timestamp;return ENOTSUP;}

/* Legacy RAM fixtures have no kernel timeline. Refuse dependency work instead
 * of inventing GPU completion; richer tests explicitly replace this seam. */
#ifndef NN_HOST_QUEUE_TIMELINE
int nn_queue_submit_waits(nn_queue *q,nn_bo *b,size_t o,size_t n,nn_bo *const *r,size_t c,
                         const struct nn_queue_dependency *d,size_t k,uint32_t *t)
{ (void)q;(void)b;(void)o;(void)n;(void)r;(void)c;(void)d;(void)k;(void)t;return ENOTSUP; }
int nn_queue_timestamp_status(nn_queue *q,uint32_t timestamp,int *completed)
{ if(!q||!completed)return EINVAL;if(timestamp||q->count)return ENOTSUP;*completed=1;return 0; }
int nn_queue_wait_timestamp(nn_queue *q,uint32_t timestamp,uint32_t timeout)
{ (void)timeout;return !q?EINVAL:timestamp||q->count?ENOTSUP:0; }
int nn_queue_faults(nn_queue *q,int *faults)
{ (void)q;(void)faults;return ENOTSUP; }
int nn_queue_flush(nn_queue *q) { return q?0:EINVAL; }
#endif

/* CPU RAM fixtures only; production Android always links the real platform.
 * An explicit opt-in is used solely for the nonexecuted NDK RAM-fixture link. */
#if !defined(__ANDROID__) || defined(NN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND)
#include "window_session_backend.inc"
#endif
