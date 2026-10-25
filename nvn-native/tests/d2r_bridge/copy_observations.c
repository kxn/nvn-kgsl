/* HOST ONLY exact public copy ABI, bounded reader and diagnostic ordering. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int diagnostic,mode,api,raw_calls,reads;
static const NVNcopyRegion *region_argument;
static const char *names[]={"nvnCommandBufferCopyBufferToTexture","nvnCommandBufferCopyTextureToBuffer",
 "nvnCommandBufferCopyTextureToTexture","nvnCommandBufferSetCopyRowStride","nvnCommandBufferSetCopyImageStride"};
#define C ((NVNcommandBuffer *)(uintptr_t)0x123456789abc)
#define T ((const NVNtexture *)(uintptr_t)0x23456789abcd)
#define V ((const NVNtextureView *)(uintptr_t)0x3456789abcde)
#define B UINT64_C(0xfedcba9876543210)
int nn_nvn_error(void){return diagnostic;}
void nn_nvn_clear_error(void){assert(!"observer must not clear diagnostics");}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out)
{(void)out;return ENODATA;}
int nn_nvn_get_program_source_info(const NVNprogram *p,NVNbufferAddress a,struct nn_nvn_program_source_info *o)
{(void)p;(void)a;(void)o;assert(!"no extra getters");return ENODATA;}
long nn_program_mock_read(void *out,const void *source,size_t bytes)
{
 assert(api<3&&mode!=3&&source==region_argument&&bytes==sizeof(NVNcopyRegion));
 reads++;errno=EFAULT;
 NVNcopyRegion value={-7,9,11,63,47,3};memcpy(out,&value,sizeof(value));
 if(mode==1)return -1;
 if(mode==2)return (long)bytes-1;
 return (long)bytes;
}
static void called(void)
{assert(errno==E2BIG);raw_calls++;fputs("raw_called\n",stderr);errno=EDOM;if(mode==4)diagnostic=ENOTSUP;}
static void b2t(NVNcommandBuffer *c,NVNbufferAddress b,const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r,int f)
{assert(c==C&&b==B&&t==T&&v==V&&r==region_argument&&f==-19);called();}
static void t2b(NVNcommandBuffer *c,const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r,NVNbufferAddress b,int f)
{assert(c==C&&b==B&&t==T&&v==V&&r==region_argument&&f==-19);called();}
static void t2t(NVNcommandBuffer *c,const NVNtexture *s,const NVNtextureView *sv,const NVNcopyRegion *sr,const NVNtexture *d,const NVNtextureView *dv,const NVNcopyRegion *dr,int f)
{assert(c==C&&s==T&&d==T&&sv==V&&dv==V&&sr==region_argument&&dr==region_argument&&f==-19);called();}
static void stride(NVNcommandBuffer *c,ptrdiff_t n)
{assert(c==C&&n==-(ptrdiff_t)INT64_C(0x123456789));called();}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
 assert(!strcmp(name,names[api]));fputs("lookup_called\n",stderr);if(mode==5)return NULL;
#define CASE(n,pfn,fn) case n:{pfn exact=fn;return(PFNNVNGENERICFUNCPTRPROC)exact;}
 switch(api){CASE(0,PFNNVNCOMMANDBUFFERCOPYBUFFERTOTEXTUREPROC,b2t)
 CASE(1,PFNNVNCOMMANDBUFFERCOPYTEXTURETOBUFFERPROC,t2b)
 CASE(2,PFNNVNCOMMANDBUFFERCOPYTEXTURETOTEXTUREPROC,t2t)
 CASE(3,PFNNVNCOMMANDBUFFERSETCOPYROWSTRIDEPROC,stride)
 CASE(4,PFNNVNCOMMANDBUFFERSETCOPYIMAGESTRIDEPROC,stride)}
#undef CASE
 assert(0);return NULL;
}
static void invoke(void)
{
 PFNNVNGENERICFUNCPTRPROC fn=nn_d2r_nvn_get_proc_address(NULL,names[api]);errno=E2BIG;
 if(mode==3)diagnostic=EBUSY;
 switch(api){
 case 0:((PFNNVNCOMMANDBUFFERCOPYBUFFERTOTEXTUREPROC)fn)(C,B,T,V,region_argument,-19);break;
 case 1:((PFNNVNCOMMANDBUFFERCOPYTEXTURETOBUFFERPROC)fn)(C,T,V,region_argument,B,-19);break;
 case 2:((PFNNVNCOMMANDBUFFERCOPYTEXTURETOTEXTUREPROC)fn)(C,T,V,region_argument,T,V,region_argument,-19);break;
 case 3:((PFNNVNCOMMANDBUFFERSETCOPYROWSTRIDEPROC)fn)(C,-(ptrdiff_t)INT64_C(0x123456789));break;
 case 4:((PFNNVNCOMMANDBUFFERSETCOPYIMAGESTRIDEPROC)fn)(C,-(ptrdiff_t)INT64_C(0x123456789));break;
 }
 assert(errno==EDOM&&raw_calls==1&&!diagnostic);
#ifdef NN_TEST_BINDING_READER
 assert(reads==(api<3&&region_argument&&(uintptr_t)region_argument<=UINTPTR_MAX-sizeof(*region_argument)?(api==2?2:1):0));
#else
 assert(!reads);
#endif
}
static unsigned checks;
static void exercise(int selected,int selected_mode,const NVNcopyRegion *pointer)
{
 api=selected;mode=selected_mode;region_argument=pointer;
 int fd[2];assert(!pipe(fd));pid_t child=fork();assert(child>=0);
 if(!child){close(fd[0]);assert(dup2(fd[1],2)==2);close(fd[1]);invoke();_exit(0);}
 close(fd[1]);char text[32768];size_t size=0;ssize_t n;
 while((n=read(fd[0],text+size,sizeof(text)-1-size))>0)size+=(size_t)n;
 close(fd[0]);text[size]=0;int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status));
 assert(WEXITSTATUS(status)==(mode>=3&&mode<=5?78:0));checks++;
 char *trace=strstr(text,names[api]),*lookup=strstr(text,"lookup_called"),*raw=strstr(text,"raw_called");
 assert(trace&&strstr(text,"guest_pc=unavailable"));checks++;
 if(mode==3)assert(!lookup&&!raw&&strstr(text,"phase=before"));else assert(lookup&&trace<lookup);
 if(mode==4)assert(raw&&strstr(text,"phase=after"));
 if(mode==5)assert(!raw&&strstr(text,"unsupported call"));
 checks++;
 if(api<3){
  assert(strstr(text,"flags=0xffffffed")&&strstr(text,"view_fields=opaque_not_inspected"));
  if(api<2)assert(strstr(text,"buffer=0xfedcba9876543210"));
  if(mode==3)assert(strstr(text,"reason=prior_diagnostic"));
#ifdef NN_TEST_BINDING_READER
  else if(!pointer||(uintptr_t)pointer>UINTPTR_MAX-sizeof(*pointer))assert(strstr(text,"reason=invalid_range"));
  else if(mode==1||mode==2)assert(strstr(text,"reason=read")&&!strstr(text,"width="));
  else assert(strstr(text,"x=-7 y=9 z=11 width=63 height=47 depth=3"));
#else
  else assert(strstr(text,"reason=reader_not_enabled"));
#endif
 }else assert(strstr(text,"stride=-4886718345"));
 checks++;
}
int main(void)
{
 for(int a=0;a<5;a++)for(int m=0;m<=5;m++)exercise(a,m,(const NVNcopyRegion *)(uintptr_t)0x456789abcdef);
 for(int a=0;a<3;a++){
  exercise(a,0,NULL);exercise(a,0,(const NVNcopyRegion *)(UINTPTR_MAX-3));
 }
 printf("{\"passed\":true,\"checks\":%u,\"gpu_submits\":0,\"scope\":\"HOST public copy ABI and bounded observation only\"}\n",checks);
}
