/* HOST OS-callback boundary only. Synthetic vsync sequence; no display/GPU. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <time.h>
static int fake_clock_gettime(clockid_t,struct timespec *);
#define clock_gettime fake_clock_gettime
#define dlsym fake_dlsym
#ifndef NN_PRESENT_PACER_SOURCE
#define NN_PRESENT_PACER_SOURCE "../../android/present_pacer.c"
#endif
#include NN_PRESENT_PACER_SOURCE
#undef clock_gettime
#undef dlsym
static _Atomic unsigned checks;
static unsigned posts,polls,refs,wakes;
#define T(x) do{checks++;assert(x);}while(0)
struct ALooper {int x;};struct AChoreographer {int x;};struct AChoreographerFrameCallbackData {int x;};
static struct ALooper looper;static struct AChoreographer choreographer;
static AChoreographer_vsyncCallback callback;static void *context;
static struct nn_present_pacer *closing_pacer;
static int unavailable,timeout_mode,cancel_mode,duplicate,bad_timeline,create_fail;
static int64_t fake_ns=1000000000,expected_time=100,chosen_id;
static void post(AChoreographer *c,AChoreographer_vsyncCallback f,void *data)
{T(c==&choreographer&&!callback);posts++;callback=f;context=data;}
static size_t length(const AChoreographerFrameCallbackData *d){(void)d;return bad_timeline==1?0:3;}
static size_t preferred(const AChoreographerFrameCallbackData *d){(void)d;return bad_timeline==2?4:1;}
static AVsyncId id(const AChoreographerFrameCallbackData *d,size_t i){(void)d;T(i==1);return bad_timeline==4?-1:expected_time+900;}
static int64_t expected(const AChoreographerFrameCallbackData *d,size_t i){(void)d;T(i==1);return bad_timeline==3?0:expected_time;}
static void timeline(ASurfaceTransaction *t,AVsyncId i){T(t==(void *)0x1234);chosen_id=i;}
void *fake_dlsym(void *handle,const char *name)
{
    (void)handle;if(unavailable)return NULL;void *out=NULL;
#define MATCH(n,f) if(!strcmp(name,n)){__typeof__(&f) value=f;memcpy(&out,&value,sizeof(out));return out;}
    MATCH("AChoreographer_postVsyncCallback",post)
    MATCH("AChoreographerFrameCallbackData_getFrameTimelinesLength",length)
    MATCH("AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex",preferred)
    MATCH("AChoreographerFrameCallbackData_getFrameTimelineVsyncId",id)
    MATCH("AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos",expected)
    MATCH("ASurfaceTransaction_setFrameTimeline",timeline)
#undef MATCH
    T(0);return NULL;
}
ALooper *ALooper_prepare(int flags){T(flags==1);return create_fail==1?NULL:&looper;}
void ALooper_acquire(ALooper *l){T(l==&looper);refs++;}
void ALooper_release(ALooper *l){T(l==&looper&&refs);refs--;}
void ALooper_wake(ALooper *l){T(l==&looper);wakes++;}
AChoreographer *AChoreographer_getInstance(void){return create_fail==2?NULL:&choreographer;}
static int fake_clock_gettime(clockid_t c,struct timespec *out)
{T(c==CLOCK_MONOTONIC);fake_ns+=100000000;out->tv_sec=fake_ns/1000000000;out->tv_nsec=fake_ns%1000000000;return 0;}
int ALooper_pollOnce(int ms,int *f,int *e,void **d)
{
    (void)f;(void)e;(void)d;T(ms>0&&ms<=1500);polls++;
    if(cancel_mode){cancel_mode=0;nn_present_pacer_wake(closing_pacer);return ALOOPER_POLL_WAKE;}
    if(timeout_mode)return ALOOPER_POLL_TIMEOUT;
    T(callback);AChoreographer_vsyncCallback fptr=callback;void *data=context;callback=NULL;context=NULL;
    if(!duplicate)expected_time+=17+(polls%3)*9;else duplicate=0;
    struct AChoreographerFrameCallbackData input={0};fptr(&input,data);return ALOOPER_POLL_CALLBACK;
}
static void *wrong_owner(void *arg)
{int64_t t,i;T(nn_present_pacer_wait(arg,1,&t,&i)==EPERM);return NULL;}
int main(int argc,char **argv)
{
    T(argc==2);
    T(!strcmp(argv[1],"normal")||!strcmp(argv[1],"close")||!strcmp(argv[1],"timeout")||
      !strcmp(argv[1],"unavailable")||!strcmp(argv[1],"protocol")||!strcmp(argv[1],"create-looper")||!strcmp(argv[1],"create-choreographer"));
    unavailable=!strcmp(argv[1],"unavailable");
    T(nn_present_pacer_max_interval()==(unavailable?1:5));
    struct nn_present_pacer *p=NULL;
    if(unavailable){T(nn_present_pacer_create(&p)==ENOTSUP&&!p);goto done;}
    if(!strncmp(argv[1],"create-",7)){
        create_fail=!strcmp(argv[1],"create-looper")?1:2;
        T(nn_present_pacer_create(&p)==(create_fail==1?ENOMEM:ENOTSUP)&&!p&&!refs);goto done;
    }
    T(!nn_present_pacer_create(&p)&&p&&refs==1);
    pthread_t thread;T(!pthread_create(&thread,NULL,wrong_owner,p));T(!pthread_join(thread,NULL));
    int64_t t=-1,i=-1;T(nn_present_pacer_wait(p,6,&t,&i)==EINVAL);
    T(!nn_present_pacer_wait(p,0,&t,&i)&&!t&&i==-1&&!posts);
    for(int interval=1;interval<=5;interval++){
        unsigned old=posts;int64_t previous=p->last_expected;
        T(!nn_present_pacer_wait(p,interval,&t,&i)&&t>previous&&i==t+900);
        T(posts-old==(unsigned)interval);
        nn_present_pacer_set_timeline((void *)0x1234,i);T(chosen_id==i);
    }
    duplicate=1;unsigned old=posts;T(!nn_present_pacer_wait(p,2,&t,&i));T(posts-old==3);
    if(!strcmp(argv[1],"protocol")){
        for(bad_timeline=1;bad_timeline<=4;bad_timeline++){
            T(nn_present_pacer_wait(p,1,&t,&i)==EPROTO&&!p->pending&&!t&&i==-1);
        }
        T(!nn_present_pacer_destroy(p)&&!refs&&!callback);p=NULL;
    }else if(!strcmp(argv[1],"close")){
        closing_pacer=p;cancel_mode=1;T(nn_present_pacer_wait(p,5,&t,&i)==ECANCELED&&p->pending);
        T(!nn_present_pacer_destroy(p)&&!refs&&!callback);p=NULL;
    }else if(!strcmp(argv[1],"timeout")){
        timeout_mode=1;T(nn_present_pacer_wait(p,5,&t,&i)==ETIMEDOUT&&p->pending);
        T(nn_present_pacer_destroy(p)==EBUSY&&refs==1&&p->pending);
        timeout_mode=0;T(!nn_present_pacer_destroy(p)&&!refs&&!callback);p=NULL;
    }else{T(!nn_present_pacer_destroy(p)&&!refs&&!callback);p=NULL;}
    T(wakes);
done:
    printf("{\"case\":\"%s\",\"checks\":%u,\"gpu_executes\":0,\"scope\":\"HOST actual pacer with synthetic OS callbacks\"}\n",argv[1],(unsigned)checks);
}
