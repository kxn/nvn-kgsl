/* SPDX-License-Identifier: MIT
 * HOST RAM ONLY. Actual encoders/recorder; no packets execute and no original
 * shader bytes are in this source. Verified generated images are external. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../../src/nvn.c"
#include NN_DRAW_IMAGE_HEADER
#include NN_COMPUTE_IMAGE_HEADER
static unsigned checks;
#define T(x) do { checks++; if (!(x)) { fprintf(stderr,"line%d: %s errno%d\n",__LINE__,#x,nn_nvn_error()); abort(); } } while (0)
static uint64_t address(const uint32_t *w) { return w[0]|((uint64_t)w[1]<<32); }
static size_t packet(const uint32_t *words,size_t bytes,uint32_t header)
{
    size_t found=SIZE_MAX;
    for (size_t i=0;i<bytes/4;) {
        size_t n=nvn_pm4_packet_words(words[i]); T(n && n<=bytes/4-i);
        if (words[i]==header) { T(found==SIZE_MAX); found=i; }
        i+=n;
    }
    T(found!=SIZE_MAX); return found;
}
static void recorder(const struct nn_gfx_image *image,const struct nn_cs_image *compute)
{
    NVNdeviceBuilder db={0}; NVNdevice dk={0}; NVNcommandBuffer ck={0}; NVNprogram pk={0},cskey={0}; NVNtexture tk={0};
    NVNmemoryPool keys[7]={0}; _Alignas(16) unsigned char control[512];
    api_nvnDeviceBuilderSetDefaults(&db);T(api_nvnDeviceInitialize(&dk,&db));
    struct object *dev=find(&dk,DEVICE),*p[7];
    for (unsigned i=0;i<7;i++) {
        p[i]=create(keys+i,POOL);T(p[i]);p[i]->device=dev;
        p[i]->size=i==3 ? 16384 : 4096;p[i]->flags=0x24;dev->refs++;
        T(!nn_bo_alloc(dev->gpu,p[i]->size,&p[i]->bo));p[i]->storage=nn_bo_map(p[i]->bo);
    }
    T(api_nvnProgramInitialize(&pk,&dk));struct object *pr=find(&pk,PROGRAM);
    const struct nn_nvn_compiled_graphics graphics={.image=image};pr->graphics=&graphics;
    T(!nn_bo_alloc(dev->gpu,image->vs_code_bytes,&pr->bo));T(!nn_bo_alloc(dev->gpu,image->fs_code_bytes,&pr->fs_bo));
    struct object *target=create(&tk,TEXTURE);T(target);target->device=dev;target->pool=p[3];target->bo=p[3]->bo;
    target->size=16384;target->texture=(struct texture_desc){.flags=NVN_TEXTURE_FLAGS_LINEAR,
        .target=NVN_TEXTURE_TARGET_2D,.width=64,.height=64,.depth=1,.levels=1,
        .format=NVN_FORMAT_RGBA8,.stride=256,.set=TEX_COMPLETE};p[3]->refs++;
    T(api_nvnCommandBufferInitialize(&ck,&dk));
    PFNNVNCOMMANDBUFFERSETSHADERSCRATCHMEMORYPROC scratch=(PFNNVNCOMMANDBUFFERSETSHADERSCRATCHMEMORYPROC)nvnBootstrapLoader("nvnCommandBufferSetShaderScratchMemory");T(scratch);
    scratch(&ck,keys+1,16,32);T(!nn_nvn_error() && p[1]->refs==1);
    scratch(&ck,NULL,0,1);T(nn_nvn_error()==EINVAL && p[1]->refs==1);nn_nvn_clear_error();
    scratch(&ck,NULL,0,0);T(!nn_nvn_error() && !p[1]->refs);
    api_nvnCommandBufferAddCommandMemory(&ck,p[0]->key,0,4096);
    api_nvnCommandBufferAddControlMemory(&ck,control,sizeof(control));api_nvnCommandBufferBeginRecording(&ck);
    struct recording *r=find(&ck,COMMAND)->record;
    r->graphics_program=pr;pr->refs++;r->target=target;target->refs++;
    api_nvnCommandBufferBindVertexBuffer(&ck,0,nn_bo_address(p[1]->bo),4096);T(!nn_nvn_error());
    r->graphics_ubos[0][0]=(struct bound){p[2],0,64,nn_bo_address(p[2]->bo)};p[2]->refs++;
    NVNvertexAttribState attributes[3];NVNvertexStreamState stream;
    for(unsigned i=0;i<3;i++){api_nvnVertexAttribStateSetDefaults(attributes+i);T(!nn_nvn_error());}
    api_nvnVertexAttribStateSetFormat(attributes,NVN_FORMAT_RG32F,0);T(!nn_nvn_error());
    api_nvnVertexAttribStateSetFormat(attributes+1,NVN_FORMAT_RGBA32F,8);T(!nn_nvn_error());
    api_nvnVertexAttribStateSetFormat(attributes+2,NVN_FORMAT_RGBA32F,16);T(!nn_nvn_error());
    api_nvnVertexStreamStateSetDefaults(&stream);T(!nn_nvn_error());
    api_nvnVertexStreamStateSetStride(&stream,24);T(!nn_nvn_error());
    api_nvnCommandBufferBindVertexAttribState(&ck,2,attributes);T(!nn_nvn_error());
    api_nvnCommandBufferBindVertexStreamState(&ck,1,&stream);T(!nn_nvn_error());
    api_nvnCommandBufferSetViewport(&ck,0,0,64,64);T(!nn_nvn_error());
    api_nvnCommandBufferSetScissor(&ck,0,0,64,64);T(!nn_nvn_error());
    api_nvnCommandBufferSetDepthRange(&ck,0,1);T(!nn_nvn_error());
    T(!nn_nvn_error());
    PFNNVNCOMMANDBUFFERBINDIMAGESPROC bind_images=(PFNNVNCOMMANDBUFFERBINDIMAGESPROC)nvnBootstrapLoader("nvnCommandBufferBindImages");T(bind_images);
    NVNimageHandle image_handles[2]={257,258};
    bind_images(&ck,NVN_SHADER_STAGE_COMPUTE,4,2,image_handles);T(!nn_nvn_error());
    image_handles[0]=500;image_handles[1]=UINT64_C(1)<<40;
    bind_images(&ck,NVN_SHADER_STAGE_COMPUTE,4,2,image_handles);T(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
    T(r->images[NVN_SHADER_STAGE_COMPUTE][4]==257 && r->images[NVN_SHADER_STAGE_COMPUTE][5]==258);
    bind_images(&ck,NVN_SHADER_STAGE_FRAGMENT,32,0,NULL);T(!nn_nvn_error());
    PFNNVNCOMMANDBUFFERDRAWELEMENTSINSTANCEDPROC indexed=(PFNNVNCOMMANDBUFFERDRAWELEMENTSINSTANCEDPROC)nvnBootstrapLoader("nvnCommandBufferDrawElementsInstanced");T(indexed);
    PFNNVNCOMMANDBUFFERMULTIDRAWARRAYSINDIRECTCOUNTPROC arrays=(PFNNVNCOMMANDBUFFERMULTIDRAWARRAYSINDIRECTCOUNTPROC)nvnBootstrapLoader("nvnCommandBufferMultiDrawArraysIndirectCount");T(arrays);
    PFNNVNCOMMANDBUFFERMULTIDRAWELEMENTSINDIRECTCOUNTPROC elements=(PFNNVNCOMMANDBUFFERMULTIDRAWELEMENTSINDIRECTCOUNTPROC)nvnBootstrapLoader("nvnCommandBufferMultiDrawElementsIndirectCount");T(elements);
    indexed(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,6,nn_bo_address(p[4]->bo),-1,2,3);T(!nn_nvn_error());
    arrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,nn_bo_address(p[5]->bo),nn_bo_address(p[6]->bo),4,16);T(!nn_nvn_error());
    elements(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_INT,nn_bo_address(p[4]->bo),nn_bo_address(p[5]->bo),nn_bo_address(p[6]->bo),4,20);T(!nn_nvn_error());
    unsigned jobs=0;for (struct job *j=r->current->jobs;j;j=j->next) jobs++;T(jobs==3);
    T(p[4]->refs && p[5]->refs && p[6]->refs);size_t used=r->used;
    arrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,nn_bo_address(p[5]->bo),nn_bo_address(p[6]->bo),0,16);T(!nn_nvn_error() && r->used==used);
    indexed(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_INT,2000,nn_bo_address(p[4]->bo),0,0,1);T(nn_nvn_error()==EINVAL && r->used==used);nn_nvn_clear_error();
    T(api_nvnProgramInitialize(&cskey,&dk));struct object *cs=find(&cskey,PROGRAM);
    const struct nn_nvn_cbuf cbuf={.slot=6,.binding=3};
    const struct nn_nvn_compiled_cs compiled={.image=compute,.cbufs=&cbuf,.cbuf_count=1};
    cs->compiled=&compiled;T(!nn_bo_alloc(dev->gpu,compute->code_bytes,&cs->bo));
    r->program=cs;cs->refs++;r->ubos[3]=(struct bound){p[2],0,4096,nn_bo_address(p[2]->bo)};p[2]->refs++;
    PFNNVNCOMMANDBUFFERDISPATCHCOMPUTEINDIRECTPROC dispatch=(PFNNVNCOMMANDBUFFERDISPATCHCOMPUTEINDIRECTPROC)nvnBootstrapLoader("nvnCommandBufferDispatchComputeIndirect");T(dispatch);
    dispatch(&ck,nn_bo_address(p[5]->bo));T(!nn_nvn_error());
    T(r->used==used+16);jobs=0;for (struct job *j=r->current->jobs;j;j=j->next) jobs++;T(jobs==4);
    NVNcommandHandle h=api_nvnCommandBufferEndRecording(&ck);T(h && !nn_nvn_error());
    api_nvnMemoryPoolFinalize(keys+4);T(nn_nvn_error()==EBUSY);nn_nvn_clear_error();
    api_nvnDeviceFinalizeCommandHandle(&dk,h);T(!nn_nvn_error());
    T(!p[4]->refs && !p[5]->refs && !p[6]->refs);
    api_nvnCommandBufferFinalize(&ck);api_nvnProgramFinalize(&pk);api_nvnProgramFinalize(&cskey);api_nvnTextureFinalize(&tk);T(!nn_nvn_error());
    for (unsigned i=0;i<7;i++) api_nvnMemoryPoolFinalize(keys+i);
    api_nvnDeviceFinalize(&dk);T(!nn_nvn_error());
}
int main(void)
{
    nn_device *device=NULL; T(!nn_device_open(&device));
    const struct nn_gfx_image *image=&NVN_GRAPHICS_IMAGE;
    const struct nn_cs_image *compute=&NVN_COMPILED_IMAGE;
    nn_bo *bos[8]={0};
    for (unsigned i=0;i<8;i++) T(!nn_bo_alloc(device,i==2 ? image->target_bytes : 4096,bos+i));
    struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT]={0};
    buffers[NN_GFX_VS_CODE]=(struct nn_gfx_buffer){bos[0],0,image->vs_code_bytes};
    buffers[NN_GFX_FS_CODE]=(struct nn_gfx_buffer){bos[1],0,image->fs_code_bytes};
    buffers[NN_GFX_TARGET]=(struct nn_gfx_buffer){bos[2],0,image->target_bytes};
    buffers[NN_GFX_VERTICES]=(struct nn_gfx_buffer){bos[3],0,4096};
    buffers[NN_GFX_MATRIX]=(struct nn_gfx_buffer){bos[4],0,64};
    struct nn_gfx_draw_state state={.window_origin=0,.depth_mode=0};
    struct nn_gfx_draw draw={.mode=NN_GFX_DRAW_DIRECT,.primitive=4,.count=6,.instances=2,.first=1,.base_instance=5};
    /* Argument buffers may be GPU-only or written by previous GPU commands.
     * A CPU read in either encoder must fault this HOST test. */
    T(!mprotect(nn_bo_map(bos[5]),nn_bo_size(bos[5]),PROT_NONE));
    T(!mprotect(nn_bo_map(bos[6]),nn_bo_size(bos[6]),PROT_NONE));
    T(!mprotect(nn_bo_map(bos[7]),nn_bo_size(bos[7]),PROT_NONE));
    for (unsigned indexed=0;indexed<2;indexed++) for (unsigned mode=0;mode<3;mode++) {
        draw.indexed=indexed; draw.index_type=1; draw.mode=mode; draw.base_vertex=-2;
        draw.index=indexed ? (struct nn_gfx_buffer){bos[5],16,128} : (struct nn_gfx_buffer){0};
        draw.indirect=mode ? (struct nn_gfx_buffer){bos[6],32,128} : (struct nn_gfx_buffer){0};
        draw.indirect_count=mode==2 ? (struct nn_gfx_buffer){bos[7],64,4} : (struct nn_gfx_buffer){0};
        draw.max_draws=4; draw.stride=indexed ? 20 : 16;
        nn_bo *job=NULL; size_t bytes=0;
        T(!nn_gfx_encode_draw(device,image,buffers,&state,&draw,&job,&bytes)); T(job && bytes>image->command_words*4);
        const uint32_t *w=nn_bo_map(job);
        size_t p=packet(w,bytes,nvn_pm4_packet7(mode ? 0x2a : 0x38,mode ? (indexed ? (mode==2 ? 11 : 9) : (mode==2 ? 8 : 6)) : (indexed ? 7 : 3)));
        T(p>=3 && w[p-3]==nvn_pm4_packet4(0xa00e,2));
        T(w[p-2]==(indexed ? (uint32_t)-2 : 1)); T(w[p-1]==5);
        T(w[p+1]==(indexed ? (4u|256u|1024u) : (4u|256u|128u)));
        if (!mode) {
            T(w[p+2]==2 && w[p+3]==6);
            if (indexed) { T(!w[p+4]); T(address(w+p+5)==nn_bo_address(bos[5])+16); T(w[p+7]==64); }
        } else {
            T(w[p+2]==(indexed ? (mode==2 ? 7u : 4u) : (mode==2 ? 6u : 2u))); T(w[p+3]==4);
            size_t a=p+4;
            if (indexed) { T(address(w+a)==nn_bo_address(bos[5])+16); T(w[a+2]==64); a+=3; }
            T(address(w+a)==nn_bo_address(bos[6])+32); a+=2;
            if (mode==2) { T(address(w+a)==nn_bo_address(bos[7])+64); a+=2; }
            T(w[a]==draw.stride);
        }
        for (unsigned i=0;i<image->state_patch_count;i++) {
            const struct nn_gfx_state_patch *patch=image->state_patches+i;
            T(w[patch->word]==((patch->expected&~patch->mask)|(patch->value[0]&patch->mask)));
        }
        T(!nn_bo_destroy(job));
    }
    nn_bo *job=NULL;size_t bytes=99;
    draw.stride=19;T(nn_gfx_encode_draw(device,image,buffers,&state,&draw,&job,&bytes)==EINVAL); T(!job && !bytes);
    draw.stride=20;draw.max_draws=8;T(nn_gfx_encode_draw(device,image,buffers,&state,&draw,&job,&bytes)==EINVAL);T(!job && !bytes);
    struct nn_cs_buffer ubos[16]={{0}},ssbos[16]={{0}};
    for (unsigned i=0;i<16;i++) { ubos[i]=(struct nn_cs_buffer){nn_bo_address(bos[3]),4096}; ssbos[i]=ubos[i]; }
    T(!nn_cs_encode_indirect(device,bos[0],compute,ubos,ssbos,nn_bo_address(bos[6])+32,&job,&bytes));
    const uint32_t *w=nn_bo_map(job);
    size_t p=packet(w,bytes,nvn_pm4_packet7(0x41,4));
    T(!w[p+1]);T(address(w+p+2)==nn_bo_address(bos[6])+32);
    T(w[p+4]==((compute->local[0]-1)<<2| (compute->local[1]-1)<<12 | (compute->local[2]-1)<<22));
    unsigned copies=0,loads=0;
    for (size_t i=0;i<bytes/4;) {
        if (w[i]==nvn_pm4_packet7(0x73,5)) {
            T(copies<3); T(!w[i+1]);T(address(w+i+2)==nn_bo_address(job)+7168+copies*4);
            T(address(w+i+4)==nn_bo_address(bos[6])+32+copies*4);copies++;
        }
        if (w[i]==nvn_pm4_packet7(0x34,3) && address(w+i+2)==nn_bo_address(job)+7168) {
            T(((w[i+1]>>16)&3)==2 && ((w[i+1]>>22)&1023)==1); loads++;
        }
        i+=nvn_pm4_packet_words(w[i]);
    }
    T(copies==3 && loads==1);T(((const uint32_t *)((const char *)w+7168))[3]==3);
    T(!nn_bo_destroy(job));
    job=NULL;T(nn_cs_encode_indirect(device,bos[0],compute,ubos,ssbos,3,&job,&bytes)==EINVAL);T(!job && !bytes);
    for (unsigned i=5;i<8;i++) T(!mprotect(nn_bo_map(bos[i]),nn_bo_size(bos[i]),PROT_READ|PROT_WRITE));
    for (unsigned i=0;i<8;i++) T(!nn_bo_destroy(bos[i]));
    T(!nn_device_close(device));
    recorder(image,compute);
    puts("HOST encoder packets, argument buffers CPU-inaccessible; no GPU execution");
    printf("{\"checks\":%u,\"gpu_submits\":0,\"draw_modes\":6,\"compute_indirect\":1}\n",checks);
}
