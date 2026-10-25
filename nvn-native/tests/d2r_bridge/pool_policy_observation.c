/* HOST ONLY: synthetic typed bridge diagnostics. No game/GPU execution. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int diagnostic, with_observation;
static unsigned calls;
int nn_nvn_error(void) { return diagnostic; }
void nn_nvn_clear_error(void) { diagnostic=0; }
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out)
{
    if(!with_observation)return ENODATA;
    assert(diagnostic==ENOTSUP);
    *out=(struct nn_nvn_pool_access_observation){
        .pool=(const NVNmemoryPool *)(uintptr_t)UINT64_C(0x123456789abc),
        .flags=0x14,.policy=NN_NVN_GPU_UNCACHED_UNQUALIFIED,
        .offset=UINT64_C(0x123456789),.size=UINT64_C(0x23456789a),
        .consumer="synthetic GPU consumer"};
    return 0;
}
static void draw(NVNcommandBuffer *command,NVNdrawPrimitive primitive,int first,int count)
{
    assert(command==(void *)0x1234&&primitive==NVN_DRAW_PRIMITIVE_TRIANGLES&&first==0&&count==3);
    assert(!calls++); diagnostic=ENOTSUP;
}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
    assert(!strcmp(name,"nvnCommandBufferDrawArrays"));
    PFNNVNCOMMANDBUFFERDRAWARRAYSPROC check=draw;
    return (PFNNVNGENERICFUNCPTRPROC)check;
}
static void one(unsigned mode)
{
    int fd[2];assert(!pipe(fd));pid_t pid=fork();assert(pid>=0);
    if(!pid){
        close(fd[0]);assert(dup2(fd[1],2)==2);close(fd[1]);with_observation=mode!=0;
        PFNNVNCOMMANDBUFFERDRAWARRAYSPROC fn=(PFNNVNCOMMANDBUFFERDRAWARRAYSPROC)
            nn_d2r_nvn_bootstrap("nvnCommandBufferDrawArrays");assert(fn);
        if(mode==2)diagnostic=ENOTSUP;
        fn((void *)0x1234,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3);_exit(91);
    }
    close(fd[1]);char text[4096];size_t length=0;ssize_t got;
    while(length<sizeof(text)-1&&(got=read(fd[0],text+length,sizeof(text)-1-length))>0)length+=(size_t)got;
    close(fd[0]);text[length]=0;int status;assert(waitpid(pid,&status,0)==pid);
    assert(WIFEXITED(status)&&WEXITSTATUS(status)==78);
    assert(strstr(text,"nvnCommandBufferDrawArrays"));
    assert(strstr(text,mode==2?"phase=before":"phase=after"));
    const char *observation=strstr(text,"pool_access:"),*fatal=strstr(text,"terminating execution");
    assert(fatal&&(!!observation)==(mode!=0));
    if(observation){
        assert(observation<fatal);
        assert(strstr(text,"pool=0x123456789abc flags=0x00000014 policy=1 offset=4886718345 size=9468016794 consumer=synthetic GPU consumer guest_pc=unavailable"));
    }
}
int main(void)
{
    for(unsigned i=0;i<3;i++)one(i);
    puts("PASS HOST ONLY: pool policy identity/64-bit ranges printed before fatal; no-context and preexisting-sticky cases preserved; no GPU or guest execution");
    return 0;
}
