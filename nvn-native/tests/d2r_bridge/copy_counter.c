/* HOST ONLY actual Copy + Counter recorders/materializers, RAM timestamps.
 * Native PM4 is inspected but not executed. No query values/GPU qualification. */
#include NN_COPY_BASELINE
static unsigned counter_ops_count(const struct handle*h)
{unsigned n=0;for(struct counter_op*op=h->counter_ops;op;op=op->next)n++;return n;}
static void copy_counter_replay(void)
{
    NVNdeviceBuilder db={0};NVNdevice dk={0};NVNqueue q={0};NVNqueueBuilder qb={0};
    OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object*dev=find(&dk,DEVICE);
    queue(&dk,&q,&qb,262144);
    unsigned char keys[3];struct object*arena0=pool(dev,&keys[0],4096),*arena1=pool(dev,&keys[1],4096),*output=pool(dev,&keys[2],4096);
    NVNcommandBuffer src={0},dst={0};_Alignas(16) unsigned char controls[2][256];
    begin(&dk,&src,arena0,controls[0],256);
    PFNNVNCOMMANDBUFFERRESETCOUNTERPROC reset=(PFNNVNCOMMANDBUFFERRESETCOUNTERPROC)nvnBootstrapLoader("nvnCommandBufferResetCounter");
    PFNNVNCOMMANDBUFFERREPORTCOUNTERPROC report=(PFNNVNCOMMANDBUFFERREPORTCOUNTERPROC)nvnBootstrapLoader("nvnCommandBufferReportCounter");
    T(reset&&report);
    OK(reset(&src,NVN_COUNTER_TYPE_SAMPLES_PASSED));
    OK(report(&src,NVN_COUNTER_TYPE_TIMESTAMP,nn_bo_address(output->bo)+16));
    OK(report(&src,NVN_COUNTER_TYPE_SAMPLES_PASSED,nn_bo_address(output->bo)+32));
    NVNcommandHandle a=api_nvnCommandBufferEndRecording(&src);T(a);struct handle*source=lookup(a);
    T(counter_ops_count(source)==3&&output->refs==1&&!source->jobs);
    begin(&dk,&dst,arena1,controls[1],256);OK(api_nvnCommandBufferInsertDebugMarkerDynamic(&dst,3,"copy prefix"));
    struct recording*r=find(&dst,COMMAND)->record;size_t prefix=r->used;
    T(prefix&&prefix%4==0);size_t before=r->used,refs=output->refs;
    deny_malloc_size=sizeof(struct counter_op);ERR(copy_commands(&dst,1,&a),ENOMEM);
    T(!deny_malloc_size&&r->used==before&&output->refs==refs&&!r->current->counter_ops&&!r->current->count);
    OK(copy_commands(&dst,1,&a));copies++;
    T(counter_ops_count(r->current)==3&&output->refs==2);
    const struct counter_op*original=source->counter_ops,*clone=r->current->counter_ops;
    for(unsigned i=0;i<3;i++,original=original->next,clone=clone->next){
        T(original&&clone&&clone!=original);
        T(clone->command_offset==prefix+16*i&&clone->operation==original->operation);
        T(clone->destination==original->destination&&clone->destination_offset==original->destination_offset);
    }
    T(!original&&!clone);
    source->counter_ops->next->destination_offset=2032; /* independent snapshot */
    memset(nn_bo_map(arena0->bo),0xcd,arena0->size);
    OK(api_nvnDeviceFinalizeCommandHandle(&dk,a));OK(api_nvnCommandBufferFinalize(&src));
    OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)arena0->key));
    T(output->refs==1&&r->current->counter_ops->next->destination_offset==16);
    NVNcommandHandle b=api_nvnCommandBufferEndRecording(&dst);T(b);
    struct handle*target=lookup(b);T(counter_ops_count(target)==3);
    /* Abort an active copy, without creating a destination handle: deferred
     * operations and their output-resource pins must not leak. */
    unsigned char abort_key;struct object*abort_arena=pool(dev,&abort_key,4096);
    NVNcommandBuffer aborted={0};_Alignas(16) unsigned char abort_control[128];
    begin(&dk,&aborted,abort_arena,abort_control,sizeof(abort_control));
    OK(copy_commands(&aborted,1,&b));copies++;
    struct handle*aborting=find(&aborted,COMMAND)->record->current;
    T(counter_ops_count(aborting)==3&&aborting->counter_ops!=target->counter_ops&&output->refs==2);
    OK(api_nvnCommandBufferFinalize(&aborted));T(output->refs==1);
    OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)abort_arena->key));
    for(unsigned replay=0;replay<2;replay++){
        OK(api_nvnQueueSubmitCommands(&q,1,&b));T(nn_test_last_has(output->bo));
        uint32_t*parent=nn_bo_map(nn_test_last_parent());T(parent);
        /* Actual counter parent:16B BEGIN prefix + immutable logical stream
         * +16B END suffix; logical metadata remains unchanged across replay. */
        T(parent[0]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
        for(const struct counter_op*op=target->counter_ops;op;op=op->next){
            size_t at=(16+op->command_offset)/4;
            T(parent[at]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
            T((parent[at+1]||parent[at+2])&&parent[at+3]);
            T(op->command_offset>=prefix&&op->command_offset<prefix+48);
        }
        size_t suffix=(16+target->bytes)/4;T(parent[suffix]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
    }
    nn_bo*output_bo=output->bo;
    ERR(api_nvnMemoryPoolFinalize((NVNmemoryPool*)output->key),EBUSY);
    OK(api_nvnDeviceFinalizeCommandHandle(&dk,b));T(!output->refs);
    OK(api_nvnCommandBufferFinalize(&dst));OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)arena1->key));
    ERR(api_nvnMemoryPoolFinalize((NVNmemoryPool*)output->key),EBUSY);
    T(nn_test_last_has(output_bo));OK(api_nvnQueueFinish(&q));
    OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)output->key));
    OK(api_nvnQueueFinalize(&q));cleanup_device(&dk);
}
int main(void)
{
    T(copy_baseline_main()==0);unsigned before=checks;
    copy_counter_replay();
    printf("{\"passed\":true,\"scope\":\"HOST ONLY final Copy+Counter; RAM timeline, PM4 not executed\",\"checks\":%u,\"counter_copy_checks\":%u,\"retired_abort_cases\":%u,\"gpu_submits\":0,\"game_modules_executed\":false}\n",checks,checks-before,retired_abort_cases);
    return 0;
}
