/* SPDX-License-Identifier: MIT. CPU metadata/owners only, no codec/submit. */
#include "nvn_native/texture_volume_shadow.h"
#include "nvn_native/resource_access.h"
#include <errno.h>
#include <string.h>
static int source(const struct nn_tex_source_layout *s)
{
 int e=nn_tex_source_validate(s);if(e)return e;
 if(s->target!=2||s->linear||s->layers!=1||!s->volume_depth||
    (s->format!=37&&s->format!=0x29&&(s->format<0x41||s->format>0x44)))return ENOTSUP;
 return 0;
}
int nn_volume_shadow_plan(const struct nn_tex_source_layout *s,uint32_t bl,uint32_t nl,struct nn_volume_shadow_plan *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=source(s);if(e)return e;
 struct nn_volume_shadow_plan p={0};p.descriptor=(struct nn_fdl_volume_desc){.schema=1,.chip=0x06050002,.format=s->format,
  .width=s->level[0].width,.height=s->level[0].height,.depth=s->volume_depth,.levels=s->levels,.base_level=bl,.level_count=nl,.usage=NN_FDL_SAMPLED};
 if((e=nn_fdl_volume(&p.descriptor,&p.native)))return e;
 if(p.native.block_width!=s->block_w||p.native.block_height!=s->block_h||p.native.bytes_per_block!=s->bytes_per_block)return EINVAL;
 struct nn_tex_volume_layout v;if((e=nn_tex_source_volume(s,&v)))return e;
 for(unsigned i=0;i<s->levels;i++){
  const struct nn_tex_source_level *a=s->level+i;const struct nn_fdl_volume_level *n=p.native.level+i;
  if(a->width!=n->width||a->height!=n->height||s->level_depths[i]!=n->depth||n->offset%4||n->slice_stride>UINT32_MAX)return EINVAL;
  struct nn_volume_copy_region r={.level=i,.slices=n->depth,.row_bytes=a->row_bytes,.rows=a->blocks_y,
   .linear_row_stride=n->pitch,.linear_image_stride=(uint32_t)n->slice_stride,.direction=0};
  if((e=nn_volume_copy_plan(&v,&r,p.copy+i)))return e;
  if(n->offset>p.native.required_span||p.copy[i].linear_bytes>p.native.required_span-n->offset)return EINVAL;
 }
 p.count=s->levels;*out=p;return 0;
}
static size_t owned(const struct nn_volume_shadow *s,nn_bo *out[2+2*NN_FDL_LEVELS])
{
 size_t n=0;for(unsigned i=0;i<NN_FDL_LEVELS;i++)if(s->dispatch[i])out[n++]=s->dispatch[i];
 for(unsigned i=0;i<NN_FDL_LEVELS;i++)if(s->uniform[i])out[n++]=s->uniform[i];
 if(s->code)out[n++]=s->code;
 if(s->storage)out[n++]=s->storage;
 return n;
}
int nn_volume_shadow_destroy(struct nn_volume_shadow *s)
{
 if(!s)return EINVAL;
 s->ready=0;nn_bo *bos[2+2*NN_FDL_LEVELS];size_t n=owned(s,bos);
 for(size_t i=0;i<n;i++){
  if(bos[i]==s->canonical)return EINVAL;
  for(size_t j=0;j<i;j++)if(bos[i]==bos[j])return EINVAL;
  int e=nn_bo_idle(bos[i]);if(e)return e;
 }
 for(unsigned i=0;i<NN_FDL_LEVELS;i++)if(s->dispatch[i]){int e=nn_bo_destroy(s->dispatch[i]);if(e)return e;s->dispatch[i]=NULL;}
 for(unsigned i=0;i<NN_FDL_LEVELS;i++)if(s->uniform[i]){int e=nn_bo_destroy(s->uniform[i]);if(e)return e;s->uniform[i]=NULL;}
 if(s->code){int e=nn_bo_destroy(s->code);if(e)return e;s->code=NULL;}
 if(s->storage){int e=nn_bo_destroy(s->storage);if(e)return e;s->storage=NULL;}
 memset(s,0,sizeof(*s));return 0;
}
int nn_volume_shadow_create(nn_device *d,const struct nn_cs_image *k,const struct nn_tex_source_layout *s,
 const struct nn_volume_shadow_plan *p,const struct nn_block_copy_range *c,struct nn_volume_shadow *out)
{
 if(!out)return EINVAL;
 nn_bo *occupied[2+2*NN_FDL_LEVELS];if(owned(out,occupied)||out->canonical||out->count||out->ready)return EBUSY;
 if(!d||nn_device_chip(d)!=0x06050002||!k||!p||!c||!c->bo)return EINVAL;
 struct nn_volume_shadow_plan expected;int e=nn_volume_shadow_plan(s,p->descriptor.base_level,p->descriptor.level_count,&expected);if(e)return e;
 if(memcmp(p,&expected,sizeof(expected)))return EINVAL;
 uint64_t base=nn_bo_address(c->bo);
 if(!base||c->owner_offset>nn_bo_size(c->bo)||c->owner_bytes>nn_bo_size(c->bo)-c->owner_offset||c->offset<c->owner_offset||
    c->offset-c->owner_offset>c->owner_bytes||s->payload_bytes>c->owner_bytes-(c->offset-c->owner_offset)||
    c->offset>UINT64_MAX-base||s->payload_bytes>UINT64_MAX-base-c->offset)return EINVAL;
 uint64_t ca=base+c->offset;
 if(ca%4||(ca&~UINT64_C(63))<base+c->owner_offset||ca+s->payload_bytes-1>=(UINT64_C(1)<<49))return EINVAL;
 if(k->schema!=1||k->chip!=0x06050002||k->local[0]!=8||k->local[1]!=8||k->local[2]!=1||k->ssbo_slots!=2||
    !k->commands||!k->command_words||k->command_words>1024||!k->patches||!k->patch_count||!k->code||!k->code_bytes||k->code_bytes>65536)return ENOTSUP;
 out->canonical=c->bo;out->canonical_offset=c->offset;out->canonical_bytes=(size_t)s->payload_bytes;out->descriptor=p->descriptor;out->view=p->native;out->count=p->count;
 if((e=nn_bo_alloc(d,(size_t)p->native.required_span,&out->storage))||(e=nn_bo_alloc(d,k->code_bytes,&out->code)))goto fail;
 /* The only storage initialization is GPU raw-block dispatch, not a CPU map. */
 struct nn_fdl_volume_desc actual=p->descriptor;actual.address=nn_bo_address(out->storage);
 if((e=nn_fdl_volume(&actual,&out->view)))goto fail;
 if(out->storage==out->canonical||out->code==out->canonical||out->storage==out->code||out->view.required_span>nn_bo_size(out->storage)){e=EINVAL;goto fail;}
 /* A known GPUVA overlap is a rejection, never positive physical-alias proof. */
 uint64_t na=nn_bo_address(out->storage);
 if(ca<na+out->view.required_span&&na<ca+s->payload_bytes){e=EINVAL;goto fail;}
 void *code=nn_bo_map(out->code);if(!code){e=ENOTSUP;goto fail;}memcpy(code,k->code,k->code_bytes);
 if((e=nn_bo_cache(out->code,0,k->code_bytes,0)))goto fail;
 struct nn_tex_volume_layout volume;if((e=nn_tex_source_volume(s,&volume)))goto fail;
 for(unsigned i=0;i<p->count;i++){
  if((e=nn_bo_alloc(d,64,out->uniform+i)))goto fail;
  void *u=nn_bo_map(out->uniform[i]);if(!u){e=ENOTSUP;goto fail;}
  struct nn_block_copy_range n={out->storage,0,nn_bo_size(out->storage),(size_t)p->native.level[i].offset},uniform={out->uniform[i],0,nn_bo_size(out->uniform[i]),0};
  struct nn_volume_copy_binding b;
  if((e=nn_volume_copy_bind(d,k,&volume,p->copy+i,c,&n,&uniform,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)))goto fail;
  memcpy(u,&b.args,64);
  if((e=nn_bo_cache(out->uniform[i],0,64,0))||(e=nn_cs_encode(d,out->code,k,b.ubos,b.ssbos,b.groups,out->dispatch+i,out->dispatch_bytes+i)))goto fail;
  if((e=nn_bo_cache(out->dispatch[i],0,nn_bo_size(out->dispatch[i]),0)))goto fail;
 }
 out->ready=1;return 0;
fail:
 (void)nn_volume_shadow_destroy(out);return e;
}
static int valid(const struct nn_volume_shadow *s)
{
 if(!s||!s->ready||!s->count||s->count>NN_FDL_LEVELS||!s->canonical||!s->storage||!s->code||s->count!=s->view.levels)return EINVAL;
 nn_bo *bos[2+2*NN_FDL_LEVELS];size_t total=owned(s,bos);
 if(total!=2+2*s->count)return EINVAL;
 for(size_t i=0;i<total;i++){if(bos[i]==s->canonical)return EINVAL;for(size_t j=0;j<i;j++)if(bos[i]==bos[j])return EINVAL;}
 struct nn_fdl_volume_desc d=s->descriptor;d.address=nn_bo_address(s->storage);struct nn_fdl_volume_view v;int e=nn_fdl_volume(&d,&v);if(e)return e;
 if(memcmp(&v,&s->view,sizeof(v))||v.required_span>nn_bo_size(s->storage))return EINVAL;
 for(unsigned i=0;i<s->count;i++)if(!s->uniform[i]||!s->dispatch[i]||!s->dispatch_bytes[i]||s->dispatch_bytes[i]%4||s->dispatch_bytes[i]>nn_bo_size(s->dispatch[i]))return EINVAL;
 return 0;
}
int nn_volume_shadow_resources(const struct nn_volume_shadow *s,nn_bo **out,size_t capacity,size_t *count)
{
 if(count)*count=0;
 if(!out||!count)return EINVAL;
 int e=valid(s);if(e)return e;size_t n=3+2*s->count;if(capacity<n)return ENOSPC;
 out[0]=s->canonical;out[1]=s->storage;out[2]=s->code;
 for(unsigned i=0;i<s->count;i++){out[3+2*i]=s->uniform[i];out[4+2*i]=s->dispatch[i];}
 *count=n;return 0;
}
int nn_volume_shadow_dispatches(const struct nn_volume_shadow *s,struct nn_volume_shadow_dispatch *out,size_t capacity,size_t *count)
{
 if(count)*count=0;
 if(!out||!count)return EINVAL;
 int e=valid(s);if(e)return e;if(capacity<s->count)return ENOSPC;
 for(unsigned i=0;i<s->count;i++)out[i]=(struct nn_volume_shadow_dispatch){s->dispatch[i],s->dispatch_bytes[i]};
 *count=s->count;return 0;
}
int nn_volume_shadow_descriptor(const struct nn_volume_shadow *s,uint32_t out[16])
{
 if(!out)return EINVAL;
 memset(out,0,64);int e=valid(s);if(e)return e;memcpy(out,s->view.descriptor,64);return 0;
}
