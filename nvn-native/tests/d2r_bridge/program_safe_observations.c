/* HOST ONLY: synthetic reader/driver around actual candidate bridge. */
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
static int diagnostic,mode,count,reads,infos,called;
static NVNboolean value;
static const NVNshaderData *const array=(void *)(uintptr_t)UINT64_C(0xfedcba987650);
static NVNprogram *const program=(void *)(uintptr_t)UINT64_C(0x123456789abc);
static const void *const control=(void *)(uintptr_t)UINT64_C(0x654321fedc00);
int nn_nvn_error(void){return diagnostic;}
void nn_nvn_clear_error(void){diagnostic=0;}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out)
{(void)out;return ENODATA;}
int nn_nvn_get_program_source_info(const NVNprogram *p,NVNbufferAddress a,struct nn_nvn_program_source_info *out)
{
    assert(p==program&&a>=UINT64_C(0x123456780000)&&a<UINT64_C(0x123456780006));++infos;
    if(mode==9)return ENOENT;
    *out=(struct nn_nvn_program_source_info){.pool=(void *)(uintptr_t)UINT64_C(0x13579bdf0000),.flags=0x64,.policy=NN_NVN_GPU_POOL_CACHED,.offset=123,.available=456,.cpu_readable=1};
    return 0;
}
long nn_program_mock_read(void *out,const void *address,size_t bytes)
{
    ++reads;errno=EPERM;
    if(address==array){
        assert(bytes==(size_t)count*sizeof(NVNshaderData)&&count>=1&&count<=6);
        if(mode==1)return -1;
        if(mode==2)return (long)bytes-1;
        NVNshaderData copy[6];
        for(int i=0;i<count;++i)copy[i]=(NVNshaderData){UINT64_C(0x123456780000)+(unsigned)i,mode==10?NULL:mode==11?(const void *)(UINTPTR_MAX-3):control};
        memcpy(out,copy,bytes);return (long)bytes;
    }
    assert(address==control);
    if(bytes==4){
        if(mode==3)return -1;
        if(mode==4)return 3;
        unsigned char data[4]={0x34,0x12,0x76,0x98};if(mode==8)data[3]=0;
        memcpy(out,data,4);return 4;
    }
    assert(bytes==0x898);
    if(mode==5)return -1;
    if(mode==6)return 0x897;
    memset(out,0x5a,bytes);
    unsigned char magic[4]={0x34,0x12,0x76,0x98};if(mode==7)magic[3]=0;memcpy(out,magic,4);
    unsigned char fields[8]={0x00,0x03,0,0,0x00,0x04,0,0};
    memcpy((unsigned char *)out+0x700,fields,8);return 0x898;
}
static NVNboolean raw(NVNprogram *p,int n,const NVNshaderData *s)
{
    assert(p==program&&n==count&&s==(mode==12?(const void *)(UINTPTR_MAX-3):array));
    assert(errno==E2BIG);++called;fputs("raw_called\n",stderr);
    if(mode==15)diagnostic=ENOTSUP;
    return value;
}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
    assert(!strcmp(name,"nvnProgramSetShaders"));fputs("lookup_called\n",stderr);
    if(mode==14)return NULL;
    PFNNVNPROGRAMSETSHADERSPROC f=raw;return (PFNNVNGENERICFUNCPTRPROC)f;
}
static void one(PFNNVNPROGRAMSETSHADERSPROC f,int which,int n)
{
    mode=which;count=n;value=(which&1)?0:0xa5;
    int fd[2];assert(!pipe(fd));pid_t pid=fork();assert(pid>=0);
    if(!pid){close(fd[0]);assert(dup2(fd[1],2)==2);close(fd[1]);errno=E2BIG;
        diagnostic=mode==13?EBUSY:0;
        NVNboolean v=f(program,count,mode==12?(const void *)(UINTPTR_MAX-3):array);
        assert(v==value&&!diagnostic&&errno==E2BIG&&called==1);
        int active=count>=1&&count<=6;
        assert((reads>0)==(active&&mode!=12));
        assert((infos>0)==(active&&mode!=1&&mode!=2&&mode!=12));
        if(active&&mode!=1&&mode!=2&&mode!=12){int expected=1+count*((mode==10||mode==11)?0:(mode==3||mode==4||mode==8)?1:2);assert(reads==expected&&infos==count);}
        _exit(0);}
    close(fd[1]);char text[131072];size_t used=0;ssize_t got;
    while(used<sizeof(text)-1&&(got=read(fd[0],text+used,sizeof(text)-1-used))>0)used+=(size_t)got;
    close(fd[0]);text[used]=0;int status;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status));
    assert(WEXITSTATUS(status)==(which>=13?78:0));
    if(which>=13){assert(strstr(text,"terminating execution (exit 78)")&&!strstr(text,"nvnProgramSetShaders: result="));if(which==13)assert(!strstr(text,"shader_input:")&&!strstr(text,"lookup_called"));}
    else assert(strstr(text,"raw_called")&&strstr(text,"nvnProgramSetShaders: result="));
    if(n>=1&&n<=6&&which==0){assert(strstr(text,"data=0x0000123456780000")&&strstr(text,"pool=0x13579bdf0000 flags=0x00000064 policy=0 offset=123 pool_remaining=456 cpu_readable=1 code_bytes=unknown")&&strstr(text,"raw700=768 raw704=1024 lengths_validated=false"));}
    if(which==9)assert(strstr(text,"shader_source: index=0 unavailable="));
    if(which==8||which==5||which==6||which==7)assert(!strstr(text,"raw700=")&&!strstr(text,"shader_control_snapshot:"));
    if(which==0&&n>=1&&n<=6)assert(strstr(text,"offset=0x880 bytes=24")&&strstr(text,"profile=caller_898_candidate"));
    if(which==1||which==2)assert(!strstr(text,"shader_source:"));
}
int main(void)
{
    PFNNVNPROGRAMSETSHADERSPROC f=(PFNNVNPROGRAMSETSHADERSPROC)nn_d2r_nvn_bootstrap("nvnProgramSetShaders");
    for(int m=0;m<16;++m)one(f,m,2);
    const int counts[]={INT_MIN,-1,0,1,6,7,INT_MAX};
    for(unsigned i=0;i<sizeof(counts)/sizeof(counts[0]);++i)one(f,0,counts[i]);
    puts("PASS HOST ONLY: 23 cases; exact/failed/short safe reads, poison/overflow/NULL, count bounds, pool identity, unchanged errno/arguments/boolean, old sticky and fatal ordering; no game/GPU");
    return 0;
}
