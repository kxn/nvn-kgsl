/* SPDX-License-Identifier: MIT. Existing raw-block CS + actual A2D only. */
#include "nvn_native/depth_shadow.h"
#include "nvn_native/resource_access.h"
#include <errno.h>
#include <string.h>
int nn_depth_shadow_plan(const struct nn_tex_source_layout*s,uint32_t level,uint32_t layer,struct nn_depth_shadow_plan*out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=nn_tex_source_validate(s);if(e)return e;
 if(s->linear||(s->target!=1&&s->target!=4)||s->format<0x32||s->format>0x34||s->block_w!=1||s->block_h!=1)return ENOTSUP;
 if(level>=s->levels||layer>=s->layers)return EINVAL;
 struct nn_depth_shadow_plan p={0};p.attachment=(struct nn_depth_fdl_desc){.schema=1,.chip=0x06050002,.format=s->format,
  .target=s->target,.width=s->level[0].width,.height=s->level[0].height,.levels=s->levels,.layers=s->layers,
  .base_level=level,.base_layer=layer,.roles=NN_DEPTH_ATTACHMENT};
 if((e=nn_depth_a2d(&p.attachment,0,&p.aliases)))return e;
 const struct nn_tex_source_level*m=s->level+level;
 if(p.aliases.tiled.width!=m->width||p.aliases.tiled.height!=m->height||p.aliases.linear.bytes>UINT32_MAX)return EINVAL;
 for(unsigned i=0;i<2;i++){
  struct nn_block_copy_region r={.level=level,.base_layer=layer,.layers=1,.row_bytes=m->row_bytes,.rows=m->blocks_y,
   .linear_row_stride=p.aliases.linear.pitch,.linear_image_stride=(uint32_t)p.aliases.linear.bytes,.direction=i};
  if((e=nn_block_copy_plan(s,&r,p.copy+i)))return e;
  if(p.copy[i].linear_bytes>p.aliases.linear.bytes)return EINVAL;
 }
 *out=p;return 0;
}
static size_t owned(struct nn_depth_shadow*s,nn_bo**out)
{
 size_t n=0;for(unsigned i=0;i<2;i++)if(s->a2d[i].job)out[n++]=s->a2d[i].job;
 for(unsigned i=0;i<2;i++)if(s->copy[i].job)out[n++]=s->copy[i].job;
 for(unsigned i=0;i<2;i++)if(s->uniform[i])out[n++]=s->uniform[i];
 if(s->code)out[n++]=s->code;
 if(s->linear.storage)out[n++]=s->linear.storage;
 if(s->attachment.storage)out[n++]=s->attachment.storage;
 return n;
}
int nn_depth_shadow_destroy(struct nn_depth_shadow*s)
{
 if(!s)return EINVAL;
 nn_bo*bos[9];size_t n=owned(s,bos);for(size_t i=0;i<n;i++){int e=nn_bo_idle(bos[i]);if(e)return e;}
 s->ready=0;
 for(unsigned i=0;i<2;i++)if(s->a2d[i].job){int e=nn_bo_destroy(s->a2d[i].job);if(e)return e;s->a2d[i].job=NULL;s->a2d[i].bytes=0;}
 for(unsigned i=0;i<2;i++)if(s->copy[i].job){int e=nn_bo_destroy(s->copy[i].job);if(e)return e;s->copy[i].job=NULL;s->copy[i].bytes=0;}
 for(unsigned i=0;i<2;i++)if(s->uniform[i]){int e=nn_bo_destroy(s->uniform[i]);if(e)return e;s->uniform[i]=NULL;}
 if(s->code){int e=nn_bo_destroy(s->code);if(e)return e;s->code=NULL;}
 int e=nn_depth_readback_destroy(&s->linear);if(e)return e;
 if((e=nn_depth_attachment_destroy(&s->attachment)))return e;
 memset(s,0,sizeof(*s));return 0;
}
static int canonical_range(const struct nn_tex_source_layout*s,const struct nn_block_copy_range*c)
{
 if(!c||!c->bo||c->owner_offset>nn_bo_size(c->bo)||c->owner_bytes>nn_bo_size(c->bo)-c->owner_offset||
  c->offset<c->owner_offset||c->offset-c->owner_offset>c->owner_bytes||s->payload_bytes>c->owner_bytes-(c->offset-c->owner_offset))return EINVAL;
 uint64_t a=nn_bo_address(c->bo);if(!a||c->offset>UINT64_MAX-a||s->payload_bytes>UINT64_MAX-a-c->offset)return EOVERFLOW;
 a+=c->offset;
 if(a%4||(a&~UINT64_C(63))<nn_bo_address(c->bo)+c->owner_offset||a+s->payload_bytes-1>=(UINT64_C(1)<<49))return EINVAL;
 return 0;
}
int nn_depth_shadow_create(nn_device*d,const struct nn_cs_image*k,const struct nn_pipeline_platform*platform,
 const struct nn_tex_source_layout*source,const struct nn_depth_shadow_plan*p,const struct nn_block_copy_range*c,size_t capacity,struct nn_depth_shadow*out)
{
 if(!out)return EINVAL;
 nn_bo*bos[9];if(out->ready||out->canonical.bo||owned(out,bos))return EBUSY;
 if(!d||!p)return EINVAL;
 if(capacity<NN_DEPTH_SHADOW_RESOURCES)return ENOSPC;
 struct nn_depth_shadow_plan expected;int e=nn_depth_shadow_plan(source,p->attachment.base_level,p->attachment.base_layer,&expected);if(e)return e;
 if(memcmp(p,&expected,sizeof(expected)))return EINVAL;
 if((e=canonical_range(source,c))||(e=nn_depth_transfer_platform_validate(d,platform)))return e;
 if(!k||k->schema!=1||k->chip!=0x06050002||k->chip!=nn_device_chip(d)||k->ssbo_slots!=2||
  k->local[0]!=8||k->local[1]!=8||k->local[2]!=1||!k->code||!k->code_bytes||!k->commands||!k->command_words||k->command_words>1024||(k->patch_count&&!k->patches))return ENOTSUP;
 out->canonical=*c;out->plan=*p;
 if((e=nn_depth_attachment_create(d,&p->attachment,&out->attachment))||
    (e=nn_depth_readback_create(d,&out->attachment,&out->linear))||
    (e=nn_bo_alloc(d,k->code_bytes,&out->code)))goto fail;
 void*map=nn_bo_map(out->code);if(!map){e=ENOTSUP;goto fail;}memcpy(map,k->code,k->code_bytes);
 if((e=nn_bo_cache(out->code,0,k->code_bytes,0)))goto fail;
 for(unsigned i=0;i<2;i++){
  if((e=nn_bo_alloc(d,64,out->uniform+i)))goto fail;
  struct nn_block_copy_range linear={out->linear.storage,0,nn_bo_size(out->linear.storage),0};
  struct nn_block_copy_range uniform={out->uniform[i],0,nn_bo_size(out->uniform[i]),0};
  struct nn_block_copy_binding b;
  if((e=nn_block_copy_bind(d,k,source,p->copy+i,c,&linear,&uniform,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)))goto fail;
  map=nn_bo_map(out->uniform[i]);if(!map){e=ENOTSUP;goto fail;}memcpy(map,&b.args,64);
  if((e=nn_bo_cache(out->uniform[i],0,64,0))||
     (e=nn_cs_encode(d,out->code,k,b.ubos,b.ssbos,b.groups,&out->copy[i].job,&out->copy[i].bytes)))goto fail;
  if((e=nn_bo_cache(out->copy[i].job,0,nn_bo_size(out->copy[i].job),0)))goto fail;
 }
 struct nn_depth_a2d_region r={0,0,out->attachment.view.view_width,out->attachment.view.view_height};
 if((e=nn_depth_transfer_encode(d,&out->attachment,&out->linear,platform,NN_DEPTH_A2D_UPLOAD,&r,0,&out->a2d[0].job,&out->a2d[0].bytes))||
    (e=nn_depth_transfer_encode(d,&out->attachment,&out->linear,platform,NN_DEPTH_A2D_READBACK,&r,0,&out->a2d[1].job,&out->a2d[1].bytes)))goto fail;
 out->ready=1;return 0;
fail:(void)nn_depth_shadow_destroy(out);return e;
}
int nn_depth_shadow_resources(const struct nn_depth_shadow*s,nn_bo**out,size_t cap,size_t*count)
{
 if(count)*count=0;
 if(!s||!out||!count||!s->ready||!s->canonical.bo)return EINVAL;
 int e=nn_depth_readback_validate(&s->attachment,&s->linear);if(e)return e;
 if(!s->code||!s->uniform[0]||!s->uniform[1])return EINVAL;
 for(unsigned i=0;i<2;i++)if(!s->copy[i].job||!s->copy[i].bytes||s->copy[i].bytes%4||s->copy[i].bytes>nn_bo_size(s->copy[i].job)||
  !s->a2d[i].job||!s->a2d[i].bytes||s->a2d[i].bytes%4||s->a2d[i].bytes>nn_bo_size(s->a2d[i].job))return EINVAL;
 if(cap<NN_DEPTH_SHADOW_RESOURCES)return ENOSPC;
 nn_bo*all[NN_DEPTH_SHADOW_RESOURCES]={s->canonical.bo,s->attachment.storage,s->linear.storage,s->code,
  s->uniform[0],s->uniform[1],s->copy[0].job,s->copy[1].job,s->a2d[0].job,s->a2d[1].job};
 for(unsigned i=0;i<NN_DEPTH_SHADOW_RESOURCES;i++)for(unsigned j=0;j<i;j++)if(all[i]==all[j])return EINVAL;
 memcpy(out,all,sizeof(all));*count=NN_DEPTH_SHADOW_RESOURCES;return 0;
}
int nn_depth_shadow_sequence(const struct nn_depth_shadow*s,struct nn_depth_shadow_step load[2],struct nn_depth_shadow_step store[2])
{
 if(!load||!store)return EINVAL;
 nn_bo*refs[NN_DEPTH_SHADOW_RESOURCES];size_t n;int e=nn_depth_shadow_resources(s,refs,NN_DEPTH_SHADOW_RESOURCES,&n);if(e)return e;
 struct nn_depth_shadow_step a[2]={s->copy[0],s->a2d[0]},b[2]={s->a2d[1],s->copy[1]};
 memcpy(load,a,sizeof(a));memcpy(store,b,sizeof(b));return 0;
}
