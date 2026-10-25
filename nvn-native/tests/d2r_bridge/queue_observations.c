/* Synthetic public C ABI/observation test. No real queue, GPU, kernel or guest.
 * Large arguments/return canaries test carriers, not valid NVN capacities. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int diagnostic, missing, fail_after;
static unsigned fatal_cases;
static size_t size_result=UINT64_C(0xfedcba9876543210);
static NVNboolean bool_result=0xa5;
static const char *const apis[]={
    "nvnQueueBuilderSetFlags", "nvnQueueBuilderSetCommandMemorySize",
    "nvnQueueBuilderSetComputeMemorySize", "nvnQueueBuilderSetControlMemorySize",
    "nvnQueueBuilderSetCommandFlushThreshold", "nvnQueueBuilderGetQueueMemorySize",
    "nvnQueueBuilderSetQueueMemory", "nvnQueueInitialize"
};
/* Synthetic driver has no native pool-policy diagnostic. */
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out)
{ (void)out; return ENODATA; }
int nn_nvn_error(void){return diagnostic;}
void nn_nvn_clear_error(void){diagnostic=0;}
static void raw_called(void)
{ fputs("[test raw] called\n",stderr);if(fail_after)diagnostic=ENOTSUP; }
static void flags(NVNqueueBuilder *builder,int value)
{ assert(builder==(void *)0x1234&&(uint32_t)value==UINT32_C(0xfedcba98));raw_called(); }
#define SIZE_API(name,index) static void name(NVNqueueBuilder *builder,size_t size) { \
    assert(builder==(void *)0x1234&&size==UINT64_C(0x123456789)+(index));raw_called(); }
SIZE_API(command_size,1)
SIZE_API(compute_size,2)
SIZE_API(control_size,3)
SIZE_API(flush_threshold,4)
#undef SIZE_API
static size_t memory_size(const NVNqueueBuilder *builder)
{ assert(builder==(void *)0x1234);raw_called();return size_result; }
static void memory(NVNqueueBuilder *builder,void *storage,size_t bytes)
{ assert(builder==(void *)0x1234&&(uintptr_t)storage==UINT64_C(0x123456789abc)&&bytes==UINT64_C(0x123456789));raw_called(); }
static NVNboolean initialize(NVNqueue *queue,const NVNqueueBuilder *builder)
{ assert(queue==(void *)0x5678&&builder==(void *)0x1234);raw_called();return bool_result; }
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
    fputs("[test driver lookup]\n",stderr);
    if(missing)return NULL;
#define PROVIDE(index,function,type) if(!strcmp(name,apis[index])){type check=function;return (PFNNVNGENERICFUNCPTRPROC)check;}
    PROVIDE(0,flags,PFNNVNQUEUEBUILDERSETFLAGSPROC)
    PROVIDE(1,command_size,PFNNVNQUEUEBUILDERSETCOMMANDMEMORYSIZEPROC)
    PROVIDE(2,compute_size,PFNNVNQUEUEBUILDERSETCOMPUTEMEMORYSIZEPROC)
    PROVIDE(3,control_size,PFNNVNQUEUEBUILDERSETCONTROLMEMORYSIZEPROC)
    PROVIDE(4,flush_threshold,PFNNVNQUEUEBUILDERSETCOMMANDFLUSHTHRESHOLDPROC)
    PROVIDE(5,memory_size,PFNNVNQUEUEBUILDERGETQUEUEMEMORYSIZEPROC)
    PROVIDE(6,memory,PFNNVNQUEUEBUILDERSETQUEUEMEMORYPROC)
    PROVIDE(7,initialize,PFNNVNQUEUEINITIALIZEPROC)
#undef PROVIDE
    return NULL;
}
static void call(unsigned api,PFNNVNGENERICFUNCPTRPROC function)
{
    switch(api){
    case 0:((PFNNVNQUEUEBUILDERSETFLAGSPROC)function)((void *)0x1234,(int)UINT32_C(0xfedcba98));break;
    case 1:((PFNNVNQUEUEBUILDERSETCOMMANDMEMORYSIZEPROC)function)((void *)0x1234,UINT64_C(0x12345678a));break;
    case 2:((PFNNVNQUEUEBUILDERSETCOMPUTEMEMORYSIZEPROC)function)((void *)0x1234,UINT64_C(0x12345678b));break;
    case 3:((PFNNVNQUEUEBUILDERSETCONTROLMEMORYSIZEPROC)function)((void *)0x1234,UINT64_C(0x12345678c));break;
    case 4:((PFNNVNQUEUEBUILDERSETCOMMANDFLUSHTHRESHOLDPROC)function)((void *)0x1234,UINT64_C(0x12345678d));break;
    case 5:assert(((PFNNVNQUEUEBUILDERGETQUEUEMEMORYSIZEPROC)function)((void *)0x1234)==size_result);break;
    case 6:((PFNNVNQUEUEBUILDERSETQUEUEMEMORYPROC)function)((void *)0x1234,(void *)(uintptr_t)UINT64_C(0x123456789abc),UINT64_C(0x123456789));break;
    case 7:assert(((PFNNVNQUEUEINITIALIZEPROC)function)((void *)0x5678,(void *)0x1234)==bool_result);break;
    default:assert(0);
    }
}
static void check_fatal(unsigned api,unsigned failure)
{
    int descriptors[2];assert(!pipe(descriptors));pid_t pid=fork();assert(pid>=0);
    if(!pid){
        close(descriptors[0]);assert(dup2(descriptors[1],STDERR_FILENO)==STDERR_FILENO);close(descriptors[1]);
        PFNNVNGENERICFUNCPTRPROC function=nn_d2r_nvn_get_proc_address(NULL,apis[api]);
        if(failure==0)missing=1;
        if(failure==1)diagnostic=EBUSY;
        if(failure==2)fail_after=1;
        call(api,function);_exit(91);
    }
    close(descriptors[1]);char text[4096];size_t bytes=0;ssize_t got;
    while(bytes<sizeof(text)-1&&(got=read(descriptors[0],text+bytes,sizeof(text)-1-bytes))>0)bytes+=(size_t)got;
    close(descriptors[0]);text[bytes]=0;int status;
    assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==78);
    const char *entry=strstr(text,apis[api]),*lookup=strstr(text,"[test driver lookup]"),*raw=strstr(text,"[test raw] called"),*fatal=strstr(text,"terminating execution");
    assert(entry&&fatal&&entry<fatal&&strstr(text,"guest_pc=unavailable"));
    assert((lookup!=NULL)==(failure!=1));if(lookup)assert(entry<lookup);
    assert((raw!=NULL)==(failure==2));if(raw)assert(lookup<raw&&raw<fatal);
    char expected[96];snprintf(expected,sizeof(expected),"errno=%d; phase=%s",failure==0?ENOSYS:failure==1?EBUSY:ENOTSUP,failure==0?"call":failure==1?"before":"after");assert(strstr(text,expected));
    if(api==0)assert(strstr(text,"flags=0xfedcba98"));
    if(api>=1&&api<=4){snprintf(expected,sizeof(expected),"%s=%zu",api==4?"threshold":"size",(size_t)UINT64_C(0x123456789)+api);assert(strstr(text,expected));}
    if(api==6)assert(strstr(text,"storage=0x123456789abc size=4886718345"));
    if(api==5||api==7)assert(!strstr(text,"result="));
    fputs(text,stdout);fatal_cases++;
}
int main(void)
{
    for(unsigned api=0;api<sizeof(apis)/sizeof(apis[0]);api++){
        PFNNVNGENERICFUNCPTRPROC function=nn_d2r_nvn_bootstrap(apis[api]);
        assert(function&&function!=nvnBootstrapLoader(apis[api]));
        assert(function==nn_d2r_nvn_get_proc_address(NULL,apis[api]));
        call(api,function);assert(!diagnostic);
        for(unsigned failure=0;failure<3;failure++)check_fatal(api,failure);
    }
    size_result=0;call(5,nn_d2r_nvn_bootstrap(apis[5]));assert(!diagnostic);
    bool_result=0;call(7,nn_d2r_nvn_bootstrap(apis[7]));assert(!diagnostic);
    assert(fatal_cases==24);
    puts("PASS HOST ONLY: 8 typed Queue observations; full64 pointers/size/result and full32flags; 24 fatal children validate pre-lookup/missing/old-sticky/after-error; zero/false results preserved; no storage dereference or fabricated guest PC");
    return 0;
}
