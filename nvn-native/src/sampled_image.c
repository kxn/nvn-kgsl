/* SPDX-License-Identifier: MIT. Actual FDL + existing raw-block commands. */
#include "nvn_native/sampled_image.h"
#include "nvn_native/depth_native.h"
#ifndef NN_PIPELINE_DEPTH_NATIVE
#define NN_PIPELINE_DEPTH_NATIVE 0
#endif
#include "nvn_native/texture_bc_shadow.h"
#include "nvn_native/pipeline_native_program.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifndef NN_PIPELINE_GATHER
#define NN_PIPELINE_GATHER 0
#endif
#ifndef NN_PIPELINE_SAMPLED_MIP_FILTER
#define NN_PIPELINE_SAMPLED_MIP_FILTER 0
#endif
static struct nn_fdl_image_desc desc(const struct nn_fdl_image_view *v)
{
 return (struct nn_fdl_image_desc){.schema=1,.chip=v->chip,.format=v->format,.target=v->target,.width=v->width,.height=v->height,
  .levels=v->levels,.layers=v->layers,.base_level=v->base_level,.level_count=v->level_count,.base_layer=v->base_layer,.layer_count=v->layer_count,.usage=NN_FDL_SAMPLED};
}
int nn_sampled_image_validate(const struct nn_fdl_image_view *v,struct nn_fdl_format_info *info)
{
 if(info)memset(info,0,sizeof(*info));
 if(!v||v->schema!=1||v->chip!=0x06050002||v->target!=1||v->layers!=1||v->base_layer||v->layer_count!=1||v->color_target)return ENOTSUP;
 struct nn_fdl_format_info traits;int e=nn_fdl_image_format_info(v->chip,v->format,&traits);if(e)return e;
 if(traits.result_type!=NN_FDL_RESULT_FLOAT||traits.block_width!=1||traits.block_height!=1)return ENOTSUP;
 struct nn_fdl_image_desc d=desc(v);struct nn_fdl_image_view expected;if((e=nn_fdl_image(&d,&expected)))return e;
 if(memcmp(v,&expected,sizeof(expected)))return EINVAL;
 if(info)*info=traits;
 return 0;
}
int nn_sampled_image_plan(const struct nn_tex_source_layout *s,uint32_t base,uint32_t levels,struct nn_sampled_image_plan *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=nn_tex_source_validate(s);if(e)return e;
 if(s->linear||s->target!=1||s->layers!=1||s->volume_depth||s->block_w!=1||s->block_h!=1)return ENOTSUP;
 struct nn_sampled_image_plan p={0};p.descriptor=(struct nn_fdl_image_desc){.schema=1,.chip=0x06050002,.format=s->format,.target=1,
  .width=s->level[0].width,.height=s->level[0].height,.levels=s->levels,.layers=1,.base_level=base,.level_count=levels,.layer_count=1,.usage=NN_FDL_SAMPLED};
 if((e=nn_fdl_image(&p.descriptor,&p.native))||(e=nn_sampled_image_validate(&p.native,&p.format)))return e;
 if(p.format.bytes_per_block!=s->bytes_per_block)return EINVAL;
 for(unsigned i=0;i<s->levels;i++){
  const struct nn_tex_source_level *a=s->level+i;const struct nn_fdl_level *b=p.native.level+i;
  if(a->width!=b->width||a->height!=b->height||b->offset%4)return EINVAL;
  struct nn_block_copy_region r={.level=i,.layers=1,.row_bytes=a->row_bytes,.rows=a->blocks_y,.linear_row_stride=b->pitch,.linear_image_stride=b->layer_stride,.direction=0};
  if((e=nn_block_copy_plan(s,&r,p.copy+i)))return e;
  if(b->offset>p.native.required_span||p.copy[i].linear_bytes>p.native.required_span-b->offset)return EINVAL;
 }
 p.count=s->levels;*out=p;return 0;
}
int nn_sampled_image_descriptor(const struct nn_fdl_image_view *v,const struct nn_sampled_image_binding *b,uint32_t out[16])
{
 if(!out)return EINVAL;
 memset(out,0,64);int e=nn_sampled_image_validate(v,NULL);if(e)return e;
 if(!b||!b->bo)return EINVAL;
 size_t n=nn_bo_size(b->bo);
 if(b->owner_offset>n||b->owner_bytes>n-b->owner_offset||b->offset<b->owner_offset||b->offset-b->owner_offset>b->owner_bytes||
    b->bytes>b->owner_bytes-(b->offset-b->owner_offset)||v->required_span>b->bytes)return EINVAL;
 uint64_t a=nn_bo_address(b->bo);if(!a||b->offset>UINT64_MAX-a||b->bytes>UINT64_MAX-a-b->offset)return EOVERFLOW;
 struct nn_fdl_image_desc d=desc(v);d.address=a+b->offset;struct nn_fdl_image_view actual;if((e=nn_fdl_image(&d,&actual)))return e;
 memcpy(out,actual.descriptor,64);return 0;
}
int nn_sampled_image_sampler(const struct nn_fdl_image_view *v,const struct nn_sampler_state *s,const struct nn_sampler_layout *l,uint32_t out[4])
{
 if(!out)return EINVAL;
 memset(out,0,16);int e=nn_sampled_image_validate(v,NULL);if(e)return e;
 if(!l||l->width!=v->view_width||l->height!=v->view_height||l->levels!=v->level_count||l->samples!=1||
    l->dimensions!=2||l->normalized!=1||l->depth||l->integer)return ENOTSUP;
#if NN_PIPELINE_SAMPLED_MIP_FILTER
 return nn_sampler_state_encode_mipmapped(v->chip,s,l,out);
#else
 return nn_sampler_state_encode(v->chip,s,l,out);
#endif
}
int nn_sampled_image_program(const nn_pipeline_program *p,const struct nn_pipeline_shader_recipe *r,uint32_t binding,uint32_t *operations)
{
 if(!operations)return EINVAL;
 *operations=0;if(!p||!r||binding>=16)return EINVAL;
 const struct nn_pipeline_texture_types *t=nn_pipeline_program_texture_types(p);
 if(!t||r->chip!=0x06050002||t->version!=1||t->recipe!=r||t->texture_mask!=r->texture_mask||t->sampler_mask!=r->sampler_mask)return EINVAL;
 const struct nn_pipeline_depth_export *depth=nn_pipeline_program_depth_export(p);
 if(r->version!=NN_PIPELINE_RECIPE_VERSION&&r->version!=NN_PIPELINE_DEPTH_RECIPE_VERSION)return ENOTSUP;
 if(depth){
#if NN_PIPELINE_DEPTH_NATIVE
  if(depth->recipe!=r||depth->writes_depth!=(r->version==NN_PIPELINE_DEPTH_RECIPE_VERSION)||
     (!depth->writes_depth&&depth->depth_regid!=252))return EINVAL;
  int e=nn_depth_native_recipe(depth);if(e)return e;
#else
  return ENOTSUP;
#endif
 }else if(r->version==NN_PIPELINE_DEPTH_RECIPE_VERSION)return ENOTSUP;
 if(!(r->texture_mask&(1u<<binding))||t->dimensions[binding]!=2||nn_pipeline_texture_type_validate(t,binding))return ENOTSUP;
 if((t->operations[binding]&NN_PIPELINE_TEX_GATHER)&&!NN_PIPELINE_GATHER)return ENOTSUP;
 if(!!(t->operations[binding]&(NN_PIPELINE_TEX_SAMPLE|NN_PIPELINE_TEX_GATHER))!=!!(r->sampler_mask&(1u<<binding)))return EINVAL;
 /* Constructor binds exact recipe + source SPIR-V + code to this owner.
  * Recheck source identity before exporting accepted operation facts. */
 for(unsigned stage=0;stage<2;stage++){
  struct nn_pipeline_stage_view v;int e=nn_pipeline_program_stage(p,stage,&v);if(e)return e;
  if(!v.source||!v.source->spirv||!t->spirv[stage]||v.source->spirv_bytes!=t->spirv_bytes[stage]||
     t->spirv_bytes[stage]<20||t->spirv_bytes[stage]%4||t->spirv_bytes[stage]>1048576||memcmp(v.source->spirv,t->spirv[stage],t->spirv_bytes[stage]))return EINVAL;
 }
 *operations=t->operations[binding];return 0;
}
/* Reuse the existing format-neutral owner ledger's destruction/resources and
 * descriptor routines. Its BC admission/plan/create functions are NOT called:
 * this allocation has an independently validated sampled FLOAT image role. */
struct nn_sampled_image_shadow {struct nn_bc_shadow upload;struct nn_fdl_image_view zero_view;};
int nn_sampled_image_shadow_destroy(struct nn_sampled_image_shadow **p)
{
 if(!p)return EINVAL;
 if(!*p)return 0;
 int e=nn_bc_shadow_destroy(&(*p)->upload);if(e)return e;
 free(*p);*p=NULL;return 0;
}
int nn_sampled_image_shadow_create(nn_device *d,const struct nn_cs_image *kernel,const struct nn_tex_source_layout *s,
 const struct nn_sampled_image_plan *p,const struct nn_block_copy_range *c,struct nn_sampled_image_shadow **out)
{
 if(!out)return EINVAL;
 if(*out)return EBUSY;
 if(!d||nn_device_chip(d)!=0x06050002||!p||!c||!c->bo)return EINVAL;
 struct nn_sampled_image_plan expected;int e=nn_sampled_image_plan(s,p->descriptor.base_level,p->descriptor.level_count,&expected);if(e)return e;
 if(memcmp(&expected,p,sizeof(expected)))return EINVAL;
 uint64_t a=nn_bo_address(c->bo);
 if(!a||c->owner_offset>nn_bo_size(c->bo)||c->owner_bytes>nn_bo_size(c->bo)-c->owner_offset||c->offset<c->owner_offset||
    c->offset-c->owner_offset>c->owner_bytes||s->payload_bytes>c->owner_bytes-(c->offset-c->owner_offset)||
    c->offset>UINT64_MAX-a||s->payload_bytes>UINT64_MAX-a-c->offset)return EINVAL;
 a+=c->offset;if(a%4||(a&~UINT64_C(63))<nn_bo_address(c->bo)+c->owner_offset||a+s->payload_bytes-1>=(UINT64_C(1)<<49))return EINVAL;
 if(!kernel||kernel->schema!=1||kernel->chip!=0x06050002||kernel->ssbo_slots!=2||kernel->local[0]!=8||kernel->local[1]!=8||kernel->local[2]!=1||!kernel->code||!kernel->code_bytes||!kernel->commands||!kernel->command_words)return ENOTSUP;
 struct nn_sampled_image_shadow *o=calloc(1,sizeof(*o));if(!o)return ENOMEM;
 *out=o;o->zero_view=p->native;struct nn_bc_shadow *u=&o->upload;u->canonical=c->bo;u->canonical_offset=c->offset;u->canonical_bytes=(size_t)s->payload_bytes;u->count=p->count;
 if((e=nn_bo_alloc(d,(size_t)p->native.required_span,&u->storage))||(e=nn_bo_alloc(d,kernel->code_bytes,&u->code)))goto fail;
 uint64_t na=nn_bo_address(u->storage),bytes=p->native.required_span;
 if(!na||bytes>UINT64_MAX-na||(na<a+s->payload_bytes&&a<na+bytes)){e=EINVAL;goto fail;}
 struct nn_fdl_image_desc actual=p->descriptor;actual.address=na;if((e=nn_fdl_image(&actual,&u->view)))goto fail;
 if(u->view.required_span>nn_bo_size(u->storage)){e=EINVAL;goto fail;}
 void *code=nn_bo_map(u->code);if(!code){e=ENOTSUP;goto fail;}memcpy(code,kernel->code,kernel->code_bytes);
 if((e=nn_bo_cache(u->code,0,kernel->code_bytes,0)))goto fail;
 for(unsigned i=0;i<p->count;i++){
  if((e=nn_bo_alloc(d,64,u->uniform+i)))goto fail;
  void *ptr=nn_bo_map(u->uniform[i]);if(!ptr){e=ENOTSUP;goto fail;}
  struct nn_block_copy_range native={u->storage,0,nn_bo_size(u->storage),(size_t)p->native.level[i].offset};
  struct nn_block_copy_range uniform={u->uniform[i],0,nn_bo_size(u->uniform[i]),0};struct nn_block_copy_binding binding;
  if((e=nn_block_copy_bind(d,kernel,s,p->copy+i,c,&native,&uniform,NN_BLOCK_COPY_DISJOINT_PROVEN,&binding)))goto fail;
  memcpy(ptr,&binding.args,64);if((e=nn_bo_cache(u->uniform[i],0,64,0)))goto fail;
  if((e=nn_cs_encode(d,u->code,kernel,binding.ubos,binding.ssbos,binding.groups,u->dispatch+i,u->dispatch_bytes+i)))goto fail;
  if((e=nn_bo_cache(u->dispatch[i],0,nn_bo_size(u->dispatch[i]),0)))goto fail;
 }
 u->ready=1;return 0;
 fail:(void)nn_sampled_image_shadow_destroy(out);return e;
}
int nn_sampled_image_shadow_resources(const struct nn_sampled_image_shadow *o,nn_bo **out,size_t n,size_t *count)
{if(!o){if(count)*count=0;return EINVAL;}return nn_bc_shadow_resources(&o->upload,out,n,count);}
int nn_sampled_image_shadow_dispatch(const struct nn_sampled_image_shadow *o,uint32_t mip,nn_bo **bo,size_t *bytes)
{
 if(bo)*bo=NULL;
 if(bytes)*bytes=0;
 if(!o||!bo||!bytes||!o->upload.ready||mip>=o->upload.count)return EINVAL;
 nn_bo *b=o->upload.dispatch[mip];size_t n=o->upload.dispatch_bytes[mip];if(!b||!n||n%4||n>nn_bo_size(b))return EINVAL;
 *bo=b;*bytes=n;return 0;
}
int nn_sampled_image_shadow_view(const struct nn_sampled_image_shadow *o,const struct nn_fdl_image_view **out)
{if(!out)return EINVAL;*out=NULL;if(!o||!o->upload.ready)return EINVAL;*out=&o->zero_view;return 0;}
int nn_sampled_image_shadow_descriptor(const struct nn_sampled_image_shadow *o,uint32_t out[16])
{if(!o){if(out)memset(out,0,64);return EINVAL;}return nn_bc_shadow_descriptor(&o->upload,out);}

int nn_sampled_image_shadow_binding(const struct nn_sampled_image_shadow *o,struct nn_sampled_image_binding *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!o||!o->upload.ready||!o->upload.storage)return EINVAL;
 *out=(struct nn_sampled_image_binding){o->upload.storage,0,nn_bo_size(o->upload.storage),0,(size_t)o->zero_view.required_span};return 0;
}
