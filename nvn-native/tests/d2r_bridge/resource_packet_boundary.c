#ifndef NN_RESOURCE_ORIGINAL_FIXTURE
#define NN_RESOURCE_ORIGINAL_FIXTURE "resource_copy.c"
#endif
#define main original_resource_main
#include NN_RESOURCE_ORIGINAL_FIXTURE
#undef main
/* HOST packet inspection only. The RAM backend never executes CP or shaders. */
static int validate_job(nn_bo *bo,size_t bytes,size_t expected_data,size_t expected_writes,size_t expected_moves)
{
    const uint32_t *w=nn_bo_map(bo);size_t at=0,data=0,writes=0,moves=0;
    while(at<bytes/4){
        uint32_t h=w[at],size=nvn_pm4_packet_words(h);CHECK(size&&size<=bytes/4-at);
        if((h>>28)==7){unsigned op=(h>>16)&0x7f,n=h&0x3fff;
            CHECK(n<=0x3fff);
            if(op==NVN_CP_MEM_WRITE){CHECK(n>=3);data+=n-2;writes++;}
            if(op==NVN_CP_MEM_TO_MEM){CHECK(n==5);moves++;}
        }
        at+=size;
    }
    CHECK(at==bytes/4);CHECK(data==expected_data&&writes==expected_writes&&moves==expected_moves);
    CHECK(nn_bo_size(bo)>=bytes+4);return 0;
}
int main(void)
{
    CHECK(!posix_memalign(&storage,4096,2097152));memset(storage,0xa5,2097152);
    api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&device,&db));
    api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&device);api_nvnMemoryPoolBuilderSetStorage(&pb,storage,2097152);api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);CHECK(api_nvnMemoryPoolInitialize(&pool,&pb));
    struct object *device_object=find(&device,DEVICE),*p=find(&pool,POOL);
    uint64_t base=api_nvnMemoryPoolGetBufferAddress(&pool);struct resource_span s={p,NULL,0,262144,262144,1},d={p,NULL,1048576,262144,262144,1};
    nn_bo *bo=NULL;size_t bytes=0;CHECK(!spans_disjoint(&s,&d));CHECK(!copy_job_encode(device_object,&s,&d,NULL,0,0,&bo,&bytes));CHECK(bytes==(32+65536*6)*4);CHECK(!validate_job(bo,bytes,0,0,65536));CHECK(!nn_bo_destroy(bo));
    s.row_bytes=d.row_bytes=262148;unsigned bos=device_object->gpu->bos;CHECK(copy_job_encode(device_object,&s,&d,NULL,0,0,&bo,&bytes)==E2BIG);CHECK(!bo&&bytes==0&&device_object->gpu->bos==bos);
    /* All writes respect 14-bit payload, including address words. Test all
     * transition boundaries and maximum 256KiB wide clear. */
    uint32_t value=0x10203040;
    const size_t data_words[]={1,16380,16381,16382,16384,32762,32763,65524,65525,65536};
    for(unsigned i=0;i<sizeof(data_words)/sizeof(*data_words);i++){
        size_t n=data_words[i];d.row_bytes=d.pitch=n*4;d.rows=1;
        CHECK(!copy_job_encode(device_object,NULL,&d,&value,1,0,&bo,&bytes));
        CHECK(bytes==(32+n+3*((n+16380)/16381))*4);
        CHECK(!validate_job(bo,bytes,n,(n+16380)/16381,0));
        const uint32_t *w=nn_bo_map(bo);CHECK(w[30]==value);CHECK(!nn_bo_destroy(bo));
    }
    /* Maximum legal UniformBuffer update is a real public NVN recording, not
     * a direct-helper-only fixture. Caller data changes after record. */
    void *command_storage;CHECK(!posix_memalign(&command_storage,4096,65536));NVNmemoryPool arena;NVNmemoryPoolBuilder ab;
    api_nvnMemoryPoolBuilderSetDefaults(&ab);api_nvnMemoryPoolBuilderSetDevice(&ab,&device);api_nvnMemoryPoolBuilderSetStorage(&ab,command_storage,65536);api_nvnMemoryPoolBuilderSetFlags(&ab,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);CHECK(api_nvnMemoryPoolInitialize(&arena,&ab));CHECK(api_nvnCommandBufferInitialize(&command,&device));api_nvnCommandBufferAddCommandMemory(&command,&arena,0,65536);api_nvnCommandBufferAddControlMemory(&command,control,sizeof(control));
    uint32_t *data=malloc(65536);CHECK(data);for(unsigned i=0;i<16384;i++)data[i]=0x12340000u+i;
    begin();api_nvnCommandBufferUpdateUniformBuffer(&command,base+1048576,65536,0,65536,data);GOOD();memset(data,0,65536);
    struct job *job=find(&command,COMMAND)->record->current->jobs;CHECK(job&&job->bo);
    const uint32_t *w=nn_bo_map(job->bo);size_t words=32+16384+6;CHECK(!validate_job(job->bo,words*4,16384,2,0));CHECK(w[30]==0x12340000u);CHECK(w[27+3+16380]==0x12343ffcu);
    size_t second=27+3+16381;CHECK(w[second]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,5));CHECK(w[second+3]==0x12343ffdu&&w[second+5]==0x12343fffu);
    discard();GOOD();
    begin();size_t used=find(&command,COMMAND)->record->used;api_nvnCommandBufferUpdateUniformBuffer(&command,base+1048576,65540,0,65540,data);BAD(EINVAL);CHECK(find(&command,COMMAND)->record->used==used);api_nvnCommandBufferUpdateUniformBuffer(&command,base+1048576,65536,4,65536,data);BAD(EINVAL);CHECK(find(&command,COMMAND)->record->used==used);
    api_nvnCommandBufferUpdateUniformBuffer(&command,base+1048576,65536,0,4,data);GOOD();discard();GOOD();
    api_nvnCommandBufferFinalize(&command);api_nvnMemoryPoolFinalize(&arena);api_nvnMemoryPoolFinalize(&pool);api_nvnDeviceFinalize(&device);GOOD();
    while(objects){CHECK(objects->kind==DB||objects->kind==PB);remove_object(objects);}free(storage);free(command_storage);free(data);CHECK(!handles);
    printf("{\"passed\":true,\"checks\":%u,\"wide_write_sizes\":10,\"max_copy_bytes\":262144,\"max_update_bytes\":65536,\"packet_payload_bits\":14,\"gpu_executes\":0}\n",checks);return 0;
}
