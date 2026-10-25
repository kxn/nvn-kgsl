/* SPDX-License-Identifier: MIT */
#include "activity_host.h"
#include "nvn_native/window_session.h"
#include "nvn_native/nvn.h"
#include "host_bridge.h"
#include <android/native_window.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>

extern void dm_focus_set(int);
extern const void *dm_vi_native_window_token(void);
extern int dm_android_display_configure(unsigned,unsigned,const char *);
extern int dm_android_main(int,char **);
static pthread_mutex_t control=PTHREAD_MUTEX_INITIALIZER;
static uint64_t attached_generation;
static int started,closed;
#define PUBLIC __attribute__((visibility("default")))
PUBLIC const void *nn_d2r_activity_host_token(void) { return dm_vi_native_window_token(); }
PUBLIC int nn_d2r_activity_host_attach(void *native_window,const char *resolution,uint64_t *generation)
{
    if(!generation)return EINVAL;
    *generation=0;
    if(!native_window)return EINVAL;
    pthread_mutex_lock(&control);
    /* The existing Window does not support live-generation rebinding. Do not
     * turn a fresh registry attach into a fake resume of the original game. */
    int error=started?ENOTSUP:0;
    const void *token=dm_vi_native_window_token();
    if(!error && !token)error=EINVAL;
    if(!error && attached_generation && !closed)error=EBUSY;
    if(!error){
        int width=ANativeWindow_getWidth(native_window),height=ANativeWindow_getHeight(native_window);
        if(width<=0||height<=0)error=EINVAL;
        else if(!dm_android_display_configure((unsigned)width,(unsigned)height,resolution))error=EINVAL;
    }
    if(!error)error=nn_window_registry_register(token);
    uint64_t next=0;
    if(!error)error=nn_window_registry_attach(token,native_window,&next);
    if(!error){attached_generation=next;closed=0;*generation=next;
        fprintf(stderr,"[native D2R Activity] exact VI token=%p generation=%llu; same-process ANW attached, no DISPLAY qualification implied\n",token,(unsigned long long)next);}
    pthread_mutex_unlock(&control);return error;
}
PUBLIC int nn_d2r_activity_host_close(uint64_t generation,uint32_t timeout_ms)
{
    if(!generation||timeout_ms>1500)return EINVAL;
    pthread_mutex_lock(&control);
    if(generation!=attached_generation){pthread_mutex_unlock(&control);return ESTALE;}
    closed=1;
    pthread_mutex_unlock(&control);
    /* UI only: compositor stop acknowledgement, never NVN/GPU retirement. */
    return nn_window_registry_close(dm_vi_native_window_token(),generation,timeout_ms);
}
PUBLIC int nn_d2r_activity_host_run(const char *modules,int stage)
{
    if(!modules||!*modules||stage<1||stage>3)return EINVAL;
    pthread_mutex_lock(&control);
    int error=started?EALREADY:!attached_generation||closed?EAGAIN:0;
    if(!error)started=1;
    pthread_mutex_unlock(&control);
    if(error)return error;
    char *mode=stage==1?"--sdk-init":stage==2?"--constructors":"--run";
    char *argv[]={"d2r-native-activity",(char *)modules,mode,NULL};
    fprintf(stderr,"[native D2R Activity] original ARM64 owner entry stage=%s host_abi=2; stage return is not guest initialization/frame success\n",mode);
    return dm_android_main(3,argv);
}
PUBLIC int nn_d2r_activity_host_check_bootstrap(void)
{
    PFNNVNGENERICFUNCPTRPROC bootstrap=nn_d2r_nvn_bootstrap("nvnDeviceGetProcAddress");
    if(!bootstrap)return ENOSYS;
    PFNNVNDEVICEGETPROCADDRESSPROC get=(PFNNVNDEVICEGETPROCADDRESSPROC)bootstrap;
    PFNNVNGENERICFUNCPTRPROC via_bootstrap=get(NULL,"nvnDeviceBuilderSetDefaults");
    PFNNVNGENERICFUNCPTRPROC via_adapter=nn_d2r_nvn_get_proc_address(NULL,"nvnDeviceBuilderSetDefaults");
    return via_bootstrap && via_bootstrap==via_adapter?0:ENOSYS;
}

/* Original oe notification producer only: no guest call, TLS, save, clock or
 * NVN operation. Original game HLE queries consume state/message15 themselves. */
PUBLIC int nn_d2r_activity_host_notify_focus(int state)
{
    if(state<1||state>3)return EINVAL;
    dm_focus_set(state);
    return 0; /* Accepted notification; never a save/suspend/resume ack. */
}
