/* SPDX-License-Identifier: MIT
 * Qualification caller only: actual public NVN Register/Clear/Draw/Copy/Finish.
 * No raw draw/CS path, wrappers, CPU image conversion, game or compositor claim.
 * CPU input seeds and mathematical expected pixels live only in this probe. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/nvn.h"
#include "nvn_native/texture_bridge.h"
#include "texture_cases.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_TEXTURE_API_HEADER
#include NVN_TEXTURE_BRIDGE_HEADER
#ifndef O_NOFOLLOW
#error "Regular-file no-follow required"
#endif
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
#include "texture_bridge_diag_config.h"
static unsigned bridge_diagnostic_mask=255;
unsigned nn_tex_bridge_probe_mask(void){return bridge_diagnostic_mask;}
static struct {nn_bo *bo;size_t offset;unsigned inputs,writeback;} bridge_marks[32];
static size_t bridge_mark_count;
static int bridge_mark_overflow;
void nn_tex_bridge_probe_track(nn_bo *bo,size_t offset,unsigned inputs,unsigned writeback)
{
 if(bridge_mark_count==32){bridge_mark_overflow=1;return;}
 bridge_marks[bridge_mark_count].bo=bo;bridge_marks[bridge_mark_count].offset=offset;
 bridge_marks[bridge_mark_count].inputs=inputs;bridge_marks[bridge_mark_count].writeback=writeback;bridge_mark_count++;
}
/* Only accepted, unfinished submissions are observed: the existing unknown-
 * completion ledger still owns ALL these wrapper BOs. Normal-NC mapping plus
 * aligned volatile reads samples CP progress; no finish, fault PC or timing is
 * inferred. No original pool/image bytes are touched here. */
static void bridge_progress(char *json,size_t capacity)
{
 size_t at=0;int n=snprintf(json,capacity,"[");if(n<0||(size_t)n>=capacity)return;at=(size_t)n;
 for(size_t i=0;i<bridge_mark_count;i++){
  nn_bo *bo=bridge_marks[i].bo;size_t off=bridge_marks[i].offset;uint32_t mark=0;
  int e=off%4||off>nn_bo_size(bo)||4>nn_bo_size(bo)-off||!nn_bo_map(bo)?EINVAL:0;
  if(!e)e=nn_bo_cache(bo,off,4,1);
  if(!e)mark=*(volatile const uint32_t *)((const unsigned char *)nn_bo_map(bo)+off);
  n=snprintf(json+at,capacity-at,"%s{\"wrapper\":%zu,\"inputs\":%u,\"writeback\":%s,\"mark\":%u,\"read_errno\":%d}",i?",":"",i,bridge_marks[i].inputs,bridge_marks[i].writeback?"true":"false",mark,e);
  if(n<0||(size_t)n>=capacity-at){snprintf(json,capacity,"[]");bridge_mark_overflow=1;return;}at+=(size_t)n;
 }
 snprintf(json+at,capacity-at,"]");
}
#endif
enum { BP_CODE,BP_TARGET,BP_VERTEX,BP_UNIFORM,BP_SAMPLE,BP_COMMAND,BP_TPOOL,BP_SPOOL,BP_POOLS };
enum { BV_VS,BV_FS,BV_TARGET,BV_READBACK,BV_VERTEX,BV_VUB,BV_FUB,BV_BUFFERS };
struct bridge_probe_owner {
 struct bridge_probe_owner *next;NVNdevice device;NVNqueue queue;NVNprogram program;NVNtexture target,sample;
 NVNcommandBuffer command;NVNtexturePool tp;NVNsamplerPool sp;NVNsampler sampler;
 NVNmemoryPool pools[BP_POOLS];NVNbuffer buffers[BV_BUFFERS];NVNcommandHandle handle;
 NVNdeviceBuilder db;NVNqueueBuilder qb;NVNmemoryPoolBuilder pb;NVNbufferBuilder bb;NVNtextureBuilder tb;NVNsamplerBuilder sb;
 unsigned char *memory[BP_POOLS];size_t sizes[BP_POOLS];unsigned char *expected[BP_POOLS];
 unsigned ready_device,ready_queue,ready_program,ready_target,ready_sample,ready_command,ready_tp,ready_sp,ready_sampler;
 unsigned ready_pools[BP_POOLS],ready_buffers[BV_BUFFERS];_Alignas(16) unsigned char control[2048];
};
static struct bridge_probe_owner *retained_bridge_probes;
static int bridge_raw(const char *dir,unsigned index,const void *data,size_t bytes)
{
 char path[1024];int n=snprintf(path,sizeof(path),"%s/bridge-%u.bin",dir,index);if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return errno;
 const unsigned char *p=data;size_t done=0;int e=0;
 while(done<bytes){ssize_t k=write(fd,p+done,bytes-done);if(k<0&&errno==EINTR)continue;if(k<=0){e=k<0?errno:EIO;break;}done+=(size_t)k;}
 if(!e&&fsync(fd))e=errno;
 if(close(fd)&&!e)e=errno;
 return e;
}
int nn_nvn_texture_bridge_probe(const char *directory,char *json,size_t capacity)
{
 struct bridge_probe_owner *o=calloc(1,sizeof(*o));if(!o)return -1;
 const struct nn_gfx_image *image=nn_test_graphics.image;const char *stage="profile";int e=0,cleanup=0,pending=0;
 unsigned completed=0,submits=0,finishes=0,raw_files=0,aliases=0;uint64_t channels=0,guards=0,readonly=0;
 size_t offsets[BV_BUFFERS]={0},lengths[BV_BUFFERS]={0};NVNbufferAddress addresses[BV_BUFFERS]={0};
 const unsigned bp[BV_BUFFERS]={BP_CODE,BP_CODE,BP_TARGET,BP_TARGET,BP_VERTEX,BP_UNIFORM,BP_UNIFORM};
 struct nn_tex_source_layout tl={0},sl={0};const size_t target_offset=4096,sample_offset=4096,readback_offset=32768;
 unsigned bad_case=~0u,bad_x=~0u,bad_y=~0u,bad_channel=~0u;size_t bad_byte=SIZE_MAX;unsigned bad_pool=~0u;
 char diagnostic_json[8192]="[]";const char *diagnostic_enabled="false",*diagnostic_overflow="false";
 char diagnostic_mask_text[5]="null";
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
 diagnostic_enabled="true";bridge_mark_count=0;bridge_mark_overflow=0;
#endif
#define ENTRY(name,type) type name=(type)nvnBootstrapLoader(#name);if(!name)e=ENOSYS;
#include "../src/nvn_entries.inc"
#undef ENTRY
#define CALL(x) do{x;if((e=nn_nvn_error()))goto done;}while(0)
#define REQUIRE(x) do{if(!(x)){e=nn_nvn_error();if(!e)e=EINVAL;goto done;}}while(0)
#define BOOL(x) REQUIRE(x)
 if(!json||!capacity){free(o);return -1;}nn_nvn_clear_error();
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
 stage="diagnostic-config";bridge_diagnostic_mask=255;
 if((e=nn_texture_bridge_diag_config(directory,&bridge_diagnostic_mask)))goto done;
 snprintf(diagnostic_mask_text,sizeof(diagnostic_mask_text),"%u",bridge_diagnostic_mask);
 stage="profile";
#endif
 if(e||!directory||!image||image->schema!=3||image->chip!=0x06050002||image->profile!=NN_GFX_TEXTURE_2D_NORMALIZED||
    image->width!=64||image->height!=64||image->texture_width!=2||image->texture_height!=2||nn_test_graphics.sampled_count!=1){e=e?e:ENOTSUP;goto done;}
 struct nn_tex_source_desc desc={.flags=1,.target=1,.format=37,.width=64,.height=64,.layers=1,.levels=1,.block_height_log2=-1};
 if((e=nn_tex_source_layout(&desc,&tl)))goto done;
 desc.flags=0;desc.width=desc.height=2;if((e=nn_tex_source_layout(&desc,&sl)))goto done;
 stage="device";CALL(nvnDeviceBuilderSetDefaults(&o->db));BOOL(nvnDeviceInitialize(&o->device,&o->db));o->ready_device=1;
 if((e=nn_nvn_register_graphics(&o->device,&nn_test_graphics))||(e=nn_nvn_register_texture_bridge(&o->device,&NN_TEXTURE_BRIDGE_BUNDLE)))goto done;
 CALL(nvnDeviceSetWindowOriginMode(&o->device,NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT));CALL(nvnDeviceSetDepthMode(&o->device,NVN_DEPTH_MODE_NEAR_IS_MINUS_W));
 CALL(nvnQueueBuilderSetDefaults(&o->qb));CALL(nvnQueueBuilderSetDevice(&o->qb,&o->device));BOOL(nvnQueueInitialize(&o->queue,&o->qb));o->ready_queue=1;
 offsets[BV_VS]=4096;lengths[BV_VS]=nn_test_graphics.vs.source_bytes;offsets[BV_FS]=((offsets[BV_VS]+lengths[BV_VS]+4095)&~(size_t)4095)+4096;lengths[BV_FS]=nn_test_graphics.fs.source_bytes;
 o->sizes[BP_CODE]=((offsets[BV_FS]+lengths[BV_FS]+4095)&~(size_t)4095)+4096;
 o->sizes[BP_TARGET]=65536;o->sizes[BP_VERTEX]=4096;o->sizes[BP_UNIFORM]=4096;o->sizes[BP_SAMPLE]=12288;o->sizes[BP_COMMAND]=4096;o->sizes[BP_TPOOL]=16384;o->sizes[BP_SPOOL]=16384;
 offsets[BV_TARGET]=target_offset;lengths[BV_TARGET]=(size_t)tl.payload_bytes;offsets[BV_READBACK]=readback_offset;lengths[BV_READBACK]=(size_t)tl.payload_bytes;
 offsets[BV_VERTEX]=256;lengths[BV_VERTEX]=96;offsets[BV_VUB]=64;lengths[BV_VUB]=64;offsets[BV_FUB]=256;lengths[BV_FUB]=64;
 stage="pools";
 for(unsigned i=0;i<BP_POOLS;i++){
  if((e=posix_memalign((void**)&o->memory[i],4096,o->sizes[i])))goto done;
  o->expected[i]=malloc(o->sizes[i]);if(!o->expected[i]){e=ENOMEM;goto done;}memset(o->memory[i],0xa5,o->sizes[i]);
  CALL(nvnMemoryPoolBuilderSetDefaults(&o->pb));CALL(nvnMemoryPoolBuilderSetDevice(&o->pb,&o->device));CALL(nvnMemoryPoolBuilderSetStorage(&o->pb,o->memory[i],o->sizes[i]));
  CALL(nvnMemoryPoolBuilderSetFlags(&o->pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED|(i==BP_CODE?NVN_MEMORY_POOL_FLAGS_SHADER_CODE:0)|((i==BP_TARGET||i==BP_SAMPLE)?NVN_MEMORY_POOL_FLAGS_COMPRESSIBLE:0)));
  BOOL(nvnMemoryPoolInitialize(&o->pools[i],&o->pb));o->ready_pools[i]=1;REQUIRE(nvnMemoryPoolMap(&o->pools[i])==o->memory[i]);
 }
 stage="buffers";
 for(unsigned i=0;i<BV_BUFFERS;i++){
  CALL(nvnBufferBuilderSetDefaults(&o->bb));CALL(nvnBufferBuilderSetDevice(&o->bb,&o->device));CALL(nvnBufferBuilderSetStorage(&o->bb,&o->pools[bp[i]],(ptrdiff_t)offsets[i],lengths[i]));BOOL(nvnBufferInitialize(&o->buffers[i],&o->bb));o->ready_buffers[i]=1;
  addresses[i]=nvnBufferGetAddress(&o->buffers[i]);REQUIRE(addresses[i]);
 }
 memcpy(o->memory[BP_CODE]+offsets[BV_VS],nn_test_graphics.vs.source_code,lengths[BV_VS]);memcpy(o->memory[BP_CODE]+offsets[BV_FS],nn_test_graphics.fs.source_code,lengths[BV_FS]);
 CALL(nvnMemoryPoolFlushMappedRange(&o->pools[BP_CODE],0,o->sizes[BP_CODE]));
 stage="program";BOOL(nvnProgramInitialize(&o->program,&o->device));o->ready_program=1;
 const NVNshaderData shaders[2]={{addresses[BV_VS],nn_test_graphics.vs.source_control},{addresses[BV_FS],nn_test_graphics.fs.source_control}};BOOL(nvnProgramSetShaders(&o->program,2,shaders));
 stage="source-textures";
 for(unsigned i=0;i<2;i++){
  CALL(nvnTextureBuilderSetDefaults(&o->tb));CALL(nvnTextureBuilderSetDevice(&o->tb,&o->device));CALL(nvnTextureBuilderSetFlags(&o->tb,i?8:9));CALL(nvnTextureBuilderSetFormat(&o->tb,NVN_FORMAT_RGBA8));CALL(nvnTextureBuilderSetSize2D(&o->tb,i?2:64,i?2:64));
  REQUIRE(nvnTextureBuilderGetStorageSize(&o->tb)==(i?sl.payload_bytes:tl.payload_bytes));REQUIRE(nvnTextureBuilderGetStorageAlignment(&o->tb)==512);
  CALL(nvnTextureBuilderSetStorage(&o->tb,&o->pools[i?BP_SAMPLE:BP_TARGET],(ptrdiff_t)(i?sample_offset:target_offset)));
  BOOL(nvnTextureInitialize(i?&o->sample:&o->target,&o->tb));if(i)o->ready_sample=1;else o->ready_target=1;
  REQUIRE(nvnTextureGetFlags(i?&o->sample:&o->target)==(i?8:9));REQUIRE(nvnTextureGetRawStorageClass(i?&o->sample:&o->target)==NVN_RAW_STORAGE_CLASS_COLOR_AND_ZF32);REQUIRE(nvnTextureGetStorageSize(i?&o->sample:&o->target)==(int)(i?sl.payload_bytes:tl.payload_bytes));
 }
 REQUIRE(nvnTextureGetTextureAddress(&o->target)==addresses[BV_TARGET]);REQUIRE(nvnTextureGetStride(&o->target)==0);REQUIRE(nvnTextureGetTextureAddress(&o->sample)==nvnMemoryPoolGetBufferAddress(&o->pools[BP_SAMPLE])+sample_offset);
 stage="register";BOOL(nvnTexturePoolInitialize(&o->tp,&o->pools[BP_TPOOL],0,272));o->ready_tp=1;BOOL(nvnSamplerPoolInitialize(&o->sp,&o->pools[BP_SPOOL],0,272));o->ready_sp=1;
 CALL(nvnSamplerBuilderSetDefaults(&o->sb));CALL(nvnSamplerBuilderSetDevice(&o->sb,&o->device));CALL(nvnSamplerBuilderSetMinMagFilter(&o->sb,NVN_MIN_FILTER_NEAREST,NVN_MAG_FILTER_NEAREST));CALL(nvnSamplerBuilderSetWrapMode(&o->sb,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE));BOOL(nvnSamplerInitialize(&o->sampler,&o->sb));o->ready_sampler=1;
 CALL(nvnTexturePoolRegisterTexture(&o->tp,257,&o->sample,NULL));CALL(nvnSamplerPoolRegisterSampler(&o->sp,259,&o->sampler));
 /* The descriptor's canonical/layout snapshot must survive its Texture key. */
 CALL(nvnTextureFinalize(&o->sample));o->ready_sample=0;
 stage="record";BOOL(nvnCommandBufferInitialize(&o->command,&o->device));o->ready_command=1;memset(o->control,0xa5,sizeof(o->control));CALL(nvnCommandBufferAddCommandMemory(&o->command,&o->pools[BP_COMMAND],0,o->sizes[BP_COMMAND]));CALL(nvnCommandBufferAddControlMemory(&o->command,o->control,sizeof(o->control)));CALL(nvnCommandBufferBeginRecording(&o->command));
 CALL(nvnCommandBufferBindProgram(&o->command,&o->program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));CALL(nvnCommandBufferBindUniformBuffer(&o->command,NVN_SHADER_STAGE_VERTEX,0,addresses[BV_VUB],64));CALL(nvnCommandBufferBindUniformBuffer(&o->command,NVN_SHADER_STAGE_FRAGMENT,0,addresses[BV_FUB],64));
 NVNvertexAttribState attributes[3];for(unsigned i=0;i<3;i++){CALL(nvnVertexAttribStateSetDefaults(&attributes[i]));CALL(nvnVertexAttribStateSetFormat(&attributes[i],i==2?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,i==2?16:(int)i*8));CALL(nvnVertexAttribStateSetStreamIndex(&attributes[i],0));}
 NVNvertexStreamState stream;CALL(nvnVertexStreamStateSetDefaults(&stream));CALL(nvnVertexStreamStateSetStride(&stream,32));CALL(nvnCommandBufferBindVertexAttribState(&o->command,3,attributes));CALL(nvnCommandBufferBindVertexStreamState(&o->command,1,&stream));CALL(nvnCommandBufferBindVertexBuffer(&o->command,0,addresses[BV_VERTEX],96));
 const NVNtexture *rt[1]={&o->target};CALL(nvnCommandBufferSetRenderTargets(&o->command,1,rt,NULL,NULL,NULL));CALL(nvnCommandBufferSetViewport(&o->command,0,0,64,64));CALL(nvnCommandBufferSetScissor(&o->command,0,0,64,64));CALL(nvnCommandBufferSetDepthRange(&o->command,0,1));
 CALL(nvnCommandBufferSetTexturePool(&o->command,&o->tp));CALL(nvnCommandBufferSetSamplerPool(&o->command,&o->sp));const struct nn_nvn_sampled *mapping=nn_test_graphics.sampled;
 CALL(nvnCommandBufferBindSeparateTexture(&o->command,mapping->stage,mapping->texture_slot,(NVNseparateTextureHandle){257}));CALL(nvnCommandBufferBindSeparateSampler(&o->command,mapping->stage,mapping->sampler_slot,(NVNseparateSamplerHandle){259ull<<20}));
 const float clear[4]={17/255.0f,29/255.0f,43/255.0f,59/255.0f};CALL(nvnCommandBufferClearColor(&o->command,0,clear,15));CALL(nvnCommandBufferDrawArrays(&o->command,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
 const float patch[4]={0,0,1,1};NVNcopyRegion region={.xoffset=4,.yoffset=5,.width=11,.height=17,.depth=1};CALL(nvnCommandBufferClearTexture(&o->command,&o->target,NULL,&region,patch,15));
 CALL(nvnCommandBufferCopyBufferToBuffer(&o->command,addresses[BV_TARGET],addresses[BV_READBACK],(size_t)tl.payload_bytes,0));
 o->handle=nvnCommandBufferEndRecording(&o->command);REQUIRE(o->handle);CALL(nvnCommandBufferFinalize(&o->command));o->ready_command=0;
 for(unsigned test=0;test<4;test++){
  struct nn_texture_case_inputs input;REQUIRE(!nn_texture_case_prepare(test,&input));
  memset(o->memory[BP_TARGET],0xa5,o->sizes[BP_TARGET]);memset(o->memory[BP_SAMPLE],0xa5,o->sizes[BP_SAMPLE]);memset(o->memory[BP_VERTEX],0xa5,o->sizes[BP_VERTEX]);memset(o->memory[BP_UNIFORM],0xa5,o->sizes[BP_UNIFORM]);
  memcpy(o->memory[BP_VERTEX]+offsets[BV_VERTEX],input.vertices,96);memcpy(o->memory[BP_UNIFORM]+offsets[BV_VUB],input.vs_ubo,64);memcpy(o->memory[BP_UNIFORM]+offsets[BV_FUB],input.fs_ubo,64);
  /* Test input initialization only; source remains canonical GOB. No native
   * image is generated/uploaded by CPU, nor is runtime output converted. */
  for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++){uint64_t off=0;if((e=nn_tex_source_offset(&sl,0,0,x,y,0,&off)))goto done;memcpy(o->memory[BP_SAMPLE]+sample_offset+off,input.texels+(y*2+x)*4,4);}
  for(unsigned i=0;i<BP_POOLS;i++){CALL(nvnMemoryPoolFlushMappedRange(&o->pools[i],0,o->sizes[i]));memcpy(o->expected[i],o->memory[i],o->sizes[i]);}
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
  bridge_mark_count=0;bridge_mark_overflow=0;
#endif
  stage="submit";pending=1;CALL(nvnQueueSubmitCommands(&o->queue,1,&o->handle));submits++;stage="finish";CALL(nvnQueueFinish(&o->queue));pending=0;finishes++;
  stage="invalidate";for(unsigned i=0;i<BP_POOLS;i++)CALL(nvnMemoryPoolInvalidateMappedRange(&o->pools[i],0,o->sizes[i]));
  stage="oracle";
  for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
   struct nn_texture_expected pixel=nn_texture_case_pixel(&input,x,y);if(pixel.reason>=NN_TEXTURE_INVALID){e=EINVAL;goto done;}
   if(x>=4&&x<15&&y>=5&&y<22){pixel.rgba[0]=pixel.rgba[1]=0;pixel.rgba[2]=pixel.rgba[3]=255;pixel.tolerance=0;}
   uint64_t off=0;if((e=nn_tex_source_offset(&tl,0,0,x,y,0,&off)))goto done;
   for(unsigned k=0;k<4;k++){
    unsigned actual=o->memory[BP_TARGET][target_offset+off+k],wanted=pixel.rgba[k],delta=actual>wanted?actual-wanted:wanted-actual;
    if(delta>pixel.tolerance){bad_case=test;bad_x=x;bad_y=y;bad_channel=k;e=EIO;goto save;}
    if(o->memory[BP_TARGET][readback_offset+off+k]!=actual){bad_case=test;bad_byte=readback_offset+off+k;e=EIO;goto save;}
    channels++;aliases++;
   }
  }
  stage="guards";
  for(unsigned i=0;i<BP_POOLS;i++)for(size_t k=0;k<o->sizes[i];k++){
   if(i==BP_TARGET&&((k>=target_offset&&k<target_offset+tl.payload_bytes)||(k>=readback_offset&&k<readback_offset+tl.payload_bytes)))continue;
   if(o->memory[i][k]!=o->expected[i][k]){bad_case=test;bad_pool=i;bad_byte=k;e=EIO;goto save;}
   if(i==BP_TARGET)guards++;else readonly++;
  }
 save:;
  int collected=bridge_raw(directory,test,o->memory[BP_TARGET],o->sizes[BP_TARGET]);if(!collected)raw_files++;else if(!e)e=collected;
  if(e)goto done;
  completed++;
 }
 stage="complete";
 done:
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
 if(pending&&submits>finishes)bridge_progress(diagnostic_json,sizeof(diagnostic_json));
 diagnostic_overflow=bridge_mark_overflow?"true":"false";
#endif
 if(pending)cleanup=EBUSY;
 else{
#define CLEAN(ready,expression) do{if(ready){nn_nvn_clear_error();expression;cleanup=nn_nvn_error();if(cleanup)goto retain;ready=0;}}while(0)
  CLEAN(o->ready_command,nvnCommandBufferFinalize(&o->command));
  if(o->handle){nn_nvn_clear_error();nvnDeviceFinalizeCommandHandle(&o->device,o->handle);cleanup=nn_nvn_error();if(cleanup)goto retain;o->handle=0;}
  CLEAN(o->ready_tp,nvnTexturePoolFinalize(&o->tp));CLEAN(o->ready_sp,nvnSamplerPoolFinalize(&o->sp));CLEAN(o->ready_sampler,nvnSamplerFinalize(&o->sampler));CLEAN(o->ready_sample,nvnTextureFinalize(&o->sample));CLEAN(o->ready_target,nvnTextureFinalize(&o->target));CLEAN(o->ready_program,nvnProgramFinalize(&o->program));
  for(unsigned i=0;i<BV_BUFFERS;i++)CLEAN(o->ready_buffers[i],nvnBufferFinalize(&o->buffers[i]));
  for(unsigned i=0;i<BP_POOLS;i++)CLEAN(o->ready_pools[i],nvnMemoryPoolFinalize(&o->pools[i]));
  CLEAN(o->ready_queue,nvnQueueFinalize(&o->queue));CLEAN(o->ready_device,nvnDeviceFinalize(&o->device));
#undef CLEAN
 }
 retain:;
 int passed=!e&&!cleanup&&completed==4&&submits==4&&finishes==4&&raw_files==4&&channels==65536&&aliases==65536&&guards&&readonly;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"nvn_texture_bridge\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"completed_cases\":%u,\"submits\":%u,\"finishes\":%u,\"raw_files\":%u,\"channel_checks\":%llu,\"alias_channel_checks\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"target_offset\":%zu,\"readback_offset\":%zu,\"target_payload_bytes\":%llu,\"raw_bytes\":65536,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,\"bad_pool\":%u,\"bad_byte\":%zu,\"cpu_image_conversion\":false,\"original_game_execution\":false,\"passed\":%s}\n",
  (unsigned)getuid(),image?image->chip:0,stage,e,cleanup,cleanup?"true":"false",completed,submits,finishes,raw_files,(unsigned long long)channels,aliases,(unsigned long long)guards,(unsigned long long)readonly,target_offset,readback_offset,(unsigned long long)tl.payload_bytes,bad_case,bad_x,bad_y,bad_channel,bad_pool,bad_byte,passed?"true":"false");
 if(n>=2&&(size_t)n<capacity){
  size_t tail=(size_t)n-2;
  int extra=snprintf(json+tail,capacity-tail,",\"diagnostic_instrumented\":%s,\"diagnostic_mask\":%s,\"diagnostic_progress\":%s,\"diagnostic_overflow\":%s}\n",diagnostic_enabled,diagnostic_mask_text,diagnostic_json,diagnostic_overflow);
  if(extra<0||(size_t)extra>=capacity-tail)n=-1;else n=(int)(tail+(size_t)extra);
 }
 if(cleanup){o->next=retained_bridge_probes;retained_bridge_probes=o;}else{for(unsigned i=0;i<BP_POOLS;i++){free(o->memory[i]);free(o->expected[i]);}free(o);}
 return passed&&n>=0&&(size_t)n<capacity?0:-1;
#undef CALL
#undef BOOL
#undef REQUIRE
}
