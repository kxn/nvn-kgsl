/* SPDX-License-Identifier: MIT. Pinned Gallium attachment/lateZ + A6 XML. */
#include "nvn_native/depth_native.h"
#include "nvn_native/pm4.h"
#include "nvn_native/render_state.h"
#include "fd6_hw.h"
#include <cerrno>
#include <cstring>
int nn_depth_native_recipe(const struct nn_pipeline_depth_export *d)
{
 if(!d||d->version!=1||!d->recipe||d->writes_depth>1||(d->writes_depth?d->depth_regid>=192:d->depth_regid!=252)||d->color_outputs>1||d->late_z_required!=1||d->lrz_disabled_required!=1)return ENOTSUP;
 const nn_pipeline_shader_recipe *r=d->recipe;
 if(r->version!=(d->writes_depth?NN_PIPELINE_DEPTH_RECIPE_VERSION:NN_PIPELINE_RECIPE_VERSION)||r->chip!=0x06050002||!r->words||!r->word_count||r->word_count>4096)return ENOTSUP;
 const uint32_t regs[]={REG_A6XX_SP_PS_OUTPUT_CNTL,REG_A6XX_RB_PS_OUTPUT_CNTL,REG_A6XX_RB_PS_MRT_CNTL,REG_A6XX_SP_PS_MRT_CNTL,REG_A6XX_SP_PS_OUTPUT_MASK,REG_A6XX_RB_PS_OUTPUT_MASK};
 const uint32_t values[]={A6XX_SP_PS_OUTPUT_CNTL_DEPTH_REGID(d->depth_regid)|A6XX_SP_PS_OUTPUT_CNTL_SAMPMASK_REGID(252)|A6XX_SP_PS_OUTPUT_CNTL_STENCILREF_REGID(252),(d->writes_depth?A6XX_RB_PS_OUTPUT_CNTL_FRAG_WRITES_Z:0u),A6XX_RB_PS_MRT_CNTL_MRT(d->color_outputs),A6XX_SP_PS_MRT_CNTL_MRT(d->color_outputs),d->color_outputs?15u:0u,d->color_outputs?15u:0u};
 uint32_t seen=0;
 for(size_t i=0;i<r->word_count;){uint32_t h=r->words[i],n=nvn_pm4_packet_words(h);if(!n||n>r->word_count-i)return EINVAL;
  if((h>>28)==7&&((h>>16)&127)!=50&&((h>>16)&127)!=52)return ENOTSUP;
  if((h>>28)==4){uint32_t base=(h>>8)&0x7ffffu;for(unsigned j=0;j<6;j++)if(regs[j]>=base&&regs[j]-base<n-1){if((seen&(1u<<j))||r->words[i+1+regs[j]-base]!=values[j])return EINVAL;seen|=1u<<j;}}
  i+=n;
 }
 return seen==63?0:EINVAL;
}
static int native_words(const struct nn_depth_fdl_view*vptr,const struct nn_pipeline_depth_export*d,
 const struct nn_depth_controls_desc*c,uint32_t*out,size_t capacity,size_t*count)
{
 if(!count)return EINVAL;
 *count=0;if(!vptr||!d||!c)return EINVAL;
 int e=nn_depth_native_recipe(d);if(e)return e;
 if(c->shader_writes_depth!=d->writes_depth||c->depth_regid!=d->depth_regid)return ENOTSUP;
 struct nn_depth_fdl_view actual;if((e=nn_depth_fdl(&vptr->desc,&actual)))return e;
 if(memcmp(&actual,vptr,sizeof(actual)))return EINVAL;
 if(!(vptr->desc.roles&NN_DEPTH_ATTACHMENT)||vptr->desc.chip!=c->chip)return ENOTSUP;
 nn_depth_controls control;if((e=nn_depth_fdl_controls(c,&control)))return e;
 uint32_t words[96];size_t n=0;
 auto reg=[&](uint32_t r,uint32_t v){words[n++]=nvn_pm4_packet4(r,1);words[n++]=v;};
 auto addr=[&](uint32_t r,uint64_t v){words[n++]=nvn_pm4_packet4(r,2);words[n++]=(uint32_t)v;words[n++]=(uint32_t)(v>>32);};
 const nn_depth_fdl_view &v=*vptr;uint64_t base=v.desc.address+v.view_offset;
 reg(REG_A6XX_RB_DEPTH_BUFFER_INFO,v.rb_depth_buffer_info);reg(REG_A6XX_RB_DEPTH_BUFFER_PITCH,v.rb_depth_buffer_pitch);reg(REG_A6XX_RB_DEPTH_BUFFER_ARRAY_PITCH,v.rb_depth_buffer_array_pitch);addr(REG_A6XX_RB_DEPTH_BUFFER_BASE,base);reg(REG_A6XX_RB_DEPTH_GMEM_BASE,0);
 reg(REG_A6XX_GRAS_SU_DEPTH_BUFFER_INFO,v.gras_su_depth_buffer_info);addr(REG_A6XX_RB_DEPTH_FLAG_BUFFER_BASE,0);reg(REG_A6XX_RB_DEPTH_FLAG_BUFFER_PITCH,0);
 reg(REG_A6XX_RB_STENCIL_CNTL,0);reg(REG_A6XX_GRAS_SU_STENCIL_CNTL,0);reg(REG_A6XX_RB_STENCIL_BUFFER_INFO,0);reg(REG_A6XX_RB_STENCIL_BUFFER_PITCH,0);reg(REG_A6XX_RB_STENCIL_BUFFER_ARRAY_PITCH,0);addr(REG_A6XX_RB_STENCIL_BUFFER_BASE,0);reg(REG_A6XX_RB_STENCIL_GMEM_BASE,0);
 reg(REG_A6XX_RB_LRZ_CNTL,control.rb_lrz_cntl);reg(REG_A6XX_GRAS_LRZ_CNTL,control.gras_lrz_cntl);reg(REG_A6XX_RB_DEPTH_PLANE_CNTL,control.rb_depth_plane_cntl);reg(REG_A6XX_GRAS_SU_DEPTH_PLANE_CNTL,control.gras_su_depth_plane_cntl);
 reg(REG_A6XX_RB_DEPTH_CNTL,control.rb_depth_cntl);reg(REG_A6XX_GRAS_SU_DEPTH_CNTL,control.gras_su_depth_cntl);reg(REG_A6XX_SP_PS_OUTPUT_CNTL,control.sp_ps_output_cntl);reg(REG_A6XX_RB_PS_OUTPUT_CNTL,control.rb_ps_output_cntl);
 if(n>96)return EOVERFLOW;
 if(!out&&!capacity){*count=n;return 0;}
 if(!out||capacity<n)return E2BIG;
 memcpy(out,words,n*4);*count=n;return 0;
}

int nn_depth_native_plan(const struct nn_depth_fdl_view*v,const struct nn_pipeline_depth_export*d,const struct nn_depth_controls_desc*c,size_t*count)
{
 if(!count)return EINVAL;
 *count=0;if(!v||v->desc.address)return EINVAL;
 return native_words(v,d,c,NULL,0,count);
}
int nn_depth_native_words(const struct nn_depth_attachment*a,const struct nn_pipeline_depth_export*d,const struct nn_depth_controls_desc*c,uint32_t*out,size_t capacity,size_t*count)
{
 if(!count)return EINVAL;
 *count=0;int e=nn_depth_attachment_validate(a);if(e)return e;
 return native_words(&a->view,d,c,out,capacity,count);
}

int nn_depth_native_render(const struct nn_render_state*s,const struct nn_depth_controls_desc*c,struct nn_render_state*out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));if(!s||!c)return EINVAL;
 nn_depth_controls v;int e=nn_depth_fdl_controls(c,&v);if(e)return e;
 if(s->version!=NN_RENDER_STATE_VERSION||s->depth.depth_test!=c->test_enable||s->depth.depth_func!=c->compare||
    (s->depth.depth_test&&s->depth.depth_write)!=c->write_enable)return EINVAL;
 if(s->depth.stencil_test||s->depth_bounds_enable)return ENOTSUP;
 *out=*s;out->depth.depth_test=0;return 0;
}
