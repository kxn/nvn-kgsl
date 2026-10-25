/* SPDX-License-Identifier: MIT. Native resource/view/copy family.
 * CP DWORD copies: Mesa fd6_emit.cc:1152-1173; CP_MEM_WRITE and addresses:
 * adreno_pm4.xml. No CPU image copy, tiling conversion, filtering or resolve.
 * Caller storage identity, offsets and GPUVAs are never replaced. */
#include <stdio.h>
#include "nvn_native/resource_access.h"
static void api_nvnTextureBuilderSetSize1D(NVNtextureBuilder *key,int width)
{
    struct object *o=find(key,TB); if (!o) return;
    o->texture.width=width; o->texture.height=o->texture.depth=1;
    o->texture.set|=TEX_WIDTH|TEX_HEIGHT;
}
static void api_nvnTextureBuilderSetPackagedTextureLayout(NVNtextureBuilder *key,const NVNpackagedTextureLayout *layout)
{
    struct object *o=find(key,TB); if (!o) return;
    o->texture_packaged=layout!=NULL; memset(o->packaged_layout,0,8);
    if (layout) memcpy(o->packaged_layout,layout->layout,8);
    /* Metadata is retained exactly. Initialize/GetStorageSize reject packaged
     * Tegra layouts; NULL explicitly restores the supported LINEAR profile. */
}
static void api_nvnTextureViewSetDefaults(NVNtextureView *key)
{ if (!key) { error(EINVAL); return; } memset(key,0,sizeof(*key)); }
static void view_set(NVNtextureView *key,int which,int a,int b)
{
    if (!key) { error(EINVAL); return; }
    struct resource_view v; int e=resource_view_read(key,&v); if (e) { error(e); return; }
    if (which<2 && (a<0 || b<0 || a>INT_MAX-b)) { error(EINVAL); return; }
    if (which==0) { v.levels=1;v.base_level=a;v.level_count=b; }
    if (which==1) { v.layers=1;v.base_layer=a;v.layer_count=b; }
    /* CPU view metadata does not select a native consumer. The SDK setter
     * stores the format enum; each consumer checks reinterpretation support. */
    if (which==2) { if ((unsigned)a>NVN_FORMAT_BGRA8_SRGB) { error(EINVAL);return; } v.format=1;v.format_value=a; }
    if (which==3) { if (a<NVN_TEXTURE_TARGET_1D || a>NVN_TEXTURE_TARGET_BUFFER) { error(EINVAL);return; } v.target=1;v.target_value=a; }
    resource_view_write(key,&v);
}
static void api_nvnTextureViewSetLevels(NVNtextureView *key,int a,int b) { view_set(key,0,a,b); }
static void api_nvnTextureViewSetLayers(NVNtextureView *key,int a,int b) { view_set(key,1,a,b); }
static void api_nvnTextureViewSetFormat(NVNtextureView *key,NVNformat f) { view_set(key,2,f,0); }
static void api_nvnTextureViewSetTarget(NVNtextureView *key,NVNtextureTarget t) { view_set(key,3,t,0); }
static NVNboolean api_nvnTextureViewGetLayers(const NVNtextureView *key,int *a,int *b)
{
    struct resource_view v; int e=resource_view_read(key,&v);
    if (!key || !a || !b || e) { error(e?e:EINVAL);return 0; }
    *a=v.base_layer; *b=v.layer_count; return v.layers;
}
static NVNboolean api_nvnTextureCompare(const NVNtexture *a,const NVNtexture *b)
{
    struct object *x=find(a,TEXTURE),*y=find(b,TEXTURE); if (!x || !y) return 0;
    const struct texture_desc *p=&x->texture,*q=&y->texture;
    int same_storage=x->bo==y->bo && x->offset==y->offset;
    if (!x->pool->external && !y->pool->external)
        same_storage=(uintptr_t)x->pool->storage+x->offset==(uintptr_t)y->pool->storage+y->offset;
    if (x->pool->external && y->pool->external) {
        struct nn_external_image_view u,v;
        if (!nn_external_image_view(x->pool->external,&u) && !nn_external_image_view(y->pool->external,&v))
            same_storage=u.backing_inode==v.backing_inode && u.backing_dev_major==v.backing_dev_major &&
                u.backing_dev_minor==v.backing_dev_minor && x->offset==y->offset;
    }
    return x->device==y->device && same_storage && x->size==y->size &&
        p->target==q->target && p->width==q->width && p->height==q->height && p->depth==q->depth &&
        p->levels==q->levels && p->samples==q->samples && p->format==q->format &&
        p->flags==q->flags && p->stride==q->stride && texture_sampling_equal(p,q);
}
struct resource_span { struct object *pool,*texture; size_t offset,pitch,row_bytes,rows; };
static size_t span_bytes(const struct resource_span *s) { return (s->rows-1)*s->pitch+s->row_bytes; }
#include "nvn_command_copy_types.inc"
static int texture_region(struct object *t,const NVNtextureView *view,const NVNcopyRegion *r,struct resource_span *out)
{
    if (!r) return EINVAL;
    struct texture_desc d; size_t size;
    int e=resolve_texture_view(t,view,&d); if (e) return e;
    if ((e=linear_texture_bytes(&d,&size))) return e;
    if (r->xoffset<0 || r->yoffset<0 || r->zoffset || r->width<1 || r->height<1 || r->depth!=1 ||
        r->xoffset>d.width || r->width>d.width-r->xoffset ||
        r->yoffset>d.height || r->height>d.height-r->yoffset) return EINVAL;
    unsigned cpp=linear_cpp(d.format);
    size_t rel=(size_t)r->yoffset*(size_t)d.stride+(size_t)r->xoffset*cpp;
    *out=(struct resource_span){t->pool,t,t->offset+rel,(size_t)d.stride,(size_t)r->width*cpp,(size_t)r->height};
    if (out->offset>t->pool->size || span_bytes(out)>t->pool->size-out->offset) return EINVAL;
    if ((out->offset|out->row_bytes|out->pitch)&3) return ENOTSUP;
    if (gpu_object_access(t,"texture copy/read/clear")) return ENOTSUP;
    return 0;
}
#include "canonical_copy_region.inc"
static int buffer_span(struct object *device,uint64_t address,size_t row,size_t rows,ptrdiff_t pitch,struct resource_span *out)
{
    if (!address || address%4 || !row || row%4 || !rows || pitch<0 || (pitch && ((size_t)pitch<row || pitch%4))) return EINVAL;
    size_t p=pitch?(size_t)pitch:row;
    if (rows-1>(SIZE_MAX-row)/p) return EOVERFLOW;
    size_t bytes=(rows-1)*p+row;
    struct object *pool=address_pool(device,address,bytes);
    if (!pool || pool->external) return ENOTSUP;
    *out=(struct resource_span){pool,NULL,(size_t)(address-nn_bo_address(pool->bo)),p,row,rows};
    return gpu_pool_access(pool,out->offset,bytes,"buffer copy/update");
}
/* Live full anonymous private mappings + caller lifetime contract. Imported
 * pages remain pinned. Distinct virtual addresses alone are never evidence.
 * Shared/file/device/unknown mappings are outside this first native subset. */
static int private_storage(const void *pointer,size_t bytes)
{
    uintptr_t begin=(uintptr_t)pointer,end;
    if (!begin || bytes>UINTPTR_MAX-begin) return EINVAL;
    end=begin+bytes; FILE *f=fopen("/proc/self/maps","r"); if (!f) return errno;
    char line[1024],perm[5]; unsigned long long a,b,offset,inode; unsigned major,minor;
    uintptr_t cursor=begin;
    while (fgets(line,sizeof(line),f)) {
        if (sscanf(line,"%llx-%llx %4s %llx %x:%x %llu",&a,&b,perm,&offset,&major,&minor,&inode)!=7) continue;
        if (a>cursor || b<=cursor) continue;
        if (memcmp(perm,"rw-p",4) || major || minor || inode) { fclose(f);return ENOTSUP; }
        cursor=b>end?end:(uintptr_t)b; if (cursor==end) break;
    }
    fclose(f);return cursor==end?0:ENOTSUP;
}
static int range_overlap(uint64_t a,size_t an,uint64_t b,size_t bn)
{ return a<=UINT64_MAX-an && b<=UINT64_MAX-bn && a<b+bn && b<a+an; }
static int spans_disjoint(const struct resource_span *s,const struct resource_span *d)
{
    size_t sn=span_bytes(s),dn=span_bytes(d);
    if (s->pool->bo==d->pool->bo)
        return range_overlap(s->offset,sn,d->offset,dn)?EINVAL:0;
    if (!s->pool->external && !d->pool->external) {
        int e=private_storage(s->pool->storage,s->pool->size); if (e) return e;
        e=private_storage(d->pool->storage,d->pool->size); if (e) return e;
        return range_overlap((uintptr_t)s->pool->storage+s->offset,sn,(uintptr_t)d->pool->storage+d->offset,dn)?EINVAL:0;
    }
    if (s->pool->external && d->pool->external) {
        struct nn_external_image_view a,b;
        int e=nn_external_image_view(s->pool->external,&a); if (e) return e;
        if ((e=nn_external_image_view(d->pool->external,&b))) return e;
        return a.backing_inode==b.backing_inode && a.backing_dev_major==b.backing_dev_major && a.backing_dev_minor==b.backing_dev_minor &&
            range_overlap(s->offset,sn,d->offset,dn)?EINVAL:0;
    }
    const struct object *p=s->pool->external?d->pool:s->pool;
    return private_storage(p->storage,p->size);
}
static int copy_job_encode(struct object *device,const struct resource_span *s,const struct resource_span *d,
                           const uint32_t *pattern,unsigned pattern_words,int direct,
                           nn_bo **out,size_t *bytes)
{
    *out=NULL; *bytes=0;
    if (nn_device_chip(device->gpu)!=0x06050002u) return ENOTSUP;
    if (!d || !d->row_bytes || d->row_bytes%4 || !d->rows || d->rows>16384 ||
        (s && (s->row_bytes!=d->row_bytes || s->rows!=d->rows))) return EINVAL;
    /* Bound packet/IB size. This DWORD route is correctness-oriented, not a
     * large-image performance substitute for a separately qualified blitter. */
    if (d->row_bytes>262144 || d->rows>262144/d->row_bytes) return E2BIG;
    if (!s && (!pattern || !pattern_words || pattern_words>4)) return EINVAL;
    size_t data_words=d->row_bytes/4, moves=data_words*d->rows;
    size_t words=32+(s?moves*6:(data_words+3*((data_words+16380)/16381))*d->rows);
    if (words>0xfffffu || words>(SIZE_MAX-8)/4) return E2BIG;
    nn_bo *bo=NULL; int e=nn_bo_alloc(device->gpu,words*4+4,&bo); if (e) return e;
    uint32_t *w=nn_bo_map(bo); if (!w) { e=ENOTSUP; goto fail; }
    memset(w,0,words*4+4); size_t n=0,k=0;
    e=nn_order_words(nn_device_chip(device->gpu),nn_bo_address(bo)+words*4,NN_ORDER_FULL_CACHE,w,words,&k); if (e) goto fail;
    n+=k;
    for (size_t y=0;y<d->rows;y++) {
        uint64_t dst=nn_bo_address(d->pool->bo)+d->offset+y*d->pitch;
        if (s) {
            uint64_t src=nn_bo_address(s->pool->bo)+s->offset+y*s->pitch;
            for (size_t x=0;x<data_words;x++) {
                uint64_t a=src+x*4,b=dst+x*4;
                w[n++]=nvn_pm4_packet7(NVN_CP_MEM_TO_MEM,5);w[n++]=0;
                w[n++]=(uint32_t)b;w[n++]=(uint32_t)(b>>32);w[n++]=(uint32_t)a;w[n++]=(uint32_t)(a>>32);
            }
        } else {
            /* Packet-7 payload count has 14 bits, so split wide rows. */
            for (size_t x=0;x<data_words;) {
                size_t amount=data_words-x; if (amount>16381) amount=16381;
                uint64_t a=dst+x*4;
                w[n++]=nvn_pm4_packet7(NVN_CP_MEM_WRITE,(uint16_t)(amount+2));w[n++]=(uint32_t)a;w[n++]=(uint32_t)(a>>32);
                for (size_t j=0;j<amount;j++) w[n++]=direct?pattern[x+j]:pattern[(x+j)%pattern_words];
                x+=amount;
            }
        }
    }
    e=nn_order_words(nn_device_chip(device->gpu),0,NN_ORDER_INVALIDATE,w+n,words-n,&k); if (e) goto fail;
    n+=k;
    e=nn_bo_cache(bo,0,words*4+4,0); if (e) goto fail;
    *out=bo;*bytes=n*4;return 0;
fail:
    /* Allocation never submitted; retain a BO on an unexpected free failure. */
    { int cleanup=nn_bo_destroy(bo); if (cleanup) { *out=bo; return cleanup; } }
    return e;
}
/* CP clear operates only on private native storage; the wrapper performs both
 * canonical conversion directions at enqueue, including partial-region restore.
 * No CPU texels are read or generated; only the uniform clear word is stored. */
static int bridge_materialize_clear(struct object *device,const struct late_draw *draw,nn_bo *native,
 const struct nn_tex_native_layout *n,nn_bo **out,size_t *bytes)
{
    struct object pool={.bo=native,.size=n->bytes};
    const NVNcopyRegion *r=&draw->clear_region;
    struct resource_span d={.pool=&pool,.offset=(size_t)r->yoffset*n->pitch+(size_t)r->xoffset*4,
        .pitch=n->pitch,.row_bytes=(size_t)r->width*4,.rows=(size_t)r->height};
    return copy_job_encode(device,NULL,&d,&draw->clear_word,1,0,out,bytes);
}
static void bridge_record_clear_native(struct object *command,struct object *target,const NVNcopyRegion *region,uint32_t word)
{
    struct recording *r=command->record;
    const struct nn_tex_bridge_bundle *bundle=command->device->texture_bridge;
    int e=gpu_object_access(target,"canonical ClearTexture");if(e)return;
    if(!bundle || !target->source_layout || !region ||
       target->texture.width!=(int)bundle->clear_layout.width || target->texture.height!=(int)bundle->clear_layout.height ||
       region->xoffset<0 || region->yoffset<0 || region->zoffset || region->width<1 || region->height<1 || region->depth!=1 ||
       region->xoffset>target->texture.width || region->width>target->texture.width-region->xoffset ||
       region->yoffset>target->texture.height || region->height>target->texture.height-region->yoffset){error(ENOTSUP);return;}
    if((e=bridge_target_arena_alias(target)) || (e=command_reserve(command,24))){error(e);return;}
    if(gpu_pool_access(r->pool,r->offset,r->capacity,"canonical clear command arena"))return;
    struct late_draw *draw=calloc(1,sizeof(*draw));if(!draw){error(ENOMEM);return;}
    size_t old=r->current->count;
    e=add_resource(r->current,target);if(!e)e=add_resource(r->current,target->pool);
    if(e){while(r->current->count>old)r->current->resources[--r->current->count]->refs--;free(draw);error(e);return;}
    *draw=(struct late_draw){.bridge_clear=1,.clear_word=word,.clear_region=*region,.target=target,
        .command_offset=r->logical_bytes+r->used-r->start};
    memset((char*)nn_bo_map(r->pool->bo)+r->offset+r->used,0,16);r->used+=16;
    draw->next=r->current->draws;r->current->draws=draw;
}
static void record_resource_job_native(struct object *command,const struct resource_span *s,const struct resource_span *d,
                                const uint32_t *pattern,unsigned pattern_words,int direct)
{
    struct recording *r=command->record;
    { int e=command_reserve(command,24); if (e) { error(e);return; } }
    if (gpu_pool_access(r->pool,r->offset,r->capacity,"copy command arena")) return;
    if (r->used>r->capacity || r->capacity-r->used<24) { error(ENOSPC);return; }
    /* Never allow this subset to rewrite a live command arena (including a
     * second import of its caller pages). Self-modifying IBs require a wholly
     * different qualified recording protocol. */
    for (struct object *o=objects;o;o=o->next) if (o->kind==COMMAND && o->record->pool) {
        struct object *p=o->record->pool;
        if (p->bo==d->pool->bo || (p->storage && d->pool->storage &&
            range_overlap((uintptr_t)p->storage,p->size,(uintptr_t)d->pool->storage+d->offset,span_bytes(d)))) { error(ENOTSUP);return; }
    }
    int e=s?spans_disjoint(s,d):0; if (e) { error(e);return; }
    struct job *job=calloc(1,sizeof(*job)); if (!job) { error(ENOMEM);return; }
    size_t bytes=0; e=copy_job_encode(command->device,s,d,pattern,pattern_words,direct,&job->bo,&bytes);
    if (e) { if (job->bo) { job->next=command->device->retired;command->device->retired=job;command->device->refs++; } else free(job);error(e);return; }
    size_t old=r->current->count;
    struct object *refs[]={s?s->pool:NULL,s?s->texture:NULL,d->pool,d->texture};
    for (unsigned i=0;i<4;i++) if (refs[i] && (e=add_resource(r->current,refs[i]))) break;
    if (e) {
        while (r->current->count>old) r->current->resources[--r->current->count]->refs--;
        job->next=command->device->retired;command->device->retired=job;command->device->refs++;reap_jobs(command->device);error(e);return;
    }
    uint64_t address=nn_bo_address(job->bo);
    uint32_t *w=(uint32_t *)((char *)nn_bo_map(r->pool->bo)+r->offset+r->used);
    w[0]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);w[1]=(uint32_t)address;w[2]=(uint32_t)(address>>32);w[3]=(uint32_t)(bytes/4);
    job->copy_source=s?s->texture:NULL;job->copy_destination=d->texture;
    job->copy_destination_pool=d->pool;job->copy_destination_offset=d->offset;job->copy_destination_bytes=span_bytes(d);
    r->used+=16; job->next=r->current->jobs;r->current->jobs=job;command->device->refs++;
}
#include "nvn_command_copy_cp_record.inc"
static void api_nvnCommandBufferSetCopyRowStride(NVNcommandBuffer *key,ptrdiff_t stride)
{
    struct object *o=recording_command(key);if(!o)return;
    /* SDK53.314465bb0 stores w1. Retain that signed32 metadata independently
     * of the alignment/range supported by a later native copy consumer. */
    uint32_t low=(uint32_t)stride;int32_t value;memcpy(&value,&low,sizeof(value));
    o->record->copy_row_stride=value;
}
static void api_nvnCommandBufferSetCopyImageStride(NVNcommandBuffer *key,ptrdiff_t stride)
{
    struct object *o=recording_command(key);if(!o)return;
    /* SDK53.314465bc0 has the same raw32 store, with no setter validation. */
    uint32_t low=(uint32_t)stride;int32_t value;memcpy(&value,&low,sizeof(value));
    o->record->copy_image_stride=value;
}
/* SDK53.314465bd0/465be0 load W0: the stored low word is zero-extended,
 * including values whose signed setter metadata is negative. Queries also
 * work outside recording; BeginRecording does not reset either value. */
static ptrdiff_t api_nvnCommandBufferGetCopyRowStride(const NVNcommandBuffer *key)
{ struct object *o=find(key,COMMAND);return o?(ptrdiff_t)(uint32_t)o->record->copy_row_stride:0; }
static ptrdiff_t api_nvnCommandBufferGetCopyImageStride(const NVNcommandBuffer *key)
{ struct object *o=find(key,COMMAND);return o?(ptrdiff_t)(uint32_t)o->record->copy_image_stride:0; }
static void api_nvnCommandBufferCopyBufferToBuffer(NVNcommandBuffer *key,NVNbufferAddress src,NVNbufferAddress dst,size_t bytes,int flags)
{
    struct object *o=recording_command(key); if (!o) return;
    if (flags) { error(ENOTSUP);return; }
    struct resource_span s,d;int e=buffer_span(o->device,src,bytes,1,0,&s);
    if (!e) e=buffer_span(o->device,dst,bytes,1,0,&d);
    if (e) {error(e);return;}record_resource_job(o,&s,&d,NULL,0,0);
}
#include "canonical_copy_record.inc"
#include "canonical_texture_copy.inc"
#include "nvn_command_copy_canonical_record.inc"
static void copy_buffer_texture(NVNcommandBuffer *key,NVNbufferAddress address,const NVNtexture *keyt,const NVNtextureView *view,const NVNcopyRegion *region,int flags,int to_texture)
{
    struct object *o=recording_command(key),*t=find(keyt,TEXTURE);if(!o||!t)return;
    if (flags || t->device!=o->device) { error(ENOTSUP);return; }
    if(t->source_layout){
#if NN_NVN_BLOCK_COPY
        int e=record_canonical_copy(o,t,view,region,address,to_texture);if(e)error(e);
#else
        error(ENOTSUP);
#endif
        return;
    }
    struct resource_span b,s;int e=texture_region(t,view,region,&s);
    if (!e) e=buffer_span(o->device,address,s.row_bytes,s.rows,o->record->copy_row_stride,&b);
    if (e) {error(e);return;}record_resource_job(o,to_texture?&b:&s,to_texture?&s:&b,NULL,0,0);
}
static void api_nvnCommandBufferCopyBufferToTexture(NVNcommandBuffer *k,NVNbufferAddress b,const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r,int f)
{ copy_buffer_texture(k,b,t,v,r,f,1); }
static void api_nvnCommandBufferCopyTextureToBuffer(NVNcommandBuffer *k,const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r,NVNbufferAddress b,int f)
{ copy_buffer_texture(k,b,t,v,r,f,0); }
static void api_nvnCommandBufferCopyTextureToTexture(NVNcommandBuffer *key,const NVNtexture *src,const NVNtextureView *sv,const NVNcopyRegion *sr,const NVNtexture *dst,const NVNtextureView *dv,const NVNcopyRegion *dr,int flags)
{
    struct object *o=recording_command(key),*s=find(src,TEXTURE),*d=find(dst,TEXTURE);if(!o||!s||!d)return;
    if (flags || s->device!=o->device || d->device!=o->device || s->texture.format!=d->texture.format) {error(ENOTSUP);return;}
    if(s->source_layout||d->source_layout){
#if NN_NVN_BLOCK_COPY
        int e=record_canonical_texture_copy(o,s,sv,sr,d,dv,dr);if(e)error(e);
#else
        error(ENOTSUP);
#endif
        return;
    }
    struct resource_span a,b;int e=texture_region(s,sv,sr,&a);if(!e)e=texture_region(d,dv,dr,&b);
    if (!e && (a.row_bytes!=b.row_bytes || a.rows!=b.rows)) e=ENOTSUP;
    if(e){error(e);return;}record_resource_job(o,&a,&b,NULL,0,0);
}
static int clear_pattern(int format,const void *color,int integer,uint32_t w[4],unsigned *count)
{
    if(!color)return EINVAL;
    unsigned channels=format==NVN_FORMAT_R32F || format==NVN_FORMAT_R32UI?1:
        format==NVN_FORMAT_RG32F || format==NVN_FORMAT_RG32UI?2:4;
    if(integer) {
        const uint32_t *u=color;
        if(format==NVN_FORMAT_RGBA8UI){for(unsigned i=0;i<4;i++)if(u[i]>255)return ENOTSUP;w[0]=u[0]|u[1]<<8|u[2]<<16|u[3]<<24;*count=1;return 0;}
        if(format!=NVN_FORMAT_R32UI && format!=NVN_FORMAT_RG32UI && format!=NVN_FORMAT_RGBA32UI)return ENOTSUP;
        memcpy(w,u,channels*4);*count=channels;return 0;
    }
    const float *f=color;
    if(format==NVN_FORMAT_RGBA8){w[0]=0;for(unsigned i=0;i<4;i++){if(!(f[i]>=0 && f[i]<=1))return ENOTSUP;w[0]|=(uint32_t)(f[i]*255.0f+0.5f)<<(i*8);}*count=1;return 0;}
    if(format!=NVN_FORMAT_R32F && format!=NVN_FORMAT_RG32F && format!=NVN_FORMAT_RGBA32F)return ENOTSUP;
    memcpy(w,f,channels*4);*count=channels;return 0;
}
static void clear_texture(NVNcommandBuffer *key,const NVNtexture *texture,const NVNtextureView *view,const NVNcopyRegion *region,const void *color,int mask,int integer)
{
    struct object *o=recording_command(key),*t=find(texture,TEXTURE);if(!o||!t)return;
    if(t->device!=o->device){error(EINVAL);return;}
    /* No CP read-modify-write is invented for partial channel masks. */
    if(mask!=0xf){error(ENOTSUP);return;}
    struct resource_span d;uint32_t pattern[4]={0};unsigned count=0;
    if(t->source_layout){
        struct texture_desc resolved;int e=resolve_texture_view(t,view,&resolved);
        if(!e)e=clear_pattern(resolved.format,color,integer,pattern,&count);
        if(e || count!=1){error(e?e:ENOTSUP);return;}bridge_record_clear(o,t,region,pattern[0]);return;
    }
    int e=texture_region(t,view,region,&d);if(!e)e=clear_pattern(t->texture.format,color,integer,pattern,&count);
    if(e){error(e);return;}record_resource_job(o,NULL,&d,pattern,count,0);
}
static void api_nvnCommandBufferClearTexture(NVNcommandBuffer *k,const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r,const float *c,int m)
{ clear_texture(k,t,v,r,c,m,0); }
static void api_nvnCommandBufferClearTextureui(NVNcommandBuffer *k,const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r,const uint32_t *c,int m)
{ clear_texture(k,t,v,r,c,m,1); }
static void api_nvnCommandBufferUpdateUniformBuffer(NVNcommandBuffer *key,NVNbufferAddress address,size_t buffer_size,ptrdiff_t offset,size_t bytes,const void *data)
{
    struct object *o=recording_command(key);if(!o)return;
    if(!data || !buffer_size || buffer_size>65536 || offset<0 || (size_t)offset>buffer_size || bytes>buffer_size-(size_t)offset || !bytes || bytes%4 || offset%4 || address%16 || address>UINT64_MAX-(size_t)offset){error(EINVAL);return;}
    struct resource_span d;int e=buffer_span(o->device,address,buffer_size,1,0,&d);
    if(e){error(e);return;}d.offset+=(size_t)offset;d.row_bytes=d.pitch=bytes;
    /* Snapshot caller bytes into the recorded CP_MEM_WRITE payload now. */
    record_resource_job(o,NULL,&d,data,1,1);
}
static void api_nvnCommandBufferDownsampleTextureView(NVNcommandBuffer *key,const NVNtexture *src,const NVNtextureView *sv,const NVNtexture *dst,const NVNtextureView *dv)
{
    if(!recording_command(key)||!find(src,TEXTURE)||!find(dst,TEXTURE))return;
    (void)sv;(void)dv;
    /* MSAA resolve requires a real multisample layout + blitter qualification.
     * A byte copy of single-sample images is not a downsample implementation. */
    error(ENOTSUP);
}
#include "texture_texels.inc"
struct readback_owner { struct readback_owner *next;struct object *texture,*device;nn_bo *commands,*destination;nn_queue *queue; };
static struct readback_owner *retained_readbacks;
static int readback_cleanup(struct readback_owner *r)
{
    int e=0;
    if(r->queue){e=nn_queue_destroy(r->queue);if(e)return e;r->queue=NULL;}
    if(r->commands){e=nn_bo_destroy(r->commands);if(e)return e;r->commands=NULL;}
    if(r->destination){e=nn_bo_destroy(r->destination);if(e)return e;r->destination=NULL;}
    r->texture->refs--;r->device->refs--;free(r);return 0;
}
static int read_source_idle(struct object *texture)
{
    struct object *source=texture->pool;
    if (!source->external) {
        int e=private_storage(source->storage,source->size);if(e)return e;
    }
    struct nn_external_image_view sv={0};
    if(source->external){int e=nn_external_image_view(source->external,&sv);if(e)return e;}
    for(struct object *pool=objects;pool;pool=pool->next)if(pool->kind==POOL && pool->device==texture->device) {
        int alias=pool->bo==source->bo;
        if(!source->external && !pool->external && pool->storage)
            alias|=range_overlap((uintptr_t)source->storage,source->size,(uintptr_t)pool->storage,pool->size);
        if(source->external && pool->external){
            struct nn_external_image_view v;int e=nn_external_image_view(pool->external,&v);if(e)return e;
            alias|=sv.backing_inode==v.backing_inode && sv.backing_dev_major==v.backing_dev_major && sv.backing_dev_minor==v.backing_dev_minor;
        }
        if(alias){int e=nn_bo_idle(pool->bo);if(e)return e;}
    }
    return 0;
}
static void read_texels(const NVNtexture *key,const NVNtextureView *view,const NVNcopyRegion *region,void *destination,ptrdiff_t row,ptrdiff_t image)
{
    struct object *t=find(key,TEXTURE);if(!t)return;
    struct resource_span s;int e=texture_region(t,view,region,&s);if(e){error(e);return;}
    if(!destination || (uintptr_t)destination%4 || row<0 || row%4 || image<0 || image%4 || (row && (size_t)row<s.row_bytes)){error(EINVAL);return;}
    size_t pitch=row?(size_t)row:s.row_bytes;
    if(s.rows-1>(SIZE_MAX-s.row_bytes)/pitch){error(EOVERFLOW);return;}
    size_t span=(s.rows-1)*pitch+s.row_bytes;
    size_t page=nn_device_page(t->device->gpu);uintptr_t p=(uintptr_t)destination;
    if(page!=4096 || span>UINTPTR_MAX-p || p+span>UINTPTR_MAX-(page-1)){error(EINVAL);return;}
    uintptr_t begin=p&~(uintptr_t)(page-1),end=(p+span+page-1)&~(uintptr_t)(page-1);
    e=private_storage((void *)begin,end-begin);if(e){error(e);return;}
    /* CPU destinations must not alias ANY live imported pool, even through an
     * earlier Buffer or another texture. The whole imported page envelope is
     * protected; unknown external mappings fail private_storage above. */
    for(struct object *pool=objects;pool;pool=pool->next)if(pool->kind==POOL && pool->storage &&
        range_overlap(begin,end-begin,(uintptr_t)pool->storage,pool->size)){error(EINVAL);return;}
    /* The caller synchronizes the producer queue first. An unrelated queue
     * cannot order those writes. Busy/uncertain BOs reject before any import
     * or readback submit; this function never waits arbitrary device queues. */
    e=read_source_idle(t);if(e){error(e);return;}
    struct readback_owner *r=calloc(1,sizeof(*r));if(!r){error(ENOMEM);return;}
    r->texture=t;r->device=t->device;t->refs++;t->device->refs++;
    int submitted=0;
    e=nn_bo_import(t->device->gpu,(void *)begin,end-begin,&r->destination);
    if(!e)e=nn_bo_cache(r->destination,0,end-begin,0);
    struct object temporary={.bo=r->destination};
    struct resource_span d={.pool=&temporary,.offset=p-begin,.pitch=pitch,.row_bytes=s.row_bytes,.rows=s.rows};
    size_t bytes=0;
    if(!e)e=copy_job_encode(t->device,&s,&d,NULL,0,0,&r->commands,&bytes);
    if(!e)e=nn_queue_create(t->device->gpu,&r->queue);
    uint32_t timestamp=0;nn_bo *refs[]={t->bo,r->destination};
    if(!e){submitted=1;e=nn_queue_submit(r->queue,r->commands,0,bytes,refs,2,&timestamp);}
    if(!e)e=nn_queue_finish(r->queue,1000);
    if(!e)e=nn_bo_cache(r->destination,p-begin,span,1);
    if(e && submitted){r->next=retained_readbacks;retained_readbacks=r;error(e);return;}
    int cleanup=readback_cleanup(r);
    if(cleanup){r->next=retained_readbacks;retained_readbacks=r;}
    error(e?e:cleanup);
}
static void api_nvnTextureReadTexels(const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r,void *p)
{struct object *o=find(t,TEXTURE);if(o&&o->source_layout)error(canonical_texel_transfer(o,v,r,p,0,0,0));else if(o)read_texels(t,v,r,p,0,0);}
static void api_nvnTextureReadTexelsStrided(const NVNtexture *t,const NVNtextureView *v,const NVNcopyRegion *r,void *p,ptrdiff_t row,ptrdiff_t image)
{struct object *o=find(t,TEXTURE);if(o&&o->source_layout)error(canonical_texel_transfer(o,v,r,p,row,image,0));else if(o)read_texels(t,v,r,p,row,image);}
