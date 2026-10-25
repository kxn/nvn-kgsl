/* SPDX-License-Identifier: MIT. Actual FDL + qualified raw-block primitives;
 * combined public color-target GPU path remains separately unqualified. */
#include "nvn_native/color_target_shadow.h"
#include <errno.h>
#include <string.h>
int nn_color_target_shadow_plan(const struct nn_tex_source_layout *s,const struct nn_tex_native_view *v,struct nn_color_target_shadow_plan *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=nn_tex_source_validate(s);if(e)return e;
 if(s->linear||s->target!=1||s->levels!=1||s->layers!=1||s->volume_depth||s->block_w!=1||s->block_h!=1||s->payload_bytes>UINT32_MAX)return ENOTSUP;
 struct nn_color_target_shadow_plan p={0};if((e=nn_color_target_validate(v,&p.info)))return e;
 if(v->format!=s->format||v->width!=s->level[0].width||v->height!=s->level[0].height||
    p.info.bytes_per_pixel!=s->bytes_per_block||v->pitch<s->level[0].row_bytes||v->required_span_bytes<v->layer_bytes)return EINVAL;
 p.native=*v;
 for(unsigned direction=0;direction<2;direction++){
  struct nn_block_copy_region r={.layers=1,.row_bytes=s->level[0].row_bytes,.rows=s->level[0].blocks_y,
   .linear_row_stride=v->pitch,.linear_image_stride=v->layer_bytes,.direction=direction};
  if((e=nn_block_copy_plan(s,&r,p.copy+direction)))return e;
  if(p.copy[direction].linear_bytes>v->required_span_bytes)return EINVAL;
 }
 *out=p;return 0;
}
int nn_color_target_shadow_create(nn_device *d,const struct nn_cs_image *image,const struct nn_tex_source_layout *s,
 const struct nn_color_target_shadow_plan *p,const struct nn_block_copy_range *canonical,struct nn_tex_shadow *out)
{
 if(!out)return EINVAL;
 for(unsigned i=0;i<NN_TEX_SHADOW_BO_COUNT;i++)if(out->bos[i])return EBUSY;
 if(out->pre_bytes||out->post_bytes)return EBUSY;
 if(!d||nn_device_chip(d)!=0x06050002||!p||!canonical||!canonical->bo)return EINVAL;
 struct nn_color_target_shadow_plan expected;int e=nn_color_target_shadow_plan(s,&p->native,&expected);if(e)return e;
 if(memcmp(&expected,p,sizeof(expected)))return EINVAL;
 uint64_t a=nn_bo_address(canonical->bo);
 if(!a||canonical->owner_offset>nn_bo_size(canonical->bo)||canonical->owner_bytes>nn_bo_size(canonical->bo)-canonical->owner_offset||
    canonical->offset<canonical->owner_offset||canonical->offset-canonical->owner_offset>canonical->owner_bytes||
    s->payload_bytes>canonical->owner_bytes-(canonical->offset-canonical->owner_offset)||
    canonical->offset>UINT64_MAX-a||s->payload_bytes>UINT64_MAX-a-canonical->offset)return EINVAL;
 a+=canonical->offset;
 if(a%4||(a&~UINT64_C(63))<nn_bo_address(canonical->bo)+canonical->owner_offset||a+s->payload_bytes-1>=(UINT64_C(1)<<49))return EINVAL;
 if(!image||image->schema!=1||image->chip!=0x06050002||image->ssbo_slots!=2||image->local[0]!=8||image->local[1]!=8||image->local[2]!=1||!image->code||!image->code_bytes||!image->commands||!image->command_words)return ENOTSUP;
 if((e=nn_bo_alloc(d,p->native.required_span_bytes,out->bos+NN_TEX_SHADOW_STORAGE))||
    (e=nn_bo_alloc(d,image->code_bytes,out->bos+NN_TEX_SHADOW_CODE))||
    (e=nn_bo_alloc(d,128,out->bos+NN_TEX_SHADOW_UNIFORM)))goto fail;
 /* Fresh driver storage proves physical independence. Also reject an
  * inconsistent allocator/GPUVA result instead of encoding overlapping spans. */
 uint64_t na=nn_bo_address(out->bos[NN_TEX_SHADOW_STORAGE]),nb=p->native.required_span_bytes;
 if(!na||nb>UINT64_MAX-na||(na<a+s->payload_bytes&&a<na+nb)){e=EINVAL;goto fail;}
 /* Only metadata/code map; never read canonical or private native pixels. */
 void *code=nn_bo_map(out->bos[NN_TEX_SHADOW_CODE]),*uniform=nn_bo_map(out->bos[NN_TEX_SHADOW_UNIFORM]);
 if(!code||!uniform){e=ENOTSUP;goto fail;}
 memcpy(code,image->code,image->code_bytes);memset(uniform,0,128);
 for(unsigned direction=0;direction<2;direction++){
  struct nn_block_copy_range native={out->bos[NN_TEX_SHADOW_STORAGE],0,nn_bo_size(out->bos[NN_TEX_SHADOW_STORAGE]),0};
  struct nn_block_copy_range u={out->bos[NN_TEX_SHADOW_UNIFORM],0,128,direction*64};struct nn_block_copy_binding binding;
  if((e=nn_block_copy_bind(d,image,s,p->copy+direction,canonical,&native,&u,NN_BLOCK_COPY_DISJOINT_PROVEN,&binding)))goto fail;
  memcpy((char*)uniform+direction*64,&binding.args,64);
  size_t *bytes=direction?&out->post_bytes:&out->pre_bytes;
  if((e=nn_cs_encode(d,out->bos[NN_TEX_SHADOW_CODE],image,binding.ubos,binding.ssbos,binding.groups,
       out->bos+(direction?NN_TEX_SHADOW_POST:NN_TEX_SHADOW_PRE),bytes)))goto fail;
 }
 for(unsigned i=NN_TEX_SHADOW_CODE;i<NN_TEX_SHADOW_BO_COUNT;i++)if((e=nn_bo_cache(out->bos[i],0,nn_bo_size(out->bos[i]),0)))goto fail;
 return 0;
fail:
 (void)nn_tex_shadow_destroy(out);return e;
}
