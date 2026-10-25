/* Actual NVN core/external module plus synthetic session/RAM backend.
 * No Android compositor, dma-buf, GPU commands or real game execution. */
#define _GNU_SOURCE
#define NN_HOST_TEST_REAL_WINDOW_SESSION 1
#define nn_bo_import_dmabuf host_unused_dmabuf
#define nn_queue_submit host_unused_submit
#define nn_queue_finish host_memory_finish
#include "memory_backend.c"
#undef nn_bo_import_dmabuf
#undef nn_queue_submit
#undef nn_queue_finish
int nn_bo_import_dmabuf(nn_device *,int,size_t,nn_bo **);
int nn_queue_finish(nn_queue *,uint32_t);
int nn_queue_submit(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,uint32_t *);
#include "../../src/nvn.c"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks, releases, creates, enqueues, finishes, submissions;
static int create_error, finish_error, retire_error, destroy_error, acquire_error;
static unsigned last_finish_for_enqueue;
static const unsigned char token_identity;
struct nn_window_session {
    struct nn_window_slot slots[3]; struct nn_window_ticket tickets[3];
    unsigned states[3], count, cursor; int current, retired;
    uint64_t serial; struct nn_window_session_stats stats;
};
static nn_window_session *latest_session;
int nn_bo_import_dmabuf(nn_device *device,int fd,size_t bytes,nn_bo **out)
{ assert(fd>=0); return nn_bo_alloc(device,bytes,out); }
int nn_queue_finish(nn_queue *queue,uint32_t timeout)
{ finishes++;if(finish_error)return finish_error;return host_memory_finish(queue,timeout); }
int nn_queue_submit(nn_queue *queue,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
    (void)offset;assert(bytes&&count+1<=64-queue->count);submissions++;
    queue->pending[queue->count++]=ib;ib->busy++;
    for(size_t i=0;i<count;i++){queue->pending[queue->count++]=refs[i];refs[i]->busy++;}
    *timestamp=submissions;return 0;
}
int nn_window_session_create(const void *token,const struct nn_window_slot *slots,uint32_t count,nn_window_session **out)
{
    *out=NULL;creates++;if(token!=&token_identity)return EINVAL;if(create_error)return create_error;
    assert(count==2||count==3);nn_window_session *s=calloc(1,sizeof(*s));assert(s);s->count=count;s->current=-1;
    for(unsigned i=0;i<count;i++){assert(slots[i].backing_inode&&slots[i].width==64&&slots[i].pitch==256);s->slots[i]=slots[i];}
    *out=s;latest_session=s;return 0;
}
int nn_window_session_acquire(nn_window_session *s,uint32_t timeout,struct nn_window_ticket *out)
{
    assert(timeout==1500);memset(out,0,sizeof(*out));if(acquire_error)return acquire_error;
    for(unsigned i=0;i<s->count;i++){unsigned n=(i+s->cursor)%s->count;if(s->states[n]!=NN_WINDOW_FREE)continue;
        s->states[n]=NN_WINDOW_ACQUIRED;s->cursor=(n+1)%s->count;
        *out=s->tickets[n]=(struct nn_window_ticket){1,++s->serial,n};return 0;}
    return EBUSY;
}
int nn_window_session_check_acquired(nn_window_session *s,const struct nn_window_ticket *ticket)
{
    if(ticket->index>=s->count||ticket->generation!=1||!ticket->serial||ticket->serial!=s->tickets[ticket->index].serial)return ESTALE;
    return s->states[ticket->index]==NN_WINDOW_ACQUIRED?0:EBUSY;
}
int nn_window_session_enqueue_ready(nn_window_session *s,const struct nn_window_ticket *ticket)
{
    assert(!nn_window_session_check_acquired(s,ticket));assert(finishes>last_finish_for_enqueue);
    last_finish_for_enqueue=finishes;s->states[ticket->index]=NN_WINDOW_QUEUED;enqueues++;return 0;
}
int nn_window_session_get_stats(nn_window_session *s,struct nn_window_session_stats *out)
{ memset(out,0,sizeof(*out));out->slots=s->count;out->current_index=s->current;for(unsigned i=0;i<s->count;i++)out->state[i]=s->states[i];return 0; }
int nn_window_session_retire(nn_window_session *s,uint32_t timeout)
{ assert(timeout==1500);if(retire_error)return retire_error;s->retired=1;return 0; }
int nn_window_session_destroy(nn_window_session *s)
{ if(destroy_error)return destroy_error;assert(s->retired);free(s);latest_session=NULL;return 0; }
/* This historical session mock supports interval1 only; production NDK
 * linkage below uses the real interval0..5 platform implementation. */
int nn_window_session_interval_supported(int interval)
{return interval==1?0:ENOTSUP;}
int nn_window_session_create_interval(const void *token,const struct nn_window_slot *slots,uint32_t count,int interval,nn_window_session **out)
{int e=nn_window_session_interval_supported(interval);if(e){if(out)*out=NULL;return e;}return nn_window_session_create(token,slots,count,out);}
int nn_window_session_set_present_interval(nn_window_session *session,int interval)
{if(!session)return EINVAL;return nn_window_session_interval_supported(interval);}
static void fake_worker_show(nn_window_session *s,unsigned index)
{
    assert(s->states[index]==NN_WINDOW_QUEUED);
    if(s->current>=0)s->states[s->current]=NN_WINDOW_FREE;
    s->current=(int)index;s->states[index]=NN_WINDOW_CURRENT;
}
struct owner { int fd; unsigned released; };
static int owner_release(void *pointer)
{ struct owner *o=pointer;assert(!o->released);o->released++;releases++;assert(!close(o->fd));return 0; }
static struct nn_external_image_desc image_desc(struct owner *owner)
{
    char path[]="/tmp/nn-window-mock-XXXXXX";owner->fd=mkstemp(path);assert(owner->fd>=0);assert(!unlink(path));assert(!ftruncate(owner->fd,32768));
    return (struct nn_external_image_desc){.fd=owner->fd,.fd_bytes=32768,.pixel_offset=4096,.payload_bytes=16384,
        .width=64,.height=64,.pitch=256,.layers=1,.format=NN_EXTERNAL_RGBA8,.usage=819,
        .display_buffer=owner,.owner_cookie=owner,.release_owner=owner_release};
}
static void texture_builder(NVNtextureBuilder *b,NVNdevice *device,NVNmemoryPool *pool)
{
    api_nvnTextureBuilderSetDefaults(b);api_nvnTextureBuilderSetDevice(b,device);
    api_nvnTextureBuilderSetFlags(b,NVN_TEXTURE_FLAGS_LINEAR);api_nvnTextureBuilderSetTarget(b,NVN_TEXTURE_TARGET_2D);
    api_nvnTextureBuilderSetSize2D(b,64,64);api_nvnTextureBuilderSetLevels(b,1);api_nvnTextureBuilderSetSamples(b,0);
    api_nvnTextureBuilderSetFormat(b,NVN_FORMAT_RGBA8);api_nvnTextureBuilderSetStride(b,256);api_nvnTextureBuilderSetStorage(b,pool,4096);
}
static struct object *peek(const void *key)
{ for(struct object *o=objects;o;o=o->next)if(o->key==key)return o;return NULL; }
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d %s sticky=%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define CLEAN() CHECK(!nn_nvn_error())
#define ERROR(e) do{CHECK(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static NVNcommandHandle fake_target_handle(struct object *device,struct object *commands,struct object *target)
{
    /* Metadata only: no original/generated draw or command execution. */
    struct handle *h=calloc(1,sizeof(*h));assert(h);h->device=device;device->refs++;
    h->commands=commands;commands->refs++;h->bytes=16;h->id=next_handle++;
    assert(!add_resource(h,target)&&!add_resource(h,target->pool));h->next=handles;handles=h;return h->id;
}
static int cases(unsigned count)
{
    NVNdeviceBuilder db={0};NVNdevice device={0},dead={0};NVNqueueBuilder qb={0};NVNqueue queue={0},other={0};
    api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&device,&db));
    api_nvnQueueBuilderSetDefaults(&qb);api_nvnQueueBuilderSetDevice(&qb,&device);
    CHECK(api_nvnQueueInitialize(&queue,&qb));CHECK(api_nvnQueueInitialize(&other,&qb));CLEAN();
    NVNmemoryPool pools[3];NVNtexture textures[3];NVNtextureBuilder tb[3];struct owner owners[3]={0};
    for(unsigned i=0;i<count;i++){struct nn_external_image_desc desc=image_desc(owners+i);CHECK(!nn_nvn_import_image_pool(&device,pools+i,&desc));texture_builder(tb+i,&device,pools+i);CHECK(api_nvnTextureInitialize(textures+i,tb+i));}CLEAN();
    NVNwindowBuilder wb={0};NVNwindow window={0};NVNtexture *array[3]={textures,textures+1,textures+2};
    api_nvnWindowBuilderSetDevice(&wb,&device);api_nvnWindowBuilderSetDefaults(&wb);CLEAN();
    CHECK(peek(&wb)->device==peek(&device)&&api_nvnWindowBuilderGetPresentInterval(&wb)==1);
    api_nvnWindowBuilderSetNativeWindow(&wb,(void *)&token_identity);api_nvnWindowBuilderSetTextures(&wb,(int)count,array);CLEAN();
    CHECK(api_nvnWindowBuilderGetDevice(&wb)==&device);
    CHECK(api_nvnWindowBuilderGetNumTextures(&wb)==(int)count);
    for(unsigned i=0;i<count;i++)CHECK(api_nvnWindowBuilderGetTexture(&wb,(int)i)==textures+i);
    CHECK(!api_nvnWindowBuilderGetTexture(&wb,-1));ERROR(EINVAL);
    CHECK(!api_nvnWindowBuilderGetTexture(&wb,(int)count));ERROR(EINVAL);
    array[0]=NULL; /* SDK stores the array pointer until Initialize. */
    CHECK(!api_nvnWindowBuilderGetTexture(&wb,0));CLEAN();
    unsigned initial_creates=creates;CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(EINVAL);
    CHECK(creates==initial_creates&&!peek(&window));array[0]=textures;
    create_error=ENOMEM;CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(ENOMEM);create_error=0;
    CHECK(!peek(&window)&&!latest_session);
    for(unsigned i=0;i<count;i++)CHECK(peek(textures+i)->refs==0&&peek(pools+i)->refs==1);
    CHECK(api_nvnDeviceInitialize(&dead,&db));api_nvnWindowBuilderSetDevice(&wb,&dead);api_nvnDeviceFinalize(&dead);CLEAN();
    unsigned before=creates;CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(EINVAL);CHECK(creates==before&&!peek(&window));
    api_nvnWindowBuilderSetDevice(&wb,&device);
    api_nvnTextureFinalize(textures);CLEAN();CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(EINVAL);
    CHECK(api_nvnTextureInitialize(textures,tb));CLEAN();
    /* Different BO addresses and fake AHB/owner pointers can still alias the
     * same verified FD storage. Only actual fstat identity catches this. */
    struct owner alias={0};struct nn_external_image_desc alias_desc=image_desc(&alias);
    CHECK(!close(alias.fd));alias.fd=dup(owners[0].fd);CHECK(alias.fd>=0);alias_desc.fd=alias.fd;
    NVNmemoryPool alias_pool={0};NVNtexture alias_texture={0};NVNtextureBuilder alias_builder={0};
    CHECK(!nn_nvn_import_image_pool(&device,&alias_pool,&alias_desc));texture_builder(&alias_builder,&device,&alias_pool);
    CHECK(api_nvnTextureInitialize(&alias_texture,&alias_builder));CLEAN();
    NVNtexture *aliases[3]={textures,&alias_texture,textures+2};api_nvnWindowBuilderSetTextures(&wb,(int)count,aliases);CLEAN();
    before=creates;CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(EINVAL);CHECK(creates==before&&!peek(&window));
    api_nvnTextureFinalize(&alias_texture);api_nvnMemoryPoolFinalize(&alias_pool);CLEAN();CHECK(alias.released==1);
    NVNtexture *originals[3]={textures,textures+1,textures+2};api_nvnWindowBuilderSetTextures(&wb,(int)count,originals);
    api_nvnWindowBuilderSetTextures(&wb,1,originals);ERROR(ENOTSUP);CHECK(peek(&wb)->window_builder.count==(int)count);
    api_nvnWindowBuilderSetNativeWindow(&wb,(void *)0x1234);CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(EINVAL);CHECK(!peek(&window));
    api_nvnWindowBuilderSetNativeWindow(&wb,(void *)&token_identity);
    union { NVNwindowBuilder b; NVNwindow w; } alias_window;
    api_nvnWindowBuilderSetDevice(&alias_window.b,&device);api_nvnWindowBuilderSetDefaults(&alias_window.b);
    api_nvnWindowBuilderSetNativeWindow(&alias_window.b,(void *)&token_identity);
    api_nvnWindowBuilderSetTextures(&alias_window.b,(int)count,originals);CLEAN();
    CHECK(api_nvnWindowInitialize(&alias_window.w,&alias_window.b));CLEAN();
    CHECK(api_nvnWindowGetNumTextures(&alias_window.w)==(int)count);
    CHECK(api_nvnWindowGetPresentInterval(&alias_window.w)==1);
    for(unsigned i=0;i<count;i++)CHECK(peek(textures+i)->refs==1&&peek(pools+i)->refs==2);
    api_nvnWindowFinalize(&alias_window.w);CLEAN();CHECK(!peek(&alias_window.w));
    for(unsigned i=0;i<count;i++)CHECK(peek(textures+i)->refs==0&&peek(pools+i)->refs==1);
    CHECK(api_nvnWindowInitialize(&window,&wb));CLEAN();
    originals[0]=NULL; /* Successful live Window owns a snapshot, not the array. */
    CHECK(api_nvnWindowGetNumTextures(&window)==(int)count);
    CHECK(api_nvnWindowBuilderGetNumTextures(&wb)==(int)count);
    CHECK(!api_nvnWindowBuilderGetTexture(&wb,0));CLEAN();
    CHECK(peek(&window)->window->textures[0]==peek(textures));
    CHECK(!peek(&window)->window_builder.textures);
    for(unsigned i=0;i<count;i++)CHECK(peek(&window)->window->textures[i]==peek(textures+i));
    CHECK(api_nvnWindowGetNativeWindow(&window)==(void *)&token_identity&&api_nvnWindowGetPresentInterval(&window)==1);
    for(unsigned i=0;i<count;i++){CHECK(peek(textures+i)->refs==1&&peek(pools+i)->refs==2);api_nvnTextureFinalize(textures+i);ERROR(EBUSY);api_nvnMemoryPoolFinalize(pools+i);ERROR(EBUSY);}
    api_nvnWindowSetPresentInterval(&window,0);ERROR(ENOTSUP);api_nvnWindowSetCrop(&window,0,0,32,64);ERROR(ENOTSUP);
    NVNrectangle rectangle={0};api_nvnWindowGetCrop(&window,&rectangle);CHECK(rectangle.width==64&&rectangle.height==64);CLEAN();
    struct nn_window_session_stats stats;last_error=E2BIG;
    CHECK(!nn_nvn_window_get_stats(&window,&stats)&&nn_nvn_error()==E2BIG&&stats.slots==count);
    CHECK(nn_nvn_window_get_stats((void *)0x1234,&stats)==EINVAL&&nn_nvn_error()==E2BIG);nn_nvn_clear_error();
    NVNsync sync={0};CHECK(api_nvnSyncInitialize(&sync,&device));
    /* An initialized, unfenced object is not a completed GPU fence. */
    CHECK(api_nvnSyncWait(&sync,0)==NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED);CLEAN();
    int index=123;acquire_error=ECANCELED;
    CHECK(api_nvnWindowAcquireTexture(&window,&sync,&index)==NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR);ERROR(ECANCELED);CHECK(index==-1&&!peek(&sync)->availability);acquire_error=0;
    void *command_storage=NULL;CHECK(!posix_memalign(&command_storage,4096,4096));memset(command_storage,0,4096);
    NVNmemoryPoolBuilder pb={0};NVNmemoryPool commands={0};api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&device);api_nvnMemoryPoolBuilderSetStorage(&pb,command_storage,4096);api_nvnMemoryPoolBuilderSetFlags(&pb,0x24);CHECK(api_nvnMemoryPoolInitialize(&commands,&pb));
    NVNcommandHandle handle=fake_target_handle(peek(&device),peek(&commands),peek(textures));before=submissions;
    api_nvnQueueSubmitCommands(&queue,1,&handle);CHECK(nn_nvn_error()!=0);nn_nvn_clear_error();CHECK(submissions==before&&!peek(&window)->window->producer);
    CHECK(api_nvnWindowAcquireTexture(&window,&sync,&index)==NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_SUCCESS&&index==0);CLEAN();
    CHECK(api_nvnSyncWait(&sync,UINT64_MAX)==NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED);CHECK(api_nvnQueueWaitSync(&queue,&sync));CLEAN();
    NVNcommandBuffer cb={0};CHECK(api_nvnCommandBufferInitialize(&cb,&device));api_nvnCommandBufferAddCommandMemory(&cb,&commands,0,4096);_Alignas(struct control) unsigned char control[256];api_nvnCommandBufferAddControlMemory(&cb,control,sizeof(control));api_nvnCommandBufferBeginRecording(&cb);
    api_nvnCommandBufferWaitSync(&cb,&sync);CLEAN();CHECK(api_nvnCommandBufferGetCommandMemoryUsed(&cb)==0);api_nvnCommandBufferFinalize(&cb);CLEAN();
    NVNcommandHandle unavailable=fake_target_handle(peek(&device),peek(&commands),peek(textures+1));
    NVNcommandHandle pair[2]={handle,unavailable};api_nvnQueueSubmitCommands(&queue,2,pair);
    CHECK(nn_nvn_error()!=0);nn_nvn_clear_error();CHECK(submissions==before&&!peek(&window)->window->producer);
    api_nvnDeviceFinalizeCommandHandle(&device,unavailable);CLEAN();
    api_nvnQueueSubmitCommands(&queue,1,&handle);CLEAN();CHECK(submissions==before+1&&peek(&queue)->refs==1);
    api_nvnQueueFinalize(&queue);ERROR(EBUSY);CHECK(peek(&queue));
    api_nvnQueueSubmitCommands(&other,1,&handle);ERROR(EBUSY);CHECK(submissions==before+1);
    unsigned previous_enqueues=enqueues;api_nvnQueuePresentTexture(&queue,&window,index);CLEAN();CHECK(enqueues==previous_enqueues+1);
    unsigned before_finish=finishes;api_nvnQueuePresentTexture(&queue,&window,index);ERROR(EBUSY);
    CHECK(finishes==before_finish&&enqueues==previous_enqueues+1);
    before=submissions;api_nvnQueueSubmitCommands(&queue,1,&handle);ERROR(EBUSY);CHECK(submissions==before);
    fake_worker_show(latest_session,0);api_nvnQueueSubmitCommands(&queue,1,&handle);ERROR(EBUSY);CHECK(submissions==before);
    int second=-1;CHECK(api_nvnQueueAcquireTexture(&other,&window,&second)==NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR);ERROR(EINVAL);
    CHECK(api_nvnQueueAcquireTexture(&queue,&window,&second)==NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS&&second==1);CLEAN();
    api_nvnQueuePresentTexture(&queue,&window,second);CLEAN();fake_worker_show(latest_session,1);
    CHECK(latest_session->states[0]==NN_WINDOW_FREE);
    /* Acquisition of another slot does not authorize replay into old FREE0. */
    api_nvnQueueSubmitCommands(&queue,1,&handle);ERROR(EBUSY);CHECK(submissions==before);
    if(count==3){int third=-1;CHECK(api_nvnQueueAcquireTexture(&queue,&window,&third)==NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS&&third==2);CLEAN();}
    CHECK(api_nvnWindowAcquireTexture(&window,NULL,&index)==NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_SUCCESS&&index==0);CLEAN();
    api_nvnQueueSubmitCommands(&queue,1,&handle);CLEAN();CHECK(submissions==before+1);
    /* Finalize must finish even unpresented acquired writes. */
    finish_error=ETIMEDOUT;api_nvnWindowFinalize(&window);ERROR(ETIMEDOUT);CHECK(peek(&window)&&peek(&queue)->refs==1);
    for(unsigned i=0;i<count;i++)CHECK(peek(pools+i)->refs==2+(i==0));
    finish_error=0;retire_error=EBUSY;api_nvnWindowFinalize(&window);ERROR(EBUSY);CHECK(peek(&window)&&latest_session);
    retire_error=0;destroy_error=EBUSY;api_nvnWindowFinalize(&window);ERROR(EBUSY);CHECK(peek(&window)&&latest_session);
    destroy_error=0;api_nvnWindowFinalize(&window);CLEAN();CHECK(!peek(&window)&&!latest_session&&peek(&queue)->refs==0);
    api_nvnDeviceFinalizeCommandHandle(&device,handle);api_nvnSyncFinalize(&sync);CLEAN();
    for(unsigned i=0;i<count;i++){CHECK(peek(textures+i)->refs==0&&peek(pools+i)->refs==1&&!peek(textures+i)->window_owner);api_nvnTextureFinalize(textures+i);api_nvnMemoryPoolFinalize(pools+i);CLEAN();CHECK(owners[i].released==1);}
    api_nvnMemoryPoolFinalize(&commands);CLEAN();free(command_storage);
    api_nvnQueueFinalize(&queue);api_nvnQueueFinalize(&other);CLEAN();api_nvnDeviceFinalize(&device);CLEAN();
    while(objects){CHECK(objects->kind==DB||objects->kind==QB||objects->kind==WB||objects->kind==TB||objects->kind==PB);remove_object(objects);}
    return 0;
}
int main(void)
{
    CHECK(!cases(2));CHECK(!cases(3));
    CHECK(nvnBootstrapLoader("nvnQueueFenceSync")&&!nvnBootstrapLoader("nvnCommandBufferFenceSync"));
    printf("{\"passed\":true,\"scope\":\"host core/resource/session mocks only; no GPU/compositor/game\",\"checks\":%u,\"session_create_calls\":%u,\"owner_releases\":%u,\"submissions_mocked\":%u,\"enqueue_calls\":%u,\"finish_calls\":%u}\n",checks,creates,releases,submissions,enqueues,finishes);
    return 0;
}
