/* Whitebox actual core + existing host memory backend; no guest/GPU work. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#ifndef NN_QUEUE_CORE
#error "compile with the explicit candidate or baseline core path"
#endif
#include NN_QUEUE_CORE
static unsigned checks, creates, destroys, live_queues;
static int fail_create, fail_destroy;
int __real_nn_queue_create(nn_device *, nn_queue **);
int __real_nn_queue_destroy(nn_queue *);
int __wrap_nn_queue_create(nn_device *device, nn_queue **out)
{
    creates++;
    if (fail_create) { int e=fail_create; fail_create=0; *out=NULL; return e; }
    int e=__real_nn_queue_create(device,out);
    if (!e) live_queues++;
    return e;
}
int __wrap_nn_queue_destroy(nn_queue *queue)
{
    destroys++;
    if (fail_destroy) { int e=fail_destroy; fail_destroy=0; return e; }
    int e=__real_nn_queue_destroy(queue);
    if (!e) { assert(live_queues); live_queues--; }
    return e;
}
static struct object *peek(const void *key)
{ for(struct object *o=objects;o;o=o->next) if(o->key==key) return o; return NULL; }
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"FAIL line%d: %s errno=%d\n",__LINE__,#x,nn_nvn_error());return 1;} } while(0)
#define CLEAN() CHECK(nn_nvn_error()==0)
#define ERROR(e) do { CHECK(nn_nvn_error()==(e));nn_nvn_clear_error(); } while(0)
int main(void)
{
    PFNNVNDEVICEBUILDERSETDEFAULTSPROC nvnDeviceBuilderSetDefaults = (PFNNVNDEVICEBUILDERSETDEFAULTSPROC)nvnBootstrapLoader("nvnDeviceBuilderSetDefaults");
    assert(nvnDeviceBuilderSetDefaults);
    PFNNVNDEVICEFINALIZEPROC nvnDeviceFinalize = (PFNNVNDEVICEFINALIZEPROC)nvnBootstrapLoader("nvnDeviceFinalize");
    assert(nvnDeviceFinalize);
    PFNNVNDEVICEINITIALIZEPROC nvnDeviceInitialize = (PFNNVNDEVICEINITIALIZEPROC)nvnBootstrapLoader("nvnDeviceInitialize");
    assert(nvnDeviceInitialize);
    PFNNVNQUEUEBUILDERSETDEFAULTSPROC nvnQueueBuilderSetDefaults = (PFNNVNQUEUEBUILDERSETDEFAULTSPROC)nvnBootstrapLoader("nvnQueueBuilderSetDefaults");
    assert(nvnQueueBuilderSetDefaults);
    PFNNVNQUEUEBUILDERSETDEVICEPROC nvnQueueBuilderSetDevice = (PFNNVNQUEUEBUILDERSETDEVICEPROC)nvnBootstrapLoader("nvnQueueBuilderSetDevice");
    assert(nvnQueueBuilderSetDevice);
    PFNNVNQUEUEFINALIZEPROC nvnQueueFinalize = (PFNNVNQUEUEFINALIZEPROC)nvnBootstrapLoader("nvnQueueFinalize");
    assert(nvnQueueFinalize);
    PFNNVNQUEUEFINISHPROC nvnQueueFinish = (PFNNVNQUEUEFINISHPROC)nvnBootstrapLoader("nvnQueueFinish");
    assert(nvnQueueFinish);
    PFNNVNQUEUEINITIALIZEPROC nvnQueueInitialize = (PFNNVNQUEUEINITIALIZEPROC)nvnBootstrapLoader("nvnQueueInitialize");
    assert(nvnQueueInitialize);
    NVNdeviceBuilder db={0};NVNdevice device={0},dead={0};
    NVNqueueBuilder builder_key={0},empty={0},stale={0};NVNqueue queue={0};
    nvnDeviceBuilderSetDefaults(&db);CHECK(nvnDeviceInitialize(&device,&db));CLEAN();
    /* Conventional order, then a repeated Defaults must preserve SetDevice. */
    nvnQueueBuilderSetDefaults(&builder_key);CLEAN();
    CHECK(peek(&builder_key)&&peek(&builder_key)->device==NULL);
    nvnQueueBuilderSetDevice(&builder_key,&device);CLEAN();
    nvnQueueBuilderSetDefaults(&builder_key);CLEAN();
    CHECK(peek(&builder_key)->device==peek(&device));
    CHECK(nvnQueueInitialize(&queue,&builder_key));CLEAN();
    CHECK(creates==1&&live_queues==1&&peek(&device)->refs==1);
    CHECK(peek(&queue)->device==peek(&device));
    /* Builder is a weak reference; the initialized Queue holds the strong one. */
    nvnQueueBuilderSetDefaults(&builder_key);CLEAN();
    CHECK(peek(&builder_key)->device==peek(&device)&&peek(&queue)->device==peek(&device));
    nvnDeviceFinalize(&device);ERROR(EBUSY);CHECK(peek(&device));
    unsigned before=creates;
    CHECK(!nvnQueueInitialize(&queue,&builder_key));ERROR(EEXIST);
    CHECK(creates==before&&live_queues==1&&peek(&device)->refs==1);
    /* Existing Finalize error/rollback behavior must not be changed. */
    fail_destroy=EAGAIN;nvnQueueFinalize(&queue);ERROR(EAGAIN);
    CHECK(peek(&queue)&&live_queues==1&&peek(&device)->refs==1);
    nvnQueueFinalize(&queue);CLEAN();
    CHECK(!peek(&queue)&&live_queues==0&&peek(&device)->refs==0);
    /* A new no-device builder cannot enter the backend or publish a Queue. */
    nvnQueueBuilderSetDefaults(&empty);CLEAN();before=creates;
    CHECK(!nvnQueueInitialize(&queue,&empty));ERROR(EINVAL);
    CHECK(creates==before&&!peek(&queue)&&peek(&device)->refs==0);
    /* Device was set while live, then finalized, before Defaults/Initialize. */
    CHECK(nvnDeviceInitialize(&dead,&db));CLEAN();
    nvnQueueBuilderSetDefaults(&stale);nvnQueueBuilderSetDevice(&stale,&dead);CLEAN();
    CHECK(peek(&dead)->refs==0);nvnDeviceFinalize(&dead);CLEAN();CHECK(!peek(&dead));
    before=creates;CHECK(!nvnQueueInitialize(&queue,&stale));ERROR(EINVAL);
    CHECK(creates==before&&!peek(&queue)&&live_queues==0);
    nvnQueueBuilderSetDefaults(&stale);CLEAN();
    CHECK(!nvnQueueInitialize(&queue,&stale));ERROR(EINVAL);
    CHECK(creates==before&&!peek(&queue)&&live_queues==0);
    /* Live wrong-kind metadata is also not a valid DEVICE weak reference. */
    peek(&stale)->device=peek(&db);
    CHECK(!nvnQueueInitialize(&queue,&stale));ERROR(EINVAL);
    CHECK(creates==before&&!peek(&queue)&&peek(&device)->refs==0);
    /* Rebinding after a rejected stale reference remains usable. */
    nvnQueueBuilderSetDevice(&stale,&device);CLEAN();
    CHECK(nvnQueueInitialize(&queue,&stale));CLEAN();
    CHECK(creates==before+1&&live_queues==1&&peek(&device)->refs==1);
    nvnQueueFinalize(&queue);CLEAN();
    /* Backend creation failure must remove the unpublished Queue and refs. */
    before=creates;fail_create=ENOMEM;
    CHECK(!nvnQueueInitialize(&queue,&stale));ERROR(ENOMEM);
    CHECK(creates==before+1&&!peek(&queue)&&live_queues==0&&peek(&device)->refs==0);
    CHECK(nvnQueueInitialize(&queue,&stale));CLEAN();
    CHECK(creates==before+2&&live_queues==1&&peek(&device)->refs==1);
    nvnQueueFinish(&queue);CLEAN();nvnQueueFinalize(&queue);CLEAN();
    CHECK(live_queues==0&&peek(&device)->refs==0);
    nvnDeviceFinalize(&device);CLEAN();CHECK(!peek(&device));
    /* Free test-owned side-table builders so leak sanitizer checks real owners. */
    while(objects){CHECK(objects->kind==DB||objects->kind==QB);remove_object(objects);}
    CHECK(!handles);
    printf("{\"passed\":true,\"scope\":\"host core/backend only; no GPU/game execution\",\"checks\":%u,\"queue_create_calls\":%u,\"queue_destroy_calls\":%u,\"live_queues\":%u}\n",checks,creates,destroys,live_queues);
    return 0;
}
