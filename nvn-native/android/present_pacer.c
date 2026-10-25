/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "present_pacer.h"
#include <android/choreographer.h>
#include <android/looper.h>
#include <android/surface_control.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
struct timeline_api {
    void (*post)(AChoreographer *,AChoreographer_vsyncCallback,void *);
    size_t (*length)(const AChoreographerFrameCallbackData *);
    size_t (*preferred)(const AChoreographerFrameCallbackData *);
    AVsyncId (*id)(const AChoreographerFrameCallbackData *,size_t);
    int64_t (*expected)(const AChoreographerFrameCallbackData *,size_t);
    void (*set_timeline)(ASurfaceTransaction *,AVsyncId);
};
static struct timeline_api api;
static pthread_once_t once=PTHREAD_ONCE_INIT;
#if !defined(__ANDROID__) || __ANDROID_API__ >= 33
#define CHECK_TYPE(member,function) _Static_assert(__builtin_types_compatible_p( \
    __typeof__(api.member),__typeof__(&function)),"Android typed API: " #function)
CHECK_TYPE(post,AChoreographer_postVsyncCallback);
CHECK_TYPE(length,AChoreographerFrameCallbackData_getFrameTimelinesLength);
CHECK_TYPE(preferred,AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex);
CHECK_TYPE(id,AChoreographerFrameCallbackData_getFrameTimelineVsyncId);
CHECK_TYPE(expected,AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos);
CHECK_TYPE(set_timeline,ASurfaceTransaction_setFrameTimeline);
#undef CHECK_TYPE
#endif

static void load_api(void)
{
#define LOAD(member,name) do {void *symbol=dlsym(RTLD_DEFAULT,name); \
    _Static_assert(sizeof(api.member)==sizeof(symbol),"Android function-pointer ABI"); \
    memcpy(&api.member,&symbol,sizeof(symbol));} while(0)
    LOAD(post,"AChoreographer_postVsyncCallback");
    LOAD(length,"AChoreographerFrameCallbackData_getFrameTimelinesLength");
    LOAD(preferred,"AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex");
    LOAD(id,"AChoreographerFrameCallbackData_getFrameTimelineVsyncId");
    LOAD(expected,"AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos");
    LOAD(set_timeline,"ASurfaceTransaction_setFrameTimeline");
#undef LOAD
}
int nn_present_pacer_max_interval(void)
{
    (void)pthread_once(&once,load_api);
    return api.post&&api.length&&api.preferred&&api.id&&api.expected&&api.set_timeline?5:1;
}
/* Used by present.c after wait; dynamically resolved with the same capability. */
void nn_present_pacer_set_timeline(ASurfaceTransaction *transaction,int64_t id)
{if(api.set_timeline)api.set_timeline(transaction,id);}
struct nn_present_pacer {
    ALooper *looper;
    AChoreographer *choreographer;
    pthread_t owner;
    _Atomic int closing;
    int pending,received,error;
    int64_t expected,id,last_expected;
};
static void frame(const AChoreographerFrameCallbackData *data,void *context)
{
    struct nn_present_pacer *p=context;
    /* Choreographer invokes on this owning Looper thread, never UI or binder.
     * pending is cleared only here; a timeout cannot free this context. */
    size_t n=api.length(data),i=api.preferred(data);
    p->error=(!n||i>=n)?EPROTO:0;
    if(!p->error){p->expected=api.expected(data,i);p->id=api.id(data,i);
        if(p->expected<=0||p->id<0)p->error=EPROTO;}
    p->pending=0;p->received=1;
}
static int deadline(struct timespec *d)
{
    if(clock_gettime(CLOCK_MONOTONIC,d))return errno;
    d->tv_sec+=1;d->tv_nsec+=500000000L;
    if(d->tv_nsec>=1000000000L){d->tv_sec++;d->tv_nsec-=1000000000L;}return 0;
}
static int remaining(const struct timespec *d)
{
    struct timespec n;if(clock_gettime(CLOCK_MONOTONIC,&n))return -1;
    int64_t ns=(int64_t)(d->tv_sec-n.tv_sec)*1000000000+(d->tv_nsec-n.tv_nsec);
    if(ns<=0)return 0;
    return (int)((ns+999999)/1000000);
}
int nn_present_pacer_create(struct nn_present_pacer **out)
{
    if(!out)return EINVAL;
    *out=NULL;
    if(nn_present_pacer_max_interval()!=5)return ENOTSUP;
    struct nn_present_pacer *p=calloc(1,sizeof(*p));if(!p)return ENOMEM;
    p->looper=ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if(!p->looper){free(p);return ENOMEM;}
    ALooper_acquire(p->looper);p->choreographer=AChoreographer_getInstance();
    if(!p->choreographer){ALooper_release(p->looper);free(p);return ENOTSUP;}
    p->owner=pthread_self();*out=p;return 0;
}
int nn_present_pacer_wait(struct nn_present_pacer *p,int interval,int64_t *expected,int64_t *id)
{
    if(!p||!expected||!id||interval<0||interval>5)return EINVAL;
    *expected=0;*id=-1;
    if(!pthread_equal(p->owner,pthread_self()))return EPERM;
    if(atomic_load_explicit(&p->closing,memory_order_acquire))return ECANCELED;
    if(!interval)return 0;
    if(p->pending)return EBUSY;
    struct timespec d;int e=deadline(&d);if(e)return e;
    unsigned count=0;int64_t seen=p->last_expected;
    while(count<(unsigned)interval){
        if(!p->pending){p->received=0;api.post(p->choreographer,frame,p);p->pending=1;}
        while(!p->received){
            if(atomic_load_explicit(&p->closing,memory_order_acquire))return ECANCELED;
            int ms=remaining(&d);if(ms<0)return errno;if(!ms)return ETIMEDOUT;
            int r=ALooper_pollOnce(ms,NULL,NULL,NULL);
            if(r==ALOOPER_POLL_ERROR)return EIO;
        }
        if(atomic_load_explicit(&p->closing,memory_order_acquire))return ECANCELED;
        if(p->error)return p->error;
        /* Count actual distinct future OS vsyncs, never multiply a presumed
         * refresh period or extrapolate a 60Hz clock. Missed OS callbacks can
         * slow presentation; they cannot shorten the interval. */
        if(p->expected>seen){seen=p->expected;count++;}
    }
    p->last_expected=seen;*expected=seen;*id=p->id;return 0;
}
void nn_present_pacer_wake(struct nn_present_pacer *p)
{
    if(p){atomic_store_explicit(&p->closing,1,memory_order_release);ALooper_wake(p->looper);}
}
int nn_present_pacer_destroy(struct nn_present_pacer *p)
{
    if(!p)return 0;
    if(!pthread_equal(p->owner,pthread_self()))return EPERM;
    nn_present_pacer_wake(p);
    struct timespec d;int e=deadline(&d);if(e)return e;
    /* Public Choreographer has no cancel-vsync operation. Drain the one shot
     * on the same thread; if the display stops delivering it, retain p/Looper
     * and let the session quarantine SC/AHB ownership. No dangling callback. */
    while(p->pending){int ms=remaining(&d);if(ms<0)return errno;if(!ms)return EBUSY;
        if(ALooper_pollOnce(ms,NULL,NULL,NULL)==ALOOPER_POLL_ERROR)return EIO;}
    ALooper_release(p->looper);free(p);return 0;
}
