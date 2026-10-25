/* SPDX-License-Identifier: MIT */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include "nvn_native/nvn.h"
#include "../android/ahb_image_pool.h"
#include "external_image_probe.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include NVN_GRAPHICS_API_HEADER

/* Qualification caller only. All target pixels, including background, are
 * produced through NVN Draw. CPU locks only READ, after QueueFinish; no raw
 * draw/submit, allocator parser, vendor-handle fields or CPU image copy. */
enum { SOURCE, VERTEX, MATRIX, COMMAND, POOLS };
enum { VS, FS, BG_VB, MAIN_VB, BG_UB, MAIN_UB, VIEWS };
#define GUARDED(type) struct { unsigned char before[16]; type value; unsigned char after[16]; }
struct probe_state {
    GUARDED(NVNdevice) device; GUARDED(NVNqueue) queue; GUARDED(NVNprogram) program;
    GUARDED(NVNcommandBuffer) command;
    GUARDED(NVNmemoryPool) pools[POOLS], targets[3];
    GUARDED(NVNtexture) textures[3]; GUARDED(NVNbuffer) buffers[VIEWS];
    NVNdeviceBuilder db; NVNqueueBuilder qb; NVNmemoryPoolBuilder pb;
    NVNbufferBuilder bb; NVNtextureBuilder tb;
    _Alignas(16) unsigned char control[3][512];
    unsigned char *memory[POOLS], *shadow[POOLS];
    size_t sizes[POOLS], offsets[VIEWS], lengths[VIEWS];
    size_t command_used[3], control_used[3];
    NVNbufferAddress addresses[VIEWS]; NVNcommandHandle handles[3];
    struct nn_ahb_image_pool_report ahb[3];
    struct nn_ahb_pixel pixel[3];
    uint64_t inode[3]; uint32_t device_major[3],device_minor[3];
    unsigned device_ready,queue_ready,program_ready,command_ready;
    unsigned pool_ready[POOLS],buffer_ready[VIEWS],target_ready[3],texture_ready[3],pins[3];
};
static unsigned rounded(float x) { return (unsigned)(255.0f*x+0.5f); }
static int bytes_equal(const void *memory,const void *shadow,size_t bytes,uint64_t *checks)
{
    const unsigned char *a=memory,*b=shadow;
    for(size_t i=0;i<bytes;i++){if(a[i]!=b[i])return EIO;++*checks;}
    return 0;
}
static int guard(const unsigned char *p,size_t n,uint64_t *checks)
{
    for(size_t i=0;i<n;i++){if(p[i]!=0x5a)return EIO;++*checks;}return 0;
}
static int ahb_view(const struct nn_ahb_view *v,const struct nn_ahb_pixel *p)
{
    /* Existing nn_ahb_lock performs the actual dev/ino/VMA identification. */
    return !v->address || v->bytes_per_pixel!=4 || v->bytes_per_stride!=256 ||
        !v->mapping.found || !v->mapping.shared || v->mapping.matches!=1 ||
        strcmp(v->mapping.permissions,"rw-s") || v->mapping.payload_bytes!=16384 ||
        v->mapping.fd_index!=p->fd_index || v->mapping.offset!=p->offset ? ENOTSUP : 0;
}
int nn_nvn_external_image_probe(const char *directory,char *json,size_t capacity,unsigned slots)
{
    const struct nn_gfx_image *image=nn_test_graphics.image;
    const char *stage="arguments",*submission="not_attempted";
    struct probe_state *s=NULL;
    unsigned completed=0,submissions=0,channel_checks=0,locks=0,unlocks=0,negatives=0;
    unsigned pools_released=0,imported=0,bad_slot=~0u,bad_case=~0u,bad_x=~0u,bad_y=~0u,bad_channel=~0u;
    unsigned bad_actual=0,bad_expected=0,guarded_objects=0;
    uint64_t readonly_checks=0,guard_checks=0;
    int error=0,cleanup=0,pending=0,unsafe=0,registered=0,locked=0;
    unsigned active_slot=0;
    struct nn_ahb_view locked_view={0};
    if(!json || capacity<4096)return -1;
#define ENTRY(name,type) type name=(type)nvnBootstrapLoader(#name);if(!name)error=ENOSYS;
#include "../src/nvn_entries.inc"
#undef ENTRY
#define CALL(x) do{x;int e_=nn_nvn_error();if(e_){error=e_;goto done;}}while(0)
#define BOOL(x) do{int ok_=(x),e_=nn_nvn_error();if(!ok_||e_){error=e_?e_:EIO;goto done;}}while(0)
#define REQUIRE(x) do{if(!(x)){error=ENOTSUP;goto done;}}while(0)
#define CHECK(x) do{int e_=(x);if(e_){error=e_;goto done;}}while(0)
#define REJECT(x,e) do{x;if(nn_nvn_error()!=(e)){error=EIO;goto done;}nn_nvn_clear_error();++negatives;}while(0)
#define GUARD(x) do{int e_=guard((x).before,sizeof((x).before),&guard_checks);if(!e_)e_=guard((x).after,sizeof((x).after),&guard_checks);if(e_){error=e_;unsafe=1;goto done;}++guarded_objects;}while(0)
    if(error)goto done;
    nn_nvn_clear_error();
    REQUIRE(directory && (slots==2||slots==3));
    stage="profile";
    REQUIRE(image && image->schema==3 && image->profile==NN_GFX_COLOR && image->chip==0x06050002 &&
        image->width==64 && image->height==64 && image->target_pitch==256 && image->target_bytes==16384 &&
        image->vertex_bytes==72 && image->matrix_bytes==64 && sysconf(_SC_PAGESIZE)==4096 &&
        nn_test_graphics.vs.source_code && nn_test_graphics.fs.source_code &&
        nn_test_graphics.vs.source_control && nn_test_graphics.fs.source_control &&
        nn_test_graphics.vs.source_bytes && nn_test_graphics.fs.source_bytes &&
        nn_test_graphics.vs.source_bytes<=1048576 && nn_test_graphics.fs.source_bytes<=1048576);
    s=calloc(1,sizeof(*s));if(!s){error=ENOMEM;goto done;}
#define INIT(x) memset(&(x),0x5a,sizeof(x))
    INIT(s->device);INIT(s->queue);INIT(s->program);INIT(s->command);INIT(s->pools);INIT(s->targets);INIT(s->textures);INIT(s->buffers);
#undef INIT
    memset(s->control,0xa5,sizeof(s->control));
    for(unsigned i=0;i<3;i++){s->pixel[i].fd=-1;s->pixel[i].fd_index=-1;}
    const unsigned view_pool[VIEWS]={SOURCE,SOURCE,VERTEX,VERTEX,MATRIX,MATRIX};
    s->offsets[VS]=4096;s->lengths[VS]=nn_test_graphics.vs.source_bytes;
    s->offsets[FS]=((s->offsets[VS]+s->lengths[VS]+4095)&~(size_t)4095)+4096;s->lengths[FS]=nn_test_graphics.fs.source_bytes;
    s->sizes[SOURCE]=((s->offsets[FS]+s->lengths[FS]+4095)&~(size_t)4095)+4096;
    s->offsets[BG_VB]=256;s->offsets[MAIN_VB]=512;s->lengths[BG_VB]=s->lengths[MAIN_VB]=72;
    s->offsets[BG_UB]=64;s->offsets[MAIN_UB]=128;s->lengths[BG_UB]=s->lengths[MAIN_UB]=64;
    s->sizes[VERTEX]=s->sizes[MATRIX]=s->sizes[COMMAND]=4096;
    stage="device";
    CALL(nvnDeviceBuilderSetDefaults(&s->db));BOOL(nvnDeviceInitialize(&s->device.value,&s->db));s->device_ready=1;
    REQUIRE(nvnDeviceGetWindowOriginMode(&s->device.value)==NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT);
    REQUIRE(nvnDeviceGetDepthMode(&s->device.value)==NVN_DEPTH_MODE_NEAR_IS_MINUS_W);
    CHECK(nn_nvn_register_graphics(&s->device.value,&nn_test_graphics));REQUIRE(!nn_nvn_error());registered=1;
    CALL(nvnQueueBuilderSetDefaults(&s->qb));CALL(nvnQueueBuilderSetDevice(&s->qb,&s->device.value));
    BOOL(nvnQueueInitialize(&s->queue.value,&s->qb));s->queue_ready=1;
    stage="ordinary-pools";
    for(unsigned i=0;i<POOLS;i++){
        CHECK(posix_memalign((void **)&s->memory[i],4096,s->sizes[i]));memset(s->memory[i],0xa5,s->sizes[i]);
        s->shadow[i]=malloc(s->sizes[i]);if(!s->shadow[i]){error=ENOMEM;goto done;}
        CALL(nvnMemoryPoolBuilderSetDefaults(&s->pb));CALL(nvnMemoryPoolBuilderSetDevice(&s->pb,&s->device.value));
        CALL(nvnMemoryPoolBuilderSetStorage(&s->pb,s->memory[i],s->sizes[i]));
        CALL(nvnMemoryPoolBuilderSetFlags(&s->pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED|
            (i==SOURCE?NVN_MEMORY_POOL_FLAGS_SHADER_CODE:0)));
        BOOL(nvnMemoryPoolInitialize(&s->pools[i].value,&s->pb));s->pool_ready[i]=1;
        REQUIRE(nvnMemoryPoolMap(&s->pools[i].value)==s->memory[i]);
    }
    for(unsigned i=0;i<VIEWS;i++){
        CALL(nvnBufferBuilderSetDefaults(&s->bb));CALL(nvnBufferBuilderSetDevice(&s->bb,&s->device.value));
        CALL(nvnBufferBuilderSetStorage(&s->bb,&s->pools[view_pool[i]].value,s->offsets[i],s->lengths[i]));
        BOOL(nvnBufferInitialize(&s->buffers[i].value,&s->bb));s->buffer_ready[i]=1;
        REQUIRE(nvnBufferMap(&s->buffers[i].value)==s->memory[view_pool[i]]+s->offsets[i]);
        s->addresses[i]=nvnBufferGetAddress(&s->buffers[i].value);REQUIRE(s->addresses[i]);
    }
    memcpy(s->memory[SOURCE]+s->offsets[VS],nn_test_graphics.vs.source_code,s->lengths[VS]);
    memcpy(s->memory[SOURCE]+s->offsets[FS],nn_test_graphics.fs.source_code,s->lengths[FS]);
    const float background_vertices[3][6]={{-1,-1,0,0,0,1},{3,-1,0,0,0,1},{-1,3,0,0,0,1}};
    float identity[16]={0};identity[0]=identity[5]=identity[10]=identity[15]=1;
    memcpy(s->memory[VERTEX]+s->offsets[BG_VB],background_vertices,sizeof(background_vertices));
    memcpy(s->memory[MATRIX]+s->offsets[BG_UB],identity,sizeof(identity));
    stage="program";BOOL(nvnProgramInitialize(&s->program.value,&s->device.value));s->program_ready=1;
    const NVNshaderData shaders[2]={{s->addresses[FS],nn_test_graphics.fs.source_control},{s->addresses[VS],nn_test_graphics.vs.source_control}};
    BOOL(nvnProgramSetShaders(&s->program.value,2,shaders));
    for(unsigned i=0;i<slots;i++){
        stage="external-pool";CHECK(nn_nvn_ahb_image_pool_create(&s->device.value,&s->targets[i].value,&s->ahb[i]));
        s->target_ready[i]=1;++imported;REQUIRE(s->ahb[i].transferred && s->ahb[i].borrowed_owner);
        CHECK(nn_ahb_get_pixel(s->ahb[i].borrowed_owner,&s->pixel[i]));
        REQUIRE(s->pixel[i].fd>=0 && s->pixel[i].fd_index==s->ahb[i].fd_index &&
            s->pixel[i].offset==s->ahb[i].pixel_offset && s->pixel[i].bytes==s->ahb[i].fd_bytes);
        struct stat backing;if(fstat(s->pixel[i].fd,&backing)){error=errno;goto done;}
        REQUIRE(backing.st_size>0 && (uint64_t)backing.st_size==s->pixel[i].bytes);
        s->inode[i]=backing.st_ino;s->device_major[i]=major(backing.st_dev);s->device_minor[i]=minor(backing.st_dev);
        for(unsigned j=0;j<i;j++)REQUIRE(s->inode[j]!=s->inode[i] || s->device_major[j]!=s->device_major[i] || s->device_minor[j]!=s->device_minor[i]);
        struct nn_external_image_view view;CHECK(nn_nvn_image_pool_view(&s->targets[i].value,&view));
        REQUIRE(view.display_buffer==nn_ahb_buffer(s->ahb[i].borrowed_owner) && view.pixel_offset==s->pixel[i].offset &&
            view.fd_bytes==s->pixel[i].bytes && view.payload_bytes==16384);
        REQUIRE(nn_bo_map(view.bo)==NULL);
        REQUIRE(nvnMemoryPoolMap(&s->targets[i].value)==NULL);REQUIRE(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();++negatives;
        REJECT(nvnMemoryPoolFlushMappedRange(&s->targets[i].value,0,0),ENOTSUP);
        REJECT(nvnMemoryPoolInvalidateMappedRange(&s->targets[i].value,0,0),ENOTSUP);
        CALL(nvnBufferBuilderSetDefaults(&s->bb));CALL(nvnBufferBuilderSetDevice(&s->bb,&s->device.value));
        REJECT(nvnBufferBuilderSetStorage(&s->bb,&s->targets[i].value,(ptrdiff_t)s->pixel[i].offset,64),ENOTSUP);
        stage="caller-texture";
        CALL(nvnTextureBuilderSetDefaults(&s->tb));CALL(nvnTextureBuilderSetDevice(&s->tb,&s->device.value));
        CALL(nvnTextureBuilderSetFlags(&s->tb,NVN_TEXTURE_FLAGS_LINEAR));CALL(nvnTextureBuilderSetTarget(&s->tb,NVN_TEXTURE_TARGET_2D));
        CALL(nvnTextureBuilderSetSize2D(&s->tb,64,64));CALL(nvnTextureBuilderSetLevels(&s->tb,1));
        CALL(nvnTextureBuilderSetSamples(&s->tb,0));CALL(nvnTextureBuilderSetFormat(&s->tb,NVN_FORMAT_RGBA8));
        CALL(nvnTextureBuilderSetStride(&s->tb,256));REQUIRE(nvnTextureBuilderGetStorageSize(&s->tb)==16384);
        CALL(nvnTextureBuilderSetStorage(&s->tb,&s->targets[i].value,(ptrdiff_t)s->pixel[i].offset));
        BOOL(nvnTextureInitialize(&s->textures[i].value,&s->tb));s->texture_ready[i]=1;
        CHECK(nn_nvn_image_pool_pin(&s->targets[i].value));s->pins[i]=1;
        REQUIRE(nvnTextureGetTextureAddress(&s->textures[i].value)==nn_bo_address(view.bo)+s->pixel[i].offset);
        REQUIRE(nvnTextureGetStorageSize(&s->textures[i].value)==16384 && nvnTextureGetStride(&s->textures[i].value)==256);
        REJECT(nvnMemoryPoolFinalize(&s->targets[i].value),EBUSY);
        stage="record";
        BOOL(nvnCommandBufferInitialize(&s->command.value,&s->device.value));s->command_ready=1;
        REJECT(nvnCommandBufferAddCommandMemory(&s->command.value,&s->targets[i].value,(ptrdiff_t)s->pixel[i].offset,64),ENOTSUP);
        CALL(nvnCommandBufferAddCommandMemory(&s->command.value,&s->pools[COMMAND].value,i*1024,1024));
        CALL(nvnCommandBufferAddControlMemory(&s->command.value,s->control[i],sizeof(s->control[i])));
        CALL(nvnCommandBufferBeginRecording(&s->command.value));
        CALL(nvnCommandBufferBindProgram(&s->command.value,&s->program.value,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
        NVNvertexAttribState attributes[2];NVNvertexStreamState stream;
        for(unsigned a=0;a<2;a++){
            CALL(nvnVertexAttribStateSetDefaults(&attributes[a]));
            CALL(nvnVertexAttribStateSetFormat(&attributes[a],a?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,a?8:0));
            CALL(nvnVertexAttribStateSetStreamIndex(&attributes[a],0));
        }
        CALL(nvnVertexStreamStateSetDefaults(&stream));CALL(nvnVertexStreamStateSetStride(&stream,24));CALL(nvnVertexStreamStateSetDivisor(&stream,0));
        CALL(nvnCommandBufferBindVertexAttribState(&s->command.value,2,attributes));
        CALL(nvnCommandBufferBindVertexStreamState(&s->command.value,1,&stream));
        const NVNtexture *targets[1]={&s->textures[i].value};
        CALL(nvnCommandBufferSetRenderTargets(&s->command.value,1,targets,NULL,NULL,NULL));
        CALL(nvnCommandBufferSetViewport(&s->command.value,0,0,64,64));CALL(nvnCommandBufferSetScissor(&s->command.value,0,0,64,64));
        CALL(nvnCommandBufferSetDepthRange(&s->command.value,0.0f,1.0f));
        /* Two independent input ranges; all background pixels are GPU-written. */
        for(unsigned draw=0;draw<2;draw++){
            CALL(nvnCommandBufferBindVertexBuffer(&s->command.value,0,s->addresses[draw?MAIN_VB:BG_VB],72));
            CALL(nvnCommandBufferBindUniformBuffer(&s->command.value,NVN_SHADER_STAGE_VERTEX,0,s->addresses[draw?MAIN_UB:BG_UB],64));
            CALL(nvnCommandBufferDrawArrays(&s->command.value,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
        }
        s->handles[i]=nvnCommandBufferEndRecording(&s->command.value);REQUIRE(s->handles[i] && !nn_nvn_error());
        s->command_used[i]=nvnCommandBufferGetCommandMemoryUsed(&s->command.value);
        s->control_used[i]=nvnCommandBufferGetControlMemoryUsed(&s->command.value);
        REQUIRE(s->command_used[i]>0 && s->command_used[i]<=1024 && s->control_used[i]>0 && s->control_used[i]<=sizeof(s->control[i]));
        CALL(nvnCommandBufferFinalize(&s->command.value));s->command_ready=0;
    }
    for(unsigned i=0;i<POOLS;i++)CALL(nvnMemoryPoolFlushMappedRange(&s->pools[i].value,0,s->sizes[i]));
    for(unsigned slot=0;slot<slots;slot++)for(unsigned test=0;test<4;test++){
        active_slot=slot;
        float matrix[16]={0};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;
        if(test==1)matrix[3]=1;
        if(test==2)matrix[3]=3;
        float vertices[3][6]={{-1,-1,1,0,0,1},{3,-1,1,0,0,1},{-1,3,1,0,0,1}};
        if(test==1||test==2)for(unsigned v=0;v<3;v++){vertices[v][2]=0;vertices[v][3]=1;}
        if(test==3){vertices[1][2]=0;vertices[1][3]=1;vertices[2][2]=0;vertices[2][4]=1;}
        memcpy(s->memory[VERTEX]+s->offsets[MAIN_VB],vertices,sizeof(vertices));
        memcpy(s->memory[MATRIX]+s->offsets[MAIN_UB],matrix,sizeof(matrix));
        for(unsigned i=0;i<POOLS;i++)memcpy(s->shadow[i],s->memory[i],s->sizes[i]);
        stage="flush";CALL(nvnMemoryPoolFlushMappedRange(&s->pools[VERTEX].value,0,s->sizes[VERTEX]));
        CALL(nvnMemoryPoolFlushMappedRange(&s->pools[MATRIX].value,0,s->sizes[MATRIX]));
        stage="nvn-submit";submission="unknown";pending=1;
        CALL(nvnQueueSubmitCommands(&s->queue.value,1,&s->handles[slot]));++submissions;submission="accepted";
        REJECT(nvnMemoryPoolFinalize(&s->targets[slot].value),EBUSY);
        stage="nvn-finish";CALL(nvnQueueFinish(&s->queue.value));pending=0;
        stage="read-only-lock";
        int lock_error=nn_ahb_lock(s->ahb[slot].borrowed_owner,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,&locked_view);
        if(lock_error){error=lock_error;unsafe=1;goto done;}
        locked=1;++locks;CHECK(ahb_view(&locked_view,&s->pixel[slot]));
        stage="pixels";
        for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
            unsigned expected[4]={255,0,0,255};
            if(test==1 && x>=32){expected[0]=0;expected[1]=255;}
            if(test==2 || (test==1 && x<32)){expected[0]=expected[1]=expected[2]=0;expected[3]=255;}
            if(test==3){float green=(x+0.5f)/128.0f,blue=(y+0.5f)/128.0f;
                expected[0]=rounded(1-green-blue);expected[1]=rounded(green);expected[2]=rounded(blue);}
            for(unsigned c=0;c<4;c++){
                unsigned actual=((const unsigned char *)locked_view.address)[y*256+x*4+c];
                unsigned delta=actual>expected[c]?actual-expected[c]:expected[c]-actual;
                if(delta>(test==3?1u:0u)){bad_slot=slot;bad_case=test;bad_x=x;bad_y=y;bad_channel=c;
                    bad_actual=actual;bad_expected=expected[c];error=EIO;goto save_readback;}
                ++channel_checks;
            }
        }
save_readback:;
        char path[1024];int path_bytes=snprintf(path,sizeof(path),"%s/external-slot-%u-case-%u.rgba",directory,slot,test);
        if(path_bytes<0 || (size_t)path_bytes>=sizeof(path)){if(!error)error=ENAMETOOLONG;goto done;}
        FILE *file=fopen(path,"wb");if(!file){if(!error)error=errno;goto done;}
        int write_error=fwrite(locked_view.address,1,16384,file)!=16384,close_error=fclose(file);
        if(write_error||close_error){if(!error)error=EIO;goto done;}
        stage=error?"pixels":"read-only-unlock";
        int unlock_error=nn_ahb_unlock(s->ahb[slot].borrowed_owner);locked=0;
        if(unlock_error){unsafe=1;if(!error)error=unlock_error;goto done;}++unlocks;
        if(error)goto done;
        stage="readonly-and-guards";
        for(unsigned i=0;i<POOLS;i++){
            CALL(nvnMemoryPoolInvalidateMappedRange(&s->pools[i].value,0,s->sizes[i]));
            CHECK(bytes_equal(s->memory[i],s->shadow[i],s->sizes[i],&readonly_checks));
        }
        GUARD(s->device);GUARD(s->queue);GUARD(s->program);GUARD(s->command);
        for(unsigned i=0;i<POOLS;i++)GUARD(s->pools[i]);
        for(unsigned i=0;i<VIEWS;i++)GUARD(s->buffers[i]);
        for(unsigned i=0;i<slots;i++){
            GUARD(s->targets[i]);GUARD(s->textures[i]);
            const unsigned char *tail=s->control[i]+s->control_used[i];
            size_t tail_bytes=sizeof(s->control[i])-s->control_used[i];
            for(size_t byte=0;byte<tail_bytes;byte++){REQUIRE(tail[byte]==0xa5);++guard_checks;}
        }
        ++completed;
    }
    stage="complete";
done:
    if(s){
        for(unsigned i=0;i<3;i++)if(s->ahb[i].resources_retained)unsafe=1;
        if(locked){int e_=nn_ahb_unlock(s->ahb[active_slot].borrowed_owner);locked=0;
            if(e_){unsafe=1;if(!error)error=e_;}else ++unlocks;}
        if(pending||unsafe)cleanup=EBUSY;
        else {
#define CLEAN(x) do{nn_nvn_clear_error();x;int e_=nn_nvn_error();if(e_){cleanup=e_;goto cleanup_end;}}while(0)
            if(s->command_ready){CLEAN(nvnCommandBufferFinalize(&s->command.value));s->command_ready=0;}
            for(unsigned i=0;i<slots;i++)if(s->handles[i]){CLEAN(nvnDeviceFinalizeCommandHandle(&s->device.value,s->handles[i]));s->handles[i]=0;}
            for(unsigned i=0;i<slots;i++){
                if(s->texture_ready[i]){CLEAN(nvnTextureFinalize(&s->textures[i].value));s->texture_ready[i]=0;}
                if(s->target_ready[i] && s->pins[i]){
                    nn_nvn_clear_error();nvnMemoryPoolFinalize(&s->targets[i].value);
                    if(nn_nvn_error()!=EBUSY){cleanup=EIO;goto cleanup_end;}nn_nvn_clear_error();++negatives;
                    int e_=nn_nvn_image_pool_unpin(&s->targets[i].value);if(e_){cleanup=e_;goto cleanup_end;}s->pins[i]=0;
                }
                if(s->target_ready[i]){CLEAN(nvnMemoryPoolFinalize(&s->targets[i].value));s->target_ready[i]=0;
                    s->ahb[i].borrowed_owner=NULL;++pools_released;}
            }
            if(s->program_ready){CLEAN(nvnProgramFinalize(&s->program.value));s->program_ready=0;}
            for(unsigned i=0;i<VIEWS;i++)if(s->buffer_ready[i]){CLEAN(nvnBufferFinalize(&s->buffers[i].value));s->buffer_ready[i]=0;}
            for(unsigned i=0;i<POOLS;i++)if(s->pool_ready[i]){CLEAN(nvnMemoryPoolFinalize(&s->pools[i].value));s->pool_ready[i]=0;}
            if(s->queue_ready){CLEAN(nvnQueueFinalize(&s->queue.value));s->queue_ready=0;}
            if(s->device_ready){CLEAN(nvnDeviceFinalize(&s->device.value));s->device_ready=0;}
#undef CLEAN
        }
    }
cleanup_end:;
    char identities[1024];size_t identity_bytes=0;identities[0]=0;
    if(s)for(unsigned i=0;i<slots;i++){
        int n_=snprintf(identities+identity_bytes,sizeof(identities)-identity_bytes,
            "%s{\"slot\":%u,\"fd_at_import\":%d,\"fd_index\":%d,\"fd_bytes\":%" PRIu64 ",\"pixel_offset\":%" PRIu64
            ",\"inode\":%" PRIu64 ",\"device_major\":%u,\"device_minor\":%u,\"pitch\":%u}",
            i?",":"",i,s->pixel[i].fd,s->pixel[i].fd_index,s->pixel[i].bytes,s->pixel[i].offset,
            s->inode[i],s->device_major[i],s->device_minor[i],s->pixel[i].pitch);
        if(n_<0 || (size_t)n_>=sizeof(identities)-identity_bytes){if(!error)error=EOVERFLOW;identities[0]=0;break;}
        identity_bytes+=(size_t)n_;
    }
    int passed=!error && !cleanup && registered && (slots==2||slots==3) &&
        completed==slots*4 && submissions==completed && channel_checks==slots*65536 &&
        locks==completed && unlocks==locks && readonly_checks && guard_checks && pools_released==slots;
    int n=snprintf(json,capacity,
        "{\"schema\":1,\"operation\":\"nvn_external_image\",\"uid\":%u,\"stage\":\"%s\","
        "\"errno\":%d,\"cleanup_errno\":%d,\"slots\":%u,\"image_schema\":%u,\"chip_id\":%u,"
        "\"submission\":\"%s\",\"nvn_submissions\":%u,\"completed_cases\":%u,\"channel_checks\":%u,"
        "\"readback_locks\":%u,\"readback_unlocks\":%u,\"mapping_identification_locks\":%u,"
        "\"negative_checks\":%u,\"readonly_byte_checks\":%" PRIu64 ",\"guard_byte_checks\":%" PRIu64 ","
        "\"guarded_object_checks\":%u,\"external_pools_imported\":%u,\"external_pools_released\":%u,"
        "\"target_cpu_writes\":0,\"cpu_image_copy\":false,\"raw_draw_calls\":0,\"window_presented\":false,"
        "\"game_modules_executed\":false,\"resources_retained\":%s,\"targets\":[%s],"
        "\"bad_slot\":%u,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,"
        "\"bad_actual\":%u,\"bad_expected\":%u,\"passed\":%s}\n",
        (unsigned)getuid(),stage,error,cleanup,slots,image?image->schema:0,registered?image->chip:0,
        submission,submissions,completed,channel_checks,locks,unlocks,imported,negatives,readonly_checks,guard_checks,
        guarded_objects,imported,pools_released,cleanup?"true":"false",identities,
        bad_slot,bad_case,bad_x,bad_y,bad_channel,bad_actual,bad_expected,passed?"true":"false");
    if(s && !cleanup){for(unsigned i=0;i<POOLS;i++){free(s->memory[i]);free(s->shadow[i]);}free(s);}

#undef GUARD
#undef REJECT
#undef CHECK
#undef REQUIRE
#undef BOOL
#undef CALL
    return passed && n>=0 && (size_t)n<capacity ? 0 : -1;
}
#undef GUARDED
