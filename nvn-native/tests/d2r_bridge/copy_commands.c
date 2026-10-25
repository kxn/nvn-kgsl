/* HOST ONLY actual NVN Copy recorder/materializer + deterministic RAM timeline.
 * Reuse the repository's existing binding fixture and verified ignored shader
 * header. Its whitebox Program/bind setup does not test source registration or
 * execute a GPU shader. No original shader bytes are embedded in this source. */
#define main device_modes_baseline_main
#include NN_DEVICE_MODES_FIXTURE
#undef main
#include "timeline_backend.h"
static PFNNVNCOMMANDBUFFERCOPYCOMMANDSPROC copy_commands;
static unsigned copies, model_unknowns, retired_abort_cases;
static volatile size_t deny_malloc_size;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t bytes)
{if(deny_malloc_size&&bytes==deny_malloc_size){deny_malloc_size=0;return NULL;}return __real_malloc(bytes);}
static struct handle*lookup(NVNcommandHandle id)
{struct handle*h=handles;while(h&&h->id!=id)h=h->next;return h;}
static unsigned borrowed(const struct handle*h)
{unsigned n=0;for(struct job_borrow*b=h->borrowed_jobs;b;b=b->next)n++;return n;}
static unsigned retired_count(const struct object*d)
{unsigned n=0;for(struct job*j=d->retired;j;j=j->next)n++;return n;}
struct arenas { NVNmemoryPool*pool; unsigned calls; };
static void more_commands(NVNcommandBuffer*c,NVNcommandBufferMemoryEvent kind,size_t need,void*user)
{
    struct arenas*a=user;T(kind==NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY&&need<=4096&&!a->calls);
    a->calls++;api_nvnCommandBufferAddCommandMemory(c,a->pool,4096,4096);
}
struct copy_callback_state { NVNdevice *device; NVNcommandBuffer *source; NVNcommandHandle *selection;
    struct object *source_pool,*destination_pool; unsigned calls; };
static void retire_source_in_callback(NVNcommandBuffer*c,NVNcommandBufferMemoryEvent kind,size_t need,void*user)
{
    struct copy_callback_state*x=user;T(!x->calls&&kind==NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY&&need<=4096);
    memset(nn_bo_map(x->source_pool->bo),0xee,x->source_pool->size);
    NVNcommandHandle old=*x->selection;OK(api_nvnDeviceFinalizeCommandHandle(x->device,old));
    OK(api_nvnCommandBufferFinalize(x->source));
    OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)x->source_pool->key));
    *x->selection=UINT64_MAX;x->calls++;
    OK(api_nvnCommandBufferAddCommandMemory(c,x->destination_pool->key,0,4096));
}
static void begin(NVNdevice*d,NVNcommandBuffer*c,struct object*p,void*control,size_t control_bytes)
{
    T(api_nvnCommandBufferInitialize(c,d));OK(api_nvnCommandBufferAddCommandMemory(c,p->key,0,4096));
    OK(api_nvnCommandBufferAddControlMemory(c,control,control_bytes));OK(api_nvnCommandBufferBeginRecording(c));
}
static void cleanup_device(NVNdevice*d)
{
    OK(api_nvnDeviceFinalize(d));while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
}
static void queue(NVNdevice*d,NVNqueue*q,NVNqueueBuilder*b,size_t compute)
{
    OK(api_nvnQueueBuilderSetDefaults(b));OK(api_nvnQueueBuilderSetDevice(b,d));
    OK(api_nvnQueueBuilderSetComputeMemorySize(b,compute));T(api_nvnQueueInitialize(q,b));T(!nn_nvn_error());
}
static void snapshot_and_jobs(void)
{
    NVNdeviceBuilder db={0};NVNdevice dk={0};NVNqueue q0={0},q={0},small={0};NVNqueueBuilder qb={0};NVNsync sync={0};
    OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object*dev=find(&dk,DEVICE);
    queue(&dk,&q0,&qb,262144);queue(&dk,&q,&qb,262144);queue(&dk,&small,&qb,0);
    unsigned char keys[7];struct object*source_arena=pool(dev,&keys[0],16384),*dest_arena=pool(dev,&keys[1],4096);
    struct object*data=pool(dev,&keys[2],4096),*seed_arena=pool(dev,&keys[3],4096);
    NVNcommandBuffer seed={0},src={0},dst={0};_Alignas(16) unsigned char controls[3][256];
    begin(&dk,&seed,seed_arena,controls[0],256);OK(api_nvnCommandBufferClearBuffer(&seed,nn_bo_address(data->bo),4,7));
    NVNcommandHandle seed_id=api_nvnCommandBufferEndRecording(&seed);T(seed_id);OK(api_nvnQueueSubmitCommands(&q0,1,&seed_id));
    T(api_nvnSyncInitialize(&sync,&dk));OK(api_nvnQueueFenceSync(&q0,&sync,NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE,0));
    T(api_nvnCommandBufferInitialize(&src,&dk));OK(api_nvnCommandBufferAddCommandMemory(&src,source_arena->key,0,64));
    OK(api_nvnCommandBufferAddControlMemory(&src,controls[1],256));struct arenas arenas={(NVNmemoryPool*)source_arena->key,0};
    OK(api_nvnCommandBufferSetMemoryCallback(&src,more_commands));OK(api_nvnCommandBufferSetMemoryCallbackData(&src,&arenas));
    OK(api_nvnCommandBufferBeginRecording(&src));OK(api_nvnCommandBufferWaitSync(&src,&sync));
    OK(api_nvnCommandBufferBarrier(&src,NVN_BARRIER_ORDER_PRIMITIVES_BIT));
    OK(api_nvnCommandBufferClearBuffer(&src,nn_bo_address(data->bo)+64,4,123));
    char label[180];memset(label,'x',179);label[179]=0;OK(api_nvnCommandBufferInsertDebugMarkerDynamic(&src,13,label));
    NVNcommandHandle source_id=api_nvnCommandBufferEndRecording(&src);T(source_id);struct handle*source=lookup(source_id);
    T(source&&source->chunks&&source->chunks->next&&source->wait_count==1&&source->jobs&&arenas.calls==1);
    struct job*job=source->jobs;nn_bo*job_bo=job->bo;unsigned destroyed=nn_test_destroy_count(job_bo);
    /* Classification-only injection: the queue must admit copied compute job
     * references too. The RAM fixture never executes this native barrier IB. */
    job->compute=1;size_t size=source->bytes;T(source->call_limit==size);
    unsigned char*expected=malloc(size);T(expected);T(!copy_source_bytes(source,expected));
    begin(&dk,&dst,dest_arena,controls[2],256);
    struct recording*r=find(&dst,COMMAND)->record;size_t before=r->used,refs=data->refs;
    NVNcommandHandle bad[2]={source_id,UINT64_MAX};ERR(copy_commands(&dst,2,bad),EINVAL);T(r->used==before&&data->refs==refs&&!job->borrowers);
    deny_malloc_size=sizeof(struct job_borrow);ERR(copy_commands(&dst,1,&source_id),ENOMEM);
    T(!deny_malloc_size&&r->used==before&&data->refs==refs&&!job->borrowers);
    struct late_draw invalid={.command_offset=size};source->draws=&invalid;
    ERR(copy_commands(&dst,1,&source_id),EINVAL);T(r->used==before&&data->refs==refs&&!job->borrowers);source->draws=NULL;
    OK(copy_commands(&dst,1,&source_id));copies++;T(r->used==size&&borrowed(r->current)==1&&job->borrowers==1&&r->current->wait_count==1);
    T(!memcmp(nn_bo_map(dest_arena->bo),expected,size));before=r->used;refs=data->refs;
    ERR(copy_commands(&dst,1,&source_id),ENOTSUP);T(r->used==before&&data->refs==refs&&job->borrowers==1);
    nn_test_retire(find(&q0,QUEUE)->queue,1);OK(copy_commands(&dst,1,&source_id));copies++;
    T(r->used==size*2&&borrowed(r->current)==1&&job->borrowers==1);
    memset(nn_bo_map(source_arena->bo),0xcd,source_arena->size);
    T(!memcmp(nn_bo_map(dest_arena->bo),expected,size)&&!memcmp((char*)nn_bo_map(dest_arena->bo)+size,expected,size));free(expected);
    OK(api_nvnDeviceFinalizeCommandHandle(&dk,source_id));T(!lookup(source_id));
    OK(api_nvnCommandBufferFinalize(&src));T(job->owner_retired&&job->borrowers==1&&nn_test_destroy_count(job_bo)==destroyed);
    OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)source_arena->key));
    reap_jobs(dev);T(!nn_nvn_error()&&job->borrowers==1&&nn_test_destroy_count(job_bo)==destroyed);
    OK(api_nvnDeviceFinalizeCommandHandle(&dk,seed_id));OK(api_nvnCommandBufferFinalize(&seed));
    OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)seed_arena->key));
    NVNcommandHandle destination_id=api_nvnCommandBufferEndRecording(&dst);T(destination_id);struct handle*destination=lookup(destination_id);
    T(destination&&destination->bytes==size*2+8&&destination->wait_count==1&&destination->call_limit==destination->bytes);
    unsigned accepted=nn_test_accepted();ERR(api_nvnQueueSubmitCommands(&small,1,&destination_id),ENOSPC);T(nn_test_accepted()==accepted);
    OK(api_nvnQueueSubmitCommands(&q,1,&destination_id));T(nn_test_last_has(job_bo));
    T(find(&q,QUEUE)->queue_state->compute_bytes==sizeof(nn_bo*));
    OK(api_nvnQueueSubmitCommands(&q,1,&destination_id));T(nn_test_last_has(job_bo));
    OK(api_nvnSyncFinalize(&sync));ERR(api_nvnQueueFinalize(&q0),EBUSY);
    OK(api_nvnDeviceFinalizeCommandHandle(&dk,destination_id));T(job->borrowers==0&&job->owner_retired);
    T(nn_test_destroy_count(job_bo)==destroyed&&retired_count(dev)>=3);
    OK(api_nvnCommandBufferFinalize(&dst));OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)dest_arena->key));
    ERR(api_nvnMemoryPoolFinalize((NVNmemoryPool*)data->key),EBUSY);
    OK(api_nvnQueueFinish(&q));T(nn_test_destroy_count(job_bo)==destroyed+1);
    OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)data->key));
    OK(api_nvnQueueFinalize(&small));OK(api_nvnQueueFinalize(&q));OK(api_nvnQueueFinalize(&q0));cleanup_device(&dk);
}
static void nested_abort_and_unknown(void)
{
    NVNdeviceBuilder db={0};NVNdevice dk={0};NVNqueue q={0};NVNqueueBuilder qb={0};
    OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object*dev=find(&dk,DEVICE);queue(&dk,&q,&qb,262144);
    unsigned char keys[4];struct object*p[4];NVNcommandBuffer c[4]={0};NVNcommandHandle ids[3]={0};_Alignas(16) unsigned char ctrl[4][128];
    for(unsigned i=0;i<4;i++)p[i]=pool(dev,&keys[i],4096);
    begin(&dk,c,p[0],ctrl[0],128);OK(api_nvnCommandBufferBarrier(c,NVN_BARRIER_ORDER_PRIMITIVES_BIT));ids[0]=api_nvnCommandBufferEndRecording(c);T(ids[0]);
    struct job*j=lookup(ids[0])->jobs;nn_bo*bo=j->bo;unsigned destroyed=nn_test_destroy_count(bo);
    for(unsigned i=1;i<3;i++){begin(&dk,c+i,p[i],ctrl[i],128);OK(copy_commands(c+i,1,ids+i-1));copies++;ids[i]=api_nvnCommandBufferEndRecording(c+i);T(ids[i]);}
    T(j->borrowers==2&&borrowed(lookup(ids[1]))==1&&borrowed(lookup(ids[2]))==1);
    for(unsigned i=0;i<2;i++){OK(api_nvnDeviceFinalizeCommandHandle(&dk,ids[i]));OK(api_nvnCommandBufferFinalize(c+i));OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)p[i]->key));}
    T(j->owner_retired&&j->borrowers==1&&nn_test_destroy_count(bo)==destroyed);
    begin(&dk,c+3,p[3],ctrl[3],128);OK(copy_commands(c+3,1,ids+2));copies++;T(j->borrowers==2);
    OK(api_nvnCommandBufferFinalize(c+3));T(j->borrowers==1);
    /* Reserve invokes caller code only after raw bytes, jobs, refs and all
     * selectors were frozen. Finalize/poison source plus mutate IDs there. */
    size_t source_bytes=lookup(ids[2])->bytes;unsigned char*expected=malloc(source_bytes);T(expected);
    T(!copy_source_bytes(lookup(ids[2]),expected));NVNcommandHandle old_source=ids[2];
    NVNcommandHandle selected[2]={old_source,old_source};
    T(api_nvnCommandBufferInitialize(c+3,&dk));OK(api_nvnCommandBufferAddCommandMemory(c+3,p[3]->key,0,8));
    OK(api_nvnCommandBufferAddControlMemory(c+3,ctrl[3],128));
    struct copy_callback_state callback={&dk,c+2,selected,p[2],p[3],0};
    OK(api_nvnCommandBufferSetMemoryCallback(c+3,retire_source_in_callback));
    OK(api_nvnCommandBufferSetMemoryCallbackData(c+3,&callback));OK(api_nvnCommandBufferBeginRecording(c+3));
    OK(copy_commands(c+3,2,selected));copies++;
    T(callback.calls==1&&selected[0]==UINT64_MAX&&selected[1]==old_source&&!lookup(old_source)&&j->borrowers==1);
    T(!memcmp(nn_bo_map(p[3]->bo),expected,source_bytes));
    T(!memcmp((char*)nn_bo_map(p[3]->bo)+source_bytes,expected,source_bytes));free(expected);
    NVNcommandHandle target=api_nvnCommandBufferEndRecording(c+3);T(target);
    nn_test_ambiguous_next();ERR(api_nvnQueueSubmitCommands(&q,1,&target),EIO);model_unknowns++;
    OK(api_nvnDeviceFinalizeCommandHandle(&dk,target));T(j->borrowers==0&&nn_test_destroy_count(bo)==destroyed);
    OK(api_nvnCommandBufferFinalize(c+3));OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)p[3]->key));
    ERR(api_nvnQueueFinish(&q),EBUSY);T(nn_test_destroy_count(bo)==destroyed&&retired_count(dev)>=2);
    /* A HOST fixture can resolve its own mock. Real KGSL has no such API and
     * the driver never claimed the ambiguous submission completed. */
    nn_test_resolve_unknown(find(&q,QUEUE)->queue);OK(api_nvnQueueFinish(&q));T(nn_test_destroy_count(bo)==destroyed+1);
    OK(api_nvnQueueFinalize(&q));cleanup_device(&dk);
}
static void abort_last_retired_borrow(void)
{
    NVNdeviceBuilder db={0};NVNdevice dk={0};NVNcommandBuffer src={0},dst={0};
    _Alignas(16) unsigned char control[2][128];unsigned char keys[2];
    OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));
    struct object*dev=find(&dk,DEVICE),*source=pool(dev,keys,4096),*target=pool(dev,keys+1,4096);
    begin(&dk,&src,source,control[0],128);
    OK(api_nvnCommandBufferBarrier(&src,NVN_BARRIER_ORDER_PRIMITIVES_BIT));
    NVNcommandHandle id=api_nvnCommandBufferEndRecording(&src);T(id);
    struct job*j=lookup(id)->jobs;T(j&&j->bo);nn_bo*bo=j->bo;
    unsigned destroyed=nn_test_destroy_count(bo);
    begin(&dk,&dst,target,control[1],128);OK(copy_commands(&dst,1,&id));copies++;
    OK(api_nvnDeviceFinalizeCommandHandle(&dk,id));
    OK(api_nvnCommandBufferFinalize(&src));OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)source->key));
    T(j->owner_retired&&j->borrowers==1&&nn_test_destroy_count(bo)==destroyed);
    /* The destination has never ended recording or submitted. Aborting its
     * final borrowed owner must still find the device and retire the child. */
    OK(api_nvnCommandBufferFinalize(&dst));
    T(nn_test_destroy_count(bo)==destroyed+1&&!dev->retired);
    OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)target->key));
    cleanup_device(&dk);retired_abort_cases++;
}
static void late_draw_snapshot(void)
{
    NVNdeviceBuilder db={0};NVNdevice dk={0};NVNprogram pk={0};NVNqueue q={0};NVNqueueBuilder qb={0};
    OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object*dev=find(&dk,DEVICE);queue(&dk,&q,&qb,262144);
    struct nn_gfx_image image=NVN_GRAPHICS_IMAGE;T(image.schema==3&&image.profile==NN_GFX_TEXTURE_2D_NORMALIZED&&graphics_profile(&image));
    unsigned char keys[11];struct object*p[11];for(unsigned i=0;i<11;i++)p[i]=pool(dev,&keys[i],i==3||i==6||i==7?16384:4096);
    NVNtexture rtkey={0},tx0={0},tx1={0},tx2={0};struct object*rt=texture(dev,&rtkey,p[3],64);
    T(api_nvnProgramInitialize(&pk,&dk));struct object*program=find(&pk,PROGRAM);
    struct nn_nvn_sampled map={.stage=NVN_SHADER_STAGE_FRAGMENT,.texture_slot=10,.sampler_slot=0};
    struct nn_nvn_compiled_graphics compiled={.image=&image,.sampled=&map,.sampled_count=1};program->graphics=&compiled;
    T(!nn_bo_alloc(dev->gpu,image.vs_code_bytes,&program->bo));T(!nn_bo_alloc(dev->gpu,image.fs_code_bytes,&program->fs_bo));
    memcpy(nn_bo_map(program->bo),image.vs_code,image.vs_code_bytes);memcpy(nn_bo_map(program->fs_bo),image.fs_code,image.fs_code_bytes);
    NVNtexturePool tp={0};NVNsamplerPool sp={0};NVNsampler sampler={0};
    T(api_nvnTexturePoolInitialize(&tp,p[6]->key,0,272));T(api_nvnSamplerPoolInitialize(&sp,p[7]->key,0,272));
    texture(dev,&tx0,p[5],2);texture(dev,&tx1,p[9],2);texture(dev,&tx2,p[10],2);
    struct object*sm=create(&sampler,SAMPLER);T(sm);sm->device=dev;dev->refs++;
    sm->sampler=(struct sampler_desc){.min=NVN_MIN_FILTER_NEAREST,.mag=NVN_MAG_FILTER_NEAREST,
        .wrap={NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE},
        .compare=NVN_COMPARE_MODE_NONE,.compare_func=NVN_COMPARE_FUNC_LESS,.anisotropy=1,.reduction=NVN_SAMPLER_REDUCTION_AVERAGE};
    OK(api_nvnTexturePoolRegisterTexture(&tp,257,&tx0,NULL));OK(api_nvnSamplerPoolRegisterSampler(&sp,259,&sampler));
    NVNcommandBuffer src={0},dst={0};_Alignas(16) unsigned char ctrl[2][128];begin(&dk,&src,p[0],ctrl[0],128);
    struct recording*r=find(&src,COMMAND)->record;r->graphics_program=program;program->refs++;r->target=rt;rt->refs++;
    OK(api_nvnCommandBufferBindVertexBuffer(&src,0,nn_bo_address(p[1]->bo),image.vertex_bytes));
    r->graphics_ubos[0][0]=(struct bound){p[2],0,64,nn_bo_address(p[2]->bo)};p[2]->refs++;
    r->graphics_ubos[1][0]=(struct bound){p[4],0,64,nn_bo_address(p[4]->bo)};p[4]->refs++;
    NVNvertexAttribState attributes[3];NVNvertexStreamState stream;
    for(unsigned i=0;i<3;i++)OK(api_nvnVertexAttribStateSetDefaults(attributes+i));
    OK(api_nvnVertexAttribStateSetFormat(attributes,NVN_FORMAT_RG32F,0));
    OK(api_nvnVertexAttribStateSetFormat(attributes+1,NVN_FORMAT_RG32F,8));
    OK(api_nvnVertexAttribStateSetFormat(attributes+2,NVN_FORMAT_RGBA32F,16));
    OK(api_nvnVertexStreamStateSetDefaults(&stream));
    OK(api_nvnVertexStreamStateSetStride(&stream,32));
    OK(api_nvnCommandBufferBindVertexAttribState(&src,3,attributes));
    OK(api_nvnCommandBufferBindVertexStreamState(&src,1,&stream));
    OK(api_nvnCommandBufferSetViewport(&src,0,0,64,64));
    OK(api_nvnCommandBufferSetScissor(&src,0,0,64,64));
    OK(api_nvnCommandBufferSetDepthRange(&src,0,1));
    r->render.blend_color[2]=0.375f;
    OK(api_nvnCommandBufferSetTexturePool(&src,&tp));OK(api_nvnCommandBufferSetSamplerPool(&src,&sp));
    OK(api_nvnCommandBufferBindSeparateTexture(&src,1,10,(NVNseparateTextureHandle){257}));
    OK(api_nvnCommandBufferBindSeparateSampler(&src,1,0,(NVNseparateSamplerHandle){259ull<<20}));
    OK(api_nvnCommandBufferDrawArrays(&src,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
    NVNcommandHandle a=api_nvnCommandBufferEndRecording(&src);T(a);struct late_draw*original=lookup(a)->draws;T(original);
    struct late_draw saved=*original;
    begin(&dk,&dst,p[8],ctrl[1],128);OK(api_nvnCommandBufferInsertDebugMarkerDynamic(&dst,1,"prefix"));
    size_t prefix=find(&dst,COMMAND)->record->used,refs_before=program->refs,cpu_refs_before=find(&tp,TPOOL)->refs;
    deny_malloc_size=sizeof(struct late_draw);ERR(copy_commands(&dst,1,&a),ENOMEM);
    T(!deny_malloc_size&&find(&dst,COMMAND)->record->used==prefix&&program->refs==refs_before&&find(&tp,TPOOL)->refs==cpu_refs_before);
    deny_malloc_size=(lookup(a)->count+1)*sizeof(struct object*);ERR(copy_commands(&dst,1,&a),ENOMEM);
    T(!deny_malloc_size&&find(&dst,COMMAND)->record->used==prefix&&program->refs==refs_before&&find(&tp,TPOOL)->refs==cpu_refs_before);
    OK(copy_commands(&dst,1,&a));copies++;
    struct late_draw*clone=find(&dst,COMMAND)->record->current->draws;
    T(clone&&clone!=original&&clone->command_offset==prefix+saved.command_offset);
    T(!memcmp(&clone->render,&saved.render,sizeof(saved.render))&&!memcmp(&clone->draw,&saved.draw,sizeof(saved.draw)));
    T(clone->texture_id==257&&clone->sampler_id==259&&clone->render.blend_color[2]==0.375f);
    T(find(&dst,COMMAND)->record->current->cpu_count==2);
    memset(&original->render,0xcd,sizeof(original->render));original->texture_id=258;
    memset(nn_bo_map(p[0]->bo),0xcd,p[0]->size);OK(api_nvnDeviceFinalizeCommandHandle(&dk,a));OK(api_nvnCommandBufferFinalize(&src));
    OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)p[0]->key));
    OK(api_nvnTexturePoolRegisterTexture(&tp,257,&tx1,NULL));OK(api_nvnTextureFinalize(&tx0));
    OK(api_nvnTextureFinalize(&tx1));OK(api_nvnTextureFinalize(&tx2));OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)p[5]->key));
    NVNcommandHandle b=api_nvnCommandBufferEndRecording(&dst);T(b);OK(api_nvnQueueSubmitCommands(&q,1,&b));T(nn_test_last_has(p[9]->bo));
    uint32_t*parent=nn_bo_map(nn_test_last_parent());T(parent[prefix/4]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
    /* Registration is still late-bound at ENQUEUE. Copy freezes selectors,
     * not the later contents of native descriptor-pool slots. */
    texture(dev,&tx2,p[10],2);OK(api_nvnTexturePoolRegisterTexture(&tp,257,&tx2,NULL));OK(api_nvnTextureFinalize(&tx2));
    OK(api_nvnQueueSubmitCommands(&q,1,&b));T(nn_test_last_has(p[10]->bo));
    ERR(api_nvnMemoryPoolFinalize((NVNmemoryPool*)p[9]->key),EBUSY);OK(api_nvnQueueFinish(&q));
    OK(api_nvnDeviceFinalizeCommandHandle(&dk,b));OK(api_nvnCommandBufferFinalize(&dst));
    OK(api_nvnTexturePoolFinalize(&tp));OK(api_nvnSamplerPoolFinalize(&sp));OK(api_nvnSamplerFinalize(&sampler));
    OK(api_nvnTextureFinalize(&rtkey));OK(api_nvnProgramFinalize(&pk));
    for(unsigned i=1;i<11;i++)if(i!=5)OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)p[i]->key));
    OK(api_nvnQueueFinalize(&q));cleanup_device(&dk);
}
int main(void)
{
    copy_commands=(PFNNVNCOMMANDBUFFERCOPYCOMMANDSPROC)nvnBootstrapLoader("nvnCommandBufferCopyCommands");T(copy_commands);
    T((PFNNVNCOMMANDBUFFERCALLCOMMANDSPROC)nvnBootstrapLoader("nvnCommandBufferCallCommands")==api_nvnCommandBufferCallCommands);
    T(device_modes_baseline_main()==0);
    snapshot_and_jobs();nested_abort_and_unknown();abort_last_retired_borrow();late_draw_snapshot();
    printf("{\"passed\":true,\"scope\":\"HOST ONLY actual Copy recorder and real materializer; RAM timeline, PM4 not executed\",\"checks\":%u,\"copy_calls\":%u,\"ambiguous_model_submits\":%u,\"gpu_submits\":0,\"game_modules_executed\":false}\n",checks,copies,model_unknowns);
    return 0;
}
