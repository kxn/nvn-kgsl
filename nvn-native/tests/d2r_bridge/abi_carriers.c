/* Synthetic public-ABI carrier tests; not GPU, NVN conformance, or game execution. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int diagnostics;
/* Synthetic driver has no native pool-policy diagnostic. */
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out)
{ (void)out; return ENODATA; }
int nn_nvn_error(void){return diagnostics;}
void nn_nvn_clear_error(void){diagnostics=0;}
static NVNboolean compare(const NVNtexture *a,const NVNtexture *b){assert(a==(void*)0x1234&&b==(void*)0x5678);return 0;}
static float bias(const NVNsamplerBuilder *b){assert(b==(void*)0x1234);return 3.25f;}
static void lod(const NVNsamplerBuilder *b,float *lo,float *hi){assert(b==(void*)0x1234);*lo=-10.5f;*hi=17.25f;}
static NVNseparateTextureHandle handle(const NVNdevice *d,int i){assert(d==(void*)0x1234&&i==-177);return (NVNseparateTextureHandle){UINT64_C(0xfedcba9876543210)};}
static NVNmemoryPool aggregate(const NVNbufferBuilder *b){assert(b==(void*)0x1234);NVNmemoryPool r;for(size_t i=0;i<sizeof(r);i++)((unsigned char*)&r)[i]=(unsigned char)(i^0x9d);return r;}
static void bind(NVNcommandBuffer *c,NVNshaderStage stage,int index,NVNbufferAddress address,size_t size){assert(c==(void*)0x1234&&stage==NVN_SHADER_STAGE_FRAGMENT&&index==-1234567&&address==UINT64_C(0xfedcba9876543210)&&size==UINT64_C(0x123456789));}
static void clear(NVNcommandBuffer *c,float depth,NVNboolean mask,int stencil,int stencil_mask){assert(c==(void*)0x1234&&depth==0.125f&&mask==0xa5&&stencil==-1234567&&stencil_mask==0x6f4e3322);}
static void callback(NVNcommandBuffer *c,NVNcommandBufferMemoryEvent event,size_t size,void *user){assert(c==(void*)0x1234&&(int)event==77&&size==UINT64_C(0x123456789)&&user==(void*)0x5678);}
static PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC get_callback(const NVNcommandBuffer *c){assert(c==(void*)0x1234);return callback;}
static void must_not_forward(void){abort();}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name){
#define TEST(n,f,type) if(!strcmp(name,n)){type check=f;return (PFNNVNGENERICFUNCPTRPROC)check;}
 TEST("nvnTextureCompare",compare,PFNNVNTEXTURECOMPAREPROC)
 TEST("nvnSamplerBuilderGetLodBias",bias,PFNNVNSAMPLERBUILDERGETLODBIASPROC)
 TEST("nvnSamplerBuilderGetLodClamp",lod,PFNNVNSAMPLERBUILDERGETLODCLAMPPROC)
 TEST("nvnDeviceGetSeparateTextureHandle",handle,PFNNVNDEVICEGETSEPARATETEXTUREHANDLEPROC)
 TEST("nvnBufferBuilderGetMemoryPool",aggregate,PFNNVNBUFFERBUILDERGETMEMORYPOOLPROC)
 TEST("nvnCommandBufferBindUniformBuffer",bind,PFNNVNCOMMANDBUFFERBINDUNIFORMBUFFERPROC)
 TEST("nvnCommandBufferClearDepthStencil",clear,PFNNVNCOMMANDBUFFERCLEARDEPTHSTENCILPROC)
 TEST("nvnCommandBufferGetMemoryCallback",get_callback,PFNNVNCOMMANDBUFFERGETMEMORYCALLBACKPROC)
 if(!strcmp(name,"nvnTextureGetRawStorageClass"))return must_not_forward;
 if(!strcmp(name,"nvnDeviceGetProcAddress"))return must_not_forward;
 return NULL;
#undef TEST
}
#define GET(n,type) type n=(type)nn_d2r_nvn_bootstrap(#n);assert(n)
int main(void){
 GET(nvnTextureCompare,PFNNVNTEXTURECOMPAREPROC);assert(!nvnTextureCompare((void*)0x1234,(void*)0x5678));
 GET(nvnSamplerBuilderGetLodBias,PFNNVNSAMPLERBUILDERGETLODBIASPROC);assert(nvnSamplerBuilderGetLodBias((void*)0x1234)==3.25f);
 GET(nvnSamplerBuilderGetLodClamp,PFNNVNSAMPLERBUILDERGETLODCLAMPPROC);float lo=0,hi=0;nvnSamplerBuilderGetLodClamp((void*)0x1234,&lo,&hi);assert(lo==-10.5f&&hi==17.25f);
 GET(nvnDeviceGetSeparateTextureHandle,PFNNVNDEVICEGETSEPARATETEXTUREHANDLEPROC);assert(nvnDeviceGetSeparateTextureHandle((void*)0x1234,-177).value==UINT64_C(0xfedcba9876543210));
 GET(nvnBufferBuilderGetMemoryPool,PFNNVNBUFFERBUILDERGETMEMORYPOOLPROC);NVNmemoryPool pool=nvnBufferBuilderGetMemoryPool((void*)0x1234);for(size_t i=0;i<sizeof(pool);i++)assert(((unsigned char*)&pool)[i]==(unsigned char)(i^0x9d));
 GET(nvnCommandBufferBindUniformBuffer,PFNNVNCOMMANDBUFFERBINDUNIFORMBUFFERPROC);nvnCommandBufferBindUniformBuffer((void*)0x1234,NVN_SHADER_STAGE_FRAGMENT,-1234567,UINT64_C(0xfedcba9876543210),UINT64_C(0x123456789));
 GET(nvnCommandBufferClearDepthStencil,PFNNVNCOMMANDBUFFERCLEARDEPTHSTENCILPROC);nvnCommandBufferClearDepthStencil((void*)0x1234,0.125f,0xa5,-1234567,0x6f4e3322);
 GET(nvnCommandBufferGetMemoryCallback,PFNNVNCOMMANDBUFFERGETMEMORYCALLBACKPROC);PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC cb=nvnCommandBufferGetMemoryCallback((void*)0x1234);assert(cb==callback);cb((void*)0x1234,(NVNcommandBufferMemoryEvent)77,UINT64_C(0x123456789),(void*)0x5678);
 PFNNVNDEVICEGETPROCADDRESSPROC get=(PFNNVNDEVICEGETPROCADDRESSPROC)nn_d2r_nvn_bootstrap("nvnDeviceGetProcAddress");assert(get==nn_d2r_nvn_get_proc_address&&get(NULL,"nvnDeviceGetProcAddress")==(PFNNVNGENERICFUNCPTRPROC)get&&get(NULL,"nvnTextureCompare")==(PFNNVNGENERICFUNCPTRPROC)nvnTextureCompare);
 pid_t pid=fork();assert(pid>=0);if(!pid){nn_d2r_nvn_bootstrap("nvnTextureGetRawStorageClass")();_exit(91);}int status;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==78);
 puts("PASS synthetic public-ABI carriers: bool false preserved; float scalar/out params; all64bit argument/handle bits; 256B sret aggregate; mixed integer/FP/NVNboolean args; callback return; resolver cannot escape; unknown ABI never forwarded. HOST ABI check only");
}
