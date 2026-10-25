/* HOST ONLY: full-width typed observations using a synthetic driver. */
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
static NVNboolean value=0xa5;
static const char *names[]={"nvnTexturePoolInitialize","nvnSamplerPoolInitialize"};
int nn_nvn_error(void){return diagnostic;}
void nn_nvn_clear_error(void){diagnostic=0;}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out){(void)out;return ENODATA;}
static NVNboolean called(void *o,const NVNmemoryPool *p,ptrdiff_t offset,int count)
{
 assert(o==(void *)(uintptr_t)UINT64_C(0x123456789abc)&&p==(void *)(uintptr_t)UINT64_C(0x23456789abcd));
 assert(offset==INT64_C(-4886718345)&&count==-1234567);fputs("raw_called\n",stderr);
 if(after)diagnostic=ENOTSUP;
 return value;
}
static NVNboolean texture(NVNtexturePool *o,const NVNmemoryPool *p,ptrdiff_t a,int c){return called(o,p,a,c);}
static NVNboolean sampler(NVNsamplerPool *o,const NVNmemoryPool *p,ptrdiff_t a,int c){return called(o,p,a,c);}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *n)
{
 fputs("lookup_called\n",stderr);if(missing)return NULL;
 if(!strcmp(n,names[0])){PFNNVNTEXTUREPOOLINITIALIZEPROC f=texture;return (PFNNVNGENERICFUNCPTRPROC)f;}
 assert(!strcmp(n,names[1]));PFNNVNSAMPLERPOOLINITIALIZEPROC f=sampler;return (PFNNVNGENERICFUNCPTRPROC)f;
}
static void call(unsigned i,PFNNVNGENERICFUNCPTRPROC f)
{
 NVNboolean r=i?((PFNNVNSAMPLERPOOLINITIALIZEPROC)f)((void *)(uintptr_t)UINT64_C(0x123456789abc),(void *)(uintptr_t)UINT64_C(0x23456789abcd),INT64_C(-4886718345),-1234567):
 ((PFNNVNTEXTUREPOOLINITIALIZEPROC)f)((void *)(uintptr_t)UINT64_C(0x123456789abc),(void *)(uintptr_t)UINT64_C(0x23456789abcd),INT64_C(-4886718345),-1234567);
 assert(r==value);
}
int main(void)
{
 for(unsigned i=0;i<2;i++){
  PFNNVNGENERICFUNCPTRPROC f=nn_d2r_nvn_bootstrap(names[i]);call(i,f);value=0;call(i,f);value=0xa5;
  for(unsigned mode=0;mode<3;mode++){
   int fd[2];assert(!pipe(fd));pid_t pid=fork();assert(pid>=0);
   if(!pid){close(fd[0]);assert(dup2(fd[1],2)==2);close(fd[1]);missing=mode==0;diagnostic=mode==1?EBUSY:0;after=mode==2;call(i,f);_exit(91);}
   close(fd[1]);char text[2048];size_t n=0;ssize_t got;while(n<sizeof(text)-1&&(got=read(fd[0],text+n,sizeof(text)-1-n))>0)n+=(size_t)got;
   close(fd[0]);text[n]=0;int status;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==78);
   const char *entry=strstr(text,names[i]),*lookup=strstr(text,"lookup_called"),*raw=strstr(text,"raw_called"),*fatal=strstr(text,"terminating execution");
   assert(entry&&fatal&&entry<fatal&&strstr(text,"object=0x123456789abc pool=0x23456789abcd offset=-4886718345 count=-1234567 guest_pc=unavailable"));
   assert((lookup!=NULL)==(mode!=1)&&(raw!=NULL)==(mode==2));if(lookup)assert(entry<lookup);if(raw)assert(lookup<raw&&raw<fatal);
   assert(!strstr(text,"result="));
  }
 }
 puts("PASS HOST ONLY: two typed descriptor observations,64-bit signed offset/pointers,32-bit signed count,nonzero/false results and6 fatal boundaries; no GPU/game");return 0;
}
