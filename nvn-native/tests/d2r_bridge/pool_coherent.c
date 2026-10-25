/* Actual core + host-only memory backend. Synthetic coherent flags are NOT
 * proof of GPU execution, Android import/cache behavior, or CPU/GPU coherence. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __user
#include "nvn-native/third_party/kgsl/msm_kgsl.h"
#undef __user
#include "nvn-native/src/nvn.c"

static unsigned checks, imports, coherent_imports, caches, destroys, live_imports;
static unsigned accepted_flags, rejected_flags;
static int fail_coherent, fail_destroy, fail_publish;
static void *expected_storage;
static const void *expected_key;
int __real_nn_bo_import(nn_device *, void *, size_t, nn_bo **);
int __real_nn_bo_import_coherent(nn_device *, void *, size_t, nn_bo **);
int __real_nn_bo_destroy(nn_bo *);
int __real_nn_bo_cache(nn_bo *, size_t, size_t, int);
void *__real_calloc(size_t, size_t);

static struct object *peek(const void *key)
{ for (struct object *o=objects; o; o=o->next) if (o->key==key) return o; return NULL; }

int __wrap_nn_bo_import(nn_device *device, void *storage, size_t bytes, nn_bo **out)
{
    imports++;
    int e=__real_nn_bo_import(device, storage, bytes, out);
    if (!e) live_imports++;
    return e;
}
int __wrap_nn_bo_import_coherent(nn_device *device, void *storage, size_t bytes, nn_bo **out)
{
    coherent_imports++;
    assert(storage==expected_storage && bytes==4096 && !peek(expected_key));
    if (fail_coherent) { int e=fail_coherent; fail_coherent=0; *out=NULL; return e; }
    int e=__real_nn_bo_import_coherent(device, storage, bytes, out);
    if (!e) { live_imports++; assert(nn_bo_map(*out)==storage); }
    return e;
}
int __wrap_nn_bo_destroy(nn_bo *bo)
{
    destroys++;
    if (fail_destroy) { int e=fail_destroy; fail_destroy=0; return e; }
    int e=__real_nn_bo_destroy(bo);
    if (!e) { assert(live_imports); live_imports--; }
    return e;
}
int __wrap_nn_bo_cache(nn_bo *bo, size_t offset, size_t bytes, int from)
{ caches++; return __real_nn_bo_cache(bo, offset, bytes, from); }
void *__wrap_calloc(size_t count, size_t bytes)
{
    /* Fail only the core object publication allocation, after import. The
     * preallocated rollback job and host backend BO allocation remain real. */
    if (fail_publish && count==1 && bytes==sizeof(struct object)) {
        fail_publish=0; return NULL;
    }
    return __real_calloc(count, bytes);
}

#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL %d %s errno=%d\n",__LINE__,#x,nn_nvn_error()); return 1; } } while (0)
#define CLEAN() CHECK(nn_nvn_error()==0)
#define ERROR(e) do { CHECK(nn_nvn_error()==(e)); nn_nvn_clear_error(); } while (0)

int main(void)
{
    NVNdeviceBuilder db={0}; NVNdevice device={0}, failed_device={0};
    NVNmemoryPoolBuilder pb={0}; NVNmemoryPool pool={0};
    api_nvnDeviceBuilderSetDefaults(&db);
    CHECK(api_nvnDeviceInitialize(&device,&db)); CLEAN();
    void *storage=NULL; CHECK(!posix_memalign(&storage,4096,4096));
    memset(storage,0xa7,4096); expected_storage=storage; expected_key=&pool;
    api_nvnMemoryPoolBuilderSetDefaults(&pb);
    api_nvnMemoryPoolBuilderSetDevice(&pb,&device);
    api_nvnMemoryPoolBuilderSetStorage(&pb,storage,4096); CLEAN();
    PFNNVNMEMORYPOOLMAPPROC map=(PFNNVNMEMORYPOOLMAPPROC)nvnBootstrapLoader("nvnMemoryPoolMap");
    PFNNVNBUFFERMAPPROC buffer_map=(PFNNVNBUFFERMAPPROC)nvnBootstrapLoader("nvnBufferMap");
    CHECK(map && buffer_map);
    const int good[]={0x22,0x62,0xa2,0xe2};
    for (unsigned i=0; i<sizeof(good)/sizeof(good[0]); i++) {
        unsigned before=coherent_imports;
        api_nvnMemoryPoolBuilderSetFlags(&pb,good[i]);
        CHECK(api_nvnMemoryPoolInitialize(&pool,&pb)); CLEAN();
        CHECK(coherent_imports==before+1 && imports==0 && caches==0);
        struct object *object=peek(&pool);
        CHECK(object && object->flags==good[i] && object->storage==storage && object->size==4096);
        CHECK(object->device==peek(&device) && peek(&device)->refs==1 && live_imports==1);
        CHECK(nn_bo_map(object->bo)==storage && map(&pool)==storage); CLEAN();
        uint32_t flags=nn_bo_flags(object->bo);
        CHECK((flags&KGSL_CACHEMODE_MASK)==(KGSL_CACHEMODE_WRITEBACK<<KGSL_CACHEMODE_SHIFT));
        CHECK(flags&KGSL_MEMFLAGS_IOCOHERENT);
        CHECK(api_nvnMemoryPoolGetBufferAddress(&pool)==(uintptr_t)storage);
        before=coherent_imports; CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb)); ERROR(EEXIST);
        CHECK(coherent_imports==before && imports==0);
        NVNbufferBuilder bb={0}; NVNbuffer buffer={0};
        api_nvnBufferBuilderSetDefaults(&bb); api_nvnBufferBuilderSetDevice(&bb,&device);
        api_nvnBufferBuilderSetStorage(&bb,&pool,128,256);
        CHECK(api_nvnBufferInitialize(&buffer,&bb)); CLEAN();
        CHECK(buffer_map(&buffer)==(char *)storage+128); CLEAN();
        CHECK(api_nvnBufferGetAddress(&buffer)==(uintptr_t)storage+128);
        api_nvnMemoryPoolFlushMappedRange(&pool,0,4096);
        api_nvnMemoryPoolInvalidateMappedRange(&pool,96,128);
        api_nvnBufferFlushMappedRange(&buffer,16,32);
        api_nvnBufferInvalidateMappedRange(&buffer,48,64);
        api_nvnMemoryPoolFlushMappedRange(&pool,4096,0);
        api_nvnBufferInvalidateMappedRange(&buffer,256,0); CLEAN();
        CHECK(caches==0);
        api_nvnMemoryPoolFlushMappedRange(&pool,-1,0); ERROR(EINVAL);
        api_nvnMemoryPoolInvalidateMappedRange(&pool,4097,0); ERROR(EINVAL);
        api_nvnMemoryPoolFlushMappedRange(&pool,4096,1); ERROR(EINVAL);
        api_nvnMemoryPoolInvalidateMappedRange(&pool,1,SIZE_MAX); ERROR(EINVAL);
        api_nvnBufferFlushMappedRange(&buffer,-1,0); ERROR(EINVAL);
        api_nvnBufferInvalidateMappedRange(&buffer,256,1); ERROR(EINVAL);
        api_nvnBufferFlushMappedRange(&buffer,1,SIZE_MAX); ERROR(EINVAL);
        CHECK(caches==0);
        api_nvnMemoryPoolFinalize(&pool); ERROR(EBUSY); CHECK(peek(&pool));
        api_nvnDeviceFinalize(&device); ERROR(EBUSY); CHECK(peek(&device));
        api_nvnBufferFinalize(&buffer); CLEAN();
        fail_destroy=EAGAIN; api_nvnMemoryPoolFinalize(&pool); ERROR(EAGAIN);
        CHECK(peek(&pool) && live_imports==1 && peek(&device)->refs==1);
        api_nvnMemoryPoolFinalize(&pool); CLEAN();
        CHECK(!peek(&pool) && live_imports==0 && peek(&device)->refs==0);
        for (size_t n=0; n<4096; n++) CHECK(((unsigned char *)storage)[n]==0xa7);
        accepted_flags++;
    }
    /* Unknown access/modifier combinations still reject; exact0x12/0x14
     * CPU-only GPU_UNCACHED constructors are covered by pool_gpu_uncached.
     * These reject before either importer; accepting coherent CPU access cannot mask them. */
    const int bad[]={0x23,0x26,0x122,-1};
    for (unsigned i=0; i<sizeof(bad)/sizeof(bad[0]); i++) {
        unsigned before=coherent_imports;
        api_nvnMemoryPoolBuilderSetFlags(&pb,bad[i]);
        CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb)); ERROR(ENOTSUP);
        CHECK(coherent_imports==before && imports==0 && !peek(&pool)); rejected_flags++;
    }
    api_nvnMemoryPoolBuilderSetFlags(&pb,0x22);
    const int import_errors[]={ENOTSUP,ENOMEM};
    for (unsigned i=0; i<sizeof(import_errors)/sizeof(import_errors[0]); i++) {
        unsigned before=coherent_imports;
        fail_coherent=import_errors[i];
        CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb)); ERROR(import_errors[i]);
        CHECK(coherent_imports==before+1 && imports==0 && live_imports==0);
        CHECK(!peek(&pool) && peek(&device)->refs==0 && !peek(&device)->retired);
    }
    /* Import succeeded, then core object publication failed. */
    unsigned before=destroys; fail_publish=1;
    CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb)); ERROR(ENOMEM);
    CHECK(!fail_publish && destroys==before+1 && live_imports==0);
    CHECK(!peek(&pool) && peek(&device)->refs==0 && imports==0);
    /* Rollback destruction itself fails: retain the imported BO in the
     * existing device retire owner; DeviceFinalize must safely reap it. */
    CHECK(api_nvnDeviceInitialize(&failed_device,&db)); CLEAN();
    api_nvnMemoryPoolBuilderSetDevice(&pb,&failed_device); CLEAN();
    fail_publish=1; fail_destroy=EAGAIN;
    CHECK(!api_nvnMemoryPoolInitialize(&pool,&pb)); ERROR(ENOMEM);
    CHECK(!fail_publish && !peek(&pool) && live_imports==1);
    CHECK(peek(&failed_device)->retired && peek(&failed_device)->refs==1);
    api_nvnDeviceFinalize(&failed_device); CLEAN();
    CHECK(!peek(&failed_device) && live_imports==0);
    /* Failed initialization did not consume the public pool key. */
    api_nvnMemoryPoolBuilderSetDevice(&pb,&device);
    CHECK(api_nvnMemoryPoolInitialize(&pool,&pb)); CLEAN();
    CHECK(map(&pool)==storage && imports==0 && caches==0); CLEAN();
    api_nvnMemoryPoolFinalize(&pool); CLEAN();
    api_nvnDeviceFinalize(&device); CLEAN(); CHECK(live_imports==0);
    for (size_t n=0; n<4096; n++) CHECK(((unsigned char *)storage)[n]==0xa7);
    while (objects) { CHECK(objects->kind==DB || objects->kind==PB || objects->kind==BB); remove_object(objects); }
    free(storage);
    printf("{\"passed\":true,\"scope\":\"host mock flags/core ownership only; no GPU or coherent hardware proof\",\"checks\":%u,\"accepted_flags\":%u,\"rejected_flags\":%u,\"coherent_import_calls\":%u,\"fallback_import_calls\":%u,\"cache_calls\":%u,\"destroy_calls\":%u,\"live_imports\":%u}\n",
           checks,accepted_flags,rejected_flags,coherent_imports,imports,caches,destroys,live_imports);
    return 0;
}
