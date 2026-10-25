/* Qualification Activity. One detached owner runs NVN across generations.
 * UI only controls registry/lifecycle and never joins GPU/compositor work. */
#define _POSIX_C_SOURCE 200809L
#include <android/native_activity.h>
#include <android/native_window.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "nvn_native/window_session.h"
#include "../tests/window_probe.h"
#if NVN_WINDOW_SLOTS != 2 && NVN_WINDOW_SLOTS != 3
#error "NVN_WINDOW_SLOTS must be 2 or 3"
#endif
struct window_probe_activity {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    ANativeWindow *pending;
    unsigned created,destroyed_windows,resumes,pauses;
    unsigned resume_epoch,done_epoch,started,completed,frames;
    unsigned current_frames,current_slot,current_case,current_mask;
    unsigned held_final,hold_released;
    unsigned char token;
    uint64_t generation;
    int destroyed,resumed,stop_current,active,ready,retained,error,thread_started;
    int x,y,width,height,pending_x,pending_y,pending_width,pending_height;
    char root[1024],nonce[33],directory[1200],probe[16384];
    const char *phase;
};
/* mutex held. Paths/geometry copied at UI entry; worker never holds Activity. */
static void publish(struct window_probe_activity *s)
{
    char temporary[1200],destination[1200];
    snprintf(temporary,sizeof(temporary),"%s/result.pending",s->root);
    snprintf(destination,sizeof(destination),"%s/result.json",s->root);
    FILE *file=fopen(temporary,"w");if(!file){if(!s->error)s->error=errno;return;}
    int result=fprintf(file,
        "{\"schema\":1,\"operation\":\"nvn_window\",\"uid\":%u,\"run_nonce\":\"%s\","
        "\"phase\":\"%s\",\"errno\":%d,\"ready\":%s,\"owner_thread\":\"worker\","
        "\"worker_running\":%s,\"resources_retained\":%s,\"generation\":%llu,"
        "\"window_created\":%u,\"window_destroyed\":%u,\"resumes\":%u,\"pauses\":%u,"
        "\"started_sessions\":%u,\"completed_sessions\":%u,\"frames\":%u,\"current_frames\":%u,"
        "\"current_slot\":%u,\"current_case\":%u,\"slots\":%u,"
        "\"current_case_mask\":%u,\"held_final\":%s,\"hold_released_by_pause\":%s,\"activity_destroyed\":%s,"
        "\"window_width\":%d,\"window_height\":%d,\"window_screen_x\":%d,\"window_screen_y\":%d,"
        "\"directory\":\"%s\",\"cpu_image_copy\":false,\"game_modules_executed\":false,"
        "\"probe\":%s,\"passed\":%s}\n",
        (unsigned)getuid(),s->nonce,s->phase,s->error,s->ready?"true":"false",s->active?"true":"false",
        s->retained?"true":"false",(unsigned long long)s->generation,s->created,s->destroyed_windows,
        s->resumes,s->pauses,s->started,s->completed,s->frames,s->current_frames,s->current_slot,s->current_case,
        (unsigned)NVN_WINDOW_SLOTS,s->current_mask,s->held_final?"true":"false",
        s->hold_released?"true":"false",s->destroyed?"true":"false",s->width,s->height,s->x,s->y,s->directory,s->probe[0]?s->probe:"null",
        !s->error&&!s->retained&&!s->active&&s->completed==2&&s->started==2&&s->pauses>=2&&s->destroyed_windows>=2?"true":"false");
    int e=fclose(file);if(result<0||e||rename(temporary,destination)){if(!s->error)s->error=EIO;}
}
static void close_current(struct window_probe_activity *s)
{
    s->stop_current=1;s->ready=0;
    if(s->generation){int e=nn_window_registry_close(&s->token,s->generation,0);
        if(e&&e!=ESTALE&&!s->error)s->error=e;}
    pthread_cond_broadcast(&s->changed);
}
static int closing(void *user)
{
    struct window_probe_activity *s=user;pthread_mutex_lock(&s->mutex);
    int result=s->stop_current||s->destroyed||!s->resumed||s->error;
    pthread_mutex_unlock(&s->mutex);return result;
}
static int presented(void *user,unsigned slot,unsigned test)
{
    struct window_probe_activity *s=user;pthread_mutex_lock(&s->mutex);
    int error=0;
    if(s->error)error=s->error;
    else if(!s->active||s->stop_current||s->destroyed||!s->resumed)error=ECANCELED;
    else if(slot>=NVN_WINDOW_SLOTS||test>=4||s->current_frames>=NVN_WINDOW_SLOTS*4||
            (s->current_mask&(1u<<(slot*4+test))))error=EIO;
    else{
        s->frames++;s->current_frames++;s->current_slot=slot;s->current_case=test;
        s->current_mask|=1u<<(slot*4+test);
        /* Earlier callbacks continue immediately. Only the final original RGB
         * case can become screenshot-ready and hold for an actual UI pause. */
        if(s->current_frames==NVN_WINDOW_SLOTS*4){
            if(test!=3||s->current_mask!=((1u<<(NVN_WINDOW_SLOTS*4))-1))error=EIO;
            else{
                s->held_final=1;s->ready=1;s->phase="presented";publish(s);
                while(!s->stop_current&&!s->destroyed&&s->resumed&&!s->error)
                    pthread_cond_wait(&s->changed,&s->mutex);
                if(s->error)error=s->error;
                else if(!s->hold_released)error=ECANCELED;
                s->ready=0;s->phase="retiring";publish(s);
            }
        }
    }
    pthread_mutex_unlock(&s->mutex);return error;
}
static void *owner(void *user)
{
    struct window_probe_activity *s=user;
    for(;;){
        pthread_mutex_lock(&s->mutex);
        while(!s->destroyed&&!s->error&&!s->retained&&
              (!s->resumed||!s->pending||s->done_epoch==s->resume_epoch||s->completed>=2))pthread_cond_wait(&s->changed,&s->mutex);
        if(s->destroyed||s->error||s->retained){pthread_mutex_unlock(&s->mutex);break;}
        unsigned epoch=s->resume_epoch;ANativeWindow *window=s->pending;ANativeWindow_acquire(window);
        s->active=1;s->stop_current=0;s->ready=0;s->current_frames=0;s->current_mask=0;
        s->held_final=0;s->hold_released=0;s->probe[0]=0;s->phase="drawing";
        s->x=s->pending_x;s->y=s->pending_y;s->width=s->pending_width;s->height=s->pending_height;
        int error=nn_window_registry_attach(&s->token,window,&s->generation);
        if(!error){s->started++;
            int n=snprintf(s->directory,sizeof(s->directory),"%s/window-%s-g%llu",s->root,s->nonce,(unsigned long long)s->generation);
            if(n<0||(size_t)n>=sizeof(s->directory))error=ENAMETOOLONG;
            else if(mkdir(s->directory,0700))error=errno;}
        publish(s);if(!error&&s->error)error=s->error;pthread_mutex_unlock(&s->mutex);
        char result[16384]={0};int probe_called=0;
        if(!error){probe_called=1;struct nn_nvn_window_probe_callbacks cb={.user=s,.presented=presented,.closing=closing};
            error=nn_nvn_window_probe(s->directory,result,sizeof(result),NVN_WINDOW_SLOTS,&s->token,&cb);}
        ANativeWindow_release(window);
        pthread_mutex_lock(&s->mutex);
        if(!memchr(result,0,sizeof(result))){result[0]=0;s->retained=1;if(!error)error=EPROTO;}
        if(!error&&(!result[0]||s->current_frames!=NVN_WINDOW_SLOTS*4||
            s->current_mask!=((1u<<(NVN_WINDOW_SLOTS*4))-1)||!s->held_final||!s->hold_released))error=ECANCELED;
        close_current(s);s->active=0;s->done_epoch=epoch;
        if(result[0])memcpy(s->probe,result,sizeof(s->probe));
        /* A nonzero producer result cannot prove callback/context retirement.
         * Quarantine this Activity context on every probe failure, independent
         * of JSON whitespace. Probe JSON retains its own precise resource data. */
        if((probe_called&&error)||strstr(result,"\"resources_retained\":true"))s->retained=1;
        if(error&&!s->error)s->error=error>0?error:EIO;
        if(!error&&!s->error&&!s->retained)s->completed++;
        s->phase=s->error?"error":"retired";publish(s);
        pthread_mutex_unlock(&s->mutex);
    }
    pthread_mutex_lock(&s->mutex);
    while(!s->destroyed&&!s->retained)pthread_cond_wait(&s->changed,&s->mutex);
    int release=s->destroyed&&!s->retained;
    pthread_mutex_unlock(&s->mutex);
    /* A poisoned probe deliberately retains its complete owner/context. */
    if(release){pthread_cond_destroy(&s->changed);pthread_mutex_destroy(&s->mutex);free(s);}
    return NULL;
}
static void resumed(ANativeActivity *activity)
{
    struct window_probe_activity *s=activity->instance;if(!s)return;
    pthread_mutex_lock(&s->mutex);s->resumed=1;s->resumes++;s->resume_epoch++;
    publish(s);pthread_cond_broadcast(&s->changed);pthread_mutex_unlock(&s->mutex);
}
static void paused(ANativeActivity *activity)
{
    struct window_probe_activity *s=activity->instance;if(!s)return;pthread_mutex_lock(&s->mutex);
    if(s->active&&s->held_final&&s->current_frames==NVN_WINDOW_SLOTS*4)s->hold_released=1;
    s->resumed=0;s->pauses++;close_current(s);publish(s);pthread_mutex_unlock(&s->mutex);
}
static void window_created(ANativeActivity *activity,ANativeWindow *window)
{
    struct window_probe_activity *s=activity->instance;if(!s)return;pthread_mutex_lock(&s->mutex);
    if(s->pending||!window)s->error=EINVAL;
    else{
        JNIEnv *env=activity->env;jclass type=(*env)->GetObjectClass(env,activity->clazz);
        jfieldID x=NULL,y=NULL;
        if(type&&!(*env)->ExceptionCheck(env))x=(*env)->GetFieldID(env,type,"probeSurfaceX","I");
        if(x&&!(*env)->ExceptionCheck(env))y=(*env)->GetFieldID(env,type,"probeSurfaceY","I");
        if(!x||!y||(*env)->ExceptionCheck(env)){(*env)->ExceptionClear(env);if(!s->error)s->error=ENOTSUP;}
        else{s->pending_x=(*env)->GetIntField(env,activity->clazz,x);s->pending_y=(*env)->GetIntField(env,activity->clazz,y);
            if((*env)->ExceptionCheck(env)){(*env)->ExceptionClear(env);if(!s->error)s->error=ENOTSUP;}}
        if(type)(*env)->DeleteLocalRef(env,type);
        ANativeWindow_acquire(window);s->pending=window;s->created++;s->resume_epoch++;
        s->pending_width=ANativeWindow_getWidth(window);s->pending_height=ANativeWindow_getHeight(window);
    }
    publish(s);pthread_cond_broadcast(&s->changed);pthread_mutex_unlock(&s->mutex);
}
static void window_destroyed(ANativeActivity *activity,ANativeWindow *window)
{
    struct window_probe_activity *s=activity->instance;if(!s)return;pthread_mutex_lock(&s->mutex);
    close_current(s);
    if(s->pending!=window){if(!s->error)s->error=EINVAL;}
    else{ANativeWindow_release(s->pending);s->pending=NULL;s->destroyed_windows++;}
    publish(s);pthread_mutex_unlock(&s->mutex);
}
static void destroyed(ANativeActivity *activity)
{
    struct window_probe_activity *s=activity->instance;if(!s)return;
    pthread_mutex_lock(&s->mutex);activity->instance=NULL;s->destroyed=1;close_current(s);
    if(s->pending){ANativeWindow_release(s->pending);s->pending=NULL;}
    publish(s);int release=!s->thread_started;pthread_mutex_unlock(&s->mutex);
    if(release){pthread_cond_destroy(&s->changed);pthread_mutex_destroy(&s->mutex);free(s);}
}
void nn_window_probe_activity_create(ANativeActivity *activity)
{
    struct window_probe_activity *s=calloc(1,sizeof(*s));if(!s){ANativeActivity_finish(activity);return;}
    int e=pthread_mutex_init(&s->mutex,NULL);if(e){free(s);ANativeActivity_finish(activity);return;}
    e=pthread_cond_init(&s->changed,NULL);if(e){pthread_mutex_destroy(&s->mutex);free(s);ANativeActivity_finish(activity);return;}
    int n=activity->internalDataPath?snprintf(s->root,sizeof(s->root),"%s",activity->internalDataPath):-1;
    if(n<0||(size_t)n>=sizeof(s->root)){pthread_cond_destroy(&s->changed);pthread_mutex_destroy(&s->mutex);
        free(s);ANativeActivity_finish(activity);return;}
    s->phase="waiting-window";
    char path[1200];snprintf(path,sizeof(path),"%s/window-run-nonce",s->root);
    FILE *file=fopen(path,"r");if(!file)e=errno;
    else{if(fread(s->nonce,1,32,file)!=32||fgetc(file)!=EOF||ferror(file))e=EINVAL;if(fclose(file)&&!e)e=errno;
        for(unsigned i=0;i<32;i++)if(!((s->nonce[i]>='0'&&s->nonce[i]<='9')||(s->nonce[i]>='a'&&s->nonce[i]<='f')))e=EINVAL;}
    if(e){strcpy(s->nonce,"invalid");s->error=e;}
    if(!s->error)s->error=nn_window_registry_register(&s->token);
    activity->instance=s;activity->callbacks->onResume=resumed;activity->callbacks->onPause=paused;
    activity->callbacks->onNativeWindowCreated=window_created;activity->callbacks->onNativeWindowDestroyed=window_destroyed;
    activity->callbacks->onDestroy=destroyed;
    pthread_attr_t attr;e=pthread_attr_init(&attr);
    if(!e){e=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
        if(!e){pthread_t thread;e=pthread_create(&thread,&attr,owner,s);}pthread_attr_destroy(&attr);}
    pthread_mutex_lock(&s->mutex);if(e)s->error=e;else s->thread_started=1;
    publish(s);pthread_mutex_unlock(&s->mutex);
}
