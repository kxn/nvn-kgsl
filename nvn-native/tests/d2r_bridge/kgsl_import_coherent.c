/* Actual kgsl.c + mocked ioctl/close/open. HOST ONLY: never opens a device,
 * executes GPU work, or establishes physical CPU/GPU coherence. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "nvn-native/src/kgsl.c"

static unsigned checks, cases, total_imports, total_infos, total_frees, total_closes, total_caches;
static unsigned poison_owners_released_by_test;
static void *storage;
static const int mock_fd=424242;
static struct {
    unsigned imports, infos, frees, shared_frees, caches, closes, opens, stats;
    int import_error, info_error, info_eintr, free_failures, shared_free_failures, close_error;
    uint64_t request_clear, request_set, info_clear, info_set;
    uint32_t import_id, info_id;
    uint64_t gpuaddr, bytes;
    int coherent, dmabuf, stat_error;
    uint64_t fd_bytes;
} mock;
static uint64_t expected_flags(void)
{ return mock.dmabuf ? 0 : (KGSL_CACHEMODE_WRITEBACK<<KGSL_CACHEMODE_SHIFT) | (mock.coherent?KGSL_MEMFLAGS_IOCOHERENT:0); }
static void reset_mock(void)
{
    memset(&mock,0,sizeof(mock)); mock.import_id=17; mock.info_id=17;
    mock.gpuaddr=0x800000; mock.bytes=4096; mock.fd_bytes=4096; mock.coherent=1; cases++;
}
static nn_device *new_device(void)
{
    nn_device *device=calloc(1,sizeof(*device)); assert(device);
    device->fd=mock_fd; device->chip=0x06050002; device->page=4096;
    return device;
}
int __wrap_open(const char *name,int flags,...)
{ (void)name;(void)flags;mock.opens++;abort(); }
int __wrap_close(int fd)
{
    assert(fd==mock_fd); mock.closes++;total_closes++;
    if(mock.close_error){errno=mock.close_error;return -1;}return 0;
}
int __wrap_fstat(int fd,struct stat *info)
{
    assert(fd==12345); mock.stats++;
    if(mock.stat_error){errno=mock.stat_error;return -1;}
    memset(info,0,sizeof(*info));info->st_size=(off_t)mock.fd_bytes;return 0;
}
int __wrap_ioctl(int fd,unsigned long operation,...)
{
    assert(fd==mock_fd);va_list args;va_start(args,operation);void *argument=va_arg(args,void *);va_end(args);
    if(operation==IOCTL_KGSL_GPUOBJ_IMPORT){
        mock.imports++;total_imports++;
        struct kgsl_gpuobj_import *request=argument;
        assert(request->type==(mock.dmabuf?KGSL_USER_MEM_TYPE_DMABUF:KGSL_USER_MEM_TYPE_ADDR));
        assert(request->priv_len==(mock.dmabuf?sizeof(struct kgsl_gpuobj_import_dma_buf):4096));
        assert(request->flags==expected_flags()&&request->id==0);
        if(mock.dmabuf) {
            const struct kgsl_gpuobj_import_dma_buf *buffer=(const void *)(uintptr_t)request->priv;
            assert(buffer->fd==12345);
        } else {
        const struct kgsl_gpuobj_import_useraddr *user=(const void *)(uintptr_t)request->priv;
        uintptr_t address=(uintptr_t)storage;
#if defined(__aarch64__)
        address&=UINT64_C(0x00ffffffffffffff);
#endif
        assert(user->virtaddr==address);
        }
        if(mock.import_error){errno=mock.import_error;return -1;}
        request->id=mock.import_id;
        request->flags=(request->flags&~mock.request_clear)|mock.request_set;
        return 0;
    }
    if(operation==IOCTL_KGSL_GPUMEM_GET_INFO){
        mock.infos++;total_infos++;
        struct kgsl_gpumem_get_info *info=argument;assert(info->id==mock.import_id);
        if(mock.info_eintr){mock.info_eintr--;errno=EINTR;return -1;}
        if(mock.info_error){errno=mock.info_error;return -1;}
        info->id=mock.info_id;info->gpuaddr=mock.gpuaddr;info->size=mock.bytes;
        info->flags=(expected_flags()&~mock.info_clear)|mock.info_set;
        return 0;
    }
    if(operation==IOCTL_KGSL_GPUOBJ_FREE){
        mock.frees++;total_frees++;
        const struct kgsl_gpuobj_free *release=argument;assert(release->id==17);
        if(mock.free_failures){mock.free_failures--;errno=EAGAIN;return -1;}return 0;
    }
    if(operation==IOCTL_KGSL_SHAREDMEM_FREE){
        mock.shared_frees++;const struct kgsl_sharedmem_free *release=argument;
        assert(release->gpuaddr==0x800000);
        if(mock.shared_free_failures){mock.shared_free_failures--;errno=EAGAIN;return -1;}return 0;
    }
    if(operation==IOCTL_KGSL_GPUMEM_SYNC_CACHE){
        mock.caches++;total_caches++;const struct kgsl_gpumem_sync_cache *request=argument;
        assert(!mock.coherent&&request->id==17&&request->offset==64&&request->length==128);
        assert(request->op==(KGSL_GPUMEM_CACHE_RANGE|(mock.caches==1?KGSL_GPUMEM_CACHE_TO_GPU:KGSL_GPUMEM_CACHE_FROM_GPU)));
        return 0;
    }
    /* Includes fallback MAP_USER_MEM, context creation and GPU_COMMAND. */
    fprintf(stderr,"Unexpected mocked ioctl %lx\n",operation);abort();
}
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)

static int validation_and_success(void)
{
    reset_mock();nn_device *device=new_device();nn_bo *bo=(void *)1;
    CHECK(nn_bo_import_coherent(device,storage,4096,NULL)==EINVAL);
    CHECK(nn_bo_import_coherent(NULL,storage,4096,&bo)==EINVAL&&bo==NULL);
    CHECK(nn_bo_import_coherent(device,NULL,4096,&bo)==EINVAL&&bo==NULL);
    CHECK(nn_bo_import_coherent(device,storage,0,&bo)==EINVAL&&bo==NULL);
    CHECK(nn_bo_import_coherent(device,(char *)storage+1,4096,&bo)==EINVAL&&bo==NULL);
    CHECK(nn_bo_import_coherent(device,storage,4095,&bo)==EINVAL&&bo==NULL);
    device->chip=0x43030b00;
    CHECK(nn_bo_import_coherent(device,storage,4096,&bo)==ENOTSUP&&bo==NULL);
    CHECK(mock.imports==0&&mock.infos==0&&device->bos==0);device->chip=0x06050002;
    CHECK(nn_bo_import_coherent(device,storage,4096,&bo)==0&&bo);
    CHECK(mock.imports==1&&mock.infos==1&&device->bos==1&&!device->failed_imports);
    CHECK(nn_bo_map(bo)==storage&&nn_bo_size(bo)==4096&&nn_bo_address(bo)==0x800000);
    CHECK(nn_bo_flags(bo)==expected_flags());
    CHECK(nn_bo_cache(bo,0,4096,0)==0);CHECK(nn_bo_cache(bo,0,4096,1)==0);
    CHECK(nn_bo_cache(bo,4096,0,0)==0);CHECK(nn_bo_cache(bo,4097,0,0)==EINVAL);
    CHECK(nn_bo_cache(bo,4096,1,1)==EINVAL);CHECK(nn_bo_cache(bo,1,SIZE_MAX,0)==EINVAL);
    CHECK(mock.caches==0&&mock.imports==1);
    bo->busy=1;CHECK(nn_bo_destroy(bo)==EBUSY&&mock.shared_frees==0);
    CHECK(nn_device_close(device)==EBUSY&&mock.closes==0);bo->busy=0;
    mock.shared_free_failures=1;
    CHECK(nn_bo_destroy(bo)==EAGAIN&&mock.shared_frees==1&&device->bos==1);
    CHECK(nn_bo_map(bo)==storage);
    CHECK(nn_bo_destroy(bo)==0&&mock.shared_frees==2&&device->bos==0);
    CHECK(nn_device_close(device)==0&&mock.closes==1&&mock.opens==0);
    return 0;
}
static int returned_metadata_rejections(void)
{
    for(unsigned variant=0;variant<14;variant++){
        reset_mock();nn_device *device=new_device();nn_bo *bo=(void *)1;
        switch(variant){
        case 0:mock.request_clear=KGSL_MEMFLAGS_IOCOHERENT;break;
        case 1:mock.info_clear=KGSL_MEMFLAGS_IOCOHERENT;break;
        case 2:mock.request_clear=KGSL_CACHEMODE_MASK;mock.request_set=KGSL_CACHEMODE_WRITECOMBINE<<KGSL_CACHEMODE_SHIFT;break;
        case 3:mock.info_clear=KGSL_CACHEMODE_MASK;mock.info_set=KGSL_CACHEMODE_WRITECOMBINE<<KGSL_CACHEMODE_SHIFT;break;
        case 4:mock.request_set=KGSL_MEMFLAGS_GPUREADONLY;break;
        case 5:mock.info_set=KGSL_MEMFLAGS_GPUREADONLY;break;
        case 6:mock.request_set=KGSL_MEMFLAGS_SECURE;break;
        case 7:mock.info_set=KGSL_MEMFLAGS_SECURE;break;
        case 8:mock.bytes=8192;break;
        case 9:mock.bytes=4095;break;
        case 10:mock.info_id=18;break;
        case 11:mock.gpuaddr=0;break;
        case 12:mock.gpuaddr=0x800001;break;
        case 13:mock.import_id=0;break;
        }
        CHECK(nn_bo_import_coherent(device,storage,4096,&bo)==ENOTSUP&&bo==NULL);
        CHECK(mock.imports==1&&mock.infos==1&&mock.caches==0);
        if(variant==13){
            CHECK(mock.frees==0&&device->bos==1&&device->failed_imports&&device->failed_imports->id==0);
        }else CHECK(mock.frees==1&&device->bos==0&&!device->failed_imports);
        CHECK(nn_device_close(device)==0&&mock.closes==1);
    }
    return 0;
}
static int failed_info_owner(void)
{
    reset_mock();nn_device *device=new_device();nn_bo *bo=NULL;
    mock.info_error=EIO;mock.free_failures=2;
    CHECK(nn_bo_import_coherent(device,storage,4096,&bo)==EIO&&bo==NULL);
    CHECK(mock.imports==1&&mock.infos==1&&mock.frees==1);
    CHECK(device->bos==1&&device->failed_imports&&device->failed_imports->id==17);
    CHECK(device->failed_imports->imported==3&&device->failed_imports->dev==device);
    CHECK(nn_device_close(device)==EAGAIN&&mock.closes==0);
    CHECK(device->bos==1&&device->failed_imports&&mock.frees==2&&mock.imports==1);
    CHECK(nn_device_close(device)==0&&mock.frees==3&&mock.closes==1);
    /* Metadata qualification failure has exactly the same retained owner. */
    reset_mock();device=new_device();mock.info_clear=KGSL_MEMFLAGS_IOCOHERENT;mock.free_failures=1;
    CHECK(nn_bo_import_coherent(device,storage,4096,&bo)==ENOTSUP&&bo==NULL);
    CHECK(device->bos==1&&device->failed_imports&&mock.frees==1);
    CHECK(nn_device_close(device)==0&&mock.frees==2&&mock.closes==1);
    /* GET_INFO is read-only and may retry EINTR; IMPORT may not. */
    reset_mock();device=new_device();mock.info_eintr=1;
    CHECK(nn_bo_import_coherent(device,storage,4096,&bo)==0&&bo);
    CHECK(mock.imports==1&&mock.infos==2);
    CHECK(nn_bo_destroy(bo)==0);CHECK(nn_device_close(device)==0);
    return 0;
}
static int uncertain_import_owner(void)
{
    const int errors[]={EIO,EINTR,ENOMEM};
    for(unsigned i=0;i<sizeof(errors)/sizeof(errors[0]);i++){
        reset_mock();nn_device *device=new_device();nn_bo *bo=(void *)1;mock.import_error=errors[i];
        CHECK(nn_bo_import_coherent(device,storage,4096,&bo)==errors[i]&&bo==NULL);
        CHECK(mock.imports==1&&mock.infos==0&&mock.frees==0);
        CHECK(device->bos==1&&device->failed_imports&&device->failed_imports->id==0);
        CHECK(nn_device_close(device)==0&&mock.closes==1&&mock.frees==0);
    }
    reset_mock();nn_device *device=new_device();nn_bo *ordinary=NULL,*failed=NULL;
    CHECK(nn_bo_import_coherent(device,storage,4096,&ordinary)==0&&ordinary);
    mock.import_error=EIO;
    CHECK(nn_bo_import_coherent(device,storage,4096,&failed)==EIO&&!failed);
    CHECK(device->bos==2&&device->failed_imports&&mock.imports==2);
    CHECK(nn_device_close(device)==EBUSY&&mock.closes==0&&mock.frees==0);
    CHECK(nn_bo_destroy(ordinary)==0&&device->bos==1);
    device->queues=1;CHECK(nn_device_close(device)==EBUSY&&mock.closes==0);
    device->queues=0;CHECK(nn_device_close(device)==0&&mock.closes==1);
    /* Both known and unidentified failed imports are owned by whole-fd close. */
    reset_mock();device=new_device();mock.info_error=EIO;mock.free_failures=1;
    CHECK(nn_bo_import_coherent(device,storage,4096,&failed)==EIO&&!failed);
    mock.import_error=EIO;
    CHECK(nn_bo_import_coherent(device,storage,4096,&failed)==EIO&&!failed);
    CHECK(device->bos==2&&device->failed_imports&&device->failed_imports->failed_next);
    CHECK(nn_device_close(device)==0&&mock.closes==1&&mock.frees==1);
    return 0;
}
static void release_poisoned_test_owner(nn_device *device)
{
    /* Production intentionally retains this poisoned owner. Only the synthetic
     * test frees its fixture after proving repeated close does not touch fd. */
    while(device->failed_imports){nn_bo *bo=device->failed_imports;device->failed_imports=bo->failed_next;free(bo);}
    free(device);poison_owners_released_by_test++;
}
static int close_failure_keeps_owner(void)
{
    for(unsigned unidentified=0;unidentified<2;unidentified++){
        reset_mock();nn_device *device=new_device();nn_bo *bo=NULL;
        if(unidentified){mock.import_error=EIO;CHECK(nn_bo_import_coherent(device,storage,4096,&bo)==EIO&&!bo);}
        mock.close_error=EIO;
        CHECK(nn_device_close(device)==EIO&&mock.closes==1);
        CHECK(device->close_error==EIO&&device->fd==-1);
        CHECK(nn_device_close(device)==EIO&&mock.closes==1);
        CHECK(nn_device_close(device)==EIO&&mock.closes==1);
        CHECK((device->failed_imports!=NULL)==(unidentified!=0));
        CHECK(device->bos==unidentified);
        release_poisoned_test_owner(device);
    }
    return 0;
}
static int cached_import_regression(void)
{
    reset_mock();mock.coherent=0;nn_device *device=new_device();nn_bo *bo=NULL;
    CHECK(nn_bo_import(device,storage,4096,&bo)==0&&bo);
    CHECK(nn_bo_map(bo)==storage&&nn_bo_flags(bo)==expected_flags());
    CHECK(nn_bo_cache(bo,64,128,0)==0&&mock.caches==1);
    CHECK(nn_bo_cache(bo,64,128,1)==0&&mock.caches==2);
    CHECK(nn_bo_cache(bo,4096,0,0)==0&&mock.caches==2);
    CHECK(nn_bo_cache(bo,4096,1,0)==EINVAL&&mock.caches==2);
    CHECK(nn_bo_destroy(bo)==0);CHECK(nn_device_close(device)==0);
    return 0;
}
static int dmabuf_ownership(void)
{
    reset_mock();mock.dmabuf=1;nn_device *device=new_device();nn_bo *bo=(void *)1;
    CHECK(nn_bo_import_dmabuf(device,12345,4096,NULL)==EINVAL);
    CHECK(nn_bo_import_dmabuf(NULL,12345,4096,&bo)==EINVAL&&!bo);
    CHECK(nn_bo_import_dmabuf(device,-1,4096,&bo)==EINVAL&&!bo);
    CHECK(nn_bo_import_dmabuf(device,12345,4095,&bo)==EINVAL&&!bo);
    device->chip=0x43030b00;CHECK(nn_bo_import_dmabuf(device,12345,4096,&bo)==ENOTSUP&&!bo);
    device->chip=0x06050002;mock.fd_bytes=8192;
    CHECK(nn_bo_import_dmabuf(device,12345,4096,&bo)==EINVAL&&!bo&&mock.imports==0);
    mock.fd_bytes=4096;mock.stat_error=EBADF;
    CHECK(nn_bo_import_dmabuf(device,12345,4096,&bo)==EBADF&&!bo&&mock.imports==0);
    mock.stat_error=0;mock.info_set=KGSL_CACHEMODE_WRITECOMBINE<<KGSL_CACHEMODE_SHIFT;
    CHECK(!nn_bo_import_dmabuf(device,12345,4096,&bo)&&bo);
    CHECK(!nn_bo_map(bo)&&nn_bo_size(bo)==4096&&nn_bo_flags(bo)==mock.info_set);
    CHECK(nn_bo_cache(bo,0,4096,0)==ENOTSUP&&mock.caches==0);
    CHECK(nn_bo_cache(bo,4096,1,0)==EINVAL);
    mock.shared_free_failures=1;CHECK(nn_bo_destroy(bo)==EAGAIN&&device->bos==1);
    CHECK(nn_device_close(device)==EBUSY&&mock.closes==0);
    CHECK(!nn_bo_destroy(bo)&&!nn_device_close(device)&&mock.closes==1);
    for(unsigned variant=0;variant<9;variant++){
        reset_mock();mock.dmabuf=1;device=new_device();bo=(void *)1;
        switch(variant){
        case 0:mock.request_set=KGSL_MEMFLAGS_GPUREADONLY;break;
        case 1:mock.info_set=KGSL_MEMFLAGS_GPUREADONLY;break;
        case 2:mock.request_set=KGSL_MEMFLAGS_SECURE;break;
        case 3:mock.info_set=KGSL_MEMFLAGS_SECURE;break;
        case 4:mock.bytes=8192;break;
        case 5:mock.gpuaddr=0x800001;break;
        case 6:mock.info_id=18;break;
        case 7:mock.import_id=0;break;
        case 8:mock.info_error=EIO;mock.free_failures=2;break;
        }
        CHECK(nn_bo_import_dmabuf(device,12345,4096,&bo)==(variant==8?EIO:ENOTSUP)&&!bo);
        CHECK(mock.imports==1&&mock.infos==1&&mock.caches==0);
        if(variant==7)CHECK(device->bos==1&&device->failed_imports&&!mock.frees);
        else if(variant==8){
            CHECK(device->bos==1&&device->failed_imports&&device->failed_imports->id==17);
            CHECK(nn_device_close(device)==EAGAIN&&mock.frees==2&&mock.closes==0);
        }else CHECK(!device->bos&&!device->failed_imports&&mock.frees==1);
        CHECK(!nn_device_close(device)&&mock.closes==1&&mock.imports==1);
    }
    reset_mock();mock.dmabuf=1;device=new_device();mock.import_error=EINTR;
    CHECK(nn_bo_import_dmabuf(device,12345,4096,&bo)==EINTR&&!bo);
    CHECK(mock.imports==1&&!mock.infos&&!mock.frees&&device->failed_imports&&device->bos==1);
    CHECK(!nn_device_close(device)&&mock.closes==1);
    return 0;
}
int main(void)
{
    CHECK(!posix_memalign(&storage,4096,4096));memset(storage,0xa7,4096);
    CHECK(!validation_and_success());CHECK(!returned_metadata_rejections());
    CHECK(!failed_info_owner());CHECK(!uncertain_import_owner());
    CHECK(!close_failure_keeps_owner());CHECK(!cached_import_regression());
    CHECK(!dmabuf_ownership());
    for(size_t i=0;i<4096;i++)CHECK(((unsigned char *)storage)[i]==0xa7);
    free(storage);
    printf("{\"passed\":true,\"scope\":\"actual kgsl.c mocked ioctl/close only; no device/GPU/game or physical coherency proof\",\"checks\":%u,\"cases\":%u,\"import_ioctl_calls\":%u,\"get_info_ioctl_calls\":%u,\"free_id_ioctl_calls\":%u,\"close_calls\":%u,\"cache_ioctl_calls\":%u,\"poisoned_fixture_owners_manually_released\":%u}\n",checks,cases,total_imports,total_infos,total_frees,total_closes,total_caches,poison_owners_released_by_test);
    return 0;
}
