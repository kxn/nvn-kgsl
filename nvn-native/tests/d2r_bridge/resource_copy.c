/* Actual candidate core + RAM ownership backend. PM4 is inspected only; this
 * fixture does NOT execute/interpret shader, CP or GPU operations. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_alloc ram_bo_alloc
#define nn_bo_cache ram_bo_cache
#define nn_bo_destroy ram_bo_destroy
#define nn_bo_import ram_bo_import
#define nn_bo_idle ram_bo_idle
#define nn_queue_submit ram_queue_submit
#define nn_queue_finish ram_queue_finish
#include NN_RESOURCE_MEMORY_BACKEND
#undef nn_bo_alloc
#undef nn_bo_cache
#undef nn_bo_destroy
#undef nn_bo_import
#undef nn_bo_idle
#undef nn_queue_submit
#undef nn_queue_finish
static unsigned submits,imports,finishes,allocs,caches,destroys;
static int fail_submit,fail_finish,fail_alloc,fail_cache,fail_destroy,fail_import;
static uint32_t observed[400000];static size_t observed_words;
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **out){allocs++;if(fail_alloc){fail_alloc=0;*out=NULL;return ENOMEM;}return ram_bo_alloc(d,n,out);}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int from){caches++;if(fail_cache){fail_cache=0;return EIO;}return ram_bo_cache(b,o,n,from);}
int nn_bo_destroy(nn_bo *b){destroys++;if(fail_destroy){fail_destroy=0;return EIO;}return ram_bo_destroy(b);}
int nn_bo_import(nn_device *d,void *p,size_t n,nn_bo **out){imports++;if(fail_import){fail_import=0;*out=NULL;return ENOMEM;}return ram_bo_import(d,p,n,out);}
int nn_bo_idle(const nn_bo *b){return !b?EINVAL:b->busy?EBUSY:0;}
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t off,size_t n,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
    submits++;if(n/4>400000 || q->count+count+1>64)return E2BIG;
    observed_words=n/4;memcpy(observed,(char *)ib->map+off,n);
    q->pending[q->count++]=ib;ib->busy++;
    for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}
    if(fail_submit){fail_submit=0;return EDEADLK;}
    *timestamp=submits;return 0;
}
int nn_queue_finish(nn_queue *q,uint32_t timeout){finishes++;if(fail_finish){fail_finish=0;return ETIMEDOUT;}return ram_queue_finish(q,timeout);}
#include NN_RESOURCE_CORE
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s error%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define GOOD() CHECK(!nn_nvn_error())
#define BAD(e) do{CHECK(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static NVNdevice device;static NVNdeviceBuilder db;
static NVNmemoryPool pool;static NVNmemoryPoolBuilder pb;
static NVNcommandBuffer command;static NVNtexture source,target;static NVNtextureBuilder tb;
static void *storage,*output;static unsigned char control[8192] __attribute__((aligned(8)));
static int texture(NVNtexture *t,size_t offset,int format,int width,int height,int stride)
{
    api_nvnTextureBuilderSetDefaults(&tb);api_nvnTextureBuilderSetDevice(&tb,&device);
    api_nvnTextureBuilderSetFlags(&tb,NVN_TEXTURE_FLAGS_LINEAR);api_nvnTextureBuilderSetTarget(&tb,NVN_TEXTURE_TARGET_2D);
    api_nvnTextureBuilderSetSize2D(&tb,width,height);api_nvnTextureBuilderSetLevels(&tb,1);api_nvnTextureBuilderSetSamples(&tb,0);
    api_nvnTextureBuilderSetFormat(&tb,format);api_nvnTextureBuilderSetStride(&tb,stride);
    api_nvnTextureBuilderSetStorage(&tb,&pool,(ptrdiff_t)offset);return api_nvnTextureInitialize(t,&tb);
}
static void begin(void){api_nvnCommandBufferBeginRecording(&command);}
static void discard(void)
{
    NVNcommandHandle h=api_nvnCommandBufferEndRecording(&command);assert(h);api_nvnDeviceFinalizeCommandHandle(&device,h);
}
static uint32_t *child(void){return nn_bo_map(find(&command,COMMAND)->record->current->jobs->bo);}
static uint64_t pair_address(const uint32_t *p){return p[0]|(uint64_t)p[1]<<32;}
static int empty_recordings(void)
{
    NVNqueueBuilder qb={0};NVNqueue queue={0};NVNcommandHandle recorded[2];
    api_nvnQueueBuilderSetDefaults(&qb);api_nvnQueueBuilderSetDevice(&qb,&device);
    CHECK(api_nvnQueueInitialize(&queue,&qb));GOOD();
    CHECK(!api_nvnCommandBufferEndRecording(&command));BAD(EINVAL);
    unsigned before=submits;
    for(unsigned i=0;i<2;i++){
        ptrdiff_t saved_row=api_nvnCommandBufferGetCopyRowStride(&command),saved_image=api_nvnCommandBufferGetCopyImageStride(&command);GOOD();
        begin();GOOD();CHECK(api_nvnCommandBufferIsRecording(&command));
        CHECK(api_nvnCommandBufferGetCopyRowStride(&command)==saved_row&&api_nvnCommandBufferGetCopyImageStride(&command)==saved_image);GOOD();
        if(i)api_nvnCommandBufferSetCopyRowStride(&command,64); /* state only */
        recorded[i]=api_nvnCommandBufferEndRecording(&command);CHECK(recorded[i]);GOOD();
        CHECK(!api_nvnCommandBufferIsRecording(&command));
        CHECK(api_nvnCommandBufferGetCopyRowStride(&command)==(i?64:saved_row)&&api_nvnCommandBufferGetCopyImageStride(&command)==saved_image);GOOD();
        CHECK(!api_nvnCommandBufferEndRecording(&command));BAD(EINVAL);
    }
    CHECK(recorded[0]!=recorded[1]);
    for(unsigned replay=0;replay<2;replay++){
        api_nvnQueueSubmitCommands(&queue,2,recorded);GOOD();
        api_nvnQueueFinish(&queue);GOOD();
    }
    CHECK(submits==before+4); /* two handles, submitted twice */
    for(unsigned i=0;i<2;i++)api_nvnDeviceFinalizeCommandHandle(&device,recorded[i]);
    GOOD();api_nvnQueueFinalize(&queue);GOOD();
    remove_object(find(&qb,QB));return 0;
}
static int metadata(void)
{
    const char *names[]={"nvnTextureBuilderSetPackagedTextureLayout","nvnTextureBuilderSetSize1D","nvnTextureViewSetDefaults","nvnTextureViewSetFormat","nvnTextureViewSetLevels","nvnTextureViewSetLayers","nvnTextureViewSetTarget","nvnTextureViewGetLayers","nvnTextureCompare","nvnTextureReadTexels","nvnTextureReadTexelsStrided","nvnTextureInvalidateTexels","nvnCommandBufferCopyBufferToBuffer","nvnCommandBufferCopyBufferToTexture","nvnCommandBufferCopyTextureToBuffer","nvnCommandBufferCopyTextureToTexture","nvnCommandBufferClearTexture","nvnCommandBufferClearTextureui","nvnCommandBufferDownsampleTextureView","nvnCommandBufferSetCopyRowStride","nvnCommandBufferUpdateUniformBuffer"};
    for(unsigned i=0;i<sizeof(names)/sizeof(*names);i++)CHECK(nvnBootstrapLoader(names[i]));
    NVNtextureView v,copy;api_nvnTextureViewSetDefaults(&v);GOOD();int a=9,b=10;
    CHECK(!api_nvnTextureViewGetLayers(&v,&a,&b));CHECK(a==0&&b==0);GOOD();
    api_nvnTextureViewSetLevels(&v,0,1);api_nvnTextureViewSetLayers(&v,0,1);api_nvnTextureViewSetFormat(&v,NVN_FORMAT_RGBA8);api_nvnTextureViewSetTarget(&v,NVN_TEXTURE_TARGET_2D);GOOD();
    memcpy(&copy,&v,sizeof(v));CHECK(api_nvnTextureViewGetLayers(&copy,&a,&b));CHECK(a==0&&b==1);GOOD();
    NVNtextureView old=copy;api_nvnTextureViewSetLayers(&copy,-1,1);BAD(EINVAL);CHECK(!memcmp(&old,&copy,sizeof(copy)));
    api_nvnTextureViewSetLevels(&copy,INT_MAX,1);BAD(EINVAL);CHECK(!memcmp(&old,&copy,sizeof(copy)));
    api_nvnTextureViewGetLayers(NULL,&a,&b);BAD(EINVAL);api_nvnTextureViewGetLayers(&v,NULL,&b);BAD(EINVAL);
    struct texture_desc resolved;CHECK(!resolve_texture_view(find(&source,TEXTURE),&copy,&resolved));
    api_nvnTextureViewSetFormat(&copy,NVN_FORMAT_RGBA8UI);GOOD();CHECK(resolve_texture_view(find(&source,TEXTURE),&copy,&resolved)==ENOTSUP);
    api_nvnTextureViewSetDefaults(&copy);((unsigned char *)&copy)[39]=1;CHECK(resolve_texture_view(find(&source,TEXTURE),&copy,&resolved)==EINVAL);
    CHECK(api_nvnTextureCompare(&source,&source));CHECK(!api_nvnTextureCompare(&source,&target));GOOD();
    PFNNVNTEXTUREGETRAWSTORAGECLASSPROC raw=(PFNNVNTEXTUREGETRAWSTORAGECLASSPROC)nvnBootstrapLoader("nvnTextureGetRawStorageClass");
    CHECK(raw);CHECK(raw(&source)==NVN_RAW_STORAGE_CLASS_LINEAR);GOOD();
    NVNpackagedTextureLayout layout={{1,2,3,4,5,6,7,8}};
    api_nvnTextureBuilderSetPackagedTextureLayout(&tb,&layout);GOOD();CHECK(!api_nvnTextureBuilderGetStorageSize(&tb));BAD(ENOTSUP);
    CHECK(find(&tb,TB)->texture_packaged && !memcmp(find(&tb,TB)->packaged_layout,layout.layout,8));
    api_nvnTextureBuilderSetPackagedTextureLayout(&tb,NULL);CHECK(api_nvnTextureBuilderGetStorageSize(&tb)==16384);GOOD();
    unsigned formats[]={NVN_FORMAT_R8,NVN_FORMAT_RG8,NVN_FORMAT_RGBA8,NVN_FORMAT_RGBA16F,NVN_FORMAT_RGBA32F};
    for(unsigned i=0;i<5;i++){
        api_nvnTextureBuilderSetFormat(&tb,formats[i]);api_nvnTextureBuilderSetSize1D(&tb,97);api_nvnTextureBuilderSetTarget(&tb,NVN_TEXTURE_TARGET_1D);
        unsigned cpp=linear_cpp(formats[i]);api_nvnTextureBuilderSetStride(&tb,(97*cpp+255)&~255u);
        CHECK(api_nvnTextureBuilderGetStorageSize(&tb)==(((size_t)find(&tb,TB)->texture.stride*4+4095)&~(size_t)4095));GOOD();
    }
    api_nvnTextureBuilderSetStride(&tb,65);CHECK(!api_nvnTextureBuilderGetStorageSize(&tb));BAD(ENOTSUP);
    CHECK(texture(&(NVNtexture){0},32768,NVN_FORMAT_RGBA8,13,7,256));
    /* The temporary key above is alive for this function; finalize before exit. */
    struct object *o=objects;while(o && !(o->kind==TEXTURE && o->offset==32768))o=o->next;CHECK(o);api_nvnTextureFinalize((NVNtexture *)o->key);GOOD();
    return 0;
}
static int copies(void)
{
    uint64_t base=api_nvnMemoryPoolGetBufferAddress(&pool);NVNcopyRegion r={.width=4,.height=3,.depth=1};
    unsigned before=submits;begin();api_nvnCommandBufferSetCopyRowStride(&command,64);GOOD();
    api_nvnCommandBufferCopyBufferToTexture(&command,base+32768,&target,NULL,&r,0);GOOD();
    uint32_t *w=child();CHECK(w[27]==nvn_pm4_packet7(NVN_CP_MEM_TO_MEM,5));CHECK(w[28]==0);
    CHECK(pair_address(w+29)==base+16384);CHECK(pair_address(w+31)==base+32768);
    CHECK(pair_address(w+29+4*6)==base+16384+256);CHECK(pair_address(w+31+4*6)==base+32768+64);
    CHECK(find(&target,TEXTURE)->refs==1);api_nvnTextureFinalize(&target);BAD(EBUSY);
    CHECK(submits==before);
    NVNqueue queue;NVNqueueBuilder qb;api_nvnQueueBuilderSetDefaults(&qb);api_nvnQueueBuilderSetDevice(&qb,&device);CHECK(api_nvnQueueInitialize(&queue,&qb));
    NVNcommandHandle h=api_nvnCommandBufferEndRecording(&command);CHECK(h);api_nvnQueueSubmitCommands(&queue,1,&h);GOOD();CHECK(submits==before+1);CHECK(nn_bo_idle(find(&target,TEXTURE)->bo)==EBUSY);
    api_nvnDeviceFinalizeCommandHandle(&device,h);GOOD();CHECK(find(&device,DEVICE)->retired);CHECK(find(&target,TEXTURE)->refs==0);
    api_nvnQueueFinalize(&queue);BAD(EBUSY);api_nvnQueueFinish(&queue);GOOD();CHECK(!find(&device,DEVICE)->retired);CHECK(!nn_bo_idle(find(&target,TEXTURE)->bo));api_nvnQueueFinalize(&queue);GOOD();
    begin();api_nvnCommandBufferCopyTextureToBuffer(&command,&source,NULL,&r,base+32768,0);GOOD();w=child();CHECK(pair_address(w+29)==base+32768);CHECK(pair_address(w+31)==base);discard();GOOD();
    begin();api_nvnCommandBufferCopyTextureToTexture(&command,&source,NULL,&r,&target,NULL,&r,0);GOOD();w=child();CHECK(pair_address(w+29)==base+16384);CHECK(pair_address(w+31)==base);discard();GOOD();
    begin();api_nvnCommandBufferCopyBufferToBuffer(&command,base+32768,base+65536,16384,0);GOOD();w=child();CHECK(pair_address(w+29)==base+65536);CHECK(pair_address(w+31)==base+32768);discard();GOOD();
    begin();struct recording *rec=find(&command,COMMAND)->record;size_t used=rec->used,refs=rec->current->count;unsigned bos=find(&device,DEVICE)->gpu->bos;
    api_nvnCommandBufferCopyBufferToBuffer(&command,base,base+4,4096,0);BAD(EINVAL);CHECK(rec->used==used&&rec->current->count==refs&&find(&device,DEVICE)->gpu->bos==bos);
    api_nvnCommandBufferCopyBufferToBuffer(&command,base,base+32768,0,0);BAD(EINVAL);
    uint64_t arena=nn_bo_address(rec->pool->bo);api_nvnCommandBufferCopyBufferToBuffer(&command,base,arena,64,0);BAD(ENOTSUP);CHECK(rec->used==used);
    api_nvnCommandBufferCopyBufferToBuffer(&command,base+1,base+32768,4,0);BAD(EINVAL);
    api_nvnCommandBufferCopyBufferToBuffer(&command,base,base+32768,4,NVN_COPY_FLAGS_LINEAR_FILTER_BIT);BAD(ENOTSUP);
    api_nvnCommandBufferCopyBufferToBuffer(&command,base,base+524288,262148,0);BAD(E2BIG);
    /* Raw signed32 setter metadata is independent of copy consumption. The
     * SDK stores w1, including low-word truncation of ptrdiff_t arguments. */
    PFNNVNCOMMANDBUFFERGETCOPYROWSTRIDEPROC row_get=(PFNNVNCOMMANDBUFFERGETCOPYROWSTRIDEPROC)nvnBootstrapLoader("nvnCommandBufferGetCopyRowStride");
    PFNNVNCOMMANDBUFFERGETCOPYIMAGESTRIDEPROC image_get=(PFNNVNCOMMANDBUFFERGETCOPYIMAGESTRIDEPROC)nvnBootstrapLoader("nvnCommandBufferGetCopyImageStride");
    CHECK(row_get&&image_get);
    CHECK(!row_get(NULL));BAD(EINVAL);CHECK(!image_get(NULL));BAD(EINVAL);
    const ptrdiff_t stride_args[]={-4,3,(ptrdiff_t)INT64_C(0x100000040),
        (ptrdiff_t)INT64_C(0x180000003),0};
    const ptrdiff_t stride_values[]={-4,3,64,-INT64_C(2147483645),0};
    for(unsigned i=0;i<sizeof(stride_args)/sizeof(*stride_args);i++){
        api_nvnCommandBufferSetCopyRowStride(&command,stride_args[i]);GOOD();
        api_nvnCommandBufferSetCopyImageStride(&command,stride_args[i]);GOOD();
        CHECK(rec->copy_row_stride==stride_values[i]&&rec->copy_image_stride==stride_values[i]);
        CHECK(row_get(&command)==(ptrdiff_t)(uint32_t)stride_values[i]);
        CHECK(image_get(&command)==(ptrdiff_t)(uint32_t)stride_values[i]);GOOD();
        error(EIO);CHECK(row_get(&command)==(ptrdiff_t)(uint32_t)stride_values[i]);BAD(EIO);
        CHECK(rec->used==used&&rec->current->count==refs&&find(&device,DEVICE)->gpu->bos==bos);
    }
    for(unsigned i=0;i<2;i++){
        api_nvnCommandBufferSetCopyRowStride(&command,stride_args[i]);GOOD();
        api_nvnCommandBufferCopyBufferToTexture(&command,base+32768,&target,NULL,&r,0);BAD(EINVAL);
        CHECK(rec->used==used&&rec->current->count==refs&&find(&device,DEVICE)->gpu->bos==bos);
    }
    api_nvnCommandBufferSetCopyRowStride(&command,64);GOOD();
    api_nvnCommandBufferSetCopyImageStride(&command,0);GOOD();
    NVNcopyRegion invalid=r;invalid.xoffset=63;api_nvnCommandBufferCopyTextureToTexture(&command,&source,NULL,&invalid,&target,NULL,&r,0);BAD(EINVAL);
    invalid=r;invalid.depth=2;api_nvnCommandBufferCopyTextureToTexture(&command,&source,NULL,&invalid,&target,NULL,&r,0);BAD(EINVAL);
    fail_alloc=1;api_nvnCommandBufferCopyBufferToBuffer(&command,base+32768,base+65536,64,0);BAD(ENOMEM);CHECK(rec->used==used&&find(&device,DEVICE)->gpu->bos==bos);
    fail_cache=1;api_nvnCommandBufferCopyBufferToBuffer(&command,base+32768,base+65536,64,0);BAD(EIO);CHECK(rec->used==used&&find(&device,DEVICE)->gpu->bos==bos);
    fail_cache=1;fail_destroy=1;api_nvnCommandBufferCopyBufferToBuffer(&command,base+32768,base+65536,64,0);BAD(EIO);CHECK(find(&device,DEVICE)->retired);reap_jobs(find(&device,DEVICE));GOOD();CHECK(find(&device,DEVICE)->gpu->bos==bos);
    api_nvnCommandBufferCopyBufferToBuffer(&command,base+32768,base+65536,64,0);GOOD();discard();GOOD();
    /* Duplicate imports of the exact same caller pages cannot evade overlap
     * checks merely by giving the pool/BO a second host identity. */
    NVNmemoryPool alias;NVNmemoryPoolBuilder ab;api_nvnMemoryPoolBuilderSetDefaults(&ab);api_nvnMemoryPoolBuilderSetDevice(&ab,&device);api_nvnMemoryPoolBuilderSetStorage(&ab,storage,1048576);api_nvnMemoryPoolBuilderSetFlags(&ab,NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);CHECK(api_nvnMemoryPoolInitialize(&alias,&ab));GOOD();
    struct resource_span a={find(&pool,POOL),NULL,0,16,16,1},b={find(&alias,POOL),NULL,4,16,16,1};CHECK(spans_disjoint(&a,&b)==EINVAL);b.offset=4096;CHECK(!spans_disjoint(&a,&b));
    /* An otherwise-idle source BO cannot hide a busy duplicate import. */
    struct nn_external_image_view unused; (void)unused;
    NVNcopyRegion rr={.width=4,.height=1,.depth=1};unsigned attempts=submits;find(&alias,POOL)->bo->busy++;
    api_nvnTextureReadTexels(&source,NULL,&rr,output);BAD(EBUSY);CHECK(submits==attempts);find(&alias,POOL)->bo->busy--;
    api_nvnMemoryPoolFinalize(&alias);GOOD();return 0;
}
static int clears_updates(void)
{
    NVNcopyRegion r={.width=4,.height=2,.depth=1};float red[4]={1,0,0,1};begin();
    api_nvnCommandBufferClearTexture(&command,&target,NULL,&r,red,15);GOOD();uint32_t *w=child();CHECK(w[27]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,6));CHECK(w[30]==0xff0000ffu && w[33]==0xff0000ffu);discard();GOOD();
    begin();size_t used=find(&command,COMMAND)->record->used;api_nvnCommandBufferClearTexture(&command,&target,NULL,&r,red,7);BAD(ENOTSUP);CHECK(find(&command,COMMAND)->record->used==used);
    uint32_t color[4]={2,4,6,8};api_nvnCommandBufferClearTextureui(&command,&target,NULL,&r,color,15);BAD(ENOTSUP);
    NVNtexture integer;CHECK(texture(&integer,32768,NVN_FORMAT_RGBA8UI,64,64,256));api_nvnCommandBufferClearTextureui(&command,&integer,NULL,&r,color,15);GOOD();w=child();CHECK(w[30]==0x08060402u);discard();GOOD();api_nvnTextureFinalize(&integer);GOOD();
    uint32_t data[4]={0x10203040,0x50607080,0x90a0b0c0,0xd0e0f001};uint64_t address=api_nvnMemoryPoolGetBufferAddress(&pool)+65536;begin();api_nvnCommandBufferUpdateUniformBuffer(&command,address,64,16,sizeof(data),data);GOOD();memset(data,0,sizeof(data));w=child();CHECK(w[30]==0x10203040&&w[33]==0xd0e0f001);discard();GOOD();
    begin();used=find(&command,COMMAND)->record->used;api_nvnCommandBufferUpdateUniformBuffer(&command,address,64,60,8,data);BAD(EINVAL);CHECK(find(&command,COMMAND)->record->used==used);api_nvnCommandBufferDownsampleTextureView(&command,&source,NULL,&target,NULL);BAD(ENOTSUP);api_nvnCommandBufferUpdateUniformBuffer(&command,address,64,0,16,data);GOOD();discard();GOOD();return 0;
}
static int reads(void)
{
    NVNcopyRegion r={.width=4,.height=3,.depth=1};unsigned before=submits,im=imports,fin=finishes;
    memset(output,0xab,16384);api_nvnTextureReadTexelsStrided(&source,NULL,&r,output,64,4096);GOOD();
    CHECK(submits==before+1&&imports==im+1&&finishes==fin+1);CHECK(observed[27]==nvn_pm4_packet7(NVN_CP_MEM_TO_MEM,5));CHECK(pair_address(observed+29)==(uintptr_t)output);CHECK(pair_address(observed+31)==(uintptr_t)storage);CHECK(pair_address(observed+29+24)==(uintptr_t)output+64);
    /* RAM backend did not execute packets; no implementation CPU copy wrote it. */
    for(unsigned i=0;i<16384;i++)CHECK(((unsigned char *)output)[i]==0xab);
    CHECK(!retained_readbacks);CHECK(!find(&source,TEXTURE)->refs);
    nn_bo *bo=find(&pool,POOL)->bo;bo->busy++;before=submits;im=imports;fin=finishes;api_nvnTextureReadTexels(&source,NULL,&r,output);BAD(EBUSY);CHECK(submits==before&&imports==im&&finishes==fin);bo->busy--;
    api_nvnTextureReadTexels(&source,NULL,&r,storage);BAD(EINVAL);CHECK(submits==before);
    CHECK(!mprotect(output,16384,PROT_READ));api_nvnTextureReadTexels(&source,NULL,&r,output);BAD(ENOTSUP);CHECK(!mprotect(output,16384,PROT_READ|PROT_WRITE));CHECK(submits==before);
    api_nvnTextureReadTexelsStrided(&source,NULL,&r,output,8,0);BAD(EINVAL);
    api_nvnTextureReadTexelsStrided(&source,NULL,&r,(char *)output+1,64,0);BAD(EINVAL);
    fail_import=1;api_nvnTextureReadTexels(&source,NULL,&r,output);BAD(ENOMEM);CHECK(!retained_readbacks&&!find(&source,TEXTURE)->refs);
    fail_cache=1;api_nvnTextureReadTexels(&source,NULL,&r,output);BAD(EIO);CHECK(!retained_readbacks&&!find(&source,TEXTURE)->refs);
    /* Uncertain submit and wait retain original source/target/queue/IB until
     * process exit; exercise in children so leak sanitizers retain meaning. */
    for(unsigned i=0;i<2;i++){
        pid_t pid=fork();CHECK(pid>=0);if(!pid){if(i)fail_finish=1;else fail_submit=1;api_nvnTextureReadTexels(&source,NULL,&r,output);assert(nn_nvn_error()==(i?ETIMEDOUT:EDEADLK));assert(retained_readbacks&&find(&source,TEXTURE)->refs==1);nn_nvn_clear_error();api_nvnTextureFinalize(&source);assert(nn_nvn_error()==EBUSY);_exit(0);}int status;CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);
    }
    return 0;
}
int main(void)
{
    CHECK(!posix_memalign(&storage,4096,1048576));CHECK(!posix_memalign(&output,4096,16384));memset(storage,0xa5,1048576);
    api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&device,&db));GOOD();
    api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&device);api_nvnMemoryPoolBuilderSetStorage(&pb,storage,1048576);api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);CHECK(api_nvnMemoryPoolInitialize(&pool,&pb));GOOD();
    CHECK(texture(&source,0,NVN_FORMAT_RGBA8,64,64,256));CHECK(texture(&target,16384,NVN_FORMAT_RGBA8,64,64,256));GOOD();
    CHECK(api_nvnCommandBufferInitialize(&command,&device));api_nvnCommandBufferAddCommandMemory(&command,&pool,393216,65536);BAD(ENOTSUP); /* CPU_NO_ACCESS arena is forbidden. */
    NVNmemoryPool arena;NVNmemoryPoolBuilder ap;void *command_storage;CHECK(!posix_memalign(&command_storage,4096,65536));
    api_nvnMemoryPoolBuilderSetDefaults(&ap);api_nvnMemoryPoolBuilderSetDevice(&ap,&device);api_nvnMemoryPoolBuilderSetStorage(&ap,command_storage,65536);api_nvnMemoryPoolBuilderSetFlags(&ap,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED);CHECK(api_nvnMemoryPoolInitialize(&arena,&ap));api_nvnCommandBufferAddCommandMemory(&command,&arena,0,65536);api_nvnCommandBufferAddControlMemory(&command,control,sizeof(control));GOOD();
    CHECK(!empty_recordings());CHECK(!metadata());CHECK(!copies());CHECK(!clears_updates());CHECK(!reads());
    /* All source/dest image bytes stayed CPU-untouched throughout recording and
     * RAM submits. This is not a GPU pixel oracle or hardware qualification. */
    for(unsigned i=0;i<32768;i++)CHECK(((unsigned char *)storage)[i]==0xa5);
    api_nvnTextureFinalize(&source);api_nvnTextureFinalize(&target);api_nvnCommandBufferFinalize(&command);GOOD();api_nvnMemoryPoolFinalize(&pool);api_nvnMemoryPoolFinalize(&arena);GOOD();api_nvnDeviceFinalize(&device);GOOD();
    while(objects){CHECK(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}CHECK(!handles);free(storage);free(output);free(command_storage);
    printf("{\"passed\":true,\"checks\":%u,\"families\":4,\"api_names\":22,\"ram_gpu_executes\":0,\"cpu_image_copies\":0,\"uncertain_children\":2}\n",checks);return 0;
}
