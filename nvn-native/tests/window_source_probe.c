/* SPDX-License-Identifier: MIT
 * Original test patterns generated directly in canonical source GOB storage.
 * No CPU input-image conversion, CPU AHB write, Draw or real game execution. */
#define _POSIX_C_SOURCE 200809L
#include "window_source_probe.h"
#include "window_interval_config.h"
#include "nvn_native/nvn.h"
#include "nvn_native/window_probe_access.h"
#include "nvn_native/texture_bridge.h"
#include "nvn_native/texture_source.h"
#include "nvn_native/display_shape.h"
#include "nvn_native/window_session.h"
#include "../android/ahb.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include NVN_WINDOW_SOURCE_BRIDGE_HEADER

enum { W=2880,H=1620,GUARD=4096 };
struct target {
    NVNmemoryPool pool;NVNbuffer buffer;NVNtexture texture;
    unsigned pool_ready,buffer_ready,texture_ready,cases,pending,pending_case;
    unsigned char *memory,*shadow,*padding;size_t size,extent;
    NVNbufferAddress address;ptrdiff_t offset;struct nn_nvn_window_source_identity identity;
    struct nn_external_image_view external;struct nn_ahb_pixel pixel;
    struct nn_display_rgba8_shape shape;
    struct nn_tex_source_layout source_layout;
};
struct owner {NVNdevice device;NVNqueue queue;NVNwindow window;NVNdeviceBuilder db;NVNqueueBuilder qb;NVNwindowBuilder wb;struct target targets[3];unsigned device_ready,queue_ready,window_ready;};
struct counts {uint64_t channels,guards,readonly,source_updates;unsigned baseline_locks,read_locks,unlocks,negatives;};
static void pattern(unsigned test,unsigned x,unsigned y,unsigned char p[4])
{
    p[0]=p[1]=p[2]=0;p[3]=255;
    if(test==0)p[0]=255;else if(test==1&&x>=W/2)p[1]=255;
    else if(test==3){unsigned g=x*127/(W-1),b=y*127/(H-1);p[0]=(unsigned char)(255-g-b);p[1]=(unsigned char)g;p[2]=(unsigned char)b;}
}
/* Independent sector-coordinate arithmetic for generating ORIGINAL data;
 * no read of a linear source image and no shared production offset function. */
static uint64_t original_gob_byte(const struct nn_tex_source_layout *l,unsigned byte_x,unsigned y)
{
    uint64_t bh=UINT64_C(1)<<l->level[0].block_height_log2;
    uint64_t gob_x=byte_x>>6,gob_y=y>>3,sector_x=(byte_x&63)>>4,sector_y=y&7;
    uint64_t bank=(sector_x>>1)*256+(sector_y>>1)*64+(sector_x&1)*32+(sector_y&1)*16;
    return ((gob_y/bh)*(l->level[0].pitch/64)+gob_x)*bh*512+(gob_y%bh)*512+bank+(byte_x&15);
}
static int equal(const unsigned char *a,const unsigned char *b,size_t n,uint64_t *checks)
{for(size_t i=0;i<n;i++){if(a[i]!=b[i])return EIO;(*checks)++;}return 0;}
static int save(const char *dir,unsigned slot,unsigned test,const unsigned char *p,unsigned pitch)
{
    char path[1500];int n=snprintf(path,sizeof(path),"%s/window-source-slot-%u-case-%u.rgba",dir,slot,test);if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return errno;int error=0;
    for(unsigned y=0;y<H&&!error;y++){const unsigned char *row=p+(size_t)y*pitch;size_t left=W*4;
        while(left){ssize_t z=write(fd,row,left);if(z<0){error=errno;break;}if(!z){error=EIO;break;}left-=(size_t)z;row+=z;}}
    if(fsync(fd)&&!error)error=errno;if(close(fd)&&!error)error=errno;return error;
}
static int readback(struct owner *o,unsigned slot,unsigned test,int baseline,const char *dir,struct counts *c,int *unsafe)
{
    struct target *t=o->targets+slot;struct nn_ahb_view view;int error=nn_nvn_window_target_read_lock(&o->window,slot,&view);if(error){*unsafe=1;return error;}
    if(baseline)c->baseline_locks++;else c->read_locks++;
    if(!view.address||view.bytes_per_pixel!=4||view.bytes_per_stride!=(int)t->pixel.pitch||!view.mapping.shared||view.mapping.matches!=1||strcmp(view.mapping.permissions,"rw-s")||view.mapping.fd_index!=t->pixel.fd_index||view.mapping.offset!=t->pixel.offset||view.mapping.payload_bytes!=t->shape.storage_bytes)error=ENOTSUP;
    const unsigned char *p=view.address;
    if(!error)for(unsigned y=0;y<H&&!error;y++){
        if(!baseline)for(unsigned x=0;x<W&&!error;x++){unsigned char expected[4];pattern(test,x,y,expected);for(unsigned k=0;k<4;k++){if(p[(size_t)y*t->pixel.pitch+x*4+k]!=expected[k]){error=EIO;break;}c->channels++;}}
        for(size_t j=W*4;j<t->pixel.pitch&&!error;j++){size_t at=(size_t)y*t->pixel.pitch+j;if(baseline)t->padding[at]=p[at];else if(p[at]!=t->padding[at])error=EIO;else c->guards++;}
    }
    if(!error)for(uint64_t at=t->shape.visible_span;at<t->shape.storage_bytes;at++){if(baseline)t->padding[at]=p[at];else if(p[at]!=t->padding[at]){error=EIO;break;}else c->guards++;}
    if(!error&&!baseline)error=save(dir,slot,test,p,t->pixel.pitch);
    int e=nn_nvn_window_target_read_unlock(&o->window,slot);if(e){*unsafe=1;if(!error)error=e;}else c->unlocks++;
    if(!error&&!baseline)t->pending=0;return error;
}
static int wait_shown(struct owner *o,unsigned index,uint64_t shown,const struct nn_nvn_window_probe_callbacks *cb,struct nn_window_session_stats *stats)
{
    struct timespec start,now,pause={0,5000000};if(clock_gettime(CLOCK_MONOTONIC,&start))return errno;
    for(;;){int e=nn_nvn_window_get_stats(&o->window,stats);if(e)return e;if(stats->first_error)return stats->first_error;
        if(stats->shown>=shown&&stats->current_index==(int)index&&stats->state[index]==NN_WINDOW_CURRENT)return 0;
        if(stats->closing||cb->closing(cb->user))return ECANCELED;
        if(clock_gettime(CLOCK_MONOTONIC,&now))return errno;
        if((int64_t)(now.tv_sec-start.tv_sec)*1000000000+now.tv_nsec-start.tv_nsec>=3000000000)return ETIMEDOUT;
        if(nanosleep(&pause,NULL)&&errno!=EINTR)return errno;}
}
int nn_window_source_probe(const char *directory,char *json,size_t capacity,int slot_count,const void *token,const struct nn_nvn_window_probe_callbacks *cb)
{
    if(!json||capacity<8192)return EINVAL;
    const unsigned slots=slot_count==2||slot_count==3?(unsigned)slot_count:0;
    struct owner *o=NULL;struct counts counts={0};struct nn_window_session_stats stats={0};
    struct nn_window_interval_config interval={0,1};unsigned interval_getters=0,interval_mutations=0,interval_observations=0;
    unsigned completed=0,presented=0,callbacks=0,acquires=0,finalized=0,retired=0,released_pools=0;int error=0,cleanup=0,unsafe=0;
    const char *stage="arguments";
#define ENTRY(name,type) type name=(type)nvnBootstrapLoader(#name);if(!name)error=ENOSYS;
#include "../src/nvn_entries.inc"
#undef ENTRY
#define TRY(x) do{int e_=(x);if(e_){error=e_;goto done;}}while(0)
#define CALL(x) do{x;int e_=nn_nvn_error();if(e_){error=e_;goto done;}}while(0)
#define BOOL(x) do{int z_=(x),e_=nn_nvn_error();if(!z_||e_){error=e_?e_:EIO;goto done;}}while(0)
#define REQUIRE(x) do{if(!(x)){error=ENOTSUP;goto done;}}while(0)
    if(error)goto done;nn_nvn_clear_error();
    REQUIRE(directory&&slots&&token&&cb&&cb->presented&&cb->closing&&sysconf(_SC_PAGESIZE)==4096);
    stage="interval-config";TRY(nn_window_interval_read_config(directory,&interval));
    o=calloc(1,sizeof(*o));if(!o){error=ENOMEM;goto done;}
    stage="device";CALL(nvnDeviceBuilderSetDefaults(&o->db));BOOL(nvnDeviceInitialize(&o->device,&o->db));o->device_ready=1;
    TRY(nn_nvn_register_texture_bridge(&o->device,&NN_TEXTURE_BRIDGE_BUNDLE));
    CALL(nvnQueueBuilderSetDefaults(&o->qb));CALL(nvnQueueBuilderSetDevice(&o->qb,&o->device));BOOL(nvnQueueInitialize(&o->queue,&o->qb));o->queue_ready=1;
    stage="canonical-source";
    for(unsigned i=0;i<slots;i++){
        struct target *t=o->targets+i;NVNtextureBuilder tb;NVNmemoryPoolBuilder pb;NVNbufferBuilder bb;
        CALL(nvnTextureBuilderSetDefaults(&tb));CALL(nvnTextureBuilderSetDevice(&tb,&o->device));CALL(nvnTextureBuilderSetFlags(&tb,9));CALL(nvnTextureBuilderSetTarget(&tb,NVN_TEXTURE_TARGET_2D));CALL(nvnTextureBuilderSetSize2D(&tb,W,H));CALL(nvnTextureBuilderSetLevels(&tb,1));CALL(nvnTextureBuilderSetSamples(&tb,0));CALL(nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8));
        t->extent=(size_t)nvnTextureBuilderGetStorageSize(&tb);CALL((void)0);REQUIRE(t->extent==19169280&&nvnTextureBuilderGetStorageAlignment(&tb)==512);CALL((void)0);
        t->size=(t->extent+2*GUARD+4095)&~(size_t)4095;
        int e=posix_memalign((void **)&t->memory,4096,t->size);if(e){error=e;goto done;}memset(t->memory,0xa5,t->size);t->shadow=malloc(t->size);if(!t->shadow){error=ENOMEM;goto done;}
        CALL(nvnMemoryPoolBuilderSetDefaults(&pb));CALL(nvnMemoryPoolBuilderSetDevice(&pb,&o->device));CALL(nvnMemoryPoolBuilderSetFlags(&pb,0xa4));CALL(nvnMemoryPoolBuilderSetStorage(&pb,t->memory,t->size));BOOL(nvnMemoryPoolInitialize(&t->pool,&pb));t->pool_ready=1;
        CALL(nvnBufferBuilderSetDefaults(&bb));CALL(nvnBufferBuilderSetDevice(&bb,&o->device));CALL(nvnBufferBuilderSetStorage(&bb,&t->pool,GUARD,t->extent));BOOL(nvnBufferInitialize(&t->buffer,&bb));t->buffer_ready=1;
        REQUIRE(nvnBufferMap(&t->buffer)==t->memory+GUARD);CALL((void)0);t->address=nvnBufferGetAddress(&t->buffer);CALL((void)0);REQUIRE(t->address);
        CALL(nvnTextureBuilderSetStorage(&tb,&t->pool,GUARD));BOOL(nvnTextureInitialize(&t->texture,&tb));t->texture_ready=1;
        t->offset=GUARD;

    }
    NVNtexture *textures[3]={&o->targets[0].texture,&o->targets[1].texture,&o->targets[2].texture};
    CALL(nvnWindowBuilderSetDefaults(&o->wb));CALL(nvnWindowBuilderSetDevice(&o->wb,&o->device));CALL(nvnWindowBuilderSetNativeWindow(&o->wb,(NVNnativeWindow)token));CALL(nvnWindowBuilderSetTextures(&o->wb,(int)slots,textures));
    if(interval.enabled){
        REQUIRE(nvnWindowBuilderGetPresentInterval(&o->wb)==1);CALL((void)0);interval_getters++;
        CALL(nvnWindowBuilderSetPresentInterval(&o->wb,interval.interval));
        REQUIRE(nvnWindowBuilderGetPresentInterval(&o->wb)==interval.interval);CALL((void)0);interval_getters++;
    }
    BOOL(nvnWindowInitialize(&o->window,&o->wb));o->window_ready=1;
    if(interval.enabled){REQUIRE(nvnWindowGetPresentInterval(&o->window)==interval.interval);CALL((void)0);interval_getters++;}
    for(unsigned i=0;i<slots;i++){struct target *t=o->targets+i;TRY(nn_nvn_window_target_info(&o->window,i,&t->external,&t->pixel));TRY(nn_display_rgba8_shape(W,H,t->pixel.pitch,&t->shape));REQUIRE(t->shape.storage_bytes==t->pixel.payload_bytes&&t->external.bo&&t->external.backing_inode);TRY(nn_nvn_window_source_layout(&o->window,i,&t->source_layout));REQUIRE(t->source_layout.payload_bytes==t->extent);TRY(nn_nvn_window_source_identity(&o->window,i,&t->identity));REQUIRE(t->identity.pool==&t->pool&&t->identity.offset==GUARD&&t->identity.address==t->address&&t->identity.bytes==t->extent&&t->identity.entry_address&&t->identity.entry_bytes);t->padding=malloc((size_t)t->shape.storage_bytes);if(!t->padding){error=ENOMEM;goto done;}}
    while(completed<slots*4){
        if(cb->closing(cb->user)){error=ECANCELED;goto done;}int index=-1;stage="acquire";NVNqueueAcquireTextureResult result=nvnQueueAcquireTexture(&o->queue,&o->window,&index);CALL((void)0);REQUIRE(result==NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS&&index>=0&&(unsigned)index<slots);acquires++;
        struct target *t=o->targets+index;unsigned test=t->cases;REQUIRE(test<4);
        if(t->pending){stage="released-readback";TRY(readback(o,(unsigned)index,t->pending_case,0,directory,&counts,&unsafe));}
        if(!test){stage="padding-baseline";TRY(readback(o,(unsigned)index,0,1,directory,&counts,&unsafe));}
        stage="source-update";unsigned char *source=nvnBufferMap(&t->buffer);CALL((void)0);REQUIRE(source==t->memory+GUARD&&nvnBufferGetAddress(&t->buffer)==t->address);CALL((void)0);
        memset(source,0xa5,t->extent);
        for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++){unsigned char p[4];pattern(test,x,y,p);for(unsigned k=0;k<4;k++){uint64_t at=original_gob_byte(&t->source_layout,x*4+k,y);REQUIRE(at<t->extent);source[at]=p[k];counts.source_updates++;}}
        memcpy(t->shadow,t->memory,t->size);CALL(nvnBufferFlushMappedRange(&t->buffer,0,t->extent));
        struct nn_nvn_window_source_identity current;TRY(nn_nvn_window_source_identity(&o->window,(unsigned)index,&current));REQUIRE(current.pool==t->identity.pool&&current.address==t->identity.address&&current.offset==t->identity.offset&&current.bytes==t->identity.bytes&&current.entry_address==t->identity.entry_address&&current.entry_bytes==t->identity.entry_bytes);
        if(interval.enabled){CALL(nvnWindowSetPresentInterval(&o->window,interval.interval));REQUIRE(nvnWindowGetPresentInterval(&o->window)==interval.interval);CALL((void)0);interval_getters++;}
        stage="present";CALL(nvnQueuePresentTexture(&o->queue,&o->window,index));presented++;
        if(interval.enabled){
            /* Enqueue has copied the argument. This mutation is not a forced
             * worker interleaving: deterministic snapshot races are HOST tests. */
            int changed=(interval.interval+1)%6;CALL(nvnWindowSetPresentInterval(&o->window,changed));
            REQUIRE(nvnWindowGetPresentInterval(&o->window)==changed);CALL((void)0);interval_getters++;interval_mutations++;
        }
        TRY(wait_shown(o,(unsigned)index,presented,cb,&stats));
        if(interval.enabled){int effective=!interval.interval&&slots==2?1:interval.interval;
            REQUIRE(stats.last_shown_interval==effective);
            for(unsigned k=0;k<6;k++)REQUIRE(stats.shown_intervals[k]==(k==(unsigned)effective?presented:0));
            interval_observations++;
        }
        CALL(nvnMemoryPoolInvalidateMappedRange(&t->pool,0,t->size));TRY(equal(t->memory,t->shadow,t->size,&counts.readonly));
        /* Native probe access must reject a CURRENT slot before even trying
         * an Android read lock. This is not a GPU completion substitution. */
        if(!counts.negatives){stage="current-read-rejection";struct nn_ahb_view forbidden;REQUIRE(nn_nvn_window_target_read_lock(&o->window,(unsigned)index,&forbidden)==EBUSY);counts.negatives++;}
        t->pending=1;t->pending_case=test;t->cases++;completed++;
        TRY(cb->presented(cb->user,(unsigned)index,test));callbacks++;
    }
    stage="retire";TRY(nn_nvn_window_get_stats(&o->window,&stats));TRY(nn_nvn_window_retire_for_readback(&o->window));retired=1;
    for(unsigned i=0;i<slots;i++)if(o->targets[i].pending)TRY(readback(o,i,o->targets[i].pending_case,0,directory,&counts,&unsafe));
    REQUIRE(completed==slots*4&&presented==slots*4&&callbacks==slots*4&&counts.channels==(uint64_t)slots*4*W*H*4);stage="complete";
done:
    if(o){
        if(unsafe)cleanup=EBUSY;
        if(!cleanup&&o->window_ready){nvnWindowFinalize(&o->window);int e=nn_nvn_error();if(e)cleanup=e;else{o->window_ready=0;finalized=1;}}
        if(!cleanup)for(unsigned i=0;i<slots&&!cleanup;i++){struct target *t=o->targets+i;
            if(t->texture_ready){nvnTextureFinalize(&t->texture);cleanup=nn_nvn_error();if(!cleanup)t->texture_ready=0;}
            if(!cleanup&&t->buffer_ready){nvnBufferFinalize(&t->buffer);cleanup=nn_nvn_error();if(!cleanup)t->buffer_ready=0;}
            if(!cleanup&&t->pool_ready){nvnMemoryPoolFinalize(&t->pool);cleanup=nn_nvn_error();if(!cleanup){t->pool_ready=0;released_pools++;}}
        }
        if(!cleanup&&o->queue_ready){nvnQueueFinalize(&o->queue);cleanup=nn_nvn_error();if(!cleanup)o->queue_ready=0;}
        if(!cleanup&&o->device_ready){nvnDeviceFinalize(&o->device);cleanup=nn_nvn_error();if(!cleanup)o->device_ready=0;}
    }
    int result=error?error:cleanup;size_t used=0;int n=snprintf(json,capacity,
        "{\"schema\":1,\"operation\":\"nvn_window_source\",\"width\":%u,\"height\":%u,\"slots\":%u,\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,"
        "\"game_modules_executed\":false,\"cpu_image_copy\":false,\"target_cpu_writes\":0,\"nvn_draw_calls\":0,\"raw_draw_calls\":0,\"completed_cases\":%u,\"window_presentations\":%u,\"presented_callbacks\":%u,\"queue_acquires\":%u,\"window_finalized\":%s,\"session_retired\":%s,\"canonical_pools_released\":%u,"
        "\"channel_checks\":%"PRIu64",\"guard_byte_checks\":%"PRIu64",\"readonly_byte_checks\":%"PRIu64",\"source_pattern_byte_writes\":%"PRIu64",\"padding_baseline_locks\":%u,\"readback_locks\":%u,\"readback_unlocks\":%u,\"current_lock_rejections\":%u,\"canonical_address_unchanged\":%s,\"same_ib_replay\":%s,"
        "\"present_interval_configured\":%s,\"present_interval\":%d,\"interval_getter_checks\":%u,\"post_enqueue_interval_mutations\":%u,\"platform_snapshot_observations\":%u,\"cadence_measured\":false,"
        "\"interval_platform\":{\"last\":%d,\"shown\":[%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64"]},"
        "\"sc_before_retire\":{\"generation\":%"PRIu64",\"shown\":%"PRIu64",\"releases\":%"PRIu64",\"current_index\":%d,\"first_error\":%d},\"targets\":[",
        W,H,slots,(unsigned)getuid(),NN_TEXTURE_BRIDGE_BUNDLE.chip,stage,error,cleanup,cleanup?"true":"false",completed,presented,callbacks,acquires,finalized?"true":"false",retired?"true":"false",released_pools,counts.channels,counts.guards,counts.readonly,counts.source_updates,counts.baseline_locks,counts.read_locks,counts.unlocks,counts.negatives,!result?"true":"false",!result?"true":"false",interval.enabled?"true":"false",interval.interval,interval_getters,interval_mutations,interval_observations,stats.last_shown_interval,stats.shown_intervals[0],stats.shown_intervals[1],stats.shown_intervals[2],stats.shown_intervals[3],stats.shown_intervals[4],stats.shown_intervals[5],stats.generation,stats.shown,stats.releases,stats.current_index,stats.first_error);
    if(n<0||(size_t)n>=capacity){if(!result)result=ENOSPC;}else used=(size_t)n;
    if(o&&used)for(unsigned i=0;i<slots;i++){struct target *t=o->targets+i;n=snprintf(json+used,capacity-used,"%s{\"slot\":%u,\"fd_index\":%d,\"fd_at_import\":%d,\"fd_bytes\":%"PRIu64",\"pixel_offset\":%"PRIu64",\"payload_bytes\":%"PRIu64",\"pitch\":%u,\"inode\":%"PRIu64",\"device_major\":%u,\"device_minor\":%u,\"source_address\":%"PRIu64",\"source_offset\":%td,\"source_extent\":%zu}",i?",":"",i,t->pixel.fd_index,t->pixel.fd,t->pixel.bytes,t->pixel.offset,t->pixel.payload_bytes,t->pixel.pitch,t->external.backing_inode,t->external.backing_dev_major,t->external.backing_dev_minor,t->address,t->offset,t->extent);if(n<0||(size_t)n>=capacity-used){used=0;if(!result)result=ENOSPC;break;}used+=(size_t)n;}
    if(used){n=snprintf(json+used,capacity-used,"],\"passed\":%s}",!result?"true":"false");if(n<0||(size_t)n>=capacity-used){if(!result)result=ENOSPC;}}
    if(o&&!cleanup){for(unsigned i=0;i<slots;i++){free(o->targets[i].memory);free(o->targets[i].shadow);free(o->targets[i].padding);}free(o);}return result;
}
