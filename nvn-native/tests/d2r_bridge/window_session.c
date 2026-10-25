/* SPDX-License-Identifier: MIT */
/* Host ownership/concurrency tests ONLY. The fake compositor executes no GPU
 * work and consumes no pixels. Its return boundary models the already-existing
 * present helper contract; it does not verify Android compositor behavior. */
#include "../../android/window_session.c"
#include <stdio.h>
#include <assert.h>
#include <stdatomic.h>
static _Atomic unsigned checks;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);abort();}}while(0)
struct fake_ref { unsigned refs; };
struct fake_present {struct nn_window_platform_stats stats;};
static pthread_mutex_t fm=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fc;
static unsigned creates,shows,detaches,destroys,wakes;
static int intervals[32],cancel_on_wake;
static int hold_create,hold_show,hold_detach,create_error,show_error,detach_error,stats_error;
static void acquire_ref(void *p){pthread_mutex_lock(&fm);((struct fake_ref *)p)->refs++;pthread_mutex_unlock(&fm);}
static void release_ref(void *p){pthread_mutex_lock(&fm);T(((struct fake_ref *)p)->refs>1);((struct fake_ref *)p)->refs--;pthread_mutex_unlock(&fm);}
static int validate(const struct nn_window_slot *s){return s->buffer?0:EINVAL;}
static int fake_create(void *w,void **out)
{
    (void)w;*out=NULL;pthread_mutex_lock(&fm);creates++;pthread_cond_broadcast(&fc);
    while(hold_create)pthread_cond_wait(&fc,&fm);
    int e=create_error;pthread_mutex_unlock(&fm);
    if(!e){*out=calloc(1,sizeof(struct fake_present));if(!*out)return ENOMEM;}return e;
}
static int fake_show(void *p,void *b)
{
    (void)b;pthread_mutex_lock(&fm);shows++;pthread_cond_broadcast(&fc);
    while(hold_show)pthread_cond_wait(&fc,&fm);
    int e=show_error;
    if(!e){struct fake_present *f=p;f->stats.show_completed++;f->stats.completion_callbacks++;
        f->stats.present_fences_waited++;f->stats.release_fences_waited++;}
    pthread_mutex_unlock(&fm);return e;
}
static int fake_detach(void *p)
{
    pthread_mutex_lock(&fm);detaches++;pthread_cond_broadcast(&fc);
    while(hold_detach)pthread_cond_wait(&fc,&fm);
    int e=detach_error;
    if(!e){struct fake_present *f=p;f->stats.detach_completed++;f->stats.completion_callbacks++;f->stats.release_fences_waited++;}
    pthread_mutex_unlock(&fm);return e;
}
static int fake_destroy(void *p){pthread_mutex_lock(&fm);destroys++;pthread_mutex_unlock(&fm);free(p);return 0;}
static int fake_stats(void *p,struct nn_window_platform_stats *out)
{
    pthread_mutex_lock(&fm);*out=((struct fake_present *)p)->stats;int e=stats_error;pthread_mutex_unlock(&fm);return e;
}
static int fake_max_interval(void){return 5;}
static int fake_show_interval(void *p,void *b,int interval)
{
    pthread_mutex_lock(&fm);intervals[shows]=interval;
    if(cancel_on_wake){shows++;pthread_cond_broadcast(&fc);while(!wakes)pthread_cond_wait(&fc,&fm);
        pthread_mutex_unlock(&fm);return ECANCELED;}
    pthread_mutex_unlock(&fm);return fake_show(p,b);
}
static void fake_wake(void *p)
{ (void)p;pthread_mutex_lock(&fm);wakes++;pthread_cond_broadcast(&fc);pthread_mutex_unlock(&fm); }
const struct nn_window_platform *nn_window_platform_get(void)
{
    static const struct nn_window_platform ops={acquire_ref,release_ref,validate,acquire_ref,release_ref,
        fake_create,fake_show,fake_detach,fake_destroy,fake_stats,fake_max_interval,fake_show_interval,fake_wake};return &ops;
}
static void wait_fake(unsigned *counter,unsigned value)
{
    struct timespec d;T(!deadline(1200,&d));pthread_mutex_lock(&fm);
    while(*counter<value){int e=pthread_cond_timedwait(&fc,&fm,&d);T(!e);}
    pthread_mutex_unlock(&fm);
}
static void wait_state(nn_window_session *s,unsigned kind,uint64_t value)
{
    struct timespec d;T(!deadline(1200,&d));pthread_mutex_lock(&s->mutex);
    while((kind==0?s->stats.shown:kind==1?s->stats.worker_done:(uint64_t)s->gpu_retired)<value){
        int e=pthread_cond_timedwait(&s->changed,&s->mutex,&d);T(!e);
    }
    pthread_mutex_unlock(&s->mutex);
}
static void open_gates(void)
{
    pthread_mutex_lock(&fm);hold_create=hold_show=hold_detach=0;pthread_cond_broadcast(&fc);pthread_mutex_unlock(&fm);
}
static struct fake_ref window={1},buffers[3]={{1},{1},{1}};
static int token,unknown;
static uint64_t generation;
static struct nn_window_slot slots[3];
static nn_window_session *setup(unsigned n)
{
    T(!nn_window_registry_register(&token));T(!nn_window_registry_attach(&token,&window,&generation));
    for(unsigned i=0;i<3;i++)slots[i]=(struct nn_window_slot){.buffer=buffers+i,.owner=buffers+i,.payload_bytes=16384,
        .backing_inode=100+i,.backing_dev_major=1,.backing_dev_minor=2,.width=64,.height=64,.pitch=256,.format=1,.usage=819};
    nn_window_session *s=NULL;T(!nn_window_session_create(&token,slots,n,&s)&&s);return s;
}
static void clean(nn_window_session *s)
{
    T(!nn_window_session_retire(s,1000));struct nn_window_session_stats stats;
    T(!nn_window_session_get_stats(s,&stats));T(stats.retired&&stats.worker_done&&stats.compositor_retired&&!stats.resources_retained);
    for(unsigned i=0;i<stats.slots;i++)T(stats.state[i]==NN_WINDOW_FREE);
    T(!nn_window_registry_close(&token,generation,1000));T(!nn_window_session_destroy(s));
    T(window.refs==1);for(unsigned i=0;i<3;i++)T(buffers[i].refs==1);
}
static void normal(unsigned n)
{
    nn_window_session *s=setup(n);struct nn_window_ticket t={0},old={0};
    T(nn_window_session_destroy(s)==EBUSY);
    for(unsigned round=0;round<2*n+1;round++){
        T(!nn_window_session_acquire(s,1000,&t));T(!nn_window_session_check_acquired(s,&t));
        struct nn_window_ticket bad=t;bad.generation++;T(nn_window_session_check_acquired(s,&bad)==ESTALE);
        bad=t;bad.serial++;T(nn_window_session_check_acquired(s,&bad)==ESTALE);
        if(round>n)T(nn_window_session_check_acquired(s,&old)!=0);
        T(!nn_window_session_enqueue_ready(s,&t));T(nn_window_session_enqueue_ready(s,&t)==EINVAL);
        wait_state(s,0,round+1);old=t;
    }
    struct nn_window_session_stats stats;T(!nn_window_session_get_stats(s,&stats));
    T(stats.shown==2*n+1 && stats.releases==2*n && stats.current_index>=0);
    uint64_t old_generation=generation;clean(s);
    T(!nn_window_registry_attach(&token,&window,&generation)&&generation>old_generation);
    T(nn_window_registry_close(&token,old_generation,0)==ESTALE);
    T(!nn_window_registry_close(&token,generation,1000));T(window.refs==1);
}
static void ticket_recreate(void)
{
    nn_window_session *s=setup(2);struct nn_window_ticket old,fresh;
    T(!nn_window_session_acquire(s,1000,&old));T(!nn_window_session_retire(s,1000));T(!nn_window_session_destroy(s));
    T(!nn_window_session_create(&token,slots,2,&s));T(!nn_window_session_acquire(s,1000,&fresh));
    T(old.generation==fresh.generation&&old.index==fresh.index&&old.serial!=fresh.serial);
    T(nn_window_session_check_acquired(s,&old)==ESTALE);clean(s);
}
static void alias_cases(void)
{
    nn_window_session *s=setup(2),*other=(void *)1;T(nn_window_session_create(&token,slots,2,&other)==EBUSY&&!other);
    struct nn_window_slot bad[3];memcpy(bad,slots,sizeof(bad));bad[1].buffer=bad[0].buffer;
    T(nn_window_session_create(&token,bad,2,&other)==EEXIST&&!other);
    memcpy(bad,slots,sizeof(bad));bad[1].owner=bad[0].owner;T(nn_window_session_create(&token,bad,2,&other)==EEXIST&&!other);
    memcpy(bad,slots,sizeof(bad));bad[1].backing_inode=bad[0].backing_inode;bad[1].pixel_offset=64;
    T(nn_window_session_create(&token,bad,2,&other)==EEXIST&&!other);
    memcpy(bad,slots,sizeof(bad));bad[1].backing_inode=0;T(nn_window_session_create(&token,bad,2,&other)==ENOTSUP&&!other);
    memcpy(bad,slots,sizeof(bad));bad[1].pixel_offset=UINT64_MAX-63;T(nn_window_session_create(&token,bad,2,&other)==ENOTSUP&&!other);
    T(nn_window_session_create(&unknown,slots,2,&other)==EAGAIN&&!other);
    struct nn_window_ticket tickets[3];T(!nn_window_session_acquire(s,1000,tickets));T(!nn_window_session_acquire(s,1000,tickets+1));
    T(nn_window_session_acquire(s,1000,tickets+2)==EBUSY);
    clean(s);
}
struct thread_call {nn_window_session *s;int result;struct nn_window_ticket ticket;};
static void *acquire_thread(void *p){struct thread_call *c=p;c->result=nn_window_session_acquire(c->s,1200,&c->ticket);return NULL;}
static void *close_thread(void *p){struct thread_call *c=p;c->result=nn_window_registry_close(&token,generation,1000);return NULL;}
static void *retire_thread(void *p){struct thread_call *c=p;c->result=nn_window_session_retire(c->s,1000);return NULL;}
static nn_window_session *blocked_replacement(struct nn_window_ticket *first)
{
    nn_window_session *s=setup(2);struct nn_window_ticket next;
    T(!nn_window_session_acquire(s,1000,first));T(!nn_window_session_enqueue_ready(s,first));wait_state(s,0,1);
    T(!nn_window_session_acquire(s,1000,&next));pthread_mutex_lock(&fm);hold_show=1;pthread_mutex_unlock(&fm);
    T(!nn_window_session_enqueue_ready(s,&next));wait_fake(&shows,2);
    struct nn_window_session_stats st;T(!nn_window_session_get_stats(s,&st));T(st.state[first->index]==NN_WINDOW_CURRENT);
    T(nn_window_session_acquire(s,0,&next)==EAGAIN);return s;
}
static void release_boundary(void)
{
    struct nn_window_ticket old;nn_window_session *s=blocked_replacement(&old);
    struct thread_call call={.s=s};pthread_t waiter;T(!pthread_create(&waiter,NULL,acquire_thread,&call));
    open_gates();T(!pthread_join(waiter,NULL));T(!call.result&&call.ticket.index==old.index&&call.ticket.serial!=old.serial);
    T(nn_window_session_check_acquired(s,&old)==ESTALE);clean(s);
}
static void close_wake(void)
{
    struct nn_window_ticket old;nn_window_session *s=blocked_replacement(&old);
    struct thread_call call={.s=s};pthread_t waiter;T(!pthread_create(&waiter,NULL,acquire_thread,&call));
    T(!nn_window_registry_close(&token,generation,0));T(!pthread_join(waiter,NULL));T(call.result==ECANCELED);
    open_gates();clean(s);
}
static void concurrent_close(void)
{
    struct nn_window_ticket old;nn_window_session *s=blocked_replacement(&old);
    struct thread_call ui={.s=s},owner={.s=s};pthread_t a,b;
    T(!pthread_create(&a,NULL,close_thread,&ui));T(!pthread_create(&b,NULL,retire_thread,&owner));
    wait_state(s,2,1);open_gates();T(!pthread_join(b,NULL));T(!owner.result);
    int e=nn_window_session_destroy(s);T(!e||e==EBUSY);
    T(!pthread_join(a,NULL));T(!ui.result);if(e)T(!nn_window_session_destroy(s));
    T(window.refs==1);for(unsigned i=0;i<3;i++)T(buffers[i].refs==1);
}
static void close_create(void)
{
    hold_create=1;nn_window_session *s=setup(2);wait_fake(&creates,1);
    T(!nn_window_registry_close(&token,generation,0));open_gates();clean(s);T(!shows&&detaches==1&&destroys==1);
}
static void failure(const char *name)
{
    if(!strcmp(name,"create-fail"))create_error=ENOTSUP;
    else if(!strcmp(name,"show-fail"))show_error=EIO;
    else if(!strcmp(name,"detach-fail"))detach_error=EIO;
    else if(!strcmp(name,"stats-fail"))stats_error=EIO;
    nn_window_session *s=setup(2);struct nn_window_ticket ticket;
    if(create_error){T(nn_window_session_acquire(s,1000,&ticket)==ENOTSUP);clean(s);return;}
    T(!nn_window_session_acquire(s,1000,&ticket));T(!nn_window_session_enqueue_ready(s,&ticket));
    if(detach_error){wait_state(s,0,1);T(nn_window_session_retire(s,1000)==EIO);}
    else wait_state(s,1,1);
    struct nn_window_session_stats st;T(!nn_window_session_get_stats(s,&st));
    T(st.resources_retained&&!st.compositor_retired&&st.first_error==EIO);T(nn_window_session_destroy(s)==EBUSY);
    T(nn_window_session_retire(s,1000)==EIO);T(!nn_window_registry_close(&token,generation,0));
    for(unsigned i=0;i<2;i++)T(buffers[i].refs==2);
}
static void timeout_case(void)
{
    struct nn_window_ticket old;nn_window_session *s=blocked_replacement(&old);
    T(nn_window_registry_close(&token,generation,10)==ETIMEDOUT);
    struct nn_window_session_stats st;T(!nn_window_session_get_stats(s,&st));T(st.resources_retained&&!st.worker_done);
    open_gates();wait_state(s,1,1);T(!detaches&&!destroys); /* no additional transaction after timeout */
    T(nn_window_session_acquire(s,1000,&old)==ETIMEDOUT);T(nn_window_session_destroy(s)==EBUSY);
    for(unsigned i=0;i<2;i++)T(buffers[i].refs==2);
}
static void interval_init(void)
{
    for(int interval=0;interval<=5;interval++){
        /* Setup closes registry after each completed owner, then reattaches. */
        T(!nn_window_registry_register(&token));T(!nn_window_registry_attach(&token,&window,&generation));
        for(unsigned i=0;i<2;i++)slots[i]=(struct nn_window_slot){.buffer=buffers+i,.owner=buffers+i,.payload_bytes=16384,
            .backing_inode=100+i,.backing_dev_major=1,.backing_dev_minor=2,.width=64,.height=64,.pitch=256,.format=1,.usage=819};
        nn_window_session *s=NULL;T(!nn_window_session_create_interval(&token,slots,2,interval,&s)&&s);
        struct nn_window_ticket t;T(!nn_window_session_acquire(s,1000,&t));
        T(!nn_window_session_enqueue_ready(s,&t));wait_state(s,0,1);
        T(intervals[interval]==(interval?interval:1));
        struct nn_window_session_stats stats;T(!nn_window_session_get_stats(s,&stats));
        int effective=interval?interval:1;T(stats.last_shown_interval==effective);
        for(int k=0;k<6;k++)T(stats.shown_intervals[k]==(uint64_t)(k==effective));
        clean(s);
    }
}
static void interval_case(const char *name)
{
    T(nn_window_session_interval_supported(-1)==EINVAL);
    T(nn_window_session_interval_supported(6)==EINVAL);
    for(int i=0;i<=5;i++)T(!nn_window_session_interval_supported(i));
    nn_window_session *s=setup(3);struct nn_window_ticket t[3];
    for(unsigned i=0;i<3;i++)T(!nn_window_session_acquire(s,1000,t+i));
    if(!strcmp(name,"interval-close")){
        cancel_on_wake=1;T(!nn_window_session_set_present_interval(s,5));
        T(!nn_window_session_enqueue_ready(s,t));wait_fake(&shows,1);
        T(!nn_window_registry_close(&token,generation,1000));
        T(wakes&&detaches==1&&destroys==1);clean(s);return;
    }
    pthread_mutex_lock(&fm);hold_show=1;pthread_mutex_unlock(&fm);
    T(!nn_window_session_enqueue_ready(s,t));wait_fake(&shows,1);
    T(!nn_window_session_set_present_interval(s,2));T(!nn_window_session_enqueue_ready(s,t+1));
    int last=!strcmp(name,"interval-mailbox")?0:5;
    T(!nn_window_session_set_present_interval(s,last));
    T(!nn_window_session_enqueue_ready(s,t+2));
    struct nn_window_session_stats st;T(!nn_window_session_get_stats(s,&st));
    T(st.state[t[0].index]==NN_WINDOW_SHOWING);
    if(!last){T(st.cancelled==1&&st.queued==1&&st.state[t[1].index]==NN_WINDOW_FREE);}
    else T(st.cancelled==0&&st.queued==2);
    T(nn_window_session_set_present_interval(s,6)==EINVAL);
    open_gates();wait_state(s,0,last?3:2);
    T(intervals[0]==1&&intervals[last?2:1]==last);
    if(last)T(intervals[1]==2);
    T(!nn_window_session_get_stats(s,&st));T(st.current_index==(int)t[2].index);T(st.last_shown_interval==last);
    for(int k=0;k<6;k++)T(st.shown_intervals[k]==(uint64_t)(k==1||k==last||(last&&k==2)));
    clean(s);
}
int main(int argc,char **argv)
{
    T(argc==2);pthread_condattr_t attr;T(!pthread_condattr_init(&attr));T(!pthread_condattr_setclock(&attr,CLOCK_MONOTONIC));
    T(!pthread_cond_init(&fc,&attr));T(!pthread_condattr_destroy(&attr));
    if(!strcmp(argv[1],"interval-init"))interval_init();
    else if(!strncmp(argv[1],"interval-",9))interval_case(argv[1]);
    else if(!strcmp(argv[1],"normal2"))normal(2);else if(!strcmp(argv[1],"normal3"))normal(3);
    else if(!strcmp(argv[1],"ticket-recreate"))ticket_recreate();
    else if(!strcmp(argv[1],"alias"))alias_cases();else if(!strcmp(argv[1],"release"))release_boundary();
    else if(!strcmp(argv[1],"close-wake"))close_wake();else if(!strcmp(argv[1],"concurrent"))concurrent_close();
    else if(!strcmp(argv[1],"close-create"))close_create();else if(!strcmp(argv[1],"timeout"))timeout_case();
    else failure(argv[1]);
    printf("{\"scope\":\"host fake compositor ownership only\",\"case\":\"%s\",\"checks\":%u,\"gpu_executes\":0}\n",argv[1],checks);return 0;
}
