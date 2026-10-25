/* HOST actual core + RAM only. Opaque storage is reused, no CPU/GPU shader execution. */
#define _POSIX_C_SOURCE 200809L
#include NN_BUILDER_CORE
#include NN_BUILDER_MEMORY_BACKEND
#include <stdio.h>
static unsigned checks,builders_reused,source_aliases,fail_object_alloc;
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t count,size_t bytes)
{if(fail_object_alloc&&count==1&&bytes==sizeof(struct object)){fail_object_alloc=0;return NULL;}return __real_calloc(count,bytes);}
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line %d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(e) do{T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
union storage {NVNdevice d;NVNdeviceBuilder db;NVNqueue q;NVNqueueBuilder qb;NVNmemoryPool p;NVNmemoryPoolBuilder pb;NVNbuffer b;NVNbufferBuilder bb;NVNtexture t;NVNtextureBuilder tb;NVNsampler s;NVNsamplerBuilder sb;NVNwindowBuilder wb;NVNprogram program;NVNcommandBuffer cb;NVNsync sync;NVNevent e;NVNeventBuilder eb;NVNtexturePool tp;NVNsamplerPool sp;};
static struct object *peek(const void *p){for(struct object*o=objects;o;o=o->next)if(o->key==p)return o;return NULL;}
static void texture_builder(NVNtextureBuilder*b,NVNdevice*d,NVNmemoryPool*p,int kind)
{
 OK(api_nvnTextureBuilderSetDefaults(b));OK(api_nvnTextureBuilderSetDevice(b,d));
 OK(api_nvnTextureBuilderSetTarget(b,kind==2?NVN_TEXTURE_TARGET_BUFFER:NVN_TEXTURE_TARGET_2D));OK(api_nvnTextureBuilderSetFormat(b,NVN_FORMAT_RGBA8));
 OK(api_nvnTextureBuilderSetSize2D(b,16,kind==2?1:8));OK(api_nvnTextureBuilderSetFlags(b,kind==1?NVN_TEXTURE_FLAGS_LINEAR:0));
 if(kind==1)OK(api_nvnTextureBuilderSetStride(b,64));
 OK(api_nvnTextureBuilderSetStorage(b,p,4096));
}
int main(void)
{
 NVNdeviceBuilder db={0};NVNdevice d={0};NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool={0};void*ram=NULL;
 OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&d,&db));T(!nn_nvn_error());
 T(!posix_memalign(&ram,4096,65536));memset(ram,0x51,65536);OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&d));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,0x24));OK(api_nvnMemoryPoolBuilderSetStorage(&pb,ram,65536));T(api_nvnMemoryPoolInitialize(&pool,&pb));
 /* All public builder kinds have no Finalize. No builder source is needed
  * by ProgramInitialize: these keys represent former out-of-scope values. */
 for(unsigned k=0;k<8;k++){
  union storage slot={0};
  switch(k){case 0:OK(api_nvnDeviceBuilderSetDefaults(&slot.db));break;case 1:OK(api_nvnQueueBuilderSetDefaults(&slot.qb));break;case 2:OK(api_nvnMemoryPoolBuilderSetDefaults(&slot.pb));break;case 3:OK(api_nvnBufferBuilderSetDefaults(&slot.bb));break;case 4:OK(api_nvnTextureBuilderSetDefaults(&slot.tb));break;case 5:OK(api_nvnSamplerBuilderSetDefaults(&slot.sb));break;case 6:OK(api_nvnWindowBuilderSetDefaults(&slot.wb));break;default:OK(api_nvnEventBuilderSetDefaults(&slot.eb));}
  T(peek(&slot)&&builder_kind(peek(&slot)->kind));T(api_nvnProgramInitialize(&slot.program,&d));T(!nn_nvn_error());T(peek(&slot)->kind==PROGRAM);++builders_reused;
  T(!api_nvnProgramInitialize(&slot.program,&d));BAD(EEXIST);OK(api_nvnProgramFinalize(&slot.program));T(!peek(&slot));
 }
 /* Strongly referenced metadata and ALL live resources are not reclaimable.
  * No ordinary public operation creates builder strongrefs; inject one only
  * to test that the defensive lifetime guard cannot silently destroy it. */
 union storage held={0};OK(api_nvnTextureBuilderSetDefaults(&held.tb));struct object*retained=peek(&held);retained->refs=1;
 T(!api_nvnProgramInitialize(&held.program,&d));BAD(EEXIST);T(peek(&held)==retained&&retained->refs==1&&retained->kind==TB);retained->refs=0;
 fail_object_alloc=1;T(!api_nvnProgramInitialize(&held.program,&d));BAD(ENOMEM);T(!fail_object_alloc&&peek(&held)==retained&&retained->kind==TB&&retained->refs==0);
 T(api_nvnProgramInitialize(&held.program,&d));T(peek(&held)->kind==PROGRAM);OK(api_nvnProgramFinalize(&held.program));
 /* Source-builder/destination-key alias: explicit constructor input is read
  * before replacement. This is defensive support, not an NVN overlapping-live
  * object guarantee. Verify actual metadata and owner pins after construction. */
 union storage a={0};
 OK(api_nvnDeviceBuilderSetDefaults(&a.db));OK(api_nvnDeviceBuilderSetFlags(&a.db,NVN_DEVICE_FLAG_ENABLE_SEPARATE_SAMPLER_TEXTURE_SUPPORT));T(api_nvnDeviceInitialize(&a.d,&a.db));T(peek(&a.d)->flags==NVN_DEVICE_FLAG_ENABLE_SEPARATE_SAMPLER_TEXTURE_SUPPORT);OK(api_nvnDeviceFinalize(&a.d));++source_aliases;
 OK(api_nvnQueueBuilderSetDefaults(&a.qb));OK(api_nvnQueueBuilderSetDevice(&a.qb,&d));OK(api_nvnQueueBuilderSetFlags(&a.qb,1));T(api_nvnQueueInitialize(&a.q,&a.qb));T(peek(&a.q)->flags==1&&peek(&a.q)->queue_config.flush);OK(api_nvnQueueFinalize(&a.q));++source_aliases;
 OK(api_nvnMemoryPoolBuilderSetDefaults(&a.pb));OK(api_nvnMemoryPoolBuilderSetDevice(&a.pb,&d));OK(api_nvnMemoryPoolBuilderSetStorage(&a.pb,ram,65536));OK(api_nvnMemoryPoolBuilderSetFlags(&a.pb,0x24));T(api_nvnMemoryPoolInitialize(&a.p,&a.pb));T(api_nvnMemoryPoolMap(&a.p)==ram&&peek(&a.p)->size==65536&&peek(&a.p)->flags==0x24);OK(api_nvnMemoryPoolFinalize(&a.p));++source_aliases;
 OK(api_nvnBufferBuilderSetDefaults(&a.bb));OK(api_nvnBufferBuilderSetDevice(&a.bb,&d));OK(api_nvnBufferBuilderSetStorage(&a.bb,&pool,256,777));T(api_nvnBufferInitialize(&a.b,&a.bb));T(api_nvnBufferGetSize(&a.b)==777&&api_nvnBufferGetMemoryOffset(&a.b)==256);T(peek(&pool)->refs==1);api_nvnMemoryPoolFinalize(&pool);BAD(EBUSY);OK(api_nvnBufferFinalize(&a.b));++source_aliases;
 for(unsigned kind=0;kind<3;kind++){
  texture_builder(&a.tb,&d,&pool,(int)kind);T(api_nvnTextureInitialize(&a.t,&a.tb));T(api_nvnTextureGetFormat(&a.t)==NVN_FORMAT_RGBA8&&api_nvnTextureGetWidth(&a.t)==16);T(peek(&a)->pool==peek(&pool)&&peek(&pool)->refs==1);T((peek(&a)->source_layout!=NULL)==(kind==0));OK(api_nvnTextureFinalize(&a.t));++source_aliases;
 }
 OK(api_nvnSamplerBuilderSetDefaults(&a.sb));OK(api_nvnSamplerBuilderSetDevice(&a.sb,&d));OK(api_nvnSamplerBuilderSetLodClamp(&a.sb,2,7));T(api_nvnSamplerInitialize(&a.s,&a.sb));float lo=0,hi=0;api_nvnSamplerGetLodClamp(&a.s,&lo,&hi);T(lo==2&&hi==7);OK(api_nvnSamplerFinalize(&a.s));++source_aliases;
 OK(api_nvnEventBuilderSetDefaults(&a.eb));OK(api_nvnEventBuilderSetStorage(&a.eb,&pool,32));T(api_nvnEventInitialize(&a.e,&a.eb));T(api_nvnEventGetMemoryOffset(&a.e)==32&&api_nvnEventGetValue(&a.e)==0x51515151);T(peek(&pool)->refs==1);OK(api_nvnEventFinalize(&a.e));++source_aliases;
 /* Other resource constructors (no builder input) may reuse old builder
  * keys, including descriptors. Independent descriptor snapshots retain pins. */
 OK(api_nvnTextureBuilderSetDefaults(&a.tb));T(api_nvnCommandBufferInitialize(&a.cb,&d));OK(api_nvnCommandBufferFinalize(&a.cb));
 OK(api_nvnTextureBuilderSetDefaults(&a.tb));T(api_nvnSyncInitialize(&a.sync,&d));OK(api_nvnSyncFinalize(&a.sync));
 OK(api_nvnTextureBuilderSetDefaults(&a.tb));T(api_nvnTexturePoolInitialize(&a.tp,&pool,0,257));T(peek(&pool)->refs==1);OK(api_nvnTexturePoolFinalize(&a.tp));
 NVNsamplerPool table={0};T(api_nvnSamplerPoolInitialize(&table,&pool,16384,257));
 OK(api_nvnSamplerBuilderSetDefaults(&a.sb));OK(api_nvnSamplerBuilderSetDevice(&a.sb,&d));OK(api_nvnSamplerBuilderSetLodClamp(&a.sb,1,4));OK(api_nvnSamplerPoolRegisterSamplerBuilder(&table,256,&a.sb));T(peek(&table)->slots[256].valid&&peek(&table)->slots[256].snapshot.sampler.lod_min==1);
 T(api_nvnSamplerInitialize(&a.s,&a.sb));T(peek(&table)->slots[256].valid&&peek(&table)->slots[256].snapshot.sampler.lod_min==1);OK(api_nvnSamplerFinalize(&a.s));OK(api_nvnSamplerPoolFinalize(&table));T(peek(&pool)->refs==0);
 OK(api_nvnTextureBuilderSetDefaults(&a.tb));T(api_nvnSamplerPoolInitialize(&a.sp,&pool,16384,257));OK(api_nvnSamplerPoolFinalize(&a.sp));
 T(peek(&pool)->refs==0);
 NVNtexture source={0};NVNtexturePool textures={0};texture_builder(&a.tb,&d,&pool,0);T(api_nvnTextureInitialize(&source,&a.tb));T(api_nvnTexturePoolInitialize(&textures,&pool,0,257));
 OK(api_nvnTexturePoolRegisterTexture(&textures,256,&source,NULL));T(peek(&pool)->refs==3);OK(api_nvnTextureFinalize(&source));T(peek(&pool)->refs==2);
 T(api_nvnProgramInitialize(&a.program,&d));T(peek(&pool)->refs==2&&peek(&textures)->slots[256].source_layout&&peek(&textures)->slots[256].snapshot.texture.width==16);OK(api_nvnProgramFinalize(&a.program));
 api_nvnMemoryPoolFinalize(&pool);BAD(EBUSY);OK(api_nvnTexturePoolFinalize(&textures));T(peek(&pool)->refs==0);
 /* Only explicit descriptor-slot writes are allowed; source/event data and
  * all surrounding storage remain byte-identical. */
 for(size_t i=0;i<65536;i++)if(!((i>=8192&&i<8224)||(i>=24576&&i<24608)))T(((unsigned char*)ram)[i]==0x51);
 OK(api_nvnMemoryPoolFinalize(&pool));free(ram);OK(api_nvnDeviceFinalize(&d));while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"builder_kinds_reused\":%u,\"source_alias_cases\":%u,\"GPU_execution\":false}\n",checks,builders_reused,source_aliases);return 0;
}
