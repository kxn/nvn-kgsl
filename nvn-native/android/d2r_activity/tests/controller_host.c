#define _GNU_SOURCE
#include <android/native_activity.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
static int scenario;
static _Atomic int freed,entered,close_calls,run_calls,publications,syncs;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static int proceed;
static unsigned char token;
static int failure;
static _Atomic int focus_calls,last_focus;
static int legacy_context_reads,thread_context_reads,task_context_reads;
#define CHECK(x) do{if(!(x)){failure=__LINE__;return 1;}}while(0)
static void fake_free(void *p){free(p);atomic_fetch_add(&freed,1);}
static uid_t fake_uid(void){return 10017;}
static FILE *fake_fopen(const char *name,const char *mode){
 if(!strcmp(name,"/proc/self/attr/current")){legacy_context_reads++;errno=EACCES;return NULL;}
 if(!strcmp(name,"/proc/thread-self/attr/current")){
  thread_context_reads++;
  if(scenario==16||scenario==17||scenario==18){errno=scenario==16?EACCES:scenario==17?ENOENT:EIO;return NULL;}
  static char label[]="u:r:untrusted_app:s0:c17\n";return fmemopen(label,sizeof(label)-1,"r");
 }
 if(!strncmp(name,"/proc/self/task/",16)&&strstr(name,"/attr/current")){
  task_context_reads++;if(scenario!=17)abort();
  static char label[]="u:r:untrusted_app:s0:c17\n";return fmemopen(label,sizeof(label)-1,"r");
 }
 if(scenario==19&&!strcmp(name,"/proc/self/stat")){errno=EACCES;return NULL;}
 return fopen(name,mode);
}
static const void *fake_token(void){return &token;}
static int fake_attach(void *w,const char *resolution,uint64_t *g){if(!w||resolution)return EINVAL;*g=77;return 0;}
static int fake_close(uint64_t g,uint32_t ms){atomic_fetch_add(&close_calls,1);return g==77&&!ms?0:EINVAL;}
static int fake_run(const char *modules,int stage){
 if(!strstr(modules,"/d2r-activity/modules")||stage!=1)return EINVAL;
 atomic_fetch_add(&run_calls,1);pthread_mutex_lock(&gate);atomic_store(&entered,1);pthread_cond_broadcast(&changed);
 while((scenario==2||scenario==3)&&!proceed)pthread_cond_wait(&changed,&gate);
 pthread_mutex_unlock(&gate);return 0;
}
static int fake_bootstrap(void){return 0;}
static int fake_focus(int state){if(state<1||state>3)return EINVAL;atomic_store(&last_focus,state);atomic_fetch_add(&focus_calls,1);return 0;}
static void *fake_dlopen(const char *name,int flags){(void)flags;if(strcmp(name,"libdmhost_activity_kgsl.so")||scenario==5)return NULL;return &token;}
static const char *fake_dlerror(void){return "explicit host fixture load denial";}
static void *fake_dlsym(void *handle,const char *name){
 (void)handle;if(scenario==6&&!strcmp(name,"nn_d2r_activity_host_attach"))return NULL;
 if(scenario==22&&!strcmp(name,"nn_d2r_activity_host_notify_focus"))return NULL;
 if(!strcmp(name,"nn_d2r_activity_host_notify_focus"))return (void *)fake_focus;
 if(!strcmp(name,"nn_d2r_activity_host_token"))return (void *)fake_token;
 if(!strcmp(name,"nn_d2r_activity_host_attach"))return (void *)fake_attach;
 if(!strcmp(name,"nn_d2r_activity_host_close"))return (void *)fake_close;
 if(!strcmp(name,"nn_d2r_activity_host_run"))return (void *)fake_run;
 if(!strcmp(name,"nn_d2r_activity_host_check_bootstrap"))return (void *)fake_bootstrap;
 return NULL;
}
static int fake_create(pthread_t *t,const pthread_attr_t *a,void *(*fn)(void *),void *user){
 if(scenario==7)return EAGAIN;
 return pthread_create(t,a,fn,user);
}
static int fake_fsync(int fd){if(scenario==13){errno=EIO;return -1;}int e=fsync(fd);if(!e)atomic_fetch_add(&syncs,1);return e;}
static long fake_syscall(long number,...){
 if(number!=SYS_renameat2)abort();
 va_list arguments;va_start(arguments,number);
 int olddir=va_arg(arguments,int);const char *a=va_arg(arguments,const char *);
 int newdir=va_arg(arguments,int);const char *b=va_arg(arguments,const char *);
 int flags=va_arg(arguments,int);va_end(arguments);
 if(olddir!=AT_FDCWD||newdir!=AT_FDCWD||flags!=RENAME_NOREPLACE)abort();
 if(scenario==14||scenario==20||scenario==21){errno=scenario==14?EACCES:scenario==20?ENOSYS:EOPNOTSUPP;return -1;}
 FILE *f=fopen(a,"r");if(!f)abort();if(fseek(f,-2,SEEK_END))abort();if(fgetc(f)!='}'||fgetc(f)!='\n')abort();fclose(f);
 if(atomic_load(&syncs)<=atomic_load(&publications))abort();
 long e=syscall(number,olddir,a,newdir,b,flags);if(!e)atomic_fetch_add(&publications,1);return e;
}
#define free fake_free
#define getuid fake_uid
#define fopen fake_fopen
#define dlopen fake_dlopen
#define dlerror fake_dlerror
#define dlsym fake_dlsym
#define pthread_create fake_create
#define fsync fake_fsync
#define syscall fake_syscall
#include "../controller.c"
#undef free
#undef getuid
#undef fopen
#undef dlopen
#undef dlerror
#undef dlsym
#undef pthread_create
#undef fsync
#undef syscall
void ANativeWindow_acquire(ANativeWindow *w){atomic_fetch_add(&w->refs,1);}
void ANativeWindow_release(ANativeWindow *w){if(atomic_fetch_sub(&w->refs,1)<=1)abort();}
static int wait_atomic(_Atomic int *p,int wanted){
 for(unsigned i=0;i<10000;i++){if(atomic_load(p)==wanted)return 0;struct timespec pause={0,1000000};nanosleep(&pause,NULL);}return ETIMEDOUT;
}
static int result_exists(const char *files){
 char name[1400];snprintf(name,sizeof(name),"%s/d2r-activity/runs/0123456789abcdef0123456789abcdef/result.json",files);
 for(unsigned i=0;i<10000;i++){if(!access(name,F_OK))return 0;struct timespec pause={0,1000000};nanosleep(&pause,NULL);}return ETIMEDOUT;
}
int main(int argc,char **argv){
 CHECK(argc==3);scenario=atoi(argv[1]);ANativeActivityCallbacks cb={0};ANativeActivity activity={.callbacks=&cb,.internalDataPath=argv[2]};
 ANativeWindow window,other;atomic_init(&window.refs,1);atomic_init(&other.refs,1);
 if(scenario==15){errno=0;CHECK(!fake_fopen("/proc/self/attr/current","r")&&errno==EACCES);}
 if(scenario==11){close(STDOUT_FILENO);close(STDERR_FILENO);}
 ANativeActivity_onCreate(&activity,NULL,0); /* Invalid config/IO cases exit79. */
 CHECK(activity.instance&&cb.onDestroy&&cb.onResume);
 CHECK(legacy_context_reads==(scenario==15?1:0)&&thread_context_reads==1&&task_context_reads==(scenario==17?1:0));
 if(scenario==7){CHECK(result_exists(argv[2])==0);cb.onDestroy(&activity);CHECK(wait_atomic(&freed,1)==0);CHECK(atomic_load(&run_calls)==0);return 0;}
 if(scenario==4)cb.onPause(&activity);
 if(scenario==23){cb.onNativeWindowCreated(&activity,&other);cb.onNativeWindowDestroyed(&activity,&other);CHECK(atomic_load(&other.refs)==1);CHECK(atomic_load(&run_calls)==0);}
 if(scenario==24)cb.onNativeWindowDestroyed(&activity,&other);
 cb.onResume(&activity);cb.onNativeWindowCreated(&activity,&window);
 if(scenario==2||scenario==3){
  CHECK(wait_atomic(&entered,1)==0);CHECK(atomic_load(&window.refs)==3);
  if(scenario==3)cb.onNativeWindowCreated(&activity,&other);
  if(scenario==2){cb.onPause(&activity);CHECK(atomic_load(&last_focus)==3);int observed=atomic_load(&focus_calls);cb.onResume(&activity);CHECK(atomic_load(&focus_calls)==observed);}
  cb.onDestroy(&activity);CHECK(!activity.instance);CHECK(atomic_load(&freed)==0);CHECK(atomic_load(&window.refs)==2);
  pthread_mutex_lock(&gate);proceed=1;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);
  CHECK(wait_atomic(&freed,1)==0);CHECK(atomic_load(&window.refs)==1);CHECK(atomic_load(&close_calls)>=2);return 0;
 }
 CHECK(result_exists(argv[2])==0);
 if(scenario==12){
  char name[1400],before[4096],after[4096];snprintf(name,sizeof(name),"%s/d2r-activity/runs/0123456789abcdef0123456789abcdef/result.json",argv[2]);
  FILE *f=fopen(name,"r");CHECK(f);size_t n=fread(before,1,sizeof(before),f);fclose(f);
  CHECK(status(activity.instance,"result.json","must-not-overwrite",EINVAL,0,0)==EEXIST);
  f=fopen(name,"r");CHECK(f);size_t m=fread(after,1,sizeof(after),f);fclose(f);CHECK(n==m&&!memcmp(before,after,n));
 }
 cb.onNativeWindowDestroyed(&activity,&window);cb.onDestroy(&activity);CHECK(wait_atomic(&freed,1)==0);
 CHECK(atomic_load(&window.refs)==1);CHECK(atomic_load(&other.refs)==1);
 CHECK(atomic_load(&run_calls)==((scenario==5||scenario==6||scenario==22)?0:1));
 return 0;
}
