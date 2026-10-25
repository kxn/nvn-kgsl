/* HOST ONLY: actual typed bridge with a synthetic driver, no GPU/game. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int diagnostic,missing,after;
static NVNimageHandle value=UINT64_C(0xfedcba9876543210);
int nn_nvn_error(void){return diagnostic;}
void nn_nvn_clear_error(void){diagnostic=0;}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out)
{(void)out;return ENODATA;}
static NVNimageHandle image(const NVNdevice *device,int id)
{
    assert(device==(const void *)(uintptr_t)UINT64_C(0x123456789abc));
    assert(id==-1234567);fputs("raw_called\n",stderr);
    if(after)diagnostic=ENOTSUP;
    return value;
}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
    assert(!strcmp(name,"nvnDeviceGetImageHandle"));fputs("lookup_called\n",stderr);
    if(missing)return NULL;
    PFNNVNDEVICEGETIMAGEHANDLEPROC function=image;
    return (PFNNVNGENERICFUNCPTRPROC)function;
}
static void call(PFNNVNDEVICEGETIMAGEHANDLEPROC function)
{
    NVNimageHandle result=function((const void *)(uintptr_t)UINT64_C(0x123456789abc),-1234567);
    assert(result==value);
}
int main(void)
{
    PFNNVNDEVICEGETIMAGEHANDLEPROC function=(PFNNVNDEVICEGETIMAGEHANDLEPROC)
        nn_d2r_nvn_bootstrap("nvnDeviceGetImageHandle");
    call(function);value=0;call(function);value=UINT64_C(0xfedcba9876543210);
    for(unsigned mode=0;mode<3;mode++){
        int fd[2];assert(!pipe(fd));pid_t pid=fork();assert(pid>=0);
        if(!pid){close(fd[0]);assert(dup2(fd[1],2)==2);close(fd[1]);
            missing=mode==0;diagnostic=mode==1?EBUSY:0;after=mode==2;
            call(function);_exit(91);}
        close(fd[1]);char text[2048];size_t used=0;ssize_t n;
        while(used<sizeof(text)-1&&(n=read(fd[0],text+used,sizeof(text)-1-used))>0)used+=(size_t)n;
        close(fd[0]);text[used]=0;int status;
        assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==78);
        const char *entry=strstr(text,"device=0x123456789abc id=-1234567 guest_pc=unavailable");
        const char *lookup=strstr(text,"lookup_called"),*raw=strstr(text,"raw_called");
        const char *fatal=strstr(text,"terminating execution (exit 78)");
        assert(entry&&fatal&&entry<fatal&&!strstr(text,"result="));
        assert((lookup!=NULL)==(mode!=1)&&(raw!=NULL)==(mode==2));
        if(lookup)assert(entry<lookup);
        if(raw)assert(lookup<raw&&raw<fatal);
    }
    puts("PASS HOST ONLY: signed image ID,64-bit device/return,zero return and3 fatal boundaries; no GPU/game");
    return 0;
}
