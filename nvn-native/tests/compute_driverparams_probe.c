/* SPDX-License-Identifier: MIT
 * Own raw-native CS qualification. Not NVN API or original game execution. */
#include "nvn_native/compute.h"
#include "nvn_native/order.h"
#include "nvn_native/pm4.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_COMPUTE_DRIVERPARAMS_HEADER

enum {CODE,DATA,ARGS,PARENT,JOB,BO_COUNT};
enum {SOURCE_X=128,RESULT=256,ARGUMENT=64,DRIVER=7168,CS_SEQUENCE=6144};
struct raw_owner {
    struct raw_owner *next;nn_device *device;nn_queue *queue;nn_bo *bo[BO_COUNT];
    unsigned char *expected[BO_COUNT];size_t parent_bytes,job_bytes;
};
static struct raw_owner *retained;
static void word(void *map,size_t offset,uint32_t value){memcpy((char*)map+offset,&value,4);}
static uint32_t read_word(const void *map,size_t offset){uint32_t v;memcpy(&v,(const char*)map+offset,4);return v;}
int nn_compute_driverparams_probe(char *json,size_t capacity)
{
    const struct nn_cs_image *image=&NVN_COMPILED_IMAGE;
    struct raw_owner *o=NULL;const char *stage="profile",*submission="not_attempted";
    int error=0,cleanup=0,pending=0;unsigned completed=0,submissions=0,retire_checks=0,busy_checks=0,zero_checks=0;
    unsigned result_checks=0,param_checks=0,args_checks=0;uint64_t all_checks=0,readonly_checks=0;
    unsigned bad_bo=~0u,bad_case=~0u,bad_actual=0,bad_expected=0;size_t bad_offset=SIZE_MAX;
    uint32_t chip=0;char cases[512]="";size_t cases_used=0;
#define TRY(x) do{if((error=(x)))goto done;}while(0)
    if(!json||!capacity)return -1;
    if(image->schema!=1||image->chip!=0x06050002||image->local[0]!=1||image->local[1]!=1||image->local[2]!=1||image->ssbo_slots!=1||image->code_bytes>4096){error=ENOTSUP;goto done;}
    o=calloc(1,sizeof(*o));if(!o){error=ENOMEM;goto done;}
    stage="open";TRY(nn_device_open(&o->device));chip=nn_device_chip(o->device);
    if(chip!=image->chip||nn_device_page(o->device)!=4096){error=ENOTSUP;goto done;}
    stage="allocate";for(unsigned i=0;i<JOB;i++){TRY(nn_bo_alloc(o->device,4096,o->bo+i));memset(nn_bo_map(o->bo[i]),0xa5,nn_bo_size(o->bo[i]));}
    memcpy(nn_bo_map(o->bo[CODE]),image->code,image->code_bytes);
    struct nn_cs_buffer ubos[16]={{0}},ssbos[16]={{0}};
    ssbos[0]=(struct nn_cs_buffer){nn_bo_address(o->bo[DATA])+RESULT,16};
    stage="encode";TRY(nn_cs_encode_indirect(o->device,o->bo[CODE],image,ubos,ssbos,nn_bo_address(o->bo[ARGS])+ARGUMENT,o->bo+JOB,&o->job_bytes));
    /* These private offsets are the explicit nn_cs_encode[_indirect] layout,
     * not a scan of machine code: descriptor4096, seq6144, params7168. */
    if(nn_bo_size(o->bo[JOB])!=8192||o->job_bytes>4096){error=ENOTSUP;goto done;}
    uint32_t *w=nn_bo_map(o->bo[PARENT]);size_t n=0;
    const uint64_t args=nn_bo_address(o->bo[ARGS])+ARGUMENT,source=nn_bo_address(o->bo[DATA])+SOURCE_X;
    w[n++]=nvn_pm4_packet7(NVN_CP_MEM_WRITE,5);w[n++]=(uint32_t)args;w[n++]=args>>32;w[n++]=1;w[n++]=1;w[n++]=1;
    /* Public adreno_pm4.xml CP_MEM_TO_MEM, same recipe as indirect driver
     * params: GPU reads sourceX on every execution of this unchanged IB. */
    w[n++]=nvn_pm4_packet7(0x73,5);w[n++]=0;w[n++]=(uint32_t)args;w[n++]=args>>32;w[n++]=(uint32_t)source;w[n++]=source>>32;
    size_t k=0;TRY(nn_order_words(chip,0,NN_ORDER_INVALIDATE,w+n,128-n,&k));n+=k;
    const uint64_t job=nn_bo_address(o->bo[JOB]);
    w[n++]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);w[n++]=(uint32_t)job;w[n++]=job>>32;w[n++]=(uint32_t)(o->job_bytes/4);
    w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0);o->parent_bytes=n*4;
    for(unsigned i=0;i<BO_COUNT;i++){o->expected[i]=malloc(nn_bo_size(o->bo[i]));if(!o->expected[i]){error=ENOMEM;goto done;}}
    stage="queue";TRY(nn_queue_create(o->device,&o->queue));
    for(unsigned repeat=0;repeat<2;repeat++){
        bad_case=repeat;const uint32_t groups[3]={repeat?3u:1u,1,1};
        unsigned char *data=nn_bo_map(o->bo[DATA]),*a=nn_bo_map(o->bo[ARGS]),*j=nn_bo_map(o->bo[JOB]);
        memset(data,0xa5,4096);word(data,SOURCE_X,groups[0]);memset(a+ARGUMENT,0,12);memset(j+DRIVER,0,12);word(j,CS_SEQUENCE,0);
        for(unsigned axis=0;axis<3;axis++)if(read_word(a,ARGUMENT+axis*4)){error=EIO;goto done;}
        ++zero_checks;
        for(unsigned i=0;i<BO_COUNT;i++)memcpy(o->expected[i],nn_bo_map(o->bo[i]),nn_bo_size(o->bo[i]));
        for(unsigned axis=0;axis<3;axis++){
            word(o->expected[DATA],RESULT+axis*4,groups[axis]);word(o->expected[ARGS],ARGUMENT+axis*4,groups[axis]);word(o->expected[JOB],DRIVER+axis*4,groups[axis]);
        }
        word(o->expected[DATA],RESULT+12,UINT32_C(0x13579bdf));word(o->expected[JOB],CS_SEQUENCE,2); /* A650 CS CACHE_FLUSH_TS value2 */
        for(unsigned i=0;i<BO_COUNT;i++)TRY(nn_bo_cache(o->bo[i],0,nn_bo_size(o->bo[i]),0));
        nn_bo *resources[]={o->bo[CODE],o->bo[DATA],o->bo[ARGS],o->bo[JOB]};uint32_t timestamp=0;
        stage="submit";submission="unknown";pending=1;TRY(nn_queue_submit(o->queue,o->bo[PARENT],0,o->parent_bytes,resources,4,&timestamp));submission="accepted";++submissions;
        if(nn_bo_destroy(o->bo[DATA])!=EBUSY){error=EINVAL;goto done;}++busy_checks;
        stage="finish";TRY(nn_queue_finish(o->queue,1000));pending=0;++retire_checks;
        for(unsigned i=0;i<BO_COUNT;i++)TRY(nn_bo_cache(o->bo[i],0,nn_bo_size(o->bo[i]),1));
        stage="shader-result";
        for(unsigned axis=0;axis<4;axis++){
            uint32_t expected=axis<3?groups[axis]:UINT32_C(0x13579bdf);
            if(read_word(data,RESULT+axis*4)!=expected){bad_bo=DATA;bad_offset=RESULT+axis*4;bad_actual=read_word(data,bad_offset);bad_expected=expected;error=EIO;goto done;}++result_checks;
            expected=axis<3?groups[axis]:3;
            if(read_word(j,DRIVER+axis*4)!=expected){bad_bo=JOB;bad_offset=DRIVER+axis*4;bad_actual=read_word(j,bad_offset);bad_expected=expected;error=EIO;goto done;}++param_checks;
            if(axis<3){if(read_word(a,ARGUMENT+axis*4)!=groups[axis]){error=EIO;goto done;}++args_checks;}
        }
        stage="all-bytes";
        for(unsigned i=0;i<BO_COUNT;i++){
            const unsigned char *actual=nn_bo_map(o->bo[i]);
            for(size_t offset=0;offset<nn_bo_size(o->bo[i]);offset++){
                if(actual[offset]!=o->expected[i][offset]){bad_bo=i;bad_offset=offset;bad_actual=actual[offset];bad_expected=o->expected[i][offset];error=EIO;goto done;}++all_checks;
                if(!((i==DATA&&offset>=RESULT&&offset<RESULT+16)||(i==ARGS&&offset>=ARGUMENT&&offset<ARGUMENT+12)||(i==JOB&&((offset>=DRIVER&&offset<DRIVER+12)||(offset>=CS_SEQUENCE&&offset<CS_SEQUENCE+4)))))++readonly_checks;
            }
        }
        int bytes=snprintf(cases+cases_used,sizeof(cases)-cases_used,"%s{\"replay\":%u,\"groups\":[%u,1,1],\"observed\":[%u,1,1,324508639],\"passed\":true}",cases_used?",":"",repeat,groups[0],read_word(data,RESULT));
        if(bytes<0||(size_t)bytes>=sizeof(cases)-cases_used){error=ENOSPC;goto done;}cases_used+=(size_t)bytes;++completed;
    }
    stage="complete";
done:
    if(pending)cleanup=EBUSY;
    else if(o){
        if(o->queue){int e=nn_queue_destroy(o->queue);if(e&&!cleanup)cleanup=e;if(!e)o->queue=NULL;}
        for(unsigned i=0;i<BO_COUNT;i++)if(o->bo[i]){int e=nn_bo_destroy(o->bo[i]);if(e&&!cleanup)cleanup=e;if(!e)o->bo[i]=NULL;}
        if(o->device){int e=nn_device_close(o->device);if(e&&!cleanup)cleanup=e;if(!e)o->device=NULL;}
    }
    if(o){if(cleanup){o->next=retained;retained=o;}else{for(unsigned i=0;i<BO_COUNT;i++)free(o->expected[i]);free(o);}}
    int passed=!error&&!cleanup&&completed==2&&submissions==2&&retire_checks==2&&busy_checks==2&&zero_checks==2&&result_checks==8&&param_checks==8&&args_checks==6&&all_checks==49152&&readonly_checks==49064;
    int json_bytes=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"kgsl_compute_driverparams\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"chip_id\":%u,\"submission\":\"%s\",\"completed_cases\":%u,\"submissions\":%u,\"retire_checks\":%u,\"inflight_destroy_rejections\":%u,\"cpu_zero_group_checks\":%u,\"result_word_checks\":%u,\"private_parameter_word_checks\":%u,\"argument_word_checks\":%u,\"all_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"original_game_shader\":false,\"nvn_api\":false,\"runtime_compiler\":false,\"driverparams_consumed\":%s,\"reused_native_ibs\":%s,\"resources_retained\":%s,\"bad_case\":%u,\"bad_bo\":%u,\"bad_offset\":%zu,\"observed\":%u,\"expected\":%u,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),stage,error,cleanup,chip,submission,completed,submissions,retire_checks,busy_checks,zero_checks,result_checks,param_checks,args_checks,(unsigned long long)all_checks,(unsigned long long)readonly_checks,passed?"true":"false",completed==2?"true":"false",cleanup?"true":"false",passed?~0u:bad_case,bad_bo,bad_offset,bad_actual,bad_expected,cases,passed?"true":"false");
#undef TRY
    return passed&&json_bytes>=0&&(size_t)json_bytes<capacity?0:-1;
}
