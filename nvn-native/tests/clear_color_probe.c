/* SPDX-License-Identifier: MIT. Public NVN caller; only GPU makes clear pixels. */
#define _POSIX_C_SOURCE 200809L
#include "public_copy_probe.h"
#include "clear_color_cases.h"
#include "nvn_native/nvn.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_PUBLIC_COPY_HEADER
struct ncc_api {
#define ENTRY(name,type) type name;
#include "../src/nvn_entries.inc"
#undef ENTRY
};
struct ncc_active {
 NVNmemoryPool pools[2];NVNtexture textures[2];NVNcommandBuffer commands[3];NVNcommandHandle handles[4];
 unsigned pools_ready[2],textures_ready[2],commands_ready[3];
 unsigned char *memory[2],*arena_expected,*control[3],*control_expected[3];size_t sizes[2];
};
struct ncc_owner {struct ncc_owner *next;struct ncc_api api;NVNdevice device;NVNqueue queue;unsigned device_ready,queue_ready;struct ncc_active *active;};
static struct ncc_owner *retained;
static int api_load(struct ncc_api *a)
{
#define ENTRY(name,type) a->name=(type)nvnBootstrapLoader(#name);if(!a->name)return ENOSYS;
#include "../src/nvn_entries.inc"
#undef ENTRY
 return 0;
}
#define C(x) do{(x);int e_=nn_nvn_error();if(e_)return e_;}while(0)
#define B(x) do{int z_=(x),e_=nn_nvn_error();if(!z_||e_)return e_?e_:EIO;}while(0)
static int pool_create(struct ncc_owner *o,unsigned i,size_t size)
{
 struct ncc_active *a=o->active;struct ncc_api *p=&o->api;NVNmemoryPoolBuilder b={0};a->sizes[i]=size;
 int e=posix_memalign((void**)&a->memory[i],4096,size);if(e)return e;memset(a->memory[i],0xa5,size);
 C(p->nvnMemoryPoolBuilderSetDefaults(&b));C(p->nvnMemoryPoolBuilderSetDevice(&b,&o->device));C(p->nvnMemoryPoolBuilderSetFlags(&b,0xa4));C(p->nvnMemoryPoolBuilderSetStorage(&b,a->memory[i],size));B(p->nvnMemoryPoolInitialize(a->pools+i,&b));a->pools_ready[i]=1;return 0;
}
static int texture_create(struct ncc_owner *o,unsigned i,unsigned format,unsigned width,unsigned height,size_t offset,uint64_t expected)
{
 struct ncc_active *a=o->active;struct ncc_api *p=&o->api;NVNtextureBuilder b={0};
 C(p->nvnTextureBuilderSetDefaults(&b));C(p->nvnTextureBuilderSetDevice(&b,&o->device));C(p->nvnTextureBuilderSetSize2D(&b,width,height));C(p->nvnTextureBuilderSetFormat(&b,(NVNformat)format));
 if(expected&&p->nvnTextureBuilderGetStorageSize(&b)!=expected)return EINVAL;
 if(nn_nvn_error())return nn_nvn_error();
 C(p->nvnTextureBuilderSetStorage(&b,a->pools,offset));B(p->nvnTextureInitialize(a->textures+i,&b));a->textures_ready[i]=1;return 0;
}
static int set_target(struct ncc_owner *o,unsigned command,unsigned texture)
{const NVNtexture *targets[]={o->active->textures+texture};C(o->api.nvnCommandBufferSetRenderTargets(o->active->commands+command,1,targets,NULL,NULL,NULL));return 0;}
static int set_rect(struct ncc_owner *o,unsigned command,const struct nn_clear_color_rect *r)
{C(o->api.nvnCommandBufferSetScissor(o->active->commands+command,r->x,r->y,r->width,r->height));return 0;}
static int record(struct ncc_owner *o,const struct nn_clear_probe_case *c,unsigned *source_finalized,unsigned *call_count,unsigned *record_pins)
{
 struct ncc_active *a=o->active;struct ncc_api *p=&o->api;
 for(unsigned i=0;i<3;i++){
  int e=posix_memalign((void**)&a->control[i],16,8192);if(e)return e;memset(a->control[i],0xa5,8192);a->control_expected[i]=malloc(8192);if(!a->control_expected[i])return ENOMEM;
  B(p->nvnCommandBufferInitialize(a->commands+i,&o->device));a->commands_ready[i]=1;C(p->nvnCommandBufferAddCommandMemory(a->commands+i,a->pools+1,i*32768,32768));C(p->nvnCommandBufferAddControlMemory(a->commands+i,a->control[i],8192));
 }
 /* Seed only CPU recorder prerequisites. Neither child writes target/scissor
  * GPU-state deltas; the actual Call inherits those from its parent. */
 C(p->nvnCommandBufferBeginRecording(a->commands));int e=set_target(o,0,0);if(e)return e;
 const struct nn_clear_color_rect old={1,2,21,13};if((e=set_rect(o,0,&old)))return e;
 NVNcommandHandle seed=p->nvnCommandBufferEndRecording(a->commands);if(!seed||nn_nvn_error())return EINVAL;
 C(p->nvnDeviceFinalizeCommandHandle(&o->device,seed));
 for(unsigned op=0;op<2;op++){
  C(p->nvnCommandBufferBeginRecording(a->commands));C(p->nvnCommandBufferClearColor(a->commands,0,c->color[op],c->mask[op]));
  a->handles[op]=p->nvnCommandBufferEndRecording(a->commands);if(!a->handles[op]||nn_nvn_error())return EINVAL;
 }
 C(p->nvnCommandBufferBeginRecording(a->commands+1));if((e=set_target(o,1,1)))return e;
 for(unsigned op=0;op<2;op++){if((e=set_rect(o,1,c->rect+op)))return e;C(p->nvnCommandBufferClearColor(a->commands+1,0,c->color[op],c->mask[op]));}
 a->handles[2]=p->nvnCommandBufferEndRecording(a->commands+1);if(!a->handles[2]||nn_nvn_error())return EINVAL;
 C(p->nvnCommandBufferBeginRecording(a->commands+2));if((e=set_target(o,2,1)))return e;
 for(unsigned op=0;op<2;op++){if((e=set_rect(o,2,c->rect+op)))return e;C(p->nvnCommandBufferCallCommands(a->commands+2,1,a->handles+op));(*call_count)++;}
 a->handles[3]=p->nvnCommandBufferEndRecording(a->commands+2);if(!a->handles[3]||nn_nvn_error())return EINVAL;
 for(unsigned i=0;i<2;i++){C(p->nvnDeviceFinalizeCommandHandle(&o->device,a->handles[i]));a->handles[i]=0;(*source_finalized)++;}
 for(unsigned i=0;i<3;i++){C(p->nvnCommandBufferFinalize(a->commands+i));a->commands_ready[i]=0;memcpy(a->control_expected[i],a->control[i],8192);}
 a->arena_expected=malloc(a->sizes[1]);if(!a->arena_expected)return ENOMEM;memcpy(a->arena_expected,a->memory[1],a->sizes[1]);
 p->nvnTextureFinalize(a->textures);if(nn_nvn_error()!=EBUSY)return EINVAL;nn_nvn_clear_error();
 p->nvnTextureFinalize(a->textures+1);if(nn_nvn_error()!=EBUSY)return EINVAL;nn_nvn_clear_error();(*record_pins)+=2;return 0;
}
static int destroy_active(struct ncc_owner *o)
{
 struct ncc_active *a=o->active;if(!a)return 0;struct ncc_api *p=&o->api;nn_nvn_clear_error();
 for(unsigned i=0;i<4;i++)if(a->handles[i]){C(p->nvnDeviceFinalizeCommandHandle(&o->device,a->handles[i]));a->handles[i]=0;}
 for(unsigned i=0;i<3;i++)if(a->commands_ready[i]){C(p->nvnCommandBufferFinalize(a->commands+i));a->commands_ready[i]=0;}
 for(unsigned i=0;i<2;i++)if(a->textures_ready[i]){C(p->nvnTextureFinalize(a->textures+i));a->textures_ready[i]=0;}
 for(unsigned i=0;i<2;i++)if(a->pools_ready[i]){C(p->nvnMemoryPoolFinalize(a->pools+i));a->pools_ready[i]=0;}
 for(unsigned i=0;i<2;i++)free(a->memory[i]);
 for(unsigned i=0;i<3;i++){free(a->control[i]);free(a->control_expected[i]);}
 free(a->arena_expected);free(a);o->active=NULL;return 0;
}
static int raw(const char *dir,unsigned n,unsigned origin,unsigned phase,unsigned repeat,const struct ncc_active *a)
{
 char path[1200];if(snprintf(path,sizeof(path),"%s/clear-color-%03u-o%u-p%u-r%u.raw",dir,n,origin,phase,repeat)>=(int)sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;FILE *f=fdopen(fd,"wb");if(!f){int e=errno;close(fd);return e;}
 int e=fwrite(a->memory[0],1,a->sizes[0],f)!=a->sizes[0]?EIO:0;if(fflush(f)||fsync(fd))e=EIO;if(fclose(f))e=EIO;return e;
}
#undef C
#undef B
int nn_public_copy_probe(const char *directory,char *json,size_t capacity)
{
 struct ncc_owner *o=NULL;int error=0,cleanup=0,pending=0;const char *stage="arguments";
 unsigned completed=0,submits=0,finishes=0,files=0,source_finalized=0,calls=0,busy=0,record_pins=0,bad_case=~0u,bad_origin=~0u,bad_phase=~0u,bad_repeat=~0u;
 uint64_t raw_bytes=0,readonly=0;uint32_t chip=0;
#define TRY(x) do{if((error=(x)))goto done;}while(0)
#define CALL(x) do{(x);if((error=nn_nvn_error()))goto done;}while(0)
#define BOOL(x) do{int z_=(x);error=nn_nvn_error();if(!z_||error){if(!error)error=EIO;goto done;}}while(0)
#define REQUIRE(x) do{if(!(x)){error=EINVAL;goto done;}}while(0)
 if(!directory||!json||!capacity)return -1;
 o=calloc(1,sizeof(*o));if(!o){error=ENOMEM;goto done;}TRY(api_load(&o->api));struct ncc_api *p=&o->api;nn_nvn_clear_error();
 stage="inspect";struct nn_device_identity identity;TRY(nn_device_inspect(&identity));chip=identity.chip_id;REQUIRE(chip==0x06050002&&identity.page_bytes==4096);
 NVNdeviceBuilder db={0};NVNqueueBuilder qb={0};stage="device";CALL(p->nvnDeviceBuilderSetDefaults(&db));BOOL(p->nvnDeviceInitialize(&o->device,&db));o->device_ready=1;TRY(nn_nvn_register_clear_color(&o->device,&NVN_COMPILED_IMAGE));CALL(p->nvnQueueBuilderSetDefaults(&qb));CALL(p->nvnQueueBuilderSetDevice(&qb,&o->device));
 for(unsigned n=0;n<NN_CLEAR_PROBE_CASES;n++)for(unsigned origin=0;origin<2;origin++){
  bad_case=n;bad_origin=origin;stage="queue";BOOL(p->nvnQueueInitialize(&o->queue,&qb));o->queue_ready=1;struct nn_clear_probe_case c;TRY(nn_clear_probe_case(n,&c));
  stage="pools";o->active=calloc(1,sizeof(*o->active));if(!o->active){error=ENOMEM;goto done;}struct ncc_active *a=o->active;TRY(pool_create(o,0,c.pool_bytes));TRY(pool_create(o,1,131072));
  stage="textures";TRY(texture_create(o,0,63,64,64,512,0));TRY(texture_create(o,1,c.format,c.width,c.height,NN_CLEAR_PROBE_TARGET_OFFSET,c.source.payload_bytes));
  CALL(p->nvnDeviceSetWindowOriginMode(&o->device,(NVNwindowOriginMode)origin));stage="public-record";TRY(record(o,&c,&source_finalized,&calls,&record_pins));
  for(unsigned phase=0;phase<2;phase++)for(unsigned repeat=0;repeat<2;repeat++){
   bad_phase=phase;bad_repeat=repeat;stage="raw-input";memset(a->memory[0],nn_clear_probe_seed(n,origin,phase,repeat),a->sizes[0]);
   stage="public-flush";for(unsigned i=0;i<2;i++)CALL(p->nvnMemoryPoolFlushMappedRange(a->pools+i,0,a->sizes[i]));
   stage="public-submit";pending=1;CALL(p->nvnQueueSubmitCommands(&o->queue,1,a->handles+2+phase));submits++;
   p->nvnTextureFinalize(a->textures+1);REQUIRE(nn_nvn_error()==EBUSY);nn_nvn_clear_error();busy++;
   stage="public-finish";CALL(p->nvnQueueFinish(&o->queue));pending=0;finishes++;
   stage="public-invalidate";for(unsigned i=0;i<2;i++)CALL(p->nvnMemoryPoolInvalidateMappedRange(a->pools+i,0,a->sizes[i]));
   stage="save-actual";TRY(raw(directory,n,origin,phase,repeat,a));files++;raw_bytes+=a->sizes[0];
   stage="command-control-readonly";REQUIRE(!memcmp(a->memory[1],a->arena_expected,a->sizes[1]));readonly+=a->sizes[1];
   for(unsigned i=0;i<3;i++){REQUIRE(!memcmp(a->control[i],a->control_expected[i],8192));readonly+=8192;}
   completed++;
  }
  stage="retire-queue";CALL(p->nvnQueueFinalize(&o->queue));o->queue_ready=0;
  stage="retire-case";TRY(destroy_active(o));
 }
 stage="complete";
done:
 if(pending)cleanup=EBUSY;
 else if(o){cleanup=destroy_active(o);if(!cleanup){nn_nvn_clear_error();if(o->queue_ready){o->api.nvnQueueFinalize(&o->queue);cleanup=nn_nvn_error();if(!cleanup)o->queue_ready=0;}if(!cleanup&&o->device_ready){o->api.nvnDeviceFinalize(&o->device);cleanup=nn_nvn_error();if(!cleanup)o->device_ready=0;}}}
 if(o){if(cleanup){o->next=retained;retained=o;}else free(o);}
 int passed=!error&&!cleanup&&completed==NN_CLEAR_PROBE_FILES&&submits==NN_CLEAR_PROBE_FILES&&finishes==NN_CLEAR_PROBE_FILES&&files==NN_CLEAR_PROBE_FILES&&source_finalized==NN_CLEAR_PROBE_CASES*4&&calls==NN_CLEAR_PROBE_CASES*4&&busy==NN_CLEAR_PROBE_FILES&&record_pins==NN_CLEAR_PROBE_CASES*4;
 int z=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"public_clear_color\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"cases\":%u,\"formats\":22,\"origins\":2,\"phases\":2,\"repeats\":2,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"completed\":%u,\"source_public_finalizations\":%u,\"CallCommands_calls\":%u,\"CopyCommands_calls\":0,\"recorded_texture_finalize_rejections\":%u,\"inflight_texture_finalize_rejections\":%u,\"data_bytes\":%llu,\"command_control_readonly_bytes\":%llu,\"bad_case\":%u,\"bad_origin\":%u,\"bad_phase\":%u,\"bad_repeat\":%u,\"pixel_oracle_on_host\":true,\"nvn_api\":true,\"cpu_image_conversion\":false,\"probe_raw_submit_calls\":0,\"probe_private_encode_calls\":0,\"game_modules_executed\":false,\"passed\":%s}\n",(unsigned)getuid(),chip,stage,error,cleanup,cleanup?"true":"false",NN_CLEAR_PROBE_CASES,submits,finishes,files,completed,source_finalized,calls,record_pins,busy,(unsigned long long)raw_bytes,(unsigned long long)readonly,passed?~0u:bad_case,passed?~0u:bad_origin,passed?~0u:bad_phase,passed?~0u:bad_repeat,passed?"true":"false");
 return passed&&z>=0&&(size_t)z<capacity?0:-1;
}
