/* HOST RAM ONLY: real provider bridge/core; memory backend never runs GPU work. */
#define _POSIX_C_SOURCE 200809L
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include NVN_TEST_OFFLINE_HEADER
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;assert(x);}while(0)
#define LOAD(name,type) type name=(type)nvnBootstrapLoader(#name);CHECK(name)
int main(void)
{
 LOAD(nvnDeviceBuilderSetDefaults,PFNNVNDEVICEBUILDERSETDEFAULTSPROC);LOAD(nvnDeviceFinalize,PFNNVNDEVICEFINALIZEPROC);LOAD(nvnProgramInitialize,PFNNVNPROGRAMINITIALIZEPROC);LOAD(nvnProgramFinalize,PFNNVNPROGRAMFINALIZEPROC);LOAD(nvnProgramSetShaders,PFNNVNPROGRAMSETSHADERSPROC);
 LOAD(nvnMemoryPoolBuilderSetDefaults,PFNNVNMEMORYPOOLBUILDERSETDEFAULTSPROC);LOAD(nvnMemoryPoolBuilderSetDevice,PFNNVNMEMORYPOOLBUILDERSETDEVICEPROC);LOAD(nvnMemoryPoolBuilderSetFlags,PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC);LOAD(nvnMemoryPoolBuilderSetStorage,PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC);LOAD(nvnMemoryPoolInitialize,PFNNVNMEMORYPOOLINITIALIZEPROC);LOAD(nvnMemoryPoolFinalize,PFNNVNMEMORYPOOLFINALIZEPROC);LOAD(nvnMemoryPoolGetBufferAddress,PFNNVNMEMORYPOOLGETBUFFERADDRESSPROC);
 PFNNVNDEVICEINITIALIZEPROC initialize=(PFNNVNDEVICEINITIALIZEPROC)nn_d2r_nvn_bootstrap("nvnDeviceInitialize");CHECK(initialize);
 PFNNVNPROGRAMSETSHADERSPROC wrapped=(PFNNVNPROGRAMSETSHADERSPROC)nn_d2r_nvn_bootstrap("nvnProgramSetShaders");CHECK(wrapped);
 unsigned matches=0,misses=0;
 for(unsigned cycle=0;cycle<2;++cycle){
  NVNdeviceBuilder db;NVNdevice device;nvnDeviceBuilderSetDefaults(&db);CHECK(initialize(&device,&db));
  NVNprogram program;CHECK(nvnProgramInitialize(&program,&device));
  for(size_t i=0;i<NN_OFFLINE_COMPUTE_COUNT;++i){
   const struct nn_nvn_compiled_cs *entry=nn_offline_compute_entries[i];size_t size=(entry->source_bytes+128+4095)&~(size_t)4095;
   void *storage=NULL;CHECK(!posix_memalign(&storage,4096,size));memset(storage,0xa5,size);memcpy((char *)storage+128,entry->source_code,entry->source_bytes);
   unsigned char *control=malloc(entry->control_bytes);CHECK(control);memcpy(control,entry->source_control,entry->control_bytes);
   NVNmemoryPoolBuilder pb;NVNmemoryPool pool;nvnMemoryPoolBuilderSetDefaults(&pb);nvnMemoryPoolBuilderSetDevice(&pb,&device);nvnMemoryPoolBuilderSetFlags(&pb,0x64);nvnMemoryPoolBuilderSetStorage(&pb,storage,size);CHECK(nvnMemoryPoolInitialize(&pool,&pb));
   NVNshaderData data={nvnMemoryPoolGetBufferAddress(&pool)+128,control};CHECK(wrapped(&program,1,&data));CHECK(!nn_nvn_error());++matches;
   ((unsigned char *)storage)[128+entry->source_bytes-1]^=1;CHECK(!nvnProgramSetShaders(&program,1,&data));CHECK(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();++misses;((unsigned char *)storage)[128+entry->source_bytes-1]^=1;
   const size_t offsets[]={0,0x700,0x87f,0x880,0x881,0x882,0x883,0x884,0x885,0x886,0x887,0x888,0x889,0x88a,0x88b,0x88c,0x88d,0x88e,0x88f,0x890,0x891,0x892,0x893,0x894,0x895,0x896,0x897};
   for(unsigned j=0;j<sizeof(offsets)/sizeof(offsets[0]);++j){control[offsets[j]]^=1;CHECK(!nvnProgramSetShaders(&program,1,&data));CHECK(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();++misses;control[offsets[j]]^=1;}
   NVNshaderData pair[2]={data,data};CHECK(!nvnProgramSetShaders(&program,2,pair));CHECK(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();++misses;
   CHECK(wrapped(&program,1,&data));CHECK(!nn_nvn_error());++matches;
   nvnMemoryPoolFinalize(&pool);CHECK(!nn_nvn_error());free(storage);free(control);
  }
  nvnDeviceFinalize(&device);CHECK(nn_nvn_error()==EBUSY);nn_nvn_clear_error();nvnProgramFinalize(&program);CHECK(!nn_nvn_error());nvnDeviceFinalize(&device);CHECK(!nn_nvn_error());
 }
 printf("{\"scope\":\"HOST RAM ONLY\",\"checks\":%u,\"entries\":%zu,\"automatic_registration_devices\":2,\"full_input_matches\":%u,\"full_input_misses\":%u,\"gpu_submits\":0}\n",checks,(size_t)NN_OFFLINE_COMPUTE_COUNT,matches,misses);
 return 0;
}
