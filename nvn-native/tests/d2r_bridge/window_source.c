#ifndef NN_WINDOW_MEMORY_BACKEND
#define NN_WINDOW_MEMORY_BACKEND "memory_backend.c"
#endif
#ifndef NN_WINDOW_CORE_SOURCE
#define NN_WINDOW_CORE_SOURCE "../../src/nvn.c"
#endif
#ifndef NN_WINDOW_BRIDGE_BUNDLE
#error "Provide the verified generated texture bridge bundle header"
#endif
/* Actual NVN core/external module plus synthetic session/RAM backend.
 * No Android compositor, dma-buf, GPU commands or real game execution. */
#define _GNU_SOURCE
#define NN_HOST_TEST_REAL_WINDOW_SESSION 1
#define NN_HOST_TEST_REAL_WINDOW_DISPLAY 1
#define nn_bo_map host_memory_map
#define nn_bo_import_dmabuf host_unused_dmabuf
#define nn_queue_submit host_unused_submit
#define nn_queue_finish host_memory_finish
#include NN_WINDOW_MEMORY_BACKEND
#undef nn_bo_map
#undef nn_bo_import_dmabuf
#undef nn_queue_submit
#undef nn_queue_finish
static nn_bo *blocked_source,*blocked_display[16];static unsigned blocked_display_count;
void *nn_bo_map(const nn_bo *b){assert(b!=blocked_source);for(unsigned i=0;i<blocked_display_count;i++)assert(b!=blocked_display[i]);return host_memory_map(b);}
int nn_bo_import_dmabuf(nn_device *,int,size_t,nn_bo **);
int nn_queue_finish(nn_queue *,uint32_t);
int nn_queue_submit(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,uint32_t *);
#include NN_WINDOW_CORE_SOURCE
#include NN_WINDOW_BRIDGE_BUNDLE
#include "ahb.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks, releases, creates, enqueues, finishes, submissions;
static int create_error, finish_error, retire_error, destroy_error, acquire_error;
static unsigned last_finish_for_enqueue;
static nn_queue *failed_conversion_queue;static int conversion_submit_error,conversion_finish_error,display_create_error,display_create_retained,display_destroy_error;
static unsigned display_creates,display_destroys;
static int interval_error;
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
{ finishes++;if(finish_error)return finish_error;if(queue==failed_conversion_queue&&conversion_finish_error)return conversion_finish_error;return host_memory_finish(queue,timeout); }
int nn_queue_submit(nn_queue *queue,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
    (void)offset;assert(bytes&&count+1<=64-queue->count);submissions++;
    if(queue==failed_conversion_queue&&conversion_submit_error)return conversion_submit_error;
    queue->pending[queue->count++]=ib;ib->busy++;
    for(size_t i=0;i<count;i++){queue->pending[queue->count++]=refs[i];refs[i]->busy++;}
    *timestamp=submissions;return 0;
}
int nn_window_session_create(const void *token,const struct nn_window_slot *slots,uint32_t count,nn_window_session **out)
{
    *out=NULL;creates++;if(token!=&token_identity)return EINVAL;if(create_error)return create_error;
    assert(count==2||count==3);nn_window_session *s=calloc(1,sizeof(*s));assert(s);s->count=count;s->current=-1;
    for(unsigned i=0;i<count;i++){assert(slots[i].backing_inode&&slots[i].width>0&&slots[i].pitch>=slots[i].width*4);s->slots[i]=slots[i];}
    *out=s;latest_session=s;return 0;
}
int nn_window_session_interval_supported(int interval)
{ return interval<0||interval>5?EINVAL:0; }
int nn_window_session_create_interval(const void *token,const struct nn_window_slot *slots,
    uint32_t count,int interval,nn_window_session **out)
{ assert(interval>=0&&interval<=5);return nn_window_session_create(token,slots,count,out); }
int nn_window_session_set_present_interval(nn_window_session *s,int interval)
{ (void)s;return interval_error?interval_error:nn_window_session_interval_supported(interval); }
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
    /* Match actual window_session.c ticket_check precedence. A valid ticket
     * outside ACQUIRED is EINVAL; only the private READ wrapper translates
     * that exclusion to EBUSY. Producer conflicts have their own EBUSY path. */
    if(!s)return EINVAL;
    if(s->stats.resources_retained)return s->stats.first_error?s->stats.first_error:EBUSY;
    if(s->stats.closing)return ECANCELED;
    if(s->stats.first_error)return s->stats.first_error;
    if(!ticket||ticket->index>=s->count||ticket->generation!=1||!ticket->serial||ticket->serial!=s->tickets[ticket->index].serial)return ESTALE;
    return s->states[ticket->index]==NN_WINDOW_ACQUIRED?0:EINVAL;
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
static int ticket_contract(void)
{
    struct nn_window_session s={0};s.count=2;
    struct nn_window_ticket ticket={1,9,0};s.tickets[0]=ticket;
    CHECK(nn_window_session_check_acquired(NULL,&ticket)==EINVAL);
    CHECK(nn_window_session_check_acquired(&s,NULL)==ESTALE);
    s.states[0]=NN_WINDOW_ACQUIRED;CHECK(!nn_window_session_check_acquired(&s,&ticket));
    struct nn_window_ticket invalid=ticket;invalid.serial=0;CHECK(nn_window_session_check_acquired(&s,&invalid)==ESTALE);
    invalid=ticket;invalid.generation=2;CHECK(nn_window_session_check_acquired(&s,&invalid)==ESTALE);
    invalid=ticket;invalid.index=2;CHECK(nn_window_session_check_acquired(&s,&invalid)==ESTALE);
    const unsigned states[]={NN_WINDOW_QUEUED,NN_WINDOW_CURRENT,NN_WINDOW_FREE};
    for(unsigned i=0;i<3;i++){s.states[0]=states[i];CHECK(nn_window_session_check_acquired(&s,&ticket)==EINVAL);}
    s.stats.closing=1;CHECK(nn_window_session_check_acquired(&s,NULL)==ECANCELED);
    s.stats.first_error=EIO;CHECK(nn_window_session_check_acquired(&s,NULL)==ECANCELED);
    s.stats.resources_retained=1;CHECK(nn_window_session_check_acquired(&s,NULL)==EIO);
    s.stats.first_error=0;CHECK(nn_window_session_check_acquired(&s,NULL)==EBUSY);
    s.stats.resources_retained=s.stats.closing=0;s.stats.first_error=EIO;
    CHECK(nn_window_session_check_acquired(&s,NULL)==EIO);CLEAN();return 0;
}
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
    for(int interval=0;interval<=5;interval++){
        api_nvnWindowBuilderSetPresentInterval(&wb,interval);CLEAN();
        CHECK(api_nvnWindowBuilderGetPresentInterval(&wb)==interval);
    }
    api_nvnWindowBuilderSetPresentInterval(&wb,-1);ERROR(EINVAL);
    CHECK(api_nvnWindowBuilderGetPresentInterval(&wb)==5);
    api_nvnWindowBuilderSetPresentInterval(&wb,6);ERROR(EINVAL);
    api_nvnWindowBuilderSetPresentInterval(&wb,1);CLEAN();
    api_nvnWindowBuilderSetNativeWindow(&wb,(void *)&token_identity);api_nvnWindowBuilderSetTextures(&wb,(int)count,array);CLEAN();
    array[0]=NULL; /* SDK stores the array pointer until Initialize. */
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
    CHECK(api_nvnWindowInitialize(&window,&wb));CLEAN();
    originals[0]=NULL; /* Successful live Window owns a snapshot, not the array. */
    CHECK(peek(&window)->window->textures[0]==peek(textures));
    CHECK(!peek(&window)->window_builder.textures);
    CHECK(api_nvnWindowGetNativeWindow(&window)==(void *)&token_identity&&api_nvnWindowGetPresentInterval(&window)==1);
    for(unsigned i=0;i<count;i++){CHECK(peek(textures+i)->refs==1&&peek(pools+i)->refs==2);api_nvnTextureFinalize(textures+i);ERROR(EBUSY);api_nvnMemoryPoolFinalize(pools+i);ERROR(EBUSY);}
    for(int interval=0;interval<=5;interval++){
        api_nvnWindowSetPresentInterval(&window,interval);CLEAN();
        CHECK(api_nvnWindowGetPresentInterval(&window)==interval);
    }
    api_nvnWindowSetPresentInterval(&window,-1);ERROR(EINVAL);
    CHECK(api_nvnWindowGetPresentInterval(&window)==5);
    api_nvnWindowSetPresentInterval(&window,6);ERROR(EINVAL);
    interval_error=EIO;api_nvnWindowSetPresentInterval(&window,5);CLEAN();
    api_nvnWindowSetPresentInterval(&window,0);ERROR(EIO);
    CHECK(api_nvnWindowGetPresentInterval(&window)==5);interval_error=0;
    api_nvnWindowSetPresentInterval(&window,1);CLEAN();
    api_nvnWindowSetCrop(&window,0,0,32,64);ERROR(ENOTSUP);
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
    unsigned before_finish=finishes;api_nvnQueuePresentTexture(&queue,&window,index);ERROR(EINVAL);
    CHECK(finishes==before_finish&&enqueues==previous_enqueues+1);
    /* The same ticket is now QUEUED: writable qualification propagates
     * the actual session EINVAL before submitting or finishing anything. */
    before=submissions;api_nvnQueueSubmitCommands(&queue,1,&handle);ERROR(EINVAL);CHECK(submissions==before);
    fake_worker_show(latest_session,0);api_nvnQueueSubmitCommands(&queue,1,&handle);ERROR(EINVAL);CHECK(submissions==before);
    int second=-1;CHECK(api_nvnQueueAcquireTexture(&other,&window,&second)==NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR);ERROR(EINVAL);
    CHECK(api_nvnQueueAcquireTexture(&queue,&window,&second)==NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS&&second==1);CLEAN();
    api_nvnQueuePresentTexture(&queue,&window,second);CLEAN();fake_worker_show(latest_session,1);
    CHECK(latest_session->states[0]==NN_WINDOW_FREE);
    /* Acquisition of another slot does not authorize replay into old FREE0.
     * Its ticket still matches, so the non-ACQUIRED error remains EINVAL. */
    api_nvnQueueSubmitCommands(&queue,1,&handle);ERROR(EINVAL);CHECK(submissions==before);
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
/* Fake display ownership only: no AHB pixels are generated or converted. */
struct nn_window_display {nn_bo *bo;struct nn_external_image_view view;unsigned locked;};
static unsigned native_read_locks;
int nn_window_display_create(nn_device *device,uint32_t width,uint32_t height,nn_window_display **out)
{
    *out=NULL;display_creates++;if(display_create_error&&!display_create_retained)return display_create_error;
    struct nn_display_rgba8_shape shape;int e=nn_display_rgba8_shape(width,height,(width*4+63)&~63u,&shape);if(e)return e;
    nn_window_display *d=calloc(1,sizeof(*d));assert(d);e=nn_bo_alloc(device,(size_t)shape.storage_bytes,&d->bo);assert(!e);
    d->view=(struct nn_external_image_view){.bo=d->bo,.fd_bytes=shape.storage_bytes,.payload_bytes=shape.storage_bytes,
        .width=width,.height=height,.pitch=shape.pitch,.format=NN_EXTERNAL_RGBA8,.backing_inode=10000+display_creates,.display_buffer=d};
    assert(blocked_display_count<16);blocked_display[blocked_display_count++]=d->bo;
    *out=d;return display_create_error;
}
int nn_window_display_view(nn_window_display *d,struct nn_external_image_view *out){*out=d->view;return 0;}
int nn_window_display_destroy(nn_window_display *d){if(!d)return 0;if(display_destroy_error)return display_destroy_error;int e=nn_bo_destroy(d->bo);if(e)return e;for(unsigned i=0;i<blocked_display_count;i++)if(blocked_display[i]==d->bo){blocked_display[i]=blocked_display[--blocked_display_count];break;}display_destroys++;free(d);return 0;}
int nn_window_display_pixel(nn_window_display *d,struct nn_ahb_pixel *p){*p=(struct nn_ahb_pixel){.fd=7,.fd_index=0,.bytes=d->view.fd_bytes,.payload_bytes=d->view.payload_bytes,.width=d->view.width,.height=d->view.height,.pitch=d->view.pitch};return 0;}
int nn_window_display_read_lock(nn_window_display *d,struct nn_ahb_view *v){assert(!d->locked);d->locked=1;native_read_locks++;*v=(struct nn_ahb_view){.address=host_memory_map(d->bo)};return 0;}
int nn_window_display_read_unlock(nn_window_display *d){assert(d->locked);d->locked=0;return 0;}
static int source_cases(unsigned count)
{
    NVNdeviceBuilder db={0};NVNdevice device={0};NVNqueueBuilder qb={0};NVNqueue queue={0};
    api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&device,&db));
    api_nvnQueueBuilderSetDefaults(&qb);api_nvnQueueBuilderSetDevice(&qb,&device);CHECK(api_nvnQueueInitialize(&queue,&qb));CLEAN();
    /* Canonical textures share a single caller allocation at independent
     * offsets, matching the actual D2R ownership model. No GPU work executes. */
    enum {WIDTH=2880,HEIGHT=1620};NVNtextureBuilder tb[3];NVNtexture textures[3];NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool={0};
    api_nvnTextureBuilderSetDefaults(tb);api_nvnTextureBuilderSetDevice(tb,&device);api_nvnTextureBuilderSetFlags(tb,9);
    api_nvnTextureBuilderSetSize2D(tb,WIDTH,HEIGHT);api_nvnTextureBuilderSetFormat(tb,NVN_FORMAT_RGBA8);
    size_t extent=(size_t)api_nvnTextureBuilderGetStorageSize(tb);CLEAN();CHECK(extent==19169280);
    void *storage=NULL;size_t pool_size=((extent*count+4095)&~(size_t)4095);CHECK(!posix_memalign(&storage,4096,pool_size));memset(storage,0x37,pool_size);
    api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&device);api_nvnMemoryPoolBuilderSetStorage(&pb,storage,pool_size);api_nvnMemoryPoolBuilderSetFlags(&pb,0xa1);CHECK(api_nvnMemoryPoolInitialize(&pool,&pb));CLEAN();
    for(unsigned i=0;i<count;i++){if(i){api_nvnTextureBuilderSetDefaults(tb+i);api_nvnTextureBuilderSetDevice(tb+i,&device);api_nvnTextureBuilderSetFlags(tb+i,9);api_nvnTextureBuilderSetSize2D(tb+i,WIDTH,HEIGHT);api_nvnTextureBuilderSetFormat(tb+i,NVN_FORMAT_RGBA8);}api_nvnTextureBuilderSetStorage(tb+i,&pool,(ptrdiff_t)(extent*i));CHECK(api_nvnTextureInitialize(textures+i,tb+i));}CLEAN();
    nn_bo *canonical=peek(&pool)->bo;blocked_source=canonical;uint64_t address=nn_bo_address(canonical);size_t offsets[3];for(unsigned i=0;i<count;i++)offsets[i]=peek(textures+i)->offset;
    NVNwindowBuilder wb={0};NVNwindow window={0};NVNtexture *array[3]={textures,textures+1,textures+2};
    api_nvnWindowBuilderSetDevice(&wb,&device);api_nvnWindowBuilderSetDefaults(&wb);api_nvnWindowBuilderSetNativeWindow(&wb,(void *)&token_identity);api_nvnWindowBuilderSetTextures(&wb,(int)count,array);
    unsigned before=display_creates;CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(ENOTSUP);CHECK(!peek(&window)&&display_creates==before);
    /* Registration itself is independently tested by the sibling's actual
     * public-host API suite; this whitebox test configures the exact same
     * immutable device pointer, never a substitute conversion implementation. */
    CHECK(!nn_nvn_register_texture_bridge(&device,&NN_TEXTURE_BRIDGE_BUNDLE));
    array[1]=textures;CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(EINVAL);array[1]=textures+1;CHECK(display_creates==before);
    display_create_error=ENOMEM;CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(ENOMEM);display_create_error=0;CHECK(!peek(&window));
    CHECK(peek(&pool)->refs==count);for(unsigned i=0;i<count;i++)CHECK(!peek(textures+i)->refs);
    /* Allocator error with an unsafe-cleanup owner must retain the failed
     * Window and canonical pins. Only an explicit later successful cleanup
     * releases those pins; returning false does not imply no ownership. */
    display_create_error=EIO;display_create_retained=1;display_destroy_error=EBUSY;
    CHECK(!api_nvnWindowInitialize(&window,&wb));ERROR(EIO);CHECK(peek(&window)&&peek(&window)->window->closing&&peek(&pool)->refs==2*count);
    unsigned failed_owned=display_destroys;api_nvnWindowFinalize(&window);ERROR(EBUSY);CHECK(peek(&window)&&display_destroys==failed_owned);
    display_create_error=display_create_retained=display_destroy_error=0;api_nvnWindowFinalize(&window);CLEAN();CHECK(!peek(&window)&&peek(&pool)->refs==count&&!blocked_display_count);
    CHECK(api_nvnWindowInitialize(&window,&wb));CLEAN();struct object *w=peek(&window);CHECK(w&&w->window->conversion_queue&&w->window->source_display[0]);
    for(unsigned i=0;i<count;i++){CHECK(peek(textures+i)->bo==canonical&&peek(textures+i)->offset==offsets[i]);CHECK(peek(textures+i)->refs==1);CHECK(w->window->display_views[i].width==WIDTH&&w->window->display_views[i].height==HEIGHT&&w->window->display_views[i].pitch==11520);}
    CHECK(peek(&pool)->bo==canonical&&nn_bo_address(canonical)==address&&peek(&pool)->refs==2*count);
    api_nvnTextureFinalize(textures);ERROR(EBUSY);api_nvnMemoryPoolFinalize(&pool);ERROR(EBUSY);
    NVNrectangle crop={0};api_nvnWindowGetCrop(&window,&crop);CHECK(crop.width==WIDTH&&crop.height==HEIGHT);api_nvnWindowSetCrop(&window,0,0,WIDTH,HEIGHT);CLEAN();api_nvnWindowSetCrop(&window,0,0,64,64);ERROR(ENOTSUP);
    nn_bo *entries[3]={0};for(unsigned i=0;i<count;i++)entries[i]=w->window->conversions[i].bos[NN_TEX_CONVERSION_ENTRY];
    for(unsigned round=0;round<2;round++)for(unsigned i=0;i<count;i++){
        int index=-1;CHECK(api_nvnQueueAcquireTexture(&queue,&window,&index)==NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS);CLEAN();CHECK(index==(int)i);
        struct nn_ahb_view read;struct nn_external_image_view external;struct nn_ahb_pixel pixel;
        CHECK(!nn_nvn_window_target_info(&window,i,&external,&pixel)&&pixel.width==WIDTH);
        CHECK(!nn_nvn_window_target_read_lock(&window,i,&read)&&read.address);CHECK(!nn_nvn_window_target_read_unlock(&window,i));
        struct nn_window_ticket saved_ticket=w->window->tickets[i];w->window->tickets[i].serial=0;
        unsigned stale_locks=native_read_locks;CHECK(nn_nvn_window_target_read_lock(&window,i,&read)==ESTALE&&native_read_locks==stale_locks);
        w->window->tickets[i]=saved_ticket;CLEAN();
        struct nn_nvn_window_source_identity identity;CHECK(!nn_nvn_window_source_identity(&window,i,&identity));CHECK(identity.pool==&pool&&identity.offset==offsets[i]&&identity.address==address+offsets[i]);
        unsigned prior=submissions,prior_finish=finishes,prior_enqueue=enqueues;
        api_nvnQueuePresentTexture(&queue,&window,index);CLEAN();CHECK(submissions==prior+1&&finishes==prior_finish+2&&enqueues==prior_enqueue+1);
        CHECK(w->window->conversions[i].bos[NN_TEX_CONVERSION_ENTRY]==entries[i]&&!w->window->conversion_pending);
        CHECK(peek(textures+i)->bo==canonical&&peek(textures+i)->offset==offsets[i]&&nn_bo_address(canonical)==address);
        /* QUEUED and CURRENT both exclude CPU READ before touching the
         * Android mock; the native wrapper maps session EINVAL to EBUSY. */
        unsigned before_lock=native_read_locks;struct nn_ahb_view forbidden;
        CHECK(nn_nvn_window_target_read_lock(&window,i,&forbidden)==EBUSY&&native_read_locks==before_lock);
        fake_worker_show(latest_session,i);
        CHECK(nn_nvn_window_target_read_lock(&window,i,&forbidden)==EBUSY&&native_read_locks==before_lock);
    }
    int index=-1;CHECK(api_nvnQueueAcquireTexture(&queue,&window,&index)==NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS);CLEAN();
    failed_conversion_queue=w->window->conversion_queue;conversion_submit_error=EIO;conversion_finish_error=EBUSY;
    before=enqueues;api_nvnQueuePresentTexture(&queue,&window,index);ERROR(EIO);CHECK(enqueues==before&&w->window->conversion_pending&&w->window->closing);
    struct nn_ahb_view unknown_read;unsigned locks_before_unknown=native_read_locks;
    CHECK(nn_nvn_window_target_read_lock(&window,(unsigned)index,&unknown_read)==EBUSY&&native_read_locks==locks_before_unknown);
    unsigned destroyed=display_destroys;api_nvnWindowFinalize(&window);ERROR(EBUSY);CHECK(peek(&window)&&display_destroys==destroyed&&peek(&pool)->refs==2*count);
    /* Synthetic fixture recovery is explicit: no unknown GPU completion is
     * inferred; allow a positive timeline proof solely to test final cleanup. */
    conversion_submit_error=conversion_finish_error=0;failed_conversion_queue=NULL;
    retire_error=EIO;api_nvnWindowFinalize(&window);ERROR(EIO);CHECK(peek(&window)&&display_destroys==destroyed);retire_error=0;
    api_nvnWindowFinalize(&window);CLEAN();CHECK(!peek(&window)&&display_destroys==destroyed+count&&peek(&pool)->refs==count);
    /* A fresh Window may reuse the original canonical resources after full
     * retirement, without rebinding their BO or requiring their recreation. */
    CHECK(api_nvnWindowInitialize(&window,&wb));CLEAN();CHECK(peek(&pool)->refs==2*count);
    CHECK(api_nvnQueueAcquireTexture(&queue,&window,&index)==NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS);CLEAN();
    api_nvnQueuePresentTexture(&queue,&window,index);CLEAN();fake_worker_show(latest_session,(unsigned)index);
    CHECK(!nn_nvn_window_retire_for_readback(&window));CHECK(!peek(&window)->window->session);
    for(unsigned slot=0;slot<count;slot++){struct nn_ahb_view read;CHECK(!nn_nvn_window_target_read_lock(&window,slot,&read));CHECK(!nn_nvn_window_target_read_unlock(&window,slot));}
    api_nvnWindowFinalize(&window);CLEAN();CHECK(!peek(&window)&&peek(&pool)->refs==count&&!blocked_display_count);
    blocked_source=NULL;
    for(unsigned i=0;i<count;i++){CHECK(peek(textures+i)->bo==canonical&&peek(textures+i)->offset==offsets[i]&&!peek(textures+i)->window_owner);api_nvnTextureFinalize(textures+i);}CLEAN();api_nvnMemoryPoolFinalize(&pool);CLEAN();free(storage);
    api_nvnQueueFinalize(&queue);api_nvnDeviceFinalize(&device);CLEAN();
    while(objects){CHECK(objects->kind==DB||objects->kind==QB||objects->kind==WB||objects->kind==TB||objects->kind==PB);remove_object(objects);}
    return 0;
}
int main(void)
{
    CHECK(!ticket_contract());CHECK(!cases(2));CHECK(!cases(3));CHECK(!source_cases(2));CHECK(!source_cases(3));
    CHECK(nvnBootstrapLoader("nvnQueueFenceSync")&&!nvnBootstrapLoader("nvnCommandBufferFenceSync"));
    printf("{\"passed\":true,\"scope\":\"host core/resource/session mocks only; no GPU/compositor/game\",\"checks\":%u,\"session_create_calls\":%u,\"owner_releases\":%u,\"submissions_mocked\":%u,\"enqueue_calls\":%u,\"finish_calls\":%u}\n",checks,creates,releases,submissions,enqueues,finishes);
    return 0;
}
