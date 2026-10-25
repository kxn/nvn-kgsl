/* SPDX-License-Identifier: MIT. No shader parser, address whitelist or first variant. */
#include "nvn_native/pipeline_frontend.h"
#include "nvn_native/depth_native.h"
#include "nvn_native/pipeline_runtime_constants.h"
#ifndef NN_PIPELINE_RUNTIME_CONSTANTS
#define NN_PIPELINE_RUNTIME_CONSTANTS 0
#endif
#ifndef NN_PIPELINE_DEPTH_NATIVE
#define NN_PIPELINE_DEPTH_NATIVE 0
#endif
#ifndef NN_PIPELINE_FRONT_FACING
#define NN_PIPELINE_FRONT_FACING 0
#endif
#include <errno.h>
#include <math.h>
#include <string.h>
static int range(const struct nn_pipeline_buffer *b,size_t need)
{
 if(!b->bo||!b->bytes||b->bytes>65536||b->offset>nn_bo_size(b->bo)||
    b->bytes>nn_bo_size(b->bo)-b->offset||need>b->bytes)return EINVAL;
 uint64_t address=nn_bo_address(b->bo);
 if(!address||b->offset>UINT64_MAX-address||b->bytes>UINT64_MAX-address-b->offset)return EOVERFLOW;
 return (address+b->offset)%256?EINVAL:0;
}
static int uniforms(const struct nn_offline_graphics_stage *s,
 const struct nn_pipeline_shader_recipe *r,const struct nn_pipeline_buffer api[14],
 struct nn_pipeline_buffer out[16],uint32_t version)
{
 if(!out)return EINVAL;
 memset(out,0,16*sizeof(*out));
 if(!s||s->stage>1||!r||r->version!=version||r->chip!=0x06050002||!api||
    s->source_bytes<s->code_bytes||s->cbuf_count>16||(s->cbuf_count&&!s->cbufs)||
    r->patch_count>256||(r->patch_count&&!r->patches))return EINVAL;
 uint32_t source[16],used=0;for(unsigned i=0;i<16;i++)source[i]=UINT32_MAX;
 for(size_t i=0;i<s->cbuf_count;i++) {
  uint32_t b=s->cbufs[i].binding,slot=s->cbufs[i].slot;
  if(b>=16||slot>=32||source[b]!=UINT32_MAX)return EINVAL;
  for(unsigned j=0;j<16;j++)if(source[j]==slot)return EINVAL;
  source[b]=slot;
 }
 struct nn_pipeline_buffer proposed[16]={0};
 for(size_t i=0;i<r->patch_count;i++) {
  const struct nn_pipeline_address_patch *p=r->patches+i;
  if((p->role!=NN_PIPELINE_UBO&&p->role!=NN_PIPELINE_DYNAMIC_UBO)||p->stage!=s->stage)continue;
#if !NN_PIPELINE_RUNTIME_CONSTANTS
  if(p->role==NN_PIPELINE_DYNAMIC_UBO)return ENOTSUP;
#endif
  if(p->binding>=16||!p->bytes||(uint64_t)p->offset+p->bytes>SIZE_MAX)return EINVAL;
  unsigned b=p->binding;size_t need=(size_t)p->offset+p->bytes;
  uint32_t slot=source[b];
  if(slot==1) {
   /* Full immutable C1 belongs to the uploaded owner, never an API binding. */
   if(!s->source||!s->code_bytes||s->source_bytes==s->code_bytes||
      need>s->source_bytes-s->code_bytes||
      (p->role==NN_PIPELINE_DYNAMIC_UBO&&((s->source_bytes-s->code_bytes)>65536||(s->source_bytes-s->code_bytes)%16)))return ENOTSUP;
   continue;
  }
  if(slot<3||slot>16)return ENOTSUP; /* Includes unknown/injected binding0. */
  int e=range(api+slot-3,need);if(e)return e;
#if NN_PIPELINE_RUNTIME_CONSTANTS
  if(p->role==NN_PIPELINE_DYNAMIC_UBO){uint32_t desc[2];if((e=nn_pipeline_runtime_ubo_descriptor(api[slot-3],desc)))return e;}
#endif
  proposed[b]=api[slot-3];used|=1u<<b;
 }
 for(unsigned b=0;b<16;b++)if(used&(1u<<b))out[b]=proposed[b];
 return 0;
}
int nn_pipeline_frontend_uniforms(const struct nn_offline_graphics_stage*s,const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_buffer api[14],struct nn_pipeline_buffer out[16])
{return uniforms(s,r,api,out,NN_PIPELINE_RECIPE_VERSION);}
int nn_pipeline_frontend_uniforms_depth(const struct nn_offline_graphics_stage*s,const struct nn_pipeline_depth_export*d,const struct nn_pipeline_buffer api[14],struct nn_pipeline_buffer out[16])
{
 if(!out)return EINVAL;
 memset(out,0,16*sizeof(*out));
#if NN_PIPELINE_DEPTH_NATIVE
 int e=nn_depth_native_recipe(d);if(e)return e;
 return uniforms(s,d->recipe,api,out,d->writes_depth?NN_PIPELINE_DEPTH_RECIPE_VERSION:NN_PIPELINE_RECIPE_VERSION);
#else
 (void)s;(void)d;(void)api;return ENOTSUP;
#endif
}
static uint32_t float_bits(float v){uint32_t b;memcpy(&b,&v,4);return b;}
static int scalar(const struct nn_pipeline_query *q,uint32_t type,uint32_t value,uint32_t *out,size_t *n)
{if(q->arguments||q->type!=type||q->count!=1)return EINVAL;out[0]=value;*n=1;return 0;}
int nn_pipeline_frontend_query(void *user,uint32_t stage,const struct nn_pipeline_query *q,uint32_t out[32],size_t *n)
{
 if(!n)return EINVAL;
 *n=0;
 if(!user||!q||!q->name||!out||stage>1)return EINVAL;
 const struct nn_pipeline_frontend_queries *s=user;
 if(s->version!=1||s->chip!=0x06050002||s->depth_mode>1||s->window_origin>1||
    s->render.version!=1||s->primitive>6||!isfinite(s->point_size)||
    !isfinite(s->alpha_test_reference))return EINVAL;
 const char *name=q->name;uint32_t value=0,type=NN_PIPELINE_QUERY_BOOL;
 if(!strcmp(name,"graphics/attribute_types")||!strcmp(name,"graphics/fragment_output_types")) {
  size_t count=!strcmp(name,"graphics/attribute_types")?32:8;
  if(q->arguments||q->type!=NN_PIPELINE_QUERY_I32||q->count!=count)return EINVAL;
  const uint32_t *a=count==32?s->attribute_types:s->fragment_output_types;
  /* Current native pipeline only supports floating/normalized vertex decoding
   * and floating RGBA8 MRT. Other actual types must not match a float variant. */
  for(size_t i=0;i<count;i++)if(a[i]>4)return EINVAL;
  memcpy(out,a,count*4);*n=count;return 0;
 }
 if(!strcmp(name,"QueryTransformFeedbackEnabled"))value=s->transform_feedback;
 else if(!strcmp(name,"QueryHasConstantBufferDrawParameters")||!strcmp(name,"graphics/has_constant_buffer_draw_parameters"))value=s->draw_parameters;
 else if(!strcmp(name,"graphics/early_z_force"))value=s->early_z_force;
 else if(!strcmp(name,"graphics/topology")) {type=NN_PIPELINE_QUERY_I32;value=s->primitive==0?0:s->primitive<4?1:3;}
 else if(!strcmp(name,"graphics/tess_cw"))value=0; /* No tess stage supported. */
 else if(!strcmp(name,"graphics/tess_patch_type")){type=NN_PIPELINE_QUERY_I32;value=1;}
 else if(!strcmp(name,"graphics/tess_spacing")){type=NN_PIPELINE_QUERY_I32;value=0;}
 else if(!strcmp(name,"graphics/alpha_to_coverage_enable"))value=s->render.multisample.alpha_to_coverage;
 else if(!strcmp(name,"graphics/alpha_to_coverage_dither_enable"))value=s->render.multisample.alpha_to_coverage_dither;
 else if(!strcmp(name,"graphics/viewport_transform_disable"))value=s->viewport_transform_disable;
 else if(!strcmp(name,"graphics/depth_mode"))value=!s->depth_mode; /* EmitterContext DepthMode means -1..1. */
 else if(!strcmp(name,"graphics/program_point_size_enable"))value=s->program_point_size_enable;
 else if(!strcmp(name,"graphics/point_size")){type=NN_PIPELINE_QUERY_F32;value=float_bits(s->point_size);}
 else if(!strcmp(name,"graphics/alpha_test_compare")){type=NN_PIPELINE_QUERY_I32;value=s->render.alpha_func;}
 else if(!strcmp(name,"graphics/alpha_test_reference")){type=NN_PIPELINE_QUERY_F32;value=float_bits(s->alpha_test_reference);}
 else if(!strcmp(name,"graphics/dual_source_blend_enable"))value=s->dual_source_blend;
 else if(!strcmp(name,"graphics/y_negate_enabled"))value=s->y_negate;
 else if(!strcmp(name,"graphics/origin_upper_left"))value=1; /* Native SPIR-V/IR3 FragCoord convention; viewport is separate. */
 else if(!strcmp(name,"graphics/halve_primitive_id"))value=s->halve_primitive_id;
 else if(!strcmp(name,"QueryHostSupportsDepthClipControl")||!strcmp(name,"QueryHostSupportsBgraFormat")||
         !strcmp(name,"QueryHostSupportsShaderBarrierDivergence"))value=1;
 else if(!strcmp(name,"QueryHostHasFrontFacingBug")) {
#if NN_PIPELINE_FRONT_FACING
  /* Translator query selects its Intel-only workaround, not a bool claim
   * about native GPU support. A650 FRONT_FACE is delivered by the exact
   * normal recipe's half scalar/FACEREGID/faceness state. Default stays OFF. */
  value=0;
#else
  return ENOTSUP;
#endif
 }
 else if(!strcmp(name,"QueryHostGatherBiasPrecision")){type=NN_PIPELINE_QUERY_I32;value=0;}
 else {
  /* Explicit portable common-IR compiler policy: unsupported optional features
   * use their existing translator lowerings or are rejected by the recipe. */
  static const char *const no[]={"QueryHostSupportsScaledVertexFormats","QueryHostSupportsTransformFeedback",
   "QueryHostSupportsLayerVertexTessellation","QueryHostSupportsShaderFloat64","QueryHostHasVectorIndexingBug",
   "QueryHostFragmentQuadTransfer","QueryHostReducedPrecision","QueryHostSupportsFragmentShaderInterlock",
   "QueryHostSupportsFragmentShaderOrderingIntel","QueryHostSupportsGeometryShaderPassthrough",
   "QueryHostSupportsShaderBallot","QueryHostSupportsShaderNonUniformIndexing","QueryHostSupportsTextureShadowLod",
   "QueryHostSupportsViewportMask","QueryHostSupportsNonConstantTextureOffset",
   "QueryHostSupportsSnormBufferTextureFormat"};
  unsigned found=0;for(size_t i=0;i<sizeof(no)/sizeof(no[0]);i++)if(!strcmp(name,no[i]))found=1;
  if(!found)return ENOTSUP;
 }
 if(type==NN_PIPELINE_QUERY_BOOL&&value>1)return EINVAL;
 return scalar(q,type,value,out,n);
}
