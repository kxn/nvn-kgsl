/* SPDX-License-Identifier: MIT. Actual public NVN copy caller, authored bytes. */
#define _POSIX_C_SOURCE 200809L
#include "texture_texels_probe.h"
#include "public_copy_cases.h"
#include "nvn_native/nvn.h"
#include "nvn_native/kgsl.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_TEXTURE_TEXELS_HEADER
struct ntp_api {
#define ENTRY(name,type) type name;
#include "../src/nvn_entries.inc"
#undef ENTRY
};
struct ntp_active {
 NVNmemoryPool pools[3];NVNbuffer buffers[2];NVNtexture textures[2];NVNcommandBuffer commands[2];NVNcommandHandle handles[2];
 unsigned pools_ready[3],buffers_ready[2],textures_ready[2],commands_ready[2];
 unsigned char *memory[3],*expected[3],*control[2],*control_expected[2];size_t sizes[3];
};
struct ntp_owner {struct ntp_owner *next;struct ntp_api api;NVNdevice device;NVNqueue queue;unsigned device_ready,queue_ready;struct ntp_active *active;};
static struct ntp_owner *retained;
static int api_load(struct ntp_api *a)
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
/* Independent inverse-sector equation, same public minigbm oracle family as
 * standalone qualification. Not the production offset function. */
static uint64_t coordinate(const struct nn_tex_source_layout *s,unsigned level,unsigned layer,unsigned x,unsigned y)
{
 const struct nn_tex_source_level *m=s->level+level;unsigned h=1u<<m->block_height_log2,bc=x/64,br=y/(8*h),xd=x%64/4,yl=y%(8*h);
 unsigned k=(yl/8)*128+(xd&8)*8+((yl&6)<<3)+(xd&4)*2+((yl&1)<<2)+(xd&3);
 return layer*s->layer_stride+m->offset+((uint64_t)br*(m->pitch/64)+bc)*512*h+k*4+x%4;
}
static int equal(const void *a,const void *b,size_t n,size_t *bad)
{const unsigned char *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i]){*bad=i;return EIO;}return 0;}
static int pool_create_flags(struct ntp_owner *o,unsigned index,size_t bytes,int flags)
{
 struct ntp_active *a=o->active;struct ntp_api *p=&o->api;NVNmemoryPoolBuilder b={0};a->sizes[index]=bytes;
 int e=posix_memalign((void**)&a->memory[index],4096,bytes);if(e)return e;memset(a->memory[index],0xa5,bytes);a->expected[index]=malloc(bytes);if(!a->expected[index])return ENOMEM;
 C(p->nvnMemoryPoolBuilderSetDefaults(&b));C(p->nvnMemoryPoolBuilderSetDevice(&b,&o->device));C(p->nvnMemoryPoolBuilderSetFlags(&b,flags));C(p->nvnMemoryPoolBuilderSetStorage(&b,a->memory[index],bytes));B(p->nvnMemoryPoolInitialize(a->pools+index,&b));a->pools_ready[index]=1;
 R(p->nvnMemoryPoolMap(a->pools+index)==((flags&1)?NULL:a->memory[index]));R(p->nvnMemoryPoolGetBufferAddress(a->pools+index)!=0);return 0;
}
static int buffer_create(struct ntp_owner *o,unsigned index,unsigned pool,size_t offset,size_t bytes)
{
 struct ntp_active *a=o->active;struct ntp_api *p=&o->api;NVNbufferBuilder b={0};C(p->nvnBufferBuilderSetDefaults(&b));C(p->nvnBufferBuilderSetDevice(&b,&o->device));C(p->nvnBufferBuilderSetStorage(&b,a->pools+pool,offset,bytes));B(p->nvnBufferInitialize(a->buffers+index,&b));a->buffers_ready[index]=1;R(p->nvnBufferMap(a->buffers+index)==a->memory[pool]+offset);R(p->nvnBufferGetAddress(a->buffers+index)==p->nvnMemoryPoolGetBufferAddress(a->pools+pool)+offset);return 0;
}
static int texture_create(struct ntp_owner *o,const struct nn_public_copy_case *c,unsigned index,unsigned pool,size_t offset)
{
 struct ntp_active *a=o->active;struct ntp_api *p=&o->api;NVNtextureBuilder b={0};C(p->nvnTextureBuilderSetDefaults(&b));C(p->nvnTextureBuilderSetDevice(&b,&o->device));C(p->nvnTextureBuilderSetFlags(&b,c->flags));C(p->nvnTextureBuilderSetTarget(&b,c->target));C(p->nvnTextureBuilderSetSize3D(&b,c->width,c->height,c->layers));C(p->nvnTextureBuilderSetLevels(&b,c->levels));C(p->nvnTextureBuilderSetSamples(&b,0));C(p->nvnTextureBuilderSetFormat(&b,c->format));
 /* Public builder chooses its ordinary source layout. Never mutate an
  * initialized object/BH or use a native image layout in place of it. */
 R(p->nvnTextureBuilderGetStorageSize(&b)==c->source.payload_bytes);R(p->nvnTextureBuilderGetStorageAlignment(&b)==512);R(p->nvnTextureBuilderGetStorageClass(&b)==0x1fe);
 C(p->nvnTextureBuilderSetStorage(&b,a->pools+pool,offset));B(p->nvnTextureInitialize(a->textures+index,&b));a->textures_ready[index]=1;
 R(p->nvnTextureGetStorageSize(a->textures+index)==(int)c->source.payload_bytes);R(p->nvnTextureGetTextureAddress(a->textures+index)==p->nvnMemoryPoolGetBufferAddress(a->pools+pool)+offset);R((unsigned)p->nvnTextureGetFormat(a->textures+index)==c->format);R((unsigned)p->nvnTextureGetTarget(a->textures+index)==c->target);R((unsigned)p->nvnTextureGetFlags(a->textures+index)==c->flags);R(p->nvnTextureGetWidth(a->textures+index)==(int)c->width&&p->nvnTextureGetHeight(a->textures+index)==(int)c->height&&p->nvnTextureGetDepth(a->textures+index)==(int)c->layers);R(p->nvnTextureGetLevels(a->textures+index)==(int)c->levels&&p->nvnTextureGetSamples(a->textures+index)==0);return 0;
}
static int destroy_active(struct ntp_owner *o)
{
 struct ntp_active *a=o->active;if(!a)return 0;struct ntp_api *p=&o->api;nn_nvn_clear_error();
 for(unsigned i=0;i<2;i++)if(a->handles[i]){C(p->nvnDeviceFinalizeCommandHandle(&o->device,a->handles[i]));a->handles[i]=0;}
 for(unsigned i=0;i<2;i++)if(a->commands_ready[i]){C(p->nvnCommandBufferFinalize(a->commands+i));a->commands_ready[i]=0;}
 for(unsigned i=0;i<2;i++)if(a->buffers_ready[i]){C(p->nvnBufferFinalize(a->buffers+i));a->buffers_ready[i]=0;}
 for(unsigned i=0;i<2;i++)if(a->textures_ready[i]){C(p->nvnTextureFinalize(a->textures+i));a->textures_ready[i]=0;}
 for(unsigned i=0;i<3;i++)if(a->pools_ready[i]){C(p->nvnMemoryPoolFinalize(a->pools+i));a->pools_ready[i]=0;}
 for(unsigned i=0;i<3;i++){free(a->memory[i]);free(a->expected[i]);}for(unsigned i=0;i<2;i++){free(a->control[i]);free(a->control_expected[i]);}free(a);o->active=NULL;return 0;
}
static int raw(const char *dir,unsigned n,unsigned replay,const struct ntp_active *a)
{
 char path[1200];int z=snprintf(path,sizeof(path),"%s/texels-%03u-r%u.bin",dir,n,replay);if(z<0||(size_t)z>=sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;FILE *f=fdopen(fd,"wb");if(!f){int e=errno;close(fd);return e;}int error=0;
 for(unsigned i=0;i<2;i++)if(a->memory[i]&&fwrite(a->memory[i],1,a->sizes[i],f)!=a->sizes[i])error=EIO;
 if(fflush(f)||fsync(fd))error=EIO;
 if(fclose(f))error=EIO;
 return error;
}
/* Qualification-only read-only observers. Always forward the exact original
 * args to the real driver. They are linked only into this standalone probe,
 * never the genuine host; they neither synthesize GPU values nor alter work. */
static struct {unsigned submits,accepted,failed,finishes,finished,finish_failed,bad_arguments;} observed;
int __real_nn_queue_submit(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,uint32_t *);
int __real_nn_queue_finish(nn_queue *,uint32_t);
int __wrap_nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *ts)
{
 observed.submits++;if(!q||!ib||!bytes||bytes%4||offset>nn_bo_size(ib)||bytes>nn_bo_size(ib)-offset||!ts||(count&&!refs))observed.bad_arguments++;
 int e=__real_nn_queue_submit(q,ib,offset,bytes,refs,count,ts);if(e)observed.failed++;else observed.accepted++;return e;
}
int __wrap_nn_queue_finish(nn_queue *q,uint32_t timeout)
{observed.finishes++;int e=__real_nn_queue_finish(q,timeout);if(e)observed.finish_failed++;else observed.finished++;return e;}
static int ordinary_memory(struct ntp_owner *o,unsigned i,size_t bytes)
{struct ntp_active *a=o->active;a->sizes[i]=bytes;int e=posix_memalign((void**)&a->memory[i],4096,bytes);if(e)return e;a->expected[i]=malloc(bytes);if(!a->expected[i])return ENOMEM;memset(a->memory[i],0xa5,bytes);return 0;}
static int noaccess_negative(struct ntp_owner *o,unsigned *checks)
{
 const struct nn_public_copy_case *c=nn_public_copy_cases;struct ntp_api *p=&o->api;o->active=calloc(1,sizeof(*o->active));if(!o->active)return ENOMEM;
 int e=pool_create_flags(o,0,c->pool_bytes[0],0xa1);if(e)return e;
 e=ordinary_memory(o,1,c->pool_bytes[1]);if(e)return e;e=texture_create(o,c,0,0,512);if(e)return e;
 unsigned old=observed.submits;p->nvnTextureWriteTexels(o->active->textures,NULL,c->region,o->active->memory[1]);
 if(nn_nvn_error()!=ENOTSUP)return nn_nvn_error()?nn_nvn_error():EINVAL;
 if(observed.submits!=old)return EINVAL;
 nn_nvn_clear_error();(*checks)++;
 p->nvnTextureReadTexels(o->active->textures,NULL,c->region,o->active->memory[1]);
 if(nn_nvn_error()!=ENOTSUP)return nn_nvn_error()?nn_nvn_error():EINVAL;
 if(observed.submits!=old)return EINVAL;
 nn_nvn_clear_error();(*checks)++;
 return destroy_active(o);
}
static int three_d_negative(struct ntp_owner *o,unsigned *checks,unsigned *stage)
{
 struct ntp_api *p=&o->api;NVNtextureBuilder b={0};C(p->nvnTextureBuilderSetDefaults(&b));C(p->nvnTextureBuilderSetDevice(&b,&o->device));C(p->nvnTextureBuilderSetFlags(&b,0));C(p->nvnTextureBuilderSetTarget(&b,NVN_TEXTURE_TARGET_3D));C(p->nvnTextureBuilderSetFormat(&b,NVN_FORMAT_RGBA8));C(p->nvnTextureBuilderSetSize3D(&b,8,8,8));C(p->nvnTextureBuilderSetLevels(&b,1));
 size_t size=p->nvnTextureBuilderGetStorageSize(&b);int e=nn_nvn_error();
 if(e){if(e!=ENOTSUP)return e;nn_nvn_clear_error();*stage=1;(*checks)++;return 0;}
 if(!size||size>16777216)return EINVAL;
 o->active=calloc(1,sizeof(*o->active));if(!o->active)return ENOMEM;
 e=pool_create_flags(o,0,(size+512+4095)&~(size_t)4095,0xa4);if(e)return e;
 C(p->nvnTextureBuilderSetStorage(&b,o->active->pools,512));int ok=p->nvnTextureInitialize(o->active->textures,&b);e=nn_nvn_error();
 if(!ok||e){if(e!=ENOTSUP)return e?e:EINVAL;nn_nvn_clear_error();*stage=2;(*checks)++;return destroy_active(o);}
 o->active->textures_ready[0]=1;e=ordinary_memory(o,1,16384);if(e)return e;NVNcopyRegion region={0,0,0,8,8,8};unsigned old=observed.submits;
 p->nvnTextureWriteTexels(o->active->textures,NULL,&region,o->active->memory[1]);if(nn_nvn_error()!=ENOTSUP)return nn_nvn_error()?nn_nvn_error():EINVAL;
 if(observed.submits!=old)return EINVAL;
 nn_nvn_clear_error();(*checks)++;
 p->nvnTextureReadTexels(o->active->textures,NULL,&region,o->active->memory[1]);if(nn_nvn_error()!=ENOTSUP)return nn_nvn_error()?nn_nvn_error():EINVAL;
 if(observed.submits!=old)return EINVAL;
 nn_nvn_clear_error();(*checks)++;*stage=3;return destroy_active(o);
}
static int busy_negative(struct ntp_owner *o,const char *directory,unsigned *checks,unsigned *pending,unsigned *verified)
{
 const struct nn_public_copy_case *c=nn_public_copy_cases;struct ntp_api *p=&o->api;o->active=calloc(1,sizeof(*o->active));if(!o->active)return ENOMEM;struct ntp_active *a=o->active;
 int e=pool_create_flags(o,0,65536,0xa4);if(e)return e;e=ordinary_memory(o,1,16384);if(e)return e;e=pool_create_flags(o,2,65536,0xa4);if(e)return e;e=texture_create(o,c,0,0,512);if(e)return e;e=buffer_create(o,0,0,512,c->source.payload_bytes);if(e)return e;
 NVNqueueBuilder qb={0};C(p->nvnQueueBuilderSetDefaults(&qb));C(p->nvnQueueBuilderSetDevice(&qb,&o->device));B(p->nvnQueueInitialize(&o->queue,&qb));o->queue_ready=1;
 e=posix_memalign((void**)&a->control[0],16,32768);if(e)return e;memset(a->control[0],0xa5,32768);
 B(p->nvnCommandBufferInitialize(a->commands,&o->device));a->commands_ready[0]=1;C(p->nvnCommandBufferAddCommandMemory(a->commands,a->pools+2,0,65536));C(p->nvnCommandBufferAddControlMemory(a->commands,a->control[0],32768));C(p->nvnCommandBufferBeginRecording(a->commands));
 uint64_t address=p->nvnBufferGetAddress(a->buffers);C(p->nvnCommandBufferClearBuffer(a->commands,address,4,0x2468ace0));a->handles[0]=p->nvnCommandBufferEndRecording(a->commands);R(a->handles[0]);C(p->nvnMemoryPoolFlushMappedRange(a->pools,0,65536));
 *pending=1;C(p->nvnQueueSubmitCommands(&o->queue,1,a->handles));unsigned old=observed.submits,fin=observed.finishes;
 p->nvnTextureWriteTexels(a->textures,NULL,c->region,a->memory[1]);if(nn_nvn_error()!=EBUSY||observed.submits!=old||observed.finishes!=fin)return EINVAL;nn_nvn_clear_error();(*checks)++;
 p->nvnTextureReadTexels(a->textures,NULL,c->region,a->memory[1]);if(nn_nvn_error()!=EBUSY||observed.submits!=old||observed.finishes!=fin)return EINVAL;nn_nvn_clear_error();(*checks)++;
 C(p->nvnQueueFinish(&o->queue));*pending=0;C(p->nvnMemoryPoolInvalidateMappedRange(a->pools,0,65536));
 uint32_t value;memcpy(&value,a->memory[0]+512,4);if(value!=0x2468ace0)return EIO;
 for(size_t i=0;i<65536;i++)if((i<512||i>=516)&&a->memory[0][i]!=0xa5)return EIO;
 char path[1200];int z=snprintf(path,sizeof(path),"%s/texels-busy.bin",directory);if(z<0||(size_t)z>=sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;FILE*f=fdopen(fd,"wb");if(!f){e=errno;close(fd);return e;}e=fwrite(a->memory[0],1,65536,f)==65536?0:EIO;if(fflush(f)||fsync(fd))e=EIO;if(fclose(f))e=EIO;if(e)return e;
 *verified=1;return destroy_active(o);
}
#undef C
#undef B
#undef R
int nn_texture_texels_probe(const char *directory,char *json,size_t capacity)
{
 if(!directory||!json||!capacity)return -1;
 struct ntp_owner *o=calloc(1,sizeof(*o));int error=0,cleanup=0;unsigned pending=0;const char*stage="arguments";unsigned completed=0,calls=0,replays=0,files=0,noaccess=0,three_d=0,three_d_stage=0,busy=0,busy_verified=0,pool_imports=0,pool_releases=0,texture_inits=0,texture_releases=0,buffers=0;uint32_t chip=0;unsigned bad_case=~0u,bad_replay=~0u,bad_pool=~0u;size_t bad_offset=SIZE_MAX;uint64_t data_checks=0,active_checks=0,unchanged=0,readonly=0,cache_checks=0;
 memset(&observed,0,sizeof(observed));
#define TRY(x) do{if((error=(x)))goto done;}while(0)
#define CALL(x) do{(x);if((error=nn_nvn_error()))goto done;}while(0)
#define BOOL(x) do{int z_=(x);error=nn_nvn_error();if(!z_||error){if(!error)error=EIO;goto done;}}while(0)
#define REQUIRE(x) do{if(!(x)){error=EINVAL;goto done;}if((error=nn_nvn_error()))goto done;}while(0)
 if(!o){error=ENOMEM;goto done;}TRY(api_load(&o->api));struct ntp_api *p=&o->api;nn_nvn_clear_error();struct nn_device_identity identity;stage="inspect";TRY(nn_device_inspect(&identity));chip=identity.chip_id;REQUIRE(chip==0x06050002&&identity.page_bytes==4096&&NN_TEXTURE_TEXELS_CASES==156);
 NVNdeviceBuilder db={0};stage="device";CALL(p->nvnDeviceBuilderSetDefaults(&db));BOOL(p->nvnDeviceInitialize(&o->device,&db));o->device_ready=1;TRY(nn_nvn_register_block_copy(&o->device,&NVN_COMPILED_IMAGE));
 stage="noaccess-negative";TRY(noaccess_negative(o,&noaccess));stage="3d-negative";TRY(three_d_negative(o,&three_d,&three_d_stage));
 for(unsigned n=0;n<NN_TEXTURE_TEXELS_CASES;n++){
  const struct nn_public_copy_case *c=nn_texture_texel_cases[n];bad_case=n;stage="caller-storage";o->active=calloc(1,sizeof(*o->active));if(!o->active){error=ENOMEM;goto done;}struct ntp_active *a=o->active;
  TRY(pool_create_flags(o,0,c->pool_bytes[0],0xa4));pool_imports++;if(c->pool_bytes[1])TRY(ordinary_memory(o,1,c->pool_bytes[1]));
  stage="texture";TRY(texture_create(o,c,0,0,c->canonical_offset));texture_inits++;TRY(buffer_create(o,0,0,c->canonical_offset,c->source.payload_bytes));buffers++;if(c->shared){TRY(buffer_create(o,1,0,c->linear_offset,c->linear_bytes));buffers++;}
  NVNtextureView view={0};CALL(p->nvnTextureViewSetDefaults(&view));CALL(p->nvnTextureViewSetFormat(&view,c->format));CALL(p->nvnTextureViewSetLevels(&view,c->level,1));CALL(p->nvnTextureViewSetLayers(&view,c->base_layer,c->layer_count));
  for(unsigned replay=0;replay<2;replay++){
   bad_replay=replay;stage="raw-input";for(unsigned i=0;i<2;i++)if(a->memory[i])memset(a->memory[i],0xa5,a->sizes[i]);
   unsigned source_pool=c->family==0&&!c->shared?1:0;size_t input_offset=c->family==0?c->linear_offset:c->canonical_offset,input_bytes=c->family==0?c->linear_bytes:(size_t)c->source.payload_bytes;
   unsigned char *input=a->memory[source_pool]+input_offset;for(size_t i=0;i<input_bytes;i++)input[i]=pattern(input_offset+i,c->id,replay);
   for(unsigned i=0;i<2;i++)if(a->memory[i])memcpy(a->expected[i],a->memory[i],a->sizes[i]);
   unsigned destination_pool=c->family==0?0:(c->shared?0:1);uint64_t active=(uint64_t)c->layer_count*c->rows*c->row_bytes;
   for(unsigned layer=0;layer<c->layer_count;layer++)for(unsigned y=0;y<c->rows;y++)for(unsigned x=0;x<c->row_bytes;x++){
    uint64_t canonical=c->canonical_offset+coordinate(&c->source,c->level,c->base_layer+layer,c->x_bytes+x,c->y_rows+y),linear=c->linear_offset+(uint64_t)layer*c->image_stride+(uint64_t)y*c->row_stride+x;
    uint64_t from=c->family==0?linear:canonical,to=c->family==0?canonical:linear;REQUIRE(to<a->sizes[destination_pool]);a->expected[destination_pool][to]=pattern(from,c->id,replay);
   }
   /* Publish authored raw canonical bytes before a GPU consumer. Caller
    * linear memory is never explicitly imported/flushed by this probe. */
   stage="publish-source";CALL(p->nvnTextureFlushTexels(a->textures,&view,NULL));
   void *linear=a->memory[c->shared?0:1]+c->linear_offset;
   stage=c->family==0?"public-write":"public-read";pending=1;calls++;
   if(c->family==0){if(c->variant)CALL(p->nvnTextureWriteTexelsStrided(a->textures,&view,c->region,linear,c->row_stride,c->image_stride));else CALL(p->nvnTextureWriteTexels(a->textures,&view,c->region,linear));}
   else{if(c->variant)CALL(p->nvnTextureReadTexelsStrided(a->textures,&view,c->region,linear,c->row_stride,c->image_stride));else CALL(p->nvnTextureReadTexels(a->textures,&view,c->region,linear));}
   pending=0;REQUIRE(p->nvnTextureGetTextureAddress(a->textures)==p->nvnBufferGetAddress(a->buffers));
   /* Capture and compare BEFORE any caller invalidate: successful immediate
    * return must already expose actual output to this CPU caller. */
   stage="save-return-bytes";TRY(raw(directory,n,replay,a));files++;stage="return-visibility";uint64_t bytes=0;
   for(unsigned i=0;i<2;i++)if(a->memory[i]){bad_pool=i;TRY(equal(a->memory[i],a->expected[i],a->sizes[i],&bad_offset));bytes+=a->sizes[i];}
   /* This SDK ignores region/mip/format for cache span. A non-null unused
    * region must not be dereferenced, and cache ops must preserve all bytes. */
   stage="public-cache";CALL(p->nvnTextureFlushTexels(a->textures,&view,(const NVNcopyRegion *)(uintptr_t)1));CALL(p->nvnTextureInvalidateTexels(a->textures,&view,NULL));
   for(unsigned i=0;i<2;i++)if(a->memory[i]){bad_pool=i;TRY(equal(a->memory[i],a->expected[i],a->sizes[i],&bad_offset));}
   data_checks+=bytes;active_checks+=active;unchanged+=bytes-active;readonly+=input_bytes;cache_checks+=bytes;completed++;if(replay)replays++;
  }
  stage="retire-case";TRY(destroy_active(o));pool_releases++;texture_releases++;
 }
 stage="busy-negative";TRY(busy_negative(o,directory,&busy,&pending,&busy_verified));files++;stage="complete";
done:
 if(pending)cleanup=EBUSY;
 else if(o){cleanup=destroy_active(o);if(!cleanup){nn_nvn_clear_error();if(o->queue_ready){o->api.nvnQueueFinalize(&o->queue);cleanup=nn_nvn_error();if(!cleanup)o->queue_ready=0;}if(!cleanup&&o->device_ready){o->api.nvnDeviceFinalize(&o->device);cleanup=nn_nvn_error();if(!cleanup)o->device_ready=0;}}}
 if(o){if(cleanup){o->next=retained;retained=o;}else free(o);}
 int passed=!error&&!cleanup&&completed==312&&calls==312&&replays==156&&files==313&&pool_imports==156&&pool_releases==156&&texture_inits==156&&texture_releases==156&&buffers==234&&noaccess==2&&three_d>0&&busy==2&&busy_verified&&observed.submits==313&&observed.accepted==313&&!observed.failed&&observed.finishes==313&&observed.finished==313&&!observed.finish_failed&&!observed.bad_arguments&&data_checks&&active_checks&&unchanged&&readonly&&cache_checks;
 int z=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"texture_texels\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"cases\":156,\"formats\":37,\"completed\":%u,\"public_transfer_calls\":%u,\"replays\":%u,\"files\":%u,\"pool_imports_main\":%u,\"pool_releases_main\":%u,\"texture_initializations_main\":%u,\"texture_finalizations_main\":%u,\"buffer_initializations_main\":%u,\"noaccess_rejections\":%u,\"three_d_rejections\":%u,\"three_d_rejection_stage\":%u,\"busy_rejections\":%u,\"busy_GPU_word_verified\":%s,\"native_submit_attempts\":%u,\"native_submit_accepted\":%u,\"native_submit_failed\":%u,\"native_finish_calls\":%u,\"native_finish_completed\":%u,\"native_finish_failed\":%u,\"native_bad_arguments\":%u,\"data_byte_checks\":%llu,\"active_byte_checks\":%llu,\"unchanged_data_byte_checks\":%llu,\"source_readonly_byte_checks\":%llu,\"post_cache_stability_byte_checks\":%llu,\"bad_case\":%u,\"bad_replay\":%u,\"bad_pool\":%u,\"bad_offset\":%zu,\"nvn_api\":true,\"probe_raw_transfer_calls\":0,\"probe_private_encode_calls\":0,\"read_only_forwarding_observers\":true,\"CPU_visibility_before_caller_invalidate\":true,\"game_shader\":false,\"cpu_image_conversion\":false,\"BC_decode\":false,\"passed\":%s}\n",(unsigned)getuid(),chip,stage,error,cleanup,cleanup?"true":"false",completed,calls,replays,files,pool_imports,pool_releases,texture_inits,texture_releases,buffers,noaccess,three_d,three_d_stage,busy,busy_verified?"true":"false",observed.submits,observed.accepted,observed.failed,observed.finishes,observed.finished,observed.finish_failed,observed.bad_arguments,(unsigned long long)data_checks,(unsigned long long)active_checks,(unsigned long long)unchanged,(unsigned long long)readonly,(unsigned long long)cache_checks,passed?~0u:bad_case,passed?~0u:bad_replay,passed?~0u:bad_pool,passed?SIZE_MAX:bad_offset,passed?"true":"false");
 return passed&&z>=0&&(size_t)z<capacity?0:-1;
}
