/* Actual nvn.c APIs, host-only memory backend: no GPU or guest execution. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
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
static NVNdevice device;
static void run_child(int which, const char *api, int error, const char *phase)
{
 int descriptors[2];assert(!pipe(descriptors));pid_t pid=fork();assert(pid>=0);
 if(!pid){close(descriptors[0]);assert(dup2(descriptors[1],STDERR_FILENO)==STDERR_FILENO);close(descriptors[1]);
  if(which==0){PFNNVNDEVICEGETINTEGERPROC f=(PFNNVNDEVICEGETINTEGERPROC)nn_d2r_nvn_get_proc_address(NULL,"nvnDeviceGetInteger");int output=0x11223344;f(&device,(NVNdeviceInfo)-1,&output);}
  if(which==1){PFNNVNDEVICEBUILDERSETFLAGSPROC f=(PFNNVNDEVICEBUILDERSETFLAGSPROC)nn_d2r_nvn_bootstrap("nvnDeviceBuilderSetFlags");NVNdeviceBuilder b;((PFNNVNDEVICEBUILDERSETDEFAULTSPROC)nn_d2r_nvn_bootstrap("nvnDeviceBuilderSetDefaults"))(&b);f(&b,0x115);}
  if(which==2){PFNNVNGENERICFUNCPTRPROC f=nn_d2r_nvn_bootstrap("nvnQueueBuilderSetQueuePriority");f();}
  if(which==3)nn_d2r_nvn_bootstrap("nvnUnknownEntryMustNotSucceed");
  if(which==4)nn_d2r_nvn_get_proc_address(NULL,NULL);
  if(which==5){PFNNVNDEVICEGETINTEGERPROC wrapped=(PFNNVNDEVICEGETINTEGERPROC)nn_d2r_nvn_bootstrap("nvnDeviceGetInteger");PFNNVNDEVICEGETINTEGERPROC raw=(PFNNVNDEVICEGETINTEGERPROC)nvnBootstrapLoader("nvnDeviceGetInteger");int output=0;raw(&device,(NVNdeviceInfo)-1,&output);assert(nn_nvn_error()==ENOTSUP);wrapped(&device,NVN_DEVICE_INFO_TEXTURE_DESCRIPTOR_SIZE,&output);}
  if(which==6){NVNmemoryPoolBuilder b;NVNmemoryPool pool;
   ((PFNNVNMEMORYPOOLBUILDERSETDEFAULTSPROC)nn_d2r_nvn_bootstrap("nvnMemoryPoolBuilderSetDefaults"))(&b);
   ((PFNNVNMEMORYPOOLBUILDERSETDEVICEPROC)nn_d2r_nvn_bootstrap("nvnMemoryPoolBuilderSetDevice"))(&b,&device);
   ((PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC)nn_d2r_nvn_bootstrap("nvnMemoryPoolBuilderSetFlags"))(&b,0x12);
   ((PFNNVNMEMORYPOOLINITIALIZEPROC)nn_d2r_nvn_bootstrap("nvnMemoryPoolInitialize"))(&pool,&b);}
  _exit(91);
 }
 close(descriptors[1]);char text[2048];size_t n=0;ssize_t got;
 while(n<sizeof(text)-1&&(got=read(descriptors[0],text+n,sizeof(text)-1-n))>0)n+=(size_t)got;
 close(descriptors[0]);text[n]=0;int status;assert(waitpid(pid,&status,0)==pid);assert(WIFEXITED(status)&&WEXITSTATUS(status)==78);
 assert(strstr(text,api)&&strstr(text,phase));char expected[64];snprintf(expected,sizeof(expected),"errno=%d",error);assert(strstr(text,expected));fputs(text,stdout);
 if(which==6){assert(strstr(text,"flags=0x00000012")&&strstr(text,"guest_pc=unavailable"));assert(!strstr(text,"nvnMemoryPoolInitialize: result="));}
}
#define ENTRY(name,type) type name=(type)nn_d2r_nvn_bootstrap(#name);assert(name)
int main(void)
{
 const hle_force_ent *hle=(const hle_force_ent *)__start_swforce;assert((size_t)(__stop_swforce-__start_swforce)==sizeof(*hle));assert(!strcmp(hle->name,"nvnBootstrapLoader"));
 PFNNVNBOOTSTRAPLOADERPROC boot=(PFNNVNBOOTSTRAPLOADERPROC)hle->fn;
 PFNNVNDEVICEGETPROCADDRESSPROC get=(PFNNVNDEVICEGETPROCADDRESSPROC)boot("nvnDeviceGetProcAddress");assert(get==nn_d2r_nvn_get_proc_address);
 assert(get(NULL,"nvnDeviceGetProcAddress")== (PFNNVNGENERICFUNCPTRPROC)get);
 ENTRY(nvnDeviceBuilderSetDefaults,PFNNVNDEVICEBUILDERSETDEFAULTSPROC);ENTRY(nvnDeviceInitialize,PFNNVNDEVICEINITIALIZEPROC);ENTRY(nvnDeviceFinalize,PFNNVNDEVICEFINALIZEPROC);ENTRY(nvnDeviceGetInteger,PFNNVNDEVICEGETINTEGERPROC);
 assert((PFNNVNGENERICFUNCPTRPROC)nvnDeviceGetInteger!=nvnBootstrapLoader("nvnDeviceGetInteger"));assert(get(NULL,"nvnDeviceGetInteger")== (PFNNVNGENERICFUNCPTRPROC)nvnDeviceGetInteger);
 NVNdeviceBuilder db;nvnDeviceBuilderSetDefaults(&db);((PFNNVNDEVICEBUILDERSETFLAGSPROC)nn_d2r_nvn_bootstrap("nvnDeviceBuilderSetFlags"))(&db,0x100);assert(nvnDeviceInitialize(&device,&db));assert(get(&device,"nvnDeviceGetInteger")==get(NULL,"nvnDeviceGetInteger"));int property=-1;nvnDeviceGetInteger(&device,NVN_DEVICE_INFO_TEXTURE_DESCRIPTOR_SIZE,&property);assert(property==32&&!nn_nvn_error());
 ENTRY(nvnSamplerBuilderSetDevice,PFNNVNSAMPLERBUILDERSETDEVICEPROC);ENTRY(nvnSamplerBuilderSetDefaults,PFNNVNSAMPLERBUILDERSETDEFAULTSPROC);ENTRY(nvnSamplerBuilderSetLodClamp,PFNNVNSAMPLERBUILDERSETLODCLAMPPROC);ENTRY(nvnSamplerBuilderGetLodClamp,PFNNVNSAMPLERBUILDERGETLODCLAMPPROC);ENTRY(nvnSamplerBuilderGetLodBias,PFNNVNSAMPLERBUILDERGETLODBIASPROC);ENTRY(nvnSamplerBuilderGetMaxAnisotropy,PFNNVNSAMPLERBUILDERGETMAXANISOTROPYPROC);
 NVNsamplerBuilder sb;nvnSamplerBuilderSetDevice(&sb,&device);nvnSamplerBuilderSetDefaults(&sb);float lo=0,hi=0;nvnSamplerBuilderGetLodClamp(&sb,&lo,&hi);assert(lo==0&&hi==1000);nvnSamplerBuilderSetLodClamp(&sb,0,0);nvnSamplerBuilderGetLodClamp(&sb,&lo,&hi);assert(lo==0&&hi==0);assert(nvnSamplerBuilderGetLodBias(&sb)==0&&nvnSamplerBuilderGetMaxAnisotropy(&sb)==1);
 ENTRY(nvnDeviceGetSeparateTextureHandle,PFNNVNDEVICEGETSEPARATETEXTUREHANDLEPROC);ENTRY(nvnDeviceGetSeparateSamplerHandle,PFNNVNDEVICEGETSEPARATESAMPLERHANDLEPROC);assert(nvnDeviceGetSeparateTextureHandle(&device,0xfedcb).value==UINT64_C(0x1000fedcb));assert(nvnDeviceGetSeparateSamplerHandle(&device,0xfed).value==((uint64_t)0xfed<<20));
 ENTRY(nvnMemoryPoolBuilderSetDefaults,PFNNVNMEMORYPOOLBUILDERSETDEFAULTSPROC);ENTRY(nvnMemoryPoolBuilderSetDevice,PFNNVNMEMORYPOOLBUILDERSETDEVICEPROC);ENTRY(nvnMemoryPoolBuilderSetFlags,PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC);ENTRY(nvnMemoryPoolBuilderSetStorage,PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC);ENTRY(nvnMemoryPoolInitialize,PFNNVNMEMORYPOOLINITIALIZEPROC);ENTRY(nvnMemoryPoolMap,PFNNVNMEMORYPOOLMAPPROC);ENTRY(nvnMemoryPoolFinalize,PFNNVNMEMORYPOOLFINALIZEPROC);
 NVNmemoryPoolBuilder pb;NVNmemoryPool pool;void *storage=NULL;assert(!posix_memalign(&storage,4096,4096));nvnMemoryPoolBuilderSetDefaults(&pb);nvnMemoryPoolBuilderSetDevice(&pb,&device);nvnMemoryPoolBuilderSetFlags(&pb,0x24);nvnMemoryPoolBuilderSetStorage(&pb,storage,4096);assert(nvnMemoryPoolInitialize(&pool,&pb));assert(nvnMemoryPoolMap(&pool)==storage&&!nn_nvn_error());nvnMemoryPoolFinalize(&pool);free(storage);assert(!nn_nvn_error());
 run_child(0,"nvnDeviceGetInteger",ENOTSUP,"phase=after");run_child(1,"nvnDeviceBuilderSetFlags",ENOTSUP,"phase=after");run_child(2,"nvnQueueBuilderSetQueuePriority",ENOTSUP,"phase=call");run_child(3,"nvnUnknownEntryMustNotSucceed",ENOSYS,"phase=lookup");run_child(4,"(null)",EINVAL,"phase=lookup");run_child(5,"nvnDeviceGetInteger",ENOTSUP,"phase=before");
 /* GPU_UNCACHED is now representable; missing storage is invalid before any access. */
 run_child(6,"nvnMemoryPoolInitialize",EINVAL,"phase=after");
 assert(nvnBootstrapLoader("nvnDeviceBuilderSetFlags")!=NULL);nvnDeviceFinalize(&device);assert(!nn_nvn_error());puts("PASS actual-driver APIs, typed resolver routing, valid getters/zero/floats/struct handles, unsupported args and stale diagnostic fatal, unsupported flags/unknown ABI/name fatal, original HLE_FORCE; HOST ONLY");
}
