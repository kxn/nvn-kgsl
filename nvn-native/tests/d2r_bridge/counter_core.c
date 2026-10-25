/* Actual candidate NVN recorder/submit/materializer + RAM ownership backend.
 * No PM4 execution, no samples/pixels emulation and no GPU clock emulation. */
#define main original_resource_main
#include NN_COUNTER_RESOURCE_FIXTURE
#undef main
#include NN_COUNTER_IMAGE
static int make_queue(NVNqueue *q,NVNqueueBuilder *b)
{api_nvnQueueBuilderSetDefaults(b);api_nvnQueueBuilderSetDevice(b,&device);return api_nvnQueueInitialize(q,b);}
static uint64_t submitted_child_state(void)
{
    uint64_t address=pair_address(observed+1);const uint32_t *w=(const void *)(uintptr_t)address;
    return pair_address(w+2);
}
static NVNcommandHandle record_query(int reset,NVNbufferAddress address)
{
    begin();if(reset)api_nvnCommandBufferResetCounter(&command,NVN_COUNTER_TYPE_SAMPLES_PASSED);
    else api_nvnCommandBufferReportCounter(&command,NVN_COUNTER_TYPE_SAMPLES_PASSED,address);
    return api_nvnCommandBufferEndRecording(&command);
}
int main(void)
{
    CHECK(!posix_memalign(&storage,4096,65536));memset(storage,0xa5,65536);
    api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&device,&db));
    api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&device);api_nvnMemoryPoolBuilderSetStorage(&pb,storage,65536);api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);CHECK(api_nvnMemoryPoolInitialize(&pool,&pb));
    void *arena_memory;CHECK(!posix_memalign(&arena_memory,4096,65536));NVNmemoryPool arena;NVNmemoryPoolBuilder ab;
    api_nvnMemoryPoolBuilderSetDefaults(&ab);api_nvnMemoryPoolBuilderSetDevice(&ab,&device);api_nvnMemoryPoolBuilderSetStorage(&ab,arena_memory,65536);api_nvnMemoryPoolBuilderSetFlags(&ab,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);CHECK(api_nvnMemoryPoolInitialize(&arena,&ab));
    CHECK(api_nvnCommandBufferInitialize(&command,&device));api_nvnCommandBufferAddCommandMemory(&command,&arena,0,65536);api_nvnCommandBufferAddControlMemory(&command,control,sizeof(control));GOOD();
    NVNqueue q1,q2;NVNqueueBuilder qb1,qb2;CHECK(make_queue(&q1,&qb1));CHECK(make_queue(&q2,&qb2));GOOD();
    CHECK(nvnBootstrapLoader("nvnCommandBufferResetCounter"));CHECK(nvnBootstrapLoader("nvnCommandBufferReportCounter"));CHECK(nvnBootstrapLoader("nvnDeviceGetTimestampInNanoseconds"));
    uint64_t address=api_nvnMemoryPoolGetBufferAddress(&pool);NVNcommandHandle reset=record_query(1,0),report=record_query(0,address);CHECK(reset&&report);GOOD();
    struct handle *reset_handle=handles;while(reset_handle->id!=reset)reset_handle=reset_handle->next;
    CHECK(!reset_handle->count&&reset_handle->counter_ops&&!reset_handle->counter_ops->destination);
    struct object *one=find(&q1,QUEUE),*two=find(&q2,QUEUE);unsigned before=submits,bos=find(&device,DEVICE)->gpu->bos;
    api_nvnQueueSubmitCommands(&q1,1,&report);BAD(ENODATA);CHECK(submits==before&&!one->counter&&find(&device,DEVICE)->gpu->bos==bos);
    api_nvnQueueSubmitCommands(&q1,1,&reset);GOOD();CHECK(one->counter&&one->counter->valid&&!one->counter->poisoned);CHECK(submitted_child_state()==nn_bo_address(one->counter->bo));
    CHECK(observed_words==reset_handle->bytes/4+8);
    CHECK(observed[0]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
    CHECK(observed[4]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
    CHECK(observed[observed_words-4]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
    const uint32_t *reset_words=(const void *)(uintptr_t)pair_address(observed+5);
    CHECK(reset_words[0]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,4)&&pair_address(reset_words+1)==nn_bo_address(one->counter->bo)+32);
    const uint32_t *original=(const uint32_t *)((const char *)nn_bo_map(reset_handle->commands->bo)+reset_handle->offset);CHECK(!(original[0]|original[1]|original[2]|original[3]));
    CHECK(nn_bo_idle(one->counter->bo)==EBUSY);api_nvnQueueFinalize(&q1);BAD(EBUSY);
    api_nvnQueueSubmitCommands(&q1,1,&report);GOOD();CHECK(submitted_child_state()==nn_bo_address(one->counter->bo));
    const uint32_t *report_words=(const void *)(uintptr_t)pair_address(observed+5);
    CHECK(pair_address(report_words+2)==nn_bo_address(one->counter->bo)+16);
    CHECK(report_words[21]==nvn_pm4_packet7(NVN_CP_MEM_TO_MEM,9));
    CHECK(pair_address(report_words+25)==nn_bo_address(one->counter->bo)+32);
    CHECK(((unsigned char *)storage)[0]==0xa5); /* no CPU result approximation */
    api_nvnQueueFinish(&q1);GOOD();CHECK(!nn_bo_idle(one->counter->bo));
    api_nvnQueueSubmitCommands(&q2,1,&report);BAD(ENODATA);CHECK(!two->counter);
    NVNcommandHandle both[]={reset,report};api_nvnQueueSubmitCommands(&q2,2,both);GOOD();CHECK(two->counter&&two->counter->valid&&two->counter->bo!=one->counter->bo);CHECK(submitted_child_state()==nn_bo_address(two->counter->bo));api_nvnQueueFinish(&q2);GOOD();
    api_nvnQueueSubmitCommands(&q1,1,&report);GOOD();CHECK(submitted_child_state()==nn_bo_address(one->counter->bo));api_nvnQueueFinish(&q1);GOOD();
    /* Private query BO/job bounds must fit the real caller control workspace
     * before even the first handle of a batch may submit. */
    before=submits;size_t control_bytes=one->queue_config.control;one->queue_config.control=sizeof(nn_bo *);
    api_nvnQueueSubmitCommands(&q1,2,both);BAD(ENOSPC);CHECK(submits==before);one->queue_config.control=control_bytes;
    /* Even a queryless submission after Reset brackets its commands with GPU
     * start/stop accumulation; active counting survives handle boundaries. */
    begin();uint32_t marker=0x12345678;api_nvnCommandBufferUpdateUniformBuffer(&command,address+256,256,0,4,&marker);GOOD();
    NVNcommandHandle plain=api_nvnCommandBufferEndRecording(&command);CHECK(plain);
    api_nvnQueueSubmitCommands(&q1,1,&plain);GOOD();
    CHECK(submitted_child_state()==nn_bo_address(one->counter->bo));
    CHECK(observed[observed_words-4]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
    CHECK(nn_bo_idle(one->counter->bo)==EBUSY);api_nvnQueueFinish(&q1);GOOD();
    /* Unknown/stale handles and invalid second handle preflight atomically. */
    before=submits;NVNcommandHandle invalid[]={reset,UINT64_MAX};api_nvnQueueSubmitCommands(&q1,2,invalid);BAD(EINVAL);CHECK(submits==before);
    begin();struct recording *r=find(&command,COMMAND)->record;size_t used=r->used;
    api_nvnCommandBufferReportCounter(&command,NVN_COUNTER_TYPE_SAMPLES_PASSED,address+8);BAD(EINVAL);CHECK(r->used==used&&!r->current->counter_ops);
    api_nvnCommandBufferReportCounter(&command,NVN_COUNTER_TYPE_SAMPLES_PASSED,nn_bo_address(find(&arena,POOL)->bo));BAD(ENOTSUP);CHECK(r->used==used);
    api_nvnCommandBufferResetCounter(&command,NVN_COUNTER_TYPE_INPUT_VERTICES);BAD(ENOTSUP);CHECK(r->used==used);
    api_nvnCommandBufferReportCounter(&command,NVN_COUNTER_TYPE_FRAGMENT_SHADER_INVOCATIONS,address);BAD(ENOTSUP);CHECK(r->used==used);
    enum nn_nvn_gpu_pool_policy prior=find(&pool,POOL)->gpu_policy;find(&pool,POOL)->gpu_policy=NN_NVN_GPU_UNCACHED_UNQUALIFIED;api_nvnCommandBufferReportCounter(&command,NVN_COUNTER_TYPE_SAMPLES_PASSED,address);BAD(ENOTSUP);find(&pool,POOL)->gpu_policy=prior;CHECK(r->used==used);
    api_nvnCommandBufferReportCounter(&command,NVN_COUNTER_TYPE_TIMESTAMP,address+16);GOOD();api_nvnCommandBufferReportCounter(&command,NVN_COUNTER_TYPE_TIMESTAMP_TOP,address+32);GOOD();NVNcommandHandle clocks=api_nvnCommandBufferEndRecording(&command);CHECK(clocks);api_nvnQueueSubmitCommands(&q1,1,&clocks);GOOD();api_nvnQueueFinish(&q1);GOOD();
    NVNcounterData ticks={.counter=999,.timestamp=19200000};CHECK(api_nvnDeviceGetTimestampInNanoseconds(&device,&ticks)==1000000000);GOOD();api_nvnDeviceGetTimestampInNanoseconds(&device,NULL);BAD(EINVAL);ticks.timestamp=UINT64_MAX;CHECK(!api_nvnDeviceGetTimestampInNanoseconds(&device,&ticks));BAD(EOVERFLOW);
    CHECK(!counter_image_safe(&NVN_GRAPHICS_IMAGE));struct nn_gfx_image bad=NVN_GRAPHICS_IMAGE;uint32_t dangerous[]={nvn_pm4_packet4(0x8891,1),1};bad.commands=dangerous;bad.command_words=2;CHECK(counter_image_safe(&bad)==ENOTSUP);
    /* Metadata deep clone is independent of source handle lifetime and does
     * not borrow the source queue's private baseline or child query jobs. */
    struct handle clone={0};CHECK(!counter_clone_metadata(&clone,reset_handle,128));CHECK(clone.counter_ops&&clone.counter_ops!=reset_handle->counter_ops&&clone.counter_ops->command_offset==128);
    counter_free_ops(clone.counter_ops);
    /* Safe pre-submit failure doesn't publish/reset a baseline. */
    NVNqueue q3;NVNqueueBuilder qb3;CHECK(make_queue(&q3,&qb3));struct object *three=find(&q3,QUEUE);before=submits;fail_alloc=1;api_nvnQueueSubmitCommands(&q3,1,&reset);BAD(ENOMEM);CHECK(submits==before&&(!three->counter||!three->counter->valid));fail_cache=1;api_nvnQueueSubmitCommands(&q3,1,&reset);BAD(EIO);CHECK(submits==before&&three->counter&&!three->counter->valid&&!three->counter->poisoned);
    api_nvnQueueSubmitCommands(&q3,1,&report);BAD(ENODATA);CHECK(submits==before);
    api_nvnQueueFinalize(&q3);GOOD();
    /* Unknown submit retains query state, original output pool, private IBs and
     * queue; fail-stop in a child rather than hide ownership with leak skips. */
    pid_t pid=fork();CHECK(pid>=0);if(!pid){fail_submit=1;api_nvnQueueSubmitCommands(&q1,1,&report);assert(nn_nvn_error()==EDEADLK&&one->counter->poisoned&&nn_bo_idle(one->counter->bo)==EBUSY);nn_nvn_clear_error();unsigned attempted=submits;api_nvnQueueSubmitCommands(&q1,1,&reset);assert(nn_nvn_error()==EBUSY&&submits==attempted);nn_nvn_clear_error();api_nvnQueueFinalize(&q1);assert(nn_nvn_error()==EBUSY);_exit(0);}int status;CHECK(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&!WEXITSTATUS(status));
    pid=fork();CHECK(pid>=0);if(!pid){api_nvnQueueSubmitCommands(&q1,1,&report);assert(!nn_nvn_error());fail_finish=1;api_nvnQueueFinish(&q1);assert(nn_nvn_error()==ETIMEDOUT&&one->counter->poisoned&&nn_bo_idle(one->counter->bo)==EBUSY);nn_nvn_clear_error();unsigned attempted=submits;api_nvnQueueSubmitCommands(&q1,1,&reset);assert(nn_nvn_error()==EBUSY&&submits==attempted);_exit(0);}CHECK(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&!WEXITSTATUS(status));
    /* Native queue destruction can complete before its final private BO free
     * fails. Keep the staged owner, block every further queue operation and do
     * not call through the freed native queue pointer. */
    pid=fork();CHECK(pid>=0);if(!pid){fail_destroy=1;api_nvnQueueFinalize(&q1);assert(nn_nvn_error()==EIO&&one->queue==NULL&&one->counter);nn_nvn_clear_error();api_nvnQueueFinish(&q1);assert(nn_nvn_error()==EBUSY);nn_nvn_clear_error();api_nvnQueueFinalize(&q1);assert(nn_nvn_error()==EBUSY&&one->counter);_exit(0);}CHECK(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&!WEXITSTATUS(status));
    api_nvnDeviceFinalizeCommandHandle(&device,plain);api_nvnDeviceFinalizeCommandHandle(&device,clocks);api_nvnDeviceFinalizeCommandHandle(&device,reset);api_nvnDeviceFinalizeCommandHandle(&device,report);GOOD();
    api_nvnQueueFinalize(&q1);api_nvnQueueFinalize(&q2);api_nvnCommandBufferFinalize(&command);api_nvnMemoryPoolFinalize(&pool);api_nvnMemoryPoolFinalize(&arena);api_nvnDeviceFinalize(&device);GOOD();
    while(objects){CHECK(objects->kind==DB||objects->kind==PB||objects->kind==QB);remove_object(objects);}CHECK(!handles);free(storage);free(arena_memory);
    printf("{\"passed\":true,\"checks\":%u,\"gpu_executes\":0,\"scope\":\"HOST native recorder/materializer ownership only\",\"context_isolation_proven\":false}\n",checks);return 0;
}
