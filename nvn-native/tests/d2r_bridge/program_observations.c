/* HOST ONLY: actual candidate typed bridge + synthetic driver, no game/GPU. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int diagnostic,missing,after,expected_count;
static NVNboolean value;
static const NVNshaderData *expected_shaders;
static NVNprogram *const program=(void *)(uintptr_t)UINT64_C(0x123456789abc);
int nn_nvn_error(void){return diagnostic;}
void nn_nvn_clear_error(void){diagnostic=0;}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out)
{(void)out;return ENODATA;}
static NVNboolean shaders(NVNprogram *p,int count,const NVNshaderData *data)
{
    assert(p==program&&count==expected_count&&data==expected_shaders);
    /* Never dereference this deliberately invalid array pointer. */
    fputs("raw_called\n",stderr);
    if(after)diagnostic=ENOTSUP;
    return value;
}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
    assert(!strcmp(name,"nvnProgramSetShaders"));fputs("lookup_called\n",stderr);
    if(missing)return NULL;
    PFNNVNPROGRAMSETSHADERSPROC function=shaders;
    return (PFNNVNGENERICFUNCPTRPROC)function;
}
static void run(PFNNVNPROGRAMSETSHADERSPROC function,int count,int mode,int null_array,NVNboolean result)
{
    expected_count=count;expected_shaders=null_array?NULL:(const void *)(uintptr_t)UINT64_C(0xfedcba987650);
    value=result;
    int fd[2];assert(!pipe(fd));pid_t pid=fork();assert(pid>=0);
    if(!pid){close(fd[0]);assert(dup2(fd[1],2)==2);close(fd[1]);
        missing=mode==1;diagnostic=mode==2?EBUSY:0;after=mode==3;
        NVNboolean actual=function(program,count,expected_shaders);
        assert(actual==value&&!diagnostic);_exit(0);}
    close(fd[1]);char text[4096];size_t used=0;ssize_t n;
    while(used<sizeof(text)-1&&(n=read(fd[0],text+used,sizeof(text)-1-used))>0)used+=(size_t)n;
    close(fd[0]);text[used]=0;int status;
    assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status));
    assert(WEXITSTATUS(status)==(mode?78:0));
    char entry[256];snprintf(entry,sizeof(entry),"nvnProgramSetShaders: program=%p count=%d shaders=%p guest_pc=unavailable",(void *)program,count,(const void *)expected_shaders);
    const char *start=strstr(text,entry),*lookup=strstr(text,"lookup_called"),*raw=strstr(text,"raw_called");
    assert(start&&(lookup!=NULL)==(mode!=2)&&(raw!=NULL)==(mode==0||mode==3));
    if(lookup)assert(start<lookup);
    if(raw)assert(lookup<raw);
    if(mode){const char *fatal=strstr(text,"terminating execution (exit 78)");
        assert(fatal&&start<fatal&&!strstr(text,"result="));
        if(raw)assert(raw<fatal);
        assert(strstr(text,mode==1?"phase=call":mode==2?"phase=before":"phase=after"));
    }else{char expected[64];snprintf(expected,sizeof(expected),"nvnProgramSetShaders: result=%u",(unsigned)value);
        const char *returned=strstr(text,expected);assert(returned&&raw<returned);}
}
int main(void)
{
    PFNNVNPROGRAMSETSHADERSPROC function=(PFNNVNPROGRAMSETSHADERSPROC)
        nn_d2r_nvn_bootstrap("nvnProgramSetShaders");
    const int counts[]={1,2,0,-1,INT_MIN,INT_MAX};
    for(unsigned i=0;i<sizeof(counts)/sizeof(counts[0]);i++)run(function,counts[i],0,0,i&1?0:0xa5);
    run(function,1,0,1,0); /* NULL preserved; validity is the driver's decision. */
    PFNNVNPROGRAMSETSHADERSPROC via_device=(PFNNVNPROGRAMSETSHADERSPROC)
        nn_d2r_nvn_get_proc_address(NULL,"nvnProgramSetShaders");
    run(via_device,2,0,0,0xa5);
    for(int mode=1;mode<=3;mode++)run(function,INT_MIN,mode,0,0xa5);
    puts("PASS HOST ONLY: 11 cases; typed pointers/count/boolean unchanged, invalid array never read, both resolvers, 3 fatal boundaries; no GPU/game");
    return 0;
}
