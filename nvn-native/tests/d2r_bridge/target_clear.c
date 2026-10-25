/* HOST packet/state/ref inspection only, never GPU execution. */
#ifndef NN_RESOURCE_FAMILY_FIXTURE
#define NN_RESOURCE_FAMILY_FIXTURE "resource_copy.c"
#endif
#define main resource_family_main
#include NN_RESOURCE_FAMILY_FIXTURE
#undef main
int main(void)
{
    CHECK(!resource_family_main());checks=0;
    CHECK(!posix_memalign(&storage,4096,65536));memset(storage,0x6d,65536);
    api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&device,&db));
    api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&device);api_nvnMemoryPoolBuilderSetStorage(&pb,storage,65536);api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);CHECK(api_nvnMemoryPoolInitialize(&pool,&pb));
    CHECK(texture(&source,0,NVN_FORMAT_RGBA8,64,64,256));CHECK(texture(&target,16384,NVN_FORMAT_RGBA8,64,64,256));
    void *arena_memory;CHECK(!posix_memalign(&arena_memory,4096,65536));NVNmemoryPool arena;NVNmemoryPoolBuilder ab;
    api_nvnMemoryPoolBuilderSetDefaults(&ab);api_nvnMemoryPoolBuilderSetDevice(&ab,&device);api_nvnMemoryPoolBuilderSetStorage(&ab,arena_memory,65536);api_nvnMemoryPoolBuilderSetFlags(&ab,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);CHECK(api_nvnMemoryPoolInitialize(&arena,&ab));
    CHECK(api_nvnCommandBufferInitialize(&command,&device));api_nvnCommandBufferAddCommandMemory(&command,&arena,0,65536);api_nvnCommandBufferAddControlMemory(&command,control,sizeof(control));GOOD();
    CHECK(nvnBootstrapLoader("nvnCommandBufferClearColor"));CHECK(nvnBootstrapLoader("nvnCommandBufferClearColorui"));CHECK(nvnBootstrapLoader("nvnCommandBufferClearColori"));CHECK(nvnBootstrapLoader("nvnCommandBufferClearDepthStencil"));
    float color[4]={0.25f,0.5f,0.75f,1};uint32_t u[4]={0,127,255,17};int i[4]={-200,-1,128,0};
    begin();struct recording *r=find(&command,COMMAND)->record;size_t used=r->used;
    api_nvnCommandBufferClearColor(&command,0,color,15);BAD(ENOTSUP);CHECK(r->used==used);
    api_nvnCommandBufferClearColor(&command,0,color,0);GOOD();CHECK(r->used==used);
    api_nvnCommandBufferClearDepthStencil(&command,0.5f,0,0,0);GOOD();CHECK(r->used==used);
    api_nvnCommandBufferClearDepthStencil(&command,0.5f,1,0,0);BAD(ENOTSUP);CHECK(r->used==used);
    api_nvnCommandBufferClearDepthStencil(&command,0.5f,0,1,255);BAD(ENOTSUP);CHECK(r->used==used);
    api_nvnCommandBufferClearDepthStencil(&command,2,1,0,0);BAD(EINVAL);
    api_nvnCommandBufferClearDepthStencil(&command,0.5f,2,0,0);BAD(EINVAL);
    api_nvnCommandBufferClearDepthStencil(&command,0.5f,0,0,256);BAD(EINVAL);
    const NVNtexture *attachments[]={&target};api_nvnCommandBufferSetRenderTargets(&command,1,attachments,NULL,NULL,NULL);GOOD();
    api_nvnCommandBufferClearColor(&command,0,color,15);BAD(ENOTSUP); /* no known scissor */
    api_nvnCommandBufferSetScissor(&command,0,0,64,64);GOOD();
    api_nvnCommandBufferClearColor(&command,0,color,15);GOOD();uint32_t *w=child();
    CHECK(w[27]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,66));CHECK(w[30]==0xffbf8040u);CHECK(pair_address(w+28)==api_nvnTextureGetTextureAddress(&target));
    CHECK(find(&target,TEXTURE)->refs==3); /* binding + immutable Call state + recorded clear */
    attachments[0]=&source;api_nvnCommandBufferSetRenderTargets(&command,1,attachments,NULL,NULL,NULL);GOOD();CHECK(find(&target,TEXTURE)->refs==2);
    CHECK(pair_address(w+28)==api_nvnTextureGetTextureAddress(&target)); /* later bind doesn't retarget old clear */
    used=r->used;api_nvnCommandBufferClearColor(&command,0,color,1);BAD(ENOTSUP);CHECK(r->used==used);
    api_nvnCommandBufferClearColor(&command,1,color,15);BAD(ENOTSUP);CHECK(r->used==used);
    api_nvnCommandBufferClearColor(&command,-1,color,15);BAD(EINVAL);CHECK(r->used==used);
    api_nvnCommandBufferClearColor(&command,0,color,16);BAD(EINVAL);CHECK(r->used==used);
    api_nvnCommandBufferClearColorui(&command,0,u,15);BAD(ENOTSUP);CHECK(r->used==used);
    api_nvnCommandBufferClearColori(&command,0,i,15);BAD(ENOTSUP);CHECK(r->used==used);
    discard();GOOD();CHECK(!find(&target,TEXTURE)->refs);
    /* Current root SetRenderTargets allows explicit LINEAR clear-only integer
     * attachments; Draw retains its independent compiled-image format gate. */
    NVNtexture unsigned_target,signed_target;
    CHECK(texture(&unsigned_target,32768,NVN_FORMAT_RGBA8UI,64,64,256));
    CHECK(texture(&signed_target,49152,NVN_FORMAT_RGBA8I,64,64,256));
    begin();attachments[0]=&unsigned_target;api_nvnCommandBufferSetRenderTargets(&command,1,attachments,NULL,NULL,NULL);GOOD();
    u[0]=UINT32_MAX;api_nvnCommandBufferClearColorui(&command,0,u,15);GOOD();w=child();CHECK(w[30]==0x11ff7fffu);CHECK(pair_address(w+28)==api_nvnTextureGetTextureAddress(&unsigned_target));
    attachments[0]=&signed_target;api_nvnCommandBufferSetRenderTargets(&command,1,attachments,NULL,NULL,NULL);GOOD();api_nvnCommandBufferClearColori(&command,0,i,15);GOOD();w=child();CHECK(w[30]==0x007fff80u);CHECK(pair_address(w+28)==api_nvnTextureGetTextureAddress(&signed_target));discard();GOOD();
    attachments[0]=&source;begin();api_nvnCommandBufferSetRenderTargets(&command,1,attachments,NULL,NULL,NULL);api_nvnCommandBufferClearColor(&command,0,color,15);discard();GOOD();
    api_nvnTextureFinalize(&unsigned_target);api_nvnTextureFinalize(&signed_target);GOOD();
    uint32_t words[4];unsigned count;
    CHECK(!signed_clear_pattern(NVN_FORMAT_RGBA8I,i,words,&count));CHECK(count==1&&words[0]==0x007fff80u);
    CHECK(!signed_clear_pattern(NVN_FORMAT_R32I,i,words,&count));CHECK(count==1&&words[0]==(uint32_t)-200);
    CHECK(!signed_clear_pattern(NVN_FORMAT_RG32I,i,words,&count));CHECK(count==2&&words[1]==UINT32_MAX);
    CHECK(!signed_clear_pattern(NVN_FORMAT_RGBA32I,i,words,&count));CHECK(count==4&&words[2]==128);
    CHECK(signed_clear_pattern(NVN_FORMAT_RGBA8,i,words,&count)==ENOTSUP);
    CHECK(signed_clear_pattern(NVN_FORMAT_RGBA8I,NULL,words,&count)==EINVAL);
    for(unsigned j=0;j<32768;j++)CHECK(((unsigned char *)storage)[j]==0x6d);
    api_nvnCommandBufferFinalize(&command);api_nvnTextureFinalize(&source);api_nvnTextureFinalize(&target);api_nvnMemoryPoolFinalize(&pool);api_nvnMemoryPoolFinalize(&arena);api_nvnDeviceFinalize(&device);GOOD();
    while(objects){CHECK(objects->kind==DB||objects->kind==PB||objects->kind==TB);remove_object(objects);}CHECK(!handles);free(storage);free(arena_memory);
    printf("{\"passed\":true,\"checks\":%u,\"clear_apis\":4,\"gpu_executes\":0,\"integer_attachment_host_checked\":true,\"depth_attachment_supported\":false}\n",checks);return 0;
}
