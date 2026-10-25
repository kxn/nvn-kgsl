/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200112L
#include "nvn_native/nvn.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_MAXWELL_HEADER
#define GUARDED(type) struct {uint64_t before[2];type value;uint64_t after[2];}
enum { DATA_BYTES=16384,SOURCE_OFFSET=260,TARGET_OFFSET=4356,UBO_OFFSET=10240,
       GROUP_SOURCE_OFFSET=10496,INDIRECT_OFFSET=10560,CODE_OFFSET=12288,CODE_STRIDE=512,LIMIT=65 };
struct fixture {
    struct fixture *next;
    GUARDED(NVNdevice) device;GUARDED(NVNqueue) queue;GUARDED(NVNcommandBuffer) command;
    GUARDED(NVNmemoryPool) data_pool,command_pool;GUARDED(NVNprogram) programs[4];
    _Alignas(16) unsigned char control[512];
    unsigned char *data,*commands,expected[DATA_BYTES],command_shadow[DATA_BYTES];
};
static struct fixture *quarantine;
static uint64_t original_input(unsigned i,unsigned repeat)
{
    static const uint64_t edges[]={0,UINT64_MAX,0xffffffff,UINT64_C(0x100000000),UINT64_C(0x8000000000000000),UINT64_C(0x7fffffffffffffff)};
    uint64_t x=edges[i%6]^(UINT64_C(0x123456789abcdef)*(i/6));
    return repeat?x^UINT64_C(0xfedcba9801234567):x;
}
/* Original four kernels' closed-form copy/difference oracle, independent of
 * target instructions; never executes/interprets shader machine code on CPU. */
static uint64_t original_result(const unsigned char *source,unsigned i,unsigned program)
{
    const size_t offset=(size_t)i*test_oracles[program].stride+test_oracles[program].offset;
    uint64_t a,b;memcpy(&a,source+offset,8);
    if(test_oracles[program].delta){memcpy(&b,source+offset+16,8);a=b-a;}
    return a;
}
static int check_guard(const void *p,size_t size,unsigned value,uint64_t *checks)
{
    const unsigned char *v=p;for(size_t i=0;i<size;i++){if(v[i]!=value)return EIO;++*checks;}return 0;
}
#ifndef NVN_COMPUTE_CALL
#define NVN_COMPUTE_CALL 0
#endif
#if NVN_COMPUTE_CALL
#include "maxwell_compute_call_probe.inc"
#else
int nn_maxwell_compute_probe(char *json,size_t capacity)
{
    const char *stage="resolver",*submission="not_attempted";
    int error=0,cleanup=0,pending=0,device_ready=0,queue_ready=0,data_ready=0,arena_ready=0,command_ready=0;
    unsigned program_ready[4]={0},completed=0,submissions=0,recorded=0,released=0,replays=0;
    unsigned gpu_groups=0,zero_checks=0,uniform_checks=0,busy_checks=0,barriers=0;
    uint64_t data_checks=0,command_checks=0,output_checks=0,readonly_checks=0,guard_checks=0;
    unsigned current_program=~0u,current_repeat=~0u;size_t bad_offset=SIZE_MAX;unsigned bad_actual=0,bad_expected=0;
    char cases[2400]="";size_t cases_used=0,control_used=0;NVNcommandHandle handle=0;
    struct fixture *f=NULL;NVNdeviceBuilder db;NVNqueueBuilder qb;NVNmemoryPoolBuilder pb;
#define ENTRY(name,type) type name=(type)nvnBootstrapLoader(#name);if(!name)error=ENOSYS;
#include "../src/nvn_entries.inc"
#undef ENTRY
#define CALL(x) do{x;if((error=nn_nvn_error()))goto done;}while(0)
#define BOOL(x) do{NVNboolean ok_=(x);int e_=nn_nvn_error();if(!ok_||e_){error=e_?e_:EIO;goto done;}}while(0)
#define REQUIRE(x) do{int ok_=!!(x);int e_=nn_nvn_error();if(!ok_||e_){error=e_?e_:EINVAL;goto done;}}while(0)
#define GUARD(o) do{if((error=check_guard((o).before,sizeof((o).before),0x5a,&guard_checks))||(error=check_guard((o).after,sizeof((o).after),0x5a,&guard_checks)))goto done;}while(0)
    if(!json||!capacity)return -1;
    if(error)goto done;
    nn_nvn_clear_error();stage="profile";
    if(sysconf(_SC_PAGESIZE)!=4096){error=ENOTSUP;goto done;}
    for(unsigned p=0;p<4;p++){
        const struct nn_nvn_compiled_cs *c=test_programs[p];
        if(!c||!c->image||c->image->chip!=0x06050002||c->source_bytes>CODE_STRIDE||!c->source_bytes||c->control_bytes!=0x898||c->image->local[0]!=32||c->image->local[1]!=1||c->image->local[2]!=1){error=ENOTSUP;goto done;}
    }
    f=calloc(1,sizeof(*f));if(!f){error=ENOMEM;goto done;}
    memset(&f->device,0x5a,sizeof(f->device));memset(&f->queue,0x5a,sizeof(f->queue));memset(&f->command,0x5a,sizeof(f->command));
    memset(&f->data_pool,0x5a,sizeof(f->data_pool));memset(&f->command_pool,0x5a,sizeof(f->command_pool));memset(f->programs,0x5a,sizeof(f->programs));
    if((error=posix_memalign((void**)&f->data,4096,DATA_BYTES))||(error=posix_memalign((void**)&f->commands,4096,DATA_BYTES)))goto done;
    memset(f->data,0xa5,DATA_BYTES);memset(f->commands,0xa5,DATA_BYTES);
    stage="initialize";CALL(nvnDeviceBuilderSetDefaults(&db));BOOL(nvnDeviceInitialize(&f->device.value,&db));device_ready=1;
    CALL(nvnQueueBuilderSetDefaults(&qb));CALL(nvnQueueBuilderSetDevice(&qb,&f->device.value));BOOL(nvnQueueInitialize(&f->queue.value,&qb));queue_ready=1;
    CALL(nvnMemoryPoolBuilderSetDefaults(&pb));CALL(nvnMemoryPoolBuilderSetDevice(&pb,&f->device.value));
    CALL(nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED|NVN_MEMORY_POOL_FLAGS_SHADER_CODE));
    CALL(nvnMemoryPoolBuilderSetStorage(&pb,f->data,DATA_BYTES));BOOL(nvnMemoryPoolInitialize(&f->data_pool.value,&pb));data_ready=1;
    CALL(nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));
    CALL(nvnMemoryPoolBuilderSetStorage(&pb,f->commands,DATA_BYTES));BOOL(nvnMemoryPoolInitialize(&f->command_pool.value,&pb));arena_ready=1;
    REQUIRE(nvnMemoryPoolMap(&f->data_pool.value)==f->data&&nvnMemoryPoolMap(&f->command_pool.value)==f->commands);
    NVNbufferAddress base=nvnMemoryPoolGetBufferAddress(&f->data_pool.value);REQUIRE(base);
    for(unsigned p=0;p<4;p++){
        stage="program";if((error=nn_nvn_register_compute(&f->device.value,test_programs[p])))goto done;
        memcpy(f->data+CODE_OFFSET+p*CODE_STRIDE,test_programs[p]->source_code,test_programs[p]->source_bytes);
        BOOL(nvnProgramInitialize(&f->programs[p].value,&f->device.value));program_ready[p]=1;
        NVNshaderData shader={base+CODE_OFFSET+p*CODE_STRIDE,test_programs[p]->source_control};
        BOOL(nvnProgramSetShaders(&f->programs[p].value,1,&shader));
    }
    for(unsigned p=0;p<4;p++){
        current_program=p;stage="record";memset(f->commands,0xa5,DATA_BYTES);memset(f->control,0xa5,sizeof(f->control));
        const uint32_t zeros[3]={0};memcpy(f->data+INDIRECT_OFFSET,zeros,12);memset(f->data+UBO_OFFSET,0,64);
        BOOL(nvnCommandBufferInitialize(&f->command.value,&f->device.value));command_ready=1;
        CALL(nvnCommandBufferAddCommandMemory(&f->command.value,&f->command_pool.value,0,DATA_BYTES));
        CALL(nvnCommandBufferAddControlMemory(&f->command.value,f->control,sizeof(f->control)));
        CALL(nvnCommandBufferBeginRecording(&f->command.value));
        CALL(nvnCommandBufferBindProgram(&f->command.value,&f->programs[p].value,NVN_SHADER_STAGE_COMPUTE_BIT));
        NVNbufferRange uniform={base+UBO_OFFSET,64};CALL(nvnCommandBufferBindUniformBuffers(&f->command.value,NVN_SHADER_STAGE_COMPUTE,0,1,&uniform));
        NVNbufferRange storage[2]={{base+SOURCE_OFFSET,LIMIT*test_oracles[p].stride},{base+TARGET_OFFSET,LIMIT*8}};
        CALL(nvnCommandBufferBindStorageBuffers(&f->command.value,NVN_SHADER_STAGE_COMPUTE,0,2,storage));
        REQUIRE(!memcmp(f->data+INDIRECT_OFFSET,zeros,12));++zero_checks;
        CALL(nvnCommandBufferClearBuffer(&f->command.value,base+INDIRECT_OFFSET,12,1));
        CALL(nvnCommandBufferCopyBufferToBuffer(&f->command.value,base+GROUP_SOURCE_OFFSET,base+INDIRECT_OFFSET,4,0));
        uint32_t count=LIMIT;CALL(nvnCommandBufferUpdateUniformBuffer(&f->command.value,base+UBO_OFFSET,64,0,4,&count));count=0;
        CALL(nvnCommandBufferBarrier(&f->command.value,NVN_BARRIER_ORDER_INDIRECT_DATA_BIT));++barriers;
        CALL(nvnCommandBufferDispatchComputeIndirect(&f->command.value,base+INDIRECT_OFFSET));
        REQUIRE(!memcmp(f->data+INDIRECT_OFFSET,zeros,12));++zero_checks;
        REQUIRE(!memcmp(f->data+UBO_OFFSET,&count,4));
        handle=nvnCommandBufferEndRecording(&f->command.value);REQUIRE(handle);++recorded;
        size_t used=nvnCommandBufferGetCommandMemoryUsed(&f->command.value);REQUIRE(used&&used<DATA_BYTES);
        control_used=nvnCommandBufferGetControlMemoryUsed(&f->command.value);REQUIRE(control_used&&control_used<=sizeof(f->control));
        memcpy(f->command_shadow,f->commands,DATA_BYTES);CALL(nvnCommandBufferFinalize(&f->command.value));command_ready=0;
        for(unsigned repeat=0;repeat<2;repeat++){
            current_repeat=repeat;const uint32_t groups[3]={repeat?3u:1u,1,1};const unsigned outputs=repeat?65:32;
            memset(f->data+SOURCE_OFFSET,0xa5,4096-SOURCE_OFFSET);memset(f->data+TARGET_OFFSET,0xa5,4096);
            for(unsigned i=0;i<LIMIT*test_oracles[p].stride/8;i++){uint64_t v=original_input(i,repeat);memcpy(f->data+SOURCE_OFFSET+i*8,&v,8);}
            memset(f->data+UBO_OFFSET,0,64);memcpy(f->data+INDIRECT_OFFSET,zeros,12);memcpy(f->data+GROUP_SOURCE_OFFSET,groups,4);
            REQUIRE(!memcmp(f->data+INDIRECT_OFFSET,zeros,12));++zero_checks;
            memcpy(f->expected,f->data,DATA_BYTES);memcpy(f->expected+INDIRECT_OFFSET,groups,12);const uint32_t limit=LIMIT;memcpy(f->expected+UBO_OFFSET,&limit,4);
            for(unsigned i=0;i<outputs;i++){uint64_t v=original_result(f->data+SOURCE_OFFSET,i,p);memcpy(f->expected+TARGET_OFFSET+i*8,&v,8);}
            CALL(nvnMemoryPoolFlushMappedRange(&f->data_pool.value,0,DATA_BYTES));CALL(nvnMemoryPoolFlushMappedRange(&f->command_pool.value,0,DATA_BYTES));
            stage="submit";submission="unknown";pending=1;CALL(nvnQueueSubmitCommands(&f->queue.value,1,&handle));submission="accepted";++submissions;if(repeat)++replays;
            nvnProgramFinalize(&f->programs[p].value);if(nn_nvn_error()!=EBUSY){error=EINVAL;goto done;}++busy_checks;nn_nvn_clear_error();
            stage="finish";CALL(nvnQueueFinish(&f->queue.value));pending=0;
            CALL(nvnMemoryPoolInvalidateMappedRange(&f->data_pool.value,0,DATA_BYTES));CALL(nvnMemoryPoolInvalidateMappedRange(&f->command_pool.value,0,DATA_BYTES));
            stage="readback";
            REQUIRE(!memcmp(f->data+INDIRECT_OFFSET,groups,12));++gpu_groups;REQUIRE(!memcmp(f->data+UBO_OFFSET,&limit,4));++uniform_checks;
            for(size_t i=0;i<DATA_BYTES;i++){
                if(f->data[i]!=f->expected[i]){bad_offset=i;bad_actual=f->data[i];bad_expected=f->expected[i];error=EIO;goto done;}++data_checks;
                if(i>=TARGET_OFFSET&&i<TARGET_OFFSET+outputs*8)++output_checks;
                if((i<TARGET_OFFSET||i>=TARGET_OFFSET+LIMIT*8)&&(i<INDIRECT_OFFSET||i>=INDIRECT_OFFSET+12)&&(i<UBO_OFFSET||i>=UBO_OFFSET+4))++readonly_checks;
                if(f->commands[i]!=f->command_shadow[i]){bad_offset=i;bad_actual=f->commands[i];bad_expected=f->command_shadow[i];error=EIO;goto done;}++command_checks;
            }
            GUARD(f->device);GUARD(f->queue);GUARD(f->command);GUARD(f->data_pool);GUARD(f->command_pool);for(unsigned i=0;i<4;i++){GUARD(f->programs[i]);}
            if((error=check_guard(f->control+control_used,sizeof(f->control)-control_used,0xa5,&guard_checks)))goto done;
            int n=snprintf(cases+cases_used,sizeof(cases)-cases_used,"%s{\"program\":%u,\"replay\":%u,\"groups\":[%u,1,1],\"limit\":65,\"output_elements\":%u,\"passed\":true}",cases_used?",":"",p,repeat,groups[0],outputs);
            if(n<0||(size_t)n>=sizeof(cases)-cases_used){error=ENOSPC;goto done;}cases_used+=(size_t)n;++completed;
        }
        CALL(nvnDeviceFinalizeCommandHandle(&f->device.value,handle));handle=0;++released;
    }
    stage="complete";
done:
    if(pending)cleanup=EBUSY;
    else if(f){
#define CLEAN(x) do{nn_nvn_clear_error();x;int e_=nn_nvn_error();if(e_&&!cleanup)cleanup=e_;}while(0)
        if(command_ready)CLEAN(nvnCommandBufferFinalize(&f->command.value));
        if(handle){CLEAN(nvnDeviceFinalizeCommandHandle(&f->device.value,handle));if(!cleanup)++released;}
        for(unsigned p=0;p<4;p++)if(program_ready[p])CLEAN(nvnProgramFinalize(&f->programs[p].value));
        if(data_ready)CLEAN(nvnMemoryPoolFinalize(&f->data_pool.value));
        if(arena_ready)CLEAN(nvnMemoryPoolFinalize(&f->command_pool.value));
        if(queue_ready)CLEAN(nvnQueueFinalize(&f->queue.value));
        if(device_ready)CLEAN(nvnDeviceFinalize(&f->device.value));
#undef CLEAN
    }
    if(f){if(cleanup){f->next=quarantine;quarantine=f;}else{free(f->data);free(f->commands);free(f);}}
    int passed=!error&&!cleanup&&completed==8&&submissions==8&&recorded==4&&released==4&&replays==4&&gpu_groups==8&&zero_checks==16&&uniform_checks==8&&busy_checks==8&&barriers==4&&data_checks==131072&&command_checks==131072&&output_checks==3104&&readonly_checks&&guard_checks;
    int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"nvn_maxwell_compute_indirect\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"submission\":\"%s\",\"completed_cases\":%u,\"submissions\":%u,\"handles_recorded\":%u,\"handles_released\":%u,\"handle_replays\":%u,\"gpu_group_readbacks\":%u,\"cpu_zero_group_checks\":%u,\"gpu_uniform_readbacks\":%u,\"program_busy_rejections\":%u,\"barriers_recorded\":%u,\"data_byte_checks\":%llu,\"command_byte_checks\":%llu,\"output_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"guard_byte_checks\":%llu,\"driverparams_consumed\":false,\"resources_retained\":%s,\"program\":%u,\"replay\":%u,\"mismatch_offset\":%zu,\"observed_byte\":%u,\"expected_byte\":%u,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),stage,error,cleanup,submission,completed,submissions,recorded,released,replays,gpu_groups,zero_checks,uniform_checks,busy_checks,barriers,(unsigned long long)data_checks,(unsigned long long)command_checks,(unsigned long long)output_checks,(unsigned long long)readonly_checks,(unsigned long long)guard_checks,cleanup?"true":"false",current_program,current_repeat,bad_offset,bad_actual,bad_expected,cases,passed?"true":"false");
#undef GUARD
#undef REQUIRE
#undef BOOL
#undef CALL
    return passed&&n>=0&&(size_t)n<capacity?0:-1;
}
#endif /* NVN_COMPUTE_CALL */
#undef GUARDED
