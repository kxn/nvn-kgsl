/* HOST ONLY actual candidate core + deterministic RAM queue backend.
 * PM4 is inspected but NEVER executed. No guest modules or Android device. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include NN_CORE
#include "timeline_backend.h"
static unsigned checks, debug_calls;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s sticky=%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define CLEAN() CHECK(!nn_nvn_error())
#define ERROR(e) do{CHECK(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
/* Public typed bootstrap bindings are generated below. */
static PFNNVNCOMMANDBUFFERADDCOMMANDMEMORYPROC nvnCommandBufferAddCommandMemory;
static PFNNVNCOMMANDBUFFERADDCONTROLMEMORYPROC nvnCommandBufferAddControlMemory;
static PFNNVNCOMMANDBUFFERBARRIERPROC nvnCommandBufferBarrier;
static PFNNVNCOMMANDBUFFERBEGINRECORDINGPROC nvnCommandBufferBeginRecording;
static PFNNVNCOMMANDBUFFERCLEARBUFFERPROC nvnCommandBufferClearBuffer;
static PFNNVNCOMMANDBUFFERENDRECORDINGPROC nvnCommandBufferEndRecording;
static PFNNVNCOMMANDBUFFERFINALIZEPROC nvnCommandBufferFinalize;
static PFNNVNCOMMANDBUFFERINITIALIZEPROC nvnCommandBufferInitialize;
static PFNNVNCOMMANDBUFFERINSERTDEBUGMARKERDYNAMICPROC nvnCommandBufferInsertDebugMarkerDynamic;
static PFNNVNCOMMANDBUFFERISRECORDINGPROC nvnCommandBufferIsRecording;
static PFNNVNCOMMANDBUFFERPOPDEBUGGROUPPROC nvnCommandBufferPopDebugGroup;
static PFNNVNCOMMANDBUFFERPUSHDEBUGGROUPDYNAMICPROC nvnCommandBufferPushDebugGroupDynamic;
static PFNNVNCOMMANDBUFFERSETMEMORYCALLBACKPROC nvnCommandBufferSetMemoryCallback;
static PFNNVNCOMMANDBUFFERSETMEMORYCALLBACKDATAPROC nvnCommandBufferSetMemoryCallbackData;
static PFNNVNCOMMANDBUFFERSIGNALEVENTPROC nvnCommandBufferSignalEvent;
static PFNNVNCOMMANDBUFFERWAITEVENTPROC nvnCommandBufferWaitEvent;
static PFNNVNCOMMANDBUFFERWAITSYNCPROC nvnCommandBufferWaitSync;
static PFNNVNDEVICEBUILDERSETDEFAULTSPROC nvnDeviceBuilderSetDefaults;
static PFNNVNDEVICEFINALIZEPROC nvnDeviceFinalize;
static PFNNVNDEVICEFINALIZECOMMANDHANDLEPROC nvnDeviceFinalizeCommandHandle;
static PFNNVNDEVICEINITIALIZEPROC nvnDeviceInitialize;
static PFNNVNDEVICEINSTALLDEBUGCALLBACKPROC nvnDeviceInstallDebugCallback;
static PFNNVNEVENTBUILDERSETDEFAULTSPROC nvnEventBuilderSetDefaults;
static PFNNVNEVENTBUILDERSETSTORAGEPROC nvnEventBuilderSetStorage;
static PFNNVNEVENTFINALIZEPROC nvnEventFinalize;
static PFNNVNEVENTGETVALUEPROC nvnEventGetValue;
static PFNNVNEVENTINITIALIZEPROC nvnEventInitialize;
static PFNNVNEVENTSIGNALPROC nvnEventSignal;
static PFNNVNMEMORYPOOLBUILDERSETDEFAULTSPROC nvnMemoryPoolBuilderSetDefaults;
static PFNNVNMEMORYPOOLBUILDERSETDEVICEPROC nvnMemoryPoolBuilderSetDevice;
static PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC nvnMemoryPoolBuilderSetFlags;
static PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC nvnMemoryPoolBuilderSetStorage;
static PFNNVNMEMORYPOOLFINALIZEPROC nvnMemoryPoolFinalize;
static PFNNVNMEMORYPOOLINITIALIZEPROC nvnMemoryPoolInitialize;
static PFNNVNQUEUEBUILDERGETQUEUEMEMORYSIZEPROC nvnQueueBuilderGetQueueMemorySize;
static PFNNVNQUEUEBUILDERSETCOMMANDFLUSHTHRESHOLDPROC nvnQueueBuilderSetCommandFlushThreshold;
static PFNNVNQUEUEBUILDERSETCOMMANDMEMORYSIZEPROC nvnQueueBuilderSetCommandMemorySize;
static PFNNVNQUEUEBUILDERSETCOMPUTEMEMORYSIZEPROC nvnQueueBuilderSetComputeMemorySize;
static PFNNVNQUEUEBUILDERSETCONTROLMEMORYSIZEPROC nvnQueueBuilderSetControlMemorySize;
static PFNNVNQUEUEBUILDERSETDEFAULTSPROC nvnQueueBuilderSetDefaults;
static PFNNVNQUEUEBUILDERSETDEVICEPROC nvnQueueBuilderSetDevice;
static PFNNVNQUEUEBUILDERSETFLAGSPROC nvnQueueBuilderSetFlags;
static PFNNVNQUEUEBUILDERSETQUEUEMEMORYPROC nvnQueueBuilderSetQueueMemory;
static PFNNVNQUEUEFENCESYNCPROC nvnQueueFenceSync;
static PFNNVNQUEUEFINALIZEPROC nvnQueueFinalize;
static PFNNVNQUEUEFINISHPROC nvnQueueFinish;
static PFNNVNQUEUEFLUSHPROC nvnQueueFlush;
static PFNNVNQUEUEGETERRORPROC nvnQueueGetError;
static PFNNVNQUEUEINITIALIZEPROC nvnQueueInitialize;
static PFNNVNQUEUESUBMITCOMMANDSPROC nvnQueueSubmitCommands;
static PFNNVNQUEUEWAITSYNCPROC nvnQueueWaitSync;
static PFNNVNSYNCFINALIZEPROC nvnSyncFinalize;
static PFNNVNSYNCINITIALIZEPROC nvnSyncInitialize;
static PFNNVNSYNCWAITPROC nvnSyncWait;
static void debug_callback(NVNdebugCallbackSource source,NVNdebugCallbackType type,int id,
                           NVNdebugCallbackSeverity severity,const unsigned char*message,void*user)
{
    assert(source==NVN_DEBUG_CALLBACK_SOURCE_API&&type==NVN_DEBUG_CALLBACK_TYPE_API_ERROR&&
        id==ENOTSUP&&severity==NVN_DEBUG_CALLBACK_SEVERITY_HIGH&&message&&user==&debug_calls);
    debug_calls++;
}
struct callback_state { NVNmemoryPool *pool; unsigned command,control,fail_finalize; size_t next; unsigned char controls[256]; };
static void memory_callback(NVNcommandBuffer*c,NVNcommandBufferMemoryEvent event,size_t bytes,void*user)
{
    struct callback_state*s=user;assert(bytes&&s);
    if(s->fail_finalize){s->fail_finalize=0;nvnCommandBufferFinalize(c);return;}
    if(event==NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_CONTROL_MEMORY){
        s->control++;nvnCommandBufferAddControlMemory(c,s->controls,sizeof(s->controls));
    }else{
        assert(event==NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY);
        size_t size=s->command?4096:64;
        assert(bytes<=size&&s->next+size<=16384);
        nvnCommandBufferAddCommandMemory(c,s->pool,(ptrdiff_t)s->next,size);
        s->next+=4096;s->command++;
    }
}
static struct object*peek(const void*key)
{for(struct object*p=objects;p;p=p->next)if(p->key==key)return p;return NULL;}
static int contains(const void*p,size_t n,const char*text)
{size_t len=strlen(text);for(size_t i=0;len<=n&&i<=n-len;i++)if(!memcmp((const char*)p+i,text,len))return 1;return 0;}
int main(void)
{
    nvnCommandBufferAddCommandMemory=(PFNNVNCOMMANDBUFFERADDCOMMANDMEMORYPROC)nvnBootstrapLoader("nvnCommandBufferAddCommandMemory");assert(nvnCommandBufferAddCommandMemory);
    nvnCommandBufferAddControlMemory=(PFNNVNCOMMANDBUFFERADDCONTROLMEMORYPROC)nvnBootstrapLoader("nvnCommandBufferAddControlMemory");assert(nvnCommandBufferAddControlMemory);
    nvnCommandBufferBarrier=(PFNNVNCOMMANDBUFFERBARRIERPROC)nvnBootstrapLoader("nvnCommandBufferBarrier");assert(nvnCommandBufferBarrier);
    nvnCommandBufferBeginRecording=(PFNNVNCOMMANDBUFFERBEGINRECORDINGPROC)nvnBootstrapLoader("nvnCommandBufferBeginRecording");assert(nvnCommandBufferBeginRecording);
    nvnCommandBufferClearBuffer=(PFNNVNCOMMANDBUFFERCLEARBUFFERPROC)nvnBootstrapLoader("nvnCommandBufferClearBuffer");assert(nvnCommandBufferClearBuffer);
    nvnCommandBufferEndRecording=(PFNNVNCOMMANDBUFFERENDRECORDINGPROC)nvnBootstrapLoader("nvnCommandBufferEndRecording");assert(nvnCommandBufferEndRecording);
    nvnCommandBufferFinalize=(PFNNVNCOMMANDBUFFERFINALIZEPROC)nvnBootstrapLoader("nvnCommandBufferFinalize");assert(nvnCommandBufferFinalize);
    nvnCommandBufferInitialize=(PFNNVNCOMMANDBUFFERINITIALIZEPROC)nvnBootstrapLoader("nvnCommandBufferInitialize");assert(nvnCommandBufferInitialize);
    nvnCommandBufferInsertDebugMarkerDynamic=(PFNNVNCOMMANDBUFFERINSERTDEBUGMARKERDYNAMICPROC)nvnBootstrapLoader("nvnCommandBufferInsertDebugMarkerDynamic");assert(nvnCommandBufferInsertDebugMarkerDynamic);
    nvnCommandBufferIsRecording=(PFNNVNCOMMANDBUFFERISRECORDINGPROC)nvnBootstrapLoader("nvnCommandBufferIsRecording");assert(nvnCommandBufferIsRecording);
    nvnCommandBufferPopDebugGroup=(PFNNVNCOMMANDBUFFERPOPDEBUGGROUPPROC)nvnBootstrapLoader("nvnCommandBufferPopDebugGroup");assert(nvnCommandBufferPopDebugGroup);
    nvnCommandBufferPushDebugGroupDynamic=(PFNNVNCOMMANDBUFFERPUSHDEBUGGROUPDYNAMICPROC)nvnBootstrapLoader("nvnCommandBufferPushDebugGroupDynamic");assert(nvnCommandBufferPushDebugGroupDynamic);
    nvnCommandBufferSetMemoryCallback=(PFNNVNCOMMANDBUFFERSETMEMORYCALLBACKPROC)nvnBootstrapLoader("nvnCommandBufferSetMemoryCallback");assert(nvnCommandBufferSetMemoryCallback);
    nvnCommandBufferSetMemoryCallbackData=(PFNNVNCOMMANDBUFFERSETMEMORYCALLBACKDATAPROC)nvnBootstrapLoader("nvnCommandBufferSetMemoryCallbackData");assert(nvnCommandBufferSetMemoryCallbackData);
    nvnCommandBufferSignalEvent=(PFNNVNCOMMANDBUFFERSIGNALEVENTPROC)nvnBootstrapLoader("nvnCommandBufferSignalEvent");assert(nvnCommandBufferSignalEvent);
    nvnCommandBufferWaitEvent=(PFNNVNCOMMANDBUFFERWAITEVENTPROC)nvnBootstrapLoader("nvnCommandBufferWaitEvent");assert(nvnCommandBufferWaitEvent);
    nvnCommandBufferWaitSync=(PFNNVNCOMMANDBUFFERWAITSYNCPROC)nvnBootstrapLoader("nvnCommandBufferWaitSync");assert(nvnCommandBufferWaitSync);
    nvnDeviceBuilderSetDefaults=(PFNNVNDEVICEBUILDERSETDEFAULTSPROC)nvnBootstrapLoader("nvnDeviceBuilderSetDefaults");assert(nvnDeviceBuilderSetDefaults);
    nvnDeviceFinalize=(PFNNVNDEVICEFINALIZEPROC)nvnBootstrapLoader("nvnDeviceFinalize");assert(nvnDeviceFinalize);
    nvnDeviceFinalizeCommandHandle=(PFNNVNDEVICEFINALIZECOMMANDHANDLEPROC)nvnBootstrapLoader("nvnDeviceFinalizeCommandHandle");assert(nvnDeviceFinalizeCommandHandle);
    nvnDeviceInitialize=(PFNNVNDEVICEINITIALIZEPROC)nvnBootstrapLoader("nvnDeviceInitialize");assert(nvnDeviceInitialize);
    nvnDeviceInstallDebugCallback=(PFNNVNDEVICEINSTALLDEBUGCALLBACKPROC)nvnBootstrapLoader("nvnDeviceInstallDebugCallback");assert(nvnDeviceInstallDebugCallback);
    nvnEventBuilderSetDefaults=(PFNNVNEVENTBUILDERSETDEFAULTSPROC)nvnBootstrapLoader("nvnEventBuilderSetDefaults");assert(nvnEventBuilderSetDefaults);
    nvnEventBuilderSetStorage=(PFNNVNEVENTBUILDERSETSTORAGEPROC)nvnBootstrapLoader("nvnEventBuilderSetStorage");assert(nvnEventBuilderSetStorage);
    nvnEventFinalize=(PFNNVNEVENTFINALIZEPROC)nvnBootstrapLoader("nvnEventFinalize");assert(nvnEventFinalize);
    nvnEventGetValue=(PFNNVNEVENTGETVALUEPROC)nvnBootstrapLoader("nvnEventGetValue");assert(nvnEventGetValue);
    nvnEventInitialize=(PFNNVNEVENTINITIALIZEPROC)nvnBootstrapLoader("nvnEventInitialize");assert(nvnEventInitialize);
    nvnEventSignal=(PFNNVNEVENTSIGNALPROC)nvnBootstrapLoader("nvnEventSignal");assert(nvnEventSignal);
    nvnMemoryPoolBuilderSetDefaults=(PFNNVNMEMORYPOOLBUILDERSETDEFAULTSPROC)nvnBootstrapLoader("nvnMemoryPoolBuilderSetDefaults");assert(nvnMemoryPoolBuilderSetDefaults);
    nvnMemoryPoolBuilderSetDevice=(PFNNVNMEMORYPOOLBUILDERSETDEVICEPROC)nvnBootstrapLoader("nvnMemoryPoolBuilderSetDevice");assert(nvnMemoryPoolBuilderSetDevice);
    nvnMemoryPoolBuilderSetFlags=(PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC)nvnBootstrapLoader("nvnMemoryPoolBuilderSetFlags");assert(nvnMemoryPoolBuilderSetFlags);
    nvnMemoryPoolBuilderSetStorage=(PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC)nvnBootstrapLoader("nvnMemoryPoolBuilderSetStorage");assert(nvnMemoryPoolBuilderSetStorage);
    nvnMemoryPoolFinalize=(PFNNVNMEMORYPOOLFINALIZEPROC)nvnBootstrapLoader("nvnMemoryPoolFinalize");assert(nvnMemoryPoolFinalize);
    nvnMemoryPoolInitialize=(PFNNVNMEMORYPOOLINITIALIZEPROC)nvnBootstrapLoader("nvnMemoryPoolInitialize");assert(nvnMemoryPoolInitialize);
    nvnQueueBuilderGetQueueMemorySize=(PFNNVNQUEUEBUILDERGETQUEUEMEMORYSIZEPROC)nvnBootstrapLoader("nvnQueueBuilderGetQueueMemorySize");assert(nvnQueueBuilderGetQueueMemorySize);
    nvnQueueBuilderSetCommandFlushThreshold=(PFNNVNQUEUEBUILDERSETCOMMANDFLUSHTHRESHOLDPROC)nvnBootstrapLoader("nvnQueueBuilderSetCommandFlushThreshold");assert(nvnQueueBuilderSetCommandFlushThreshold);
    nvnQueueBuilderSetCommandMemorySize=(PFNNVNQUEUEBUILDERSETCOMMANDMEMORYSIZEPROC)nvnBootstrapLoader("nvnQueueBuilderSetCommandMemorySize");assert(nvnQueueBuilderSetCommandMemorySize);
    nvnQueueBuilderSetComputeMemorySize=(PFNNVNQUEUEBUILDERSETCOMPUTEMEMORYSIZEPROC)nvnBootstrapLoader("nvnQueueBuilderSetComputeMemorySize");assert(nvnQueueBuilderSetComputeMemorySize);
    nvnQueueBuilderSetControlMemorySize=(PFNNVNQUEUEBUILDERSETCONTROLMEMORYSIZEPROC)nvnBootstrapLoader("nvnQueueBuilderSetControlMemorySize");assert(nvnQueueBuilderSetControlMemorySize);
    nvnQueueBuilderSetDefaults=(PFNNVNQUEUEBUILDERSETDEFAULTSPROC)nvnBootstrapLoader("nvnQueueBuilderSetDefaults");assert(nvnQueueBuilderSetDefaults);
    nvnQueueBuilderSetDevice=(PFNNVNQUEUEBUILDERSETDEVICEPROC)nvnBootstrapLoader("nvnQueueBuilderSetDevice");assert(nvnQueueBuilderSetDevice);
    nvnQueueBuilderSetFlags=(PFNNVNQUEUEBUILDERSETFLAGSPROC)nvnBootstrapLoader("nvnQueueBuilderSetFlags");assert(nvnQueueBuilderSetFlags);
    nvnQueueBuilderSetQueueMemory=(PFNNVNQUEUEBUILDERSETQUEUEMEMORYPROC)nvnBootstrapLoader("nvnQueueBuilderSetQueueMemory");assert(nvnQueueBuilderSetQueueMemory);
    nvnQueueFenceSync=(PFNNVNQUEUEFENCESYNCPROC)nvnBootstrapLoader("nvnQueueFenceSync");assert(nvnQueueFenceSync);
    nvnQueueFinalize=(PFNNVNQUEUEFINALIZEPROC)nvnBootstrapLoader("nvnQueueFinalize");assert(nvnQueueFinalize);
    nvnQueueFinish=(PFNNVNQUEUEFINISHPROC)nvnBootstrapLoader("nvnQueueFinish");assert(nvnQueueFinish);
    nvnQueueFlush=(PFNNVNQUEUEFLUSHPROC)nvnBootstrapLoader("nvnQueueFlush");assert(nvnQueueFlush);
    nvnQueueGetError=(PFNNVNQUEUEGETERRORPROC)nvnBootstrapLoader("nvnQueueGetError");assert(nvnQueueGetError);
    nvnQueueInitialize=(PFNNVNQUEUEINITIALIZEPROC)nvnBootstrapLoader("nvnQueueInitialize");assert(nvnQueueInitialize);
    nvnQueueSubmitCommands=(PFNNVNQUEUESUBMITCOMMANDSPROC)nvnBootstrapLoader("nvnQueueSubmitCommands");assert(nvnQueueSubmitCommands);
    nvnQueueWaitSync=(PFNNVNQUEUEWAITSYNCPROC)nvnBootstrapLoader("nvnQueueWaitSync");assert(nvnQueueWaitSync);
    nvnSyncFinalize=(PFNNVNSYNCFINALIZEPROC)nvnBootstrapLoader("nvnSyncFinalize");assert(nvnSyncFinalize);
    nvnSyncInitialize=(PFNNVNSYNCINITIALIZEPROC)nvnBootstrapLoader("nvnSyncInitialize");assert(nvnSyncInitialize);
    nvnSyncWait=(PFNNVNSYNCWAITPROC)nvnBootstrapLoader("nvnSyncWait");assert(nvnSyncWait);
    NVNdeviceBuilder db={0};NVNdevice device={0};
    nvnDeviceBuilderSetDefaults(&db);CHECK(nvnDeviceInitialize(&device,&db));CLEAN();
    /* A same stack address may start a different BUILDER lifetime. Resources
     * remain owners and cannot be silently retagged. */
    union { max_align_t align; unsigned char bytes[512]; } reused;
    NVNmemoryPoolBuilder*pb=(NVNmemoryPoolBuilder*)&reused;
    NVNqueueBuilder*qb=(NVNqueueBuilder*)&reused;
    nvnMemoryPoolBuilderSetDefaults(pb);nvnMemoryPoolBuilderSetDevice(pb,&device);CLEAN();
    nvnQueueBuilderSetDefaults(qb);CLEAN();CHECK(peek(qb)->kind==QB&&!peek(qb)->device);
    nvnQueueBuilderSetDevice(qb,&device);nvnQueueBuilderSetDefaults(qb);CLEAN();CHECK(peek(qb)->device==peek(&device));
    nvnQueueBuilderSetFlags(qb,3);CLEAN();nvnQueueBuilderSetFlags(qb,4);ERROR(ENOTSUP);CHECK(peek(qb)->flags==3);
    nvnQueueBuilderSetCommandMemorySize(qb,8192);nvnQueueBuilderSetComputeMemorySize(qb,0);
    nvnQueueBuilderSetControlMemorySize(qb,4096);nvnQueueBuilderSetCommandFlushThreshold(qb,1024);CLEAN();
    size_t bytes=nvnQueueBuilderGetQueueMemorySize(qb);CHECK(bytes==16384);CLEAN();
    nvnQueueBuilderSetCommandMemorySize(qb,257);ERROR(EINVAL);CHECK(peek(qb)->queue_config.command==8192);
    NVNqueueBuilder fresh={0};nvnQueueBuilderSetDevice(&fresh,&device);CLEAN();
    nvnQueueBuilderSetDefaults(&fresh);CLEAN();CHECK(peek(&fresh)->device==peek(&device));
    NVNqueue q1={0},q2={0},q3={0};CHECK(nvnQueueInitialize(&q1,qb));CLEAN();
    CHECK(peek(&q1)->queue_state->owned&&peek(&q1)->flags==3);
    nvnQueueBuilderSetDefaults((NVNqueueBuilder*)&q1);ERROR(EEXIST);CHECK(peek(&q1)->kind==QUEUE);
    void*workspace=aligned_alloc(4096,bytes+4096);CHECK(workspace);memset(workspace,0xa5,bytes+4096);
    nvnQueueBuilderSetQueueMemory(qb,(char*)workspace+1,bytes);ERROR(EINVAL);
    nvnQueueBuilderSetQueueMemory(qb,workspace,bytes-4096);CLEAN();CHECK(!nvnQueueInitialize(&q2,qb));ERROR(ENOSPC);CHECK(!peek(&q2));
    nvnQueueBuilderSetQueueMemory(qb,workspace,bytes);CLEAN();CHECK(nvnQueueInitialize(&q2,qb));CLEAN();
    CHECK(!peek(&q2)->queue_state->owned&&peek(&q2)->queue_state==workspace);
    CHECK(!nvnQueueInitialize(&q3,qb));ERROR(EBUSY);CHECK(!peek(&q3));
    for(size_t i=bytes;i<bytes+4096;i++)CHECK(((unsigned char*)workspace)[i]==0xa5);
    nvnQueueFlush(&q1);CLEAN();CHECK(nn_test_flushes()==1);
    NVNqueueErrorInfo error_info={{0x5a}};
    CHECK(nvnQueueGetError(&q1,&error_info)==NVN_QUEUE_GET_ERROR_RESULT_GPU_NO_ERROR);CLEAN();CHECK(error_info.dummy[0]==0x5a);
    nn_test_faults(1);CHECK(nvnQueueGetError(&q1,NULL)==NVN_QUEUE_GET_ERROR_RESULT_GPU_ERROR_UNKNOWN);CLEAN();
    CHECK(nvnQueueGetError(&q1,&error_info)==NVN_QUEUE_GET_ERROR_RESULT_GPU_ERROR_UNKNOWN);ERROR(ENOTSUP);
    CHECK(error_info.dummy[0]==0x5a);nn_test_faults(0);
    nn_test_fail_next(EIO);CHECK(nvnQueueGetError(&q1,NULL)==NVN_QUEUE_GET_ERROR_RESULT_GPU_ERROR_UNKNOWN);ERROR(EIO);
    nvnDeviceInstallDebugCallback(&device,debug_callback,&debug_calls,1);CLEAN();
    nvnQueueBuilderSetFlags(qb,4);ERROR(ENOTSUP);CHECK(debug_calls==1);
    nvnDeviceInstallDebugCallback(&device,debug_callback,&debug_calls,0);CLEAN();
    nvnQueueBuilderSetFlags(qb,4);ERROR(ENOTSUP);CHECK(debug_calls==1);
    nvnQueueBuilderSetFlags(qb,0);CLEAN();
    /* Real NVN events preserve their caller-owned DWORD and backing pool. */
    void*storage=aligned_alloc(4096,16384);CHECK(storage);memset(storage,0,16384);((uint32_t*)storage)[64]=7;
    NVNmemoryPool pool={0};nvnMemoryPoolBuilderSetDefaults(pb);nvnMemoryPoolBuilderSetDevice(pb,&device);
    nvnMemoryPoolBuilderSetStorage(pb,storage,16384);nvnMemoryPoolBuilderSetFlags(pb,0x24);CHECK(nvnMemoryPoolInitialize(&pool,pb));CLEAN();
    NVNeventBuilder eb={0};NVNevent event={0};nvnEventBuilderSetDefaults(&eb);
    nvnEventBuilderSetStorage(&eb,&pool,257);ERROR(EINVAL);CHECK(!nvnEventInitialize(&event,&eb));ERROR(EINVAL);
    nvnEventBuilderSetStorage(&eb,&pool,256);CHECK(nvnEventInitialize(&event,&eb));CLEAN();
    CHECK(nvnEventGetValue(&event)==7);CLEAN();nvnEventSignal(&event,NVN_EVENT_SIGNAL_MODE_ADD,3);CLEAN();
    CHECK(nvnEventGetValue(&event)==10);CLEAN();nvnEventSignal(&event,NVN_EVENT_SIGNAL_MODE_WRITE,0);CLEAN();
    CHECK(nvnEventGetValue(&event)==0);CLEAN();nvnMemoryPoolFinalize(&pool);ERROR(EBUSY);
    NVNsync sync={0};CHECK(nvnSyncInitialize(&sync,&device));CLEAN();
    CHECK(nvnSyncWait(&sync,0)==NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED);CLEAN();
    CHECK(!nvnQueueWaitSync(&q2,&sync));ERROR(EINVAL);
    /* Memory callback initially supplies both arenas. A forced Finalize from
     * the callback must fail closed without freeing the recording owner. */
    NVNcommandBuffer command={0};CHECK(nvnCommandBufferInitialize(&command,&device));CLEAN();
    CHECK(!nvnCommandBufferIsRecording(&command));CLEAN();
    struct callback_state cb={.pool=&pool,.fail_finalize=1};
    nvnCommandBufferSetMemoryCallback(&command,memory_callback);nvnCommandBufferSetMemoryCallbackData(&command,&cb);
    nvnCommandBufferBeginRecording(&command);ERROR(EBUSY);CHECK(peek(&command)&&!nvnCommandBufferIsRecording(&command));CLEAN();
    nvnCommandBufferBeginRecording(&command);CLEAN();CHECK(nvnCommandBufferIsRecording(&command));CLEAN();
    nvnCommandBufferPushDebugGroupDynamic(&command,41,"original-group");CLEAN();
    char dynamic[180];memset(dynamic,'a',sizeof(dynamic)-1);dynamic[sizeof(dynamic)-1]=0;memcpy(dynamic,"original-label",14);
    nvnCommandBufferInsertDebugMarkerDynamic(&command,42,dynamic);CLEAN();memset(dynamic,'b',sizeof(dynamic)-1);
    CHECK(cb.command==2&&cb.control==1&&peek(&command)->record->logical_bytes>0);
    size_t used=peek(&command)->record->used;
    nvnCommandBufferWaitEvent(&command,&event,NVN_EVENT_WAIT_MODE_GEQUAL_WRAP,1);ERROR(ENOTSUP);CHECK(peek(&command)->record->used==used);
    nvnCommandBufferSignalEvent(&command,&event,NVN_EVENT_SIGNAL_MODE_ADD,NVN_EVENT_SIGNAL_LOCATION_BOTTOM,0,1);ERROR(ENOTSUP);CHECK(peek(&command)->record->used==used);
    nvnCommandBufferSignalEvent(&command,&event,NVN_EVENT_SIGNAL_MODE_WRITE,NVN_EVENT_SIGNAL_LOCATION_TOP,0,1);CLEAN();
    nvnCommandBufferWaitEvent(&command,&event,NVN_EVENT_WAIT_MODE_EQUAL,1);CLEAN();
    nvnCommandBufferSignalEvent(&command,&event,NVN_EVENT_SIGNAL_MODE_WRITE,NVN_EVENT_SIGNAL_LOCATION_BOTTOM,0,2);CLEAN();
    nvnCommandBufferBarrier(&command,NVN_BARRIER_ORDER_PRIMITIVES_BIT|NVN_BARRIER_INVALIDATE_TEXTURE_BIT);CLEAN();
    used=peek(&command)->record->used;nvnCommandBufferBarrier(&command,NVN_BARRIER_INVALIDATE_SHADER_BIT);ERROR(ENOTSUP);CHECK(peek(&command)->record->used==used);
    CHECK(!nvnCommandBufferEndRecording(&command));ERROR(EINVAL);CHECK(nvnCommandBufferIsRecording(&command));
    nvnCommandBufferPopDebugGroup(&command);CLEAN();NVNcommandHandle h=nvnCommandBufferEndRecording(&command);CHECK(h);CLEAN();
    CHECK(!nvnCommandBufferIsRecording(&command));CLEAN();CHECK(handles->chunks&&handles->chunks->next);
    nvnEventFinalize(&event);ERROR(EBUSY);
    nvnQueueSubmitCommands(&q1,1,&h);CLEAN();CHECK(nn_test_accepted()==1);
    CHECK(peek(&q1)->queue_state->submissions==1&&peek(&q1)->queue_state->command_bytes==handles->bytes);
    CHECK(contains((char*)peek(&q1)->queue_state+4096,handles->bytes,"original-label"));
    /* RAM model does not execute the recorded event packets. */
    CHECK(nvnEventGetValue(&event)==0);CLEAN();
    nvnQueueFenceSync(&q1,&sync,NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE,0);CLEAN();
    CHECK(nvnSyncWait(&sync,0)==NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED);CLEAN();
    nvnQueueWaitSync(&q2,&sync);CLEAN();CHECK(peek(&q2)->wait_count==1);
    /* An actual KGSL wait is snapshotted at handle start, never hoisted from
     * the middle of a GPU command stream. */
    NVNcommandBuffer c2={0};_Alignas(struct control) unsigned char control2[128];CHECK(nvnCommandBufferInitialize(&c2,&device));
    nvnCommandBufferAddCommandMemory(&c2,&pool,8192,4096);nvnCommandBufferAddControlMemory(&c2,control2,sizeof(control2));
    nvnCommandBufferBeginRecording(&c2);nvnCommandBufferWaitSync(&c2,&sync);CLEAN();
    nvnCommandBufferClearBuffer(&c2,nn_bo_address(peek(&pool)->bo)+12288,4,123);CLEAN();
    used=peek(&c2)->record->used;nvnCommandBufferWaitSync(&c2,&sync);ERROR(ENOTSUP);CHECK(peek(&c2)->record->used==used);
    NVNcommandHandle h2=nvnCommandBufferEndRecording(&c2);CHECK(h2);CLEAN();
    nvnQueueSubmitCommands(&q2,1,&h2);CLEAN();CHECK(nn_test_dependencies()==1);
    nvnQueueSubmitCommands(&q1,1,&h);CLEAN();CHECK(nn_test_accepted()==3);
    nvnQueueFinalize(&q1);ERROR(EBUSY);
    CHECK(nvnSyncWait(&sync,1)==NVN_SYNC_WAIT_RESULT_CONDITION_SATISFIED);CLEAN();
    CHECK(nvnSyncWait(&sync,0)==NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED);CLEAN();
    nvnQueueFenceSync(&q1,&sync,NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE,2);ERROR(ENOTSUP);
    nvnQueueFenceSync(&q1,&sync,NVN_SYNC_CONDITION_GRAPHICS_WORLD_SPACE_COMPLETE,1);CLEAN();
    CHECK(nvnSyncWait(&sync,0)==NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED);CLEAN();
    nvnSyncFinalize(&sync);CLEAN();
    nvnQueueFinish(&q1);nvnQueueFinish(&q2);CLEAN();
    nvnDeviceFinalizeCommandHandle(&device,h2);nvnDeviceFinalizeCommandHandle(&device,h);CLEAN();
    nvnCommandBufferFinalize(&c2);nvnCommandBufferFinalize(&command);CLEAN();
    nvnEventFinalize(&event);CLEAN();nvnMemoryPoolFinalize(&pool);CLEAN();free(storage);
    nvnQueueFinalize(&q2);CLEAN();nvnQueueFinalize(&q1);CLEAN();free(workspace);
    nvnDeviceFinalize(&device);CLEAN();
    while(objects){CHECK(builder_kind(objects->kind));remove_object(objects);}
    CHECK(!handles);
    printf("{\"passed\":true,\"scope\":\"HOST ONLY RAM timeline; PM4 not executed\",\"checks\":%u,\"new_apis\":26,\"accepted_model_submissions\":%u,\"kernel_dependency_models\":%u,\"gpu_submits\":0,\"game_modules_executed\":false}\n",checks,nn_test_accepted(),nn_test_dependencies());
    return 0;
}
