/* Host whitebox ownership/cache checks only. Existing memory_backend never
 * executes GPU work; command recording below is not a GPU/game result. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvn-native/src/nvn.c"
static unsigned checks, imports, destroys, caches, live_imports, accepted_flags, rejected_flags;
static int fail_import, fail_cache, fail_destroy, initial_clean;
static size_t last_offset, last_bytes;
static int last_direction;
static nn_bo *last_import;
static const void *initial_key;
int __real_nn_bo_import(nn_device *,void *,size_t,nn_bo **);
int __real_nn_bo_cache(nn_bo *,size_t,size_t,int);
int __real_nn_bo_destroy(nn_bo *);
int __wrap_nn_bo_import(nn_device *d,void *memory,size_t size,nn_bo **out)
{
    imports++;
    if(fail_import) { int e=fail_import;fail_import=0;*out=NULL;return e; }
    int e=__real_nn_bo_import(d,memory,size,out);
    if(!e) { live_imports++;last_import=*out;assert(nn_bo_map(*out)==memory); }
    return e;
}
int __wrap_nn_bo_cache(nn_bo *b,size_t offset,size_t bytes,int from)
{
    caches++;last_offset=offset;last_bytes=bytes;last_direction=from;
    if(initial_clean) {
        assert(b==last_import&&offset==0&&bytes==4096&&from==0);
        for(struct object *o=objects;o;o=o->next) assert(o->key!=initial_key);
    }
    if(fail_cache) { int e=fail_cache;fail_cache=0;return e; }
    return __real_nn_bo_cache(b,offset,bytes,from);
}
int __wrap_nn_bo_destroy(nn_bo *b)
{
    destroys++;
    if(fail_destroy) { int e=fail_destroy;fail_destroy=0;return e; }
    int e=__real_nn_bo_destroy(b);
    if(!e) { assert(live_imports);live_imports--; }
    return e;
}
#define CHECK(x) do { checks++;if(!(x)){fprintf(stderr,"FAIL %d %s errno=%d\n",__LINE__,#x,nn_nvn_error());return 1;} } while(0)
#define CLEAN() CHECK(nn_nvn_error()==0)
#define ERROR(e) do {CHECK(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static struct object *peek(const void *key)
{ for(struct object *o=objects;o;o=o->next)if(o->key==key)return o;return NULL; }
int main(void)
{
    NVNdeviceBuilder db={0};NVNdevice device={0},failure_device={0};
    api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&device,&db));CLEAN();
    NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool={0};
    api_nvnMemoryPoolBuilderSetDefaults(&pb);CLEAN();CHECK(peek(&pb)->flags==0x21);
    api_nvnMemoryPoolBuilderSetDevice(&pb,&device);CLEAN();
    void *storage=NULL;CHECK(!posix_memalign(&storage,4096,4096));memset(storage,0xa7,4096);
    api_nvnMemoryPoolBuilderSetStorage(&pb,storage,4096);api_nvnMemoryPoolBuilderSetFlags(&pb,0xe4);
    api_nvnMemoryPoolBuilderSetDefaults(&pb);CLEAN();
    CHECK(peek(&pb)->flags==0x21&&peek(&pb)->device==peek(&device));
    CHECK(peek(&pb)->storage==NULL&&peek(&pb)->size==0);
    api_nvnMemoryPoolBuilderSetStorage(&pb,storage,4096);
    const int good[]={0x21,0x61,0xa1,0xe1,0x24,0x64,0xa4,0xe4};
    PFNNVNMEMORYPOOLMAPPROC map=(PFNNVNMEMORYPOOLMAPPROC)nvnBootstrapLoader("nvnMemoryPoolMap");
    PFNNVNBUFFERMAPPROC bmap=(PFNNVNBUFFERMAPPROC)nvnBootstrapLoader("nvnBufferMap");
    CHECK(map&&bmap);
    for(unsigned i=0;i<sizeof(good)/sizeof(good[0]);i++) {
        int flags=good[i],noaccess=flags&1;unsigned before=caches;
        api_nvnMemoryPoolBuilderSetFlags(&pb,flags);initial_clean=noaccess;initial_key=&pool;
        CHECK(api_nvnMemoryPoolInitialize(&pool,&pb));initial_clean=0;CLEAN();
        CHECK(caches==before+(unsigned)noaccess);
        CHECK(peek(&pool)->flags==flags&&peek(&pool)->storage==storage&&nn_bo_map(peek(&pool)->bo)==storage);
        CHECK(peek(&pool)->device==peek(&device)&&peek(&device)->refs==1&&live_imports==1);
        CHECK(map(&pool)==(noaccess?NULL:storage));CLEAN();
        unsigned prior_imports=imports;CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb));ERROR(EEXIST);CHECK(imports==prior_imports);
        NVNbufferBuilder bb={0};NVNbuffer buffer={0};
        api_nvnBufferBuilderSetDefaults(&bb);api_nvnBufferBuilderSetDevice(&bb,&device);
        api_nvnBufferBuilderSetStorage(&bb,&pool,128,256);CHECK(api_nvnBufferInitialize(&buffer,&bb));CLEAN();
        CHECK(bmap(&buffer)==(noaccess?NULL:(char*)storage+128));CLEAN();
        CHECK(api_nvnBufferGetAddress(&buffer)==(uintptr_t)storage+128);
        unsigned count=caches;
        api_nvnMemoryPoolFlushMappedRange(&pool,32,64);api_nvnMemoryPoolInvalidateMappedRange(&pool,96,128);
        api_nvnBufferFlushMappedRange(&buffer,16,32);api_nvnBufferInvalidateMappedRange(&buffer,48,64);CLEAN();
        CHECK(caches==count+(noaccess?0u:4u));
        if(!noaccess)CHECK(last_offset==176&&last_bytes==64&&last_direction==1);
        count=caches;
        api_nvnMemoryPoolFlushMappedRange(&pool,4096,0);api_nvnBufferInvalidateMappedRange(&buffer,256,0);CLEAN();
        CHECK(caches==count+(noaccess?0u:2u));
        api_nvnMemoryPoolFlushMappedRange(&pool,-1,0);ERROR(EINVAL);
        api_nvnMemoryPoolInvalidateMappedRange(&pool,4097,0);ERROR(EINVAL);
        api_nvnMemoryPoolFlushMappedRange(&pool,4096,1);ERROR(EINVAL);
        api_nvnBufferFlushMappedRange(&buffer,-1,0);ERROR(EINVAL);
        api_nvnBufferInvalidateMappedRange(&buffer,256,1);ERROR(EINVAL);
        api_nvnMemoryPoolFinalize(&pool);ERROR(EBUSY);CHECK(peek(&pool));
        api_nvnBufferFinalize(&buffer);api_nvnMemoryPoolFinalize(&pool);CLEAN();
        CHECK(!peek(&pool)&&live_imports==0&&peek(&device)->refs==0);
        for(size_t n=0;n<4096;n++)CHECK(((unsigned char*)storage)[n]==0xa7);
        accepted_flags++;
    }
    /* CPU_UNCACHED|GPU_CACHED now has its own coherent-import test. GPU
     * uncached and invalid access combinations remain unsupported here. */
    const int bad[]={0,1,2,4,0x20,0x25,0x28,0x124,0x224,-1};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        unsigned before=imports;api_nvnMemoryPoolBuilderSetFlags(&pb,bad[i]);
        CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb));ERROR(ENOTSUP);
        CHECK(imports==before&&!peek(&pool)&&peek(&device)->refs==0);rejected_flags++;
    }
    api_nvnMemoryPoolBuilderSetFlags(&pb,0xa1);
    fail_import=ENOMEM;CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb));ERROR(ENOMEM);
    CHECK(!peek(&pool)&&live_imports==0&&peek(&device)->refs==0);
    unsigned before=destroys;fail_cache=EIO;initial_clean=1;initial_key=&pool;
    CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb));initial_clean=0;ERROR(EIO);
    CHECK(destroys==before+1&&!peek(&pool)&&live_imports==0&&peek(&device)->refs==0);
    CHECK(api_nvnDeviceInitialize(&failure_device,&db));api_nvnMemoryPoolBuilderSetDevice(&pb,&failure_device);
    fail_cache=EIO;fail_destroy=EAGAIN;CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb));ERROR(EIO);
    CHECK(!peek(&pool)&&live_imports==1&&peek(&failure_device)->refs==1&&peek(&failure_device)->retired);
    api_nvnDeviceFinalize(&failure_device);CLEAN();CHECK(!peek(&failure_device)&&live_imports==0);
    /* Retained builder device is now freed: Defaults preserves that weak
     * value, while Initialize must reject it without dereferencing it. */
    api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetStorage(&pb,storage,4096);
    before=imports;CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb));ERROR(EINVAL);
    CHECK(imports==before&&!peek(&pool));
    api_nvnMemoryPoolBuilderSetDevice(&pb,&device);
    api_nvnMemoryPoolBuilderSetFlags(&pb,0xa1);CHECK(api_nvnMemoryPoolInitialize(&pool,&pb));CLEAN();
    NVNtexturePool denied_tp={0};NVNsamplerPool denied_sp={0};
    CHECK(!api_nvnTexturePoolInitialize(&denied_tp,&pool,0,260));ERROR(ENOTSUP);
    CHECK(!api_nvnSamplerPoolInitialize(&denied_sp,&pool,0,260));ERROR(ENOTSUP);
    CHECK(!peek(&denied_tp)&&!peek(&denied_sp)&&peek(&pool)->refs==0);
    NVNprogram denied_program={0};uint32_t control_word=0;
    CHECK(api_nvnProgramInitialize(&denied_program,&device));
    NVNshaderData denied_shader={.data=nn_bo_address(peek(&pool)->bo),.control=&control_word};
    unsigned no_shader_imports=imports;
    CHECK(!api_nvnProgramSetShaders(&denied_program,1,&denied_shader));ERROR(ENOTSUP);
    CHECK(peek(&denied_program)->bo==NULL&&peek(&denied_program)->fs_bo==NULL&&imports==no_shader_imports);
    api_nvnProgramFinalize(&denied_program);CLEAN();
    /* Real core records a GPU write into a cached command arena, retaining
     * the GPU-only target. The backend never submits/executes this packet. */
    void *command_storage=NULL;CHECK(!posix_memalign(&command_storage,4096,4096));memset(command_storage,0,4096);
    NVNmemoryPool commands={0};api_nvnMemoryPoolBuilderSetStorage(&pb,command_storage,4096);
    api_nvnMemoryPoolBuilderSetFlags(&pb,0x24);CHECK(api_nvnMemoryPoolInitialize(&commands,&pb));
    NVNcommandBuffer cb={0};_Alignas(struct control) unsigned char control[1024];
    CHECK(api_nvnCommandBufferInitialize(&cb,&device));
    api_nvnCommandBufferAddCommandMemory(&cb,&pool,0,4096);ERROR(ENOTSUP);CHECK(peek(&cb)->record->pool==NULL);
    api_nvnCommandBufferAddCommandMemory(&cb,&commands,0,4096);CLEAN();
    api_nvnCommandBufferAddCommandMemory(&cb,&pool,0,4096);ERROR(ENOTSUP);CHECK(peek(&cb)->record->pool==peek(&commands));
    api_nvnCommandBufferAddControlMemory(&cb,control,sizeof control);
    api_nvnCommandBufferBeginRecording(&cb);api_nvnCommandBufferClearBuffer(&cb,nn_bo_address(peek(&pool)->bo),16,0x12345678);
    NVNcommandHandle handle=api_nvnCommandBufferEndRecording(&cb);CHECK(handle);CLEAN();
    CHECK(*(uint32_t*)command_storage!=0&&((uint32_t*)command_storage)[3]==0x12345678);
    CHECK(map(&pool)==NULL&&nn_bo_map(peek(&pool)->bo)==storage);CLEAN();
    api_nvnMemoryPoolFinalize(&pool);ERROR(EBUSY);
    api_nvnDeviceFinalizeCommandHandle(&device,handle);api_nvnCommandBufferFinalize(&cb);
    api_nvnMemoryPoolFinalize(&pool);api_nvnMemoryPoolFinalize(&commands);CLEAN();
    CHECK(live_imports==0);api_nvnDeviceFinalize(&device);CLEAN();
    for(size_t n=0;n<4096;n++)CHECK(((unsigned char*)storage)[n]==0xa7);
    /* Builders are deliberately side-table objects in this core; release
     * their host-test metadata so leak sanitizer covers actual owned BOs. */
    while(objects){CHECK(objects->kind==DB||objects->kind==PB||objects->kind==BB);remove_object(objects);}
    free(storage);free(command_storage);
    printf("{\"passed\":true,\"scope\":\"host memory backend only, no GPU/game execution\",\"checks\":%u,\"accepted_flags\":%u,\"rejected_flags\":%u,\"imports\":%u,\"cache_calls\":%u,\"destroy_calls\":%u,\"live_imports\":%u}\n",checks,accepted_flags,rejected_flags,imports,caches,destroys,live_imports);
    return 0;
}
