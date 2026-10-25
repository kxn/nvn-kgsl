/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200112L
#include "nvn_native/nvn.h"
#include "nvn_native/graphics_image.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_GRAPHICS_API_HEADER

/* This is an NVN qualification caller, not execution of a game module. */
#define GUARDED(type) struct { uint64_t before[2]; type value; uint64_t after[2]; }
static unsigned rounded(float value) { return (unsigned)(value * 255.0f + 0.5f); }
static int check_bytes(const void *memory, size_t bytes, unsigned expected, uint64_t *checks)
{
    const unsigned char *p = memory;
    for (size_t i = 0; i < bytes; ++i) {
        if (p[i] != expected) return EIO;
        ++*checks;
    }
    return 0;
}

/* Closed-form original pass-color shader oracle, no machine-code decoding. */
static void draw_expected(unsigned mode,unsigned repeat,unsigned x,unsigned expected[4])
{
    memset(expected,0,4*sizeof(*expected));
    if((repeat&&(mode==0||mode==2))||(repeat&&x<32))return;
    float component=repeat?.125f:.25f;unsigned result=rounded(component*((mode==1||mode==2)&&!repeat?1:2));
    if(mode<3)expected[mode]=result;else expected[0]=expected[1]=expected[2]=result;
    expected[3]=result;
}

int nn_nvn_graphics_probe(const char *directory, char *json, size_t capacity)
{
    enum { SOURCE, TARGET, VERTEX, MATRIX, COMMAND, ARGUMENTS, POOLS };
    enum { VS, FS, RT, VB, UB, IDX, INDIRECT, COUNT, VIEWS };
    const struct nn_gfx_image *image = nn_test_graphics.image;
    const char *stage = "resolver", *submission = "not_attempted";
    int error = 0, cleanup = 0, pending = 0, registered = 0;
    int device_ready = 0, queue_ready = 0, program_ready = 0, texture_ready = 0, command_ready = 0;
    int pool_ready[POOLS] = {0}, buffer_ready[VIEWS] = {0};
    unsigned completed = 0, channel_checks = 0, submissions = 0;
    unsigned handles_recorded=0,handles_released=0,replayed=0,gpu_count_writes=0,count_zero_checks=0,count_readback_checks=0,indirect_barriers=0,argument_updates=0,degenerate_cases=0,nondegenerate_cases=0;
    uint64_t readonly_checks=0;
    uint64_t guard_checks = 0;
    unsigned bad_case = ~0u, bad_x = ~0u, bad_y = ~0u, bad_channel = ~0u;
    unsigned bad_actual = 0, bad_expected = 0;
    size_t bad_offset = SIZE_MAX;
    unsigned bad_pool = ~0u;
    unsigned char *memory[POOLS] = {0};
    size_t sizes[POOLS] = {0}, offsets[VIEWS] = {0}, lengths[VIEWS] = {0};
    const unsigned view_pool[VIEWS] = {SOURCE, SOURCE, TARGET, VERTEX, MATRIX, ARGUMENTS, ARGUMENTS, ARGUMENTS};
    NVNbufferAddress addresses[VIEWS] = {0};
    NVNcommandHandle handle = 0;
    size_t control_used = 0, command_used = 0;
    unsigned char expected_commands[4096],expected_arguments[4096];
    _Alignas(16) unsigned char control[512];
    GUARDED(NVNdevice) device;
    GUARDED(NVNqueue) queue;
    GUARDED(NVNprogram) program;
    GUARDED(NVNtexture) texture;
    GUARDED(NVNcommandBuffer) command;
    GUARDED(NVNmemoryPool) pools[POOLS];
    GUARDED(NVNbuffer) buffers[VIEWS];
    NVNdeviceBuilder db; NVNqueueBuilder qb; NVNmemoryPoolBuilder pb;
    NVNbufferBuilder bb; NVNtextureBuilder tb;
    NVNvertexAttribState attributes[2]; NVNvertexStreamState stream;
    const unsigned char background[4] = {0,0,0,0};
    NVNblendState blend;NVNcolorState color;NVNpolygonState polygon;
#define ENTRY(name, type) type name = (type)nvnBootstrapLoader(#name); if (!name) error = ENOSYS;
#include "../src/nvn_entries.inc"
#undef ENTRY
    PFNNVNCOMMANDBUFFERBARRIERPROC indirect_barrier=(PFNNVNCOMMANDBUFFERBARRIERPROC)nvnBootstrapLoader("nvnCommandBufferBarrier");
    if(!indirect_barrier)error=ENOSYS;
#define CALL(expression) do { expression; if ((error = nn_nvn_error())) goto done; } while (0)
#define BOOL(expression) do { NVNboolean ok_ = (expression); int e_ = nn_nvn_error(); \
    if (!ok_ || e_) { error = e_ ? e_ : EIO; goto done; } } while (0)
#define REQUIRE(expression) do { int ok_ = !!(expression); int e_ = nn_nvn_error(); \
    if (!ok_ || e_) { error = e_ ? e_ : EINVAL; goto done; } } while (0)
#define OBJECT_GUARD(object) do { \
    if ((error = check_bytes((object).before, sizeof((object).before), 0x5a, &guard_checks)) || \
        (error = check_bytes((object).after, sizeof((object).after), 0x5a, &guard_checks))) goto save_failed; \
} while (0)
    if (!json || !capacity) return -1;
    if (error) goto done;
    nn_nvn_clear_error();
    stage = "profile";
    if (!directory || !image || sysconf(_SC_PAGESIZE) != 4096 ||
        image->chip != 0x06050002 || image->schema != 3 || image->profile != NN_GFX_COLOR ||
        image->width != 64 || image->height != 64 || image->target_pitch != 256 ||
        image->target_bytes != 16384 || image->vertex_bytes != 72 || image->matrix_bytes != 64 ||
        !nn_test_graphics.vs.source_code || !nn_test_graphics.fs.source_code ||
        !nn_test_graphics.vs.source_control || !nn_test_graphics.fs.source_control ||
        !nn_test_graphics.vs.source_bytes || !nn_test_graphics.fs.source_bytes ||
        nn_test_graphics.vs.source_bytes > 1048576 || nn_test_graphics.fs.source_bytes > 1048576) {
        error = ENOTSUP; goto done;
    }
    memset(&device, 0x5a, sizeof(device)); memset(&queue, 0x5a, sizeof(queue));
    memset(&program, 0x5a, sizeof(program)); memset(&texture, 0x5a, sizeof(texture));
    memset(&command, 0x5a, sizeof(command)); memset(pools, 0x5a, sizeof(pools));
    memset(buffers, 0x5a, sizeof(buffers)); memset(control, 0xa5, sizeof(control));
    offsets[VS] = 4096; lengths[VS] = nn_test_graphics.vs.source_bytes;
    offsets[FS] = ((offsets[VS] + lengths[VS] + 4095) & ~(size_t)4095) + 4096;
    lengths[FS] = nn_test_graphics.fs.source_bytes;
    sizes[SOURCE] = ((offsets[FS] + lengths[FS] + 4095) & ~(size_t)4095) + 4096;
    offsets[RT] = 4096; lengths[RT] = image->target_bytes; sizes[TARGET] = 4096 + lengths[RT] + 4096;
    offsets[VB] = 256; lengths[VB] = 72; sizes[VERTEX] = 4096;
    offsets[UB] = 64; lengths[UB] = 64; sizes[MATRIX] = 4096; sizes[COMMAND] = 4096;
    sizes[ARGUMENTS]=4096;offsets[IDX]=64;lengths[IDX]=6;offsets[INDIRECT]=256;lengths[INDIRECT]=64;offsets[COUNT]=512;lengths[COUNT]=4;
    stage = "device";
    CALL(nvnDeviceBuilderSetDefaults(&db)); BOOL(nvnDeviceInitialize(&device.value, &db)); device_ready = 1;
    REQUIRE(nvnDeviceGetWindowOriginMode(&device.value) == NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT);
    REQUIRE(nvnDeviceGetDepthMode(&device.value) == NVN_DEPTH_MODE_NEAR_IS_MINUS_W);
    if ((error = nn_nvn_register_graphics(&device.value, &nn_test_graphics))) goto done;
    REQUIRE(nn_nvn_error() == 0); registered = 1;
    CALL(nvnQueueBuilderSetDefaults(&qb)); CALL(nvnQueueBuilderSetDevice(&qb, &device.value));
    BOOL(nvnQueueInitialize(&queue.value, &qb)); queue_ready = 1;
    stage = "pools";
    for (unsigned i = 0; i < POOLS; ++i) {
        if ((error = posix_memalign((void **)&memory[i], 4096, sizes[i]))) goto done;
        memset(memory[i], 0xa5, sizes[i]);
        CALL(nvnMemoryPoolBuilderSetDefaults(&pb)); CALL(nvnMemoryPoolBuilderSetDevice(&pb, &device.value));
        CALL(nvnMemoryPoolBuilderSetStorage(&pb, memory[i], sizes[i]));
        CALL(nvnMemoryPoolBuilderSetFlags(&pb, NVN_MEMORY_POOL_FLAGS_CPU_CACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED |
                                         (i == SOURCE ? NVN_MEMORY_POOL_FLAGS_SHADER_CODE : 0)));
        BOOL(nvnMemoryPoolInitialize(&pools[i].value, &pb)); pool_ready[i] = 1;
        REQUIRE(nvnMemoryPoolMap(&pools[i].value) == memory[i]);
    }
    stage = "buffers";
    for (unsigned i = 0; i < VIEWS; ++i) {
        CALL(nvnBufferBuilderSetDefaults(&bb)); CALL(nvnBufferBuilderSetDevice(&bb, &device.value));
        CALL(nvnBufferBuilderSetStorage(&bb, &pools[view_pool[i]].value, offsets[i], lengths[i]));
        BOOL(nvnBufferInitialize(&buffers[i].value, &bb)); buffer_ready[i] = 1;
        REQUIRE(nvnBufferMap(&buffers[i].value) == memory[view_pool[i]] + offsets[i]);
        addresses[i] = nvnBufferGetAddress(&buffers[i].value); REQUIRE(addresses[i] != 0);
        REQUIRE(addresses[i] == nvnMemoryPoolGetBufferAddress(&pools[view_pool[i]].value) + offsets[i]);
    }
    memcpy(memory[SOURCE] + offsets[VS], nn_test_graphics.vs.source_code, lengths[VS]);
    memcpy(memory[SOURCE] + offsets[FS], nn_test_graphics.fs.source_code, lengths[FS]);
    CALL(nvnMemoryPoolFlushMappedRange(&pools[SOURCE].value, 0, sizes[SOURCE]));
    stage = "program";
    BOOL(nvnProgramInitialize(&program.value, &device.value)); program_ready = 1;
    /* Reverse stage order deliberately; identity matching must be independent of it. */
    const NVNshaderData shaders[2] = {{addresses[FS], nn_test_graphics.fs.source_control},
                                     {addresses[VS], nn_test_graphics.vs.source_control}};
    BOOL(nvnProgramSetShaders(&program.value, 2, shaders));
    stage = "texture";
    CALL(nvnTextureBuilderSetDefaults(&tb)); CALL(nvnTextureBuilderSetDevice(&tb, &device.value));
    CALL(nvnTextureBuilderSetFlags(&tb, NVN_TEXTURE_FLAGS_LINEAR));
    CALL(nvnTextureBuilderSetTarget(&tb, NVN_TEXTURE_TARGET_2D));
    CALL(nvnTextureBuilderSetSize2D(&tb, 64, 64)); CALL(nvnTextureBuilderSetLevels(&tb, 1));
    CALL(nvnTextureBuilderSetSamples(&tb, 0)); CALL(nvnTextureBuilderSetFormat(&tb, NVN_FORMAT_RGBA8));
    CALL(nvnTextureBuilderSetStride(&tb, 256));
    REQUIRE(nvnTextureBuilderGetStorageSize(&tb) == 16384);
    REQUIRE(nvnTextureBuilderGetStorageAlignment(&tb) == 64);
    REQUIRE(nvnTextureBuilderGetStride(&tb) == 256);
    REQUIRE(nvnTextureBuilderGetFlags(&tb) == NVN_TEXTURE_FLAGS_LINEAR);
    CALL(nvnTextureBuilderSetStorage(&tb, &pools[TARGET].value, offsets[RT]));
    BOOL(nvnTextureInitialize(&texture.value, &tb)); texture_ready = 1;
    REQUIRE(nvnTextureGetStorageSize(&texture.value) == 16384);
    REQUIRE(nvnTextureGetStride(&texture.value) == 256);
    REQUIRE(nvnTextureGetFlags(&texture.value) == NVN_TEXTURE_FLAGS_LINEAR);
    REQUIRE(nvnTextureGetTarget(&texture.value) == NVN_TEXTURE_TARGET_2D);
    REQUIRE(nvnTextureGetFormat(&texture.value) == NVN_FORMAT_RGBA8);
    REQUIRE(nvnTextureGetWidth(&texture.value) == 64); REQUIRE(nvnTextureGetHeight(&texture.value) == 64);
    REQUIRE(nvnTextureGetDepth(&texture.value) == 1); REQUIRE(nvnTextureGetLevels(&texture.value) == 1);
    REQUIRE(nvnTextureGetSamples(&texture.value) == 0);
    REQUIRE(nvnTextureGetTextureAddress(&texture.value) == addresses[RT]);
    REQUIRE(nvnTextureGetViewOffset(&texture.value, NULL) == 0);

    for(unsigned mode=0;mode<4;mode++) {
        const int counted=mode==1||mode==2;
        stage="record";
        memset(memory[COMMAND],0xa5,sizes[COMMAND]);memset(memory[ARGUMENTS],0xa5,sizes[ARGUMENTS]);memset(control,0xa5,sizeof(control));
        const uint16_t indices[3]={0,2,1};memcpy(memory[ARGUMENTS]+offsets[IDX],indices,sizeof(indices));
        uint32_t args[5]={3,1,0,0,0};
        if(counted)for(unsigned i=0;i<2;i++)memcpy(memory[ARGUMENTS]+offsets[INDIRECT]+i*32,args,mode==2?20:16);
        uint32_t zero=0;memcpy(memory[ARGUMENTS]+offsets[COUNT],&zero,4);
        memcpy(expected_arguments,memory[ARGUMENTS],sizeof(expected_arguments));
        BOOL(nvnCommandBufferInitialize(&command.value,&device.value));command_ready=1;
        CALL(nvnCommandBufferAddCommandMemory(&command.value,&pools[COMMAND].value,0,sizes[COMMAND]));
        CALL(nvnCommandBufferAddControlMemory(&command.value,control,sizeof(control)));
        CALL(nvnCommandBufferBeginRecording(&command.value));
        CALL(nvnCommandBufferBindProgram(&command.value,&program.value,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
        CALL(nvnCommandBufferBindUniformBuffer(&command.value,NVN_SHADER_STAGE_VERTEX,0,addresses[UB],lengths[UB]));
        for(unsigned i=0;i<2;i++){
            CALL(nvnVertexAttribStateSetDefaults(attributes+i));
            CALL(nvnVertexAttribStateSetFormat(attributes+i,i?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,i?8:0));
            CALL(nvnVertexAttribStateSetStreamIndex(attributes+i,0));
        }
        CALL(nvnVertexStreamStateSetDefaults(&stream));CALL(nvnVertexStreamStateSetStride(&stream,24));CALL(nvnVertexStreamStateSetDivisor(&stream,0));
        CALL(nvnCommandBufferBindVertexAttribState(&command.value,2,attributes));CALL(nvnCommandBufferBindVertexStreamState(&command.value,1,&stream));
        memset(attributes,0,sizeof(attributes));memset(&stream,0,sizeof(stream));
        CALL(nvnCommandBufferBindVertexBuffer(&command.value,0,addresses[VB],lengths[VB]));
        const NVNtexture *targets[1]={&texture.value};
        CALL(nvnCommandBufferSetRenderTargets(&command.value,1,targets,NULL,NULL,NULL));
        CALL(nvnCommandBufferSetViewport(&command.value,0,0,64,64));CALL(nvnCommandBufferSetScissor(&command.value,0,0,64,64));CALL(nvnCommandBufferSetDepthRange(&command.value,0,1));
        CALL(nvnBlendStateSetDefaults(&blend));CALL(nvnBlendStateSetBlendTarget(&blend,0));
        CALL(nvnBlendStateSetBlendFunc(&blend,NVN_BLEND_FUNC_ONE,NVN_BLEND_FUNC_ONE,NVN_BLEND_FUNC_ONE,NVN_BLEND_FUNC_ONE));
        CALL(nvnColorStateSetDefaults(&color));CALL(nvnColorStateSetBlendEnable(&color,0,1));
        CALL(nvnCommandBufferBindBlendState(&command.value,&blend));CALL(nvnCommandBufferBindColorState(&command.value,&color));
        memset(&blend,0,sizeof(blend));memset(&color,0,sizeof(color));
        /* Index-data consumption is qualified by regular -> degenerate
         * indices on the same handle, independent of unqualified cull polarity. */
        CALL(nvnPolygonStateSetDefaults(&polygon));
        CALL(nvnCommandBufferBindPolygonState(&command.value,&polygon));memset(&polygon,0,sizeof(polygon));
        if(counted){
            REQUIRE(!memcmp(memory[ARGUMENTS]+offsets[COUNT],&zero,4));++count_zero_checks;
            CALL(nvnCommandBufferClearBuffer(&command.value,addresses[COUNT],4,1));
            CALL(indirect_barrier(&command.value,NVN_BARRIER_ORDER_INDIRECT_DATA_BIT));++indirect_barriers;
            if(mode==1)CALL(nvnCommandBufferMultiDrawArraysIndirectCount(&command.value,NVN_DRAW_PRIMITIVE_TRIANGLES,addresses[INDIRECT],addresses[COUNT],2,32));
            else CALL(nvnCommandBufferMultiDrawElementsIndirectCount(&command.value,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,addresses[IDX],addresses[INDIRECT],addresses[COUNT],2,32));
        }else if(mode==0)CALL(nvnCommandBufferDrawElementsInstanced(&command.value,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,3,addresses[IDX],0,0,2));
        else CALL(nvnCommandBufferDrawArraysInstanced(&command.value,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3,0,2));
        REQUIRE(!memcmp(memory[ARGUMENTS]+offsets[COUNT],&zero,4));if(counted)++count_zero_checks;
        handle=nvnCommandBufferEndRecording(&command.value);REQUIRE(handle);++handles_recorded;
        command_used=nvnCommandBufferGetCommandMemoryUsed(&command.value);REQUIRE(command_used&&command_used<=sizes[COMMAND]);
        control_used=nvnCommandBufferGetControlMemoryUsed(&command.value);REQUIRE(control_used&&control_used<=sizeof(control));
        memcpy(expected_commands,memory[COMMAND],sizeof(expected_commands));
        CALL(nvnCommandBufferFinalize(&command.value));command_ready=0;
        for(unsigned repeat=0;repeat<2;repeat++){
            unsigned test=mode*2+repeat;
            float matrix[16]={0};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;if(repeat)matrix[3]=1;
            float vertices[3][6]={{-1,-1,0,0,0,0},{3,-1,0,0,0,0},{-1,3,0,0,0,0}};
            float component=repeat?.125f:.25f;
            for(unsigned v=0;v<3;v++){
                if(mode<3)vertices[v][2+mode]=component;
                else vertices[v][2]=vertices[v][3]=vertices[v][4]=component;
                vertices[v][5]=component;
            }
            unsigned char *target=memory[TARGET]+offsets[RT];
            memset(memory[TARGET],0xa5,sizes[TARGET]);memset(memory[VERTEX],0xa5,sizes[VERTEX]);memset(memory[MATRIX],0xa5,sizes[MATRIX]);
            for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)memcpy(target+y*256+x*4,background,4);
            memcpy(memory[VERTEX]+offsets[VB],vertices,sizeof(vertices));memcpy(memory[MATRIX]+offsets[UB],matrix,sizeof(matrix));
            if(repeat&&(mode==0||mode==2)){
                const uint16_t degenerate[3]={0,0,0};
                memcpy(memory[ARGUMENTS]+offsets[IDX],degenerate,sizeof(degenerate));
                memcpy(expected_arguments+offsets[IDX],degenerate,sizeof(degenerate));
            }
            /* Second replay changes indirect instanceCount after recording.
             * A stale CPU snapshot would paint half the expected intensity. */
            if(counted){args[1]=repeat?2:1;for(unsigned i=0;i<2;i++){
                memcpy(memory[ARGUMENTS]+offsets[INDIRECT]+i*32,args,mode==2?20:16);
                memcpy(expected_arguments+offsets[INDIRECT]+i*32,args,mode==2?20:16);
            }}
            memcpy(memory[ARGUMENTS]+offsets[COUNT],&zero,4);memcpy(expected_arguments+offsets[COUNT],&zero,4);
            REQUIRE(!memcmp(memory[ARGUMENTS]+offsets[COUNT],&zero,4));if(counted)++count_zero_checks;
            for(unsigned i=0;i<POOLS;i++)CALL(nvnMemoryPoolFlushMappedRange(&pools[i].value,0,sizes[i]));
            stage="submit";submission="unknown";pending=1;
            CALL(nvnQueueSubmitCommands(&queue.value,1,&handle));submission="accepted";++submissions;if(repeat)++replayed;
            stage="wait";CALL(nvnQueueFinish(&queue.value));pending=0;
            for(unsigned i=0;i<POOLS;i++)CALL(nvnMemoryPoolInvalidateMappedRange(&pools[i].value,0,sizes[i]));
            if(counted){uint32_t one=1;REQUIRE(!memcmp(memory[ARGUMENTS]+offsets[COUNT],&one,4));++count_readback_checks;++gpu_count_writes;if(repeat&&mode==1)++argument_updates;memcpy(expected_arguments+offsets[COUNT],&one,4);}
            stage="pixels";
            for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
                unsigned expected[4];draw_expected(mode,repeat,x,expected);
                for(unsigned c=0;c<4;c++){
                    unsigned actual=target[y*256+x*4+c],delta=actual>expected[c]?actual-expected[c]:expected[c]-actual;
                    /* RGBA8 blending rounds on each instance; 2*0.125/0.25
                     * differs from one final ideal quantization by at most1. */
                    if(delta>(expected[c]?1u:0u)){bad_case=test;bad_x=x;bad_y=y;bad_channel=c;bad_actual=actual;bad_expected=expected[c];error=EIO;goto save_failed;}
                    ++channel_checks;
                }
            }
            stage="guards";
            for(unsigned i=0;i<POOLS;i++)for(size_t byte=0;byte<sizes[i];byte++){
                unsigned expected=0xa5;int readonly=0;
                if(i==COMMAND){expected=expected_commands[byte];readonly=1;}
                else if(i==ARGUMENTS){expected=expected_arguments[byte];readonly=byte<offsets[COUNT]||byte>=offsets[COUNT]+4;}
                else if(i==SOURCE){readonly=1;if(byte>=offsets[VS]&&byte-offsets[VS]<lengths[VS])expected=((const unsigned char*)nn_test_graphics.vs.source_code)[byte-offsets[VS]];if(byte>=offsets[FS]&&byte-offsets[FS]<lengths[FS])expected=((const unsigned char*)nn_test_graphics.fs.source_code)[byte-offsets[FS]];}
                else if(i==TARGET&&byte>=offsets[RT]&&byte-offsets[RT]<lengths[RT])continue;
                else if(i==VERTEX&&byte>=offsets[VB]&&byte-offsets[VB]<sizeof(vertices)){expected=((const unsigned char*)vertices)[byte-offsets[VB]];readonly=1;}
                else if(i==MATRIX&&byte>=offsets[UB]&&byte-offsets[UB]<sizeof(matrix)){expected=((const unsigned char*)matrix)[byte-offsets[UB]];readonly=1;}
                if(memory[i][byte]!=expected){bad_case=test;bad_pool=i;bad_offset=byte;bad_actual=memory[i][byte];bad_expected=expected;error=EIO;goto save_failed;}
                ++guard_checks;if(readonly)++readonly_checks;
            }
            OBJECT_GUARD(device);OBJECT_GUARD(queue);OBJECT_GUARD(program);OBJECT_GUARD(texture);OBJECT_GUARD(command);
            for(unsigned i=0;i<POOLS;i++){OBJECT_GUARD(pools[i]);}for(unsigned i=0;i<VIEWS;i++){OBJECT_GUARD(buffers[i]);}
            if((error=check_bytes(control+control_used,sizeof(control)-control_used,0xa5,&guard_checks)))goto save_failed;
save_failed:;
            char path[1024];int bytes=snprintf(path,sizeof(path),"%s/draw-mode-%u-replay-%u.rgba",directory,mode,repeat);
            if(bytes<0||(size_t)bytes>=sizeof(path)){if(!error)error=ENAMETOOLONG;goto done;}
            FILE *file=fopen(path,"wb");if(!file){if(!error)error=errno;goto done;}int we=fwrite(target,1,image->target_bytes,file)!=image->target_bytes;int ce=fclose(file);if(we||ce){if(!error)error=EIO;goto done;}
            if(error)goto done;
            ++completed;
            if(mode==0||mode==2){if(repeat)++degenerate_cases;else ++nondegenerate_cases;}
        }
        CALL(nvnDeviceFinalizeCommandHandle(&device.value,handle));handle=0;++handles_released;
    }
    stage="complete";
done:
    if(pending)cleanup=EBUSY;
    else{
#define CLEAN(x) do{nn_nvn_clear_error();x;int e_=nn_nvn_error();if(e_&&!cleanup)cleanup=e_;}while(0)
        if(command_ready)CLEAN(nvnCommandBufferFinalize(&command.value));
        if(handle){CLEAN(nvnDeviceFinalizeCommandHandle(&device.value,handle));if(!cleanup)++handles_released;}
        if(texture_ready)CLEAN(nvnTextureFinalize(&texture.value));
        if(program_ready)CLEAN(nvnProgramFinalize(&program.value));
        for(unsigned i=0;i<VIEWS;i++)if(buffer_ready[i])CLEAN(nvnBufferFinalize(&buffers[i].value));
        for(unsigned i=0;i<POOLS;i++)if(pool_ready[i])CLEAN(nvnMemoryPoolFinalize(&pools[i].value));
        if(queue_ready)CLEAN(nvnQueueFinalize(&queue.value));
        if(device_ready)CLEAN(nvnDeviceFinalize(&device.value));
        if(!cleanup)for(unsigned i=0;i<POOLS;i++)free(memory[i]);
#undef CLEAN
    }
    int passed=!error&&!cleanup&&completed==8&&submissions==8&&channel_checks==131072&&handles_recorded==4&&handles_released==4&&replayed==4&&gpu_count_writes==4&&count_zero_checks==8&&count_readback_checks==4&&indirect_barriers==2&&argument_updates==1&&degenerate_cases==2&&nondegenerate_cases==2&&guard_checks&&readonly_checks;
    int n=snprintf(json,capacity,
        "{\"schema\":1,\"operation\":\"nvn_graphics_draw_batch\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,"
        "\"chip_id\":%u,\"gmem_bytes\":%u,\"device_matches_artifact\":%s,\"submission\":\"%s\",\"completed_cases\":%u,\"submissions\":%u,"
        "\"mode_count\":4,\"handles_recorded\":%u,\"handles_released\":%u,\"handle_replays\":%u,\"gpu_count_writes\":%u,\"count_zero_checks\":%u,\"count_readback_checks\":%u,\"indirect_barriers_recorded\":%u,\"indirect_argument_update_cases\":%u,\"degenerate_index_cases\":%u,\"nondegenerate_index_cases\":%u,"
        "\"reused_ib\":%s,\"resources_retained\":%s,\"width\":64,\"height\":64,\"pitch\":256,\"target_bytes\":16384,\"channel_checks\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,"
        "\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,\"bad_pool\":%u,\"bad_offset\":%zu,\"bad_actual\":%u,\"bad_expected\":%u,\"passed\":%s}\n",
        (unsigned)getuid(),stage,error,cleanup,registered?image->chip:0,registered?image->gmem_bytes:0,registered?"true":"false",submission,completed,submissions,
        handles_recorded,handles_released,replayed,gpu_count_writes,count_zero_checks,count_readback_checks,indirect_barriers,argument_updates,degenerate_cases,nondegenerate_cases,replayed==4?"true":"false",cleanup?"true":"false",channel_checks,(unsigned long long)guard_checks,(unsigned long long)readonly_checks,
        bad_case,bad_x,bad_y,bad_channel,bad_pool,bad_offset,bad_actual,bad_expected,passed?"true":"false");
#undef OBJECT_GUARD
#undef REQUIRE
#undef BOOL
#undef CALL
    return passed&&n>=0&&(size_t)n<capacity?0:-1;
}
#undef GUARDED
