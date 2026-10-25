/* Authored ABI-only mock; no NVN state implementation, GPU or game execution. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned checks,calls,lookups;static int sticky,missing;
static NVNcommandBuffer object;static NVNboolean seen_enable;
static NVNformatClass seen_format;static NVNstencilFunc seen_stencil;static int seen_a,seen_b;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
_Static_assert(sizeof(NVNboolean)==1&&sizeof(NVNformatClass)==4&&sizeof(NVNstencilFunc)==4,"AArch64 byte/word ABI");
_Static_assert(NVN_FORMAT_CLASS_UNORM8==0&&NVN_FORMAT_CLASS_UNORM10==1&&NVN_FORMAT_CLASS_UNORM16==2&&NVN_FORMAT_CLASS_FP11==3&&NVN_FORMAT_CLASS_FP16==4&&NVN_FORMAT_CLASS_SRGB8==5&&NVN_FORMAT_CLASS_LARGE==-1,"Independent FormatClass values");
_Static_assert(NVN_STENCIL_FUNC_NEVER==1&&NVN_STENCIL_FUNC_ALWAYS==8,"StencilFunc range");
_Static_assert(__builtin_types_compatible_p(PFNNVNCOMMANDBUFFERSETCOLORREDUCTIONENABLEPROC,void(*)(NVNcommandBuffer*,NVNboolean)),"Boolean byte argument");
_Static_assert(__builtin_types_compatible_p(PFNNVNCOMMANDBUFFERSETCOLORREDUCTIONTHRESHOLDSPROC,void(*)(NVNcommandBuffer*,NVNformatClass,int,int)),"Distinct format class and W2/W3 ints");
_Static_assert(__builtin_types_compatible_p(PFNNVNCOMMANDBUFFERSETSTENCILCULLCRITERIAPROC,void(*)(NVNcommandBuffer*,NVNstencilFunc,int,int)),"Distinct stencil comparator and W2/W3 ints");
_Static_assert(!__builtin_types_compatible_p(PFNNVNCOMMANDBUFFERSETSTENCILCULLCRITERIAPROC,void(*)(NVNcommandBuffer*,NVNcompareFunc,int,int)),"Do not substitute CompareFunc for StencilFunc");
int nn_nvn_error(void){return sticky;}
void nn_nvn_clear_error(void){sticky=0;}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *p){(void)p;return ENODATA;}
static void enable(NVNcommandBuffer *p,NVNboolean value){T(p==&object);seen_enable=value;calls++;}
static void thresholds(NVNcommandBuffer *p,NVNformatClass f,int a,int b){T(p==&object);seen_format=f;seen_a=a;seen_b=b;calls++;}
static void stencil(NVNcommandBuffer *p,NVNstencilFunc f,int a,int b){T(p==&object);seen_stencil=f;seen_a=a;seen_b=b;calls++;}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{lookups++;if(missing)return NULL;
 if(!strcmp(name,"nvnCommandBufferSetColorReductionEnable"))return (PFNNVNGENERICFUNCPTRPROC)enable;
 if(!strcmp(name,"nvnCommandBufferSetColorReductionThresholds"))return (PFNNVNGENERICFUNCPTRPROC)thresholds;
 if(!strcmp(name,"nvnCommandBufferSetStencilCullCriteria"))return (PFNNVNGENERICFUNCPTRPROC)stencil;
 return NULL;
}
static void negative(unsigned mode)
{
 int pipefd[2];T(!pipe(pipefd));pid_t pid=fork();T(pid>=0);
 if(!pid){close(pipefd[0]);if(dup2(pipefd[1],2)!=2)_exit(98);close(pipefd[1]);
  if(mode==0){missing=1;((PFNNVNCOMMANDBUFFERSETCOLORREDUCTIONENABLEPROC)nn_d2r_nvn_bootstrap("nvnCommandBufferSetColorReductionEnable"))(&object,1);}
  if(mode==1){sticky=EIO;((PFNNVNCOMMANDBUFFERSETSTENCILCULLCRITERIAPROC)nn_d2r_nvn_bootstrap("nvnCommandBufferSetStencilCullCriteria"))(&object,NVN_STENCIL_FUNC_ALWAYS,0,0);}
  if(mode==2){((PFNNVNGENERICFUNCPTRPROC)nn_d2r_nvn_bootstrap("nvnCommandBufferInsertCheckpointStatic"))();}
  if(mode==3)nn_d2r_nvn_bootstrap("nvnNotPublicName");
  _exit(99);
 }
 close(pipefd[1]);char text[2048];size_t at=0;ssize_t nr;
 while((nr=read(pipefd[0],text+at,sizeof(text)-1-at))>0)at+=(size_t)nr;
 close(pipefd[0]);text[at]=0;int status;T(waitpid(pid,&status,0)==pid);T(WIFEXITED(status)&&WEXITSTATUS(status)==78);
 const char *messages[]={"unsupported call: nvnCommandBufferSetColorReductionEnable","phase=before","unknown ABI call: nvnCommandBufferInsertCheckpointStatic","unknown lookup: nvnNotPublicName"};T(strstr(text,messages[mode]));
}
int main(void)
{
 memset(&object,0xa7,sizeof(object));
 PFNNVNCOMMANDBUFFERSETCOLORREDUCTIONENABLEPROC e=(PFNNVNCOMMANDBUFFERSETCOLORREDUCTIONENABLEPROC)nn_d2r_nvn_bootstrap("nvnCommandBufferSetColorReductionEnable");
 PFNNVNCOMMANDBUFFERSETCOLORREDUCTIONTHRESHOLDSPROC f=(PFNNVNCOMMANDBUFFERSETCOLORREDUCTIONTHRESHOLDSPROC)nn_d2r_nvn_bootstrap("nvnCommandBufferSetColorReductionThresholds");
 PFNNVNCOMMANDBUFFERSETSTENCILCULLCRITERIAPROC s=(PFNNVNCOMMANDBUFFERSETSTENCILCULLCRITERIAPROC)nn_d2r_nvn_bootstrap("nvnCommandBufferSetStencilCullCriteria");
 T(e&&f&&s&&e!=enable&&f!=thresholds&&s!=stencil);
 for(unsigned i=0;i<256;i++){e(&object,(NVNboolean)i);T(seen_enable==i);}
 for(int i=0;i<=5;i++){f(&object,(NVNformatClass)i,INT_MIN,INT_MAX);T(seen_format==(NVNformatClass)i&&seen_a==INT_MIN&&seen_b==INT_MAX);}
 /* Forwarding preserves unvalidated enum and integer bits; it does not invent
  * release-driver clamp rules or claim that the backend implements a setter. */
 f(&object,(NVNformatClass)-1,-1,256);T(seen_format==NVN_FORMAT_CLASS_LARGE&&seen_a==-1&&seen_b==256);
 for(int i=1;i<=8;i++){s(&object,(NVNstencilFunc)i,-257,0x12345678);T(seen_stencil==(NVNstencilFunc)i&&seen_a==-257&&seen_b==0x12345678);}
 s(&object,(NVNstencilFunc)0,-1,INT_MIN);T((int)seen_stencil==0&&seen_a==-1&&seen_b==INT_MIN);
 T(calls==272&&lookups==272&&!sticky);
 for(unsigned i=0;i<sizeof(object);i++)T(((unsigned char*)&object)[i]==0xa7);
 for(unsigned i=0;i<4;i++)negative(i);
 T(calls==272&&lookups==272&&!sticky);
 printf("{\"passed\":true,\"checks\":%u,\"forward_calls\":%u,\"gpu_submits\":0,\"scope\":\"authored HOST exact typed forwarding only; no state/GPU/game capability\"}\n",checks,calls);return 0;
}
