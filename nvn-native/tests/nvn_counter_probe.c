/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "nvn_counter_probe.h"
#include "nvn_native/nvn.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_GRAPHICS_API_HEADER
/* Single owner thread: no concurrent entry into the current native NVN core.
 * Independent queues still submit real distinct KGSL contexts, per kgsl.c's
 * DRAWCTXT_CREATE per nn_queue_create; timestamps establish actual overlap. */
struct api {
#define ENTRY(name,type) type name;
#include "nvn_entries.inc"
#undef ENTRY
};
static struct api a;
enum {SHADER,TARGET0,TARGET1,VERTEX0,VERTEX1,MATRIX0,MATRIX1,COMMAND0,COMMAND1,RESULT,POOLS};
enum {REPLAY,CROSS,RESET_ONLY,DRAW_ONE,OTHER,PRIMARY_ONLY,LONG_PRIMARY,LONG_OTHER,HANDLES};
enum {OBS=15,STRESS_PRIMARY=4096,STRESS_OTHER=8};
static const char *const filenames[]={"counter-replay-0.rgba","counter-replay-1.rgba","counter-cross-0.rgba","counter-cross-1.rgba","counter-reset-zero.rgba","counter-after-reset.rgba","counter-other-context.rgba","counter-primary-gap.rgba","counter-overlap-primary.rgba","counter-overlap-other.rgba"};
static const char *const observation_names[]={"replay0","replay1","cross0","cross1","reset_zero","after_reset","other_context","primary_gap","primary_after_gap","overlap_primary_top","overlap_primary_bottom","overlap_primary_samples","overlap_other_top","overlap_other_bottom","overlap_other_samples"};
static const uint64_t expected_samples[]={4096,4096,8192,12288,0,4096,12288,0,4096,0,0,STRESS_PRIMARY*UINT64_C(4096),0,0,STRESS_OTHER*UINT64_C(4096)};
static const unsigned slot_for_observation[]={0,0,1,1,2,3,4,5,3,6,7,8,9,10,11};
struct owner {
 NVNdevice device;NVNqueue queues[2];NVNprogram program;NVNtexture textures[2];NVNcommandBuffer commands[2];NVNmemoryPool pools[POOLS];
 NVNdeviceBuilder db;NVNqueueBuilder qb;NVNmemoryPoolBuilder pb;NVNtextureBuilder tb;
 unsigned char *memory[POOLS],*shadow[POOLS];size_t sizes[POOLS],vs_offset,fs_offset;
 _Alignas(16) unsigned char control[2][65536];NVNcommandHandle handles[HANDLES];
 NVNbufferAddress addresses[POOLS];NVNcounterData actual[OBS];uint64_t ns[OBS];unsigned observed[OBS];
 unsigned device_ready,queue_ready[2],program_ready,texture_ready[2],command_ready[2],pool_ready[POOLS];
 unsigned pending,retained,submits,finishes,recorded,finalized,pixels,readbacks,sample_checks,clock_checks,convert_checks,overlap,between_context_checks;
 uint64_t guards,readonly;
 const char *stage;int error,cleanup;
};
static struct owner *active;
#define CALL(s,x) do{x;int call_error=nn_nvn_error();if(call_error)return call_error;}while(0)
#define BOOL(s,x) do{int ok=(x),call_error=nn_nvn_error();if(!ok||call_error)return call_error?call_error:EIO;}while(0)
static NVNbufferAddress output_address(struct owner *s,unsigned slot){return s->addresses[RESULT]+256+slot*32;}
static int record(struct owner *s,unsigned handle,unsigned queue,unsigned reset,unsigned draws,int report_slot,int top_slot,int bottom_slot)
{
 NVNcommandBuffer *c=&s->commands[queue];
 CALL(s,a.nvnCommandBufferBeginRecording(c));
 if(reset)CALL(s,a.nvnCommandBufferResetCounter(c,NVN_COUNTER_TYPE_SAMPLES_PASSED));
 if(top_slot>=0)CALL(s,a.nvnCommandBufferReportCounter(c,NVN_COUNTER_TYPE_TIMESTAMP_TOP,output_address(s,(unsigned)top_slot)));
 if(draws){
  NVNvertexAttribState attributes[2];NVNvertexStreamState stream;
  CALL(s,a.nvnCommandBufferBindProgram(c,&s->program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
  CALL(s,a.nvnCommandBufferBindUniformBuffer(c,NVN_SHADER_STAGE_VERTEX,0,s->addresses[MATRIX0+queue]+64,64));
  for(unsigned i=0;i<2;i++){CALL(s,a.nvnVertexAttribStateSetDefaults(attributes+i));CALL(s,a.nvnVertexAttribStateSetFormat(attributes+i,i?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,i?8:0));CALL(s,a.nvnVertexAttribStateSetStreamIndex(attributes+i,0));}
  CALL(s,a.nvnVertexStreamStateSetDefaults(&stream));CALL(s,a.nvnVertexStreamStateSetStride(&stream,24));CALL(s,a.nvnVertexStreamStateSetDivisor(&stream,0));
  CALL(s,a.nvnCommandBufferBindVertexAttribState(c,2,attributes));CALL(s,a.nvnCommandBufferBindVertexStreamState(c,1,&stream));
  CALL(s,a.nvnCommandBufferBindVertexBuffer(c,0,s->addresses[VERTEX0+queue]+256,72));
  const NVNtexture *target=&s->textures[queue];CALL(s,a.nvnCommandBufferSetRenderTargets(c,1,&target,NULL,NULL,NULL));
  CALL(s,a.nvnCommandBufferSetViewport(c,0,0,64,64));CALL(s,a.nvnCommandBufferSetScissor(c,0,0,64,64));CALL(s,a.nvnCommandBufferSetDepthRange(c,0,1));
  for(unsigned i=0;i<draws;i++)CALL(s,a.nvnCommandBufferDrawArrays(c,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
 }
 if(bottom_slot>=0)CALL(s,a.nvnCommandBufferReportCounter(c,NVN_COUNTER_TYPE_TIMESTAMP,output_address(s,(unsigned)bottom_slot)));
 if(report_slot>=0)CALL(s,a.nvnCommandBufferReportCounter(c,NVN_COUNTER_TYPE_SAMPLES_PASSED,output_address(s,(unsigned)report_slot)));
 s->handles[handle]=a.nvnCommandBufferEndRecording(c);int e=nn_nvn_error();if(e||!s->handles[handle])return e?e:EIO;
 s->recorded++;return 0;
}
static int submit(struct owner *s,unsigned handle,unsigned q)
{
 /* Before an ioctl may accept work, retain every owner even on unknown error. */
 s->pending|=1u<<q;a.nvnQueueSubmitCommands(&s->queues[q],1,&s->handles[handle]);int e=nn_nvn_error();if(e)return e;s->submits++;return 0;
}
static int finish(struct owner *s,unsigned q)
{
 a.nvnQueueFinish(&s->queues[q]);int e=nn_nvn_error();if(e)return e;s->pending&=~(1u<<q);s->finishes++;return 0;
}
static int observation(struct owner *s,unsigned index)
{
 if(s->pending)return EBUSY;
 CALL(s,a.nvnMemoryPoolInvalidateMappedRange(&s->pools[RESULT],0,s->sizes[RESULT]));
 memcpy(s->actual+index,s->memory[RESULT]+256+slot_for_observation[index]*32,sizeof(NVNcounterData));
 NVNcounterData *d=s->actual+index;s->observed[index]=1;
 int clock=index==9||index==10||index==12||index==13;
 if(clock){if(!d->timestamp||d->counter!=d->timestamp)return EIO;s->clock_checks++;}
 else {if(d->counter!=expected_samples[index]||!d->timestamp)return EIO;s->sample_checks++;}
 s->ns[index]=a.nvnDeviceGetTimestampInNanoseconds(&s->device,d);int e=nn_nvn_error();if(e)return e;
 __uint128_t expected=(__uint128_t)d->timestamp*625/12;
 if(expected>UINT64_MAX||s->ns[index]!=(uint64_t)expected)return EIO;
 s->convert_checks++;return 0;
}
static int guards(struct owner *s)
{
 if(s->pending)return EBUSY;
 for(unsigned p=0;p<POOLS;p++){
  CALL(s,a.nvnMemoryPoolInvalidateMappedRange(&s->pools[p],0,s->sizes[p]));
  for(size_t k=0;k<s->sizes[p];k++){
   if((p==TARGET0||p==TARGET1)&&k>=4096&&k<20480)continue;
   if(p==RESULT&&k>=256&&k<256+12*32&&(k-256)%32<16)continue;
   if(s->memory[p][k]!=s->shadow[p][k])return EIO;
   if(p==TARGET0||p==TARGET1||p==RESULT)s->guards++;else s->readonly++;
  }
 }
 return 0;
}
static int readback(struct owner *s,const char *directory,unsigned file,unsigned q)
{
 if(s->pending||file>=sizeof(filenames)/sizeof(*filenames))return EBUSY;
 int e=guards(s);if(e)return e;
 unsigned char *target=s->memory[TARGET0+q]+4096;
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)for(unsigned c=0;c<4;c++){
  unsigned expected=(c==q||c==3)?255:0;if(target[y*256+x*4+c]!=expected)return EIO;s->pixels++;
 }
 char path[1400];int n=snprintf(path,sizeof(path),"%s/%s",directory,filenames[file]);if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;
 size_t at=0;while(at<16384){ssize_t nbytes=write(fd,target+at,16384-at);if(nbytes<=0){e=nbytes<0?errno:EIO;break;}at+=(size_t)nbytes;}
 if(!e&&fsync(fd))e=errno;
 if(close(fd)&&!e)e=errno;
 if(!e)s->readbacks++;
 return e;
}
static int setup(struct owner *s)
{
 const struct nn_gfx_image *image=nn_test_graphics.image;
 if(!image||image->schema!=3||image->chip!=0x06050002||image->profile!=NN_GFX_COLOR||image->target_bytes!=16384||image->target_pitch!=256||image->width!=64||image->height!=64||image->vertex_bytes!=72||image->matrix_bytes!=64||sysconf(_SC_PAGESIZE)!=4096)return ENOTSUP;
 if(!nn_test_graphics.vs.source_bytes||!nn_test_graphics.fs.source_bytes||nn_test_graphics.vs.source_bytes>1048576||nn_test_graphics.fs.source_bytes>1048576)return ENOTSUP;
 CALL(s,a.nvnDeviceBuilderSetDefaults(&s->db));BOOL(s,a.nvnDeviceInitialize(&s->device,&s->db));s->device_ready=1;
 int e=nn_nvn_register_graphics(&s->device,&nn_test_graphics);if(e)return e;
 CALL(s,a.nvnDeviceSetWindowOriginMode(&s->device,NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT));CALL(s,a.nvnDeviceSetDepthMode(&s->device,NVN_DEPTH_MODE_NEAR_IS_MINUS_W));
 for(unsigned q=0;q<2;q++){
  CALL(s,a.nvnQueueBuilderSetDefaults(&s->qb));CALL(s,a.nvnQueueBuilderSetDevice(&s->qb,&s->device));
  CALL(s,a.nvnQueueBuilderSetCommandMemorySize(&s->qb,131072));CALL(s,a.nvnQueueBuilderSetControlMemorySize(&s->qb,131072));
  BOOL(s,a.nvnQueueInitialize(&s->queues[q],&s->qb));s->queue_ready[q]=1;
 }
 s->vs_offset=4096;s->fs_offset=((s->vs_offset+nn_test_graphics.vs.source_bytes+4095)&~(size_t)4095)+4096;
 s->sizes[SHADER]=((s->fs_offset+nn_test_graphics.fs.source_bytes+4095)&~(size_t)4095)+4096;
 for(unsigned p=1;p<POOLS;p++)s->sizes[p]=(p==TARGET0||p==TARGET1)?24576:(p==COMMAND0||p==COMMAND1)?262144:4096;
 for(unsigned p=0;p<POOLS;p++){
  e=posix_memalign((void **)&s->memory[p],4096,s->sizes[p]);if(e)return e;memset(s->memory[p],0xa5,s->sizes[p]);
  if(p==SHADER){memcpy(s->memory[p]+s->vs_offset,nn_test_graphics.vs.source_code,nn_test_graphics.vs.source_bytes);memcpy(s->memory[p]+s->fs_offset,nn_test_graphics.fs.source_code,nn_test_graphics.fs.source_bytes);}
  for(unsigned q=0;q<2;q++){
   if(p==VERTEX0+q){float v[3][6]={{-1,-1,0,0,0,1},{3,-1,0,0,0,1},{-1,3,0,0,0,1}};for(unsigned i=0;i<3;i++)v[i][2+q]=1;memcpy(s->memory[p]+256,v,sizeof(v));}
   if(p==MATRIX0+q){float matrix[16]={0};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;memcpy(s->memory[p]+64,matrix,sizeof(matrix));}
  }
  CALL(s,a.nvnMemoryPoolBuilderSetDefaults(&s->pb));CALL(s,a.nvnMemoryPoolBuilderSetDevice(&s->pb,&s->device));CALL(s,a.nvnMemoryPoolBuilderSetStorage(&s->pb,s->memory[p],s->sizes[p]));
  CALL(s,a.nvnMemoryPoolBuilderSetFlags(&s->pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED|(p==SHADER?NVN_MEMORY_POOL_FLAGS_SHADER_CODE:0)));
  BOOL(s,a.nvnMemoryPoolInitialize(&s->pools[p],&s->pb));s->pool_ready[p]=1;s->addresses[p]=a.nvnMemoryPoolGetBufferAddress(&s->pools[p]);if(!s->addresses[p]||nn_nvn_error())return EIO;
  CALL(s,a.nvnMemoryPoolFlushMappedRange(&s->pools[p],0,s->sizes[p]));
 }
 BOOL(s,a.nvnProgramInitialize(&s->program,&s->device));s->program_ready=1;
 NVNshaderData stages[2]={{s->addresses[SHADER]+s->fs_offset,nn_test_graphics.fs.source_control},{s->addresses[SHADER]+s->vs_offset,nn_test_graphics.vs.source_control}};
 BOOL(s,a.nvnProgramSetShaders(&s->program,2,stages));
 for(unsigned q=0;q<2;q++){
  CALL(s,a.nvnTextureBuilderSetDefaults(&s->tb));CALL(s,a.nvnTextureBuilderSetDevice(&s->tb,&s->device));CALL(s,a.nvnTextureBuilderSetFlags(&s->tb,NVN_TEXTURE_FLAGS_LINEAR));CALL(s,a.nvnTextureBuilderSetTarget(&s->tb,NVN_TEXTURE_TARGET_2D));CALL(s,a.nvnTextureBuilderSetSize2D(&s->tb,64,64));CALL(s,a.nvnTextureBuilderSetLevels(&s->tb,1));CALL(s,a.nvnTextureBuilderSetSamples(&s->tb,0));CALL(s,a.nvnTextureBuilderSetFormat(&s->tb,NVN_FORMAT_RGBA8));CALL(s,a.nvnTextureBuilderSetStride(&s->tb,256));CALL(s,a.nvnTextureBuilderSetStorage(&s->tb,&s->pools[TARGET0+q],4096));BOOL(s,a.nvnTextureInitialize(&s->textures[q],&s->tb));s->texture_ready[q]=1;
  BOOL(s,a.nvnCommandBufferInitialize(&s->commands[q],&s->device));s->command_ready[q]=1;CALL(s,a.nvnCommandBufferAddCommandMemory(&s->commands[q],&s->pools[COMMAND0+q],0,s->sizes[COMMAND0+q]));memset(s->control[q],0xa5,sizeof(s->control[q]));CALL(s,a.nvnCommandBufferAddControlMemory(&s->commands[q],s->control[q],sizeof(s->control[q])));
 }
 if((e=record(s,REPLAY,0,1,1,0,-1,-1))||(e=record(s,CROSS,0,0,1,1,-1,-1))||(e=record(s,RESET_ONLY,0,1,0,2,-1,-1))||(e=record(s,DRAW_ONE,0,0,1,3,-1,-1))||(e=record(s,OTHER,1,1,3,4,-1,-1))||(e=record(s,PRIMARY_ONLY,0,0,0,5,-1,-1))||(e=record(s,LONG_PRIMARY,0,1,STRESS_PRIMARY,8,6,7))||(e=record(s,LONG_OTHER,1,1,STRESS_OTHER,11,9,10)))return e;
 for(unsigned p=0;p<POOLS;p++){s->shadow[p]=malloc(s->sizes[p]);if(!s->shadow[p])return ENOMEM;memcpy(s->shadow[p],s->memory[p],s->sizes[p]);}
 return 0;
}
static void cleanup(struct owner *s)
{
 if(s->pending){s->cleanup=EBUSY;s->retained=1;return;}
#define CLEAN(x) do{nn_nvn_clear_error();x;int ce=nn_nvn_error();if(ce){s->cleanup=ce;s->retained=1;return;}}while(0)
 for(unsigned q=0;q<2;q++)if(s->command_ready[q]){CLEAN(a.nvnCommandBufferFinalize(&s->commands[q]));s->command_ready[q]=0;}
 for(unsigned h=0;h<HANDLES;h++)if(s->handles[h]){CLEAN(a.nvnDeviceFinalizeCommandHandle(&s->device,s->handles[h]));s->handles[h]=0;s->finalized++;}
 for(unsigned q=0;q<2;q++)if(s->queue_ready[q]){CLEAN(a.nvnQueueFinalize(&s->queues[q]));s->queue_ready[q]=0;}
 for(unsigned q=0;q<2;q++)if(s->texture_ready[q]){CLEAN(a.nvnTextureFinalize(&s->textures[q]));s->texture_ready[q]=0;}
 if(s->program_ready){CLEAN(a.nvnProgramFinalize(&s->program));s->program_ready=0;}
 for(unsigned p=0;p<POOLS;p++)if(s->pool_ready[p]){CLEAN(a.nvnMemoryPoolFinalize(&s->pools[p]));s->pool_ready[p]=0;}
 if(s->device_ready){CLEAN(a.nvnDeviceFinalize(&s->device));s->device_ready=0;}
 for(unsigned p=0;p<POOLS;p++){free(s->memory[p]);free(s->shadow[p]);s->memory[p]=s->shadow[p]=NULL;}
#undef CLEAN
}
int nn_nvn_counter_probe(const char *directory,char *json,size_t capacity)
{
 if(!directory||!json||capacity<16384)return -1;
 if(active){snprintf(json,capacity,"{\"operation\":\"nvn_counter\",\"passed\":false,\"errno\":%d}",EBUSY);return -1;}
 struct owner *s=calloc(1,sizeof(*s));if(!s)return -1;active=s;s->stage="resolver";
#define ENTRY(name,type) a.name=(type)nvnBootstrapLoader(#name);if(!a.name)s->error=ENOSYS;
#include "nvn_entries.inc"
#undef ENTRY
 nn_nvn_clear_error();if(s->error)goto done;s->stage="setup";s->error=setup(s);if(s->error)goto done;
#define TRY(x) do{s->error=(x);if(s->error)goto done;}while(0)
 s->stage="same-handle-replay";
 for(unsigned i=0;i<2;i++){TRY(submit(s,REPLAY,0));TRY(finish(s,0));TRY(observation(s,i));TRY(readback(s,directory,i,0));}
 s->stage="cross-submit-accumulator";
 for(unsigned i=0;i<2;i++){TRY(submit(s,CROSS,0));TRY(finish(s,0));TRY(observation(s,2+i));TRY(readback(s,directory,2+i,0));}
 s->stage="reset-zero";TRY(submit(s,RESET_ONLY,0));TRY(finish(s,0));TRY(observation(s,4));TRY(readback(s,directory,4,0));
 s->stage="after-reset";TRY(submit(s,DRAW_ONE,0));TRY(finish(s,0));TRY(observation(s,5));TRY(readback(s,directory,5,0));
 s->stage="other-context-gap";TRY(submit(s,RESET_ONLY,0));TRY(finish(s,0));TRY(submit(s,OTHER,1));TRY(finish(s,1));TRY(observation(s,6));TRY(readback(s,directory,6,1));TRY(submit(s,PRIMARY_ONLY,0));TRY(finish(s,0));TRY(observation(s,7));s->between_context_checks++;TRY(submit(s,DRAW_ONE,0));TRY(finish(s,0));TRY(observation(s,8));TRY(readback(s,directory,7,0));
 s->stage="within-query-context-scheduling";
 /* No blocking GPU gate and no concurrent core calls: submit independent
  * contexts back-to-back. Actual TOP/BOTTOM intervals must prove execution of
  * secondary work inside primary query interval; merely queued work fails. */
 TRY(submit(s,LONG_PRIMARY,0));TRY(submit(s,LONG_OTHER,1));TRY(finish(s,0));TRY(finish(s,1));
 int query_error=0;for(unsigned i=9;i<OBS;i++){int qe=observation(s,i);if(qe&&!query_error)query_error=qe;}
 TRY(readback(s,directory,8,0));TRY(readback(s,directory,9,1));
 s->overlap=s->actual[9].timestamp<s->actual[12].timestamp && s->actual[12].timestamp<s->actual[13].timestamp && s->actual[13].timestamp<s->actual[10].timestamp;
 if(query_error){s->error=query_error;goto done;}
 if(!s->overlap){s->error=ENODATA;s->stage="context-overlap-not-observed";goto done;}
 s->stage="complete";
done:
 cleanup(s);
 int passed=!s->error&&!s->cleanup&&!s->retained&&s->overlap&&s->between_context_checks==1&&s->sample_checks==11&&s->clock_checks==4&&s->convert_checks==15&&s->readbacks==10&&s->pixels==163840&&s->submits==12&&s->finishes==12&&s->recorded==8&&s->finalized==8;
 size_t at=0;int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"nvn_counter\",\"uid\":%u,\"chip_id\":%u,\"image_schema\":3,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"passed\":%s,\"resources_retained\":%s,\"submissions\":%u,\"finishes\":%u,\"handles_recorded\":%u,\"handles_released\":%u,\"readbacks\":%u,\"channel_checks\":%u,\"sample_checks\":%u,\"clock_checks\":%u,\"conversion_checks\":%u,\"guard_checks\":%" PRIu64 ",\"readonly_checks\":%" PRIu64 ",\"queues_created\":2,\"context_overlap_observed\":%s,\"between_context_checks\":%u,\"primary_stress_draws\":%u,\"secondary_stress_draws\":%u,\"cpu_counter_estimates\":0,\"cpu_target_writes_after_import\":0,\"game_modules_executed\":false,\"window_presented\":false,\"observations\":[",(unsigned)getuid(),0x06050002u,s->stage,s->error,s->cleanup,passed?"true":"false",s->retained?"true":"false",s->submits,s->finishes,s->recorded,s->finalized,s->readbacks,s->pixels,s->sample_checks,s->clock_checks,s->convert_checks,s->guards,s->readonly,s->overlap?"true":"false",s->between_context_checks,STRESS_PRIMARY,STRESS_OTHER);
 if(n<0||(size_t)n>=capacity){s->retained=1;return -1;}at=(size_t)n;
 for(unsigned i=0;i<OBS;i++){n=snprintf(json+at,capacity-at,"%s{\"name\":\"%s\",\"observed\":%s,\"counter\":%" PRIu64 ",\"timestamp\":%" PRIu64 ",\"nanoseconds\":%" PRIu64 "}",i?",":"",observation_names[i],s->observed[i]?"true":"false",s->actual[i].counter,s->actual[i].timestamp,s->ns[i]);if(n<0||(size_t)n>=capacity-at){s->retained=1;return -1;}at+=(size_t)n;}
 n=snprintf(json+at,capacity-at,"]}\n");if(n<0||(size_t)n>=capacity-at){s->retained=1;return -1;}
 if(!s->retained){free(s);active=NULL;}return passed?0:-1;
#undef TRY
}
