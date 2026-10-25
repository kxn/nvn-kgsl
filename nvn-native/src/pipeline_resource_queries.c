/* SPDX-License-Identifier: MIT. No shader parser, pool-ID guess or GPU operation. */
#include "nvn_native/pipeline_resource_queries.h"
#include "nvn_native/texture_buffer_format.h"
#include <errno.h>
#include <string.h>
static int table_selector(uint32_t word,uint32_t base,uint32_t *selector)
{
 uint64_t byte=(uint64_t)word*4;
 if(byte<base||byte>=base+32*8||(byte-base)%8)return ENOTSUP;
 *selector=(uint32_t)(byte-base)/8;return 0;
}
int nn_pipeline_resource_reference(int32_t handle,int32_t cbuf,struct nn_pipeline_resource_reference *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(handle<0)return ENOTSUP;
 uint32_t bits=(uint32_t)handle,type=bits>>28,texture=bits&0x3fffu,sampler=(bits>>14)&0x3fffu;
 uint32_t tcb=cbuf<0?2u:(uint32_t)cbuf&0xffffu;
 uint32_t high=cbuf<0?0u:(uint32_t)cbuf>>16,scb=high?high-1:tcb;
 if(cbuf<-1||tcb!=2||scb!=2)return ENOTSUP;
 struct nn_pipeline_resource_reference r={0};int e;
 if(type==0){
  /* Nonpacked immediate TXF/TXS uses the separate texture table. Combined
   * sampling uses its own distinct CB table, never an equal API selector. */
  if(!table_selector(bits,0x168,&r.texture_selector))r.texture_domain=NN_PIPELINE_SEPARATE_TEXTURE;
  else if(!table_selector(bits,0x20,&r.texture_selector)){
   r.texture_domain=NN_PIPELINE_COMBINED;r.has_sampler=1;
   r.sampler_domain=NN_PIPELINE_COMBINED;r.sampler_selector=r.texture_selector;
  }else return ENOTSUP;
  r.texture_word=bits;r.sampler_word=bits;
 }else if(type==1){
  if((e=table_selector(texture,0x168,&r.texture_selector)))return e;
  if((e=table_selector(sampler,0x568,&r.sampler_selector)))return e;
  r.texture_domain=NN_PIPELINE_SEPARATE_TEXTURE;r.sampler_domain=NN_PIPELINE_SEPARATE_SAMPLER;
  r.texture_word=texture;r.sampler_word=sampler;r.has_sampler=1;
 }else return ENOTSUP; /* Direct/ID/constant-sampler forms need separate evidence. */
 *out=r;return 0;
}
int nn_pipeline_resource_texture(const struct nn_pipeline_resource_queries *s,uint32_t stage,
 int32_t word,int32_t cbuf,struct nn_pipeline_resolved_texture *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));
 if(!s||s->version!=1||stage>1||s->binding_count>NN_PIPELINE_RESOURCE_BINDINGS||
    (s->binding_count&&!s->bindings)||s->graphics_texture_cb_known>1)return EINVAL;
 if(!s->graphics_texture_cb_known||s->graphics_texture_cb!=2)return ENOTSUP;
 struct nn_pipeline_resource_reference ref;int e=nn_pipeline_resource_reference(word,cbuf,&ref);if(e)return e;
 uint32_t selector=ref.texture_selector;
 const struct nn_pipeline_resource_binding *found=NULL;
 for(size_t i=0;i<s->binding_count;i++) {
  const struct nn_pipeline_resource_binding *b=s->bindings+i;
  if(b->stage>1||b->domain>NN_PIPELINE_SEPARATE_SAMPLER||b->selector>=32||b->low_word_defined>1)return EINVAL;
  if(b->stage!=stage||b->domain!=ref.texture_domain||b->selector!=selector)continue;
  if(found)return EINVAL; /* Never choose a duplicate/first binding. */
  found=b;
 }
 if(!found||!found->low_word_defined||!s->selected_texture_pool)return ENOTSUP;
 uint32_t id=found->payload&0xfffffu;
 if(id<256||(ref.texture_domain!=NN_PIPELINE_COMBINED&&found->payload>=(1u<<20))||
    (uint32_t)found->guest_carrier!=found->payload)return EINVAL;
 const struct nn_pipeline_registered_texture *t=&found->texture;
 if(t->registered>1)return EINVAL;
 if(!t->registered)return ENOTSUP;
 if(t->descriptor_pool!=s->selected_texture_pool||t->descriptor_id!=id||
    !t->backing_pool||!t->bytes||!t->width||!t->height||!t->format||t->offset>UINT64_MAX-t->bytes)return EINVAL;
 /* Source dimension is independent of FDL/native descriptor layout. BUFFER
  * is the registered typed resource, never a guessed height-one image. */
 if(t->depth!=1||!t->levels||t->samples)return ENOTSUP;
 /* Mip count does not change ordinary2D source type/coordinate semantics.
  * Native format/view/filter admission remains independent at Draw. */
 uint32_t sampler_type;
 if(t->target==1)sampler_type=3; /* NVN2D -> translator Texture2D. */
 else if(t->target==10){
  uint32_t cpp=0;if(t->height!=1||t->levels!=1||nn_tex_buffer_element_bytes(t->format,&cpp))return ENOTSUP;
  if((uint64_t)t->width*cpp!=t->bytes)return EINVAL;
  sampler_type=2; /* NVN BUFFER -> translator TextureBuffer. */
 }else return ENOTSUP;
 *out=(struct nn_pipeline_resolved_texture){stage,2,(uint32_t)word,selector,id,sampler_type,*t};
 return 0;
}
int nn_pipeline_resource_query(void *user,uint32_t stage,const struct nn_pipeline_query *q,uint32_t out[32],size_t *count)
{
 if(!count)return EINVAL;
 *count=0;if(out)memset(out,0,32*sizeof(*out));
 if(!user||!q||!q->name||!out)return EINVAL;
 int format=!strcmp(q->name,"QueryTextureFormat");
 int normalized=!strcmp(q->name,"QueryTextureCoordNormalized");
 if(!format&&!normalized&&strcmp(q->name,"QuerySamplerType"))return ENOTSUP;
 if(q->arguments!=3||q->type!=(normalized?NN_PIPELINE_QUERY_BOOL:NN_PIPELINE_QUERY_I32)||q->count!=1||q->args[2]||q->args[3])return EINVAL;
 struct nn_pipeline_resolved_texture t;
 int e=nn_pipeline_resource_texture(user,stage,q->args[0],q->args[1],&t);if(e)return e;
 if(normalized){
  /* Explicit native compatibility policy: ordinary registered NVN2D uses
   * normalized coordinates. Public target2D/RECTANGLE are distinct; independent
   * deko TIC Pitch/BlockLinear writers use type!=Rectangle and its TSC writer
   * uses force_unnormalized=0. This is not an SDK53.314 observed descriptor.
   * BUFFER/Rectangle/unknown dimensions never inherit an archive expectation.
   * The supported native sampler path separately admits only its actual state;
   * no force-unnormalized override exists in this frontend's supported domain. */
  if(t.texture.target!=1||t.sampler_type!=3)return ENOTSUP;
  out[0]=1;
 }else if(format){
  if(t.sampler_type!=2&&t.sampler_type!=3)return ENOTSUP;
  /* Original NVN source format -> vendored Shader.TextureFormat, NOT FDL.
   * Reuse only the scalar format representation, independently of BUFFER
   * storage/admission: actual registered ordinary2D has the same NVN enum.
   * RGBA8 NVN37 differs from Shader.R8G8B8A8Unorm25; R32Float is10.
   * No equivalent exists for BGRA/sRGB/BC/depth in this shader enum. Reject
   * rather than return Unknown (the translator substitutes RGBA8 for it).
   * This query never grants image load/store, native sampling or Draw. */
  e=nn_tex_buffer_query_format(t.texture.format,out);if(e)return e;
 }else out[0]=t.sampler_type;
 *count=1;return 0;
}
int nn_pipeline_frontend_resource_query(void *user,uint32_t stage,const struct nn_pipeline_query *q,uint32_t out[32],size_t *count)
{
 if(!count)return EINVAL;
 *count=0;if(out)memset(out,0,32*sizeof(*out));
 if(!user||!q||!q->name||!out)return EINVAL;
 struct nn_pipeline_frontend_resource_queries *s=user;
 if(!strcmp(q->name,"QuerySamplerType")||!strcmp(q->name,"QueryTextureFormat")||
    !strcmp(q->name,"QueryTextureCoordNormalized"))return nn_pipeline_resource_query(&s->resources,stage,q,out,count);
 return nn_pipeline_frontend_query(&s->graphics,stage,q,out,count);
}
