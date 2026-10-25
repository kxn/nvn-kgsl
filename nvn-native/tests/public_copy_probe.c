/* SPDX-License-Identifier: MIT. Actual public NVN copy caller, authored bytes. */
#define _POSIX_C_SOURCE 200809L
#include "public_copy_probe.h"
#include "nvn_native/nvn.h"
#include "nvn_native/kgsl.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_PUBLIC_COPY_HEADER
#ifndef NVN_PUBLIC_COPY_CALL
#define NVN_PUBLIC_COPY_CALL 0
#endif
#if NVN_PUBLIC_COPY_CALL
#define NN_PUBLIC_COPY_HANDLE_FIELDS "\"CopyCommands_calls\":0,\"CallCommands_calls\":%u,\"call_commands\":true,"
#else
#define NN_PUBLIC_COPY_HANDLE_FIELDS "\"CopyCommands_calls\":%u,"
#endif
#ifndef NN_PUBLIC_COPY_OPERATION
#define NN_PUBLIC_COPY_OPERATION "public_canonical_copy"
#define NN_PUBLIC_COPY_PREFIX "public-copy"
#define NN_PUBLIC_COPY_REGISTER(d) nn_nvn_register_block_copy((d),&NVN_COMPILED_IMAGE)
#endif
#ifndef NN_PUBLIC_COPY_FORMATS_JSON
#define NN_PUBLIC_COPY_FORMATS_JSON "37"
#endif
#ifndef NN_PUBLIC_COPY_EXPERIMENT_JSON
#define NN_PUBLIC_COPY_EXPERIMENT_JSON ""
#endif
struct npc_api {
#define ENTRY(name,type) type name;
#include "../src/nvn_entries.inc"
#undef ENTRY
};
struct npc_active {
 NVNmemoryPool pools[3];NVNbuffer buffers[2];NVNtexture textures[2];NVNcommandBuffer commands[2];NVNcommandHandle handles[2];
 unsigned pools_ready[3],buffers_ready[2],textures_ready[2],commands_ready[2];
 unsigned char *memory[3],*expected[3],*control[2],*control_expected[2];size_t sizes[3];
};
struct npc_owner {struct npc_owner *next;struct npc_api api;NVNdevice device;NVNqueue queue;unsigned device_ready,queue_ready;struct npc_active *active;};
static struct npc_owner *retained;
static int api_load(struct npc_api *a)
{
#define ENTRY(name,type) a->name=(type)nvnBootstrapLoader(#name);if(!a->name)return ENOSYS;
#include "../src/nvn_entries.inc"
#undef ENTRY
 return 0;
}
#define C(x) do{(x);int e_=nn_nvn_error();if(e_)return e_;}while(0)
#define B(x) do{int ok_=(x),e_=nn_nvn_error();if(!ok_||e_)return e_?e_:EIO;}while(0)
#define R(x) do{int ok_=!!(x),e_=nn_nvn_error();if(!ok_||e_)return e_?e_:EINVAL;}while(0)
static unsigned char pattern(uint64_t offset,unsigned n,unsigned replay)
{uint32_t x=(uint32_t)offset*1664525u+1013904223u+(n+1u)*2246822519u+replay*3266489917u;x^=x>>13;x*=1274126177u;x^=x>>16;return (unsigned char)x;}
#define coordinate nn_public_copy_oracle_offset
static int equal(const void *a,const void *b,size_t n,size_t *bad)
{const unsigned char *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i]){*bad=i;return EIO;}return 0;}
static int pool_create(struct npc_owner *o,unsigned index,size_t bytes)
{
 struct npc_active *a=o->active;struct npc_api *p=&o->api;NVNmemoryPoolBuilder b={0};a->sizes[index]=bytes;
 int e=posix_memalign((void**)&a->memory[index],4096,bytes);if(e)return e;memset(a->memory[index],0xa5,bytes);a->expected[index]=malloc(bytes);if(!a->expected[index])return ENOMEM;
 C(p->nvnMemoryPoolBuilderSetDefaults(&b));C(p->nvnMemoryPoolBuilderSetDevice(&b,&o->device));C(p->nvnMemoryPoolBuilderSetFlags(&b,0xa4));C(p->nvnMemoryPoolBuilderSetStorage(&b,a->memory[index],bytes));B(p->nvnMemoryPoolInitialize(a->pools+index,&b));a->pools_ready[index]=1;
 R(p->nvnMemoryPoolMap(a->pools+index)==a->memory[index]);R(p->nvnMemoryPoolGetBufferAddress(a->pools+index)!=0);return 0;
}
static int buffer_create(struct npc_owner *o,unsigned index,unsigned pool,size_t offset,size_t bytes)
{
 struct npc_active *a=o->active;struct npc_api *p=&o->api;NVNbufferBuilder b={0};C(p->nvnBufferBuilderSetDefaults(&b));C(p->nvnBufferBuilderSetDevice(&b,&o->device));C(p->nvnBufferBuilderSetStorage(&b,a->pools+pool,offset,bytes));B(p->nvnBufferInitialize(a->buffers+index,&b));a->buffers_ready[index]=1;R(p->nvnBufferMap(a->buffers+index)==a->memory[pool]+offset);R(p->nvnBufferGetAddress(a->buffers+index)==p->nvnMemoryPoolGetBufferAddress(a->pools+pool)+offset);return 0;
}
static int texture_create(struct npc_owner *o,const struct nn_public_copy_case *c,unsigned index,unsigned pool,size_t offset)
{
 struct npc_active *a=o->active;struct npc_api *p=&o->api;NVNtextureBuilder b={0};C(p->nvnTextureBuilderSetDefaults(&b));C(p->nvnTextureBuilderSetDevice(&b,&o->device));C(p->nvnTextureBuilderSetFlags(&b,c->flags));C(p->nvnTextureBuilderSetTarget(&b,c->target));C(p->nvnTextureBuilderSetSize3D(&b,c->width,c->height,c->source.volume_depth?c->source.volume_depth:c->layers));C(p->nvnTextureBuilderSetLevels(&b,c->levels));C(p->nvnTextureBuilderSetSamples(&b,0));C(p->nvnTextureBuilderSetFormat(&b,c->format));
#if defined(NN_PUBLIC_COPY_PACKAGED_BNTX) && NN_PUBLIC_COPY_PACKAGED_BNTX
 NVNpackagedTextureLayout layout={{(unsigned char)c->source.base_block_height_log2,0,0,0,7,0,1,0}};
 C(p->nvnTextureBuilderSetPackagedTextureLayout(&b,&layout));memset(&layout,0x7c,sizeof(layout));
#endif
 /* Public builder chooses its ordinary source layout. Never mutate an
  * initialized object/BH or use a native image layout in place of it. */
 R(p->nvnTextureBuilderGetStorageSize(&b)==c->source.payload_bytes);R(p->nvnTextureBuilderGetStorageAlignment(&b)==512);R(p->nvnTextureBuilderGetStorageClass(&b)==0x1fe);
 C(p->nvnTextureBuilderSetStorage(&b,a->pools+pool,offset));B(p->nvnTextureInitialize(a->textures+index,&b));a->textures_ready[index]=1;
 R(p->nvnTextureGetStorageSize(a->textures+index)==(int)c->source.payload_bytes);R(p->nvnTextureGetTextureAddress(a->textures+index)==p->nvnMemoryPoolGetBufferAddress(a->pools+pool)+offset);R((unsigned)p->nvnTextureGetFormat(a->textures+index)==c->format);R((unsigned)p->nvnTextureGetTarget(a->textures+index)==c->target);R((unsigned)p->nvnTextureGetFlags(a->textures+index)==c->flags);R(p->nvnTextureGetWidth(a->textures+index)==(int)c->width&&p->nvnTextureGetHeight(a->textures+index)==(int)c->height&&p->nvnTextureGetDepth(a->textures+index)==(int)(c->source.volume_depth?c->source.volume_depth:c->layers));R(p->nvnTextureGetLevels(a->textures+index)==(int)c->levels&&p->nvnTextureGetSamples(a->textures+index)==0);return 0;
}
static int record_copy(struct npc_owner *o,const struct nn_public_copy_case *c,unsigned *clone_checks,unsigned *pin_checks)
{
 struct npc_active *a=o->active;struct npc_api *p=&o->api;
 for(unsigned i=0;i<2;i++){
  int e=posix_memalign((void**)&a->control[i],16,32768);if(e)return e;memset(a->control[i],0xa5,32768);a->control_expected[i]=malloc(32768);if(!a->control_expected[i])return ENOMEM;
  B(p->nvnCommandBufferInitialize(a->commands+i,&o->device));a->commands_ready[i]=1;C(p->nvnCommandBufferAddCommandMemory(a->commands+i,a->pools+2,i*32768,32768));C(p->nvnCommandBufferAddControlMemory(a->commands+i,a->control[i],32768));
 }
 NVNtextureView views[2];memset(views,0,sizeof(views));for(unsigned i=0;i<2;i++){
  C(p->nvnTextureViewSetDefaults(views+i));C(p->nvnTextureViewSetFormat(views+i,c->format));C(p->nvnTextureViewSetLevels(views+i,c->level,1));C(p->nvnTextureViewSetLayers(views+i,c->source.volume_depth?0:(i?0:c->base_layer),c->source.volume_depth?1:c->layer_count));
  if(i==0||c->family==2)R(p->nvnTextureGetViewOffset(a->textures+i,views+i)==(ptrdiff_t)(c->source.level[c->level].offset+(c->source.volume_depth||i?0:c->base_layer)*c->source.layer_stride));
 }
 NVNbufferAddress linear=c->family==2?0:p->nvnBufferGetAddress(a->buffers+1);
 C(p->nvnCommandBufferBeginRecording(a->commands));C(p->nvnCommandBufferSetCopyRowStride(a->commands,c->variant?c->row_stride:0));C(p->nvnCommandBufferSetCopyImageStride(a->commands,c->variant?c->image_stride:0));
 if(c->family==0)C(p->nvnCommandBufferCopyBufferToTexture(a->commands,linear,a->textures,views,c->region,0));
 else if(c->family==1)C(p->nvnCommandBufferCopyTextureToBuffer(a->commands,a->textures,views,c->region,linear,0));
 else C(p->nvnCommandBufferCopyTextureToTexture(a->commands,a->textures,views,c->region,a->textures+1,views+1,c->region+1,0));
 a->handles[0]=p->nvnCommandBufferEndRecording(a->commands);R(a->handles[0]);
 /* Views and stride metadata are value snapshots, not retained caller bytes. */
 memset(views,0x7c,sizeof(views));C(p->nvnCommandBufferBeginRecording(a->commands+1));C(p->nvnCommandBufferSetCopyRowStride(a->commands+1,1));C(p->nvnCommandBufferSetCopyImageStride(a->commands+1,1));
#if NVN_PUBLIC_COPY_CALL
 C(p->nvnCommandBufferCallCommands(a->commands+1,1,a->handles));
#else
 C(p->nvnCommandBufferCopyCommands(a->commands+1,1,a->handles));
#endif
 a->handles[1]=p->nvnCommandBufferEndRecording(a->commands+1);R(a->handles[1]);(*clone_checks)++;
 C(p->nvnDeviceFinalizeCommandHandle(&o->device,a->handles[0]));a->handles[0]=0;
 for(unsigned i=0;i<2;i++){C(p->nvnCommandBufferFinalize(a->commands+i));a->commands_ready[i]=0;}
 for(unsigned i=0;i<(c->family==2?2u:1u);i++){p->nvnTextureFinalize(a->textures+i);if(nn_nvn_error()!=EBUSY)return EINVAL;nn_nvn_clear_error();(*pin_checks)++;}
 for(unsigned i=0;i<3;i++)if(a->pools_ready[i])memcpy(a->expected[i],a->memory[i],a->sizes[i]);
 for(unsigned i=0;i<2;i++)memcpy(a->control_expected[i],a->control[i],32768);
 return 0;
}
static int destroy_active(struct npc_owner *o)
{
 struct npc_active *a=o->active;if(!a)return 0;struct npc_api *p=&o->api;nn_nvn_clear_error();
 for(unsigned i=0;i<2;i++)if(a->handles[i]){C(p->nvnDeviceFinalizeCommandHandle(&o->device,a->handles[i]));a->handles[i]=0;}
 for(unsigned i=0;i<2;i++)if(a->commands_ready[i]){C(p->nvnCommandBufferFinalize(a->commands+i));a->commands_ready[i]=0;}
 for(unsigned i=0;i<2;i++)if(a->buffers_ready[i]){C(p->nvnBufferFinalize(a->buffers+i));a->buffers_ready[i]=0;}
 for(unsigned i=0;i<2;i++)if(a->textures_ready[i]){C(p->nvnTextureFinalize(a->textures+i));a->textures_ready[i]=0;}
 for(unsigned i=0;i<3;i++)if(a->pools_ready[i]){C(p->nvnMemoryPoolFinalize(a->pools+i));a->pools_ready[i]=0;}
 for(unsigned i=0;i<3;i++){free(a->memory[i]);free(a->expected[i]);}for(unsigned i=0;i<2;i++){free(a->control[i]);free(a->control_expected[i]);}free(a);o->active=NULL;return 0;
}
static int raw(const char *dir,unsigned n,unsigned replay,const struct npc_active *a)
{
 char path[1200];int z=snprintf(path,sizeof(path),"%s/" NN_PUBLIC_COPY_PREFIX "-%03u-r%u.bin",dir,n,replay);if(z<0||(size_t)z>=sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;FILE *f=fdopen(fd,"wb");if(!f){int e=errno;close(fd);return e;}int error=0;
 for(unsigned i=0;i<2;i++)if(a->pools_ready[i]&&fwrite(a->memory[i],1,a->sizes[i],f)!=a->sizes[i])error=EIO;
 if(fflush(f)||fsync(fd))error=EIO;
 if(fclose(f))error=EIO;
 return error;
}
#undef C
#undef B
#undef R
int nn_public_copy_probe(const char *directory,char *json,size_t capacity)
{
 if(!directory||!json||!capacity)return -1;
 struct npc_owner *o=calloc(1,sizeof(*o));int error=0,cleanup=0,pending=0;const char *stage="arguments";unsigned completed=0,submits=0,finishes=0,replays=0,files=0,clones=0,pins=0,pool_imports=0,pool_releases=0,texture_inits=0,texture_releases=0,buffer_inits=0;uint32_t chip=0;unsigned bad_case=~0u,bad_replay=~0u,bad_pool=~0u;size_t bad_offset=SIZE_MAX;uint64_t data_checks=0,active_checks=0,unchanged=0,readonly=0,arena_checks=0;
#define TRY(x) do{if((error=(x)))goto done;}while(0)
#define CALL(x) do{(x);if((error=nn_nvn_error()))goto done;}while(0)
#define BOOL(x) do{int z_=(x);error=nn_nvn_error();if(!z_||error){if(!error)error=EIO;goto done;}}while(0)
#define REQUIRE(x) do{if(!(x)){error=EINVAL;goto done;}if((error=nn_nvn_error()))goto done;}while(0)
 if(!o){error=ENOMEM;goto done;}if(!directory||!json||!capacity){free(o);return -1;}TRY(api_load(&o->api));struct npc_api *p=&o->api;nn_nvn_clear_error();struct nn_device_identity identity;stage="inspect";TRY(nn_device_inspect(&identity));chip=identity.chip_id;REQUIRE(chip==0x06050002&&identity.page_bytes==4096&&NN_PUBLIC_COPY_CASES>0&&NN_PUBLIC_COPY_CASES%6==0);
 NVNdeviceBuilder db={0};NVNqueueBuilder qb={0};stage="device";CALL(p->nvnDeviceBuilderSetDefaults(&db));BOOL(p->nvnDeviceInitialize(&o->device,&db));o->device_ready=1;TRY(NN_PUBLIC_COPY_REGISTER(&o->device));CALL(p->nvnQueueBuilderSetDefaults(&qb));CALL(p->nvnQueueBuilderSetDevice(&qb,&o->device));BOOL(p->nvnQueueInitialize(&o->queue,&qb));o->queue_ready=1;
 for(unsigned n=0;n<NN_PUBLIC_COPY_CASES;n++){
  const struct nn_public_copy_case *c=nn_public_copy_cases+n;bad_case=n;stage="pools";o->active=calloc(1,sizeof(*o->active));if(!o->active){error=ENOMEM;goto done;}struct npc_active *a=o->active;
  TRY(pool_create(o,0,c->pool_bytes[0]));pool_imports++;if(c->pool_bytes[1]){TRY(pool_create(o,1,c->pool_bytes[1]));pool_imports++;}TRY(pool_create(o,2,65536));pool_imports++;
  stage="textures";TRY(texture_create(o,c,0,0,c->canonical_offset));texture_inits++;if(c->family==2){TRY(texture_create(o,c,1,c->shared?0:1,c->destination_offset));texture_inits++;}
  stage="buffer-aliases";TRY(buffer_create(o,0,0,c->canonical_offset,c->source.payload_bytes));buffer_inits++;TRY(buffer_create(o,1,c->shared?0:1,c->family==2?c->destination_offset:c->linear_offset,c->family==2?c->source.payload_bytes:c->linear_bytes));buffer_inits++;
  stage="public-copy-and-clone";TRY(record_copy(o,c,&clones,&pins));
  for(unsigned replay=0;replay<2;replay++){
   bad_replay=replay;stage="raw-input";for(unsigned i=0;i<2;i++)if(a->pools_ready[i])memset(a->memory[i],0xa5,a->sizes[i]);
   unsigned source_pool=c->family==0&&!c->shared?1:0;size_t input_offset=c->family==0?c->linear_offset:c->canonical_offset,input_bytes=c->family==0?c->linear_bytes:(size_t)c->source.payload_bytes;
   unsigned char *input=p->nvnBufferMap(a->buffers+(c->family==0?1:0));REQUIRE(input==a->memory[source_pool]+input_offset);
   for(size_t i=0;i<input_bytes;i++)input[i]=pattern(input_offset+i,n,replay);
   for(unsigned i=0;i<2;i++)if(a->pools_ready[i])memcpy(a->expected[i],a->memory[i],a->sizes[i]);
   unsigned destination_pool=c->family==0?0:(c->shared?0:1);uint64_t active=(uint64_t)c->layer_count*c->rows*c->row_bytes;
   for(unsigned layer=0;layer<c->layer_count;layer++)for(unsigned y=0;y<c->rows;y++)for(unsigned x=0;x<c->row_bytes;x++){
    uint64_t canonical=c->canonical_offset+coordinate(&c->source,c->level,c->base_layer+layer,c->x_bytes+x,c->y_rows+y),linear=c->linear_offset+(uint64_t)layer*c->image_stride+(uint64_t)y*c->row_stride+x;
    uint64_t from=c->family==0?linear:canonical,to=c->family==0?canonical:c->family==1?linear:c->destination_offset+coordinate(&c->source,c->level,c->region[1].zoffset+layer,c->region[1].xoffset/c->source.block_w*c->source.bytes_per_block+x,c->region[1].yoffset/c->source.block_h+y);
    REQUIRE(to<a->sizes[destination_pool]);a->expected[destination_pool][to]=pattern(from,n,replay);
   }
   stage="public-flush";for(unsigned i=0;i<3;i++)if(a->pools_ready[i])CALL(p->nvnMemoryPoolFlushMappedRange(a->pools+i,0,a->sizes[i]));
   stage="public-submit";pending=1;CALL(p->nvnQueueSubmitCommands(&o->queue,1,a->handles+1));submits++;
   stage="public-finish";CALL(p->nvnQueueFinish(&o->queue));pending=0;finishes++;
   for(unsigned i=0;i<(c->family==2?2u:1u);i++)REQUIRE(p->nvnTextureGetTextureAddress(a->textures+i)==p->nvnMemoryPoolGetBufferAddress(a->pools+(i?(c->shared?0:1):0))+(i?c->destination_offset:c->canonical_offset));
   stage="public-invalidate";for(unsigned i=0;i<3;i++)if(a->pools_ready[i])CALL(p->nvnMemoryPoolInvalidateMappedRange(a->pools+i,0,a->sizes[i]));
   stage="save-actual";TRY(raw(directory,n,replay,a));files++;stage="all-bytes";uint64_t bytes=0;for(unsigned i=0;i<2;i++)if(a->pools_ready[i]){bad_pool=i;TRY(equal(a->memory[i],a->expected[i],a->sizes[i],&bad_offset));bytes+=a->sizes[i];}TRY(equal(a->memory[2],a->expected[2],a->sizes[2],&bad_offset));for(unsigned i=0;i<2;i++)TRY(equal(a->control[i],a->control_expected[i],32768,&bad_offset));
   data_checks+=bytes;active_checks+=active;unchanged+=bytes-active;readonly+=input_bytes;arena_checks+=65536+65536;completed++;if(replay)replays++;
  }
  unsigned pools=a->pools_ready[0]+a->pools_ready[1]+a->pools_ready[2],textures=a->textures_ready[0]+a->textures_ready[1];stage="retire-case";TRY(destroy_active(o));pool_releases+=pools;texture_releases+=textures;
 }
 stage="complete";
done:
 if(pending)cleanup=EBUSY;
 else if(o){cleanup=destroy_active(o);if(!cleanup){nn_nvn_clear_error();if(o->queue_ready){o->api.nvnQueueFinalize(&o->queue);cleanup=nn_nvn_error();if(!cleanup)o->queue_ready=0;}if(!cleanup&&o->device_ready){o->api.nvnDeviceFinalize(&o->device);cleanup=nn_nvn_error();if(!cleanup)o->device_ready=0;}}}
 if(o){if(cleanup){o->next=retained;retained=o;}else free(o);}
 int passed=!error&&!cleanup&&completed==NN_PUBLIC_COPY_CASES*2&&submits==NN_PUBLIC_COPY_CASES*2&&finishes==NN_PUBLIC_COPY_CASES*2&&replays==NN_PUBLIC_COPY_CASES&&files==NN_PUBLIC_COPY_CASES*2&&clones==NN_PUBLIC_COPY_CASES&&pins==NN_PUBLIC_COPY_CASES/3*4&&pool_imports==NN_PUBLIC_COPY_CASES/2*5&&pool_releases==NN_PUBLIC_COPY_CASES/2*5&&texture_inits==NN_PUBLIC_COPY_CASES/3*4&&texture_releases==NN_PUBLIC_COPY_CASES/3*4&&buffer_inits==NN_PUBLIC_COPY_CASES*2&&data_checks&&active_checks&&unchanged&&readonly&&arena_checks;
 int z=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"" NN_PUBLIC_COPY_OPERATION "\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"cases\":%u,\"formats\":" NN_PUBLIC_COPY_FORMATS_JSON ",\"families\":3,\"completed\":%u,\"submissions\":%u,\"finishes\":%u,\"replays\":%u,\"files\":%u," NN_PUBLIC_COPY_HANDLE_FIELDS "\"recorded_texture_finalize_rejections\":%u,\"pool_imports\":%u,\"pool_releases\":%u,\"texture_initializations\":%u,\"texture_finalizations\":%u,\"buffer_initializations\":%u,\"data_byte_checks\":%llu,\"active_byte_checks\":%llu,\"unchanged_data_byte_checks\":%llu,\"source_readonly_byte_checks\":%llu,\"command_control_readonly_byte_checks\":%llu,\"bad_case\":%u,\"bad_replay\":%u,\"bad_pool\":%u,\"bad_offset\":%zu,\"nvn_api\":true,\"probe_raw_submit_calls\":0,\"probe_private_encode_calls\":0,\"game_shader\":false,\"cpu_image_conversion\":false,\"BC_decode\":false," NN_PUBLIC_COPY_EXPERIMENT_JSON "\"passed\":%s}\n",(unsigned)getuid(),chip,stage,error,cleanup,cleanup?"true":"false",NN_PUBLIC_COPY_CASES,completed,submits,finishes,replays,files,clones,pins,pool_imports,pool_releases,texture_inits,texture_releases,buffer_inits,(unsigned long long)data_checks,(unsigned long long)active_checks,(unsigned long long)unchanged,(unsigned long long)readonly,(unsigned long long)arena_checks,passed?~0u:bad_case,passed?~0u:bad_replay,passed?~0u:bad_pool,passed?SIZE_MAX:bad_offset,passed?"true":"false");
 return passed&&z>=0&&(size_t)z<capacity?0:-1;
}
