/* SPDX-License-Identifier: MIT. Actual depth FDL + existing GPU block/A2D. */
#include "nvn_native/depth_sampled.h"
#include "nvn_native/resource_access.h"
#include <errno.h>
#include <string.h>
int nn_depth_sampled_validate(const struct nn_depth_fdl_view*v)
{
 if(!v||v->desc.roles!=NN_DEPTH_SAMPLED||v->desc.format<0x32||v->desc.format>0x34)return ENOTSUP;
 struct nn_depth_fdl_view expected;int e=nn_depth_fdl(&v->desc,&expected);if(e)return e;
 return memcmp(v,&expected,sizeof(expected))?EINVAL:0;
}
int nn_depth_sampled_plan(const struct nn_tex_source_layout*s,uint32_t level,uint32_t layer,const uint32_t swizzle[4],struct nn_depth_sampled_plan*out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!swizzle)return EINVAL;
 struct nn_depth_shadow_plan existing;int e=nn_depth_shadow_plan(s,level,layer,&existing);if(e)return e;
 struct nn_depth_sampled_plan p={0};p.descriptor=existing.attachment;p.descriptor.roles=NN_DEPTH_SAMPLED;
 memcpy(p.descriptor.swizzle,swizzle,sizeof(p.descriptor.swizzle));
 if((e=nn_depth_fdl(&p.descriptor,&p.native))||(e=nn_depth_sampled_validate(&p.native))||
    (e=nn_depth_a2d(&p.descriptor,0,&p.aliases)))return e;
 p.copy=existing.copy[0];
 if(p.native.view_width!=s->level[level].width||p.native.view_height!=s->level[level].height||
    p.copy.linear_bytes>p.aliases.linear.bytes||p.copy.region.linear_row_stride!=p.aliases.linear.pitch)return EINVAL;
 *out=p;return 0;
}
int nn_depth_sampled_types(const struct nn_pipeline_texture_types*t,unsigned binding,uint32_t*out)
{
 if(!out)return EINVAL;
 *out=0;if(!t||t->version!=1||!t->recipe||binding>=16)return EINVAL;
 if(t->recipe->chip!=0x06050002||(t->recipe->version!=1&&t->recipe->version!=2)||
    t->recipe->texture_mask!=t->texture_mask||t->recipe->sampler_mask!=t->sampler_mask)return EINVAL;
 int e=nn_pipeline_texture_type_validate(t,binding);if(e)return e;
 uint32_t bit=1u<<binding,op=t->operations[binding];
 if(!(t->texture_mask&bit)||t->dimensions[binding]!=2||!op||
    !!(op&(NN_PIPELINE_TEX_SAMPLE|NN_PIPELINE_TEX_GATHER))!=!!(t->sampler_mask&bit))return ENOTSUP;
 if((op&NN_PIPELINE_TEX_GATHER)&&t->gathers[binding].component_mask!=1)return ENOTSUP;
 *out=op;return 0;
}
int nn_depth_sampled_sampler(const struct nn_depth_fdl_view*v,const struct nn_sampler_state*s,const struct nn_sampler_layout*l,uint32_t out[4])
{
 if(!out)return EINVAL;
 int e=nn_depth_sampled_validate(v);if(e)return e;
 if(!l||l->width!=v->view_width||l->height!=v->view_height||l->levels!=1||l->depth!=1)return ENOTSUP;
 return nn_sampler_state_encode_depth_noncompare(v->desc.chip,s,l,out);
}
static size_t owned(const struct nn_depth_sampled_shadow*s,nn_bo**out)
{
 size_t n=0;
 if(s->upload.job)out[n++]=s->upload.job;
 if(s->copy.job)out[n++]=s->copy.job;
 if(s->uniform)out[n++]=s->uniform;
 if(s->code)out[n++]=s->code;
 if(s->linear.storage)out[n++]=s->linear.storage;
 if(s->native.storage)out[n++]=s->native.storage;
 return n;
}
int nn_depth_sampled_destroy(struct nn_depth_sampled_shadow*s)
{
 if(!s)return EINVAL;
 nn_bo*bos[6];size_t n=owned(s,bos);for(size_t i=0;i<n;i++){int e=nn_bo_idle(bos[i]);if(e)return e;}
 s->ready=0;
 if(s->upload.job){int e=nn_bo_destroy(s->upload.job);if(e)return e;s->upload=(struct nn_depth_shadow_step){0};}
 if(s->copy.job){int e=nn_bo_destroy(s->copy.job);if(e)return e;s->copy=(struct nn_depth_shadow_step){0};}
 if(s->uniform){int e=nn_bo_destroy(s->uniform);if(e)return e;s->uniform=NULL;}
 if(s->code){int e=nn_bo_destroy(s->code);if(e)return e;s->code=NULL;}
 int e=nn_depth_readback_destroy(&s->linear);if(e)return e;
 if((e=nn_depth_attachment_destroy(&s->native)))return e;
 memset(s,0,sizeof(*s));return 0;
}
int nn_depth_sampled_create(nn_device*d,const struct nn_cs_image*k,const struct nn_pipeline_platform*platform,const struct nn_tex_source_layout*source,const struct nn_depth_sampled_plan*p,const struct nn_block_copy_range*c,size_t capacity,struct nn_depth_sampled_shadow*out)
{
 if(!out)return EINVAL;
 nn_bo*bos[6];if(out->ready||out->canonical.bo||owned(out,bos))return EBUSY;
 if(!d||!p||!c||!c->bo)return EINVAL;
 if(capacity<NN_DEPTH_SAMPLED_RESOURCES)return ENOSPC;
 struct nn_depth_sampled_plan expected;int e=nn_depth_sampled_plan(source,p->descriptor.base_level,p->descriptor.base_layer,p->descriptor.swizzle,&expected);if(e)return e;
 if(memcmp(p,&expected,sizeof(expected)))return EINVAL;
 if(c->owner_offset>nn_bo_size(c->bo)||c->owner_bytes>nn_bo_size(c->bo)-c->owner_offset||c->offset<c->owner_offset||
    c->offset-c->owner_offset>c->owner_bytes||source->payload_bytes>c->owner_bytes-(c->offset-c->owner_offset))return EINVAL;
 uint64_t a=nn_bo_address(c->bo);if(!a||c->offset>UINT64_MAX-a||source->payload_bytes>UINT64_MAX-a-c->offset)return EOVERFLOW;
 a+=c->offset;if(a%4||(a&~UINT64_C(63))<nn_bo_address(c->bo)+c->owner_offset||a>=(UINT64_C(1)<<49)||source->payload_bytes>(UINT64_C(1)<<49)-a)return EINVAL;
 if((e=nn_depth_transfer_platform_validate(d,platform)))return e;
 if(!k||k->schema!=1||k->chip!=0x06050002||k->chip!=nn_device_chip(d)||k->ssbo_slots!=2||
    k->local[0]!=8||k->local[1]!=8||k->local[2]!=1||!k->code||!k->code_bytes||!k->commands||!k->command_words||
    k->command_words>1024||(k->patch_count&&!k->patches))return ENOTSUP;
 out->canonical=*c;out->plan=*p;
 if((e=nn_depth_attachment_create(d,&p->descriptor,&out->native))||(e=nn_depth_readback_create(d,&out->native,&out->linear))||
    (e=nn_bo_alloc(d,k->code_bytes,&out->code)))goto failed;
 void*map=nn_bo_map(out->code);if(!map){e=ENOTSUP;goto failed;}memcpy(map,k->code,k->code_bytes);
 if((e=nn_bo_cache(out->code,0,k->code_bytes,0))||(e=nn_bo_alloc(d,64,&out->uniform)))goto failed;
 struct nn_block_copy_range linear={out->linear.storage,0,nn_bo_size(out->linear.storage),0};
 struct nn_block_copy_range uniform={out->uniform,0,nn_bo_size(out->uniform),0};struct nn_block_copy_binding b;
 if((e=nn_block_copy_bind(d,k,source,&p->copy,c,&linear,&uniform,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)))goto failed;
 map=nn_bo_map(out->uniform);if(!map){e=ENOTSUP;goto failed;}memcpy(map,&b.args,64);
 if((e=nn_bo_cache(out->uniform,0,64,0))||(e=nn_cs_encode(d,out->code,k,b.ubos,b.ssbos,b.groups,&out->copy.job,&out->copy.bytes))||
    (e=nn_bo_cache(out->copy.job,0,nn_bo_size(out->copy.job),0)))goto failed;
 struct nn_depth_a2d_region r={0,0,out->native.view.view_width,out->native.view.view_height};
 if((e=nn_depth_transfer_encode(d,&out->native,&out->linear,platform,NN_DEPTH_A2D_UPLOAD,&r,0,&out->upload.job,&out->upload.bytes)))goto failed;
 out->ready=1;return 0;
failed:(void)nn_depth_sampled_destroy(out);return e;
}
int nn_depth_sampled_resources(const struct nn_depth_sampled_shadow*s,nn_bo**out,size_t capacity,size_t*count)
{
 if(!count)return EINVAL;
 *count=0;if(!s||!s->ready||!s->canonical.bo||!out)return EINVAL;
 int e=nn_depth_readback_validate(&s->native,&s->linear);if(e)return e;
 if((e=nn_depth_sampled_validate(&s->native.view)))return e;
 if(!s->code||!s->uniform||!s->copy.job||!s->upload.job||!s->copy.bytes||s->copy.bytes%4||s->copy.bytes>nn_bo_size(s->copy.job)||
    !s->upload.bytes||s->upload.bytes%4||s->upload.bytes>nn_bo_size(s->upload.job))return EINVAL;
 if(capacity<NN_DEPTH_SAMPLED_RESOURCES)return ENOSPC;
 nn_bo*all[7]={s->canonical.bo,s->native.storage,s->linear.storage,s->code,s->uniform,s->copy.job,s->upload.job};
 for(unsigned i=0;i<7;i++)for(unsigned j=0;j<i;j++)if(all[i]==all[j])return EINVAL;
 memcpy(out,all,sizeof(all));*count=7;return 0;
}
int nn_depth_sampled_sequence(const struct nn_depth_sampled_shadow*s,struct nn_depth_shadow_step load[2])
{
 if(!load)return EINVAL;
 nn_bo*refs[7];size_t n;int e=nn_depth_sampled_resources(s,refs,7,&n);if(e)return e;
 const struct nn_depth_shadow_step steps[2]={s->copy,s->upload};memcpy(load,steps,sizeof(steps));return 0;
}
int nn_depth_sampled_binding(const struct nn_depth_sampled_shadow*s,struct nn_depth_attachment*out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));nn_bo*refs[7];size_t n;int e=nn_depth_sampled_resources(s,refs,7,&n);if(e)return e;
 *out=s->native;return 0;
}
