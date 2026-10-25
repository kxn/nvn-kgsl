/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <android/native_activity.h>
#include <android/native_window.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "activity_host.h"

typedef const void *(*token_fn)(void);
typedef int (*attach_fn)(void *,const char *,uint64_t *);
typedef int (*close_fn)(uint64_t,uint32_t);
typedef int (*run_fn)(const char *,int);
typedef int (*bootstrap_fn)(void);
typedef int (*focus_fn)(int);
_Static_assert(_Generic(&nn_d2r_activity_host_notify_focus,focus_fn:1,default:0),"focus ABI");
_Static_assert(_Generic(&nn_d2r_activity_host_token,token_fn:1,default:0),"token ABI");
_Static_assert(_Generic(&nn_d2r_activity_host_attach,attach_fn:1,default:0),"attach ABI");
_Static_assert(_Generic(&nn_d2r_activity_host_close,close_fn:1,default:0),"close ABI");
_Static_assert(_Generic(&nn_d2r_activity_host_run,run_fn:1,default:0),"run ABI");
_Static_assert(_Generic(&nn_d2r_activity_host_check_bootstrap,bootstrap_fn:1,default:0),"bootstrap ABI");
struct controller {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    _Atomic unsigned refs;
    ANativeWindow *pending;
    void *library; /* Deliberately process-resident: original threads/TLS/gates. */
    token_fn token;attach_fn attach;close_fn close;run_fn run;bootstrap_fn bootstrap;focus_fn notify_focus;
    const char *configuration_phase,*context_source;
    uint64_t generation,process_start_ticks;
    int stage,resumed,destroyed,started,stop,error;
    char nonce[33],files[1024],directory[1200],modules[1200],context[256];
};
static void release(struct controller *s)
{
    if(atomic_fetch_sub_explicit(&s->refs,1,memory_order_acq_rel)!=1)return;
    /* No UI/worker can reach s after its final reference. Never dlclose the
     * original host: module/TLS/thread ownership has no unload contract. */
    pthread_cond_destroy(&s->changed);pthread_mutex_destroy(&s->mutex);free(s);
}
static int path(char *out,size_t bytes,const char *base,const char *leaf)
{
    int n=snprintf(out,bytes,"%s/%s",base,leaf);return n<0||(size_t)n>=bytes?ENAMETOOLONG:0;
}
static int fresh_fd(const char *name)
{
    return open(name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
}
/* Immutable, fresh per-phase records. No overwriting a stale success record. */
static int status(struct controller *s,const char *leaf,const char *phase,int error,int returned,int result)
{
    char name[1400],temporary[1450];int e=path(name,sizeof(name),s->directory,leaf);if(e)return e;
    int bytes=snprintf(temporary,sizeof(temporary),"%s.pending",name);
    if(bytes<0||(size_t)bytes>=sizeof(temporary))return ENAMETOOLONG;
    s->configuration_phase="publication-open";
    int fd=fresh_fd(temporary);if(fd<0)return errno;
    s->configuration_phase="publication-fdopen";
    FILE *f=fdopen(fd,"w");if(!f){e=errno;close(fd);return e;}
    s->configuration_phase="publication-write";
    int n=fprintf(f,"{\"schema\":1,\"operation\":\"d2r_activity_host\",\"run_nonce\":\"%s\","
        "\"stage\":%d,\"host_abi\":2,\"phase\":\"%s\",\"pid\":%u,\"uid\":%u,"
        "\"selinux_context\":\"%s\",\"selinux_context_source\":\"%s\",\"process_start_ticks\":%llu,\"generation\":%llu,\"errno\":%d,"
        "\"stage_returned\":%s,\"return_code\":%d,\"first_frame\":false,"
        "\"controller_links_core\":false,\"original_host_resident\":%s}\n",
        s->nonce,s->stage,phase,(unsigned)getpid(),(unsigned)getuid(),s->context,s->context_source,
        (unsigned long long)s->process_start_ticks,(unsigned long long)s->generation,error,returned?"true":"false",result,s->library?"true":"false");
    if(n<0)e=EIO;
    if(!e){s->configuration_phase="publication-flush";if(fflush(f))e=errno?errno:EIO;}
    if(!e){s->configuration_phase="publication-fsync";if(fsync(fileno(f)))e=errno;}
    if(!e)s->configuration_phase="publication-close";
    if(fclose(f)&&!e)e=errno?errno:EIO;
    /* Atomic no-replace rename publishes the complete, synced, closed file.
     * It requires no hard-link permission and never replaces an old success.
     * Unsupported kernels/filesystems fail closed; there is no rename/check
     * fallback or guessed architecture-specific system-call number. */
    if(!e){
        s->configuration_phase="publication-rename-noreplace";
        if(syscall(SYS_renameat2,AT_FDCWD,temporary,AT_FDCWD,name,RENAME_NOREPLACE))e=errno;
    }
    if(e)(void)unlink(temporary);
    return e;
}
static int configure(struct controller *s,const char *internal)
{
    s->context_source="unavailable";
    s->configuration_phase="configuration-path";
    if(!internal||!*internal)return EINVAL;
    if(strlen(internal)>=sizeof(s->files))return ENAMETOOLONG;
    strcpy(s->files,internal);
    char name[1400];int e=path(name,sizeof(name),internal,"d2r-activity/run.cfg");if(e)return e;
    s->configuration_phase="configuration-open";
    int fd=open(name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return errno;
    s->configuration_phase="configuration-read";
    char config[37]={0};ssize_t n=read(fd,config,sizeof(config));int saved=errno;
    if(close(fd)&&n>=0)return errno;
    if(n<0)return saved;
    /* Exactly 32 lowercase hex + space + one stage digit + newline. */
    if(n!=35||config[32]!=' '||config[33]<'1'||config[33]>'3'||config[34]!='\n')return EINVAL;
    for(unsigned i=0;i<32;i++)if(!((config[i]>='0'&&config[i]<='9')||(config[i]>='a'&&config[i]<='f')))return EINVAL;
    memcpy(s->nonce,config,32);s->stage=config[33]-'0';
    char runs[1200];e=path(runs,sizeof(runs),internal,"d2r-activity/runs");if(e)return e;
    e=path(s->directory,sizeof(s->directory),runs,s->nonce);if(e)return e;
    s->configuration_phase="fresh-run-directory";
    if(mkdir(s->directory,0700))return errno; /* EEXIST is deliberately fatal. */
    e=path(s->modules,sizeof(s->modules),internal,"d2r-activity/modules");if(e)return e;
    const char *logs[2]={"stdout.log","stderr.log"};
    for(int i=0;i<2;i++){
        s->configuration_phase=i?"stderr-open-redirect":"stdout-open-redirect";
        e=path(name,sizeof(name),s->directory,logs[i]);if(e)return e;
        fd=fresh_fd(name);if(fd<0)return errno;
        int target=i?STDERR_FILENO:STDOUT_FILENO;
        if(fd==target){if(fcntl(fd,F_SETFD,0)<0)return errno;}
        else{if(dup2(fd,target)<0){e=errno;close(fd);return e;}close(fd);}
    }
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    /* Match Android libselinux getcon's native current-thread lookup. A
     * process-leader /proc/self/attr lookup need not have the same access
     * contract. Never invent a label or fall back after a permission error. */
    s->configuration_phase="selinux-context-open";
    s->context_source="/proc/thread-self/attr/current";
    FILE *context=fopen(s->context_source,"r");
    char thread_path[128];
    if(!context&&errno==ENOENT){
        int bytes=snprintf(thread_path,sizeof(thread_path),"/proc/self/task/%u/attr/current",(unsigned)gettid());
        if(bytes<0||(size_t)bytes>=sizeof(thread_path))return ENAMETOOLONG;
        context=fopen(thread_path,"r");s->context_source="/proc/self/task/gettid/attr/current";
    }
    if(!context)return errno;
    s->configuration_phase="selinux-context-read";
    if(!fgets(s->context,sizeof(s->context),context)){e=ferror(context)?(errno?errno:EIO):ENODATA;fclose(context);return e;}
    s->configuration_phase="selinux-context-close";
    if(fclose(context))return errno;
    s->configuration_phase="selinux-context-validate";
    s->context[strcspn(s->context,"\r\n")]=0;
    for(const char *p=s->context;*p;p++)if(*p<' '||*p=='"'||*p=='\\')return EINVAL;
    s->configuration_phase="process-stat-open";
    FILE *identity=fopen("/proc/self/stat","r");if(!identity)return errno;
    s->configuration_phase="process-stat-read";
    char record[4096];if(!fgets(record,sizeof(record),identity)){fclose(identity);return EIO;}
    s->configuration_phase="process-stat-close";
    if(fclose(identity))return errno;
    s->configuration_phase="process-stat-parse";
    char *comm_end=strrchr(record,')');if(!comm_end||comm_end[1]!=' ')return EINVAL;
    char *save=NULL,*field=strtok_r(comm_end+2," ",&save);
    for(unsigned number=3;number<22&&field;number++)field=strtok_r(NULL," ",&save);
    if(!field)return EINVAL;
    char *end;errno=0;s->process_start_ticks=strtoull(field,&end,10);
    if(errno||!*field||*end||!s->process_start_ticks)return EINVAL;
    s->configuration_phase="identity-validation";
    if(getuid()<10000||!strstr(s->context,"untrusted_app"))return EPERM;
    s->configuration_phase="start-publication";
    return status(s,"start.json","waiting-window",0,0,0);
}
static int load_host(struct controller *s)
{
    s->library=dlopen("libdmhost_activity_kgsl.so",RTLD_NOW|RTLD_LOCAL);
    if(!s->library){fprintf(stderr,"[D2R Activity controller] dlopen: %s\n",dlerror());return ENOENT;}
#define LOAD(field,name) do{void *symbol=dlsym(s->library,name); \
    _Static_assert(sizeof(s->field)==sizeof(symbol),"AAPCS64 function pointer size"); \
    if(!symbol){return ENOSYS;}memcpy(&s->field,&symbol,sizeof(symbol));}while(0)
    LOAD(token,"nn_d2r_activity_host_token");LOAD(attach,"nn_d2r_activity_host_attach");
    LOAD(close,"nn_d2r_activity_host_close");LOAD(run,"nn_d2r_activity_host_run");
    LOAD(bootstrap,"nn_d2r_activity_host_check_bootstrap");
    LOAD(notify_focus,"nn_d2r_activity_host_notify_focus");
#undef LOAD
    return 0;
}
static int environment(struct controller *s)
{
    const char *keys[]={"DM_ROOT","DM_ROMFS","DM_SAVE","DM_TEMP"};
    const char *leaves[]={"root","romfs","save","temp"};
    char game[1200],value[1400];int e=path(game,sizeof(game),s->files,"d2r-host/game");if(e)return e;
    for(unsigned i=0;i<4;i++){
        e=path(value,sizeof(value),game,leaves[i]);if(e)return e;
        if(setenv(keys[i],value,1))return errno;
        struct stat st;if(lstat(value,&st))return errno;if(!S_ISDIR(st.st_mode))return ENOTDIR;
    }
    if(setenv("DM_IMPORTS","1",1)||setenv("DM_BOOTPROF","1",1)||setenv("DM_FS","1",1))return errno;
    if(chdir(getenv("DM_ROOT")))return errno;
    return 0;
}
static void *owner(void *user)
{
    struct controller *s=user;ANativeWindow *window=NULL;int e=0,returned=0,result=0;
    pthread_mutex_lock(&s->mutex);
    while(!s->destroyed&&!s->stop&&!(s->resumed&&s->pending))pthread_cond_wait(&s->changed,&s->mutex);
    if(s->destroyed||s->stop)e=ECANCELED;
    else{s->started=1;window=s->pending;ANativeWindow_acquire(window);}
    pthread_mutex_unlock(&s->mutex);
    if(!e)e=load_host(s);
    if(!e)e=environment(s);
    uint64_t generation=0;
    if(!e)e=s->attach(window,NULL,&generation);
    pthread_mutex_lock(&s->mutex);s->generation=generation;
    if(!e&&(s->stop||s->destroyed))e=ECANCELED;
    if(!e)e=s->notify_focus(s->resumed?1:3);
    pthread_mutex_unlock(&s->mutex);
    if(!e)e=s->bootstrap();
    if(!e)e=status(s,"attached.json","original-entry-next",0,0,0);
    if(!e){
        fprintf(stderr,"[D2R Activity controller] native owner entering stage%d pid=%u uid=%u exact_vi_token=%p generation=%llu\n",
            s->stage,(unsigned)getpid(),(unsigned)getuid(),s->token(),(unsigned long long)generation);
        result=s->run(s->modules,s->stage);returned=1;if(result)e=result;
    }
    /* No NVN/GPU cleanup here. Original host stays resident until process stop;
     * close only requests compositor stopping, not a successful game teardown. */
    if(generation){int c=s->close(generation,0);if(c&&c!=ESTALE&&!e)e=c;}
    if(window)ANativeWindow_release(window);
    pthread_mutex_lock(&s->mutex);if(s->error&&!e)e=s->error;pthread_mutex_unlock(&s->mutex);
    if(e)fprintf(stderr,"[D2R Activity controller] terminal controller errno=%d stage_returned=%d return_code=%d\n",e,returned,result);
    int se=status(s,"result.json",e?"failed":"stage-returned",e,returned,result);
    if(se)fprintf(stderr,"[D2R Activity controller] result publication errno=%d\n",se);
    release(s);return NULL;
}
static void request_stop(struct controller *s,int error)
{
    uint64_t generation;close_fn close;
    pthread_mutex_lock(&s->mutex);s->stop=1;if(error&&!s->error)s->error=error;
    generation=s->generation;close=generation?s->close:NULL;pthread_cond_broadcast(&s->changed);pthread_mutex_unlock(&s->mutex);
    if(generation&&close){int e=close(generation,0);if(e&&e!=ESTALE){pthread_mutex_lock(&s->mutex);if(!s->error)s->error=e;pthread_mutex_unlock(&s->mutex);}}
}
static void resumed(ANativeActivity *a)
{
    struct controller *s=a->instance;if(!s)return;
    pthread_mutex_lock(&s->mutex);s->resumed=1;
    /* This calls the original native oe queue producer, not guest code/NVN.
     * A closed one-shot host is still terminal; do not advertise a resume. */
    if(s->generation&&!s->stop){int e=s->notify_focus(1);if(e&&!s->error)s->error=e;}
    pthread_cond_broadcast(&s->changed);pthread_mutex_unlock(&s->mutex);
}
static void paused(ANativeActivity *a)
{
    struct controller *s=a->instance;if(!s)return;
    pthread_mutex_lock(&s->mutex);s->resumed=0;int active=s->started;
    if(s->generation){int e=s->notify_focus(3);if(e&&!s->error)s->error=e;}
    pthread_mutex_unlock(&s->mutex);
    /* Initial Activity setup can pause before any native owner has started.
     * Keep waiting for the first resumed window; there is nothing to retire.
     * Once started, pause remains terminal, never a simulated game suspend. */
    if(active)request_stop(s,0);
}
static void window_created(ANativeActivity *a,ANativeWindow *window)
{
    struct controller *s=a->instance;if(!s||!window)return;
    pthread_mutex_lock(&s->mutex);
    if(s->started||s->pending){pthread_mutex_unlock(&s->mutex);request_stop(s,ENOTSUP);return;}
    ANativeWindow_acquire(window);s->pending=window;pthread_cond_broadcast(&s->changed);pthread_mutex_unlock(&s->mutex);
}
static void window_destroyed(ANativeActivity *a,ANativeWindow *window)
{
    struct controller *s=a->instance;if(!s)return;ANativeWindow *release_window=NULL;
    pthread_mutex_lock(&s->mutex);
    if(s->pending==window){release_window=s->pending;s->pending=NULL;}
    int active=s->started;pthread_mutex_unlock(&s->mutex);
    /* Replacing an initial surface, or destroying an unrelated old surface,
     * does not invalidate a window already claimed by the owner. */
    if(release_window&&active)request_stop(s,0);
    if(release_window)ANativeWindow_release(release_window);
}
static void destroyed(ANativeActivity *a)
{
    struct controller *s=a->instance;if(!s)return;a->instance=NULL;
    pthread_mutex_lock(&s->mutex);s->destroyed=1;ANativeWindow *window=s->pending;s->pending=NULL;pthread_mutex_unlock(&s->mutex);
    request_stop(s,0);if(window)ANativeWindow_release(window);release(s);
}
__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity *a,void *saved,size_t saved_bytes)
{
    (void)saved;(void)saved_bytes;
    struct controller *s=calloc(1,sizeof(*s));if(!s)_Exit(79);
    int e=pthread_mutex_init(&s->mutex,NULL);if(e)_Exit(79);
    e=pthread_cond_init(&s->changed,NULL);if(e)_Exit(79);atomic_init(&s->refs,1);
    e=configure(s,a->internalDataPath);
    if(e){fprintf(stderr,"[D2R Activity controller] configuration phase=%s errno=%d\n",s->configuration_phase?s->configuration_phase:"unavailable",e);_Exit(79);}
    a->instance=s;a->callbacks->onResume=resumed;a->callbacks->onPause=paused;
    a->callbacks->onNativeWindowCreated=window_created;a->callbacks->onNativeWindowDestroyed=window_destroyed;
    a->callbacks->onDestroy=destroyed;
    pthread_attr_t attr;e=pthread_attr_init(&attr);
    if(!e){e=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
        if(!e){pthread_t thread;atomic_fetch_add(&s->refs,1);e=pthread_create(&thread,&attr,owner,s);if(e)release(s);}pthread_attr_destroy(&attr);}
    if(e){fprintf(stderr,"[D2R Activity controller] owner thread errno=%d\n",e);status(s,"result.json","thread-failed",e,0,0);}
}
