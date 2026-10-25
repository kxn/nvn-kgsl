/* HOST ONLY actual native resource/core/typed adapter, RAM storage backend.
 * No original modules, PM4/GPU execution, CPU image emulation or game logic. */
#define _POSIX_C_SOURCE 200809L
#include "host_bridge.h"
#include "dm/host/hle.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include NN_RAW_CORE
_Static_assert(sizeof(NVNrawStorageClass)==4,"original W0 scalar result ABI");
_Static_assert(_Generic(&api_nvnTextureGetRawStorageClass,PFNNVNTEXTUREGETRAWSTORAGECLASSPROC:1,default:0),"actual getter PFN signature");
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define CLEAN() CHECK(nn_nvn_error()==0)
#define ERROR(e) do{CHECK(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static PFNNVNTEXTUREGETRAWSTORAGECLASSPROC wrapped,raw;
static NVNtexture texture;
static void child_boundary(unsigned kind,int expected,const char*phase,const char*name)
{
 int fds[2];CHECK(!pipe(fds));pid_t pid=fork();CHECK(pid>=0);
 if(!pid){close(fds[0]);assert(dup2(fds[1],STDERR_FILENO)==STDERR_FILENO);close(fds[1]);
  if(kind==0)wrapped(NULL);
  if(kind==1){find(&texture,TEXTURE)->texture.flags=0;wrapped(&texture);}
  if(kind==2){raw(NULL);wrapped(&texture);}
  if(kind==3){PFNNVNGENERICFUNCPTRPROC unknown=nn_d2r_nvn_bootstrap("nvnQueueBuilderSetQueuePriority");unknown();}
  if(kind==4){api_nvnTextureFinalize(&texture);wrapped(&texture);}
  _exit(99);
 }
 close(fds[1]);char text[4096];size_t used=0;ssize_t got;
 while(used<sizeof(text)-1&&(got=read(fds[0],text+used,sizeof(text)-1-used))>0)used+=(size_t)got;
 close(fds[0]);text[used]=0;int status;CHECK(waitpid(pid,&status,0)==pid);
 CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==78);CHECK(strstr(text,name)&&strstr(text,phase));
 char marker[40];snprintf(marker,sizeof(marker),"errno=%d",expected);CHECK(strstr(text,marker));
 fputs(text,stdout);
}
int main(void)
{
 const hle_force_ent*hle=(const hle_force_ent*)__start_swforce;
 CHECK((size_t)(__stop_swforce-__start_swforce)==sizeof(*hle));CHECK(!strcmp(hle->name,"nvnBootstrapLoader"));
 PFNNVNBOOTSTRAPLOADERPROC boot=(PFNNVNBOOTSTRAPLOADERPROC)hle->fn;
 PFNNVNDEVICEGETPROCADDRESSPROC get=(PFNNVNDEVICEGETPROCADDRESSPROC)boot("nvnDeviceGetProcAddress");CHECK(get==nn_d2r_nvn_get_proc_address);
 wrapped=(PFNNVNTEXTUREGETRAWSTORAGECLASSPROC)get(NULL,"nvnTextureGetRawStorageClass");
 raw=(PFNNVNTEXTUREGETRAWSTORAGECLASSPROC)nvnBootstrapLoader("nvnTextureGetRawStorageClass");
 CHECK(wrapped&&raw&&wrapped!=raw);CHECK((PFNNVNGENERICFUNCPTRPROC)wrapped==boot("nvnTextureGetRawStorageClass"));
 CHECK(NVN_RAW_STORAGE_CLASS_NONE==0&&NVN_RAW_STORAGE_CLASS_LINEAR==1&&NVN_RAW_STORAGE_CLASS_COLOR_AND_ZF32==2&&NVN_RAW_STORAGE_CLASS_ZF32S8==5);
 NVNdeviceBuilder db={0};NVNdevice device={0};api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&device,&db));CLEAN();
 NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool={0};void*storage=NULL;CHECK(!posix_memalign(&storage,4096,65536));memset(storage,0xa5,65536);
 api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&device);api_nvnMemoryPoolBuilderSetFlags(&pb,0x24);api_nvnMemoryPoolBuilderSetStorage(&pb,storage,65536);CHECK(api_nvnMemoryPoolInitialize(&pool,&pb));CLEAN();
 NVNtextureBuilder tb={0};api_nvnTextureBuilderSetDefaults(&tb);api_nvnTextureBuilderSetDevice(&tb,&device);
 api_nvnTextureBuilderSetFlags(&tb,NVN_TEXTURE_FLAGS_LINEAR);api_nvnTextureBuilderSetTarget(&tb,NVN_TEXTURE_TARGET_2D);
 api_nvnTextureBuilderSetSize2D(&tb,64,64);api_nvnTextureBuilderSetLevels(&tb,1);api_nvnTextureBuilderSetSamples(&tb,0);
 api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8);api_nvnTextureBuilderSetStride(&tb,256);api_nvnTextureBuilderSetStorage(&tb,&pool,0);CLEAN();
 CHECK(api_nvnTextureInitialize(&texture,&tb));CLEAN();CHECK(get(&device,"nvnTextureGetRawStorageClass")==get(NULL,"nvnTextureGetRawStorageClass"));
 struct object*t=find(&texture,TEXTURE);struct texture_desc saved=t->texture;size_t refs=t->pool->refs;
 CHECK(raw(&texture)==NVN_RAW_STORAGE_CLASS_LINEAR);CHECK(wrapped(&texture)==NVN_RAW_STORAGE_CLASS_LINEAR);CLEAN();
 CHECK(t->pool->refs==refs&&!memcmp(&saved,&t->texture,sizeof(saved)));
 /* A real initialized native texture; unknown/tiled/MSAA metadata must never
  * get class1 merely because the resolver entry exists. Whitebox corruption
  * exercises that defensive boundary without claiming those layouts exist. */
 int*fields[]={&t->texture.flags,&t->texture.samples,&t->texture.levels,&t->texture.format};
 int bad[]={0,4,2,NVN_FORMAT_DEPTH32F};
 for(unsigned i=0;i<4;i++){int value=*fields[i];*fields[i]=bad[i];CHECK(raw(&texture)==NVN_RAW_STORAGE_CLASS_NONE);ERROR(ENOTSUP);*fields[i]=value;}
 CHECK(raw(NULL)==NVN_RAW_STORAGE_CLASS_NONE);ERROR(EINVAL);
 child_boundary(0,EINVAL,"phase=after","nvnTextureGetRawStorageClass");
 child_boundary(1,ENOTSUP,"phase=after","nvnTextureGetRawStorageClass");
 child_boundary(2,EINVAL,"phase=before","nvnTextureGetRawStorageClass");
 child_boundary(3,ENOTSUP,"phase=call","nvnQueueBuilderSetQueuePriority");
 child_boundary(4,EINVAL,"phase=after","nvnTextureGetRawStorageClass");
 api_nvnTextureFinalize(&texture);CLEAN();
 /* Distinct valid LINEAR formats/dimensions remain the same raw category,
  * independent of the format-specific Tegra storage-kind category. */
 int formats[]={NVN_FORMAT_R8,NVN_FORMAT_RG8,NVN_FORMAT_RGBA8,NVN_FORMAT_RGBA16F,NVN_FORMAT_RGBA32F};
 for(unsigned i=0;i<5;i++){
  api_nvnTextureBuilderSetTarget(&tb,NVN_TEXTURE_TARGET_1D);api_nvnTextureBuilderSetSize1D(&tb,8);api_nvnTextureBuilderSetFormat(&tb,formats[i]);
  CHECK(api_nvnTextureInitialize(&texture,&tb));CHECK(wrapped(&texture)==NVN_RAW_STORAGE_CLASS_LINEAR);CLEAN();api_nvnTextureFinalize(&texture);CLEAN();
 }
 const unsigned char*bytes=storage;for(size_t i=0;i<65536;i++)CHECK(bytes[i]==0xa5);
 api_nvnMemoryPoolFinalize(&pool);CLEAN();free(storage);api_nvnDeviceFinalize(&device);CLEAN();
 while(objects){CHECK(builder_kind(objects->kind));remove_object(objects);}CHECK(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"scope\":\"HOST ONLY actual LINEAR resources and typed RawStorageClass forwarding, HLE_FORCE, no GPU/game\",\"gpu_submits\":0,\"game_modules_executed\":false}\n",checks);
 return 0;
}
