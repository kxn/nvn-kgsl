/* HOST only: source-declared metadata vs native allocation boundary.
 * Uses real typed adapter/core and RAM ownership backend; no GPU/game. */
#define _POSIX_C_SOURCE 200809L
#include "host_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include NN_RAW_CORE
static unsigned checks;
#define T(x) do {checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define PFN(name,type) ((type)nn_d2r_nvn_bootstrap(#name))
int main(void)
{
 NVNdeviceBuilder db={0};NVNdevice device={0};NVNtextureBuilder tb={0};NVNtexture texture={0};
 OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));T(!nn_nvn_error());
 OK(api_nvnTextureBuilderSetDevice(&tb,&device));OK(api_nvnTextureBuilderSetDefaults(&tb));
 struct object *o=find(&tb,TB);T(o->device==find(&device,DEVICE));
 T(api_nvnTextureBuilderGetTarget(&tb)==NVN_TEXTURE_TARGET_2D);
 T(!api_nvnTextureBuilderGetWidth(&tb)&&!api_nvnTextureBuilderGetHeight(&tb)&&!api_nvnTextureBuilderGetDepth(&tb));
 T(api_nvnTextureBuilderGetLevels(&tb)==1&&!api_nvnTextureBuilderGetSamples(&tb));
 T(!api_nvnTextureBuilderGetFlags(&tb)&&!api_nvnTextureBuilderGetStride(&tb)&&!api_nvnTextureBuilderGetFormat(&tb));
 /* Flags and all known declared targets/formats survive storage unchanged.
  * No shader/texture resource has been created by this metadata stage. */
 const int flags[]={0,1,8,0x10,0x40,0x50,0x201};
 for(unsigned i=0;i<sizeof(flags)/sizeof(*flags);i++){
  OK(PFN(nvnTextureBuilderSetFlags,PFNNVNTEXTUREBUILDERSETFLAGSPROC)(&tb,flags[i]));
  T(PFN(nvnTextureBuilderGetFlags,PFNNVNTEXTUREBUILDERGETFLAGSPROC)(&tb)==(NVNtextureFlags)flags[i]);
 }
 for(int target=NVN_TEXTURE_TARGET_1D;target<=NVN_TEXTURE_TARGET_BUFFER;target++){
  OK(PFN(nvnTextureBuilderSetTarget,PFNNVNTEXTUREBUILDERSETTARGETPROC)(&tb,(NVNtextureTarget)target));
  T(PFN(nvnTextureBuilderGetTarget,PFNNVNTEXTUREBUILDERGETTARGETPROC)(&tb)==(NVNtextureTarget)target);
 }
 OK(PFN(nvnTextureBuilderSetSize3D,PFNNVNTEXTUREBUILDERSETSIZE3DPROC)(&tb,128,64,8));
 T(api_nvnTextureBuilderGetWidth(&tb)==128&&api_nvnTextureBuilderGetHeight(&tb)==64&&api_nvnTextureBuilderGetDepth(&tb)==8);
 OK(api_nvnTextureBuilderSetSize2D(&tb,320,240));T(api_nvnTextureBuilderGetDepth(&tb)==1);
 OK(api_nvnTextureBuilderSetSize1D(&tb,96));T(api_nvnTextureBuilderGetWidth(&tb)==96&&api_nvnTextureBuilderGetHeight(&tb)==1&&api_nvnTextureBuilderGetDepth(&tb)==1);
 OK(api_nvnTextureBuilderSetFlags(&tb,0));OK(api_nvnTextureBuilderSetTarget(&tb,NVN_TEXTURE_TARGET_2D));
 OK(api_nvnTextureBuilderSetSize2D(&tb,128,64));OK(api_nvnTextureBuilderSetLevels(&tb,8));OK(api_nvnTextureBuilderSetSamples(&tb,4));
 OK(api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA16F));OK(api_nvnTextureBuilderSetStride(&tb,1024));
 T(api_nvnTextureBuilderGetLevels(&tb)==8&&api_nvnTextureBuilderGetSamples(&tb)==4&&api_nvnTextureBuilderGetFormat(&tb)==NVN_FORMAT_RGBA16F);
 struct texture_desc saved=o->texture;T(!api_nvnTextureBuilderGetStorageSize(&tb));T(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();
 T(!api_nvnTextureInitialize(&texture,&tb));T(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();
 T(!memcmp(&saved,&o->texture,sizeof(saved)));for(struct object *p=objects;p;p=p->next)T(p->key!=&texture);
 /* Actual typed trace precedes the first unsupported layout diagnostic; it
  * must retain the caller's values and the real failure phase. */
 int fds[2];T(!pipe(fds));pid_t pid=fork();T(pid>=0);
 if(!pid){close(fds[0]);assert(dup2(fds[1],2)==2);close(fds[1]);
  PFN(nvnTextureBuilderSetFlags,PFNNVNTEXTUREBUILDERSETFLAGSPROC)(&tb,9);
  PFN(nvnTextureBuilderSetSize2D,PFNNVNTEXTUREBUILDERSETSIZE2DPROC)(&tb,320,240);
  PFN(nvnTextureBuilderGetStorageSize,PFNNVNTEXTUREBUILDERGETSTORAGESIZEPROC)(&tb);_exit(99);
 }
 close(fds[1]);char text[8192];size_t used=0;ssize_t got;
 while(used<sizeof(text)-1&&(got=read(fds[0],text+used,sizeof(text)-1-used))>0)used+=(size_t)got;
 close(fds[0]);text[used]=0;int status;T(waitpid(pid,&status,0)==pid);T(WIFEXITED(status)&&WEXITSTATUS(status)==78);
 T(strstr(text,"nvnTextureBuilderSetFlags:")&&strstr(text,"value=0x00000009"));
 T(strstr(text,"nvnTextureBuilderSetSize2D:")&&strstr(text,"width=320 height=240"));
 T(strstr(text,"driver error: nvnTextureBuilderGetStorageSize; errno=95; phase=after"));
 T(!strstr(text,"nvnTextureBuilderGetStorageSize: result="));T(!memcmp(&saved,&o->texture,sizeof(saved)));
 /* Reused value-builder storage must not collide in the driver registry. */
 union {NVNsamplerBuilder sampler;NVNwindowBuilder window;NVNtextureBuilder texture;} reused;
 OK(api_nvnSamplerBuilderSetDevice(&reused.sampler,&device));OK(api_nvnSamplerBuilderSetDefaults(&reused.sampler));
 T(find(&reused.sampler,SB)->device==find(&device,DEVICE));
 OK(api_nvnWindowBuilderSetDefaults(&reused.window));T(find(&reused.window,WB)->device==NULL);
 OK(api_nvnWindowBuilderSetDevice(&reused.window,&device));OK(api_nvnWindowBuilderSetDefaults(&reused.window));
 T(find(&reused.window,WB)->device==find(&device,DEVICE));
 OK(api_nvnTextureBuilderSetDevice(&reused.texture,&device));OK(api_nvnTextureBuilderSetDefaults(&reused.texture));
 T(find(&reused.texture,TB)->device==find(&device,DEVICE));
 OK(api_nvnSamplerBuilderSetDevice(&reused.sampler,&device));OK(api_nvnSamplerBuilderSetDefaults(&reused.sampler));
 NVNsampler live={0};T(api_nvnSamplerInitialize(&live,&reused.sampler));T(!nn_nvn_error());
 api_nvnWindowBuilderSetDefaults((NVNwindowBuilder *)&live);T(nn_nvn_error()==EEXIST);nn_nvn_clear_error();
 T(find(&live,SAMPLER)->device==find(&device,DEVICE));OK(api_nvnSamplerFinalize(&live));
 NVNdevice dead={0};T(api_nvnDeviceInitialize(&dead,&db));OK(api_nvnSamplerBuilderSetDevice(&reused.sampler,&dead));
 OK(api_nvnDeviceFinalize(&dead));T(!api_nvnSamplerInitialize(&live,&reused.sampler));T(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
 OK(api_nvnDeviceFinalize(&device));while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"gpu_submits\":0,\"scope\":\"HOST typed texture metadata/trace and unsupported allocation boundary; no GPU/game\"}\n",checks);return 0;
}
