/* Actual core pure-CPU handle/ABI tests; RAM ownership backend, no GPU work. */
#ifndef NN_NVN_MEMORY_BACKEND_SOURCE
#define NN_NVN_MEMORY_BACKEND_SOURCE "memory_backend.c"
#endif
#ifndef NN_NVN_CORE_SOURCE
#define NN_NVN_CORE_SOURCE "../../src/nvn.c"
#endif
#include NN_NVN_MEMORY_BACKEND_SOURCE
#include NN_NVN_CORE_SOURCE
#include <limits.h>
_Static_assert(sizeof(NVNimageHandle)==8,"image handle uses uint64 ABI");
_Static_assert(_Generic((PFNNVNDEVICEGETIMAGEHANDLEPROC)0,
    NVNimageHandle (*)(const NVNdevice *,int):1,default:0),"typed original GetImageHandle ABI");
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
static unsigned object_count(void){unsigned n=0;for(struct object *o=objects;o;o=o->next)++n;return n;}
int main(void)
{
    PFNNVNDEVICEGETIMAGEHANDLEPROC get=(PFNNVNDEVICEGETIMAGEHANDLEPROC)nvnBootstrapLoader("nvnDeviceGetImageHandle");
    PFNNVNDEVICEINITIALIZEPROC init=(PFNNVNDEVICEINITIALIZEPROC)nvnBootstrapLoader("nvnDeviceInitialize");
    PFNNVNDEVICEBUILDERSETDEFAULTSPROC defaults=(PFNNVNDEVICEBUILDERSETDEFAULTSPROC)nvnBootstrapLoader("nvnDeviceBuilderSetDefaults");
    PFNNVNDEVICEFINALIZEPROC fini=(PFNNVNDEVICEFINALIZEPROC)nvnBootstrapLoader("nvnDeviceFinalize");
    CHECK(get&&init&&defaults&&fini);
    NVNdevice device,never_initialized;NVNdeviceBuilder builder_key;
    defaults(&builder_key);CHECK(!nn_nvn_error());CHECK(init(&device,&builder_key));CHECK(!nn_nvn_error());
    struct object *owner=find(&device,DEVICE);CHECK(owner&&!owner->refs&&!owner->gpu->bos);
    unsigned count=object_count();
    CHECK((PFNNVNDEVICEGETIMAGEHANDLEPROC)api_nvnDeviceGetProcAddress(&device,"nvnDeviceGetImageHandle")==get);
    /* Intentionally no TexturePool, SamplerPool, Texture, descriptor or queue. */
    const int valid[]={256,257,0x12345,0xabcde,0xffffe,0xfffff};
    for(unsigned i=0;i<sizeof(valid)/sizeof(*valid);i++){
        struct {uint64_t before;NVNimageHandle value;uint64_t after;} guarded={UINT64_C(0x76543210fedcba98),UINT64_MAX,UINT64_C(0x89abcdef01234567)};
        guarded.value=get(&device,valid[i]);
        CHECK(guarded.value==(uint64_t)valid[i]);CHECK(!(guarded.value>>32));
        CHECK(guarded.before==UINT64_C(0x76543210fedcba98)&&guarded.after==UINT64_C(0x89abcdef01234567));
        CHECK(!nn_nvn_error());CHECK(object_count()==count&&!owner->refs&&!owner->gpu->bos);
    }
    const int invalid[]={INT_MIN,-1,0,1,255,0x100000,0x100001,0x200100,INT_MAX};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++){
        CHECK(get(&device,invalid[i])==0);CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
        CHECK(object_count()==count&&!owner->refs&&!owner->gpu->bos);
    }
    CHECK(get(NULL,256)==0);CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
    CHECK(get(&never_initialized,256)==0);CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
    CHECK(get((const NVNdevice *)&builder_key,256)==0);CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
    api_nvnDeviceSetDepthMode(&device,(NVNdepthMode)99);CHECK(nn_nvn_error()==ENOTSUP);
    CHECK(get(&device,0xfffff)==UINT64_C(0xfffff));CHECK(nn_nvn_error()==ENOTSUP);
    CHECK(get(&device,-1)==0);CHECK(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();
    CHECK(nvnBootstrapLoader("nvnCommandBufferBindImage")==NULL);
    CHECK(nvnBootstrapLoader("nvnCommandBufferBindImages")==NULL);
    CHECK(nvnBootstrapLoader("nvnTexturePoolRegisterImage")==NULL);CHECK(!nn_nvn_error());
    int supported=0x12345678;
    api_nvnDeviceGetInteger(&device,NVN_DEVICE_INFO_SUPPORTS_IMAGES_USING_TEXTURE_HANDLES,&supported);
    CHECK(nn_nvn_error()==ENOTSUP);CHECK(supported==0x12345678);nn_nvn_clear_error();
    CHECK(object_count()==count&&!owner->refs&&!owner->gpu->bos);
    fini(&device);CHECK(!nn_nvn_error());
    CHECK(get(&device,0xfffff)==0);CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
    CHECK(init(&device,&builder_key));CHECK(get(&device,256)==256);CHECK(!nn_nvn_error());
    fini(&device);CHECK(!nn_nvn_error());
    /* Builder has no public Finalize; release fixture metadata for leak test. */
    while(objects){CHECK(objects->kind==DB);remove_object(objects);}
    CHECK(!handles);
    printf("{\"passed\":true,\"checks\":%u,\"valid_ids\":6,\"invalid_ids\":9,\"no_descriptor_pool_required\":true,\"typed_uint64_abi\":true,\"gpu_image_support\":false,\"gpu_executes\":0}\n",checks);
    return 0;
}
