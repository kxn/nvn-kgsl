/* SPDX-License-Identifier: MIT */
/* HOST ONLY. Intended tracked location: tests/d2r_bridge/device_modes.c.
 * Shader artifact data is supplied by an ignored, verified generated header;
 * no original or generated shader bytes belong in this source file. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef NN_TEST_NVN_SOURCE
#define NN_TEST_NVN_SOURCE "../../src/nvn.c"
#endif
#include NN_TEST_NVN_SOURCE
#include NN_MODE_IMAGE_HEADER
/* Focused HOST ONLY fixture: program native-code cache and already validated
 * bound resources are injected, to test the REAL Draw recorder/materializer.
 * This does not qualify source registration, GPU execution or pixels. */
static unsigned checks;
#define T(x) do { ++checks; if(!(x)) { fprintf(stderr,"line%d %s err%d\n",__LINE__,#x,nn_nvn_error()); abort(); } }while(0)
#define OK(x) do { x; T(nn_nvn_error()==0); }while(0)
#define SETUP_STATE(x) do { x; assert(nn_nvn_error()==0); }while(0)
#define ERR(x,e) do { x; T(nn_nvn_error()==(e)); nn_nvn_clear_error(); }while(0)
static struct object *pool(struct object *dev,const void *key,size_t size)
{
    struct object *p=create(key,POOL);T(p);T(!nn_bo_alloc(dev->gpu,size,&p->bo));
    p->device=dev;p->size=size;p->storage=nn_bo_map(p->bo);
    p->flags=NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED;dev->refs++;
    return p;
}
static struct object *texture(struct object *dev,const void *key,struct object *p,unsigned dim)
{
    struct object *t=create(key,TEXTURE);T(t);t->device=dev;t->pool=p;t->bo=p->bo;
    t->size=dim==64?16384:4096;t->texture=(struct texture_desc){
        .flags=NVN_TEXTURE_FLAGS_LINEAR,.target=NVN_TEXTURE_TARGET_2D,.width=dim,.height=dim,
        .depth=1,.levels=1,.samples=0,.format=NVN_FORMAT_RGBA8,.stride=256,.set=TEX_COMPLETE};
    p->refs++;return t;
}
static void encoded(nn_bo *bo,const struct nn_gfx_image *image,unsigned origin,unsigned depth)
{
    const uint32_t *words=nn_bo_map(bo);
    for(unsigned i=0;i<image->state_patch_count;i++) {
        const struct nn_gfx_state_patch *p=image->state_patches+i;
        unsigned mode=p->kind==NN_GFX_STATE_VIEWPORT_YSCALE?origin:depth;
        uint32_t want=(p->expected&~p->mask)|(p->value[mode]&p->mask);
        T(words[p->word]==want);
    }
}
int main(void)
{
    NVNdeviceBuilder db={0};NVNdevice dk={0},uninit={0};NVNcommandBuffer ck={0};NVNprogram pk={0};
    NVNtexture rtkey={0},txkey={0};NVNsampler skey={0};NVNtexturePool tpkey={0};NVNsamplerPool spkey={0};
    unsigned char keys[8];_Alignas(16) unsigned char control[512];
    PFNNVNDEVICESETWINDOWORIGINMODEPROC seto=(PFNNVNDEVICESETWINDOWORIGINMODEPROC)nvnBootstrapLoader("nvnDeviceSetWindowOriginMode");
    PFNNVNDEVICESETDEPTHMODEPROC setd=(PFNNVNDEVICESETDEPTHMODEPROC)nvnBootstrapLoader("nvnDeviceSetDepthMode");
    PFNNVNDEVICEGETWINDOWORIGINMODEPROC geto=(PFNNVNDEVICEGETWINDOWORIGINMODEPROC)nvnBootstrapLoader("nvnDeviceGetWindowOriginMode");
    PFNNVNDEVICEGETDEPTHMODEPROC getd=(PFNNVNDEVICEGETDEPTHMODEPROC)nvnBootstrapLoader("nvnDeviceGetDepthMode");
    T(seto&&setd&&geto&&getd);
    ERR(seto(&uninit,NVN_WINDOW_ORIGIN_MODE_UPPER_LEFT),EINVAL);
    OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));
    struct object *dev=find(&dk,DEVICE);T(dev);T(geto(&dk)==0&&getd(&dk)==0);
    OK(seto(&dk,NVN_WINDOW_ORIGIN_MODE_UPPER_LEFT));OK(setd(&dk,NVN_DEPTH_MODE_NEAR_IS_ZERO));
    T(geto(&dk)==1&&getd(&dk)==1);
    ERR(seto(&dk,(NVNwindowOriginMode)2),ENOTSUP);T(geto(&dk)==1);
    ERR(setd(&dk,(NVNdepthMode)-1),ENOTSUP);T(getd(&dk)==1);
    ERR(seto(NULL,NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT),EINVAL);
    ERR((void)getd((const NVNdevice *)&db),EINVAL);
    OK(seto(&dk,NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT));OK(setd(&dk,NVN_DEPTH_MODE_NEAR_IS_MINUS_W));
    struct nn_gfx_image image=NVN_GRAPHICS_IMAGE;int sampled=image.profile==NN_GFX_TEXTURE_2D_NORMALIZED;
    T(graphics_profile(&image));
    struct object *p[8];for(unsigned i=0;i<8;i++)p[i]=pool(dev,&keys[i],i==3||i>=6?16384:4096);
    struct object *rt=texture(dev,&rtkey,p[3],64);(void)rt;
    T(api_nvnProgramInitialize(&pk,&dk));struct object *pr=find(&pk,PROGRAM);T(pr);
    struct nn_nvn_sampled map={.stage=NVN_SHADER_STAGE_FRAGMENT,.texture_slot=10,.sampler_slot=0};
    struct nn_nvn_compiled_graphics compiled={.image=&image,.sampled=&map,.sampled_count=sampled?1:0};
    pr->graphics=&compiled;T(!nn_bo_alloc(dev->gpu,image.vs_code_bytes,&pr->bo));
    T(!nn_bo_alloc(dev->gpu,image.fs_code_bytes,&pr->fs_bo));
    memcpy(nn_bo_map(pr->bo),image.vs_code,image.vs_code_bytes);memcpy(nn_bo_map(pr->fs_bo),image.fs_code,image.fs_code_bytes);
    if(sampled) {
        T(api_nvnTexturePoolInitialize(&tpkey,p[6]->key,0,272));
        T(api_nvnSamplerPoolInitialize(&spkey,p[7]->key,0,272));
        texture(dev,&txkey,p[5],2);
        struct object *sm=create(&skey,SAMPLER);T(sm);sm->device=dev;dev->refs++;
        sm->sampler=(struct sampler_desc){.min=NVN_MIN_FILTER_NEAREST,.mag=NVN_MAG_FILTER_NEAREST,
            .wrap={NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE},
            .compare=NVN_COMPARE_MODE_NONE,.compare_func=NVN_COMPARE_FUNC_LESS,
            .lod_min=0,.lod_max=0,.anisotropy=1,.reduction=NVN_SAMPLER_REDUCTION_AVERAGE};
        OK(api_nvnTexturePoolRegisterTexture(&tpkey,257,&txkey,NULL));
        OK(api_nvnSamplerPoolRegisterSampler(&spkey,259,&skey));
    }
    T(api_nvnCommandBufferInitialize(&ck,&dk));
    OK(api_nvnCommandBufferAddCommandMemory(&ck,p[0]->key,0,4096));
    OK(api_nvnCommandBufferAddControlMemory(&ck,control,sizeof(control)));
    OK(api_nvnCommandBufferBeginRecording(&ck));
    /* Focused binding fixture. Existing full NVN GPU probes cover these APIs. */
    struct recording *r=find(&ck,COMMAND)->record;
    r->graphics_program=pr;pr->refs++;r->target=rt;rt->refs++;
    SETUP_STATE(api_nvnCommandBufferBindVertexBuffer(&ck,0,nn_bo_address(p[1]->bo),image.vertex_bytes));
    r->graphics_ubos[NVN_SHADER_STAGE_VERTEX][0]=(struct bound){p[2],0,64,nn_bo_address(p[2]->bo)};
    r->graphics_ubos[NVN_SHADER_STAGE_FRAGMENT][0]=(struct bound){p[4],0,64,nn_bo_address(p[4]->bo)};
    p[2]->refs++;p[4]->refs++;
    NVNvertexAttribState attributes[3];NVNvertexStreamState stream;
    for(unsigned i=0;i<3;i++)SETUP_STATE(api_nvnVertexAttribStateSetDefaults(attributes+i));
    SETUP_STATE(api_nvnVertexAttribStateSetFormat(attributes,NVN_FORMAT_RG32F,0));
    SETUP_STATE(api_nvnVertexAttribStateSetFormat(attributes+1,(sampled)?NVN_FORMAT_RG32F:NVN_FORMAT_RGBA32F,8));
    SETUP_STATE(api_nvnVertexAttribStateSetFormat(attributes+2,NVN_FORMAT_RGBA32F,16));
    SETUP_STATE(api_nvnVertexStreamStateSetDefaults(&stream));
    SETUP_STATE(api_nvnVertexStreamStateSetStride(&stream,(sampled)?32:24));
    SETUP_STATE(api_nvnCommandBufferBindVertexAttribState(&ck,(sampled)?3:2,attributes));
    SETUP_STATE(api_nvnCommandBufferBindVertexStreamState(&ck,1,&stream));
    SETUP_STATE(api_nvnCommandBufferSetViewport(&ck,0,0,64,64));
    SETUP_STATE(api_nvnCommandBufferSetScissor(&ck,0,0,64,64));
    SETUP_STATE(api_nvnCommandBufferSetDepthRange(&ck,0,1));
    if(sampled) {
        OK(api_nvnCommandBufferSetTexturePool(&ck,&tpkey));OK(api_nvnCommandBufferSetSamplerPool(&ck,&spkey));
        OK(api_nvnCommandBufferBindSeparateTexture(&ck,NVN_SHADER_STAGE_FRAGMENT,10,(NVNseparateTextureHandle){257}));
        OK(api_nvnCommandBufferBindSeparateSampler(&ck,NVN_SHADER_STAGE_FRAGMENT,0,(NVNseparateSamplerHandle){259ull<<20}));
    }
    const float first_color[4]={.125f,.25f,.5f,1};
    OK(api_nvnCommandBufferSetBlendColor(&ck,first_color));
    OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
    const float second_color[4]={.75f,.5f,.25f,1};
    OK(api_nvnCommandBufferSetBlendColor(&ck,second_color));
    OK(seto(&dk,NVN_WINDOW_ORIGIN_MODE_UPPER_LEFT));OK(setd(&dk,NVN_DEPTH_MODE_NEAR_IS_ZERO));
    OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
    size_t used=r->used,refs=r->current->count;struct late_draw *draw_head=r->current->draws;
    const float mutated_color[4]={1,0,1,0};
    OK(api_nvnCommandBufferSetBlendColor(&ck,mutated_color));
    NVNdepthStencilState ds;api_nvnDepthStencilStateSetDefaults(&ds);
    api_nvnDepthStencilStateSetDepthTestEnable(&ds,1);api_nvnCommandBufferBindDepthStencilState(&ck,&ds);
    T(!nn_nvn_error());ERR(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3),ENOTSUP);
    T(r->used==used&&r->current->count==refs&&r->current->draws==draw_head);
    api_nvnDepthStencilStateSetDefaults(&ds);api_nvnCommandBufferBindDepthStencilState(&ck,&ds);T(!nn_nvn_error());
    image.schema=2;T(!graphics_profile(&image));
    ERR(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3),ENOTSUP);
    T(r->used==used&&r->current->count==refs&&r->current->draws==draw_head);image.schema=3;
    dev->graphics_state.window_origin=2;
    ERR(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3),ENOTSUP);
    T(r->used==used&&r->current->count==refs&&r->current->draws==draw_head);
    OK(seto(&dk,NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT)); /* Current device is now0/1. */
    NVNcommandHandle h=api_nvnCommandBufferEndRecording(&ck);T(h);T(!nn_nvn_error());
    struct handle *record=handles;T(record&&record->id==h);
    if(sampled) {
        struct late_draw *d=record->draws;T(d&&d->state.window_origin==1&&d->state.depth_mode==1);
        T(d->next&&d->next->state.window_origin==0&&d->next->state.depth_mode==0&&!d->next->next);
        T(!memcmp(d->render.blend_color,second_color,sizeof(second_color)));
        T(!memcmp(d->next->render.blend_color,first_color,sizeof(first_color)));
        T(r->render.blend_color[0]==1&&r->render.blend_color[3]==0);
        for(unsigned repeat=0;repeat<2;repeat++)for(struct late_draw *a=d;a;a=a->next) {
            nn_bo *job=NULL,*sampled_bo=NULL;size_t bytes=0;
            T(!materialize_draw(dev,a,&job,&bytes,&sampled_bo));T(job&&sampled_bo==p[5]->bo&&bytes);
            encoded(job,&image,a->state.window_origin,a->state.depth_mode);T(!nn_bo_destroy(job));
        }
    } else {
        struct job *j=record->jobs;T(j&&j->next&&!j->next->next);
        encoded(j->bo,&image,1,1);encoded(j->next->bo,&image,0,0);
        OK(setd(&dk,NVN_DEPTH_MODE_NEAR_IS_MINUS_W));
        encoded(j->bo,&image,1,1);encoded(j->next->bo,&image,0,0);
    }
    OK(api_nvnCommandBufferFinalize(&ck));OK(api_nvnDeviceFinalizeCommandHandle(&dk,h));
    if(sampled) {OK(api_nvnTexturePoolFinalize(&tpkey));OK(api_nvnSamplerPoolFinalize(&spkey));
        OK(api_nvnTextureFinalize(&txkey));OK(api_nvnSamplerFinalize(&skey));}
    OK(api_nvnTextureFinalize(&rtkey));OK(api_nvnProgramFinalize(&pk));
    for(unsigned i=0;i<8;i++)OK(api_nvnMemoryPoolFinalize((NVNmemoryPool *)p[i]->key));
    OK(api_nvnDeviceFinalize(&dk));ERR((void)geto(&dk),EINVAL);
    while(objects){T(objects->kind==DB);remove_object(objects);}
    T(!handles);printf("{\"scope\":\"host_only_mode_frontend\",\"profile\":%u,\"checks\":%u,\"same_record_two_mode_snapshots\":true,\"gpu_submits\":0}\n",image.profile,checks);
    return 0;
}
