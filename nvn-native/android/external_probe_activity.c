/* Qualification harness only; NVN and allocator work have one non-UI owner.
 * This is not the game Activity or Window/Acquire/Present implementation. */
#include <android/native_activity.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../tests/external_image_probe.h"

struct external_probe_activity {
    ANativeActivity *activity;
    pthread_t thread;
    int started;
    char root[1024];
};

static void publish(struct external_probe_activity *owner, const char *result, const char *nonce)
{
    char temporary[1200], destination[1200];
    snprintf(temporary,sizeof(temporary),"%s/result.pending",owner->root);
    snprintf(destination,sizeof(destination),"%s/result.json",owner->root);
    const char *end=strrchr(result,'}');
    if(!end) return;
    FILE *file=fopen(temporary,"w");
    if(!file) return;
    int failed=fwrite(result,1,(size_t)(end-result),file)!=(size_t)(end-result);
    if(fprintf(file,",\"run_nonce\":\"%s\",\"owner_thread\":\"worker\"}\n",nonce)<0)failed=1;
    if(fclose(file))failed=1;
    if(!failed)(void)rename(temporary,destination);
}

static void *execute(void *argument)
{
    struct external_probe_activity *owner=argument;
    char nonce[33]={0},path[1200],result[8192];
    int error=0;
    snprintf(path,sizeof(path),"%s/external-run-nonce",owner->root);
    FILE *input=fopen(path,"r");
    if(!input)error=errno;
    else {
        if(fread(nonce,1,32,input)!=32 || fgetc(input)!=EOF || ferror(input))error=EINVAL;
        if(fclose(input) && !error)error=errno;
        for(unsigned i=0;i<32;i++)if(!((nonce[i]>='0'&&nonce[i]<='9') ||
            (nonce[i]>='a'&&nonce[i]<='f')))error=EINVAL;
    }
    if(error)strcpy(nonce,"invalid");
    if(!error){
        snprintf(path,sizeof(path),"%s/external-%s",owner->root,nonce);
        /* Refuse an existing namespace; previous images cannot pass this run. */
        if(mkdir(path,0700))error=errno;
    }
    if(!error) {
        result[0]=0;
        (void)nn_nvn_external_image_probe(path,result,sizeof(result),NVN_EXTERNAL_IMAGE_SLOTS);
    } else snprintf(result,sizeof(result),
        "{\"operation\":\"nvn_external_image\",\"uid\":%u,\"passed\":false,\"errno\":%d,\"stage\":\"run-namespace\"}",
        (unsigned)getuid(),error);
    publish(owner,result,nonce);
    ANativeActivity_finish(owner->activity);
    return NULL;
}

static void destroy(ANativeActivity *activity)
{
    struct external_probe_activity *owner=activity->instance;
    if(!owner)return;
    /* Join before releasing the Activity pointer. Unknown GPU owners inside
     * the qualifier stay retained until process teardown; never free them here. */
    if(owner->started)pthread_join(owner->thread,NULL);
    activity->instance=NULL;free(owner);
}

void nn_external_probe_activity_create(ANativeActivity *activity)
{
    struct external_probe_activity *owner=calloc(1,sizeof(*owner));
    if(!owner){ANativeActivity_finish(activity);return;}
    owner->activity=activity;
    if(snprintf(owner->root,sizeof(owner->root),"%s",activity->internalDataPath)>=(int)sizeof(owner->root)){
        free(owner);ANativeActivity_finish(activity);return;
    }
    activity->instance=owner;activity->callbacks->onDestroy=destroy;
    int error=pthread_create(&owner->thread,NULL,execute,owner);
    if(error){
        char result[256];snprintf(result,sizeof(result),
            "{\"operation\":\"nvn_external_image\",\"uid\":%u,\"passed\":false,\"errno\":%d,\"stage\":\"worker-create\"}",
            (unsigned)getuid(),error);
        publish(owner,result,"invalid");ANativeActivity_finish(activity);
    }else owner->started=1;
}
