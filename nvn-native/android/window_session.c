/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/display_shape.h"
#include "nvn_native/window_session.h"
#include "window_platform.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

enum { REGISTRY_CAPACITY=8, MAX_WAIT_MS=1500 };
struct registry_entry {
    const void *token; void *window;
    uint64_t generation; int closing;
    nn_window_session *session;
};
static pthread_mutex_t registry_mutex=PTHREAD_MUTEX_INITIALIZER;
static struct registry_entry registry[REGISTRY_CAPACITY];
static uint64_t next_generation=1;
static _Atomic uint64_t next_ticket_serial=1;
struct nn_window_session {
    pthread_mutex_t mutex; pthread_cond_t changed;
    const struct nn_window_platform *platform;
    struct registry_entry *entry;
    struct nn_window_slot slots[3];
    struct nn_window_ticket queue[3];
    int queue_interval[3],interval;
    uint64_t serial[3];
    uint32_t head,count,cursor,ui_calls;
    void *window,*presenter;
    int gpu_retired,deadline_set;
    struct timespec stop_deadline;
    struct nn_window_session_stats stats;
};
static struct registry_entry *lookup(const void *token)
{
    for(unsigned i=0;i<REGISTRY_CAPACITY;i++)if(registry[i].token==token)return registry+i;
    return NULL;
}
static int now(struct timespec *value){return clock_gettime(CLOCK_MONOTONIC,value)?errno:0;}
static int before(const struct timespec *a,const struct timespec *b)
{return a->tv_sec<b->tv_sec || (a->tv_sec==b->tv_sec && a->tv_nsec<b->tv_nsec);}
static int deadline(uint32_t ms,struct timespec *out)
{
    int e=now(out);if(e)return e;
    out->tv_sec+=ms/1000;out->tv_nsec+=(ms%1000)*1000000L;
    if(out->tv_nsec>=1000000000L){out->tv_sec++;out->tv_nsec-=1000000000L;}return 0;
}
/* mutex held; this never cancels or frees a callback context. */
static void poison(nn_window_session *s,int error)
{
    if(!s->stats.first_error)s->stats.first_error=error?error:EIO;
    s->stats.resources_retained=1;s->stats.closing=1;
    for(uint32_t i=0;i<s->stats.slots;i++)s->stats.state[i]=NN_WINDOW_RETAINED;
    pthread_cond_broadcast(&s->changed);
}
static int expired(nn_window_session *s)
{
    struct timespec t;int e=now(&t);
    if(e){poison(s,e);return 1;}
    if(s->deadline_set && !before(&t,&s->stop_deadline)){poison(s,ETIMEDOUT);return 1;}
    return 0;
}
static int request_stop(nn_window_session *s,uint32_t ms)
{
    struct timespec d;int e=deadline(ms?ms:MAX_WAIT_MS,&d);
    s->stats.closing=1;
    if(e){poison(s,e);return e;}
    if(!s->deadline_set || before(&d,&s->stop_deadline)){s->stop_deadline=d;s->deadline_set=1;}
    if(s->presenter && s->platform->wake)s->platform->wake(s->presenter);
    pthread_cond_broadcast(&s->changed);return 0;
}
static void cancel_queued(nn_window_session *s)
{
    while(s->count){struct nn_window_ticket *t=s->queue+s->head;
        if(!s->stats.resources_retained)s->stats.state[t->index]=NN_WINDOW_FREE;
        s->head=(s->head+1)%3;s->count--;s->stats.cancelled++;
    }
    s->stats.queued=0;
}
static void retire_if_safe(nn_window_session *s)
{
    if(s->gpu_retired && s->stats.worker_done && s->stats.compositor_retired && !s->stats.resources_retained){
        for(uint32_t i=0;i<s->stats.slots;i++)s->stats.state[i]=NN_WINDOW_FREE;
        s->stats.current_index=-1;s->stats.retired=1;
    }
}
static int capture_stats(nn_window_session *s)
{
    struct nn_window_platform_stats p={0};int e=s->platform->stats(s->presenter,&p);
    pthread_mutex_lock(&s->mutex);
    if(!e){s->stats.show_completed=p.show_completed;s->stats.detach_completed=p.detach_completed;
        s->stats.completion_callbacks=p.completion_callbacks;s->stats.present_fences_waited=p.present_fences_waited;
        s->stats.release_fences_waited=p.release_fences_waited;}
    else poison(s,e);
    pthread_mutex_unlock(&s->mutex);return e;
}
static void *worker(void *argument)
{
    nn_window_session *s=argument;int e=0;
    pthread_mutex_lock(&s->mutex);int stopping=s->stats.closing;
    pthread_mutex_unlock(&s->mutex);
    void *presenter=NULL;
    if(!stopping)e=s->platform->create(s->window,&presenter);
    pthread_mutex_lock(&s->mutex);
    s->presenter=presenter; /* publish only under mutex: UI wake never races free */
    if(s->stats.closing && presenter && s->platform->wake)s->platform->wake(presenter);
    if(e){if(!s->stats.first_error)s->stats.first_error=e;s->stats.closing=1;
        if(s->presenter)poison(s,e);}
    else if(!stopping)s->stats.ready=1;
    pthread_cond_broadcast(&s->changed);
    for(;;){
        while(!s->stats.closing && !s->count)pthread_cond_wait(&s->changed,&s->mutex);
        if(s->stats.closing){cancel_queued(s);break;}
        int interval=s->queue_interval[s->head];
        struct nn_window_ticket t=s->queue[s->head];s->head=(s->head+1)%3;s->count--;s->stats.queued=s->count;
        s->stats.state[t.index]=NN_WINDOW_SHOWING;
        pthread_mutex_unlock(&s->mutex);
        e=s->platform->show_interval?s->platform->show_interval(presenter,s->slots[t.index].buffer,interval):
            s->platform->show(presenter,s->slots[t.index].buffer);
        int se=capture_stats(s);if(!e)e=se;
        pthread_mutex_lock(&s->mutex);
        /* Only pacing can return ECANCELED, before applying an SC transaction.
         * Closing then retires SC normally; no GPU/SC ownership is invented. */
        if(e==ECANCELED && s->stats.closing && !s->stats.resources_retained){
            s->stats.state[t.index]=NN_WINDOW_FREE;s->stats.cancelled++;cancel_queued(s);break;
        }
        if(e || s->stats.resources_retained){poison(s,e);cancel_queued(s);break;}
        int old=s->stats.current_index;
        if(old>=0){s->stats.state[old]=NN_WINDOW_FREE;s->stats.releases++;}
        s->stats.state[t.index]=NN_WINDOW_CURRENT;s->stats.current_index=(int32_t)t.index;s->stats.shown++;
        s->stats.last_shown_interval=interval;s->stats.shown_intervals[interval]++;
        pthread_cond_broadcast(&s->changed);
    }
    /* Closing cancels pending GPU-complete jobs. ACQUIRED writes are retained
     * by core until explicit owner retire (GPU Finish), independently of SC. */
    int retained=s->stats.resources_retained;
    if(!retained && s->presenter && expired(s))retained=1;
    pthread_mutex_unlock(&s->mutex);
    if(!retained && s->presenter){
        e=s->platform->detach(s->presenter);
        int se=capture_stats(s);if(!e)e=se;
        pthread_mutex_lock(&s->mutex);
        if(e || s->stats.resources_retained || expired(s)){poison(s,e);retained=1;}
        pthread_mutex_unlock(&s->mutex);
        if(!retained){
            /* Withdraw under mutex before destroy: UI cannot wake a freed owner. */
            pthread_mutex_lock(&s->mutex);s->presenter=NULL;pthread_mutex_unlock(&s->mutex);
            e=s->platform->destroy(presenter);
            if(e){pthread_mutex_lock(&s->mutex);s->presenter=presenter;poison(s,e);pthread_mutex_unlock(&s->mutex);retained=1;}
        }
    }
    pthread_mutex_lock(&s->mutex);
    if(s->stats.resources_retained)retained=1;
    pthread_mutex_unlock(&s->mutex);
    if(!retained){
        for(uint32_t i=0;i<s->stats.slots;i++)s->platform->buffer_release(s->slots[i].buffer);
        s->platform->window_release(s->window);
    }
    pthread_mutex_lock(&s->mutex);
    if(!retained){if(s->stats.current_index>=0){s->stats.state[s->stats.current_index]=NN_WINDOW_FREE;s->stats.releases++;}
        s->stats.current_index=-1;s->stats.compositor_retired=1;}
    s->stats.worker_done=1;retire_if_safe(s);pthread_cond_broadcast(&s->changed);
    /* Last access by detached worker, exactly like successful OnComplete waiter. */
    pthread_mutex_unlock(&s->mutex);return NULL;
}
int nn_window_registry_register(const void *token)
{
    if(!token)return EINVAL;
    pthread_mutex_lock(&registry_mutex);struct registry_entry *entry=lookup(token);
    if(entry){pthread_mutex_unlock(&registry_mutex);return 0;}
    for(unsigned i=0;i<REGISTRY_CAPACITY;i++)if(!registry[i].token){registry[i].token=token;
        pthread_mutex_unlock(&registry_mutex);return 0;}
    pthread_mutex_unlock(&registry_mutex);return ENOSPC;
}
int nn_window_registry_attach(const void *token,void *window,uint64_t *generation)
{
    if(!generation)return EINVAL;
    *generation=0;
    if(!token||!window)return EINVAL;
    pthread_mutex_lock(&registry_mutex);struct registry_entry *e=lookup(token);
    if(!e){pthread_mutex_unlock(&registry_mutex);return ENOENT;}
    if(e->window||e->session){pthread_mutex_unlock(&registry_mutex);return EBUSY;}
    if(!next_generation){pthread_mutex_unlock(&registry_mutex);return EOVERFLOW;}
    nn_window_platform_get()->window_acquire(window);e->window=window;e->generation=next_generation++;e->closing=0;
    *generation=e->generation;pthread_mutex_unlock(&registry_mutex);return 0;
}
static int wait_stopped(nn_window_session *s,uint32_t ms)
{
    if(!ms)return s->stats.worker_done?(s->stats.resources_retained?EBUSY:0):EAGAIN;
    while(!s->stats.worker_done && !s->stats.resources_retained){
        int e=pthread_cond_timedwait(&s->changed,&s->mutex,&s->stop_deadline);
        if(e){poison(s,e);return e;}
    }
    return s->stats.resources_retained?(s->stats.first_error?s->stats.first_error:EBUSY):0;
}
int nn_window_registry_close(const void *token,uint64_t generation,uint32_t ms)
{
    if(!token||!generation||ms>MAX_WAIT_MS)return EINVAL;
    pthread_mutex_lock(&registry_mutex);struct registry_entry *e=lookup(token);
    if(!e){pthread_mutex_unlock(&registry_mutex);return ENOENT;}
    if(e->generation!=generation){pthread_mutex_unlock(&registry_mutex);return ESTALE;}
    void *window=e->window;e->window=NULL;e->closing=1;nn_window_session *s=e->session;
    int error=0;
    if(s){pthread_mutex_lock(&s->mutex);s->ui_calls++;error=request_stop(s,ms);}
    pthread_mutex_unlock(&registry_mutex);
    if(window)nn_window_platform_get()->window_release(window);
    if(!s)return 0;
    if(!error && ms)error=wait_stopped(s,ms);
    s->ui_calls--;pthread_mutex_unlock(&s->mutex);return error;
}
static int valid_slots(const struct nn_window_slot *slots,uint32_t n)
{
    if(!slots || (n!=2&&n!=3))return EINVAL;
    for(uint32_t i=0;i<n;i++){
        const struct nn_window_slot *s=slots+i;
        struct nn_display_rgba8_shape shape;
        if(!s->buffer || !s->owner || !s->backing_inode ||
            nn_display_rgba8_shape(s->width,s->height,s->pitch,&shape) ||
            s->format!=1 || s->usage!=819 || s->payload_bytes!=shape.storage_bytes || s->pixel_offset%64 ||
            s->pixel_offset>UINT64_MAX-s->payload_bytes)return ENOTSUP;
        if(i && (s->width!=slots[0].width || s->height!=slots[0].height || s->pitch!=slots[0].pitch))return ENOTSUP;
        int e=nn_window_platform_get()->validate_buffer(s);if(e)return e;
        for(uint32_t j=0;j<i;j++){
            const struct nn_window_slot *p=slots+j;
            if(p->buffer==s->buffer || p->owner==s->owner)return EEXIST;
            if(p->backing_inode==s->backing_inode && p->backing_dev_major==s->backing_dev_major &&
               p->backing_dev_minor==s->backing_dev_minor && p->pixel_offset<s->pixel_offset+s->payload_bytes &&
               s->pixel_offset<p->pixel_offset+p->payload_bytes)return EEXIST;
        }
    }
    return 0;
}
int nn_window_session_interval_supported(int interval)
{
    if(interval<0||interval>5)return EINVAL;
    const struct nn_window_platform *p=nn_window_platform_get();
    int maximum=p->max_interval?p->max_interval():1;
    return interval>maximum?ENOTSUP:0;
}
int nn_window_session_create(const void *token,const struct nn_window_slot *slots,uint32_t n,nn_window_session **out)
{return nn_window_session_create_interval(token,slots,n,1,out);}
int nn_window_session_create_interval(const void *token,const struct nn_window_slot *slots,uint32_t n,int interval,nn_window_session **out)
{
    if(!out)return EINVAL;
    *out=NULL;
    if(!token)return EINVAL;
    int error=nn_window_session_interval_supported(interval);if(error)return error;
    error=valid_slots(slots,n);if(error)return error;
    nn_window_session *s=calloc(1,sizeof(*s));if(!s)return ENOMEM;
    error=pthread_mutex_init(&s->mutex,NULL);if(error){free(s);return error;}
    pthread_condattr_t attr;error=pthread_condattr_init(&attr);
    if(!error){error=pthread_condattr_setclock(&attr,CLOCK_MONOTONIC);if(!error)error=pthread_cond_init(&s->changed,&attr);pthread_condattr_destroy(&attr);}
    if(error){pthread_mutex_destroy(&s->mutex);free(s);return error;}
    s->platform=nn_window_platform_get();s->interval=interval;s->stats.slots=n;s->stats.current_index=-1;
    memcpy(s->slots,slots,n*sizeof(*slots));
    pthread_mutex_lock(&registry_mutex);struct registry_entry *entry=lookup(token);
    if(!entry || !entry->window || entry->closing){error=EAGAIN;goto failed;}
    if(entry->session){error=EBUSY;goto failed;}
    s->entry=entry;s->window=entry->window;s->stats.generation=entry->generation;
    s->platform->window_acquire(s->window);for(uint32_t i=0;i<n;i++)s->platform->buffer_acquire(slots[i].buffer);
    pthread_attr_t ta;error=pthread_attr_init(&ta);
    if(!error){error=pthread_attr_setdetachstate(&ta,PTHREAD_CREATE_DETACHED);
        if(!error){pthread_t thread;error=pthread_create(&thread,&ta,worker,s);}pthread_attr_destroy(&ta);}
    if(error){for(uint32_t i=0;i<n;i++)s->platform->buffer_release(slots[i].buffer);s->platform->window_release(s->window);goto failed;}
    entry->session=s;*out=s;pthread_mutex_unlock(&registry_mutex);return 0;
failed:
    pthread_mutex_unlock(&registry_mutex);pthread_cond_destroy(&s->changed);pthread_mutex_destroy(&s->mutex);free(s);return error;
}
static int ticket_check(nn_window_session *s,const struct nn_window_ticket *t)
{
    if(s->stats.resources_retained)return s->stats.first_error?s->stats.first_error:EBUSY;
    if(s->stats.closing)return ECANCELED;
    if(s->stats.first_error)return s->stats.first_error;
    if(!t || t->generation!=s->stats.generation || t->index>=s->stats.slots ||
        !t->serial || t->serial!=s->serial[t->index])return ESTALE;
    return s->stats.state[t->index]==NN_WINDOW_ACQUIRED?0:EINVAL;
}
int nn_window_session_acquire(nn_window_session *s,uint32_t ms,struct nn_window_ticket *out)
{
    if(!out)return EINVAL;
    memset(out,0,sizeof(*out));
    if(!s||ms>MAX_WAIT_MS)return EINVAL;
    struct timespec d;int e=deadline(ms,&d);if(e)return e;
    pthread_mutex_lock(&s->mutex);
    for(;;){
        if(s->stats.first_error){e=s->stats.first_error;break;}
        if(s->stats.closing){e=ECANCELED;break;}
        if(s->stats.ready){
            for(uint32_t a=0;a<s->stats.slots;a++){
                uint32_t i=(s->cursor+a)%s->stats.slots;if(s->stats.state[i]!=NN_WINDOW_FREE)continue;
                uint64_t serial=atomic_load_explicit(&next_ticket_serial,memory_order_relaxed);
                while(serial && !atomic_compare_exchange_weak_explicit(&next_ticket_serial,&serial,serial+1,
                      memory_order_relaxed,memory_order_relaxed)){}
                if(!serial){e=EOVERFLOW;goto finished;}
                s->serial[i]=serial;s->stats.state[i]=NN_WINDOW_ACQUIRED;s->cursor=(i+1)%s->stats.slots;s->stats.acquires++;
                *out=(struct nn_window_ticket){s->stats.generation,s->serial[i],i};e=0;goto finished;
            }
            unsigned acquired=0;for(uint32_t i=0;i<s->stats.slots;i++)acquired+=s->stats.state[i]==NN_WINDOW_ACQUIRED;
            if(acquired==s->stats.slots){e=EBUSY;break;}
        }
        if(!ms){e=EAGAIN;break;}
        e=pthread_cond_timedwait(&s->changed,&s->mutex,&d);if(e)break;
    }
finished:
    pthread_mutex_unlock(&s->mutex);return e;
}
int nn_window_session_check_acquired(nn_window_session *s,const struct nn_window_ticket *t)
{
    if(!s)return EINVAL;
    pthread_mutex_lock(&s->mutex);int e=ticket_check(s,t);pthread_mutex_unlock(&s->mutex);return e;
}
int nn_window_session_enqueue_ready(nn_window_session *s,const struct nn_window_ticket *t)
{
    if(!s)return EINVAL;
    pthread_mutex_lock(&s->mutex);int e=ticket_check(s,t);
    if(!e){
        /* Only queued entries are GPU-complete and have not reached SC. Never
         * discard SHOWING/CURRENT or release canonical texture/BO pins here. */
        if(!s->interval && s->stats.slots>=3)cancel_queued(s);
        if(s->count>=s->stats.slots)e=ENOSPC;else{
        s->queue_interval[(s->head+s->count)%3]=!s->interval&&s->stats.slots<3?1:s->interval;
        s->queue[(s->head+s->count)%3]=*t;s->count++;s->stats.queued=s->count;s->stats.enqueued++;
        s->stats.state[t->index]=NN_WINDOW_QUEUED;pthread_cond_broadcast(&s->changed);}}
    pthread_mutex_unlock(&s->mutex);return e;
}
int nn_window_session_get_stats(nn_window_session *s,struct nn_window_session_stats *out)
{
    if(!s||!out)return EINVAL;
    pthread_mutex_lock(&s->mutex);*out=s->stats;pthread_mutex_unlock(&s->mutex);return 0;
}
int nn_window_session_retire(nn_window_session *s,uint32_t ms)
{
    if(!s||ms>MAX_WAIT_MS)return EINVAL;
    pthread_mutex_lock(&s->mutex);s->gpu_retired=1;
    int e=request_stop(s,ms);if(!e)e=wait_stopped(s,ms);retire_if_safe(s);
    if(!e&&!s->stats.retired)e=EBUSY;
    pthread_mutex_unlock(&s->mutex);return e;
}
int nn_window_session_destroy(nn_window_session *s)
{
    if(!s)return 0;
    pthread_mutex_lock(&registry_mutex);pthread_mutex_lock(&s->mutex);
    if(!s->stats.retired || s->ui_calls || s->stats.resources_retained){pthread_mutex_unlock(&s->mutex);pthread_mutex_unlock(&registry_mutex);return EBUSY;}
    if(s->entry->session!=s){pthread_mutex_unlock(&s->mutex);pthread_mutex_unlock(&registry_mutex);return EINVAL;}
    s->entry->session=NULL;pthread_mutex_unlock(&s->mutex);pthread_mutex_unlock(&registry_mutex);
    pthread_cond_destroy(&s->changed);pthread_mutex_destroy(&s->mutex);free(s);return 0;
}

int nn_window_session_set_present_interval(nn_window_session *s,int interval)
{
    if(!s)return EINVAL;
    int e=nn_window_session_interval_supported(interval);if(e)return e;
    pthread_mutex_lock(&s->mutex);
    if(s->stats.resources_retained)e=s->stats.first_error?s->stats.first_error:EBUSY;
    else if(s->stats.closing)e=ECANCELED;
    else s->interval=interval;
    pthread_mutex_unlock(&s->mutex);return e;
}
