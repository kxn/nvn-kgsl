/* SPDX-License-Identifier: MIT
 * Authored compute qualification, never an NVN storage getter/game frame.
 * Actual nn_cs_encode + KGSL queues, no substitute GPU/CPU converter. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/compute.h"
#include "nvn_native/pm4.h"
#include "texture_conversion_cases.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_TEXTURE_CONVERSION_HEADER

enum {SOURCE,NATIVE,ROUNDTRIP,UNIFORM,PARENT,DATA_COUNT,MAX_JOBS=90};
struct tc_job {nn_bo *bo;size_t bytes,sequence_offset;unsigned char *expected;};
struct tc_active {nn_bo *bo[DATA_COUNT];unsigned char *expected[DATA_COUNT],*written[2];struct tc_job jobs[MAX_JOBS];unsigned count;size_t parent_bytes;};
struct tc_owner {struct tc_owner *next;nn_device *device;nn_queue *queue;nn_bo *code[2];unsigned char *code_expected[2];struct tc_active *active;};
static struct tc_owner *retained;
static int compare(const void *actual,const void *expected,size_t bytes,uint64_t *checks,size_t *bad)
{
    const unsigned char *a=actual,*e=expected;
    for(size_t i=0;i<bytes;i++){if(a[i]!=e[i]){*bad=i;return EIO;}++*checks;}return 0;
}
static int job_expected(const struct nn_cs_image *image,struct tc_job *j)
{
    size_t size=nn_bo_size(j->bo),sequence=SIZE_MAX;uint32_t *words=nn_bo_map(j->bo);
    j->expected=malloc(size);if(!j->expected)return ENOMEM;memcpy(j->expected,words,size);
    /* Walk typed address patches only, never scan arbitrary command words. */
    for(size_t i=0;i<image->patch_count;i++)if(image->patches[i].kind==NN_CS_SEQUENCE){
        size_t p=image->patches[i].word;if(p+1>=image->command_words)return EINVAL;
        uint64_t a=words[p]|(uint64_t)words[p+1]<<32,base=nn_bo_address(j->bo);
        if(a<base||a-base>size-4||(a&3))return EINVAL;
        size_t offset=(size_t)(a-base);if(sequence!=SIZE_MAX&&sequence!=offset)return EINVAL;sequence=offset;
    }
    if(sequence==SIZE_MAX)return EINVAL;
    j->sequence_offset=sequence;
    const uint32_t final_sequence=2; /* Existing A650 common encoder tail. */
    memcpy(j->expected+sequence,&final_sequence,4);return 0;
}
static int destroy_active(struct tc_active *a)
{
    if(!a)return 0;
    for(unsigned i=0;i<a->count;i++)if(a->jobs[i].bo){int e=nn_bo_destroy(a->jobs[i].bo);if(e)return e;a->jobs[i].bo=NULL;}
    for(unsigned i=0;i<DATA_COUNT;i++)if(a->bo[i]){int e=nn_bo_destroy(a->bo[i]);if(e)return e;a->bo[i]=NULL;}
    for(unsigned i=0;i<a->count;i++)free(a->jobs[i].expected);
    for(unsigned i=0;i<DATA_COUNT;i++)free(a->expected[i]);
    free(a->written[0]);free(a->written[1]);free(a);return 0;
}
static int write_raw(const char *directory,unsigned index,unsigned repeat,const struct tc_active *a)
{
    char path[1200];int n=snprintf(path,sizeof(path),"%s/layout-%02u-r%u.bin",directory,index,repeat);
    if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;
    FILE *f=fdopen(fd,"wb");if(!f){int e=errno;close(fd);return e;}
    int error=0;for(unsigned i=SOURCE;i<=ROUNDTRIP;i++)if(fwrite(nn_bo_map(a->bo[i]),1,nn_bo_size(a->bo[i]),f)!=nn_bo_size(a->bo[i]))error=EIO;
    if(fflush(f)||fsync(fd))error=EIO;
    if(fclose(f))error=EIO;
    return error;
}
int nn_texture_conversion_probe(const char *directory,char *json,size_t capacity)
{
    const struct nn_cs_image *images[]={nn_tc_gob_copy,nn_tc_linear_copy};
    struct tc_owner *o=NULL;const char *stage="profile",*submission="not_attempted";int error=0,cleanup=0,pending=0;
    uint32_t chip=0;unsigned completed=0,submissions=0,finishes=0,dispatches=0,replays=0,busy=0,files=0,sequence_checks=0;
    uint64_t data_checks=0,readonly_checks=0,guard_checks=0;unsigned bad_case=~0u,bad_repeat=~0u,bad_bo=~0u;size_t bad_offset=SIZE_MAX;
    char cases[16000]="";size_t case_used=0;
#define TRY(x) do{if((error=(x)))goto done;}while(0)
    if(!directory||!json||!capacity)return -1;
    if(NN_TC_CASE_COUNT!=55||sysconf(_SC_PAGESIZE)!=4096){error=ENOTSUP;goto done;}
    for(unsigned i=0;i<2;i++)if(!images[i]||images[i]->schema!=1||images[i]->chip!=0x06050002||images[i]->local[0]!=8||images[i]->local[1]!=8||images[i]->local[2]!=1||images[i]->ssbo_slots!=2){error=ENOTSUP;goto done;}
    o=calloc(1,sizeof(*o));if(!o){error=ENOMEM;goto done;}
    stage="open";TRY(nn_device_open(&o->device));chip=nn_device_chip(o->device);
    if(chip!=0x06050002||nn_device_page(o->device)!=4096){error=ENOTSUP;goto done;}
    TRY(nn_queue_create(o->device,&o->queue));
    for(unsigned k=0;k<2;k++){
        TRY(nn_bo_alloc(o->device,images[k]->code_bytes,&o->code[k]));size_t size=nn_bo_size(o->code[k]);
        memset(nn_bo_map(o->code[k]),0xa5,size);memcpy(nn_bo_map(o->code[k]),images[k]->code,images[k]->code_bytes);
        o->code_expected[k]=malloc(size);if(!o->code_expected[k]){error=ENOMEM;goto done;}memcpy(o->code_expected[k],nn_bo_map(o->code[k]),size);
    }
    for(unsigned n=0;n<NN_TC_CASE_COUNT;n++){
        bad_case=n;const struct nn_tc_case *c=nn_tc_cases+n;struct nn_tex_source_layout source;TRY(nn_tex_source_layout(&c->desc,&source));
        if(source.payload_bytes!=c->source_bytes||source.layer_stride!=c->source_layer_stride||source.linear!=c->linear||source.levels!=c->levels||source.layers!=c->layers||!c->levels||c->levels>15||!c->layers||c->layers>3||c->levels*c->layers*2>MAX_JOBS||!c->native_bytes||c->native_bytes>16*1024*1024u||c->source_bytes>16*1024*1024u||(!source.linear&&(source.alignment||source.public_extent_known))){error=EINVAL;goto done;}
        struct tc_active *a=calloc(1,sizeof(*a));if(!a){error=ENOMEM;goto done;}o->active=a;stage="allocate";
        const size_t sizes[DATA_COUNT]={c->source_bytes+2*NN_TC_SOURCE_BASE,c->native_bytes+NN_TC_NATIVE_BASE+4096,c->source_bytes+2*NN_TC_SOURCE_BASE,4096,4096};
        for(unsigned i=0;i<DATA_COUNT;i++){
            TRY(nn_bo_alloc(o->device,sizes[i],a->bo+i));memset(nn_bo_map(a->bo[i]),0xa5,nn_bo_size(a->bo[i]));
            a->expected[i]=malloc(nn_bo_size(a->bo[i]));if(!a->expected[i]){error=ENOMEM;goto done;}
        }
        for(unsigned i=0;i<2;i++){a->written[i]=calloc(1,nn_bo_size(a->bo[NATIVE+i]));if(!a->written[i]){error=ENOMEM;goto done;}}
        stage="encode";const struct nn_cs_image *image=images[c->linear];
        for(unsigned direction=0;direction<2;direction++)for(unsigned layer=0;layer<c->layers;layer++)for(unsigned level=0;level<c->levels;level++){
            const struct nn_tc_level *m=c->level+level;const struct nn_tex_source_level *s=source.level+level;
            if(m->source_offset!=s->offset||m->source_bytes!=s->bytes||m->row_bytes!=s->row_bytes||m->rows!=s->blocks_y||m->source_pitch!=s->pitch||m->block_height_log2!=s->block_height_log2||!m->native_pitch||(m->native_pitch&3)||m->native_pitch<m->row_bytes||m->native_bytes!=(uint64_t)m->native_pitch*m->rows){error=EINVAL;goto done;}
            uint64_t sb=NN_TC_SOURCE_BASE+(uint64_t)layer*c->source_layer_stride+m->source_offset;
            uint64_t nb=NN_TC_NATIVE_BASE+(uint64_t)layer*m->native_layer_stride+m->native_offset;
            if(sb+m->source_bytes>NN_TC_SOURCE_BASE+(uint64_t)c->source_bytes||nb+m->native_bytes>NN_TC_NATIVE_BASE+(uint64_t)c->native_bytes||(sb&3)||(nb&3)){error=EINVAL;goto done;}
            unsigned char uniform[64]={0};
            if(c->linear){struct nn_tex_linear_copy_args args;TRY(nn_tex_linear_copy_args(&source,direction,m->native_pitch,m->native_bytes,&args));args.source_base=(uint32_t)(direction?nb:sb);args.destination_base=(uint32_t)(direction?sb:nb);memcpy(uniform,&args,sizeof(args));}
            else{struct nn_tex_gob_copy_args args;TRY(nn_tex_gob_copy_args(&source,level,direction,m->native_pitch,m->native_bytes,&args));args.source_base=(uint32_t)(direction?nb:sb);args.destination_base=(uint32_t)(direction?sb:nb);memcpy(uniform,&args,sizeof(args));}
            size_t uoffset=64+(size_t)a->count*64;if(uoffset+64>nn_bo_size(a->bo[UNIFORM])){error=ENOSPC;goto done;}
            memcpy((char*)nn_bo_map(a->bo[UNIFORM])+uoffset,uniform,64);
            struct nn_cs_buffer ubos[16]={{0}},ssbos[16]={{0}};ubos[2]=(struct nn_cs_buffer){nn_bo_address(a->bo[UNIFORM])+uoffset,64};
            ssbos[0]=(struct nn_cs_buffer){nn_bo_address(a->bo[direction?NATIVE:SOURCE]),(uint32_t)nn_bo_size(a->bo[direction?NATIVE:SOURCE])};
            ssbos[1]=(struct nn_cs_buffer){nn_bo_address(a->bo[direction?ROUNDTRIP:NATIVE]),(uint32_t)nn_bo_size(a->bo[direction?ROUNDTRIP:NATIVE])};
            const uint32_t groups[3]={((m->row_bytes+3)/4+7)/8,(m->rows+7)/8,1};
            struct tc_job *j=a->jobs+a->count;TRY(nn_cs_encode(o->device,o->code[c->linear],image,ubos,ssbos,groups,&j->bo,&j->bytes));++a->count;TRY(job_expected(image,j));
        }
        uint32_t *parent=nn_bo_map(a->bo[PARENT]);size_t words=0;
        for(unsigned i=0;i<a->count;i++){uint64_t address=nn_bo_address(a->jobs[i].bo);parent[words++]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);parent[words++]=(uint32_t)address;parent[words++]=(uint32_t)(address>>32);parent[words++]=(uint32_t)(a->jobs[i].bytes/4);}
        parent[words++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);parent[words++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0);a->parent_bytes=words*4;
        if(a->parent_bytes>nn_bo_size(a->bo[PARENT])){error=ENOSPC;goto done;}
        for(unsigned repeat=0;repeat<NN_TC_REPEATS;repeat++){
            bad_repeat=repeat;stage="initialize-pattern";
            memset(nn_bo_map(a->bo[SOURCE]),0xa5,nn_bo_size(a->bo[SOURCE]));
            for(uint32_t b=0;b<c->source_bytes;b++)((unsigned char*)nn_bo_map(a->bo[SOURCE]))[NN_TC_SOURCE_BASE+b]=nn_tc_pattern(b,n,repeat);
            memset(nn_bo_map(a->bo[NATIVE]),0xa5,nn_bo_size(a->bo[NATIVE]));memset(nn_bo_map(a->bo[ROUNDTRIP]),0x5a,nn_bo_size(a->bo[ROUNDTRIP]));
            for(unsigned i=0;i<DATA_COUNT;i++)memcpy(a->expected[i],nn_bo_map(a->bo[i]),nn_bo_size(a->bo[i]));
            /* Expected shadows are test-only closed-form values, never CPU
             * copies into a GPU destination or reads of source as oracle. */
            for(unsigned layer=0;layer<c->layers;layer++)for(unsigned level=0;level<c->levels;level++){
                const struct nn_tc_level *m=c->level+level;
                for(unsigned y=0;y<m->rows;y++)for(unsigned x=0;x<m->row_bytes;x++){
                    uint64_t offset;TRY(nn_tex_source_offset(&source,level,layer,x/source.bytes_per_block,y,x%source.bytes_per_block,&offset));
                    size_t native=NN_TC_NATIVE_BASE+(size_t)layer*m->native_layer_stride+m->native_offset+(size_t)y*m->native_pitch+x,rt=NN_TC_SOURCE_BASE+(size_t)offset;
                    if(native>=nn_bo_size(a->bo[NATIVE])||rt>=nn_bo_size(a->bo[ROUNDTRIP])){error=EINVAL;goto done;}
                    if(!repeat&&(a->written[0][native]||a->written[1][rt])){error=EINVAL;goto done;}
                    a->written[0][native]=a->written[1][rt]=1;unsigned char value=nn_tc_pattern((uint32_t)offset,n,repeat);
                    a->expected[NATIVE][native]=value;a->expected[ROUNDTRIP][rt]=value;
                }
            }
            stage="cache-clean";for(unsigned i=0;i<2;i++)TRY(nn_bo_cache(o->code[i],0,nn_bo_size(o->code[i]),0));
            for(unsigned i=0;i<DATA_COUNT;i++)TRY(nn_bo_cache(a->bo[i],0,nn_bo_size(a->bo[i]),0));
            for(unsigned i=0;i<a->count;i++)TRY(nn_bo_cache(a->jobs[i].bo,0,nn_bo_size(a->jobs[i].bo),0));
            nn_bo *refs[MAX_JOBS+6];size_t refs_count=0;refs[refs_count++]=o->code[c->linear];
            for(unsigned i=SOURCE;i<=UNIFORM;i++)refs[refs_count++]=a->bo[i];
            for(unsigned i=0;i<a->count;i++)refs[refs_count++]=a->jobs[i].bo;
            uint32_t timestamp=0;stage="submit";submission="unknown";pending=1;
            TRY(nn_queue_submit(o->queue,a->bo[PARENT],0,a->parent_bytes,refs,refs_count,&timestamp));submission="accepted";++submissions;dispatches+=a->count;
            if(nn_bo_destroy(a->bo[SOURCE])!=EBUSY){error=EINVAL;goto done;}++busy;
            stage="finish";TRY(nn_queue_finish(o->queue,1000));pending=0;++finishes;
            stage="cache-invalidate";for(unsigned i=0;i<2;i++)TRY(nn_bo_cache(o->code[i],0,nn_bo_size(o->code[i]),1));
            for(unsigned i=0;i<DATA_COUNT;i++)TRY(nn_bo_cache(a->bo[i],0,nn_bo_size(a->bo[i]),1));
            for(unsigned i=0;i<a->count;i++)TRY(nn_bo_cache(a->jobs[i].bo,0,nn_bo_size(a->jobs[i].bo),1));
            stage="all-bytes";
            for(unsigned i=0;i<2&&!error;i++)error=compare(nn_bo_map(o->code[i]),o->code_expected[i],nn_bo_size(o->code[i]),&readonly_checks,&bad_offset);
            for(unsigned i=0;i<DATA_COUNT&&!error;i++){
                bad_bo=i;error=compare(nn_bo_map(a->bo[i]),a->expected[i],nn_bo_size(a->bo[i]),i==NATIVE||i==ROUNDTRIP?&data_checks:&readonly_checks,&bad_offset);
                if(i==NATIVE||i==ROUNDTRIP)for(size_t b=0;b<nn_bo_size(a->bo[i]);b++)if(!a->written[i-NATIVE][b])++guard_checks;
            }
            for(unsigned i=0;i<a->count&&!error;i++){bad_bo=DATA_COUNT+i;uint64_t checked=0;error=compare(nn_bo_map(a->jobs[i].bo),a->jobs[i].expected,nn_bo_size(a->jobs[i].bo),&checked,&bad_offset);if(!error){readonly_checks+=checked-4;++sequence_checks;}}
            int write_error=write_raw(directory,n,repeat,a);if(!write_error)++files;if(!error)error=write_error;if(error)goto done;
            ++completed;if(repeat)++replays;
        }
        int k=snprintf(cases+case_used,sizeof(cases)-case_used,"%s{\"case\":%u,\"source_bo_bytes\":%zu,\"native_bo_bytes\":%zu,\"roundtrip_bo_bytes\":%zu,\"jobs\":%u,\"repeats\":2}",case_used?",":"",n,nn_bo_size(a->bo[SOURCE]),nn_bo_size(a->bo[NATIVE]),nn_bo_size(a->bo[ROUNDTRIP]),a->count);
        if(k<0||(size_t)k>=sizeof(cases)-case_used){error=ENOSPC;goto done;}case_used+=(size_t)k;
        stage="retire-case";error=destroy_active(a);if(error){cleanup=error;goto done;}o->active=NULL;
    }
    stage="complete";
done:
    if(pending)cleanup=EBUSY;
    else if(o&&!cleanup){
        if(o->active){cleanup=destroy_active(o->active);if(!cleanup)o->active=NULL;}
        if(!cleanup&&o->queue){cleanup=nn_queue_destroy(o->queue);if(!cleanup)o->queue=NULL;}
        for(unsigned i=0;i<2&&!cleanup;i++)if(o->code[i]){cleanup=nn_bo_destroy(o->code[i]);if(!cleanup)o->code[i]=NULL;}
        if(!cleanup&&o->device){cleanup=nn_device_close(o->device);if(!cleanup)o->device=NULL;}
    }
    if(o){if(cleanup){o->next=retained;retained=o;}else{for(unsigned i=0;i<2;i++)free(o->code_expected[i]);free(o);}}
    int passed=!error&&!cleanup&&completed==110&&submissions==110&&finishes==110&&dispatches==612&&sequence_checks==612&&replays==55&&busy==110&&files==110&&data_checks&&guard_checks&&readonly_checks;
    int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"texture_conversion\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"chip_id\":%u,\"submission\":\"%s\",\"completed_cases\":%u,\"submissions\":%u,\"finishes\":%u,\"dispatches\":%u,\"sequence_word_checks\":%u,\"replays\":%u,\"inflight_destroy_rejections\":%u,\"files\":%u,\"data_byte_checks\":%llu,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"resources_retained\":%s,\"original_game_shader\":false,\"nvn_api\":false,\"runtime_compiler\":false,\"cpu_image_conversion\":false,\"public_tiled_storage_getters_qualified\":false,\"compressible_qualified\":false,\"bad_case\":%u,\"bad_repeat\":%u,\"bad_bo\":%u,\"bad_offset\":%zu,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),stage,error,cleanup,chip,submission,completed,submissions,finishes,dispatches,sequence_checks,replays,busy,files,(unsigned long long)data_checks,(unsigned long long)guard_checks,(unsigned long long)readonly_checks,cleanup?"true":"false",passed?~0u:bad_case,passed?~0u:bad_repeat,bad_bo,bad_offset,cases,passed?"true":"false");
#undef TRY
    return passed&&n>=0&&(size_t)n<capacity?0:-1;
}
