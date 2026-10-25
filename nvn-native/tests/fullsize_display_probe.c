/* SPDX-License-Identifier: MIT
 * Authored native linear-copy qualification; NO NVN Draw or game module. */
#define _POSIX_C_SOURCE 200809L
#include "fullsize_display_probe.h"
#include "nvn_native/compute.h"
#include "nvn_native/external_image.h"
#include "nvn_native/display_shape.h"
#include "nvn_native/texture_source.h"
#include "nvn_native/window_session.h"
#include "../android/ahb.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>
#include NVN_FULLSIZE_LINEAR_HEADER

enum { WIDTH=2880,HEIGHT=1620,GUARD=4096 };
struct target {
    nn_ahb *owner;nn_external_image *image;
    struct nn_external_image_view external;
    struct nn_ahb_pixel pixel;
    struct nn_display_rgba8_shape shape;
    nn_bo *source,*uniform,*job;size_t job_bytes,source_bytes;
    unsigned char *source_shadow,*job_shadow;uint64_t job_address;
    unsigned cases,pending,pending_case,pinned;
    unsigned char uniform_shadow[4096];
};
struct owner {
    nn_device *device;nn_queue *queue;nn_bo *code;
    struct target targets[3];nn_window_session *session;
    unsigned char *code_shadow;
};
struct counts {uint64_t channels,guards,readonly;unsigned read_locks,read_unlocks,owner_releases,sentinel_locks,sentinel_unlocks;};
static void pattern(unsigned test,unsigned x,unsigned y,unsigned char out[4])
{
    out[0]=out[1]=out[2]=0;out[3]=255;
    if(test==0)out[0]=255;
    else if(test==1&&x>=WIDTH/2)out[1]=255;
    else if(test==3){unsigned g=x*127u/(WIDTH-1),b=y*127u/(HEIGHT-1);out[0]=(unsigned char)(255-g-b);out[1]=(unsigned char)g;out[2]=(unsigned char)b;}
}
static int release_owner(void *p){return nn_ahb_destroy(p);}
static int equal(const void *a,const void *b,size_t n,uint64_t *checks)
{
    const unsigned char *x=a,*y=b;for(size_t i=0;i<n;i++){if(x[i]!=y[i])return EIO;(*checks)++;}return 0;
}
static int same_pixel(const struct target *t,const struct nn_ahb_view *v)
{
    return !v->address||v->bytes_per_pixel!=4||v->bytes_per_stride!=(int32_t)t->pixel.pitch||
        !v->mapping.shared||v->mapping.matches!=1||strcmp(v->mapping.permissions,"rw-s")||
        v->mapping.fd_index!=t->pixel.fd_index||v->mapping.offset!=t->pixel.offset||
        v->mapping.payload_bytes!=t->shape.storage_bytes?ENOTSUP:0;
}
static int save_image(const char *dir,unsigned slot,unsigned test,const unsigned char *address,uint32_t pitch)
{
    char path[1400];int n=snprintf(path,sizeof(path),"%s/fullsize-slot-%u-case-%u.rgba",dir,slot,test);
    if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return errno;
    int error=0;
    for(unsigned y=0;y<HEIGHT&&!error;y++){
        size_t left=WIDTH*4;const unsigned char *p=address+(size_t)y*pitch;
        while(left){ssize_t wrote=write(fd,p,left);if(wrote<0){error=errno;break;}if(!wrote){error=EIO;break;}p+=wrote;left-=(size_t)wrote;}
    }
    if(fsync(fd)&&!error)error=errno;
    if(close(fd)&&!error)error=errno;
    return error;
}
/* Only called with target released by reacquire or successful session retire,
 * and after the producer Finish. Never lock a compositor CURRENT buffer. */
static int readback(struct target *t,unsigned slot,unsigned test,const char *dir,struct counts *c,int *unsafe)
{
    struct nn_ahb_view v;int error=nn_ahb_lock(t->owner,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,&v);
    if(error){*unsafe=1;return error;}c->read_locks++;error=same_pixel(t,&v);
    const unsigned char *data=v.address;
    if(!error)for(unsigned y=0;y<HEIGHT&&!error;y++){
        for(unsigned x=0;x<WIDTH&&!error;x++){
            unsigned char expected[4];pattern(test,x,y,expected);
            for(unsigned k=0;k<4;k++){if(data[(size_t)y*t->pixel.pitch+x*4+k]!=expected[k]){error=EIO;break;}c->channels++;}
        }
        for(size_t j=WIDTH*4;j<t->pixel.pitch&&!error;j++){if(data[(size_t)y*t->pixel.pitch+j]!=0x5a)error=EIO;else c->guards++;}
    }
    if(!error)for(uint64_t j=t->shape.visible_span;j<t->shape.storage_bytes;j++){if(data[j]!=0x5a){error=EIO;break;}c->guards++;}
    if(!error)error=save_image(dir,slot,test,data,t->pixel.pitch);
    int e=nn_ahb_unlock(t->owner);if(e){*unsafe=1;if(!error)error=e;}else c->read_unlocks++;
    if(!error)t->pending=0;
    return error;
}
static int wait_current(struct owner *o,unsigned slot,uint64_t shown,
    const struct nn_nvn_window_probe_callbacks *cb,struct nn_window_session_stats *stats)
{
    struct timespec began,now,pause={0,5000000};if(clock_gettime(CLOCK_MONOTONIC,&began))return errno;
    for(;;){int e=nn_window_session_get_stats(o->session,stats);if(e)return e;
        if(stats->first_error)return stats->first_error;
        if(stats->shown>=shown&&stats->current_index==(int32_t)slot&&stats->state[slot]==NN_WINDOW_CURRENT)return 0;
        if(stats->closing||cb->closing(cb->user))return ECANCELED;
        if(clock_gettime(CLOCK_MONOTONIC,&now))return errno;
        if((int64_t)(now.tv_sec-began.tv_sec)*1000000000+now.tv_nsec-began.tv_nsec>=2000000000)return ETIMEDOUT;
        if(nanosleep(&pause,NULL)&&errno!=EINTR)return errno;
    }
}
static int target_setup(struct owner *o,unsigned i,struct counts *c,int *unsafe)
{
    struct target *t=o->targets+i;int e=nn_ahb_create_rgba(WIDTH,HEIGHT,&t->owner);if(e)return e;
    const AHardwareBuffer_Desc *d=nn_ahb_description(t->owner);
    if(!d||d->stride>UINT32_MAX/4)return ENOTSUP;
    e=nn_display_rgba8_shape(WIDTH,HEIGHT,d->stride*4,&t->shape);if(e)return e;
    struct nn_ahb_view v;e=nn_ahb_lock(t->owner,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN|AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,&v);if(e)return e;
    c->sentinel_locks++;
    /* Fixed sentinel initialization only. These bytes cannot satisfy any of
     * the four expected opaque output patterns without a GPU overwrite. */
    memset(v.address,0x5a,(size_t)t->shape.storage_bytes);
    e=nn_ahb_unlock(t->owner);if(e){*unsafe=1;return e;}c->sentinel_unlocks++;
    e=nn_ahb_get_pixel(t->owner,&t->pixel);if(e)return e;
    if(t->pixel.width!=WIDTH||t->pixel.height!=HEIGHT||t->pixel.pitch!=t->shape.pitch||t->pixel.payload_bytes!=t->shape.storage_bytes)return ENOTSUP;
    struct nn_external_image_desc desc={.fd=t->pixel.fd,.fd_bytes=t->pixel.bytes,.pixel_offset=t->pixel.offset,.payload_bytes=t->pixel.payload_bytes,
        .width=WIDTH,.height=HEIGHT,.pitch=t->pixel.pitch,.layers=1,.format=NN_EXTERNAL_RGBA8,.usage=NN_AHB_USAGE,
        .display_buffer=nn_ahb_buffer(t->owner),.owner_cookie=t->owner,.release_owner=release_owner};
    e=nn_external_image_import(o->device,&desc,&t->image);if(e)return e;
    e=nn_external_image_view(t->image,&t->external);if(e)return e;
    e=nn_external_image_pin(t->image);if(e)return e;t->pinned=1;
    t->source_bytes=(size_t)WIDTH*HEIGHT*4+2*GUARD;
    e=nn_bo_alloc(o->device,t->source_bytes,&t->source);if(e)return e;
    e=nn_bo_alloc(o->device,4096,&t->uniform);if(e)return e;
    t->source_shadow=malloc(nn_bo_size(t->source));if(!t->source_shadow)return ENOMEM;
    memset(nn_bo_map(t->source),0xa5,nn_bo_size(t->source));memset(nn_bo_map(t->uniform),0xa5,nn_bo_size(t->uniform));
    unsigned char *uniform=nn_bo_map(t->uniform);memset(uniform,0,64);
    const struct nn_tex_linear_copy_args args={.source_base=0,.destination_base=0,.row_bytes=WIDTH*4,.rows=HEIGHT,.source_pitch=WIDTH*4,.destination_pitch=t->pixel.pitch};
    memcpy(uniform,&args,sizeof(args));memcpy(t->uniform_shadow,uniform,4096);
    const struct nn_cs_image *image=&NVN_COMPILED_IMAGE;
    struct nn_cs_buffer ubos[16]={{0}},ssbos[16]={{0}};
    ubos[2]=(struct nn_cs_buffer){nn_bo_address(t->uniform),64};
    ssbos[0]=(struct nn_cs_buffer){nn_bo_address(t->source)+GUARD,WIDTH*HEIGHT*4};
    ssbos[1]=(struct nn_cs_buffer){nn_bo_address(t->external.bo)+t->pixel.offset,(uint32_t)t->shape.storage_bytes};
    if(!ssbos[0].address||!ssbos[1].address||ssbos[0].address>UINT64_MAX-ssbos[0].bytes||ssbos[1].address>UINT64_MAX-ssbos[1].bytes)return EOVERFLOW;
    if(ssbos[0].address<ssbos[1].address+ssbos[1].bytes&&
       ssbos[1].address<ssbos[0].address+ssbos[0].bytes)return EINVAL;
    const uint32_t groups[3]={(WIDTH+7)/8,(HEIGHT+7)/8,1};
    e=nn_cs_encode(o->device,o->code,image,ubos,ssbos,groups,&t->job,&t->job_bytes);if(e)return e;
    t->job_shadow=malloc(t->job_bytes);if(!t->job_shadow)return ENOMEM;
    memcpy(t->job_shadow,nn_bo_map(t->job),t->job_bytes);t->job_address=nn_bo_address(t->job);
    e=nn_bo_cache(t->uniform,0,nn_bo_size(t->uniform),0);if(e)return e;
    return nn_bo_cache(t->job,0,nn_bo_size(t->job),0);
}
int nn_fullsize_display_probe(const char *directory,char *json,size_t capacity,int slot_count,
    const void *token,const struct nn_nvn_window_probe_callbacks *cb)
{
    if(!json||capacity<8192)return EINVAL;
    const struct nn_cs_image *image=&NVN_COMPILED_IMAGE;
    const char *stage="arguments",*submission="not_attempted";int error=0,cleanup=0,pending=0,unsafe=0,retired=0,session_destroyed=0;
    unsigned slots=(slot_count==2||slot_count==3)?(unsigned)slot_count:0,completed=0,submits=0,presented=0,callbacks=0;
    struct owner *o=NULL;struct counts c={0};struct nn_window_session_stats stats={0};
#define TRY(x) do {int e_=(x);if(e_){error=e_;goto done;}}while(0)
#define REQUIRE(x) do {if(!(x)){error=ENOTSUP;goto done;}}while(0)
    REQUIRE(directory&&slots&&token&&cb&&cb->closing&&cb->presented);
    REQUIRE(sysconf(_SC_PAGESIZE)==4096);
    REQUIRE(image->chip==0x06050002&&image->local[0]==8&&image->local[1]==8&&image->local[2]==1&&image->ssbo_slots==2);
    o=calloc(1,sizeof(*o));if(!o){error=ENOMEM;goto done;}
    stage="device";TRY(nn_device_open(&o->device));REQUIRE(nn_device_chip(o->device)==image->chip);TRY(nn_queue_create(o->device,&o->queue));
    stage="code";TRY(nn_bo_alloc(o->device,image->code_bytes,&o->code));
    o->code_shadow=malloc(nn_bo_size(o->code));if(!o->code_shadow){error=ENOMEM;goto done;}
    memset(nn_bo_map(o->code),0xa5,nn_bo_size(o->code));memcpy(nn_bo_map(o->code),image->code,image->code_bytes);
    memcpy(o->code_shadow,nn_bo_map(o->code),nn_bo_size(o->code));TRY(nn_bo_cache(o->code,0,nn_bo_size(o->code),0));
    stage="targets";for(unsigned i=0;i<slots;i++)TRY(target_setup(o,i,&c,&unsafe));
    struct nn_window_slot window_slots[3]={{0}};
    for(unsigned i=0;i<slots;i++){
        struct target *t=o->targets+i;struct nn_external_image_view *v=&t->external;
        window_slots[i]=(struct nn_window_slot){.buffer=v->display_buffer,.owner=t->image,.pixel_offset=v->pixel_offset,.payload_bytes=v->payload_bytes,
            .backing_inode=v->backing_inode,.backing_dev_major=v->backing_dev_major,.backing_dev_minor=v->backing_dev_minor,
            .width=WIDTH,.height=HEIGHT,.pitch=t->pixel.pitch,.format=NN_EXTERNAL_RGBA8,.usage=NN_AHB_USAGE};
    }
    stage="session";TRY(nn_window_session_create(token,window_slots,slots,&o->session));
    while(completed<slots*4){
        if(cb->closing(cb->user)){error=ECANCELED;goto done;}struct nn_window_ticket ticket;
        stage="acquire";TRY(nn_window_session_acquire(o->session,1500,&ticket));REQUIRE(ticket.index<slots);
        unsigned i=ticket.index;struct target *t=o->targets+i;
        if(t->pending){stage="released-readback";TRY(readback(t,i,t->pending_case,directory,&c,&unsafe));}
        unsigned test=t->cases;REQUIRE(test<4);stage="source-update";
        unsigned char *source=nn_bo_map(t->source);
        for(unsigned y=0;y<HEIGHT;y++)for(unsigned x=0;x<WIDTH;x++)pattern(test,x,y,source+GUARD+((size_t)y*WIDTH+x)*4);
        memcpy(t->source_shadow,source,nn_bo_size(t->source));TRY(nn_bo_cache(t->source,0,nn_bo_size(t->source),0));
        TRY(nn_window_session_check_acquired(o->session,&ticket));
        REQUIRE(nn_bo_address(t->job)==t->job_address);uint32_t timestamp;
        nn_bo *resources[]={o->code,t->source,t->uniform,t->external.bo,t->job};
        stage="submit";submission="unknown";pending=1;TRY(nn_queue_submit(o->queue,t->job,0,t->job_bytes,resources,5,&timestamp));submits++;submission="accepted";
        stage="finish";TRY(nn_queue_finish(o->queue,5000));pending=0;
        TRY(nn_bo_cache(t->source,0,nn_bo_size(t->source),1));TRY(equal(source,t->source_shadow,nn_bo_size(t->source),&c.readonly));
        TRY(nn_bo_cache(t->uniform,0,nn_bo_size(t->uniform),1));TRY(equal(nn_bo_map(t->uniform),t->uniform_shadow,4096,&c.readonly));
        TRY(nn_bo_cache(o->code,0,nn_bo_size(o->code),1));TRY(equal(nn_bo_map(o->code),o->code_shadow,nn_bo_size(o->code),&c.readonly));
        TRY(nn_bo_cache(t->job,0,t->job_bytes,1));TRY(equal(nn_bo_map(t->job),t->job_shadow,t->job_bytes,&c.readonly));
        stage="present";TRY(nn_window_session_enqueue_ready(o->session,&ticket));presented++;TRY(wait_current(o,i,presented,cb,&stats));
        t->pending=1;t->pending_case=test;t->cases++;completed++;
        TRY(cb->presented(cb->user,i,test));callbacks++;
    }
    stage="retire";TRY(nn_window_session_get_stats(o->session,&stats));TRY(nn_window_session_retire(o->session,1500));retired=1;
    TRY(nn_window_session_destroy(o->session));o->session=NULL;session_destroyed=1;
    for(unsigned i=0;i<slots;i++)if(o->targets[i].pending){stage="retired-readback";TRY(readback(o->targets+i,i,o->targets[i].pending_case,directory,&c,&unsafe));}
    REQUIRE(completed==slots*4&&submits==slots*4&&presented==slots*4&&callbacks==slots*4&&c.channels==(uint64_t)slots*4*WIDTH*HEIGHT*4);
    stage="complete";
done:
    if(o){
        if(pending||unsafe)cleanup=EBUSY;
        if(!cleanup&&o->session){
            int e=nn_queue_finish(o->queue,5000);if(e)cleanup=e;
            if(!cleanup){e=nn_window_session_retire(o->session,1500);if(e)cleanup=e;else retired=1;}
            if(!cleanup){e=nn_window_session_destroy(o->session);if(e)cleanup=e;else{o->session=NULL;session_destroyed=1;}}
        }
        if(!cleanup){
            for(unsigned i=0;i<slots&&!cleanup;i++){
                struct target *t=o->targets+i;nn_bo **bos[]={&t->job,&t->uniform,&t->source};
                for(unsigned b=0;b<3&&!cleanup;b++)if(*bos[b]){int e=nn_bo_destroy(*bos[b]);if(e)cleanup=e;else *bos[b]=NULL;}
                if(!cleanup&&t->image){int e=0;if(t->pinned){e=nn_external_image_unpin(t->image);if(!e)t->pinned=0;}if(!e)e=nn_external_image_destroy(t->image);if(e)cleanup=e;else{t->image=NULL;t->owner=NULL;c.owner_releases++;}}
                else if(!cleanup&&t->owner){int e=nn_ahb_destroy(t->owner);if(e)cleanup=e;else{t->owner=NULL;c.owner_releases++;}}
            }
            if(!cleanup&&o->code){int e=nn_bo_destroy(o->code);if(e)cleanup=e;else o->code=NULL;}
            if(!cleanup&&o->queue){int e=nn_queue_destroy(o->queue);if(e)cleanup=e;else o->queue=NULL;}
            if(!cleanup&&o->device){int e=nn_device_close(o->device);if(e)cleanup=e;else o->device=NULL;}
        }
    }
    int result=error?error:cleanup;
    size_t used=0;int n=snprintf(json,capacity,
        "{\"schema\":1,\"operation\":\"kgsl_ahb_linear_copy\",\"width\":%u,\"height\":%u,\"slots\":%u,\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"submission\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,"
        "\"resources_retained\":%s,\"game_modules_executed\":false,\"cpu_image_copy\":false,\"nvn_draw_calls\":0,\"raw_draw_calls\":0,\"completed_cases\":%u,\"gpu_submissions\":%u,\"window_presentations\":%u,\"presented_callbacks\":%u,\"same_ib_replay\":%s,"
        "\"channel_checks\":%"PRIu64",\"guard_byte_checks\":%"PRIu64",\"readonly_byte_checks\":%"PRIu64",\"readback_locks\":%u,\"readback_unlocks\":%u,\"target_sentinel_locks\":%u,\"target_sentinel_unlocks\":%u,\"target_cpu_pattern_writes\":0,\"owner_release_calls\":%u,\"session_retired\":%s,\"session_destroyed\":%s,"
        "\"sc_before_retire\":{\"generation\":%"PRIu64",\"shown\":%"PRIu64",\"releases\":%"PRIu64",\"current_index\":%d,\"first_error\":%d},\"targets\":[",
        WIDTH,HEIGHT,slots,(unsigned)getuid(),image->chip,stage,submission,error,cleanup,cleanup?"true":"false",completed,submits,presented,callbacks,completed==slots*4&&slots?"true":"false",
        c.channels,c.guards,c.readonly,c.read_locks,c.read_unlocks,c.sentinel_locks,c.sentinel_unlocks,c.owner_releases,retired?"true":"false",session_destroyed?"true":"false",stats.generation,stats.shown,stats.releases,stats.current_index,stats.first_error);
    if(n<0||(size_t)n>=capacity){if(!result)result=ENOSPC;}else used=(size_t)n;
    if(o&&used)for(unsigned i=0;i<slots;i++){
        struct target *t=o->targets+i;struct nn_external_image_view *v=&t->external;
        n=snprintf(json+used,capacity-used,"%s{\"slot\":%u,\"fd_index\":%d,\"fd_at_import\":%d,\"fd_bytes\":%"PRIu64",\"pixel_offset\":%"PRIu64",\"payload_bytes\":%"PRIu64",\"pitch\":%u,\"inode\":%"PRIu64",\"device_major\":%u,\"device_minor\":%u}",i?",":"",i,t->pixel.fd_index,t->pixel.fd,t->pixel.bytes,t->pixel.offset,t->shape.storage_bytes,t->pixel.pitch,v->backing_inode,v->backing_dev_major,v->backing_dev_minor);
        if(n<0||(size_t)n>=capacity-used){used=0;if(!result)result=ENOSPC;break;}used+=(size_t)n;
    }
    if(used){n=snprintf(json+used,capacity-used,"],\"passed\":%s}",!result?"true":"false");if(n<0||(size_t)n>=capacity-used){if(!result)result=ENOSPC;}}
    if(o&&!cleanup){for(unsigned i=0;i<slots;i++){free(o->targets[i].source_shadow);free(o->targets[i].job_shadow);}free(o->code_shadow);free(o);}
    return result;
#undef TRY
#undef REQUIRE
}
