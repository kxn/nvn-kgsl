/* HOST ONLY authored pointer/scalar ABI and bounded-reader seam. No GPU/guest. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int diagnostic,mode,which,read_calls,raw_calls,expected_count,expected_first;
static const void *expected_array;
static const char *names[]={"nvnTexturePoolRegisterTexture","nvnCommandBufferSetTexturePool","nvnCommandBufferBindSeparateTexture","nvnCommandBufferBindSeparateTextures","nvnTextureViewSetDefaults","nvnTextureViewSetLevels","nvnTextureViewSetLayers","nvnTextureViewSetFormat","nvnTextureViewSetTarget","nvnTextureViewSetDepthStencilMode","nvnTextureViewSetSwizzle"};
#define POOL ((const NVNtexturePool *)(uintptr_t)UINT64_C(0x123456789abc))
#define COMMAND ((NVNcommandBuffer *)(uintptr_t)UINT64_C(0x23456789abcd))
#define TEXTURE ((const NVNtexture *)(uintptr_t)UINT64_C(0x3456789abcde))
#define VIEW ((NVNtextureView *)(uintptr_t)UINT64_C(0x456789abcdef))
int nn_nvn_error(void){return diagnostic;}
void nn_nvn_clear_error(void){assert(!"observer must never clear diagnostic");}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *o){(void)o;return ENODATA;}
int nn_nvn_get_program_source_info(const NVNprogram *p,NVNbufferAddress a,struct nn_nvn_program_source_info *o)
{(void)p;(void)a;(void)o;assert(!"no extra resource getter for texture observation");return ENODATA;}
long nn_program_mock_read(void *out,const void *source,size_t bytes)
{
    assert(mode!=3);assert(which==3&&source==expected_array&&expected_count>=1&&expected_count<=32&&bytes==(size_t)expected_count*8);read_calls++;errno=EFAULT;
    NVNseparateTextureHandle *p=out;for(int i=0;i<expected_count;i++)p[i].value=UINT64_C(0x8000000100000101)+(unsigned)i;
    if(mode==1)return -1;
    if(mode==2)return (long)bytes-1;
    return (long)bytes;
}
static void called(void)
{assert(errno==E2BIG);raw_calls++;fputs("raw_called\n",stderr);errno=EDOM;if(mode==4)diagnostic=ENOTSUP;}
static void reg(const NVNtexturePool *p,int id,const NVNtexture *t,const NVNtextureView *v)
{assert(p==POOL&&id==-17&&t==TEXTURE&&v==VIEW);called();}
static void set_pool(NVNcommandBuffer *c,const NVNtexturePool *p){assert(c==COMMAND&&p==POOL);called();}
static void bind(NVNcommandBuffer *c,NVNshaderStage s,int i,NVNseparateTextureHandle h)
{assert(c==COMMAND&&s==NVN_SHADER_STAGE_FRAGMENT&&i==11&&h.value==UINT64_C(0xfedcba9800100101));called();}
static void binds(NVNcommandBuffer *c,NVNshaderStage s,int f,int count,const NVNseparateTextureHandle *p)
{assert(c==COMMAND&&s==NVN_SHADER_STAGE_FRAGMENT&&f==expected_first&&count==expected_count&&p==expected_array);called();}
static void defaults(NVNtextureView *v){assert(v==VIEW);called();}
static void levels(NVNtextureView *v,int b,int n){assert(v==VIEW&&b==-5&&n==37);called();}
static void layers(NVNtextureView *v,int b,int n){assert(v==VIEW&&b==23&&n==-7);called();}
static void format(NVNtextureView *v,NVNformat f){assert(v==VIEW&&(int)f==-37);called();}
static void target(NVNtextureView *v,NVNtextureTarget f){assert(v==VIEW&&(int)f==9);called();}
static void depth(NVNtextureView *v,NVNtextureDepthStencilMode f){assert(v==VIEW&&(int)f==-9);called();}
static void swizzle(NVNtextureView *v,NVNtextureSwizzle r,NVNtextureSwizzle g,NVNtextureSwizzle b,NVNtextureSwizzle a)
{assert(v==VIEW&&(int)r==3&&(int)g==1&&(int)b==-2&&(int)a==7);called();}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
    assert(!strcmp(name,names[which]));fputs("lookup_called\n",stderr);if(mode==5)return NULL;
#define CASE(i,type,fn) case i:{type exact=fn;return(PFNNVNGENERICFUNCPTRPROC)exact;}
    switch(which){CASE(0,PFNNVNTEXTUREPOOLREGISTERTEXTUREPROC,reg) CASE(1,PFNNVNCOMMANDBUFFERSETTEXTUREPOOLPROC,set_pool) CASE(2,PFNNVNCOMMANDBUFFERBINDSEPARATETEXTUREPROC,bind) CASE(3,PFNNVNCOMMANDBUFFERBINDSEPARATETEXTURESPROC,binds) CASE(4,PFNNVNTEXTUREVIEWSETDEFAULTSPROC,defaults) CASE(5,PFNNVNTEXTUREVIEWSETLEVELSPROC,levels) CASE(6,PFNNVNTEXTUREVIEWSETLAYERSPROC,layers) CASE(7,PFNNVNTEXTUREVIEWSETFORMATPROC,format) CASE(8,PFNNVNTEXTUREVIEWSETTARGETPROC,target) CASE(9,PFNNVNTEXTUREVIEWSETDEPTHSTENCILMODEPROC,depth) CASE(10,PFNNVNTEXTUREVIEWSETSWIZZLEPROC,swizzle)}
#undef CASE
    assert(0);return NULL;
}
static void invoke(void)
{
    PFNNVNGENERICFUNCPTRPROC f=nn_d2r_nvn_get_proc_address(NULL,names[which]);errno=E2BIG;
    if(mode==3)diagnostic=EBUSY;
    switch(which){
    case 0:((PFNNVNTEXTUREPOOLREGISTERTEXTUREPROC)f)(POOL,-17,TEXTURE,VIEW);break;
    case 1:((PFNNVNCOMMANDBUFFERSETTEXTUREPOOLPROC)f)(COMMAND,POOL);break;
    case 2:((PFNNVNCOMMANDBUFFERBINDSEPARATETEXTUREPROC)f)(COMMAND,NVN_SHADER_STAGE_FRAGMENT,11,(NVNseparateTextureHandle){UINT64_C(0xfedcba9800100101)});break;
    case 3:((PFNNVNCOMMANDBUFFERBINDSEPARATETEXTURESPROC)f)(COMMAND,NVN_SHADER_STAGE_FRAGMENT,expected_first,expected_count,expected_array);break;
    case 4:((PFNNVNTEXTUREVIEWSETDEFAULTSPROC)f)(VIEW);break;
    case 5:((PFNNVNTEXTUREVIEWSETLEVELSPROC)f)(VIEW,-5,37);break;
    case 6:((PFNNVNTEXTUREVIEWSETLAYERSPROC)f)(VIEW,23,-7);break;
    case 7:((PFNNVNTEXTUREVIEWSETFORMATPROC)f)(VIEW,(NVNformat)-37);break;
    case 8:((PFNNVNTEXTUREVIEWSETTARGETPROC)f)(VIEW,(NVNtextureTarget)9);break;
    case 9:((PFNNVNTEXTUREVIEWSETDEPTHSTENCILMODEPROC)f)(VIEW,(NVNtextureDepthStencilMode)-9);break;
    case 10:((PFNNVNTEXTUREVIEWSETSWIZZLEPROC)f)(VIEW,(NVNtextureSwizzle)3,(NVNtextureSwizzle)1,(NVNtextureSwizzle)-2,(NVNtextureSwizzle)7);break;
    }
    assert(errno==EDOM&&raw_calls==1&&!diagnostic);
    if(which!=3)assert(!read_calls);
#ifdef NN_TEST_BINDING_READER
    if(which==3)assert(read_calls==((expected_count>0&&expected_count<=32&&expected_array&&(uintptr_t)expected_array<=UINTPTR_MAX-(size_t)expected_count*8)?1:0));
#else
    assert(!read_calls);
#endif
}
static unsigned checks;
static void exercise(int selected,int selected_mode,int count,const void *array)
{
    which=selected;mode=selected_mode;expected_count=count;expected_first=10;expected_array=array;
    int fd[2];assert(!pipe(fd));pid_t child=fork();assert(child>=0);
    if(!child){close(fd[0]);assert(dup2(fd[1],2)==2);close(fd[1]);invoke();_exit(0);}
    close(fd[1]);char text[32768];size_t size=0;ssize_t n;
    while((n=read(fd[0],text+size,sizeof(text)-1-size))>0)size+=(size_t)n;
    close(fd[0]);text[size]=0;int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status));
    assert(WEXITSTATUS(status)==(mode>=3&&mode<=5?78:0));checks++;
    assert(strstr(text,names[which])&&strstr(text,"guest_pc=unavailable"));checks++;
    char *trace=strstr(text,names[which]),*lookup=strstr(text,"lookup_called"),*raw=strstr(text,"raw_called");
    if(mode==3)assert(!lookup&&!raw&&strstr(text,"phase=before"));else assert(lookup&&trace<lookup);
    if(mode==4)assert(raw&&strstr(text,"phase=after"));
    if(mode==5)assert(!raw&&strstr(text,"unsupported call"));
    checks++;
    if(which==0)assert(strstr(text,"id=-17")&&strstr(text,"view_fields=opaque_not_inspected"));
    if(which==2)assert(strstr(text,"index=11")&&strstr(text,"handle=0xfedcba9800100101"));
    if(which==3){
        if(mode==3)assert(strstr(text,"reason=prior_diagnostic"));
        else if(!count)assert(strstr(text,"values=empty"));
        else if(count<0||count>32)assert(strstr(text,"reason=observation_count_limit"));
#ifdef NN_TEST_BINDING_READER
        else if(!array||(uintptr_t)array>UINTPTR_MAX-(size_t)count*8)assert(strstr(text,"reason=invalid_range"));
        else if(mode==1||mode==2)assert(strstr(text,"reason=read")&&!strstr(text,"element="));
        else assert(strstr(text,"element=0 handle=0x8000000100000101")&&(count<2||strstr(text,"element=1 handle=0x8000000100000102")));
#else
        else assert(strstr(text,"reason=reader_not_enabled"));
#endif
    }
    checks++;
}
int main(void)
{
    for(int api=0;api<11;api++)for(int m=0;m<=5;m++)exercise(api,m,2,(void *)(uintptr_t)UINT64_C(0x12340000));
    for(int count=-1;count<=33;count++)exercise(3,0,count,(void *)(uintptr_t)UINT64_C(0x12340000));
    exercise(3,0,2,NULL);exercise(3,0,2,(void *)(UINTPTR_MAX-3));
    printf("{\"passed\":true,\"checks\":%u,\"gpu_submits\":0,\"scope\":\"HOST actual typed bridge, authored driver/reader, no opaque inspection or extra getter\"}\n",checks);
}
