/* HOST RAM ONLY: actual core and existing memory backend; no GPU execution. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/nvn.h"
#include "nvn_native/kgsl.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LOAD(name,type) type name=(type)nvnBootstrapLoader(#name);assert(name)
static unsigned checks;
#define CHECK(x) do{++checks;assert(x);}while(0)
int main(void)
{
 LOAD(nvnDeviceBuilderSetDefaults,PFNNVNDEVICEBUILDERSETDEFAULTSPROC);LOAD(nvnDeviceInitialize,PFNNVNDEVICEINITIALIZEPROC);LOAD(nvnDeviceFinalize,PFNNVNDEVICEFINALIZEPROC);
 LOAD(nvnProgramInitialize,PFNNVNPROGRAMINITIALIZEPROC);LOAD(nvnProgramFinalize,PFNNVNPROGRAMFINALIZEPROC);
 LOAD(nvnMemoryPoolBuilderSetDefaults,PFNNVNMEMORYPOOLBUILDERSETDEFAULTSPROC);LOAD(nvnMemoryPoolBuilderSetDevice,PFNNVNMEMORYPOOLBUILDERSETDEVICEPROC);LOAD(nvnMemoryPoolBuilderSetStorage,PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC);LOAD(nvnMemoryPoolBuilderSetFlags,PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC);LOAD(nvnMemoryPoolInitialize,PFNNVNMEMORYPOOLINITIALIZEPROC);LOAD(nvnMemoryPoolFinalize,PFNNVNMEMORYPOOLFINALIZEPROC);LOAD(nvnMemoryPoolGetBufferAddress,PFNNVNMEMORYPOOLGETBUFFERADDRESSPROC);
 LOAD(nvnDeviceGetInteger,PFNNVNDEVICEGETINTEGERPROC);
 NVNdeviceBuilder db;NVNdevice device,other;NVNprogram program,foreign;
 nvnDeviceBuilderSetDefaults(&db);CHECK(nvnDeviceInitialize(&device,&db));CHECK(nvnDeviceInitialize(&other,&db));CHECK(nvnProgramInitialize(&program,&device));CHECK(nvnProgramInitialize(&foreign,&other));
 struct nn_nvn_program_source_info info,want;memset(&want,0xa5,sizeof(want));info=want;
 CHECK(nn_nvn_get_program_source_info(NULL,0,&info)==EINVAL);CHECK(!memcmp(&info,&want,sizeof(info)));CHECK(nn_nvn_get_program_source_info(&program,0,NULL)==EINVAL);CHECK(nn_nvn_get_program_source_info((void *)(uintptr_t)1,0,&info)==ENOENT);CHECK(nn_nvn_get_program_source_info((void *)&db,0,&info)==ENOENT);CHECK(!nn_nvn_error());
 const int flags[]={0x24,0x22,0xa1,0x14,0x12};
 for(unsigned i=0;i<sizeof(flags)/sizeof(flags[0]);++i){
  void *storage=NULL;CHECK(!posix_memalign(&storage,4096,8192));memset(storage,0x5a,8192);
  NVNmemoryPoolBuilder pb;NVNmemoryPool pool;nvnMemoryPoolBuilderSetDefaults(&pb);nvnMemoryPoolBuilderSetDevice(&pb,&device);nvnMemoryPoolBuilderSetStorage(&pb,storage,8192);nvnMemoryPoolBuilderSetFlags(&pb,flags[i]);CHECK(nvnMemoryPoolInitialize(&pool,&pb));
  NVNbufferAddress base=nvnMemoryPoolGetBufferAddress(&pool);CHECK(base);
  CHECK(!nn_nvn_get_program_source_info(&program,base+123,&info));CHECK(info.pool==&pool&&info.flags==flags[i]&&info.offset==123&&info.available==8192-123);CHECK(info.cpu_readable==(unsigned)(i<2));CHECK(info.policy==(i>=3?NN_NVN_GPU_UNCACHED_UNQUALIFIED:NN_NVN_GPU_POOL_CACHED));
  info=want;CHECK(nn_nvn_get_program_source_info(&foreign,base,&info)==ENOENT);CHECK(!memcmp(&info,&want,sizeof(info)));CHECK(nn_nvn_get_program_source_info(&program,base+8192,&info)==ENOENT);CHECK(nn_nvn_get_program_source_info(&program,UINT64_MAX,&info)==ENOENT);CHECK(!memcmp(&info,&want,sizeof(info)));CHECK(!nn_nvn_error());
  int output=0;nvnDeviceGetInteger(&device,(NVNdeviceInfo)-1,&output);CHECK(nn_nvn_error()==ENOTSUP);CHECK(!nn_nvn_get_program_source_info(&program,base,&info));CHECK(nn_nvn_error()==ENOTSUP);CHECK(nn_nvn_get_program_source_info(&foreign,base,&info)==ENOENT);CHECK(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();
  nvnMemoryPoolFinalize(&pool);CHECK(!nn_nvn_error());info=want;CHECK(nn_nvn_get_program_source_info(&program,base,&info)==ENOENT);CHECK(!memcmp(&info,&want,sizeof(info)));free(storage);
 }
 nvnProgramFinalize(&program);CHECK(!nn_nvn_error());CHECK(nn_nvn_get_program_source_info(&program,0,&info)==ENOENT);nvnProgramFinalize(&foreign);nvnDeviceFinalize(&device);nvnDeviceFinalize(&other);CHECK(!nn_nvn_error());
 printf("PASS HOST RAM ONLY: %u source identity checks; 5 pool policies, device isolation/lifetime/bounds/sticky/output guards; no GPU/game\n",checks);return 0;
}
