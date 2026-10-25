/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <android/native_activity.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../tests/nvn_counter_probe.h"
struct counter_owner { char root[1024],directory[1200],nonce[33],leaf[48]; };
static atomic_flag started=ATOMIC_FLAG_INIT;
static int name(char *out,size_t n,const char *root,const char *leaf)
{ int k=snprintf(out,n,"%s/%s",root,leaf);return k<0||(size_t)k>=n?ENAMETOOLONG:0; }
static void publish(struct counter_owner *s,const char *json)
{
    char target[1300],temp[1300],leaf[80];
    if(name(target,sizeof(target),s->root,"result.json"))return;
    snprintf(leaf,sizeof(leaf),"counter-result-%s.pending",s->nonce);
    if(name(temp,sizeof(temp),s->root,leaf))return;
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return;
    FILE *f=fdopen(fd,"w");if(!f){close(fd);return;}
    int e=fprintf(f,"{\"run_nonce\":\"%s\",\"run_directory\":\"%s\",%s",s->nonce,s->leaf,json+1)<0;
    if(fflush(f)||fsync(fd))e=1;
    if(fclose(f))e=1;
    /* Atomic no-replace publication through the Linux syscall exported on
     * this Android ABI; unsupported/denied operations fail closed. No hardlink
     * or overwriting rename fallback. Failure keeps the unique pending file. */
    if(!e && syscall(SYS_renameat2,AT_FDCWD,temp,AT_FDCWD,target,RENAME_NOREPLACE))e=1;
}
static void *owner(void *arg)
{
    struct counter_owner *s=arg;char json[32768]={0};
    int result=nn_nvn_counter_probe(s->directory,json,sizeof(json));
    size_t length=strnlen(json,sizeof(json));
    if(json[0]!='{' || length<3 || length>=sizeof(json) || (json[length-1]!='}' && !(json[length-1]=='\n' && json[length-2]=='}')))
        snprintf(json,sizeof(json),"{\"schema\":1,\"operation\":\"nvn_counter\",\"uid\":%u,\"errno\":%d,\"stage\":\"invalid-probe-result\",\"resources_retained\":true,\"passed\":false}\n",(unsigned)getuid(),result?EIO:EOVERFLOW);
    publish(s,json);free(s);return NULL;
}
static int configure(struct counter_owner *s,const char *root)
{
    if(!root||strlen(root)>=sizeof(s->root))return ENAMETOOLONG;
    strcpy(s->root,root);char path[1300];int e=name(path,sizeof(path),root,"counter-run-nonce");if(e)return e;
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return errno;
    struct stat st;if(fstat(fd,&st)){e=errno;close(fd);return e;}
    if(!S_ISREG(st.st_mode) || st.st_size!=32 || st.st_uid!=getuid()){close(fd);return EINVAL;}
    char value[33];ssize_t n=read(fd,value,sizeof(value));e=errno;
    int ce=close(fd);if(n!=32)return n<0?e:EINVAL;if(ce)return errno;
    for(unsigned i=0;i<32;i++)if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return EINVAL;
    memcpy(s->nonce,value,32);snprintf(s->leaf,sizeof(s->leaf),"counter-%s",s->nonce);
    e=name(s->directory,sizeof(s->directory),root,s->leaf);if(e)return e;
    if(mkdir(s->directory,0700))return errno;
    return 0;
}
void nn_counter_probe_activity_create(ANativeActivity *activity)
{
    /* UI copies paths once and starts one detached owner. No GPU/compositor
     * wait or Activity/ANW reference crosses into the worker. Runner stops the
     * dedicated process after collecting results, including failure evidence. */
    if(atomic_flag_test_and_set(&started))return;
    struct counter_owner *s=calloc(1,sizeof(*s));if(!s)return;
    int e=configure(s,activity?activity->internalDataPath:NULL);pthread_attr_t attr;int ready=0;
    if(!e){e=pthread_attr_init(&attr);ready=!e;}
    if(!e)e=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    if(!e){pthread_t thread;e=pthread_create(&thread,&attr,owner,s);}
    if(ready)pthread_attr_destroy(&attr);
    if(e){char json[512];snprintf(json,sizeof(json),"{\"schema\":1,\"operation\":\"nvn_counter\",\"uid\":%u,\"errno\":%d,\"stage\":\"activity-start\",\"passed\":false}\n",(unsigned)getuid(),e);publish(s,json);free(s);}
}
