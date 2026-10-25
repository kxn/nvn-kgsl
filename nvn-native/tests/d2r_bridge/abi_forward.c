/* HOST ABI-only mock. No native backend semantics, modules, GPU or game. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include "dm/host/hle.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned checks,calls,lookups;
static int sticky,force_missing;
static unsigned char input[16384];
static NVNmemoryPool pool;
static NVNdevice device;
static NVNtexture texture;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
int nn_nvn_error(void){return sticky;}
void nn_nvn_clear_error(void){sticky=0;}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation*out){(void)out;return ENODATA;}
int nn_nvn_get_program_source_info(const NVNprogram*p,NVNbufferAddress a,struct nn_nvn_program_source_info*out){(void)p;(void)a;(void)out;return ENODATA;}
_Static_assert(_Generic((PFNNVNBUFFERBUILDERGETMEMORYPOOLPROC)0,NVNmemoryPool*(*)(const NVNbufferBuilder*):1,default:0),"SDK X0 pointer return, never opaque aggregate/hidden X8 sret");
_Static_assert(_Generic((PFNNVNTEXTUREBUILDERGETMEMORYPOOLPROC)0,NVNmemoryPool*(*)(const NVNtextureBuilder*):1,default:0),"SDK X0 pointer return");
_Static_assert(_Generic((PFNNVNTEXTUREGETMEMORYPOOLPROC)0,NVNmemoryPool*(*)(const NVNtexture*):1,default:0),"SDK X0 pointer return");
_Static_assert(sizeof(NVNqueuePriority)==4&&sizeof(NVNrawStorageClass)==4,"W0 enum ABI");
static NVNdeviceFlagBits mock_nvnDeviceBuilderGetFlags(const NVNdeviceBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return 42;}
static NVNqueuePriority mock_nvnQueueBuilderGetQueuePriority(const NVNqueueBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return NVN_QUEUE_PRIORITY_HIGH;}
static const NVNdevice* mock_nvnQueueBuilderGetDevice(const NVNqueueBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &device;}
static int mock_nvnQueueBuilderGetFlags(const NVNqueueBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return 42;}
static NVNboolean mock_nvnQueueBuilderGetCommandMemorySize(const NVNqueueBuilder* a0, int* a1){calls++;CHECK((const void*)a0==(const void*)input);*a1=0x11223344;return 0;}
static NVNboolean mock_nvnQueueBuilderGetComputeMemorySize(const NVNqueueBuilder* a0, int* a1){calls++;CHECK((const void*)a0==(const void*)input);*a1=0x11223344;return 0;}
static NVNboolean mock_nvnQueueBuilderGetControlMemorySize(const NVNqueueBuilder* a0, int* a1){calls++;CHECK((const void*)a0==(const void*)input);*a1=0x11223344;return 0;}
static NVNboolean mock_nvnQueueBuilderGetCommandFlushThreshold(const NVNqueueBuilder* a0, int* a1){calls++;CHECK((const void*)a0==(const void*)input);*a1=0x11223344;return 0;}
static int mock_nvnQueueBuilderGetMemorySize(const NVNqueueBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return 42;}
static void* mock_nvnQueueBuilderGetMemory(const NVNqueueBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &pool;}
static const NVNdevice* mock_nvnWindowBuilderGetDevice(const NVNwindowBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &device;}
static int mock_nvnWindowBuilderGetNumTextures(const NVNwindowBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return 42;}
static const NVNtexture* mock_nvnWindowBuilderGetTexture(const NVNwindowBuilder* a0, int a1){calls++;CHECK((const void*)a0==(const void*)input);CHECK(a1==2);return &texture;}
static int mock_nvnWindowBuilderGetNumActiveTextures(const NVNwindowBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return 42;}
static int mock_nvnWindowGetNumActiveTextures(const NVNwindow* a0){calls++;CHECK((const void*)a0==(const void*)input);return 42;}
static int mock_nvnWindowGetNumTextures(const NVNwindow* a0){calls++;CHECK((const void*)a0==(const void*)input);return 42;}
static const NVNdevice* mock_nvnMemoryPoolBuilderGetDevice(const NVNmemoryPoolBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &device;}
static const NVNdevice* mock_nvnBufferBuilderGetDevice(const NVNbufferBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &device;}
static const NVNdevice* mock_nvnTextureBuilderGetDevice(const NVNtextureBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &device;}
static NVNboolean mock_nvnTextureBuilderGetPackagedTextureLayout(const NVNtextureBuilder* a0, NVNpackagedTextureLayout* a1){calls++;CHECK((const void*)a0==(const void*)input);uint32_t v=0x00000432;memcpy(a1->layout,&v,4);return 0;}
static NVNrawStorageClass mock_nvnTextureBuilderGetRawStorageClass(const NVNtextureBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return NVN_RAW_STORAGE_CLASS_LINEAR;}
static const NVNdevice* mock_nvnSamplerBuilderGetDevice(const NVNsamplerBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &device;}
static const NVNmemoryPool* mock_nvnEventBuilderGetStorage(const NVNeventBuilder* a0, int64_t* a1){calls++;CHECK((const void*)a0==(const void*)input);*a1=INT64_C(0x1122334455667788);return &pool;}
static const NVNmemoryPool* mock_nvnEventBuilderGetMemoryPool(const NVNeventBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &pool;}
static int64_t mock_nvnEventBuilderGetMemoryOffset(const NVNeventBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return INT64_C(0x1122334455667788);}
static const NVNmemoryPool* mock_nvnEventGetMemoryPool(const NVNevent* a0){calls++;CHECK((const void*)a0==(const void*)input);return &pool;}
static int64_t mock_nvnEventGetMemoryOffset(const NVNevent* a0){calls++;CHECK((const void*)a0==(const void*)input);return INT64_C(0x1122334455667788);}
static NVNmemoryPool* mock_nvnBufferBuilderGetMemoryPool(const NVNbufferBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &pool;}
static NVNmemoryPool* mock_nvnTextureBuilderGetMemoryPool(const NVNtextureBuilder* a0){calls++;CHECK((const void*)a0==(const void*)input);return &pool;}
static NVNmemoryPool* mock_nvnTextureGetMemoryPool(const NVNtexture* a0){calls++;CHECK((const void*)a0==(const void*)input);return &pool;}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char*name)
{lookups++;if(force_missing)return NULL;
 if(!strcmp(name,"nvnDeviceBuilderGetFlags"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnDeviceBuilderGetFlags;
 if(!strcmp(name,"nvnQueueBuilderGetQueuePriority"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnQueueBuilderGetQueuePriority;
 if(!strcmp(name,"nvnQueueBuilderGetDevice"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnQueueBuilderGetDevice;
 if(!strcmp(name,"nvnQueueBuilderGetFlags"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnQueueBuilderGetFlags;
 if(!strcmp(name,"nvnQueueBuilderGetCommandMemorySize"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnQueueBuilderGetCommandMemorySize;
 if(!strcmp(name,"nvnQueueBuilderGetComputeMemorySize"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnQueueBuilderGetComputeMemorySize;
 if(!strcmp(name,"nvnQueueBuilderGetControlMemorySize"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnQueueBuilderGetControlMemorySize;
 if(!strcmp(name,"nvnQueueBuilderGetCommandFlushThreshold"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnQueueBuilderGetCommandFlushThreshold;
 if(!strcmp(name,"nvnQueueBuilderGetMemorySize"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnQueueBuilderGetMemorySize;
 if(!strcmp(name,"nvnQueueBuilderGetMemory"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnQueueBuilderGetMemory;
 if(!strcmp(name,"nvnWindowBuilderGetDevice"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnWindowBuilderGetDevice;
 if(!strcmp(name,"nvnWindowBuilderGetNumTextures"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnWindowBuilderGetNumTextures;
 if(!strcmp(name,"nvnWindowBuilderGetTexture"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnWindowBuilderGetTexture;
 if(!strcmp(name,"nvnWindowBuilderGetNumActiveTextures"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnWindowBuilderGetNumActiveTextures;
 if(!strcmp(name,"nvnWindowGetNumActiveTextures"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnWindowGetNumActiveTextures;
 if(!strcmp(name,"nvnWindowGetNumTextures"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnWindowGetNumTextures;
 if(!strcmp(name,"nvnMemoryPoolBuilderGetDevice"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnMemoryPoolBuilderGetDevice;
 if(!strcmp(name,"nvnBufferBuilderGetDevice"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnBufferBuilderGetDevice;
 if(!strcmp(name,"nvnTextureBuilderGetDevice"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnTextureBuilderGetDevice;
 if(!strcmp(name,"nvnTextureBuilderGetPackagedTextureLayout"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnTextureBuilderGetPackagedTextureLayout;
 if(!strcmp(name,"nvnTextureBuilderGetRawStorageClass"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnTextureBuilderGetRawStorageClass;
 if(!strcmp(name,"nvnSamplerBuilderGetDevice"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnSamplerBuilderGetDevice;
 if(!strcmp(name,"nvnEventBuilderGetStorage"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnEventBuilderGetStorage;
 if(!strcmp(name,"nvnEventBuilderGetMemoryPool"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnEventBuilderGetMemoryPool;
 if(!strcmp(name,"nvnEventBuilderGetMemoryOffset"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnEventBuilderGetMemoryOffset;
 if(!strcmp(name,"nvnEventGetMemoryPool"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnEventGetMemoryPool;
 if(!strcmp(name,"nvnEventGetMemoryOffset"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnEventGetMemoryOffset;
 if(!strcmp(name,"nvnBufferBuilderGetMemoryPool"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnBufferBuilderGetMemoryPool;
 if(!strcmp(name,"nvnTextureBuilderGetMemoryPool"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnTextureBuilderGetMemoryPool;
 if(!strcmp(name,"nvnTextureGetMemoryPool"))return (PFNNVNGENERICFUNCPTRPROC)mock_nvnTextureGetMemoryPool;
 return NULL;
}
static void fatal_case(unsigned n)
{
 int f[2];CHECK(!pipe(f));pid_t pid=fork();CHECK(pid>=0);
 if(!pid){close(f[0]);assert(dup2(f[1],STDERR_FILENO)==STDERR_FILENO);close(f[1]);
  if(n==0){PFNNVNGENERICFUNCPTRPROC x=nn_d2r_nvn_bootstrap("nvnQueueBuilderSetQueuePriority");x();}
  if(n==1){force_missing=1;((PFNNVNTEXTUREBUILDERGETDEVICEPROC)nn_d2r_nvn_bootstrap("nvnTextureBuilderGetDevice"))((const NVNtextureBuilder*)input);}
  if(n==2){sticky=ENOTSUP;((PFNNVNTEXTUREBUILDERGETDEVICEPROC)nn_d2r_nvn_bootstrap("nvnTextureBuilderGetDevice"))((const NVNtextureBuilder*)input);}
  if(n==3)nn_d2r_nvn_bootstrap("nvnDefinitelyUnknown");
  _exit(99);
 }
 close(f[1]);char text[4096];ssize_t at=0,nr;while((nr=read(f[0],text+at,sizeof(text)-1-(size_t)at))>0)at+=nr;close(f[0]);text[at]=0;int status;CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==78);
 const char*expected[]={"unknown ABI call: nvnQueueBuilderSetQueuePriority","unsupported call: nvnTextureBuilderGetDevice","phase=before","unknown lookup: nvnDefinitelyUnknown"};
 CHECK(strstr(text,expected[n]));fputs(text,stdout);
}
int main(void)
{
 const hle_force_ent*hle=(const hle_force_ent*)__start_swforce;
 CHECK((size_t)(__stop_swforce-__start_swforce)==sizeof(*hle));CHECK(!strcmp(hle->name,"nvnBootstrapLoader"));
 PFNNVNBOOTSTRAPLOADERPROC boot=(PFNNVNBOOTSTRAPLOADERPROC)hle->fn;
 PFNNVNDEVICEGETPROCADDRESSPROC get=(PFNNVNDEVICEGETPROCADDRESSPROC)boot("nvnDeviceGetProcAddress");CHECK(get==nn_d2r_nvn_get_proc_address);
 {
 PFNNVNDEVICEBUILDERGETFLAGSPROC fn=(PFNNVNDEVICEBUILDERGETFLAGSPROC)get(NULL,"nvnDeviceBuilderGetFlags");CHECK(fn&&fn!=mock_nvnDeviceBuilderGetFlags);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnDeviceBuilderGetFlags"));
 CHECK(fn((const NVNdeviceBuilder*)input)==42);CHECK(sticky==0);
 }
 {
 PFNNVNQUEUEBUILDERGETQUEUEPRIORITYPROC fn=(PFNNVNQUEUEBUILDERGETQUEUEPRIORITYPROC)get(NULL,"nvnQueueBuilderGetQueuePriority");CHECK(fn&&fn!=mock_nvnQueueBuilderGetQueuePriority);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnQueueBuilderGetQueuePriority"));
 CHECK(fn((const NVNqueueBuilder*)input)==NVN_QUEUE_PRIORITY_HIGH);CHECK(sticky==0);
 }
 {
 PFNNVNQUEUEBUILDERGETDEVICEPROC fn=(PFNNVNQUEUEBUILDERGETDEVICEPROC)get(NULL,"nvnQueueBuilderGetDevice");CHECK(fn&&fn!=mock_nvnQueueBuilderGetDevice);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnQueueBuilderGetDevice"));
 CHECK(fn((const NVNqueueBuilder*)input)==&device);CHECK(sticky==0);
 }
 {
 PFNNVNQUEUEBUILDERGETFLAGSPROC fn=(PFNNVNQUEUEBUILDERGETFLAGSPROC)get(NULL,"nvnQueueBuilderGetFlags");CHECK(fn&&fn!=mock_nvnQueueBuilderGetFlags);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnQueueBuilderGetFlags"));
 CHECK(fn((const NVNqueueBuilder*)input)==42);CHECK(sticky==0);
 }
 {
 PFNNVNQUEUEBUILDERGETCOMMANDMEMORYSIZEPROC fn=(PFNNVNQUEUEBUILDERGETCOMMANDMEMORYSIZEPROC)get(NULL,"nvnQueueBuilderGetCommandMemorySize");CHECK(fn&&fn!=mock_nvnQueueBuilderGetCommandMemorySize);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnQueueBuilderGetCommandMemorySize"));
 struct{uint32_t before;int value;uint32_t after;}out={0xaabbccdd,0,0x778899aa};
 CHECK(fn((const NVNqueueBuilder*)input,&out.value)==0);CHECK(sticky==0);CHECK(out.value==0x11223344&&out.before==0xaabbccdd&&out.after==0x778899aa);
 }
 {
 PFNNVNQUEUEBUILDERGETCOMPUTEMEMORYSIZEPROC fn=(PFNNVNQUEUEBUILDERGETCOMPUTEMEMORYSIZEPROC)get(NULL,"nvnQueueBuilderGetComputeMemorySize");CHECK(fn&&fn!=mock_nvnQueueBuilderGetComputeMemorySize);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnQueueBuilderGetComputeMemorySize"));
 struct{uint32_t before;int value;uint32_t after;}out={0xaabbccdd,0,0x778899aa};
 CHECK(fn((const NVNqueueBuilder*)input,&out.value)==0);CHECK(sticky==0);CHECK(out.value==0x11223344&&out.before==0xaabbccdd&&out.after==0x778899aa);
 }
 {
 PFNNVNQUEUEBUILDERGETCONTROLMEMORYSIZEPROC fn=(PFNNVNQUEUEBUILDERGETCONTROLMEMORYSIZEPROC)get(NULL,"nvnQueueBuilderGetControlMemorySize");CHECK(fn&&fn!=mock_nvnQueueBuilderGetControlMemorySize);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnQueueBuilderGetControlMemorySize"));
 struct{uint32_t before;int value;uint32_t after;}out={0xaabbccdd,0,0x778899aa};
 CHECK(fn((const NVNqueueBuilder*)input,&out.value)==0);CHECK(sticky==0);CHECK(out.value==0x11223344&&out.before==0xaabbccdd&&out.after==0x778899aa);
 }
 {
 PFNNVNQUEUEBUILDERGETCOMMANDFLUSHTHRESHOLDPROC fn=(PFNNVNQUEUEBUILDERGETCOMMANDFLUSHTHRESHOLDPROC)get(NULL,"nvnQueueBuilderGetCommandFlushThreshold");CHECK(fn&&fn!=mock_nvnQueueBuilderGetCommandFlushThreshold);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnQueueBuilderGetCommandFlushThreshold"));
 struct{uint32_t before;int value;uint32_t after;}out={0xaabbccdd,0,0x778899aa};
 CHECK(fn((const NVNqueueBuilder*)input,&out.value)==0);CHECK(sticky==0);CHECK(out.value==0x11223344&&out.before==0xaabbccdd&&out.after==0x778899aa);
 }
 {
 PFNNVNQUEUEBUILDERGETMEMORYSIZEPROC fn=(PFNNVNQUEUEBUILDERGETMEMORYSIZEPROC)get(NULL,"nvnQueueBuilderGetMemorySize");CHECK(fn&&fn!=mock_nvnQueueBuilderGetMemorySize);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnQueueBuilderGetMemorySize"));
 CHECK(fn((const NVNqueueBuilder*)input)==42);CHECK(sticky==0);
 }
 {
 PFNNVNQUEUEBUILDERGETMEMORYPROC fn=(PFNNVNQUEUEBUILDERGETMEMORYPROC)get(NULL,"nvnQueueBuilderGetMemory");CHECK(fn&&fn!=mock_nvnQueueBuilderGetMemory);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnQueueBuilderGetMemory"));
 CHECK(fn((const NVNqueueBuilder*)input)==&pool);CHECK(sticky==0);
 }
 {
 PFNNVNWINDOWBUILDERGETDEVICEPROC fn=(PFNNVNWINDOWBUILDERGETDEVICEPROC)get(NULL,"nvnWindowBuilderGetDevice");CHECK(fn&&fn!=mock_nvnWindowBuilderGetDevice);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnWindowBuilderGetDevice"));
 CHECK(fn((const NVNwindowBuilder*)input)==&device);CHECK(sticky==0);
 }
 {
 PFNNVNWINDOWBUILDERGETNUMTEXTURESPROC fn=(PFNNVNWINDOWBUILDERGETNUMTEXTURESPROC)get(NULL,"nvnWindowBuilderGetNumTextures");CHECK(fn&&fn!=mock_nvnWindowBuilderGetNumTextures);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnWindowBuilderGetNumTextures"));
 CHECK(fn((const NVNwindowBuilder*)input)==42);CHECK(sticky==0);
 }
 {
 PFNNVNWINDOWBUILDERGETTEXTUREPROC fn=(PFNNVNWINDOWBUILDERGETTEXTUREPROC)get(NULL,"nvnWindowBuilderGetTexture");CHECK(fn&&fn!=mock_nvnWindowBuilderGetTexture);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnWindowBuilderGetTexture"));
 CHECK(fn((const NVNwindowBuilder*)input,2)==&texture);CHECK(sticky==0);
 }
 {
 PFNNVNWINDOWBUILDERGETNUMACTIVETEXTURESPROC fn=(PFNNVNWINDOWBUILDERGETNUMACTIVETEXTURESPROC)get(NULL,"nvnWindowBuilderGetNumActiveTextures");CHECK(fn&&fn!=mock_nvnWindowBuilderGetNumActiveTextures);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnWindowBuilderGetNumActiveTextures"));
 CHECK(fn((const NVNwindowBuilder*)input)==42);CHECK(sticky==0);
 }
 {
 PFNNVNWINDOWGETNUMACTIVETEXTURESPROC fn=(PFNNVNWINDOWGETNUMACTIVETEXTURESPROC)get(NULL,"nvnWindowGetNumActiveTextures");CHECK(fn&&fn!=mock_nvnWindowGetNumActiveTextures);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnWindowGetNumActiveTextures"));
 CHECK(fn((const NVNwindow*)input)==42);CHECK(sticky==0);
 }
 {
 PFNNVNWINDOWGETNUMTEXTURESPROC fn=(PFNNVNWINDOWGETNUMTEXTURESPROC)get(NULL,"nvnWindowGetNumTextures");CHECK(fn&&fn!=mock_nvnWindowGetNumTextures);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnWindowGetNumTextures"));
 CHECK(fn((const NVNwindow*)input)==42);CHECK(sticky==0);
 }
 {
 PFNNVNMEMORYPOOLBUILDERGETDEVICEPROC fn=(PFNNVNMEMORYPOOLBUILDERGETDEVICEPROC)get(NULL,"nvnMemoryPoolBuilderGetDevice");CHECK(fn&&fn!=mock_nvnMemoryPoolBuilderGetDevice);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnMemoryPoolBuilderGetDevice"));
 CHECK(fn((const NVNmemoryPoolBuilder*)input)==&device);CHECK(sticky==0);
 }
 {
 PFNNVNBUFFERBUILDERGETDEVICEPROC fn=(PFNNVNBUFFERBUILDERGETDEVICEPROC)get(NULL,"nvnBufferBuilderGetDevice");CHECK(fn&&fn!=mock_nvnBufferBuilderGetDevice);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnBufferBuilderGetDevice"));
 CHECK(fn((const NVNbufferBuilder*)input)==&device);CHECK(sticky==0);
 }
 {
 PFNNVNTEXTUREBUILDERGETDEVICEPROC fn=(PFNNVNTEXTUREBUILDERGETDEVICEPROC)get(NULL,"nvnTextureBuilderGetDevice");CHECK(fn&&fn!=mock_nvnTextureBuilderGetDevice);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnTextureBuilderGetDevice"));
 CHECK(fn((const NVNtextureBuilder*)input)==&device);CHECK(sticky==0);
 }
 {
 PFNNVNTEXTUREBUILDERGETPACKAGEDTEXTURELAYOUTPROC fn=(PFNNVNTEXTUREBUILDERGETPACKAGEDTEXTURELAYOUTPROC)get(NULL,"nvnTextureBuilderGetPackagedTextureLayout");CHECK(fn&&fn!=mock_nvnTextureBuilderGetPackagedTextureLayout);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnTextureBuilderGetPackagedTextureLayout"));
 struct{uint64_t before;NVNpackagedTextureLayout value;uint64_t after;}out;memset(&out,0xa5,sizeof(out));
 CHECK(fn((const NVNtextureBuilder*)input,&out.value)==0);CHECK(sticky==0);uint32_t v=0;memcpy(&v,out.value.layout,4);CHECK(v==0x432);CHECK(out.before==UINT64_C(0xa5a5a5a5a5a5a5a5)&&out.after==UINT64_C(0xa5a5a5a5a5a5a5a5));
 }
 {
 PFNNVNTEXTUREBUILDERGETRAWSTORAGECLASSPROC fn=(PFNNVNTEXTUREBUILDERGETRAWSTORAGECLASSPROC)get(NULL,"nvnTextureBuilderGetRawStorageClass");CHECK(fn&&fn!=mock_nvnTextureBuilderGetRawStorageClass);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnTextureBuilderGetRawStorageClass"));
 CHECK(fn((const NVNtextureBuilder*)input)==NVN_RAW_STORAGE_CLASS_LINEAR);CHECK(sticky==0);
 }
 {
 PFNNVNSAMPLERBUILDERGETDEVICEPROC fn=(PFNNVNSAMPLERBUILDERGETDEVICEPROC)get(NULL,"nvnSamplerBuilderGetDevice");CHECK(fn&&fn!=mock_nvnSamplerBuilderGetDevice);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnSamplerBuilderGetDevice"));
 CHECK(fn((const NVNsamplerBuilder*)input)==&device);CHECK(sticky==0);
 }
 {
 PFNNVNEVENTBUILDERGETSTORAGEPROC fn=(PFNNVNEVENTBUILDERGETSTORAGEPROC)get(NULL,"nvnEventBuilderGetStorage");CHECK(fn&&fn!=mock_nvnEventBuilderGetStorage);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnEventBuilderGetStorage"));
 struct{uint64_t before;int64_t value;uint64_t after;}out={0xaabbccdd,0,0x778899aa};
 CHECK(fn((const NVNeventBuilder*)input,&out.value)==&pool);CHECK(sticky==0);CHECK(out.value==INT64_C(0x1122334455667788)&&out.before==0xaabbccdd&&out.after==0x778899aa);
 }
 {
 PFNNVNEVENTBUILDERGETMEMORYPOOLPROC fn=(PFNNVNEVENTBUILDERGETMEMORYPOOLPROC)get(NULL,"nvnEventBuilderGetMemoryPool");CHECK(fn&&fn!=mock_nvnEventBuilderGetMemoryPool);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnEventBuilderGetMemoryPool"));
 CHECK(fn((const NVNeventBuilder*)input)==&pool);CHECK(sticky==0);
 }
 {
 PFNNVNEVENTBUILDERGETMEMORYOFFSETPROC fn=(PFNNVNEVENTBUILDERGETMEMORYOFFSETPROC)get(NULL,"nvnEventBuilderGetMemoryOffset");CHECK(fn&&fn!=mock_nvnEventBuilderGetMemoryOffset);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnEventBuilderGetMemoryOffset"));
 CHECK(fn((const NVNeventBuilder*)input)==INT64_C(0x1122334455667788));CHECK(sticky==0);
 }
 {
 PFNNVNEVENTGETMEMORYPOOLPROC fn=(PFNNVNEVENTGETMEMORYPOOLPROC)get(NULL,"nvnEventGetMemoryPool");CHECK(fn&&fn!=mock_nvnEventGetMemoryPool);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnEventGetMemoryPool"));
 CHECK(fn((const NVNevent*)input)==&pool);CHECK(sticky==0);
 }
 {
 PFNNVNEVENTGETMEMORYOFFSETPROC fn=(PFNNVNEVENTGETMEMORYOFFSETPROC)get(NULL,"nvnEventGetMemoryOffset");CHECK(fn&&fn!=mock_nvnEventGetMemoryOffset);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnEventGetMemoryOffset"));
 CHECK(fn((const NVNevent*)input)==INT64_C(0x1122334455667788));CHECK(sticky==0);
 }
 {
 PFNNVNBUFFERBUILDERGETMEMORYPOOLPROC fn=(PFNNVNBUFFERBUILDERGETMEMORYPOOLPROC)get(NULL,"nvnBufferBuilderGetMemoryPool");CHECK(fn&&fn!=mock_nvnBufferBuilderGetMemoryPool);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnBufferBuilderGetMemoryPool"));
 CHECK(fn((const NVNbufferBuilder*)input)==&pool);CHECK(sticky==0);
 }
 {
 PFNNVNTEXTUREBUILDERGETMEMORYPOOLPROC fn=(PFNNVNTEXTUREBUILDERGETMEMORYPOOLPROC)get(NULL,"nvnTextureBuilderGetMemoryPool");CHECK(fn&&fn!=mock_nvnTextureBuilderGetMemoryPool);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnTextureBuilderGetMemoryPool"));
 CHECK(fn((const NVNtextureBuilder*)input)==&pool);CHECK(sticky==0);
 }
 {
 PFNNVNTEXTUREGETMEMORYPOOLPROC fn=(PFNNVNTEXTUREGETMEMORYPOOLPROC)get(NULL,"nvnTextureGetMemoryPool");CHECK(fn&&fn!=mock_nvnTextureGetMemoryPool);CHECK((PFNNVNGENERICFUNCPTRPROC)fn==get(&device,"nvnTextureGetMemoryPool"));
 CHECK(fn((const NVNtexture*)input)==&pool);CHECK(sticky==0);
 }
 CHECK(calls==30&&lookups==30);
 for(unsigned n=0;n<4;n++)fatal_case(n);
 printf("{\"passed\":true,\"checks\":%u,\"forward_calls\":%u,\"scope\":\"HOST typed ABI mock only; no native backend/GPU/game\"}\n",checks,calls);return 0;
}
