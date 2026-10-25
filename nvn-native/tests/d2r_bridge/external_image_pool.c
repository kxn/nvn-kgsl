/* Pure host lifetime/shape tests. Existing memory_backend never executes GPU.
 * Fake import supplies ordinary aligned test memory, NOT a qualified dma-buf. */
#define nn_bo_import_dmabuf host_unused_dmabuf_boundary
#include "memory_backend.c"
#undef nn_bo_import_dmabuf
#include "../../src/nvn.c"
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
static unsigned checks, import_calls, destroy_calls, destroys, owner_releases;
static int import_error, destroy_error;
static nn_bo *last_import;
/* fd7 is the existing synthetic import fixture ONLY. All other descriptors
 * retain real fstat semantics (including real failure errno). */
static int fake_stat_mode;
int __real_fstat(int,struct stat *);
int __wrap_fstat(int fd,struct stat *out)
{
    if(fd!=7)return __real_fstat(fd,out);
    if(fake_stat_mode==1){errno=EIO;return -1;}
    memset(out,0,sizeof(*out));out->st_size=fake_stat_mode==2?32769:32768;
    out->st_ino=fake_stat_mode==3?0:7007;out->st_dev=makedev(1,7);return 0;
}
int nn_bo_import_dmabuf(nn_device *d,int fd,size_t bytes,nn_bo **out)
{
    ++import_calls;*out=NULL;if(import_error)return import_error;
    if(fd<0||bytes%4096)return EINVAL;
    int e=nn_bo_alloc(d,bytes,out);last_import=*out;return e;
}
int __real_nn_bo_destroy(nn_bo *);
int __wrap_nn_bo_destroy(nn_bo *b)
{
    ++destroy_calls;if(destroy_error)return destroy_error;
    int e=__real_nn_bo_destroy(b);if(!e)++destroys;return e;
}
struct owner { unsigned calls, before; int failure; };
static int release_owner(void *pointer)
{
    struct owner *owner=pointer;
    assert(destroys>owner->before); /* BO before allocator, even on callback failure. */
    ++owner->calls;++owner_releases;return owner->failure;
}
static struct nn_external_image_desc descriptor(struct owner *owner)
{
    owner->before=destroys;
    return (struct nn_external_image_desc){7,32768,4096,16384,64,64,256,1,NN_EXTERNAL_RGBA8,819,owner,owner,release_owner};
}
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d %s err=%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define ERROR(x) do{CHECK(nn_nvn_error()==(x));nn_nvn_clear_error();}while(0)
static int module_cases(void)
{
    nn_device *d=NULL;CHECK(!nn_device_open(&d));
    struct owner o={0};struct nn_external_image_desc desc=descriptor(&o);nn_external_image *image=NULL;
    unsigned before=import_calls;
#define BAD(change) do{struct nn_external_image_desc bad=desc;change;image=(void *)1;CHECK(nn_external_image_import(d,&bad,&image)!=0);CHECK(!image&&import_calls==before&&!o.calls);}while(0)
    BAD(bad.fd=-1);BAD(bad.owner_cookie=NULL);BAD(bad.release_owner=NULL);BAD(bad.display_buffer=NULL);
    BAD(bad.width=128);BAD(bad.height=0);BAD(bad.pitch=252);BAD(bad.layers=2);BAD(bad.format=99);BAD(bad.usage=0);
    BAD(bad.fd_bytes=0);BAD(bad.fd_bytes=32767);BAD(bad.fd_bytes=1048577);BAD(bad.pixel_offset=1);BAD(bad.pixel_offset=SIZE_MAX);BAD(bad.payload_bytes=16385);
#undef BAD
    for(fake_stat_mode=1;fake_stat_mode<=3;fake_stat_mode++){
        image=(void *)1;
        CHECK(nn_external_image_import(d,&desc,&image)==(fake_stat_mode==1?EIO:ENOTSUP));
        CHECK(!image&&import_calls==before&&!o.calls);
    }
    fake_stat_mode=0;
    import_error=EIO;CHECK(nn_external_image_import(d,&desc,&image)==EIO&&!image&&!o.calls);import_error=0;
    CHECK(!nn_external_image_import(d,&desc,&image)&&image);struct nn_external_image_view view;
    CHECK(!nn_external_image_view(image,&view)&&view.pixel_offset==4096&&view.payload_bytes==16384&&
        view.backing_inode==7007&&view.backing_dev_major==1&&view.backing_dev_minor==7);
    CHECK(nn_external_image_claim_texture(image,0,16384)==EINVAL);
    CHECK(!nn_external_image_claim_texture(image,4096,16384));CHECK(nn_external_image_claim_texture(image,4096,16384)==EBUSY);
    CHECK(nn_external_image_destroy(image)==EBUSY&&!o.calls);CHECK(!nn_external_image_pin(image));
    CHECK(!nn_external_image_release_texture(image));CHECK(nn_external_image_claim_texture(image,4096,16384)==EBUSY);
    CHECK(nn_external_image_destroy(image)==EBUSY);CHECK(!nn_external_image_unpin(image));CHECK(nn_external_image_unpin(image)==EINVAL);
    last_import->busy=1;CHECK(nn_external_image_destroy(image)==EBUSY&&!o.calls);last_import->busy=0;
    CHECK(!nn_external_image_destroy(image)&&o.calls==1);CHECK(!nn_device_close(d));
    return 0;
}
static void texture_builder(NVNtextureBuilder *b,NVNdevice *d,NVNmemoryPool *p,ptrdiff_t offset)
{
    api_nvnTextureBuilderSetDefaults(b);api_nvnTextureBuilderSetDevice(b,d);
    api_nvnTextureBuilderSetFlags(b,NVN_TEXTURE_FLAGS_LINEAR);api_nvnTextureBuilderSetTarget(b,NVN_TEXTURE_TARGET_2D);
    api_nvnTextureBuilderSetSize2D(b,64,64);api_nvnTextureBuilderSetLevels(b,1);api_nvnTextureBuilderSetSamples(b,0);
    api_nvnTextureBuilderSetFormat(b,NVN_FORMAT_RGBA8);api_nvnTextureBuilderSetStride(b,256);
    api_nvnTextureBuilderSetStorage(b,p,offset);
}
static int core_cases(unsigned count)
{
    NVNdeviceBuilder builder_key={0};NVNdevice d={0};api_nvnDeviceBuilderSetDefaults(&builder_key);CHECK(api_nvnDeviceInitialize(&d,&builder_key));
    NVNmemoryPool pools[3];NVNtextureBuilder builders[3];NVNtexture textures[3];struct owner owners[3]={0};
    for(unsigned i=0;i<count;i++){
        struct nn_external_image_desc desc=descriptor(owners+i);CHECK(!nn_nvn_import_image_pool(&d,pools+i,&desc));
        unsigned calls=import_calls;CHECK(nn_nvn_import_image_pool(&d,pools+i,&desc)==EEXIST&&import_calls==calls);
        CHECK(api_nvnMemoryPoolMap(pools+i)==NULL);ERROR(ENOTSUP);
        api_nvnMemoryPoolFlushMappedRange(pools+i,0,0);ERROR(ENOTSUP);
        texture_builder(builders+i,&d,pools+i,4096);CHECK(nn_nvn_error()==0);CHECK(api_nvnTextureInitialize(textures+i,builders+i));
        CHECK(find(textures+i,TEXTURE)->pool->key==pools+i&&find(textures+i,TEXTURE)->offset==4096);
        struct nn_external_image_view view;CHECK(!nn_nvn_image_pool_view(pools+i,&view));
        CHECK(api_nvnTextureGetTextureAddress(textures+i)==nn_bo_address(view.bo)+4096);
        CHECK(address_pool(find(&d,DEVICE),api_nvnTextureGetTextureAddress(textures+i),64)==NULL);
        NVNtexturePool descriptor_key={0};struct object *descriptors=create(&descriptor_key,TPOOL);CHECK(descriptors);
        api_nvnTexturePoolRegisterTexture(&descriptor_key,256,textures+i,NULL);ERROR(ENOTSUP);remove_object(descriptors);
        NVNcommandBuffer raw={0};CHECK(api_nvnCommandBufferInitialize(&raw,&d));
        /* A genuine recording context isolates rejection of the excluded
         * external target. Private active/capacity edits without Begin/current
         * no longer model a legal recording after semantic Call ownership. */
        void *arena=NULL;CHECK(!posix_memalign(&arena,4096,4096));
        NVNmemoryPoolBuilder arena_builder={0};NVNmemoryPool arena_pool={0};
        api_nvnMemoryPoolBuilderSetDefaults(&arena_builder);api_nvnMemoryPoolBuilderSetDevice(&arena_builder,&d);
        api_nvnMemoryPoolBuilderSetFlags(&arena_builder,0x24);api_nvnMemoryPoolBuilderSetStorage(&arena_builder,arena,4096);
        CHECK(api_nvnMemoryPoolInitialize(&arena_pool,&arena_builder));
        _Alignas(struct control) unsigned char control[256];
        api_nvnCommandBufferAddCommandMemory(&raw,&arena_pool,0,4096);api_nvnCommandBufferAddControlMemory(&raw,control,sizeof(control));
        api_nvnCommandBufferBeginRecording(&raw);CHECK(!nn_nvn_error());
        api_nvnCommandBufferClearBuffer(&raw,api_nvnTextureGetTextureAddress(textures+i),4,0);ERROR(EINVAL);
        api_nvnCommandBufferFinalize(&raw);api_nvnMemoryPoolFinalize(&arena_pool);CHECK(!nn_nvn_error());free(arena);
        NVNtexture duplicate;CHECK(!api_nvnTextureInitialize(&duplicate,builders+i));ERROR(EBUSY);
        api_nvnMemoryPoolFinalize(pools+i);ERROR(EBUSY);CHECK(!owners[i].calls);
        NVNbufferBuilder bb;api_nvnBufferBuilderSetDefaults(&bb);api_nvnBufferBuilderSetDevice(&bb,&d);api_nvnBufferBuilderSetStorage(&bb,pools+i,4096,64);ERROR(ENOTSUP);
        NVNcommandBuffer commands;CHECK(api_nvnCommandBufferInitialize(&commands,&d));api_nvnCommandBufferAddCommandMemory(&commands,pools+i,4096,64);ERROR(ENOTSUP);api_nvnCommandBufferFinalize(&commands);
        CHECK(!nn_nvn_image_pool_pin(pools+i));api_nvnTextureFinalize(textures+i);CHECK(nn_nvn_error()==0);
        api_nvnMemoryPoolFinalize(pools+i);ERROR(EBUSY);CHECK(!owners[i].calls);
    }
    for(unsigned i=0;i<count;i++){
        CHECK(!nn_nvn_image_pool_unpin(pools+i));api_nvnMemoryPoolFinalize(pools+i);CHECK(nn_nvn_error()==0&&owners[i].calls==1);
    }
    api_nvnDeviceFinalize(&d);CHECK(nn_nvn_error()==0);return 0;
}
static int ordinary_case(void)
{
    NVNdeviceBuilder db={0};NVNdevice d={0};NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool={0};
    void *storage=NULL;CHECK(!posix_memalign(&storage,4096,16384));
    api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&d,&db));
    api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&d);
    api_nvnMemoryPoolBuilderSetStorage(&pb,storage,16384);
    api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    CHECK(api_nvnMemoryPoolInitialize(&pool,&pb));CHECK(api_nvnMemoryPoolMap(&pool)==storage);
    struct nn_external_image_view view;memset(&view,255,sizeof(view));
    CHECK(nn_nvn_image_pool_view(&pool,&view)==ENOTSUP&&!view.bo);
    CHECK(address_pool(find(&d,DEVICE),api_nvnMemoryPoolGetBufferAddress(&pool),64)==find(&pool,POOL));
    api_nvnMemoryPoolFlushMappedRange(&pool,0,64);CHECK(!nn_nvn_error());
    struct owner owner={0};struct nn_external_image_desc desc=descriptor(&owner);unsigned calls=import_calls;
    CHECK(nn_nvn_import_image_pool(&d,&pool,&desc)==EEXIST&&import_calls==calls&&!owner.calls);
    api_nvnMemoryPoolFinalize(&pool);api_nvnDeviceFinalize(&d);CHECK(!nn_nvn_error());free(storage);return 0;
}
static int quarantine_cases(void)
{
    nn_device *d=NULL;CHECK(!nn_device_open(&d));
    struct owner *o=calloc(1,sizeof(*o));struct nn_external_image_desc desc=descriptor(o);nn_external_image *image=NULL;
    CHECK(!nn_external_image_import(d,&desc,&image));destroy_error=EIO;unsigned calls=destroy_calls;
    CHECK(nn_external_image_destroy(image)==EIO&&!o->calls);destroy_error=0;
    CHECK(nn_external_image_destroy(image)==EBUSY&&destroy_calls==calls+1&&!o->calls&&!nn_external_image_live(image));
    /* Deliberately retain this fake BO/owner/module to process exit, matching
     * quarantine policy. No GPU work was possible in this memory backend. */
    struct owner *o2=calloc(1,sizeof(*o2));o2->failure=EBUSY;desc=descriptor(o2);nn_external_image *second=NULL;
    CHECK(!nn_external_image_import(d,&desc,&second));CHECK(nn_external_image_destroy(second)==EBUSY&&o2->calls==1);
    calls=destroy_calls;CHECK(nn_external_image_destroy(second)==EBUSY&&o2->calls==1&&destroy_calls==calls);
    struct nn_external_image_view view;CHECK(nn_external_image_view(second,&view)==EBUSY&&!view.bo);
    return 0;
}
static int isolated_quarantine(void)
{
    int channel[2];if(pipe(channel))return 1;
    pid_t child=fork();if(child<0)return 1;
    if(!child){
        close(channel[0]);if(quarantine_cases())_Exit(1);
        unsigned counts[2]={checks,owner_releases};
        if(write(channel[1],counts,sizeof(counts))!=(ssize_t)sizeof(counts))_Exit(1);
        /* These intentionally poisoned fixture owners live to process exit.
         * Normal owner paths still undergo the parent's full leak check. */
        _Exit(0);
    }
    close(channel[1]);unsigned counts[2];ssize_t bytes=read(channel[0],counts,sizeof(counts));
    close(channel[0]);int status=0;CHECK(waitpid(child,&status,0)==child);
    CHECK(bytes==(ssize_t)sizeof(counts)&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    checks=counts[0]+2;owner_releases=counts[1];return 0;
}
int main(void)
{
    if(module_cases()||core_cases(2)||core_cases(3)||ordinary_case()||isolated_quarantine())return 1;
    printf("{\"scope\":\"host ownership only; no dma-buf/GPU qualification\",\"checks\":%u,\"two_and_three_pools\":true,\"owner_releases\":%u,\"quarantines_retained\":2,\"gpu_submits\":0}\n",checks,owner_releases);
    return 0;
}
