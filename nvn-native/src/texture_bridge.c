/* SPDX-License-Identifier: MIT
 * Native SSBO conversion kernel qualified by conversion55; ordering from
 * public fd6 CCU/event recipe via order.c and common compute.c. No runtime
 * compiler, CPU conversion, Vulkan/Metal, caller-storage substitution. */
#include "nvn_native/texture_bridge.h"
#include "nvn_native/order.h"
#include "nvn_native/pm4.h"
#include <errno.h>
#include <string.h>
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
/* Qualification only: discrete CP progress, never timing/completion/fault PC.
 * The original wrapper owner/resource list retains this Normal-NC BO even on
 * an unknown submission. No new memory allocation or GPU address is hidden. */
extern void nn_tex_bridge_probe_track(nn_bo *,size_t,unsigned,unsigned);
extern unsigned nn_tex_bridge_probe_mask(void);
static void progress(uint32_t *w,size_t *k,uint64_t address,uint32_t value,unsigned mask)
{
    unsigned bit=value==0x10?0:value==0x20?1:value==0x21?2:value==0x30?3:
        value==0x40?4:value==0x50?5:value==0x60?6:7;
    if(mask&(1u<<bit)){
        w[(*k)++]=nvn_pm4_packet7(NVN_CP_MEM_WRITE,3);
        w[(*k)++]=(uint32_t)address;w[(*k)++]=(uint32_t)(address>>32);w[(*k)++]=value;
    }else{
        /* Same exact four DWORDs: no executable length, scratch displacement,
         * allocation or reference-list change between diagnostic masks. */
        w[(*k)++]=nvn_pm4_packet7(0x10,3) /* CP_NOP */;
        w[(*k)++]=0;w[(*k)++]=0;w[(*k)++]=0;
    }
}
#endif
static int native_validate(const struct nn_tex_native_layout *n)
{
    if(!n || !n->width || n->width>16384 || !n->height || n->height>16384 ||
       n->pitch<(uint64_t)n->width*4 || n->pitch%64 || n->pitch>1048576 ||
       n->layer_bytes<(uint64_t)n->pitch*n->height || n->bytes<n->layer_bytes ||
       n->bytes%4 || n->alignment!=64)return ENOTSUP;
    return 0;
}
int nn_tex_bridge_bundle_validate(const struct nn_tex_bridge_bundle *b)
{
    if(!b || b->schema!=1 || b->chip!=0x06050002 || !b->gob ||
       b->gob->schema!=1 || b->gob->chip!=b->chip || b->gob->local[0]!=8 ||
       b->gob->local[1]!=8 || b->gob->local[2]!=1 || b->gob->ssbo_slots!=2 ||
       !b->gob->code || !b->gob->code_bytes || b->gob->code_bytes>65536 ||
       !b->gob->commands || !b->gob->command_words || !b->gob->patches ||
       !b->gob->patch_count)return ENOTSUP;
    return native_validate(&b->clear_layout);
}
int nn_tex_bridge_source_validate(const struct nn_tex_source_layout *s)
{
    int e=nn_tex_source_validate(s);if(e)return e;
    if(s->linear || s->format!=37 || s->target!=1 || s->levels!=1 || s->layers!=1 ||
       s->block_w!=1 || s->block_h!=1 || s->bytes_per_block!=4 ||
       s->payload_bytes>UINT32_MAX || s->payload_bytes%4 || s->level[0].offset)return ENOTSUP;
    return 0;
}
int nn_tex_shadow_destroy(struct nn_tex_shadow *s)
{
    if(!s)return EINVAL;
    for(unsigned i=0;i<NN_TEX_SHADOW_BO_COUNT;i++)if(s->bos[i]){
        int e=nn_bo_destroy(s->bos[i]);if(e)return e;s->bos[i]=NULL;
    }
    s->pre_bytes=s->post_bytes=0;return 0;
}
int nn_tex_shadow_create(nn_device *d,const struct nn_tex_bridge_bundle *b,nn_bo *canonical,
 size_t offset,size_t bytes,const struct nn_tex_source_layout *s,const struct nn_tex_native_layout *n,struct nn_tex_shadow *out)
{
    if(!out)return EINVAL;
    memset(out,0,sizeof(*out));int e=nn_tex_bridge_bundle_validate(b);if(e)return e;
    if((e=nn_tex_bridge_source_validate(s)) || (e=native_validate(n)))return e;
    if(!d || nn_device_chip(d)!=b->chip || !canonical || bytes!=s->payload_bytes ||
       offset>nn_bo_size(canonical) || bytes>nn_bo_size(canonical)-offset ||
       offset>UINT64_MAX-nn_bo_address(canonical) || (nn_bo_address(canonical)+offset)%4 || n->width!=s->level[0].width || n->height!=s->level[0].height)return EINVAL;
    if((e=nn_bo_alloc(d,n->bytes,&out->bos[NN_TEX_SHADOW_STORAGE])) ||
       (e=nn_bo_alloc(d,b->gob->code_bytes,&out->bos[NN_TEX_SHADOW_CODE])) ||
       (e=nn_bo_alloc(d,128,&out->bos[NN_TEX_SHADOW_UNIFORM])))goto fail;
    if(!nn_bo_map(out->bos[NN_TEX_SHADOW_CODE]) || !nn_bo_map(out->bos[NN_TEX_SHADOW_UNIFORM])){e=ENOTSUP;goto fail;}
    memcpy(nn_bo_map(out->bos[NN_TEX_SHADOW_CODE]),b->gob->code,b->gob->code_bytes);
    memset(nn_bo_map(out->bos[NN_TEX_SHADOW_UNIFORM]),0,128);
    uint32_t groups[3]={((s->level[0].row_bytes+3)/4+7)/8,(s->level[0].blocks_y+7)/8,1};
    for(unsigned direction=0;direction<2;direction++){
        struct nn_tex_gob_copy_args args;
        e=nn_tex_gob_copy_args(s,0,direction,n->pitch,n->bytes,&args);if(e)goto fail;
        memcpy((char*)nn_bo_map(out->bos[NN_TEX_SHADOW_UNIFORM])+direction*64,&args,sizeof(args));
        struct nn_cs_buffer u[16]={{0}},v[16]={{0}};
        u[2]=(struct nn_cs_buffer){nn_bo_address(out->bos[NN_TEX_SHADOW_UNIFORM])+direction*64,64};
        struct nn_cs_buffer src={nn_bo_address(canonical)+offset,(uint32_t)bytes};
        struct nn_cs_buffer dst={nn_bo_address(out->bos[NN_TEX_SHADOW_STORAGE]),n->bytes};
        v[0]=direction?dst:src;v[1]=direction?src:dst;
        size_t *length=direction?&out->post_bytes:&out->pre_bytes;
        e=nn_cs_encode(d,out->bos[NN_TEX_SHADOW_CODE],b->gob,u,v,groups,
            &out->bos[direction?NN_TEX_SHADOW_POST:NN_TEX_SHADOW_PRE],length);if(e)goto fail;
    }
    for(unsigned i=1;i<NN_TEX_SHADOW_BO_COUNT;i++){
        e=nn_bo_cache(out->bos[i],0,nn_bo_size(out->bos[i]),0);if(e)goto fail;
    }
    return 0;
fail:
    {int cleanup=nn_tex_shadow_destroy(out);return cleanup?cleanup:e;}
}
static void ib(uint32_t *w,size_t *k,nn_bo *bo,size_t bytes)
{
    uint64_t a=nn_bo_address(bo);w[(*k)++]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);
    w[(*k)++]=(uint32_t)a;w[(*k)++]=(uint32_t)(a>>32);w[(*k)++]=(uint32_t)(bytes/4);
}
int nn_tex_shadow_chain(nn_device *d,const struct nn_tex_shadow *const *in,size_t count,
 nn_bo *child,size_t child_bytes,const struct nn_tex_shadow *out,nn_bo **wrapper,size_t *bytes)
{
    if(!wrapper||!bytes)return EINVAL;
    *wrapper=NULL;*bytes=0;
    if(!d||nn_device_chip(d)!=0x06050002||count>NN_TEX_SHADOW_CHAIN_MAX_INPUTS||(count&&!in)||!child||!child_bytes||child_bytes%4||child_bytes>nn_bo_size(child))return EINVAL;
    for(size_t i=0;i<count;i++)if(!in[i]||!in[i]->bos[NN_TEX_SHADOW_PRE]||!in[i]->pre_bytes||in[i]->pre_bytes%4||in[i]->pre_bytes>nn_bo_size(in[i]->bos[NN_TEX_SHADOW_PRE]))return EINVAL;
    if(out&&(!out->bos[NN_TEX_SHADOW_POST]||!out->post_bytes||out->post_bytes%4||out->post_bytes>nn_bo_size(out->bos[NN_TEX_SHADOW_POST])))return EINVAL;
    /* Keep the same proven two-level IB structure as ordinary NVN draws.
     * Copy ONLY complete encoded command ranges; every embedded GPUVA still
     * addresses its original retained code/descriptor/sequence BO. No byte
     * scanning, shader fixup or texel copy is performed. */
    if(!nn_bo_map(child))return ENOTSUP;
    if(child_bytes/4>0xfffffu-42u)return E2BIG;
    size_t words=32+5+child_bytes/4+5;
    for(size_t i=0;i<count;i++){
        if(!nn_bo_map(in[i]->bos[NN_TEX_SHADOW_PRE]))return ENOTSUP;
        if(in[i]->pre_bytes/4>0xfffffu-words)return E2BIG;
        words+=in[i]->pre_bytes/4;
    }
    if(out){
        if(!nn_bo_map(out->bos[NN_TEX_SHADOW_POST]))return ENOTSUP;
        if(words>0xfffffu-27 || out->post_bytes/4>0xfffffu-words-27)return E2BIG;
        words+=27+out->post_bytes/4;
    }
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
    size_t marks=4+count+(out?2:0);
    if(words>0xfffffu-marks*4)return E2BIG;
    words+=marks*4;
#endif
    if(words>0xfffffu)return E2BIG;
    /* Keep executable IB bytes immutable on pages distinct from GPU-written
     * completion/progress data. This changes no executable packet or IB size;
     * page padding is outside the submitted command range. Not a claimed CP
     * IB-length alignment requirement or a qualified fix for mixed workloads. */
    size_t page=nn_device_page(d),command_bytes=words*4;
    if(page<4 || (page&(page-1)))return ENOTSUP;
    if(command_bytes>SIZE_MAX-(page-1))return EOVERFLOW;
    size_t data_offset=(command_bytes+page-1)&~(page-1);
    size_t data_bytes=4;
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
    data_bytes=8;
#endif
    if(data_offset>SIZE_MAX-data_bytes)return EOVERFLOW;
    size_t allocation_bytes=data_offset+data_bytes;
    nn_bo *bo=NULL;int e=nn_bo_alloc(d,allocation_bytes,&bo);if(e)return e;
    if(nn_bo_address(bo)%page || data_offset>UINT64_MAX-nn_bo_address(bo) ||
       data_bytes>UINT64_MAX-(nn_bo_address(bo)+data_offset)){e=EOVERFLOW;goto fail;}
    uint64_t sequence=nn_bo_address(bo)+data_offset;
    uint32_t *w=nn_bo_map(bo);if(!w){e=ENOTSUP;goto fail;}memset(w,0,allocation_bytes);
    size_t k=0,n=0;
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
    uint64_t mark=sequence+4;
    unsigned diagnostic_mask=nn_tex_bridge_probe_mask();
#define MARK(value) progress(w,&k,mark,value,diagnostic_mask)
#else
#define MARK(value) ((void)0)
#endif
    e=nn_order_words(nn_device_chip(d),sequence,NN_ORDER_FULL_CACHE|NN_ORDER_INVALIDATE,w,words,&n);if(e)goto fail;k+=n;
    MARK(0x10);
    for(size_t i=0;i<count;i++){
        memcpy(w+k,nn_bo_map(in[i]->bos[NN_TEX_SHADOW_PRE]),in[i]->pre_bytes);k+=in[i]->pre_bytes/4;
        MARK(0x20+(uint32_t)i);
    }
    e=nn_order_words(nn_device_chip(d),0,NN_ORDER_INVALIDATE,w+k,words-k,&n);if(e)goto fail;k+=n;
    MARK(0x30);
    memcpy(w+k,nn_bo_map(child),child_bytes);k+=child_bytes/4;
    MARK(0x40);
    if(out){
        e=nn_order_words(nn_device_chip(d),sequence,NN_ORDER_FULL_CACHE,w+k,words-k,&n);if(e)goto fail;k+=n;
        MARK(0x50);
        memcpy(w+k,nn_bo_map(out->bos[NN_TEX_SHADOW_POST]),out->post_bytes);k+=out->post_bytes/4;
        MARK(0x60);
    }
    e=nn_order_words(nn_device_chip(d),0,NN_ORDER_INVALIDATE,w+k,words-k,&n);if(e)goto fail;k+=n;
    MARK(0x70);
#undef MARK
    if(k!=words){e=EOVERFLOW;goto fail;}
    e=nn_bo_cache(bo,0,allocation_bytes,0);if(e)goto fail;
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
    nn_tex_bridge_probe_track(bo,data_offset+4,(unsigned)count,out!=NULL);
#endif
    *wrapper=bo;*bytes=k*4;return 0;
fail:
    {int cleanup=nn_bo_destroy(bo);if(cleanup){*wrapper=bo;return cleanup;}return e;}
}

int nn_tex_conversion_destroy(struct nn_tex_conversion *owner)
{
    if(!owner)return EINVAL;
    for(unsigned i=0;i<NN_TEX_CONVERSION_BO_COUNT;i++)if(owner->bos[i]){
        int e=nn_bo_destroy(owner->bos[i]);if(e)return e;owner->bos[i]=NULL;
    }
    owner->entry_bytes=0;return 0;
}
int nn_tex_conversion_create(nn_device *d,const struct nn_tex_bridge_bundle *bundle,nn_bo *canonical,
 size_t co,size_t cb,const struct nn_tex_source_layout *source,const struct nn_tex_native_layout *layout,
 nn_bo *native,size_t no,size_t nb,uint32_t direction,struct nn_tex_conversion *owner)
{
    if(!owner)return EINVAL;
    memset(owner,0,sizeof(*owner));int e=nn_tex_bridge_bundle_validate(bundle);if(e)return e;
    if((e=nn_tex_bridge_source_validate(source)) || (e=native_validate(layout)))return e;
    if(!d||nn_device_chip(d)!=bundle->chip||!canonical||!native||direction>1||
       canonical==native||cb!=source->payload_bytes||nb<layout->bytes||nb>UINT32_MAX||
       co>nn_bo_size(canonical)||cb>nn_bo_size(canonical)-co||no>nn_bo_size(native)||nb>nn_bo_size(native)-no||
       co>UINT64_MAX-nn_bo_address(canonical)||no>UINT64_MAX-nn_bo_address(native)||
       (nn_bo_address(canonical)+co)%4||(nn_bo_address(native)+no)%4||
       source->level[0].width!=layout->width||source->level[0].height!=layout->height)return EINVAL;
    if((e=nn_bo_alloc(d,bundle->gob->code_bytes,&owner->bos[NN_TEX_CONVERSION_CODE])) ||
       (e=nn_bo_alloc(d,64,&owner->bos[NN_TEX_CONVERSION_UNIFORM])))goto fail;
    if(!nn_bo_map(owner->bos[0])||!nn_bo_map(owner->bos[1])){e=ENOTSUP;goto fail;}
    memcpy(nn_bo_map(owner->bos[0]),bundle->gob->code,bundle->gob->code_bytes);
    struct nn_tex_gob_copy_args args;e=nn_tex_gob_copy_args(source,0,direction,layout->pitch,nb,&args);if(e)goto fail;
    memset(nn_bo_map(owner->bos[1]),0,64);memcpy(nn_bo_map(owner->bos[1]),&args,sizeof(args));
    struct nn_cs_buffer u[16]={{0}},v[16]={{0}};
    u[2]=(struct nn_cs_buffer){nn_bo_address(owner->bos[1]),64};
    struct nn_cs_buffer c={nn_bo_address(canonical)+co,(uint32_t)cb},n={nn_bo_address(native)+no,(uint32_t)nb};
    v[0]=direction?n:c;v[1]=direction?c:n;
    uint32_t groups[3]={((source->level[0].row_bytes+3)/4+7)/8,(source->level[0].blocks_y+7)/8,1};size_t dispatch_bytes=0;
    e=nn_cs_encode(d,owner->bos[0],bundle->gob,u,v,groups,&owner->bos[2],&dispatch_bytes);if(e)goto fail;
    e=nn_bo_alloc(d,1024,&owner->bos[3]);if(e)goto fail;
    uint32_t *w=nn_bo_map(owner->bos[3]);if(!w){e=ENOTSUP;goto fail;}memset(w,0,1024);size_t k=0,count=0;
    e=nn_order_words(nn_device_chip(d),nn_bo_address(owner->bos[3])+1020,NN_ORDER_FULL_CACHE|NN_ORDER_INVALIDATE,w,255,&count);if(e)goto fail;k+=count;
    ib(w,&k,owner->bos[2],dispatch_bytes);
    e=nn_order_words(nn_device_chip(d),0,NN_ORDER_INVALIDATE,w+k,255-k,&count);if(e)goto fail;k+=count;
    for(unsigned i=0;i<4;i++){e=nn_bo_cache(owner->bos[i],0,nn_bo_size(owner->bos[i]),0);if(e)goto fail;}
    owner->entry_bytes=k*4;return 0;
fail:
    {int cleanup=nn_tex_conversion_destroy(owner);return cleanup?cleanup:e;}
}
