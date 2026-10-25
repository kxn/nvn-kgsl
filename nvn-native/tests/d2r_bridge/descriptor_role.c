/* HOST RAM ONLY: real native recorder/materializer, no GPU execution or pixels.
 * Program code cache/bound-state setup is whitebox; public descriptor APIs and
 * actual schema3 hardware encoding are tested. Generated shader data ignored. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef NN_DESCRIPTOR_FLAGS
#define NN_DESCRIPTOR_FLAGS 0x12
#endif
static void *fixture_realloc(void *,size_t);
#define realloc fixture_realloc
#ifndef NN_TEST_NVN_SOURCE
#define NN_TEST_NVN_SOURCE "../../src/nvn.c"
#endif
#include NN_TEST_NVN_SOURCE
#undef realloc
#include NN_MODE_IMAGE_HEADER
#define nn_queue_submit backend_submit
#define nn_bo_cache backend_cache
#ifndef NN_MEMORY_BACKEND_SOURCE
#define NN_MEMORY_BACKEND_SOURCE "memory_backend.c"
#endif
#include NN_MEMORY_BACKEND_SOURCE
#undef nn_queue_submit
#undef nn_bo_cache
static unsigned checks,submits,cache_calls,private_bases,policy_rejections;
static int fail_after=-1,fail_cache,last_cache_direction;
static nn_bo *last_cache_bo;static size_t last_cache_offset,last_cache_bytes;
static nn_bo *software_bos[2];
static const struct nn_gfx_image *fixture_image;
static nn_bo *expected_texture, *submitted_jobs[2];
static unsigned char *job_snapshots[2];static size_t job_sizes[2];
#define T(x) do {++checks;if(!(x)){fprintf(stderr,"line%d %s errno%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define SETUP_STATE(x) do { x; assert(nn_nvn_error()==0); }while(0)
#define ERR(x,e) do{x;T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static void *fixture_realloc(void *p,size_t n)
{if(fail_after==0)return NULL;if(fail_after>0)--fail_after;return realloc(p,n);}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int from)
{cache_calls++;last_cache_bo=b;last_cache_offset=o;last_cache_bytes=n;last_cache_direction=from;if(fail_cache){int e=fail_cache;fail_cache=0;return e;}return backend_cache(b,o,n,from);}
static void private_tables(nn_bo *job)
{
    unsigned found=0;const uint32_t *w=nn_bo_map(job);
    for(unsigned i=0;i<fixture_image->patch_count;i++){
        const struct nn_gfx_patch *p=fixture_image->patches+i;
        if(p->kind!=NN_GFX_TEXTURE_DESCRIPTORS&&p->kind!=NN_GFX_SAMPLERS)continue;
        uint64_t a=(uint64_t)w[p->word]|((uint64_t)w[p->word+1]<<32);
        uint64_t base=nn_bo_address(job);
        T(a>=base&&a-base<nn_bo_size(job)&&p->bytes<=nn_bo_size(job)-(a-base));
        for(unsigned j=0;j<2;j++)T(a<nn_bo_address(software_bos[j])||a-nn_bo_address(software_bos[j])>=nn_bo_size(software_bos[j]));
        found|=1u<<(p->kind==NN_GFX_SAMPLERS);private_bases++;
    }
    T(found==3);
}
/* Only RAM ownership: no packets are executed. Read actual parent IB fields,
 * look up child in provided BO list, and check fixed image patch metadata. */
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
    T(count<64&&bytes==24&&offset==0);const uint32_t *w=nn_bo_map(ib);
    T(w[0]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
    uint64_t child=(uint64_t)w[1]|((uint64_t)w[2]<<32);nn_bo *job=NULL;
    for(size_t i=0;i<count;i++){
        T(refs[i]!=software_bos[0]&&refs[i]!=software_bos[1]);
        if(nn_bo_address(refs[i])==child)job=refs[i];
    }
    struct nn_render_state state;nn_render_state_defaults(&state);
    const struct nn_render_target_info target={fixture_image->chip,0,1,0,1};size_t state_words=0;
    T(job&&!nn_render_state_encode(&state,&target,NULL,0,&state_words)&&w[3]==fixture_image->command_words+state_words);private_tables(job);
    /* Decode texture-data address only through fixed schema patch metadata. */
    const uint32_t *jw=nn_bo_map(job);unsigned data_addresses=0;
    for(unsigned i=0;i<fixture_image->patch_count;i++){
        const struct nn_gfx_patch *p=fixture_image->patches+i;
        if(p->kind!=NN_GFX_TEXTURE_DESCRIPTORS)continue;
        uint64_t base=(uint64_t)jw[p->word]|((uint64_t)jw[p->word+1]<<32);
        const uint32_t *desc=(const uint32_t *)((char *)nn_bo_map(job)+base-nn_bo_address(job)-p->offset);
        for(unsigned j=0;j<fixture_image->texture_patch_count;j++){
            const struct nn_gfx_patch *t=fixture_image->texture_patches+j;uint64_t address;
            memcpy(&address,desc+t->word,sizeof(address));
            T((address&fixture_image->texture_address_mask)==nn_bo_address(expected_texture)+t->offset);data_addresses++;
        }
    }
    T(data_addresses&&submits<2);submitted_jobs[submits]=job;job_sizes[submits]=nn_bo_size(job);
    job_snapshots[submits]=malloc(job_sizes[submits]);T(job_snapshots[submits]);
    memcpy(job_snapshots[submits],nn_bo_map(job),job_sizes[submits]);
    /* Retain immutable BO versions until Finish exactly as a submission owner.
     * This tests lifetime contracts, not an ioctl/kernel or cache coherence. */
    nn_bo *all[65];size_t n=0;all[n++]=ib;
    for(size_t i=0;i<count;i++){size_t j=0;for(;j<n;j++)if(all[j]==refs[i])break;if(j==n)all[n++]=refs[i];}
    T(q->count+n<64);for(size_t i=0;i<n;i++){all[i]->busy++;q->pending[q->count++]=all[i];}
    *timestamp=++submits;return 0;
}
static struct object *cached_pool(struct object *dev,const void *key,size_t size)
{
    struct object *p=create(key,POOL);T(p);T(!nn_bo_alloc(dev->gpu,size,&p->bo));
    p->device=dev;p->size=size;p->storage=nn_bo_map(p->bo);p->flags=0x24;dev->refs++;return p;
}
static struct object *texture(struct object *dev,const void *key,struct object *p,unsigned dim)
{
    struct object *t=create(key,TEXTURE);T(t);t->device=dev;t->pool=p;t->bo=p->bo;
    t->size=dim==64?16384:4096;t->texture=(struct texture_desc){
        .flags=NVN_TEXTURE_FLAGS_LINEAR,.target=NVN_TEXTURE_TARGET_2D,.width=dim,.height=dim,
        .depth=1,.levels=1,.format=NVN_FORMAT_RGBA8,.stride=256,.set=TEX_COMPLETE};p->refs++;return t;
}
static void rejected(void)
{
    struct nn_nvn_pool_access_observation o;T(nn_nvn_error()==ENOTSUP);
    T(!nn_nvn_get_pool_access_observation(&o));T(o.flags==NN_DESCRIPTOR_FLAGS&&o.policy==NN_NVN_GPU_UNCACHED_UNQUALIFIED);
    ++policy_rejections;nn_nvn_clear_error();
}
int main(void)
{
    NVNdeviceBuilder db={0};NVNdevice dk={0};NVNmemoryPoolBuilder pb={0};NVNmemoryPool mk[2]={0};
    NVNcommandBuffer ck={0};NVNprogram pk={0};NVNtexture rtkey={0},tk[2]={0};NVNsampler sk={0};
    NVNtexturePool tpk[2]={0};NVNsamplerPool spk[2]={0};unsigned char keys[7];
    NVNqueueBuilder qb={0};NVNqueue qk={0};_Alignas(16) unsigned char control[512];
    OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *dev=find(&dk,DEVICE);T(dev);
    struct nn_gfx_image image=NVN_GRAPHICS_IMAGE;fixture_image=&image;T(image.schema==3&&image.profile==1&&graphics_profile(&image));
    struct object *p[7];for(unsigned i=0;i<7;i++)p[i]=cached_pool(dev,keys+i,i==3?16384:4096);
    void *storage[2];struct object *meta[2],*tp[2],*sp[2];
    for(unsigned i=0;i<2;i++){
        T(!posix_memalign(storage+i,4096,2097152));memset(storage[i],0xa5,2097152);
        OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&dk));
        OK(api_nvnMemoryPoolBuilderSetStorage(&pb,storage[i],2097152));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,NN_DESCRIPTOR_FLAGS));
        T(api_nvnMemoryPoolInitialize(mk+i,&pb));meta[i]=find(mk+i,POOL);T(meta[i]&&meta[i]->gpu_policy==NN_NVN_GPU_UNCACHED_UNQUALIFIED);
        T(api_nvnMemoryPoolMap(mk+i)==storage[i]);software_bos[i]=meta[i]->bo;
        /* Observed D2R texture15256 and sampler1256 table shapes;
         * GPU test IDs/sampler contents remain explicit test specialization. */
        T(api_nvnTexturePoolInitialize(tpk+i,mk+i,0,15256));T(api_nvnSamplerPoolInitialize(spk+i,mk+i,488192,1256));
        tp[i]=find(tpk+i,TPOOL);sp[i]=find(spk+i,SPOOL);T(tp[i]&&sp[i]&&tp[i]->cpu_software_table&&sp[i]->cpu_software_table);
        T(meta[i]->refs==2);T(api_nvnTexturePoolGetMemoryPool(tpk+i)==mk+i&&api_nvnTexturePoolGetMemoryOffset(tpk+i)==0&&api_nvnTexturePoolGetSize(tpk+i)==15256);
        T(api_nvnSamplerPoolGetMemoryPool(spk+i)==mk+i&&api_nvnSamplerPoolGetMemoryOffset(spk+i)==488192&&api_nvnSamplerPoolGetSize(spk+i)==1256);
        NVNtexturePool overlap={0};T(!api_nvnTexturePoolInitialize(&overlap,mk+i,32,272));T(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();
        T(meta[i]->refs==2);ERR(api_nvnMemoryPoolFinalize(mk+i),EBUSY);
    }
    struct object *rt=texture(dev,&rtkey,p[3],64);texture(dev,tk,p[5],2);texture(dev,tk+1,p[6],2);
    struct object *sm=create(&sk,SAMPLER);T(sm);sm->device=dev;dev->refs++;
    sm->sampler=(struct sampler_desc){.min=NVN_MIN_FILTER_NEAREST,.mag=NVN_MAG_FILTER_NEAREST,
        .wrap={NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE},
        .compare=NVN_COMPARE_MODE_NONE,.compare_func=NVN_COMPARE_FUNC_LESS,.anisotropy=1,.reduction=NVN_SAMPLER_REDUCTION_AVERAGE};
    unsigned caches=cache_calls;
    OK(api_nvnTexturePoolRegisterTexture(tpk+1,257,tk+1,NULL));
    for(unsigned i=0;i<2;i++)OK(api_nvnSamplerPoolRegisterSampler(spk+i,259,&sk));
    T(cache_calls==caches+(NN_DESCRIPTOR_FLAGS==0x14?3u:0u)&&
        (NN_DESCRIPTOR_FLAGS!=0x14||(last_cache_bo==meta[1]->bo&&last_cache_offset==488192+259*32&&last_cache_bytes==32&&!last_cache_direction)));T(tp[0]->slots[257].valid==0&&tp[1]->slots[257].pool==p[6]);
    ERR(api_nvnTexturePoolRegisterTexture(tpk,255,tk,NULL),EINVAL);
    ERR(api_nvnSamplerPoolRegisterSampler(spk,1256,&sk),EINVAL);
    T(!memcmp((char *)storage[1]+257*32,&(struct texture_slot){0x54584e4e,NVN_FORMAT_RGBA8,2,2,256,4096,nn_bo_address(p[6]->bo)},32));
    OK(api_nvnSamplerFinalize(&sk)); /* Registered sampler snapshots own values. */
    T(api_nvnProgramInitialize(&pk,&dk));struct object *pr=find(&pk,PROGRAM);T(pr);
    struct nn_nvn_sampled map={.stage=NVN_SHADER_STAGE_FRAGMENT,.texture_slot=10,.sampler_slot=0};
    struct nn_nvn_compiled_graphics compiled={.image=&image,.sampled=&map,.sampled_count=1};pr->graphics=&compiled;
    T(!nn_bo_alloc(dev->gpu,image.vs_code_bytes,&pr->bo));T(!nn_bo_alloc(dev->gpu,image.fs_code_bytes,&pr->fs_bo));
    memcpy(nn_bo_map(pr->bo),image.vs_code,image.vs_code_bytes);memcpy(nn_bo_map(pr->fs_bo),image.fs_code,image.fs_code_bytes);
    T(api_nvnCommandBufferInitialize(&ck,&dk));OK(api_nvnCommandBufferAddCommandMemory(&ck,p[0]->key,0,4096));
    api_nvnCommandBufferAddCommandMemory(&ck,mk,0,4096);rejected();
    OK(api_nvnCommandBufferAddControlMemory(&ck,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&ck));
    struct recording *r=find(&ck,COMMAND)->record;r->graphics_program=pr;pr->refs++;r->target=rt;rt->refs++;
    SETUP_STATE(api_nvnCommandBufferBindVertexBuffer(&ck,0,nn_bo_address(p[1]->bo),image.vertex_bytes));
    r->graphics_ubos[0][0]=(struct bound){p[2],0,64,nn_bo_address(p[2]->bo)};p[2]->refs++;
    r->graphics_ubos[1][0]=(struct bound){p[4],0,64,nn_bo_address(p[4]->bo)};p[4]->refs++;
    NVNvertexAttribState attributes[3];NVNvertexStreamState stream;
    for(unsigned i=0;i<3;i++)SETUP_STATE(api_nvnVertexAttribStateSetDefaults(attributes+i));
    SETUP_STATE(api_nvnVertexAttribStateSetFormat(attributes,NVN_FORMAT_RG32F,0));
    SETUP_STATE(api_nvnVertexAttribStateSetFormat(attributes+1,NVN_FORMAT_RG32F,8));
    SETUP_STATE(api_nvnVertexAttribStateSetFormat(attributes+2,NVN_FORMAT_RGBA32F,16));
    SETUP_STATE(api_nvnVertexStreamStateSetDefaults(&stream));
    SETUP_STATE(api_nvnVertexStreamStateSetStride(&stream,32));
    SETUP_STATE(api_nvnCommandBufferBindVertexAttribState(&ck,3,attributes));
    SETUP_STATE(api_nvnCommandBufferBindVertexStreamState(&ck,1,&stream));
    SETUP_STATE(api_nvnCommandBufferSetViewport(&ck,0,0,64,64));
    SETUP_STATE(api_nvnCommandBufferSetScissor(&ck,0,0,64,64));
    SETUP_STATE(api_nvnCommandBufferSetDepthRange(&ck,0,1));
    PFNNVNCOMMANDBUFFERSETTEXTUREPOOLPROC set_tp=(PFNNVNCOMMANDBUFFERSETTEXTUREPOOLPROC)nvnBootstrapLoader("nvnCommandBufferSetTexturePool");
    PFNNVNCOMMANDBUFFERSETSAMPLERPOOLPROC set_sp=(PFNNVNCOMMANDBUFFERSETSAMPLERPOOLPROC)nvnBootstrapLoader("nvnCommandBufferSetSamplerPool");T(set_tp&&set_sp);
    OK(set_tp(&ck,tpk));OK(set_sp(&ck,spk));T(tp[0]->refs==1&&sp[0]->refs==1);
    OK(set_tp(&ck,NULL));OK(set_sp(&ck,NULL));OK(set_tp(&ck,NULL));OK(set_sp(&ck,NULL));T(!tp[0]->refs&&!sp[0]->refs);
    OK(set_tp(&ck,tpk));OK(set_sp(&ck,spk));
    /* Metadata marker does not grant a GPU role to the same pool. */
    uint64_t address=nn_bo_address(meta[0]->bo);size_t used=r->used;
#define DENY(call) do {call;rejected();T(r->used==used&&!r->current->count&&!r->current->cpu_count);}while(0)
    DENY(api_nvnCommandBufferBindUniformBuffer(&ck,NVN_SHADER_STAGE_VERTEX,0,address,64));
    DENY(api_nvnCommandBufferBindStorageBuffer(&ck,NVN_SHADER_STAGE_COMPUTE,0,address,64));
    DENY(api_nvnCommandBufferBindVertexBuffer(&ck,0,address,96));
    DENY(api_nvnCommandBufferClearBuffer(&ck,address,16,7));
    uint32_t ctrl=0;NVNshaderData shader={address,&ctrl};DENY((void)api_nvnProgramSetShaders(&pk,1,&shader));
    NVNtextureBuilder tb={0};NVNtexture denied_texture={0};
    OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnTextureBuilderSetDevice(&tb,&dk));
    struct object *builder=find(&tb,TB);builder->texture=(struct texture_desc){NVN_TEXTURE_TARGET_2D,2,2,1,1,0,NVN_FORMAT_RGBA8,NVN_TEXTURE_FLAGS_LINEAR,256,TEX_COMPLETE};
    OK(api_nvnTextureBuilderSetStorage(&tb,mk,0));DENY((void)api_nvnTextureInitialize(&denied_texture,&tb));
    struct descriptor_entry bad_entry={.pool=meta[0],.size=4096,.valid=1};struct texture_slot fake={0};
    DENY((void)descriptor_write(tp[0],257,&fake,bad_entry));T(!tp[0]->slots[257].valid);
    /* Both combined and wrong separate domains cannot supply required slot. */
    OK(api_nvnCommandBufferBindTexture(&ck,NVN_SHADER_STAGE_FRAGMENT,10,257|(259ull<<20)));
    ERR(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3),EINVAL);T(r->used==used);
    NVNseparateTextureHandle th[1]={{257}};NVNseparateSamplerHandle sh[1]={{259ull<<20}};
    OK(api_nvnCommandBufferBindSeparateTextures(&ck,NVN_SHADER_STAGE_FRAGMENT,11,1,th));
    OK(api_nvnCommandBufferBindSeparateSamplers(&ck,NVN_SHADER_STAGE_FRAGMENT,1,1,sh));
    ERR(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3),EINVAL);T(r->used==used);
    OK(api_nvnCommandBufferBindSeparateTextures(&ck,NVN_SHADER_STAGE_FRAGMENT,10,1,th));
    OK(api_nvnCommandBufferBindSeparateSamplers(&ck,NVN_SHADER_STAGE_FRAGMENT,0,1,sh));th[0].value=1;sh[0].value=1;
    /* Marker corruption rejects before any bytes/refs are touched. */
    tp[0]->cpu_software_table=0;ERR(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3),ENOTSUP);tp[0]->cpu_software_table=1;
    /* Exercise each of six GPU-list + two CPU-list realloc failure points. */
    size_t before_refs[7];for(unsigned i=0;i<7;i++)before_refs[i]=p[i]->refs;
    for(unsigned i=0;i<8;i++){
        fail_after=(int)i;ERR(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3),ENOMEM);fail_after=-1;
        T(r->used==used&&!r->current->count&&!r->current->cpu_count&&!r->current->draws);
        T(tp[0]->refs==1&&sp[0]->refs==1);for(unsigned j=0;j<7;j++)T(p[j]->refs==before_refs[j]);
    }
    struct handle limit={.cpu_count=65535};limit.cpu_resources=calloc(65535,sizeof(*limit.cpu_resources));T(limit.cpu_resources);T(add_cpu_resource(&limit,tp[0])==E2BIG);free(limit.cpu_resources);
    OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
    T(r->current->count==6&&r->current->cpu_count==2&&tp[0]->refs==2&&sp[0]->refs==2);
    T(!add_cpu_resource(r->current,tp[0])&&r->current->cpu_count==2&&tp[0]->refs==2);
    for(size_t i=0;i<r->current->count;i++)T(r->current->resources[i]!=meta[0]&&r->current->resources[i]!=tp[0]&&r->current->resources[i]!=sp[0]);
    NVNcommandHandle h=api_nvnCommandBufferEndRecording(&ck);T(h&&!nn_nvn_error());struct handle *record=handles;
    /* Record before registration; select A, not B with the identical ID. */
    nn_bo *job=NULL,*sampled=NULL;size_t bytes=0;T(materialize_draw(dev,record->draws,&job,&bytes,&sampled)==EINVAL&&!job);
    OK(api_nvnTexturePoolRegisterTexture(tpk,257,tk,NULL));
    T(!materialize_draw(dev,record->draws,&job,&bytes,&sampled));T(sampled==p[5]->bo);private_tables(job);T(!nn_bo_destroy(job));
    unsigned char saved=((unsigned char *)storage[0])[257*32];((unsigned char *)storage[0])[257*32]^=1;
    T(materialize_draw(dev,record->draws,&job,&bytes,&sampled)==EINVAL);((unsigned char *)storage[0])[257*32]=saved;
    OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&qk,&qb));
    /* Even metadata BO accidentally included as an actual GPU resource fails. */
    size_t previous=record->count;T(!add_resource(record,meta[0]));unsigned was=submits;
    api_nvnQueueSubmitCommands(&qk,1,&h);rejected();T(submits==was);record->resources[--record->count]->refs--;T(record->count==previous);
    /* Multi-handle all-preflight must fail before materializing first valid h. */
    struct handle forbidden={.id=999,.device=dev,.commands=meta[0],.bytes=16};record->next=&forbidden;
    NVNcommandHandle pair[]={h,999};api_nvnQueueSubmitCommands(&qk,2,pair);rejected();T(submits==was);record->next=NULL;
    expected_texture=p[5]->bo;OK(api_nvnQueueSubmitCommands(&qk,1,&h));T(submits==was+1&&p[5]->bo->busy&&dev->retired);
#if NN_DESCRIPTOR_FLAGS == 0x14
    /* Failed CPU cache publication must not replace the slot/version/refs. */
    unsigned char previous_slot[32];memcpy(previous_slot,(char *)storage[0]+257*32,32);
    struct descriptor_entry previous_entry=tp[0]->slots[257];
    size_t old_refs=p[5]->refs,new_refs=p[6]->refs;fail_cache=EIO;
    ERR(api_nvnTexturePoolRegisterTexture(tpk,257,tk+1,NULL),EIO);
    T(!memcmp(previous_slot,(char *)storage[0]+257*32,32)&&!memcmp(&previous_entry,tp[0]->slots+257,sizeof(previous_entry)));
    T(p[5]->refs==old_refs&&p[6]->refs==new_refs&&!fail_cache);
#endif
    OK(api_nvnTexturePoolRegisterTexture(tpk,257,tk+1,NULL));
    T(tp[0]->slots[257].pool==p[6]);T(!memcmp(job_snapshots[0],nn_bo_map(submitted_jobs[0]),job_sizes[0]));
    expected_texture=p[6]->bo;OK(api_nvnQueueSubmitCommands(&qk,1,&h));T(submits==was+2&&p[6]->bo->busy);
    /* Same recorded handle resolves replacement only at submit; old job lives. */
    T(p[5]->bo->busy&&p[6]->bo->busy);OK(api_nvnTextureFinalize(tk));
    ERR(api_nvnMemoryPoolFinalize((NVNmemoryPool *)p[5]->key),EBUSY);
    /* Two actual Draws in one active recording select different CPU tables;
     * abort must return both sets of CPU refs and deduplicated GPU refs. */
    struct object *watched[]={pr,rt,p[0],p[1],p[2],p[4],tp[0],sp[0],tp[1],sp[1]};
    size_t saved_refs[sizeof(watched)/sizeof(watched[0])];
    for(unsigned i=0;i<sizeof(watched)/sizeof(watched[0]);i++)saved_refs[i]=watched[i]->refs;
    NVNcommandBuffer abort_key={0};_Alignas(16) unsigned char abort_control[64];
    T(api_nvnCommandBufferInitialize(&abort_key,&dk));
    OK(api_nvnCommandBufferAddCommandMemory(&abort_key,p[0]->key,256,4096-256));
    OK(api_nvnCommandBufferAddControlMemory(&abort_key,abort_control,sizeof(abort_control)));
    OK(api_nvnCommandBufferBeginRecording(&abort_key));
    struct recording *ar=find(&abort_key,COMMAND)->record;
    ar->graphics_program=pr;pr->refs++;ar->target=rt;rt->refs++;
    SETUP_STATE(api_nvnCommandBufferBindVertexBuffer(&abort_key,0,r->vertex.address,r->vertex.size));
    ar->graphics_ubos[0][0]=r->graphics_ubos[0][0];ar->graphics_ubos[0][0].pool->refs++;
    ar->graphics_ubos[1][0]=r->graphics_ubos[1][0];ar->graphics_ubos[1][0].pool->refs++;
    SETUP_STATE(api_nvnCommandBufferBindVertexAttribState(&abort_key,3,attributes));
    SETUP_STATE(api_nvnCommandBufferBindVertexStreamState(&abort_key,1,&stream));
    SETUP_STATE(api_nvnCommandBufferSetViewport(&abort_key,0,0,64,64));
    SETUP_STATE(api_nvnCommandBufferSetScissor(&abort_key,0,0,64,64));
    SETUP_STATE(api_nvnCommandBufferSetDepthRange(&abort_key,0,1));
    OK(api_nvnCommandBufferBindSeparateTexture(&abort_key,NVN_SHADER_STAGE_FRAGMENT,10,(NVNseparateTextureHandle){257}));
    OK(api_nvnCommandBufferBindSeparateSampler(&abort_key,NVN_SHADER_STAGE_FRAGMENT,0,(NVNseparateSamplerHandle){259ull<<20}));
    for(unsigned i=0;i<2;i++){
        OK(set_tp(&abort_key,tpk+i));OK(set_sp(&abort_key,spk+i));
        OK(api_nvnCommandBufferDrawArrays(&abort_key,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
    }
    T(ar->current->count==6&&ar->current->cpu_count==4);
    T(ar->current->draws->texture_pool==tp[1]&&ar->current->draws->next->texture_pool==tp[0]);
    OK(api_nvnCommandBufferFinalize(&abort_key));
    for(unsigned i=0;i<sizeof(watched)/sizeof(watched[0]);i++)T(saved_refs[i]==watched[i]->refs);
    OK(api_nvnCommandBufferFinalize(&ck));ERR(api_nvnTexturePoolFinalize(tpk),EBUSY);
    ERR(api_nvnSamplerPoolFinalize(spk),EBUSY);OK(api_nvnDeviceFinalizeCommandHandle(&dk,h));
    T(!tp[0]->refs&&!sp[0]->refs);for(unsigned i=0;i<2;i++){
        OK(api_nvnTexturePoolFinalize(tpk+i));OK(api_nvnSamplerPoolFinalize(spk+i));T(!meta[i]->refs&&!meta[i]->bo->busy);
        OK(api_nvnMemoryPoolFinalize(mk+i));free(storage[i]);
    }
    T(p[5]->bo->busy&&p[6]->bo->busy);
    for(unsigned i=0;i<2;i++){T(!memcmp(job_snapshots[i],nn_bo_map(submitted_jobs[i]),job_sizes[i]));free(job_snapshots[i]);}
    OK(api_nvnQueueFinish(&qk));T(!dev->retired);
    OK(api_nvnQueueFinalize(&qk));OK(api_nvnTextureFinalize(tk+1));OK(api_nvnTextureFinalize(&rtkey));OK(api_nvnProgramFinalize(&pk));
    for(unsigned i=0;i<7;i++)OK(api_nvnMemoryPoolFinalize((NVNmemoryPool *)p[i]->key));
    T(!dev->refs&&!dev->gpu->bos);OK(api_nvnDeviceFinalize(&dk));
    while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==QB||objects->kind==TB);remove_object(objects);}T(!handles);
    printf("{\"scope\":\"HOST RAM ONLY descriptor role\",\"flags\":%u,\"checks\":%u,\"policy_rejections\":%u,\"host_submits\":%u,\"gpu_submits\":0,\"private_descriptor_bases\":%u}\n",NN_DESCRIPTOR_FLAGS,checks,policy_rejections,submits,private_bases);return 0;
}
