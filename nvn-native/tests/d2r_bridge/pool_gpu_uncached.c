/* HOST ONLY: actual NVN core + memory_backend. No GPU work or CPU emulation.
 * Whitebox injection is used ONLY to reach defensive late/submit backstops. */
#define _POSIX_C_SOURCE 200809L
#ifndef NN_UNQUALIFIED_FLAGS
#define NN_UNQUALIFIED_FLAGS 0x14
#endif
#if NN_UNQUALIFIED_FLAGS != 0x12 && NN_UNQUALIFIED_FLAGS != 0x14
#error "NN_UNQUALIFIED_FLAGS must be 0x12 or 0x14"
#endif
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef NN_TEST_NVN_SOURCE
#define NN_TEST_NVN_SOURCE "../../src/nvn.c"
#endif
#include NN_TEST_NVN_SOURCE
#define nn_bo_alloc backend_alloc
#define nn_bo_import backend_import
#define nn_bo_import_coherent backend_coherent
#define nn_bo_cache backend_cache
#define nn_queue_submit backend_submit
#define nn_bo_import_dmabuf backend_dmabuf
#ifndef NN_MEMORY_BACKEND_SOURCE
#define NN_MEMORY_BACKEND_SOURCE "memory_backend.c"
#endif
#include NN_MEMORY_BACKEND_SOURCE
#undef nn_bo_alloc
#undef nn_bo_import
#undef nn_bo_import_coherent
#undef nn_bo_cache
#undef nn_queue_submit
#undef nn_bo_import_dmabuf
static unsigned checks,alloc_calls,import_calls,coherent_calls,cache_calls,submit_calls,rejections;
static int fail_import;
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **b) {alloc_calls++;return backend_alloc(d,n,b);}
int nn_bo_import(nn_device *d,void *p,size_t n,nn_bo **b)
{import_calls++;if(fail_import){*b=NULL;return ENOMEM;}return backend_import(d,p,n,b);}
int nn_bo_import_coherent(nn_device *d,void *p,size_t n,nn_bo **b)
{coherent_calls++;if(fail_import){*b=NULL;return ENOMEM;}return backend_coherent(d,p,n,b);}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int f) {cache_calls++;return backend_cache(b,o,n,f);}
int nn_queue_submit(nn_queue *q,nn_bo *b,size_t o,size_t n,nn_bo *const *r,size_t c,uint32_t *t)
{submit_calls++;return backend_submit(q,b,o,n,r,c,t);}
/* External image implementation is linked normally; this synthetic exporter is
 * never invoked in this CPU-only fixture. It supplies a required backend ABI. */
int nn_bo_import_dmabuf(nn_device *d,int f,size_t n,nn_bo **b)
{(void)d;(void)f;(void)n;*b=NULL;abort();}
#define T(x) do {++checks;if(!(x)){fprintf(stderr,"line%d %s errno%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(nn_nvn_error()==0);}while(0)
#define ERR(x,e) do{x;T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static struct object *peek(const void *key)
{for(struct object *o=objects;o;o=o->next)if(o->key==key)return o;return NULL;}
static unsigned char *cpu[2];
struct checkpoint {
    struct object values[40];const struct object *keys[40];unsigned count;
    struct recording record;unsigned have_record;
    unsigned allocations,imports,coherents,caches,submits,bos;
    unsigned char bytes[2][32768];
};
static void checkpoint(struct checkpoint *s,struct object *device,struct object *command)
{
    memset(s,0,sizeof(*s));
    for(struct object *o=objects;o;o=o->next){T(s->count<40);s->keys[s->count]=o;s->values[s->count++]=*o;}
    if(command){s->record=*command->record;s->have_record=1;}
    s->allocations=alloc_calls;s->imports=import_calls;s->coherents=coherent_calls;
    s->caches=cache_calls;s->submits=submit_calls;s->bos=device->gpu->bos;
    for(unsigned i=0;i<2;i++)memcpy(s->bytes[i],cpu[i],32768);
}
static void unchanged(const struct checkpoint *s,struct object *device,struct object *command)
{
    unsigned n=0;for(struct object *o=objects;o;o=o->next){T(n<s->count&&s->keys[n]==o);T(!memcmp(o,s->values+n,sizeof(*o)));n++;}
    T(n==s->count);if(s->have_record)T(!memcmp(command->record,&s->record,sizeof(s->record)));
    T(s->allocations==alloc_calls&&s->imports==import_calls&&s->coherents==coherent_calls);
    T(s->caches==cache_calls&&s->submits==submit_calls&&s->bos==device->gpu->bos);
    for(unsigned i=0;i<2;i++)T(!memcmp(s->bytes[i],cpu[i],32768));
}
static void observed(const NVNmemoryPool *pool,size_t offset,size_t size,const char *consumer)
{
    struct nn_nvn_pool_access_observation o={0};
    T(nn_nvn_error()==ENOTSUP);T(!nn_nvn_get_pool_access_observation(&o));
    T(o.pool==pool&&o.flags==NN_UNQUALIFIED_FLAGS&&o.policy==NN_NVN_GPU_UNCACHED_UNQUALIFIED);
    T(o.offset==offset&&o.size==size&&!strcmp(o.consumer,consumer));
    T(nn_nvn_error()==ENOTSUP);rejections++;
}
static struct object *fake(const void *key,enum kind kind,struct object *device,struct object *pool,size_t off,size_t size)
{
    struct object *o=create(key,kind);T(o);o->device=device;o->pool=pool;o->offset=off;o->size=size;
    if(pool)o->bo=pool->bo;
    return o;
}
int main(void)
{
    NVNdeviceBuilder db={0};NVNdevice dev={0};NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool[2]={0};
    NVNbufferBuilder bb={0};NVNbuffer buffer={0};NVNcommandBuffer cmd={0};NVNqueueBuilder qb={0};NVNqueue queue={0};
    struct nn_nvn_pool_access_observation obs,old;
    memset(&obs,0xa5,sizeof(obs));old=obs;
    T(nn_nvn_get_pool_access_observation(&obs)==ENODATA&&!memcmp(&obs,&old,sizeof(obs)));
    T(nn_nvn_get_pool_access_observation(NULL)==EINVAL&&nn_nvn_error()==0);
    OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dev,&db));
    struct object *device=peek(&dev);T(device);
    for(unsigned i=0;i<2;i++){
        T(!posix_memalign((void **)&cpu[i],4096,32768));memset(cpu[i],0xa5,32768);
        OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&dev));
        OK(api_nvnMemoryPoolBuilderSetStorage(&pb,cpu[i],32768));
        OK(api_nvnMemoryPoolBuilderSetFlags(&pb,i?NN_UNQUALIFIED_FLAGS:0x24));T(api_nvnMemoryPoolInitialize(pool+i,&pb));
        T(api_nvnMemoryPoolMap(pool+i)==cpu[i]);T(api_nvnMemoryPoolGetBufferAddress(pool+i)==(uintptr_t)cpu[i]);
    }
    struct object *cached=peek(pool),*blocked=peek(pool+1);T(blocked->gpu_policy==NN_NVN_GPU_UNCACHED_UNQUALIFIED);
    T(cached->gpu_policy==NN_NVN_GPU_POOL_CACHED&&blocked->flags==NN_UNQUALIFIED_FLAGS&&blocked->storage==cpu[1]);
    T(import_calls==(NN_UNQUALIFIED_FLAGS==0x12?1u:2u)&&coherent_calls==(NN_UNQUALIFIED_FLAGS==0x12?1u:0u)&&cache_calls==0);
    T((nn_bo_flags(blocked->bo)&KGSL_CACHEMODE_MASK)==(KGSL_CACHEMODE_WRITEBACK<<KGSL_CACHEMODE_SHIFT));
    if(NN_UNQUALIFIED_FLAGS==0x12)T(nn_bo_flags(blocked->bo)&KGSL_MEMFLAGS_IOCOHERENT);
    const int unsupported[]={0x52,0x92,0xd2,0x54,0x94,0xd4,0x1c,0x34,-1,0x112,0x114};
    for(unsigned i=0;i<sizeof(unsupported)/sizeof(*unsupported);i++){
        NVNmemoryPool invalid={0};unsigned imports=import_calls+coherent_calls;
        OK(api_nvnMemoryPoolBuilderSetFlags(&pb,unsupported[i]));T(!api_nvnMemoryPoolInitialize(&invalid,&pb));
        T(nn_nvn_error()==ENOTSUP&&!peek(&invalid)&&import_calls+coherent_calls==imports);nn_nvn_clear_error();
    }
    /* Known virtual aliases must not route the same pages around policy. */
    for(unsigned i=0;i<2;i++) {
        NVNmemoryPool alias={0};unsigned before=import_calls+coherent_calls;
        OK(api_nvnMemoryPoolBuilderSetStorage(&pb,cpu[i]+4096,4096));
        OK(api_nvnMemoryPoolBuilderSetFlags(&pb,i?0x24:NN_UNQUALIFIED_FLAGS));
        T(!api_nvnMemoryPoolInitialize(&alias,&pb));
        T(nn_nvn_error()==ENOTSUP&&!peek(&alias)&&import_calls+coherent_calls==before);nn_nvn_clear_error();
    }
    OK(api_nvnMemoryPoolBuilderSetStorage(&pb,cpu[1],32768));
    NVNmemoryPool failed={0};
    /* Use a nonoverlapping valid allocation to reach a real import failure. */
    void *failed_storage=NULL;T(!posix_memalign(&failed_storage,4096,4096));
    OK(api_nvnMemoryPoolBuilderSetStorage(&pb,failed_storage,4096));
    OK(api_nvnMemoryPoolBuilderSetFlags(&pb,NN_UNQUALIFIED_FLAGS));
    unsigned failed_plain=import_calls,failed_coherent=coherent_calls;
    size_t refs=device->refs;fail_import=1;T(!api_nvnMemoryPoolInitialize(&failed,&pb));
    T(import_calls==failed_plain+(NN_UNQUALIFIED_FLAGS==0x12?0u:1u));
    T(coherent_calls==failed_coherent+(NN_UNQUALIFIED_FLAGS==0x12?1u:0u));
    T(nn_nvn_error()==ENOMEM&&!peek(&failed)&&device->refs==refs);nn_nvn_clear_error();fail_import=0;free(failed_storage);
    OK(api_nvnBufferBuilderSetDefaults(&bb));OK(api_nvnBufferBuilderSetDevice(&bb,&dev));
    OK(api_nvnBufferBuilderSetStorage(&bb,pool+1,128,256));T(api_nvnBufferInitialize(&buffer,&bb));
    T(api_nvnBufferMap(&buffer)==cpu[1]+128&&api_nvnBufferGetAddress(&buffer)==(uintptr_t)cpu[1]+128);
    unsigned caches=cache_calls;
    OK(api_nvnMemoryPoolFlushMappedRange(pool+1,0,32768));OK(api_nvnMemoryPoolInvalidateMappedRange(pool+1,64,64));
    OK(api_nvnBufferFlushMappedRange(&buffer,16,16));OK(api_nvnBufferInvalidateMappedRange(&buffer,32,32));T(cache_calls==caches+(NN_UNQUALIFIED_FLAGS==0x12?0u:4u));
    ERR(api_nvnMemoryPoolFlushMappedRange(pool+1,-1,1),EINVAL);
    ERR(api_nvnBufferInvalidateMappedRange(&buffer,256,1),EINVAL);
    T(cache_calls==caches+(NN_UNQUALIFIED_FLAGS==0x12?0u:4u));
    ERR(api_nvnMemoryPoolFinalize(pool+1),EBUSY);T(peek(pool+1)==blocked);
    T(api_nvnCommandBufferInitialize(&cmd,&dev));
    OK(api_nvnCommandBufferAddCommandMemory(&cmd,pool,0,8192));
    struct object *command=peek(&cmd);struct checkpoint ck;
#define REJECT(call,offset,size,name) do {checkpoint(&ck,device,command);call;observed(pool+1,offset,size,name);unchanged(&ck,device,command);nn_nvn_clear_error();}while(0)
    REJECT(api_nvnCommandBufferAddCommandMemory(&cmd,pool+1,64,4096),64,4096,"nvnCommandBufferAddCommandMemory");
    /* Sticky unrelated error must not gain an unrelated policy observation. */
    error(EINVAL);api_nvnCommandBufferAddCommandMemory(&cmd,pool+1,0,4096);
    T(nn_nvn_error()==EINVAL&&nn_nvn_get_pool_access_observation(&obs)==ENODATA);nn_nvn_clear_error();
    _Alignas(16) unsigned char control[1024]={0};OK(api_nvnCommandBufferAddControlMemory(&cmd,control,sizeof(control)));
    OK(api_nvnCommandBufferBeginRecording(&cmd));
    /* Real typed NULL unbind must return the existing binding refs and remain
     * legal when already NULL. Exercise both independent descriptor domains. */
    NVNtexturePool unbind_tp={0};NVNsamplerPool unbind_sp={0};
    T(api_nvnTexturePoolInitialize(&unbind_tp,pool,8192,272));
    T(api_nvnSamplerPoolInitialize(&unbind_sp,pool,17920,272));T(nn_nvn_error()==0);
    struct object *ut=peek(&unbind_tp),*us=peek(&unbind_sp);T(ut&&us&&!ut->refs&&!us->refs);
    PFNNVNCOMMANDBUFFERSETTEXTUREPOOLPROC set_tp=(PFNNVNCOMMANDBUFFERSETTEXTUREPOOLPROC)nvnBootstrapLoader("nvnCommandBufferSetTexturePool");
    PFNNVNCOMMANDBUFFERSETSAMPLERPOOLPROC set_sp=(PFNNVNCOMMANDBUFFERSETSAMPLERPOOLPROC)nvnBootstrapLoader("nvnCommandBufferSetSamplerPool");
    T(set_tp&&set_sp);OK(set_tp(&cmd,&unbind_tp));OK(set_sp(&cmd,&unbind_sp));
    T(ut->refs==1&&us->refs==1&&command->record->texture_pool==ut&&command->record->sampler_pool==us);
    OK(set_tp(&cmd,NULL));T(!ut->refs&&!command->record->texture_pool&&us->refs==1);
    OK(set_sp(&cmd,NULL));T(!us->refs&&!command->record->sampler_pool);
    OK(set_tp(&cmd,NULL));OK(set_sp(&cmd,NULL));T(!ut->refs&&!us->refs);
    OK(api_nvnTexturePoolFinalize(&unbind_tp));OK(api_nvnSamplerPoolFinalize(&unbind_sp));
    REJECT(api_nvnCommandBufferClearBuffer(&cmd,(uintptr_t)cpu[1]+128,16,7),128,16,"nvnCommandBufferClearBuffer");
    NVNbufferRange ranges[2]={{(uintptr_t)cpu[0]+256,64},{(uintptr_t)cpu[1]+512,64}};
    /* Public bindings retain CPU values/owners only. Actual consumed Draw or
     * Dispatch remains subject to the GPU policy gates below. */
    unsigned binds_alloc=alloc_calls,binds_cache=cache_calls,binds_submit=submit_calls;
    size_t blocked_refs=blocked->refs;
    OK(api_nvnCommandBufferBindUniformBuffers(&cmd,NVN_SHADER_STAGE_COMPUTE,0,2,ranges));
    OK(api_nvnCommandBufferBindStorageBuffers(&cmd,NVN_SHADER_STAGE_COMPUTE,0,2,ranges));
    OK(api_nvnCommandBufferBindVertexBuffer(&cmd,0,(uintptr_t)cpu[1]+96,96));
    OK(api_nvnCommandBufferBindUniformBuffer(&cmd,NVN_SHADER_STAGE_VERTEX,0,(uintptr_t)cpu[1]+64,64));
    T(command->record->ubos[1].pool==blocked&&command->record->ssbos[1].pool==blocked);
    T(command->record->vertices[0].pool==blocked&&command->record->graphics_ubos[0][0].pool==blocked);
    T(blocked->refs==blocked_refs+4&&nn_nvn_get_pool_access_observation(&obs)==ENODATA);
    checkpoint(&ck,device,command);NVNbufferRange bad_ranges[2]={ranges[0],{(uintptr_t)cpu[1]+32768,64}};
    ERR(api_nvnCommandBufferBindUniformBuffers(&cmd,NVN_SHADER_STAGE_COMPUTE,0,2,bad_ranges),EINVAL);
    unchanged(&ck,device,command);
    const NVNbufferRange unbound[2]={{0,0},{0,0}};
    OK(api_nvnCommandBufferBindUniformBuffers(&cmd,NVN_SHADER_STAGE_COMPUTE,0,2,unbound));
    OK(api_nvnCommandBufferBindStorageBuffers(&cmd,NVN_SHADER_STAGE_COMPUTE,0,2,unbound));
    OK(api_nvnCommandBufferBindVertexBuffer(&cmd,0,0,0));
    OK(api_nvnCommandBufferBindUniformBuffer(&cmd,NVN_SHADER_STAGE_VERTEX,0,0,0));
    T(blocked->refs==blocked_refs&&binds_alloc==alloc_calls&&binds_cache==cache_calls&&binds_submit==submit_calls);
    NVNtexturePool tp={0};NVNsamplerPool sp={0};
    T(api_nvnTexturePoolInitialize(&tp,pool+1,0,272));OK(api_nvnTexturePoolFinalize(&tp));
    T(api_nvnSamplerPoolInitialize(&sp,pool+1,32,272));OK(api_nvnSamplerPoolFinalize(&sp));
    NVNtextureBuilder tb={0};NVNtexture tex={0};
    OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnTextureBuilderSetDevice(&tb,&dev));
    struct object *builder=find(&tb,TB);builder->texture=(struct texture_desc){NVN_TEXTURE_TARGET_2D,2,2,1,1,0,NVN_FORMAT_RGBA8,NVN_TEXTURE_FLAGS_LINEAR,256,TEX_COMPLETE};
    OK(api_nvnTextureBuilderSetStorage(&tb,pool+1,0));
    REJECT((void)api_nvnTextureInitialize(&tex,&tb),0,4096,"nvnTextureInitialize");T(!peek(&tex));
    NVNprogram program={0};T(api_nvnProgramInitialize(&program,&dev));uint32_t control_word=0;
    NVNshaderData shader={(uintptr_t)cpu[1]+256,&control_word};
    REJECT((void)api_nvnProgramSetShaders(&program,1,&shader),256,1,"nvnProgramSetShaders");
    REJECT((void)matches_shader(device,&shader,cpu[1]+256,16,&control_word,4),256,16,"nvnProgramSetShaders");
    /* Getter does not clear/replace first sticky diagnostic. */
    api_nvnCommandBufferClearBuffer(&cmd,(uintptr_t)cpu[1]+256,16,7);T(!nn_nvn_get_pool_access_observation(&old));
    api_nvnProgramSetShaders(&program,1,&shader);T(!nn_nvn_get_pool_access_observation(&obs));
    T(!memcmp(&obs,&old,sizeof(obs)));nn_nvn_clear_error();T(nn_nvn_get_pool_access_observation(&obs)==ENODATA);
    /* Inject otherwise unreachable CPU-only resources solely for defensive gates. */
    NVNtexture fake_tex_key={0};struct object *ft=fake(&fake_tex_key,TEXTURE,device,blocked,0,16384);
    ft->texture=(struct texture_desc){NVN_TEXTURE_TARGET_2D,64,64,1,1,0,NVN_FORMAT_RGBA8,NVN_TEXTURE_FLAGS_LINEAR,256,TEX_COMPLETE};
    const NVNtexture *colors[]={&fake_tex_key};
    REJECT(api_nvnCommandBufferSetRenderTargets(&cmd,1,colors,NULL,NULL,NULL),0,16384,"nvnCommandBufferSetRenderTargets");
    NVNtexturePool fake_tp_key={0};struct object *ftp=fake(&fake_tp_key,TPOOL,device,blocked,0,8704);ftp->slot_count=272;
    ERR(api_nvnCommandBufferSetTexturePool(&cmd,&fake_tp_key),ENOTSUP);
    ftp->pool=cached;ftp->cpu_software_table=1;ftp->slots=calloc(272,sizeof(*ftp->slots));T(ftp->slots);
    REJECT(api_nvnTexturePoolRegisterTexture(&fake_tp_key,257,&fake_tex_key,NULL),0,16384,"nvnTexturePoolRegisterTexture");
    struct descriptor_entry entry={.pool=blocked,.offset=32,.size=4096,.valid=1};struct texture_slot desc={0};
    REJECT((void)descriptor_write(ftp,257,&desc,entry),32,4096,"descriptor_write");
    struct descriptor_entry empty_entry={0};
    T(!memcmp(&ftp->slots[257],&empty_entry,sizeof(empty_entry)));
    struct bound saved=command->record->vertex;
    command->record->vertex=(struct bound){.pool=blocked,.offset=128,.size=96,.address=(uintptr_t)cpu[1]+128};
    REJECT(api_nvnCommandBufferDrawArrays(&cmd,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3),128,96,"nvnCommandBufferDrawArrays");
    command->record->program=peek(&program);
    REJECT(api_nvnCommandBufferDispatchCompute(&cmd,1,1,1),128,96,"nvnCommandBufferDispatchCompute");
    command->record->program=NULL;command->record->vertex=saved;
    struct late_draw draw={.target=ft};nn_bo *job=NULL,*sampled=NULL;size_t bytes=0;
    REJECT((void)materialize_draw(device,&draw,&job,&bytes,&sampled),0,16384,"materialize_draw");T(!job&&!sampled&&!bytes);
    /* A real Draw always has a target. Keep that target cached while testing
     * the independent sampled-entry policy backstop. */
    ft->pool=cached;ft->bo=cached->bo;
    draw=(struct late_draw){.target=ft,.texture_pool=ftp,.texture_id=257};ftp->slots[257]=entry;
    REJECT((void)materialize_draw(device,&draw,&job,&bytes,&sampled),32,4096,"materialize_draw");
    OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dev));T(api_nvnQueueInitialize(&queue,&qb));
    struct object *resource_list[]={blocked};
    struct handle h2={.id=902,.device=device,.commands=blocked,.offset=64,.bytes=16};
    struct handle h1={.next=&h2,.id=901,.device=device,.commands=cached,.bytes=16};
    struct handle *saved_handles=handles;handles=&h1;NVNcommandHandle ids[]={901,902};
    REJECT(api_nvnQueueSubmitCommands(&queue,2,ids),64,16,"nvnQueueSubmitCommands");
    h2.commands=cached;h2.offset=0;h2.count=1;h2.resources=resource_list;
    REJECT(api_nvnQueueSubmitCommands(&queue,2,ids),0,32768,"nvnQueueSubmitCommands");
    h2.count=0;h2.draws=&draw;
    REJECT(api_nvnQueueSubmitCommands(&queue,2,ids),32,4096,"nvnQueueSubmitCommands");
    handles=saved_handles;T(!submit_calls);
    /* Cached consumers must still record/create normally; no GPU is executed. */
    OK(api_nvnCommandBufferBindUniformBuffer(&cmd,NVN_SHADER_STAGE_COMPUTE,0,(uintptr_t)cpu[0]+256,64));
    T(command->record->ubos[0].pool==cached);
    size_t used=command->record->used;
    OK(api_nvnCommandBufferClearBuffer(&cmd,(uintptr_t)cpu[0]+12288,16,7));
    T(command->record->used>used&&command->record->current->count==1);
    T(api_nvnTexturePoolInitialize(&tp,pool,16384,272));T(nn_nvn_error()==0);
    OK(api_nvnTexturePoolFinalize(&tp));
    OK(api_nvnTextureBuilderSetStorage(&tb,pool,0));T(api_nvnTextureInitialize(&tex,&tb));
    T(nn_nvn_error()==0);OK(api_nvnTextureFinalize(&tex));
    shader.data=(uintptr_t)cpu[0]+256;T(!api_nvnProgramSetShaders(&program,1,&shader));
    T(nn_nvn_error()==ENOTSUP&&nn_nvn_get_pool_access_observation(&obs)==ENODATA);nn_nvn_clear_error();
    ftp->slots[257]=(struct descriptor_entry){0};free(ftp->slots);remove_object(ftp);remove_object(ft);
    OK(api_nvnQueueFinalize(&queue));OK(api_nvnCommandBufferFinalize(&cmd));OK(api_nvnProgramFinalize(&program));
    OK(api_nvnBufferFinalize(&buffer));OK(api_nvnMemoryPoolFinalize(pool+1));OK(api_nvnMemoryPoolFinalize(pool));
    T(device->refs==0&&device->gpu->bos==0);OK(api_nvnDeviceFinalize(&dev));
    for(unsigned i=0;i<2;i++)free(cpu[i]);
    while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==BB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
    T(!handles&&!submit_calls);
    printf("{\"scope\":\"HOST ONLY CPU-only pool and rejection tests\",\"flags\":%u,\"checks\":%u,\"policy_rejections\":%u,\"gpu_submits\":%u}\n",(unsigned)NN_UNQUALIFIED_FLAGS,checks,rejections,submit_calls);
    return 0;
}
