/* SPDX-License-Identifier: MIT
 * Authored public-NVN caller for original color VS/FS. No legacy image,
 * internal pipeline encoder, game execution or CPU image conversion. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/pipeline_nvn.h"
#include "nvn_native/texture_source.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_NVN_PIPELINE_QUALIFICATION_HEADER
#ifndef NVN_NVN_PIPELINE_COLOR
#define NVN_NVN_PIPELINE_COLOR 0
#endif
#if NVN_NVN_PIPELINE_COLOR
#include "color_target_cases.h"
#endif
#ifndef NVN_NVN_PIPELINE_SAMPLED
#define NVN_NVN_PIPELINE_SAMPLED 0
#endif
#ifndef NVN_NVN_PIPELINE_BUFFER
#define NVN_NVN_PIPELINE_BUFFER 0
#endif
#ifndef NVN_NVN_PIPELINE_BC
#define NVN_NVN_PIPELINE_BC 0
#endif
#ifndef NVN_NVN_PIPELINE_BC_MIPS
#define NVN_NVN_PIPELINE_BC_MIPS 0
#endif
#if NVN_NVN_PIPELINE_SAMPLED
/* Authored static texels are supplied by the normal qualifier header. */
#elif NVN_NVN_PIPELINE_BUFFER
#include "nvn_pipeline_buffer_cases.h"
#elif NVN_NVN_PIPELINE_BC
#ifndef NVN_NVN_PIPELINE_BC_SRGB
#define NVN_NVN_PIPELINE_BC_SRGB 0
#endif
#if NVN_NVN_PIPELINE_BC_MIPS
#include "bc_mip_cases.h"
#elif NVN_NVN_PIPELINE_BC_SRGB
#include "bc_srgb_cases.h"
#else
#include "bc_sampled_cases.h"
#endif
#elif !NVN_NVN_PIPELINE_COLOR
#include "nvn_pipeline_oracle.h"
#endif
#ifndef NVN_NVN_PIPELINE_GOB
#define NVN_NVN_PIPELINE_GOB 0
#endif
#ifndef NVN_NVN_PIPELINE_RUNTIME_VIEWS
#define NVN_NVN_PIPELINE_RUNTIME_VIEWS 0
#endif
#ifndef NVN_NVN_PIPELINE_CALL
#define NVN_NVN_PIPELINE_CALL 0
#endif
#if NVN_NVN_PIPELINE_CALL || NVN_NVN_PIPELINE_BUFFER || NVN_NVN_PIPELINE_COLOR
#if !defined(NVN_NVN_PIPELINE_HELPERS_ONLY) && !NVN_NVN_PIPELINE_COLOR && (NVN_NVN_PIPELINE_BC || NVN_NVN_PIPELINE_GOB || NVN_NVN_PIPELINE_RUNTIME_VIEWS)
#error "Call qualifier initially requires the normal finite LINEAR64 profile"
#endif
#define CALL_API_LIST(X) \
 X(nvnColorStateSetDefaults,PFNNVNCOLORSTATESETDEFAULTSPROC) X(nvnCommandBufferBindColorState,PFNNVNCOMMANDBUFFERBINDCOLORSTATEPROC) \
 X(nvnChannelMaskStateSetDefaults,PFNNVNCHANNELMASKSTATESETDEFAULTSPROC) X(nvnCommandBufferBindChannelMaskState,PFNNVNCOMMANDBUFFERBINDCHANNELMASKSTATEPROC) \
 X(nvnPolygonStateSetDefaults,PFNNVNPOLYGONSTATESETDEFAULTSPROC) X(nvnCommandBufferBindPolygonState,PFNNVNCOMMANDBUFFERBINDPOLYGONSTATEPROC) \
 X(nvnDepthStencilStateSetDefaults,PFNNVNDEPTHSTENCILSTATESETDEFAULTSPROC) X(nvnCommandBufferBindDepthStencilState,PFNNVNCOMMANDBUFFERBINDDEPTHSTENCILSTATEPROC) \
 X(nvnMultisampleStateSetDefaults,PFNNVNMULTISAMPLESTATESETDEFAULTSPROC) X(nvnCommandBufferBindMultisampleState,PFNNVNCOMMANDBUFFERBINDMULTISAMPLESTATEPROC) \
 X(nvnBlendStateSetDefaults,PFNNVNBLENDSTATESETDEFAULTSPROC) X(nvnCommandBufferBindBlendState,PFNNVNCOMMANDBUFFERBINDBLENDSTATEPROC) \
 X(nvnCommandBufferSetDepthBounds,PFNNVNCOMMANDBUFFERSETDEPTHBOUNDSPROC) X(nvnCommandBufferCallCommands,PFNNVNCOMMANDBUFFERCALLCOMMANDSPROC)
#else
#define CALL_API_LIST(X)
#endif
#if NVN_NVN_PIPELINE_BC || NVN_NVN_PIPELINE_BUFFER || NVN_NVN_PIPELINE_SAMPLED
#define BC_API_LIST(X) \
 X(nvnTexturePoolInitialize,PFNNVNTEXTUREPOOLINITIALIZEPROC) X(nvnTexturePoolFinalize,PFNNVNTEXTUREPOOLFINALIZEPROC) \
 X(nvnTexturePoolRegisterTexture,PFNNVNTEXTUREPOOLREGISTERTEXTUREPROC) \
 X(nvnSamplerBuilderSetDefaults,PFNNVNSAMPLERBUILDERSETDEFAULTSPROC) X(nvnSamplerBuilderSetDevice,PFNNVNSAMPLERBUILDERSETDEVICEPROC) \
 X(nvnSamplerBuilderSetMinMagFilter,PFNNVNSAMPLERBUILDERSETMINMAGFILTERPROC) X(nvnSamplerBuilderSetWrapMode,PFNNVNSAMPLERBUILDERSETWRAPMODEPROC) \
 X(nvnSamplerInitialize,PFNNVNSAMPLERINITIALIZEPROC) X(nvnSamplerFinalize,PFNNVNSAMPLERFINALIZEPROC) \
 X(nvnSamplerPoolInitialize,PFNNVNSAMPLERPOOLINITIALIZEPROC) X(nvnSamplerPoolFinalize,PFNNVNSAMPLERPOOLFINALIZEPROC) \
 X(nvnSamplerPoolRegisterSampler,PFNNVNSAMPLERPOOLREGISTERSAMPLERPROC) \
 X(nvnDeviceGetSeparateTextureHandle,PFNNVNDEVICEGETSEPARATETEXTUREHANDLEPROC) X(nvnDeviceGetSeparateSamplerHandle,PFNNVNDEVICEGETSEPARATESAMPLERHANDLEPROC) \
 X(nvnCommandBufferSetTexturePool,PFNNVNCOMMANDBUFFERSETTEXTUREPOOLPROC) X(nvnCommandBufferSetSamplerPool,PFNNVNCOMMANDBUFFERSETSAMPLERPOOLPROC) \
 X(nvnCommandBufferBindSeparateTextures,PFNNVNCOMMANDBUFFERBINDSEPARATETEXTURESPROC) X(nvnCommandBufferBindSeparateSamplers,PFNNVNCOMMANDBUFFERBINDSEPARATESAMPLERSPROC)
#else
#define BC_API_LIST(X)
#endif
#if NVN_NVN_PIPELINE_BC_MIPS || NVN_NVN_PIPELINE_SAMPLED
#define MIP_API_LIST(X) \
 X(nvnTextureBuilderSetLevels,PFNNVNTEXTUREBUILDERSETLEVELSPROC) \
 X(nvnTextureViewSetDefaults,PFNNVNTEXTUREVIEWSETDEFAULTSPROC) X(nvnTextureViewSetLevels,PFNNVNTEXTUREVIEWSETLEVELSPROC)
#else
#define MIP_API_LIST(X)
#endif
#if NVN_NVN_PIPELINE_BUFFER
#define BUFFER_API_LIST(X) \
 X(nvnTextureBuilderSetTarget,PFNNVNTEXTUREBUILDERSETTARGETPROC) X(nvnTextureBuilderSetSize1D,PFNNVNTEXTUREBUILDERSETSIZE1DPROC) \
 X(nvnTextureBuilderGetStorageAlignment,PFNNVNTEXTUREBUILDERGETSTORAGEALIGNMENTPROC) X(nvnCommandBufferCopyCommands,PFNNVNCOMMANDBUFFERCOPYCOMMANDSPROC)
#else
#define BUFFER_API_LIST(X)
#endif
#if NVN_NVN_PIPELINE_COLOR
#define COLOR_API_LIST(X) \
 X(nvnColorStateSetBlendEnable,PFNNVNCOLORSTATESETBLENDENABLEPROC) \
 X(nvnChannelMaskStateSetChannelMask,PFNNVNCHANNELMASKSTATESETCHANNELMASKPROC) \
 X(nvnBlendStateSetBlendFunc,PFNNVNBLENDSTATESETBLENDFUNCPROC) \
 X(nvnCommandBufferCopyCommands,PFNNVNCOMMANDBUFFERCOPYCOMMANDSPROC)
#else
#define COLOR_API_LIST(X)
#endif
#define API_LIST(X) \
 COLOR_API_LIST(X) \
 BUFFER_API_LIST(X) \
 MIP_API_LIST(X) \
 CALL_API_LIST(X) \
 BC_API_LIST(X) \
 X(nvnDeviceBuilderSetDefaults,PFNNVNDEVICEBUILDERSETDEFAULTSPROC) \
 X(nvnDeviceInitialize,PFNNVNDEVICEINITIALIZEPROC) X(nvnDeviceFinalize,PFNNVNDEVICEFINALIZEPROC) \
 X(nvnDeviceSetDepthMode,PFNNVNDEVICESETDEPTHMODEPROC) X(nvnDeviceSetWindowOriginMode,PFNNVNDEVICESETWINDOWORIGINMODEPROC) \
 X(nvnDeviceFinalizeCommandHandle,PFNNVNDEVICEFINALIZECOMMANDHANDLEPROC) \
 X(nvnQueueBuilderSetDefaults,PFNNVNQUEUEBUILDERSETDEFAULTSPROC) X(nvnQueueBuilderSetDevice,PFNNVNQUEUEBUILDERSETDEVICEPROC) \
 X(nvnQueueInitialize,PFNNVNQUEUEINITIALIZEPROC) X(nvnQueueFinalize,PFNNVNQUEUEFINALIZEPROC) \
 X(nvnQueueSubmitCommands,PFNNVNQUEUESUBMITCOMMANDSPROC) X(nvnQueueFinish,PFNNVNQUEUEFINISHPROC) \
 X(nvnMemoryPoolBuilderSetDefaults,PFNNVNMEMORYPOOLBUILDERSETDEFAULTSPROC) \
 X(nvnMemoryPoolBuilderSetDevice,PFNNVNMEMORYPOOLBUILDERSETDEVICEPROC) \
 X(nvnMemoryPoolBuilderSetStorage,PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC) \
 X(nvnMemoryPoolBuilderSetFlags,PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC) \
 X(nvnMemoryPoolInitialize,PFNNVNMEMORYPOOLINITIALIZEPROC) X(nvnMemoryPoolFinalize,PFNNVNMEMORYPOOLFINALIZEPROC) \
 X(nvnMemoryPoolMap,PFNNVNMEMORYPOOLMAPPROC) X(nvnMemoryPoolGetBufferAddress,PFNNVNMEMORYPOOLGETBUFFERADDRESSPROC) \
 X(nvnMemoryPoolFlushMappedRange,PFNNVNMEMORYPOOLFLUSHMAPPEDRANGEPROC) \
 X(nvnMemoryPoolInvalidateMappedRange,PFNNVNMEMORYPOOLINVALIDATEMAPPEDRANGEPROC) \
 X(nvnProgramInitialize,PFNNVNPROGRAMINITIALIZEPROC) X(nvnProgramSetShaders,PFNNVNPROGRAMSETSHADERSPROC) \
 X(nvnProgramFinalize,PFNNVNPROGRAMFINALIZEPROC) X(nvnTextureBuilderSetDefaults,PFNNVNTEXTUREBUILDERSETDEFAULTSPROC) \
 X(nvnTextureBuilderSetDevice,PFNNVNTEXTUREBUILDERSETDEVICEPROC) X(nvnTextureBuilderSetFlags,PFNNVNTEXTUREBUILDERSETFLAGSPROC) \
 X(nvnTextureBuilderSetSize2D,PFNNVNTEXTUREBUILDERSETSIZE2DPROC) X(nvnTextureBuilderSetFormat,PFNNVNTEXTUREBUILDERSETFORMATPROC) \
 X(nvnTextureBuilderSetStride,PFNNVNTEXTUREBUILDERSETSTRIDEPROC) X(nvnTextureBuilderSetStorage,PFNNVNTEXTUREBUILDERSETSTORAGEPROC) \
 X(nvnTextureBuilderGetStorageSize,PFNNVNTEXTUREBUILDERGETSTORAGESIZEPROC) \
 X(nvnTextureGetTextureAddress,PFNNVNTEXTUREGETTEXTUREADDRESSPROC) X(nvnTextureGetFlags,PFNNVNTEXTUREGETFLAGSPROC) X(nvnTextureInitialize,PFNNVNTEXTUREINITIALIZEPROC) X(nvnTextureFinalize,PFNNVNTEXTUREFINALIZEPROC) \
 X(nvnCommandBufferInitialize,PFNNVNCOMMANDBUFFERINITIALIZEPROC) X(nvnCommandBufferFinalize,PFNNVNCOMMANDBUFFERFINALIZEPROC) \
 X(nvnCommandBufferAddCommandMemory,PFNNVNCOMMANDBUFFERADDCOMMANDMEMORYPROC) \
 X(nvnCommandBufferAddControlMemory,PFNNVNCOMMANDBUFFERADDCONTROLMEMORYPROC) \
 X(nvnCommandBufferBeginRecording,PFNNVNCOMMANDBUFFERBEGINRECORDINGPROC) \
 X(nvnCommandBufferEndRecording,PFNNVNCOMMANDBUFFERENDRECORDINGPROC) \
 X(nvnCommandBufferBindProgram,PFNNVNCOMMANDBUFFERBINDPROGRAMPROC) \
 X(nvnCommandBufferBindUniformBuffer,PFNNVNCOMMANDBUFFERBINDUNIFORMBUFFERPROC) \
 X(nvnCommandBufferBindVertexBuffer,PFNNVNCOMMANDBUFFERBINDVERTEXBUFFERPROC) \
 X(nvnCommandBufferBindVertexAttribState,PFNNVNCOMMANDBUFFERBINDVERTEXATTRIBSTATEPROC) \
 X(nvnCommandBufferBindVertexStreamState,PFNNVNCOMMANDBUFFERBINDVERTEXSTREAMSTATEPROC) \
 X(nvnCommandBufferSetRenderTargets,PFNNVNCOMMANDBUFFERSETRENDERTARGETSPROC) \
 X(nvnCommandBufferSetViewport,PFNNVNCOMMANDBUFFERSETVIEWPORTPROC) X(nvnCommandBufferSetScissor,PFNNVNCOMMANDBUFFERSETSCISSORPROC) \
 X(nvnCommandBufferSetDepthRange,PFNNVNCOMMANDBUFFERSETDEPTHRANGEPROC) \
 X(nvnCommandBufferDrawArrays,PFNNVNCOMMANDBUFFERDRAWARRAYSPROC) \
 X(nvnCommandBufferDrawElementsBaseVertex,PFNNVNCOMMANDBUFFERDRAWELEMENTSBASEVERTEXPROC) \
 X(nvnCommandBufferDrawArraysIndirect,PFNNVNCOMMANDBUFFERDRAWARRAYSINDIRECTPROC) \
 X(nvnCommandBufferMultiDrawElementsIndirectCount,PFNNVNCOMMANDBUFFERMULTIDRAWELEMENTSINDIRECTCOUNTPROC) \
 X(nvnVertexAttribStateSetDefaults,PFNNVNVERTEXATTRIBSTATESETDEFAULTSPROC) \
 X(nvnVertexAttribStateSetFormat,PFNNVNVERTEXATTRIBSTATESETFORMATPROC) \
 X(nvnVertexAttribStateSetStreamIndex,PFNNVNVERTEXATTRIBSTATESETSTREAMINDEXPROC) \
 X(nvnVertexStreamStateSetDefaults,PFNNVNVERTEXSTREAMSTATESETDEFAULTSPROC) \
 X(nvnVertexStreamStateSetStride,PFNNVNVERTEXSTREAMSTATESETSTRIDEPROC)
struct api {
#define FIELD(name,type) type name;
 API_LIST(FIELD)
#undef FIELD
};
enum {UNIFORM,VERTEX,COLOR,INDEX,ARGUMENT,SOURCE,ARENA,TARGET,
#if NVN_NVN_PIPELINE_BUFFER
BUFFER_ALT,
#endif
POOLS};
struct pool {NVNmemoryPool key;NVNmemoryPoolBuilder builder;unsigned char *memory,*shadow;size_t bytes;uint64_t address;int live;};
struct owner {
 struct api a;NVNdeviceBuilder db;NVNqueueBuilder qb;NVNtextureBuilder tb;
 NVNdevice device;NVNqueue queue;NVNprogram program;NVNtexture texture;NVNcommandBuffer command;
 int device_live,queue_live,program_live,texture_live,command_live,pending;
 NVNcommandHandle handle;struct pool pools[POOLS];_Alignas(16)unsigned char control[8192];
 NVNshaderData shaders[2];const struct nn_offline_graphics_stage *stages[2];
 int reader_enabled;struct owner *next;
#if NVN_NVN_PIPELINE_COLOR
 NVNcommandHandle copied;
#endif
#if NVN_NVN_PIPELINE_CALL || NVN_NVN_PIPELINE_BUFFER || NVN_NVN_PIPELINE_COLOR
 NVNcommandBuffer call_parent;int call_parent_live;NVNcommandHandle call_child;_Alignas(16)unsigned char call_control[8192];
#endif
#if NVN_NVN_PIPELINE_BC || NVN_NVN_PIPELINE_BUFFER || NVN_NVN_PIPELINE_SAMPLED
 NVNtexturePool texture_pool;NVNsamplerPool sampler_pool;NVNsamplerBuilder sb;
 NVNsampler sampler;NVNtexture sample;NVNtextureBuilder sample_builder;
#if NVN_NVN_PIPELINE_BC_MIPS || NVN_NVN_PIPELINE_SAMPLED
 NVNtextureView mip_view;
#endif
 int texture_pool_live,sampler_pool_live,sampler_live,sample_live,metadata_flags;
#if NVN_NVN_PIPELINE_BUFFER
 NVNtextureBuilder buffer_builder;NVNtexture buffer;int buffer_live;NVNcommandHandle copied;
#endif
#endif
};
static struct owner *quarantine;
#define CALL(expr) do{(expr);e=nn_nvn_error();if(e)goto done;}while(0)
#define BOOL(expr) do{int ok_=(expr);e=nn_nvn_error();if(!ok_||e){if(!e)e=EIO;goto done;}}while(0)
#define CHECK(expr) do{if(!(expr)){e=EPROTO;goto done;}}while(0)
static long reader(void *context,void *out,const void *source,size_t bytes)
{
 struct owner *o=context;uintptr_t start=(uintptr_t)source;
 if(!o->reader_enabled){errno=EFAULT;return -1;}
 const void *starts[4]={o->shaders,o->stages[0]->control,o->stages[1]->control,o->pools[SOURCE].memory};
 size_t sizes[4]={sizeof(o->shaders),o->stages[0]->control_bytes,o->stages[1]->control_bytes,o->pools[SOURCE].bytes};
 for(unsigned i=0;i<4;i++){uintptr_t base=(uintptr_t)starts[i];
  if(start>=base&&start-base<=sizes[i]&&bytes<=sizes[i]-(start-base)){memcpy(out,source,bytes);return (long)bytes;}}
 errno=EFAULT;return -1;
}
static int create_pool(struct owner *o,unsigned index,size_t bytes)
{
 struct pool *p=o->pools+index;struct api *a=&o->a;int e=posix_memalign((void **)&p->memory,4096,bytes);if(e)return e;
 p->bytes=bytes;memset(p->memory,0xa5,bytes);
 a->nvnMemoryPoolBuilderSetDefaults(&p->builder);a->nvnMemoryPoolBuilderSetDevice(&p->builder,&o->device);
 a->nvnMemoryPoolBuilderSetStorage(&p->builder,p->memory,bytes);
 int flags=NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED|(((NVN_NVN_PIPELINE_GOB||NVN_NVN_PIPELINE_COLOR)&&index==TARGET)?NVN_MEMORY_POOL_FLAGS_COMPRESSIBLE:0);
#if NVN_NVN_PIPELINE_BC || NVN_NVN_PIPELINE_BUFFER || NVN_NVN_PIPELINE_SAMPLED
 if(index==ARGUMENT)flags=o->metadata_flags;
#endif
 a->nvnMemoryPoolBuilderSetFlags(&p->builder,flags);
 if((e=nn_nvn_error()))return e;
 int ok=a->nvnMemoryPoolInitialize(&p->key,&p->builder);if((e=nn_nvn_error()))return e;if(!ok)return EIO;p->live=1;
 if(a->nvnMemoryPoolMap(&p->key)!=p->memory)return EPROTO;
 p->address=a->nvnMemoryPoolGetBufferAddress(&p->key);if((e=nn_nvn_error()))return e;return p->address?0:EPROTO;
}
static void put_float(unsigned char *p,float f){memcpy(p,&f,4);}
#if !NVN_NVN_PIPELINE_BC && !NVN_NVN_PIPELINE_BUFFER && !NVN_NVN_PIPELINE_COLOR && !NVN_NVN_PIPELINE_SAMPLED
static void inputs(struct owner *o,unsigned layout,unsigned mode,unsigned repeat)
{
 for(unsigned i=0;i<5;i++)memset(o->pools[i].memory,0xa5,o->pools[i].bytes);
 float matrix[16]={0};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;matrix[3]=repeat?.75f:0;
 memcpy(o->pools[UNIFORM].memory+256,matrix,64);
 static const float positions[3][2]={{-1,-1},{3,-1},{-1,3}};
 for(unsigned i=0;i<6;i++){
  unsigned v=i%3;unsigned char color[4]={0,0,0,255};color[repeat?(v+1)%3:v]=255;
  if(i>=3){color[0]=255;color[1]=0;color[2]=255;}
  unsigned char *p=o->pools[VERTEX].memory+256+i*(layout==0?24:layout==1?64:16);
  unsigned po=layout==0?0:layout==1?8:4;put_float(p+po,positions[v][0]);put_float(p+po+4,positions[v][1]);
  if(layout<2){unsigned co=layout==0?8:32;for(unsigned c=0;c<4;c++)put_float(p+co+c*4,color[c]/255.f);}
  else memcpy(o->pools[COLOR].memory+128+i*12+4,color,4);
 }
 uint16_t indices[6]={0,1,2,3,4,5};memcpy(o->pools[INDEX].memory+64,indices,sizeof(indices));
 uint32_t arguments[10]={3,1,0,0,0,3,1,3,0,0};if(mode==2){arguments[0]=3;arguments[1]=1;arguments[2]=arguments[3]=0;}
 memcpy(o->pools[ARGUMENT].memory+64,arguments,sizeof(arguments));uint32_t count=repeat?2:1;
 memcpy(o->pools[ARGUMENT].memory+256,&count,4);
}
#endif
 #if !NVN_NVN_PIPELINE_BUFFER && !NVN_NVN_PIPELINE_COLOR
static int save(const char *directory,unsigned number,const struct pool *p)
{
 char name[1400];int n=snprintf(name,sizeof(name),"%s/nvn-pipeline-%02u.raw",directory,number);if(n<0||(size_t)n>=sizeof(name))return ENAMETOOLONG;
 int fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;
 size_t at=0;int e=0;while(at<p->bytes){ssize_t w=write(fd,p->memory+at,p->bytes-at);if(w<0&&errno==EINTR)continue;
  if(w<=0){e=w<0?errno:EIO;break;}at+=(size_t)w;}
 if(!e&&fsync(fd))e=errno;
 if(close(fd)&&!e)e=errno;
 return e;
}
 #endif
static int cleanup(struct owner *o)
{
 struct api *a=&o->a;int e;
 if(o->pending)return EBUSY;
 if(o->handle){a->nvnDeviceFinalizeCommandHandle(&o->device,o->handle);if((e=nn_nvn_error()))return e;o->handle=0;}
#if NVN_NVN_PIPELINE_COLOR
 if(o->copied){a->nvnDeviceFinalizeCommandHandle(&o->device,o->copied);if((e=nn_nvn_error()))return e;o->copied=0;}
#endif
#if NVN_NVN_PIPELINE_CALL || NVN_NVN_PIPELINE_BUFFER || NVN_NVN_PIPELINE_COLOR
 if(o->call_parent_live){a->nvnCommandBufferFinalize(&o->call_parent);if((e=nn_nvn_error()))return e;o->call_parent_live=0;}
 if(o->call_child){a->nvnDeviceFinalizeCommandHandle(&o->device,o->call_child);if((e=nn_nvn_error()))return e;o->call_child=0;}
#endif
 if(o->command_live){a->nvnCommandBufferFinalize(&o->command);if((e=nn_nvn_error()))return e;o->command_live=0;}
#if NVN_NVN_PIPELINE_BC || NVN_NVN_PIPELINE_BUFFER || NVN_NVN_PIPELINE_SAMPLED
#if NVN_NVN_PIPELINE_BUFFER
 if(o->copied){a->nvnDeviceFinalizeCommandHandle(&o->device,o->copied);if((e=nn_nvn_error()))return e;o->copied=0;}
 if(o->buffer_live){a->nvnTextureFinalize(&o->buffer);if((e=nn_nvn_error()))return e;o->buffer_live=0;}
#endif
 if(o->texture_pool_live){a->nvnTexturePoolFinalize(&o->texture_pool);if((e=nn_nvn_error()))return e;o->texture_pool_live=0;}
 if(o->sampler_pool_live){a->nvnSamplerPoolFinalize(&o->sampler_pool);if((e=nn_nvn_error()))return e;o->sampler_pool_live=0;}
 if(o->sample_live){a->nvnTextureFinalize(&o->sample);if((e=nn_nvn_error()))return e;o->sample_live=0;}
 if(o->sampler_live){a->nvnSamplerFinalize(&o->sampler);if((e=nn_nvn_error()))return e;o->sampler_live=0;}
#endif
 if(o->texture_live){a->nvnTextureFinalize(&o->texture);if((e=nn_nvn_error()))return e;o->texture_live=0;}
 if(o->program_live){a->nvnProgramFinalize(&o->program);if((e=nn_nvn_error()))return e;o->program_live=0;}
 if(o->queue_live){a->nvnQueueFinalize(&o->queue);if((e=nn_nvn_error()))return e;o->queue_live=0;}
 for(unsigned i=0;i<POOLS;i++){struct pool *p=o->pools+i;
  if(p->live){a->nvnMemoryPoolFinalize(&p->key);if((e=nn_nvn_error()))return e;p->live=0;}
  free(p->memory);p->memory=NULL;free(p->shadow);p->shadow=NULL;}
 if(o->device_live){a->nvnDeviceFinalize(&o->device);if((e=nn_nvn_error()))return e;o->device_live=0;}
 return 0;
}
#if defined(NVN_NVN_PIPELINE_HELPERS_ONLY)
/* Shared allocator/reader/lifetime helpers for independent public qualifiers. */
#elif NVN_NVN_PIPELINE_SAMPLED
#include "nvn_pipeline_sampled_probe.inc"
#elif NVN_NVN_PIPELINE_COLOR
#include "nvn_pipeline_color_probe.inc"
#elif NVN_NVN_PIPELINE_BUFFER
#include "nvn_pipeline_buffer_probe.inc"
#elif NVN_NVN_PIPELINE_BC
#if NVN_NVN_PIPELINE_BC_MIPS
#include "nvn_pipeline_bc_mip_probe.inc"
#else
#include "nvn_pipeline_bc_probe.inc"
#endif
#else
int nn_nvn_pipeline_probe(const char *directory,char *json,size_t capacity)
{
 if(!directory||!json||!capacity)return EINVAL;
 struct owner *o=calloc(1,sizeof(*o));if(!o)return ENOMEM;struct api *a=&o->a;
 int e=0,clean=0;const char *stage="loader";unsigned completed=0,submits=0,finishes=0,recorded=0,released=0,files=0,replays=0;
 uint64_t channels=0,guards=0,readonly=0;unsigned maxerror=0,bad_case=UINT32_MAX,bad_x=UINT32_MAX,bad_y=UINT32_MAX,bad_c=UINT32_MAX,bad_actual=0,bad_expected=0;
 unsigned call_wrappers=0,call_children_finalized=0;
 size_t bad_byte=SIZE_MAX;unsigned flags_getter_checks=0,static_view_registrations=0;unsigned shapes=(NVN_NVN_PIPELINE_GOB||NVN_NVN_PIPELINE_RUNTIME_VIEWS)?3:1;char cases[12000];size_t used=0;cases[0]=0;
#define LOAD(name,type) a->name=(type)nvnBootstrapLoader(#name);if(!a->name){e=ENOSYS;goto done;}
 API_LIST(LOAD)
#undef LOAD
 nn_nvn_clear_error();const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 CHECK(NN_PIPELINE_QUAL_PAIR_INDEX<archive->pair_count);const struct nn_pipeline_pair_recipe *pair=archive->pairs+NN_PIPELINE_QUAL_PAIR_INDEX;
 o->stages[0]=archive->stages->stages+pair->vs_index;o->stages[1]=archive->stages->stages+pair->fs_index;
 CHECK(pair->recipe&&!pair->recipe->texture_mask&&!pair->recipe->sampler_mask);
 CHECK(o->stages[0]->source_bytes==o->stages[0]->code_bytes&&o->stages[1]->source_bytes==o->stages[1]->code_bytes);
 stage="device";CALL(a->nvnDeviceBuilderSetDefaults(&o->db));BOOL(a->nvnDeviceInitialize(&o->device,&o->db));o->device_live=1;
 if((e=nn_nvn_register_pipeline_archive(&o->device,archive,reader,o)))goto done;
#if NVN_NVN_PIPELINE_RUNTIME_VIEWS
 /* Qualification views describe allocation/oracle extents only. They are
  * deliberately never registered: each Draw resolves actual runtime FDL. */
 if((e=nn_nvn_enable_pipeline_runtime_views(&o->device)))goto done;
#else
 if((e=nn_nvn_register_pipeline_views(&o->device,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT)))goto done;
 static_view_registrations++;
#endif
 if((e=nn_nvn_register_texture_bridge(&o->device,&NN_TEXTURE_BRIDGE_BUNDLE)))goto done;
 CALL(a->nvnDeviceSetWindowOriginMode(&o->device,NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT));CALL(a->nvnDeviceSetDepthMode(&o->device,NVN_DEPTH_MODE_NEAR_IS_ZERO));
 CALL(a->nvnQueueBuilderSetDefaults(&o->qb));CALL(a->nvnQueueBuilderSetDevice(&o->qb,&o->device));BOOL(a->nvnQueueInitialize(&o->queue,&o->qb));o->queue_live=1;
 for(unsigned i=0;i<5;i++)if((e=create_pool(o,i,4096)))goto done;
 if((e=create_pool(o,ARENA,65536)))goto done;
 size_t source_bytes=4096;for(unsigned i=0;i<2;i++)source_bytes+=(o->stages[i]->source_bytes+4095u)&~(size_t)4095u;
 if((e=create_pool(o,SOURCE,source_bytes+4096)))goto done;
 for(unsigned i=0,off=4096;i<2;i++){
  memcpy(o->pools[SOURCE].memory+off,o->stages[i]->source,o->stages[i]->source_bytes);
  o->shaders[i]=(NVNshaderData){o->pools[SOURCE].address+off,o->stages[i]->control};off+=(o->stages[i]->source_bytes+4095u)&~4095u;
 }
 stage="source-program";BOOL(a->nvnProgramInitialize(&o->program,&o->device));o->program_live=1;o->reader_enabled=1;
 BOOL(a->nvnProgramSetShaders(&o->program,2,o->shaders));o->reader_enabled=0;
 CALL(a->nvnMemoryPoolFinalize(&o->pools[SOURCE].key));o->pools[SOURCE].live=0;free(o->pools[SOURCE].memory);o->pools[SOURCE].memory=NULL;
 for(unsigned shape=0;shape<shapes;shape++)for(unsigned layout=0;layout<3;layout++)for(unsigned mode=0;mode<4;mode++){
  unsigned test=(shape*3+layout)*4+mode;const struct nn_tex_native_view *v=NN_PIPELINE_QUAL_VIEWS[shape];
  stage="texture";CALL(a->nvnTextureBuilderSetDefaults(&o->tb));CALL(a->nvnTextureBuilderSetDevice(&o->tb,&o->device));
  CALL(a->nvnTextureBuilderSetFlags(&o->tb,NVN_NVN_PIPELINE_GOB?9:NVN_TEXTURE_FLAGS_LINEAR));
  CALL(a->nvnTextureBuilderSetSize2D(&o->tb,(int)v->width,(int)v->height));CALL(a->nvnTextureBuilderSetFormat(&o->tb,NVN_FORMAT_RGBA8));
  if(!NVN_NVN_PIPELINE_GOB)CALL(a->nvnTextureBuilderSetStride(&o->tb,v->pitch));
  ptrdiff_t payload=a->nvnTextureBuilderGetStorageSize(&o->tb);if((e=nn_nvn_error()))goto done;CHECK(payload>0);
  size_t target_bytes=4096+(((size_t)payload+4095u)&~(size_t)4095u)+4096;
  if((e=create_pool(o,TARGET,target_bytes)))goto done;
  CALL(a->nvnTextureBuilderSetStorage(&o->tb,&o->pools[TARGET].key,4096));BOOL(a->nvnTextureInitialize(&o->texture,&o->tb));o->texture_live=1;
  if(NVN_NVN_PIPELINE_GOB){CHECK(a->nvnTextureGetFlags(&o->texture)==9);CHECK(a->nvnTextureGetTextureAddress(&o->texture)==o->pools[TARGET].address+4096);if((e=nn_nvn_error()))goto done;flags_getter_checks++;}
  BOOL(a->nvnCommandBufferInitialize(&o->command,&o->device));o->command_live=1;
  CALL(a->nvnCommandBufferAddCommandMemory(&o->command,&o->pools[ARENA].key,0,65536));
  CALL(a->nvnCommandBufferAddControlMemory(&o->command,o->control,sizeof(o->control)));CALL(a->nvnCommandBufferBeginRecording(&o->command));
  CALL(a->nvnCommandBufferBindProgram(&o->command,&o->program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
  NVNvertexAttribState attributes[2];NVNvertexStreamState streams[16];
  unsigned ps=layout==2?1:layout==1?7:3,cs=layout==2?11:ps;
  for(unsigned i=0;i<16;i++)CALL(a->nvnVertexStreamStateSetDefaults(streams+i));
  CALL(a->nvnVertexStreamStateSetStride(streams+ps,layout==0?24:layout==1?64:16));
  if(layout==2)CALL(a->nvnVertexStreamStateSetStride(streams+cs,12));
  for(unsigned i=0;i<2;i++)CALL(a->nvnVertexAttribStateSetDefaults(attributes+i));
  CALL(a->nvnVertexAttribStateSetFormat(attributes,NVN_FORMAT_RG32F,layout==0?0:layout==1?8:4));
  CALL(a->nvnVertexAttribStateSetStreamIndex(attributes,(int)ps));
  CALL(a->nvnVertexAttribStateSetFormat(attributes+1,layout==2?NVN_FORMAT_RGBA8:NVN_FORMAT_RGBA32F,layout==0?8:layout==1?32:4));
  CALL(a->nvnVertexAttribStateSetStreamIndex(attributes+1,(int)cs));
  CALL(a->nvnCommandBufferBindVertexAttribState(&o->command,2,attributes));CALL(a->nvnCommandBufferBindVertexStreamState(&o->command,16,streams));
  CALL(a->nvnCommandBufferBindVertexBuffer(&o->command,(int)ps,o->pools[VERTEX].address+256,6*(layout==0?24:layout==1?64:16)));
  if(layout==2)CALL(a->nvnCommandBufferBindVertexBuffer(&o->command,(int)cs,o->pools[COLOR].address+128,72));
#if !NVN_NVN_PIPELINE_CALL
  CALL(a->nvnCommandBufferBindUniformBuffer(&o->command,NVN_SHADER_STAGE_VERTEX,0,o->pools[UNIFORM].address+256,64));
#endif
  const NVNtexture *target=&o->texture;CALL(a->nvnCommandBufferSetRenderTargets(&o->command,1,&target,NULL,NULL,NULL));
  CALL(a->nvnCommandBufferSetViewport(&o->command,0,0,(int)v->width,(int)v->height));CALL(a->nvnCommandBufferSetScissor(&o->command,0,0,(int)v->width,(int)v->height));
  CALL(a->nvnCommandBufferSetDepthRange(&o->command,0,1));
  uint64_t indices=o->pools[INDEX].address+64,args=o->pools[ARGUMENT].address+64,count=o->pools[ARGUMENT].address+256;
  if(mode==0)CALL(a->nvnCommandBufferDrawArrays(&o->command,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
  else if(mode==1)CALL(a->nvnCommandBufferDrawElementsBaseVertex(&o->command,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,3,indices,0));
  else if(mode==2)CALL(a->nvnCommandBufferDrawArraysIndirect(&o->command,NVN_DRAW_PRIMITIVE_TRIANGLES,args));
  else CALL(a->nvnCommandBufferMultiDrawElementsIndirectCount(&o->command,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,indices,args,count,2,20));
  o->handle=a->nvnCommandBufferEndRecording(&o->command);if((e=nn_nvn_error()))goto done;CHECK(o->handle);
#if NVN_NVN_PIPELINE_CALL || NVN_NVN_PIPELINE_BUFFER || NVN_NVN_PIPELINE_COLOR
  o->call_child=o->handle;o->handle=0;
  BOOL(a->nvnCommandBufferInitialize(&o->call_parent,&o->device));o->call_parent_live=1;
  CALL(a->nvnCommandBufferAddCommandMemory(&o->call_parent,&o->pools[ARENA].key,32768,32768));
  CALL(a->nvnCommandBufferAddControlMemory(&o->call_parent,o->call_control,sizeof(o->call_control)));
  CALL(a->nvnCommandBufferBeginRecording(&o->call_parent));
  NVNcolorState call_color;CALL(a->nvnColorStateSetDefaults(&call_color));CALL(a->nvnCommandBufferBindColorState(&o->call_parent,&call_color));
  NVNchannelMaskState call_channelmask;CALL(a->nvnChannelMaskStateSetDefaults(&call_channelmask));CALL(a->nvnCommandBufferBindChannelMaskState(&o->call_parent,&call_channelmask));
  NVNpolygonState call_polygon;CALL(a->nvnPolygonStateSetDefaults(&call_polygon));CALL(a->nvnCommandBufferBindPolygonState(&o->call_parent,&call_polygon));
  NVNdepthStencilState call_depthstencil;CALL(a->nvnDepthStencilStateSetDefaults(&call_depthstencil));CALL(a->nvnCommandBufferBindDepthStencilState(&o->call_parent,&call_depthstencil));
  NVNmultisampleState call_multisample;CALL(a->nvnMultisampleStateSetDefaults(&call_multisample));CALL(a->nvnCommandBufferBindMultisampleState(&o->call_parent,&call_multisample));
  NVNblendState call_blend;CALL(a->nvnBlendStateSetDefaults(&call_blend));CALL(a->nvnCommandBufferBindBlendState(&o->call_parent,&call_blend));
  CALL(a->nvnCommandBufferSetDepthBounds(&o->call_parent,0,0,1));
  CALL(a->nvnCommandBufferBindUniformBuffer(&o->call_parent,NVN_SHADER_STAGE_VERTEX,0,o->pools[UNIFORM].address+256,64));
  CALL(a->nvnCommandBufferCallCommands(&o->call_parent,1,&o->call_child));
  o->handle=a->nvnCommandBufferEndRecording(&o->call_parent);if((e=nn_nvn_error()))goto done;CHECK(o->handle);call_wrappers++;
  CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->call_child));o->call_child=0;call_children_finalized++;
  CALL(a->nvnCommandBufferFinalize(&o->command));o->command_live=0;
#endif
  recorded++;
  int n=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"width\":%u,\"height\":%u,\"layout\":%u,\"mode\":%u,\"payload_bytes\":%td,\"pool_bytes\":%zu}",used?",":"",test,v->width,v->height,layout,mode,payload,target_bytes);
  CHECK(n>0&&(size_t)n<sizeof(cases)-used);used+=(size_t)n;
  CALL(a->nvnMemoryPoolFlushMappedRange(&o->pools[TARGET].key,0,target_bytes));
  for(unsigned repeat=0;repeat<2;repeat++){
   inputs(o,layout,mode,repeat);
   for(unsigned i=0;i<5;i++){
    struct pool *p=o->pools+i;if(!p->shadow){p->shadow=malloc(p->bytes);if(!p->shadow){e=ENOMEM;goto done;}}
    memcpy(p->shadow,p->memory,p->bytes);CALL(a->nvnMemoryPoolFlushMappedRange(&p->key,0,p->bytes));}
   stage="submit";o->pending=1;CALL(a->nvnQueueSubmitCommands(&o->queue,1,&o->handle));submits++;
   stage="finish";CALL(a->nvnQueueFinish(&o->queue));o->pending=0;finishes++;
   CALL(a->nvnMemoryPoolInvalidateMappedRange(&o->pools[TARGET].key,0,target_bytes));
   stage="oracle";if(!NVN_NVN_PIPELINE_GOB)for(unsigned y=0;y<v->height;y++)for(unsigned x=0;x<v->width;x++){
    unsigned expected[4];pixel(v->width,v->height,mode,repeat,x,y,expected);
    for(unsigned c=0;c<4;c++){unsigned actual=o->pools[TARGET].memory[4096+(size_t)y*v->pitch+x*4+c];
     unsigned delta=actual>expected[c]?actual-expected[c]:expected[c]-actual;if(delta>maxerror)maxerror=delta;
     if(delta>(c==3?0u:1u)){bad_case=test*2+repeat;bad_x=x;bad_y=y;bad_c=c;bad_actual=actual;bad_expected=expected[c];e=EIO;goto save_failure;}channels++;}}
   stage="guards";for(size_t off=0;off<target_bytes;off++)if(off<4096||off>=4096+(size_t)payload){
    if(o->pools[TARGET].memory[off]!=0xa5){bad_byte=off;e=EIO;goto save_failure;}guards++;}
   stage="readonly";for(unsigned i=0;i<5;i++){
    CALL(a->nvnMemoryPoolInvalidateMappedRange(&o->pools[i].key,0,o->pools[i].bytes));
    for(size_t off=0;off<o->pools[i].bytes;off++){if(o->pools[i].memory[off]!=o->pools[i].shadow[off]){bad_byte=off;e=EIO;goto save_failure;}readonly++;}}
   save_failure:;
   int saved=save(directory,test*2+repeat,o->pools+TARGET);if(saved&&!e)e=saved;else if(!saved)files++;
   if(e)goto done;
   completed++;if(repeat)replays++;
  }
  stage="retire";CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->handle));o->handle=0;released++;
#if NVN_NVN_PIPELINE_CALL || NVN_NVN_PIPELINE_BUFFER || NVN_NVN_PIPELINE_COLOR
  CALL(a->nvnCommandBufferFinalize(&o->call_parent));o->call_parent_live=0;
#else
  CALL(a->nvnCommandBufferFinalize(&o->command));o->command_live=0;
#endif
  CALL(a->nvnTextureFinalize(&o->texture));o->texture_live=0;
  CALL(a->nvnMemoryPoolFinalize(&o->pools[TARGET].key));o->pools[TARGET].live=0;free(o->pools[TARGET].memory);o->pools[TARGET].memory=NULL;
 }
 stage="complete";
done:
 nn_nvn_clear_error();if(o->pending&&o->queue_live){a->nvnQueueFinish(&o->queue);if(!nn_nvn_error())o->pending=0;}
 clean=cleanup(o);int retained=clean||o->pending;
 if(retained){o->next=quarantine;quarantine=o;}else free(o);
 unsigned expected=shapes*24;int passed=!e&&!clean&&completed==expected;
 if(NVN_NVN_PIPELINE_CALL&&(!call_wrappers||call_wrappers!=shapes*12||call_children_finalized!=call_wrappers))passed=0;
 /* Append evidence after the common result serializer without changing the
  * established result fields or Activity/nonce/collector protocol. */
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"nvn_dynamic_pipeline\",\"uid\":%u,\"source_layout\":\"%s\",\"source_flags\":%u,\"source_flags_getter_checks\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"submissions\":%u,\"finishes\":%u,\"completed_cases\":%u,\"recorded_handles\":%u,\"released_handles\":%u,\"files\":%u,\"replays\":%u,\"channel_checks\":%llu,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"maximum_channel_error\":%u,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,\"bad_byte\":%zu,\"bad_actual\":%u,\"bad_expected\":%u,\"target_offset\":4096,\"nvn_ubo_index\":0,\"original_maxwell_cb\":3,\"same_handle_replay\":true,\"cpu_image_conversion\":false,\"legacy_image_template\":false,\"game_modules_executed\":false,\"offline_pixel_oracle_required\":%s,\"cases\":[%s],\"passed\":%s}\n",
  (unsigned)getuid(),NVN_NVN_PIPELINE_GOB?"gob":"linear",NVN_NVN_PIPELINE_GOB?9u:(unsigned)NVN_TEXTURE_FLAGS_LINEAR,flags_getter_checks,stage,e,clean,retained?"true":"false",submits,finishes,completed,recorded,released,files,replays,(unsigned long long)channels,(unsigned long long)guards,(unsigned long long)readonly,maxerror,bad_case,bad_x,bad_y,bad_c,bad_byte,bad_actual,bad_expected,NVN_NVN_PIPELINE_GOB?"true":"false",cases,passed?"true":"false");
 if(n>0&&(size_t)n<capacity){
  size_t end=(size_t)n;while(end&&(json[end-1]=='\n'||json[end-1]==' '))end--;
  if(!end||json[end-1]!='}')return EPROTO;
  int extra=snprintf(json+end-1,capacity-end+1,",\"runtime_target_views\":%s,\"static_view_registrations\":%u}\n",NVN_NVN_PIPELINE_RUNTIME_VIEWS?"true":"false",static_view_registrations);
  if(extra<0||(size_t)extra>=capacity-end+1)return EOVERFLOW;
  end=end-1+(size_t)extra;while(end&&(json[end-1]=='\n'||json[end-1]==' '))end--;
  if(!end||json[end-1]!='}')return EPROTO;
  extra=snprintf(json+end-1,capacity-end+1,",\"call_commands\":%s,\"call_source_ubo_bound\":false,\"call_parent_ubo_index\":0,\"call_wrappers\":%u,\"call_children_finalized_before_submit\":%u}\n",NVN_NVN_PIPELINE_CALL?"true":"false",call_wrappers,call_children_finalized);
  if(extra<0||(size_t)extra>=capacity-end+1)return EOVERFLOW;
 }
 return passed&&n>0&&(size_t)n<capacity?0:e?e:clean?clean:EIO;
}

#endif
