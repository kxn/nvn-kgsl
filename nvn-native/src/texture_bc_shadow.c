/* SPDX-License-Identifier: MIT. No pixel codec, compiler, or submission. */
#include "nvn_native/texture_bc_shadow.h"
#include <errno.h>
#include <string.h>
#include <nvn.h>
static int source(const struct nn_tex_source_layout *s)
{
 int e=nn_tex_source_validate(s);if(e)return e;
 if(s->linear||s->format<0x41||s->format>0x48||(s->target!=1&&s->target!=4)||
    s->block_w!=4||s->block_h!=4||s->bytes_per_block!=((s->format<0x45?s->format:s->format-4)<0x43?8u:16u))return ENOTSUP;
 return 0;
}
int nn_bc_shadow_plan(const struct nn_tex_source_layout *s,uint32_t bl,uint32_t nl,
 uint32_t ba,uint32_t na,struct nn_bc_shadow_plan *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=source(s);if(e)return e;
 struct nn_bc_shadow_plan p={0};
 p.descriptor=(struct nn_fdl_image_desc){.schema=1,.chip=0x06050002,.format=s->format,.target=s->target,
  .width=s->level[0].width,.height=s->level[0].height,.levels=s->levels,.layers=s->layers,
  .base_level=bl,.level_count=nl,.base_layer=ba,.layer_count=na,.usage=NN_FDL_SAMPLED};
 if((e=nn_fdl_image(&p.descriptor,&p.native)))return e;
 if(p.native.block_width!=s->block_w||p.native.block_height!=s->block_h||
    p.native.bytes_per_block!=s->bytes_per_block||p.native.color_target)return EINVAL;
 for(unsigned i=0;i<s->levels;i++){
  const struct nn_tex_source_level *a=s->level+i;const struct nn_fdl_level *n=p.native.level+i;
  if(a->width!=n->width||a->height!=n->height||n->offset%4)return EINVAL;
  struct nn_block_copy_region r={.level=i,.layers=s->layers,.row_bytes=a->row_bytes,.rows=a->blocks_y,
    .linear_row_stride=n->pitch,.linear_image_stride=n->layer_stride,.direction=0};
  if((e=nn_block_copy_plan(s,&r,p.copy+i)))return e;
  if(n->offset>p.native.required_span||p.copy[i].linear_bytes>p.native.required_span-n->offset)return EINVAL;
 }
 p.count=s->levels;*out=p;return 0;
}
int nn_bc_shadow_destroy(struct nn_bc_shadow *s)
{
 if(!s)return EINVAL;
 s->ready=0;
 /* Stop at the first failed destroy. All remaining handles remain reachable. */
 for(unsigned i=0;i<NN_FDL_LEVELS;i++)if(s->dispatch[i]){
  int e=nn_bo_destroy(s->dispatch[i]);if(e)return e;s->dispatch[i]=NULL;
 }
 for(unsigned i=0;i<NN_FDL_LEVELS;i++)if(s->uniform[i]){
  int e=nn_bo_destroy(s->uniform[i]);if(e)return e;s->uniform[i]=NULL;
 }
 if(s->code){int e=nn_bo_destroy(s->code);if(e)return e;s->code=NULL;}
 if(s->storage){int e=nn_bo_destroy(s->storage);if(e)return e;s->storage=NULL;}
 memset(s,0,sizeof(*s));return 0;
}
int nn_bc_shadow_create(nn_device *d,const struct nn_cs_image *kernel,const struct nn_tex_source_layout *s,
 const struct nn_bc_shadow_plan *p,const struct nn_block_copy_range *canonical,struct nn_bc_shadow *out)
{
 if(!out)return EINVAL;
 /* Never overwrite a live/failed owner. This is also checked before preflight. */
 if(out->storage||out->code||out->canonical||out->count||out->ready)return EBUSY;
 for(unsigned i=0;i<NN_FDL_LEVELS;i++)if(out->uniform[i]||out->dispatch[i])return EBUSY;
 if(!d||nn_device_chip(d)!=0x06050002||!kernel||!p||!canonical||!canonical->bo)return EINVAL;
 struct nn_bc_shadow_plan expected;int e=nn_bc_shadow_plan(s,p->descriptor.base_level,p->descriptor.level_count,
   p->descriptor.base_layer,p->descriptor.layer_count,&expected);if(e)return e;
 if(memcmp(&expected,p,sizeof(expected)))return EINVAL;
 /* Validate the full canonical window before the first allocation, including
  * the original SSBO aligned-down descriptor prefix. No public map/cache. */
 uint64_t ca=nn_bo_address(canonical->bo);
 if(!ca||canonical->owner_offset>nn_bo_size(canonical->bo)||canonical->owner_bytes>nn_bo_size(canonical->bo)-canonical->owner_offset||
    canonical->offset<canonical->owner_offset||canonical->offset-canonical->owner_offset>canonical->owner_bytes||
    s->payload_bytes>canonical->owner_bytes-(canonical->offset-canonical->owner_offset)||
    canonical->offset>UINT64_MAX-ca||s->payload_bytes>UINT64_MAX-ca-canonical->offset)return EINVAL;
 ca+=canonical->offset;
 if(ca%4||(ca&~UINT64_C(63))<nn_bo_address(canonical->bo)+canonical->owner_offset||
    ca+s->payload_bytes-1>=(UINT64_C(1)<<49))return EINVAL;
 if(kernel->schema!=1||kernel->chip!=0x06050002||kernel->local[0]!=8||kernel->local[1]!=8||kernel->local[2]!=1||
    kernel->ssbo_slots!=2||!kernel->commands||!kernel->command_words||!kernel->code||!kernel->code_bytes)return ENOTSUP;
 out->canonical=canonical->bo;out->canonical_offset=canonical->offset;out->canonical_bytes=(size_t)s->payload_bytes;
 out->count=p->count;out->view=p->native;
 if((e=nn_bo_alloc(d,(size_t)p->native.required_span,&out->storage))||
    (e=nn_bo_alloc(d,kernel->code_bytes,&out->code)))goto fail;
 /* No native storage map: allocation padding is not part of the sampled
  * logical image. Raw-block kernel copies all active blocks, including edges. */
 struct nn_fdl_image_desc actual=p->descriptor;actual.address=nn_bo_address(out->storage);
 if((e=nn_fdl_image(&actual,&out->view)))goto fail;
 if(out->view.required_span>nn_bo_size(out->storage)){e=EINVAL;goto fail;}
 void *code=nn_bo_map(out->code);if(!code){e=ENOTSUP;goto fail;}
 memcpy(code,kernel->code,kernel->code_bytes);
 if((e=nn_bo_cache(out->code,0,kernel->code_bytes,0)))goto fail;
 for(unsigned i=0;i<p->count;i++){
  if((e=nn_bo_alloc(d,64,out->uniform+i)))goto fail;
  void *u=nn_bo_map(out->uniform[i]);if(!u){e=ENOTSUP;goto fail;}
  struct nn_block_copy_range native={out->storage,0,nn_bo_size(out->storage),(size_t)p->native.level[i].offset};
  struct nn_block_copy_range uniform={out->uniform[i],0,nn_bo_size(out->uniform[i]),0};
  struct nn_block_copy_binding binding;
  /* Fresh private allocation identity supplies physical disjointness, unlike
   * merely comparing two caller-imported BO GPU addresses. */
  if((e=nn_block_copy_bind(d,kernel,s,p->copy+i,canonical,&native,&uniform,NN_BLOCK_COPY_DISJOINT_PROVEN,&binding)))goto fail;
  memcpy(u,&binding.args,64);
  if((e=nn_bo_cache(out->uniform[i],0,64,0))||
     (e=nn_cs_encode(d,out->code,kernel,binding.ubos,binding.ssbos,binding.groups,out->dispatch+i,out->dispatch_bytes+i)))goto fail;
  if((e=nn_bo_cache(out->dispatch[i],0,nn_bo_size(out->dispatch[i]),0)))goto fail;
 }
 out->ready=1;return 0;
fail:
 /* Even if cleanup fails, every BO is reachable through out. The original
  * failure is returned; the caller must inspect ownership, not just errno. */
 (void)nn_bc_shadow_destroy(out);return e;
}
int nn_bc_shadow_resources(const struct nn_bc_shadow *s,nn_bo **out,size_t capacity,size_t *count)
{
 if(count)*count=0;
 if(!s||!out||!count||!s->ready||!s->count||s->count>NN_FDL_LEVELS||!s->storage||!s->code||!s->canonical)return EINVAL;
 size_t n=3+2*s->count;if(capacity<n)return ENOSPC;
 for(unsigned i=0;i<s->count;i++)if(!s->uniform[i]||!s->dispatch[i]||!s->dispatch_bytes[i]||s->dispatch_bytes[i]%4||
   s->dispatch_bytes[i]>nn_bo_size(s->dispatch[i]))return EINVAL;
 out[0]=s->canonical;out[1]=s->storage;out[2]=s->code;
 for(unsigned i=0;i<s->count;i++){out[3+2*i]=s->uniform[i];out[4+2*i]=s->dispatch[i];}
 *count=n;return 0;
}
int nn_bc_shadow_descriptor(const struct nn_bc_shadow *s,uint32_t out[16])
{
 if(!out)return EINVAL;
 memset(out,0,64);if(!s||!s->ready||!s->storage)return EINVAL;
 memcpy(out,s->view.descriptor,64);return 0;
}
static struct nn_fdl_image_desc view_desc(const struct nn_fdl_image_view *v)
{
 return (struct nn_fdl_image_desc){.schema=1,.chip=v->chip,.format=v->format,.target=v->target,
  .width=v->width,.height=v->height,.levels=v->levels,.layers=v->layers,.base_level=v->base_level,
  .level_count=v->level_count,.base_layer=v->base_layer,.layer_count=v->layer_count,.usage=NN_FDL_SAMPLED};
}
int nn_bc_sampled_validate(const struct nn_fdl_image_view *v)
{
 if(!v||v->schema!=1||v->chip!=0x06050002||v->format<0x41||v->format>0x48||v->target!=1||
    v->layers!=1||!v->level_count||v->base_layer||v->layer_count!=1)return ENOTSUP;
 struct nn_fdl_image_desc d=view_desc(v);struct nn_fdl_image_view expected;int e=nn_fdl_image(&d,&expected);
 if(e)return e;
 return memcmp(v,&expected,sizeof(expected))?EINVAL:0;
}
int nn_bc_sampled_descriptor(const struct nn_fdl_image_view *v,nn_bo *bo,size_t offset,size_t bytes,uint32_t out[16])
{
 if(!out)return EINVAL;
 memset(out,0,64);int e=nn_bc_sampled_validate(v);if(e)return e;
 if(!bo||offset>nn_bo_size(bo)||bytes>nn_bo_size(bo)-offset||v->required_span>bytes)return EINVAL;
 uint64_t a=nn_bo_address(bo);if(!a||offset>UINT64_MAX-a||bytes>UINT64_MAX-a-offset)return EOVERFLOW;
 struct nn_fdl_image_desc d=view_desc(v);d.address=a+offset;struct nn_fdl_image_view actual;
 if((e=nn_fdl_image(&d,&actual)))return e;
 memcpy(out,actual.descriptor,64);return 0;
}
int nn_bc_sampled_sampler(const struct nn_fdl_image_view *v,const struct nn_sampler_state *s,
 const struct nn_sampler_layout *l,uint32_t out[4])
{
 if(!out)return EINVAL;
 memset(out,0,16);int e=nn_bc_sampled_validate(v);if(e)return e;
 if(!s||!l||l->width!=v->view_width||l->height!=v->view_height||l->levels!=v->level_count||l->samples!=1||
    l->dimensions!=2||l->normalized!=1||l->depth||l->integer||s->min_filter!=NVN_MIN_FILTER_NEAREST||
    s->mag_filter!=NVN_MAG_FILTER_NEAREST||s->wrap[0]!=NVN_WRAP_MODE_CLAMP_TO_EDGE||
    s->wrap[1]!=NVN_WRAP_MODE_CLAMP_TO_EDGE||s->wrap[2]!=NVN_WRAP_MODE_CLAMP_TO_EDGE)return ENOTSUP;
 return nn_sampler_state_encode(v->chip,s,l,out);
}
