/* SPDX-License-Identifier: MIT */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "nvn_native/copy_job.h"
#include "nvn_copy_probe.h"
#include "nvn_native/nvn.h"
#include "ahb.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include NVN_GRAPHICS_API_HEADER
/* Qualification-only observation. No arguments, results, stream or dispatch
 * order are changed by either wrapper. No driver/core structure is inspected. */
enum { SHADER, VERTEX, MATRIX, COMMAND, POOLS };
enum { VS,FS,BG_VB,MAIN_VB,BG_UB,MAIN_UB,VIEWS };
struct probe_state {
    NVNdevice device;NVNqueue queue;NVNprogram program;NVNcommandBuffer command;
    NVNmemoryPool pools[POOLS],source_pool;NVNtexture texture;NVNbuffer buffers[VIEWS];
    NVNdeviceBuilder db;NVNqueueBuilder qb;NVNmemoryPoolBuilder pb;NVNbufferBuilder bb;NVNtextureBuilder tb;
    unsigned char *memory[POOLS],*shadow[POOLS],*source_pages;
    size_t sizes[POOLS],offsets[VIEWS],lengths[VIEWS];NVNbufferAddress addresses[VIEWS];
    _Alignas(16) unsigned char control[512];NVNcommandHandle handle;
    unsigned device_ready,queue_ready,program_ready,command_ready,pool_ready[POOLS],source_ready,texture_ready,buffer_ready[VIEWS];
    nn_device *native_device;nn_queue *native_queue;nn_bo *source_bo,*target_bo;nn_ahb *ahb;nn_copy_job *job;
    struct nn_ahb_pixel pixel;struct stat backing;
    uint64_t source_address;uint32_t source_timestamp;
    unsigned import_observations,nvn_attempts,nvn_observed,copy_attempts,copy_observed,wrapper_phase;
    unsigned owner_pins,owner_pin_calls,owner_unpin_calls,live_validations,alias_proofs;
    int observation_error,locked,source_private_verified;
};
static struct probe_state *active;
int __real_nn_bo_import(nn_device *,void *,size_t,nn_bo **);
int __wrap_nn_bo_import(nn_device *d,void *storage,size_t bytes,nn_bo **out)
{
    struct probe_state *s=active;int watch=s && storage==s->source_pages;
    int e=__real_nn_bo_import(d,storage,bytes,out);
    if(watch){
        ++s->import_observations;
        if(e || bytes!=24576 || !out || !*out || s->native_device || s->source_bo)s->observation_error=e?e:EINVAL;
        else {s->native_device=d;s->source_bo=*out;}
    }
    return e;
}
int __real_nn_queue_submit(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,uint32_t *);
int __wrap_nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *resources,size_t count,uint32_t *timestamp)
{
    struct probe_state *s=active;unsigned phase=s?s->wrapper_phase:0;
    if(s && phase==1){
        ++s->nvn_attempts;unsigned matches=0;
        for(size_t i=0;i<count;i++){
            uint64_t address=nn_bo_address(resources[i]);size_t size=nn_bo_size(resources[i]);
            if(s->source_address>=address && s->source_address-address<=size &&
                UINT64_C(16384)<=size-(s->source_address-address)){
                ++matches;if(resources[i]!=s->source_bo)s->observation_error=EINVAL;
            }
        }
        if(matches!=1 || !s->source_bo || (s->native_queue && s->native_queue!=q))s->observation_error=EINVAL;
        if(!s->native_queue)s->native_queue=q;
    } else if(s && phase==2){
        ++s->copy_attempts;
        if(q!=s->native_queue || offset || bytes!=98324 || count!=2 ||
            resources[0]!=s->source_bo || resources[1]!=s->target_bo)s->observation_error=EINVAL;
    } else if(s)s->observation_error=EINVAL;
    int e=__real_nn_queue_submit(q,ib,offset,bytes,resources,count,timestamp);
    if(s && !e){if(phase==1){++s->nvn_observed;s->source_timestamp=*timestamp;}
        if(phase==2)++s->copy_observed;}
    return e;
}
static int pin(void *p){struct probe_state *s=p;if(s!=active || s->owner_pins)return EBUSY;++s->owner_pins;++s->owner_pin_calls;return 0;}
static int unpin(void *p){struct probe_state *s=p;if(s!=active || s->owner_pins!=1)return EBUSY;--s->owner_pins;++s->owner_unpin_calls;return 0;}
static int live(void *p){
    struct probe_state *s=p;++s->live_validations;
    return s!=active || !s->device_ready || !s->queue_ready || !s->source_ready || !s->texture_ready ||
        !s->ahb || !s->target_bo || !s->source_bo || s->locked || s->observation_error ? EBUSY:0;
}
static int source_private(struct probe_state *s)
{
    uintptr_t address=(uintptr_t)s->source_pages;
#if defined(__aarch64__)
    address &= UINT64_C(0x00ffffffffffffff);
#endif
    if(!address || address>UINTPTR_MAX-24576)return EINVAL;
    FILE *maps=fopen("/proc/self/maps","r");if(!maps)return errno;
    char line[1024];unsigned matches=0;int qualified=0;
    while(fgets(line,sizeof(line),maps)){
        unsigned long long start,end,offset,inode;unsigned major_number,minor_number;char perms[5];
        if(sscanf(line,"%llx-%llx %4s %llx %x:%x %llu",&start,&end,perms,&offset,&major_number,&minor_number,&inode)!=7)continue;
        if(address>=start && (uint64_t)address+24576<=end){
            ++matches;qualified=!strcmp(perms,"rw-p") && !major_number && !minor_number && !inode;
        }
    }
    int e=ferror(maps)?EIO:0;if(fclose(maps) && !e)e=errno;
    if(!e && (matches!=1 || !qualified))e=ENOTSUP;
    if(!e)s->source_private_verified=1;
    return e;
}
static int prove(void *p)
{
    struct probe_state *s=p;++s->alias_proofs;
    int e=source_private(s);if(e)return e;
    struct stat st;if(fstat(s->pixel.fd,&st))return errno;
    /* Physical-disjointness subset: caller freshly allocated PRIVATE anonymous
     * source pages, imported by the observed real NVN import; AHB subsequently
     * allocated independently. Neither mapping is replaced/duplicated into the
     * other owner. Unknown/general imports cannot use this provenance proof.
     * GPUVA or different owner/BO pointers are never used as the proof. */
    if(s->import_observations!=1 || !s->source_bo || !s->native_device ||
        !st.st_ino || st.st_ino!=s->backing.st_ino || st.st_dev!=s->backing.st_dev ||
        st.st_size<=0 || (uint64_t)st.st_size!=s->pixel.bytes ||
        s->source_address!=nn_bo_address(s->source_bo)+4096 || nn_bo_size(s->source_bo)!=24576)return ENOTSUP;
    return 0;
}
static int view_ok(const struct nn_ahb_view *v,const struct nn_ahb_pixel *p)
{
    return !v->address || v->bytes_per_pixel!=4 || v->bytes_per_stride!=256 ||
        !v->mapping.found || !v->mapping.shared || v->mapping.matches!=1 ||
        strcmp(v->mapping.permissions,"rw-s") || v->mapping.payload_bytes!=16384 ||
        (p && (v->mapping.fd_index!=p->fd_index || v->mapping.offset!=p->offset)) ? ENOTSUP:0;
}
static unsigned rounded(float x){return (unsigned)(255*x+0.5f);}
int nn_nvn_copy_probe(const char *directory,char *json,size_t capacity)
{
    const struct nn_gfx_image *image=nn_test_graphics.image;
    struct probe_state *s=NULL;const char *stage="arguments";
    unsigned completed=0,channels=0,locks=0,unlocks=0,source_map_rejections=0,producer_finishes=0;
    unsigned identification_locks=0,identification_unlocks=0;int identifying=0;
    unsigned copy_finishes=0,copy_submissions=0,destroyed_jobs=0;
    size_t command_guards=0,command_readonly=0,readonly=0;
    unsigned bad_mode=~0u,bad_case=~0u,bad_x=~0u,bad_y=~0u,bad_channel=~0u,bad_actual=0,bad_expected=0;
    int error=0,cleanup=0,pending=0,unsafe=0;struct nn_ahb_view view={0};
    if(!json || capacity<8192)return -1;
#define ENTRY(name,type) type name=(type)nvnBootstrapLoader(#name);if(!name)error=ENOSYS;
#include "nvn_entries.inc"
#undef ENTRY
#define CHECK(x) do{int e_=(x);if(e_){error=e_;goto done;}}while(0)
#define REQUIRE(x) do{if(!(x)){error=ENOTSUP;goto done;}}while(0)
#define CALL(x) do{x;int e_=nn_nvn_error();if(e_){error=e_;goto done;}}while(0)
#define BOOL(x) do{int ok_=(x),e_=nn_nvn_error();if(!ok_||e_){error=e_?e_:EIO;goto done;}}while(0)
    if(error)goto done;
    if(active){error=EBUSY;goto done;}
    REQUIRE(directory && image && image->schema==3 && image->chip==0x06050002 && image->profile==NN_GFX_COLOR &&
        image->width==64 && image->height==64 && image->target_pitch==256 && image->target_bytes==16384 &&
        image->gmem_bytes==1179648 && sysconf(_SC_PAGESIZE)==4096);
    s=calloc(1,sizeof(*s));if(!s){error=ENOMEM;goto done;}active=s;s->pixel.fd=s->pixel.fd_index=-1;
    const unsigned view_pool[VIEWS]={SHADER,SHADER,VERTEX,VERTEX,MATRIX,MATRIX};
    s->offsets[VS]=4096;s->lengths[VS]=nn_test_graphics.vs.source_bytes;
    s->offsets[FS]=((s->offsets[VS]+s->lengths[VS]+4095)&~(size_t)4095)+4096;s->lengths[FS]=nn_test_graphics.fs.source_bytes;
    s->sizes[SHADER]=((s->offsets[FS]+s->lengths[FS]+4095)&~(size_t)4095)+4096;
    s->offsets[BG_VB]=256;s->offsets[MAIN_VB]=512;s->lengths[BG_VB]=s->lengths[MAIN_VB]=72;
    s->offsets[BG_UB]=64;s->offsets[MAIN_UB]=128;s->lengths[BG_UB]=s->lengths[MAIN_UB]=64;
    s->sizes[VERTEX]=s->sizes[MATRIX]=s->sizes[COMMAND]=4096;
    nn_nvn_clear_error();stage="device";
    CALL(nvnDeviceBuilderSetDefaults(&s->db));BOOL(nvnDeviceInitialize(&s->device,&s->db));s->device_ready=1;
    REQUIRE(nvnDeviceGetWindowOriginMode(&s->device)==NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT &&
        nvnDeviceGetDepthMode(&s->device)==NVN_DEPTH_MODE_NEAR_IS_MINUS_W);
    CHECK(nn_nvn_register_graphics(&s->device,&nn_test_graphics));
    CALL(nvnQueueBuilderSetDefaults(&s->qb));CALL(nvnQueueBuilderSetDevice(&s->qb,&s->device));
    BOOL(nvnQueueInitialize(&s->queue,&s->qb));s->queue_ready=1;
    stage="input-pools";
    for(unsigned i=0;i<POOLS;i++){
        CHECK(posix_memalign((void **)&s->memory[i],4096,s->sizes[i]));memset(s->memory[i],0xa5,s->sizes[i]);
        s->shadow[i]=malloc(s->sizes[i]);if(!s->shadow[i]){error=ENOMEM;goto done;}
        CALL(nvnMemoryPoolBuilderSetDefaults(&s->pb));CALL(nvnMemoryPoolBuilderSetDevice(&s->pb,&s->device));
        CALL(nvnMemoryPoolBuilderSetStorage(&s->pb,s->memory[i],s->sizes[i]));
        CALL(nvnMemoryPoolBuilderSetFlags(&s->pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED|
            (i==SHADER?NVN_MEMORY_POOL_FLAGS_SHADER_CODE:0)));
        BOOL(nvnMemoryPoolInitialize(&s->pools[i],&s->pb));s->pool_ready[i]=1;
    }
    for(unsigned i=0;i<VIEWS;i++){
        CALL(nvnBufferBuilderSetDefaults(&s->bb));CALL(nvnBufferBuilderSetDevice(&s->bb,&s->device));
        CALL(nvnBufferBuilderSetStorage(&s->bb,&s->pools[view_pool[i]],s->offsets[i],s->lengths[i]));
        BOOL(nvnBufferInitialize(&s->buffers[i],&s->bb));s->buffer_ready[i]=1;
        s->addresses[i]=nvnBufferGetAddress(&s->buffers[i]);REQUIRE(s->addresses[i]);
    }
    memcpy(s->memory[SHADER]+s->offsets[VS],nn_test_graphics.vs.source_code,s->lengths[VS]);
    memcpy(s->memory[SHADER]+s->offsets[FS],nn_test_graphics.fs.source_code,s->lengths[FS]);
    const float bg[3][6]={{-1,-1,0,0,0,1},{3,-1,0,0,0,1},{-1,3,0,0,0,1}};
    float identity[16]={0};identity[0]=identity[5]=identity[10]=identity[15]=1;
    memcpy(s->memory[VERTEX]+s->offsets[BG_VB],bg,sizeof(bg));memcpy(s->memory[MATRIX]+s->offsets[BG_UB],identity,sizeof(identity));
    stage="source-noaccess-pool";
    CHECK(posix_memalign((void **)&s->source_pages,4096,24576));memset(s->source_pages,0xa5,24576);
    /* No CPU source access of any kind after this ownership transfer. */
    CHECK(source_private(s));
    CALL(nvnMemoryPoolBuilderSetDefaults(&s->pb));CALL(nvnMemoryPoolBuilderSetDevice(&s->pb,&s->device));
    CALL(nvnMemoryPoolBuilderSetStorage(&s->pb,s->source_pages,24576));
    CALL(nvnMemoryPoolBuilderSetFlags(&s->pb,NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));
    BOOL(nvnMemoryPoolInitialize(&s->source_pool,&s->pb));s->source_ready=1;
    REQUIRE(!s->observation_error && s->import_observations==1 && s->native_device && s->source_bo);
    REQUIRE(nvnMemoryPoolMap(&s->source_pool)==NULL && !nn_nvn_error());++source_map_rejections;
    stage="caller-texture";
    CALL(nvnTextureBuilderSetDefaults(&s->tb));CALL(nvnTextureBuilderSetDevice(&s->tb,&s->device));
    CALL(nvnTextureBuilderSetFlags(&s->tb,NVN_TEXTURE_FLAGS_LINEAR));CALL(nvnTextureBuilderSetTarget(&s->tb,NVN_TEXTURE_TARGET_2D));
    CALL(nvnTextureBuilderSetSize2D(&s->tb,64,64));CALL(nvnTextureBuilderSetLevels(&s->tb,1));CALL(nvnTextureBuilderSetSamples(&s->tb,0));
    CALL(nvnTextureBuilderSetFormat(&s->tb,NVN_FORMAT_RGBA8));CALL(nvnTextureBuilderSetStride(&s->tb,256));
    CALL(nvnTextureBuilderSetStorage(&s->tb,&s->source_pool,4096));BOOL(nvnTextureInitialize(&s->texture,&s->tb));s->texture_ready=1;
    s->source_address=nvnTextureGetTextureAddress(&s->texture);
    REQUIRE(s->source_address==nn_bo_address(s->source_bo)+4096 && nvnTextureGetStorageSize(&s->texture)==16384);
    stage="new-independent-ahb";CHECK(nn_ahb_create(&s->ahb));
    const AHardwareBuffer_Desc *actual=nn_ahb_description(s->ahb);
    REQUIRE(actual && actual->width==64 && actual->height==64 && actual->stride==64 && actual->layers==1 && actual->usage==NN_AHB_USAGE);
    CHECK(nn_ahb_lock(s->ahb,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,&view));s->locked=1;identifying=1;++identification_locks;
    CHECK(view_ok(&view,NULL));int identification_error=nn_ahb_unlock(s->ahb);s->locked=0;
    if(identification_error){error=identification_error;unsafe=1;goto done;}++identification_unlocks;identifying=0;
    CHECK(nn_ahb_get_pixel(s->ahb,&s->pixel));
    if(fstat(s->pixel.fd,&s->backing)){error=errno;goto done;}
    REQUIRE(s->backing.st_ino && s->backing.st_size>0 && (uint64_t)s->backing.st_size==s->pixel.bytes &&
        s->pixel.offset<=s->pixel.bytes && 16384<=s->pixel.bytes-s->pixel.offset && s->pixel.pitch==256);
    stage="dmabuf-import";
    int import_error=nn_bo_import_dmabuf(s->native_device,s->pixel.fd,s->pixel.bytes,&s->target_bo);
    if(import_error){error=import_error;unsafe=1;goto done;}
    REQUIRE(nn_bo_map(s->target_bo)==NULL);
    stage="program";BOOL(nvnProgramInitialize(&s->program,&s->device));s->program_ready=1;
    const NVNshaderData shaders[2]={{s->addresses[FS],nn_test_graphics.fs.source_control},{s->addresses[VS],nn_test_graphics.vs.source_control}};
    BOOL(nvnProgramSetShaders(&s->program,2,shaders));
    stage="record-standard-nvn";BOOL(nvnCommandBufferInitialize(&s->command,&s->device));s->command_ready=1;
    CALL(nvnCommandBufferAddCommandMemory(&s->command,&s->pools[COMMAND],0,s->sizes[COMMAND]));
    CALL(nvnCommandBufferAddControlMemory(&s->command,s->control,sizeof(s->control)));CALL(nvnCommandBufferBeginRecording(&s->command));
    CALL(nvnCommandBufferBindProgram(&s->command,&s->program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
    NVNvertexAttribState attributes[2];NVNvertexStreamState stream;
    for(unsigned a=0;a<2;a++){
        CALL(nvnVertexAttribStateSetDefaults(&attributes[a]));CALL(nvnVertexAttribStateSetFormat(&attributes[a],a?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,a?8:0));
        CALL(nvnVertexAttribStateSetStreamIndex(&attributes[a],0));
    }
    CALL(nvnVertexStreamStateSetDefaults(&stream));CALL(nvnVertexStreamStateSetStride(&stream,24));CALL(nvnVertexStreamStateSetDivisor(&stream,0));
    CALL(nvnCommandBufferBindVertexAttribState(&s->command,2,attributes));CALL(nvnCommandBufferBindVertexStreamState(&s->command,1,&stream));
    const NVNtexture *targets[1]={&s->texture};CALL(nvnCommandBufferSetRenderTargets(&s->command,1,targets,NULL,NULL,NULL));
    CALL(nvnCommandBufferSetViewport(&s->command,0,0,64,64));CALL(nvnCommandBufferSetScissor(&s->command,0,0,64,64));CALL(nvnCommandBufferSetDepthRange(&s->command,0,1));
    for(unsigned draw=0;draw<2;draw++){
        CALL(nvnCommandBufferBindVertexBuffer(&s->command,0,s->addresses[draw?MAIN_VB:BG_VB],72));
        CALL(nvnCommandBufferBindUniformBuffer(&s->command,NVN_SHADER_STAGE_VERTEX,0,s->addresses[draw?MAIN_UB:BG_UB],64));
        CALL(nvnCommandBufferDrawArrays(&s->command,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
    }
    s->handle=nvnCommandBufferEndRecording(&s->command);REQUIRE(s->handle && !nn_nvn_error());
    CALL(nvnCommandBufferFinalize(&s->command));s->command_ready=0;
    for(unsigned i=0;i<POOLS;i++)CALL(nvnMemoryPoolFlushMappedRange(&s->pools[i],0,s->sizes[i]));
    for(unsigned mode=0;mode<2;mode++)for(unsigned test=0;test<4;test++){
        float matrix[16]={0};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;if(test==1)matrix[3]=1;if(test==2)matrix[3]=3;
        float vertices[3][6]={{-1,-1,1,0,0,1},{3,-1,1,0,0,1},{-1,3,1,0,0,1}};
        if(test==1||test==2)for(unsigned v=0;v<3;v++){vertices[v][2]=0;vertices[v][3]=1;}
        if(test==3){vertices[1][2]=0;vertices[1][3]=1;vertices[2][2]=0;vertices[2][4]=1;}
        memcpy(s->memory[VERTEX]+s->offsets[MAIN_VB],vertices,sizeof(vertices));memcpy(s->memory[MATRIX]+s->offsets[MAIN_UB],matrix,sizeof(matrix));
        for(unsigned i=0;i<POOLS;i++){CALL(nvnMemoryPoolFlushMappedRange(&s->pools[i],0,s->sizes[i]));memcpy(s->shadow[i],s->memory[i],s->sizes[i]);}
        stage="nvn-submit";s->wrapper_phase=1;pending=1;CALL(nvnQueueSubmitCommands(&s->queue,1,&s->handle));s->wrapper_phase=0;
        REQUIRE(!s->observation_error && s->native_queue && s->source_timestamp);
        struct nn_copy_desc desc={.device=s->native_device,.producer=s->native_queue,.source=s->source_bo,.target=s->target_bo,
            .source_offset=4096,.source_bytes=16384,.target_offset=s->pixel.offset,.target_bytes=16384,.source_gpu_address=s->source_address,
            .producer_timestamp=s->source_timestamp,.cpu_no_access=1,.alias_proof=NN_COPY_FRESH_PRIVATE_PAGES_AND_FRESH_AHB,
            .source_image=image,.owner={s,pin,live,prove,unpin}};
        stage="copy-create";CHECK(nn_copy_job_create(&desc,&s->job));
        stage="copy-submit";s->wrapper_phase=2;CHECK(nn_copy_job_submit(s->job,mode?NN_COPY_AFTER_FINISH:NN_COPY_SAME_QUEUE));s->wrapper_phase=0;
        REQUIRE(!s->observation_error);
        stage="copy-finish";CHECK(nn_copy_job_finish(s->job,1000));pending=0;
        struct nn_copy_stats stats;CHECK(nn_copy_job_stats(s->job,&stats));
        REQUIRE(stats.state==NN_COPY_FINISHED && stats.copy_submissions==1 && stats.finish_calls==1 &&
            stats.producer_finish_calls==mode && stats.command_bytes==98324 && stats.command_guard_checks==4076 &&
            stats.command_readonly_checks==98324 && stats.owner_pinned && !stats.resources_retained);
        producer_finishes+=stats.producer_finish_calls;copy_finishes+=stats.finish_calls;copy_submissions+=stats.copy_submissions;
        command_guards+=stats.command_guard_checks;command_readonly+=stats.command_readonly_checks;
        /* Reap NVN jobs AFTER the copy Finish; samequeue mode has no earlier
         * CPU wait. This call observes no pending native BOs and closes refs. */
        CALL(nvnQueueFinish(&s->queue));
        REQUIRE(nvnTextureGetTextureAddress(&s->texture)==s->source_address && nvnMemoryPoolMap(&s->source_pool)==NULL && !nn_nvn_error());++source_map_rejections;
        stage="ahb-read";CHECK(nn_ahb_lock(s->ahb,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,&view));s->locked=1;++locks;
        CHECK(view_ok(&view,&s->pixel));
        for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
            unsigned expected[4]={255,0,0,255};
            if(test==1&&x>=32){expected[0]=0;expected[1]=255;}
            if(test==2||(test==1&&x<32)){expected[0]=expected[1]=expected[2]=0;}
            if(test==3){float green=(x+0.5f)/128,blue=(y+0.5f)/128;expected[0]=rounded(1-green-blue);expected[1]=rounded(green);expected[2]=rounded(blue);}
            for(unsigned c=0;c<4;c++){
                unsigned actual_pixel=((const unsigned char *)view.address)[y*256+x*4+c];
                unsigned delta=actual_pixel>expected[c]?actual_pixel-expected[c]:expected[c]-actual_pixel;
                if(delta>(test==3?1u:0u)){bad_mode=mode;bad_case=test;bad_x=x;bad_y=y;bad_channel=c;bad_actual=actual_pixel;bad_expected=expected[c];error=EIO;goto done;}
                ++channels;
            }
        }
        char path[1024];int n=snprintf(path,sizeof(path),"%s/copy-mode-%u-case-%u.rgba",directory,mode,test);
        REQUIRE(n>=0&&(size_t)n<sizeof(path));FILE *file=fopen(path,"wb");if(!file){error=errno;goto done;}
        int we=fwrite(view.address,1,16384,file)!=16384,ce=fclose(file);if(we||ce){error=EIO;goto done;}
        stage="ahb-unlock";int ue=nn_ahb_unlock(s->ahb);s->locked=0;if(ue){error=ue;unsafe=1;goto done;}++unlocks;
        stage="copy-destroy";CHECK(nn_copy_job_destroy(s->job));s->job=NULL;++destroyed_jobs;
        for(unsigned i=0;i<POOLS;i++){
            CALL(nvnMemoryPoolInvalidateMappedRange(&s->pools[i],0,s->sizes[i]));
            for(size_t b=0;b<s->sizes[i];b++){if(s->memory[i][b]!=s->shadow[i][b]){error=EIO;goto done;}++readonly;}
        }
        ++completed;
    }
    stage="complete";
done:
    if(s){
        s->wrapper_phase=0;
        if(s->observation_error)unsafe=1;
        if(s->locked){int e_=nn_ahb_unlock(s->ahb);s->locked=0;if(e_)unsafe=1;else if(identifying)++identification_unlocks;else ++unlocks;}
        if(s->job){struct nn_copy_stats stats;nn_copy_job_stats(s->job,&stats);
            if(stats.state==NN_COPY_SUBMITTED || stats.state==NN_COPY_RETAINED)unsafe=1;
            else {int e_=nn_copy_job_destroy(s->job);if(e_)unsafe=1;else s->job=NULL;}}
        if(pending || unsafe || s->owner_pins)cleanup=EBUSY;
        else {
#define CLEAN(x) do{nn_nvn_clear_error();x;if(nn_nvn_error()){cleanup=nn_nvn_error();goto cleanup_end;}}while(0)
            if(s->command_ready){CLEAN(nvnCommandBufferFinalize(&s->command));s->command_ready=0;}
            if(s->handle){CLEAN(nvnDeviceFinalizeCommandHandle(&s->device,s->handle));s->handle=0;}
            if(s->texture_ready){CLEAN(nvnTextureFinalize(&s->texture));s->texture_ready=0;}
            if(s->source_ready){CLEAN(nvnMemoryPoolFinalize(&s->source_pool));s->source_ready=0;}
            if(s->program_ready){CLEAN(nvnProgramFinalize(&s->program));s->program_ready=0;}
            for(unsigned i=0;i<VIEWS;i++)if(s->buffer_ready[i]){CLEAN(nvnBufferFinalize(&s->buffers[i]));s->buffer_ready[i]=0;}
            for(unsigned i=0;i<POOLS;i++)if(s->pool_ready[i]){CLEAN(nvnMemoryPoolFinalize(&s->pools[i]));s->pool_ready[i]=0;}
            if(s->target_bo){cleanup=nn_bo_destroy(s->target_bo);if(cleanup)goto cleanup_end;s->target_bo=NULL;}
            if(s->ahb){cleanup=nn_ahb_destroy(s->ahb);if(cleanup)goto cleanup_end;s->ahb=NULL;}
            if(s->queue_ready){CLEAN(nvnQueueFinalize(&s->queue));s->queue_ready=0;}
            if(s->device_ready){CLEAN(nvnDeviceFinalize(&s->device));s->device_ready=0;}
#undef CLEAN
        }
    }
cleanup_end:;
    int passed=!error&&!cleanup&&s&&completed==8&&channels==131072&&locks==8&&unlocks==8&&
        producer_finishes==4&&copy_submissions==8&&copy_finishes==8&&destroyed_jobs==8&&
        s->nvn_observed==8&&s->nvn_attempts==8&&s->copy_observed==8&&s->copy_attempts==8&&s->import_observations==1&&
        s->owner_pin_calls==8&&s->owner_unpin_calls==8&&!s->owner_pins&&source_map_rejections==9&&s->source_private_verified;
    int n=snprintf(json,capacity,
        "{\"schema\":1,\"operation\":\"nvn_gpu_copy\",\"uid\":%u,\"chip_id\":%u,\"image_schema\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,"
        "\"completed_cases\":%u,\"channel_checks\":%u,\"nvn_submissions_observed\":%u,\"copy_submissions_observed\":%u,\"copy_submissions\":%u,\"copy_finishes\":%u,\"intermediate_producer_finishes\":%u,"
        "\"source_import_observations\":%u,\"source_pool_cpu_no_access\":true,\"source_map_rejections\":%u,\"source_cpu_reads\":0,\"source_cpu_writes_after_import\":0,\"source_gpu_address\":%" PRIu64 ",\"source_offset\":4096,\"source_pool_bytes\":24576,"
        "\"source_private_vma_verified\":%s,\"source_backing_replaced\":false,\"target_cpu_writes\":0,\"cpu_image_copy\":false,\"raw_draw_calls\":0,\"readback_locks\":%u,\"readback_unlocks\":%u,\"identification_read_locks\":%u,\"identification_read_unlocks\":%u,"
        "\"copy_command_guard_checks\":%zu,\"copy_command_readonly_checks\":%zu,\"input_readonly_checks\":%zu,\"owner_pin_calls\":%u,\"owner_unpin_calls\":%u,"
        "\"target\":{\"fd_at_import\":%d,\"fd_index\":%d,\"fd_bytes\":%" PRIu64 ",\"pixel_offset\":%" PRIu64 ",\"inode\":%" PRIu64 ",\"device_major\":%u,\"device_minor\":%u,\"pitch\":%u},"
        "\"source_guard_gpu_checked\":false,\"padded_stride_gpu_checked\":false,\"window_presented\":false,\"game_modules_executed\":false,\"resources_retained\":%s,"
        "\"bad_mode\":%u,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,\"bad_actual\":%u,\"bad_expected\":%u,\"passed\":%s}\n",
        (unsigned)getuid(),image?image->chip:0,image?image->schema:0,stage,error,cleanup,completed,channels,s?s->nvn_observed:0,s?s->copy_observed:0,
        copy_submissions,copy_finishes,producer_finishes,s?s->import_observations:0,source_map_rejections,s?s->source_address:0,
        s&&s->source_private_verified?"true":"false",locks,unlocks,identification_locks,identification_unlocks,command_guards,command_readonly,readonly,s?s->owner_pin_calls:0,s?s->owner_unpin_calls:0,
        s?s->pixel.fd:-1,s?s->pixel.fd_index:-1,s?s->pixel.bytes:0,s?s->pixel.offset:0,s?(uint64_t)s->backing.st_ino:0,
        s?(unsigned)major(s->backing.st_dev):0,s?(unsigned)minor(s->backing.st_dev):0,s?s->pixel.pitch:0,
        cleanup?"true":"false",bad_mode,bad_case,bad_x,bad_y,bad_channel,bad_actual,bad_expected,passed?"true":"false");
    if(s && !cleanup){for(unsigned i=0;i<POOLS;i++){free(s->memory[i]);free(s->shadow[i]);}free(s->source_pages);active=NULL;free(s);}
#undef BOOL
#undef CALL
#undef REQUIRE
#undef CHECK
    return passed&&n>=0&&(size_t)n<capacity?0:-1;
}
