/* HOST only. Actual native producer/FDL/common encoder, RAM faults; no GPU. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_alloc ram_alloc
#define nn_bo_map ram_map
#define nn_bo_destroy ram_destroy
#include NN_MEMORY_BACKEND
#undef nn_bo_alloc
#undef nn_bo_map
#undef nn_bo_destroy
#include "nvn_native/depth_native.h"
#include "nvn_native/pipeline.h"
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/pm4.h"
#include NN_DEPTH_NATIVE_ARTIFACT
static unsigned checks,alloc_calls,destroy_calls,fail_alloc_at,fail_alloc_owner_at,fail_destroy_at,map_null_new;
static nn_bo*last_alloc;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%u: %s\n",__LINE__,#x);abort();}}while(0)
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){++alloc_calls;*out=NULL;if(alloc_calls==fail_alloc_at)return ENOMEM;int e=ram_alloc(d,n,out);last_alloc=*out;return e?e:alloc_calls==fail_alloc_owner_at?EIO:0;}
void*nn_bo_map(const nn_bo*b){return map_null_new&&b==last_alloc?NULL:ram_map(b);}
int nn_bo_destroy(nn_bo*b){if(++destroy_calls==fail_destroy_at)return EIO;return ram_destroy(b);}

#ifndef NN_TEST_DEPTH_OFF
static struct nn_tex_native_view target_view(unsigned width,unsigned height){
 struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=width,.height=height,.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=NN_FDL_SAMPLED|NN_FDL_COLOR_TARGET};struct nn_fdl_image_view a;CHECK(!nn_fdl_image(&d,&a));
 struct nn_tex_native_view v={.schema=1,.chip=a.chip,.format=a.format,.target=1,.levels=1,.layers=1,.samples=1,.width=a.width,.height=a.height,.pitch=a.level[0].pitch,.layer_bytes=a.level[0].layer_stride,.bytes=a.bytes,.alignment=a.alignment,.required_span_bytes=a.required_span,.descriptor_address_word=a.descriptor_address_word,.descriptor_address_mask=a.descriptor_address_mask,.rb_mrt_buf_info=a.rb_mrt_buf_info,.rb_mrt_pitch=a.rb_mrt_pitch,.rb_mrt_array_pitch=a.rb_mrt_array_pitch,.sp_ps_mrt_reg=a.sp_ps_mrt_reg};memcpy(v.texture_descriptor,a.descriptor,64);return v;
}
static void reject(nn_device*d,nn_pipeline_program*p,const struct nn_pipeline_depth_export*depth,const struct nn_pipeline_draw_state*s,const struct nn_depth_attachment*a,const struct nn_depth_controls_desc*c){
 unsigned old=alloc_calls;struct nn_pipeline_submission out;nn_bo*retained=(void*)1;CHECK(nn_pipeline_encode_depth(d,p,depth,&nn_pipeline_device,s,a,c,&out,&retained)!=0);CHECK(!out.job&&!out.resource_count&&!retained&&alloc_calls==old);
}
static void encoded(const struct nn_pipeline_submission*s,const struct nn_depth_attachment*a,const struct nn_depth_controls_desc*c){
 const uint32_t*w=ram_map(s->job);size_t n=s->command_bytes/4;uint32_t cntl=0,sp=0,rb=0,base[2]={0};unsigned draws=0;
 for(size_t i=0;i<n;){uint32_t h=w[i],len=nvn_pm4_packet_words(h);CHECK(len&&len<=n-i);
  if(h>>28==4){uint32_t reg=(h>>8)&0x7ffff;for(unsigned j=1;j<len;j++){uint32_t r=reg+j-1;if(r==0x8871)cntl=w[i+j];if(r==0xa98c)sp=w[i+j];if(r==0x880b)rb=w[i+j];if(r==0x8875)base[0]=w[i+j];if(r==0x8876)base[1]=w[i+j];}}
  else if(((h>>16)&127)==0x38){draws++;CHECK(cntl==((c->test_enable?65u:0u)|(c->write_enable?2u:0u)|((c->compare-1)<<2)));CHECK(sp==((c->depth_regid<<8)|(252u<<16)|(252u<<24))&&rb==2);CHECK(((uint64_t)base[0]|(uint64_t)base[1]<<32)==a->view.desc.address+a->view.view_offset);}
  i+=len;
 }CHECK(draws==1);
}

#endif
int main(void){
 nn_device*d=NULL;CHECK(!nn_device_open(&d));const struct nn_pipeline_native_artifact*artifacts[]={&depth_color_artifact,&depth_only_artifact};const struct nn_pipeline_depth_export*exports[]={&nn_pipeline_depth_color,&nn_pipeline_depth_only};unsigned jobs=0,zero_color_jobs=0;
 for(unsigned mode=0;mode<2;mode++){
  const struct nn_pipeline_native_artifact*artifact=artifacts[mode];const struct nn_pipeline_depth_export*depth=exports[mode];nn_pipeline_program*p=NULL,*retained=NULL;
  CHECK(nn_pipeline_program_create_native(d,artifact,&p,&retained)==ENOTSUP&&!p&&!retained);
#ifdef NN_TEST_DEPTH_OFF
  unsigned before=alloc_calls;CHECK(nn_pipeline_program_create_native_depth(d,artifact,depth,&p,&retained)==ENOTSUP&&!p&&!retained&&alloc_calls==before);continue;
#else
  CHECK(!nn_pipeline_program_create_native_depth(d,artifact,depth,&p,&retained)&&p&&!retained);CHECK(nn_pipeline_program_depth_export(p)==depth);
  const struct nn_offline_graphics_stage*stages[2];for(unsigned i=0;i<2;i++){struct nn_pipeline_stage_view v;CHECK(!nn_pipeline_program_stage(p,i,&v));stages[i]=v.source;}
  for(unsigned format=0x32;format<=0x34;format++)for(unsigned dim=0;dim<2;dim++){
   unsigned width=dim?73:64,height=dim?41:64;struct nn_tex_native_view view=target_view(width,height);nn_bo*target=NULL,*vertices=NULL,*uniform=NULL;CHECK(!nn_bo_alloc(d,view.required_span_bytes,&target));CHECK(!nn_bo_alloc(d,4096,&vertices));CHECK(!nn_bo_alloc(d,4096,&uniform));
   struct nn_depth_attachment attachment={0};struct nn_depth_fdl_desc desc={.schema=1,.chip=0x06050002,.format=format,.target=1,.width=width,.height=height,.levels=1,.layers=1,.roles=NN_DEPTH_ATTACHMENT};CHECK(!nn_depth_attachment_create(d,&desc,&attachment));
   struct nn_pipeline_draw_state s={.target={&view,{target,0,view.required_span_bytes}},.texture_types=artifact->textures};CHECK(!nn_pipeline_full_raster(width,height,0,1,&s.raster));nn_render_state_defaults(&s.render);s.render.depth.depth_test=1;s.render.depth.depth_func=2;s.vertex.attribute_count=2;s.vertex.stream_count=1;s.vertex.streams[0]=(struct nn_pipeline_stream){28,0};s.vertex.buffers[0]=(struct nn_pipeline_buffer){vertices,0,4096};s.vertex.attributes[0]=(struct nn_pipeline_attribute){0x22,0,0};s.vertex.attributes[1]=(struct nn_pipeline_attribute){0x2e,12,0};s.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
   for(size_t j=0;j<artifact->recipe->patch_count;j++){const struct nn_pipeline_address_patch*q=artifact->recipe->patches+j;if(q->role==NN_PIPELINE_UBO)s.ubo[q->stage][q->binding]=(struct nn_pipeline_buffer){uniform,256*q->stage,64};}
   struct nn_depth_controls_desc controls={1,0x06050002,1,1,2,1,depth->depth_regid,0};CHECK(!nn_pipeline_preflight_depth(d,stages,depth,&nn_pipeline_device,&s,&attachment,&controls));
   for(unsigned repeat=0;repeat<2;repeat++){
    struct nn_pipeline_submission out;nn_bo*job_retained=NULL;CHECK(!nn_pipeline_encode_depth(d,p,depth,&nn_pipeline_device,&s,&attachment,&controls,&out,&job_retained)&&out.job&&!job_retained);jobs++;encoded(&out,&attachment,&controls);unsigned found=0;for(size_t i=0;i<out.resource_count;i++){found+=out.resources[i]==attachment.storage;out.resources[i]->busy++;}CHECK(found==1&&out.resource_count<=128);CHECK(nn_depth_attachment_destroy(&attachment)==EBUSY&&nn_pipeline_program_destroy(p)==EBUSY);for(size_t i=0;i<out.resource_count;i++)out.resources[i]->busy--;CHECK(!nn_bo_destroy(out.job));
   }
   struct nn_pipeline_submission old_out;nn_bo*old_retained=NULL;unsigned prior=alloc_calls;CHECK(nn_pipeline_encode(d,p,artifact->recipe,&nn_pipeline_device,&s,&old_out,&old_retained)==ENOTSUP&&!old_out.job&&!old_retained&&alloc_calls==prior);
   struct nn_pipeline_depth_export wrong=*depth;reject(d,p,&wrong,&s,&attachment,&controls);struct nn_pipeline_draw_state bad=s;bad.render.depth.depth_test=0;reject(d,p,depth,&bad,&attachment,&controls);bad=s;bad.ubo[1][2]=(struct nn_pipeline_buffer){attachment.storage,0,64};reject(d,p,depth,&bad,&attachment,&controls);bad=s;bad.vertex.buffers[0]=(struct nn_pipeline_buffer){attachment.storage,0,64};reject(d,p,depth,&bad,&attachment,&controls);struct nn_depth_controls_desc c=controls;c.depth_regid^=1;reject(d,p,depth,&s,&attachment,&c);c=controls;c.write_enable=0;reject(d,p,depth,&s,&attachment,&c);bad=s;bad.render.depth.stencil_test=1;reject(d,p,depth,&bad,&attachment,&controls);bad=s;bad.render.depth_bounds_enable=1;reject(d,p,depth,&bad,&attachment,&controls);
   struct nn_pipeline_submission out;nn_bo*ret=NULL;prior=d->bos;fail_alloc_at=alloc_calls+1;CHECK(nn_pipeline_encode_depth(d,p,depth,&nn_pipeline_device,&s,&attachment,&controls,&out,&ret)==ENOMEM&&!out.job&&!ret&&d->bos==prior);fail_alloc_at=0;
   fail_alloc_owner_at=alloc_calls+1;CHECK(nn_pipeline_encode_depth(d,p,depth,&nn_pipeline_device,&s,&attachment,&controls,&out,&ret)==EIO&&!out.job&&ret&&d->bos==prior+1);fail_alloc_owner_at=0;CHECK(!nn_bo_destroy(ret));ret=NULL;
   map_null_new=1;fail_destroy_at=destroy_calls+1;CHECK(nn_pipeline_encode_depth(d,p,depth,&nn_pipeline_device,&s,&attachment,&controls,&out,&ret)!=0&&!out.job&&ret&&d->bos==prior+1);map_null_new=0;fail_destroy_at=0;CHECK(!nn_bo_destroy(ret)&&d->bos==prior);
   if(mode==1){
    struct nn_pipeline_draw_state zero=s;zero.target=(struct nn_pipeline_image){0};
    CHECK(!nn_bo_destroy(target));target=NULL;
    CHECK(!nn_pipeline_preflight_depth(d,stages,depth,&nn_pipeline_device,&zero,&attachment,&controls));
    for(unsigned replay=0;replay<2;replay++){
     unsigned before=d->bos;struct nn_pipeline_submission work;nn_bo*keep=NULL;
     CHECK(!nn_pipeline_encode_depth(d,p,depth,&nn_pipeline_device,&zero,&attachment,&controls,&work,&keep)&&work.job&&!keep&&d->bos==before+1);
     encoded(&work,&attachment,&controls);jobs++;zero_color_jobs++;CHECK(work.resource_count==6);
     const uint32_t*w=ram_map(work.job);unsigned mrt=0,mask=99,reads=99;
     for(size_t at=0;at<work.command_bytes/4;){size_t n=nvn_pm4_packet_words(w[at]);CHECK(n&&n<=work.command_bytes/4-at);
      if(w[at]>>28==4){unsigned base=(w[at]>>8)&0x7ffff;for(unsigned j=1;j<n;j++){
       unsigned reg=base+j-1;CHECK(reg!=0x8825&&reg!=0x8826&&reg!=0x8903&&reg!=0x8904); /* no color base address */
       if(reg==0x8820){mask=w[at+j];}
       if(reg==0xa989){reads=w[at+j];}
       if(reg==0xa98d||reg==0x880c){CHECK(w[at+j]==0);mrt++;}
      }}at+=n;
     }
     CHECK(mrt==2&&!(mask&0x783)&&reads==0);CHECK(!nn_bo_destroy(work.job)&&d->bos==before);
    }
    struct nn_pipeline_draw_state bad_zero=zero;bad_zero.target.buffer.bytes=64;reject(d,p,depth,&bad_zero,&attachment,&controls);
    bad_zero=zero;bad_zero.raster.scissor_width=width+1;reject(d,p,depth,&bad_zero,&attachment,&controls);
   }else{struct nn_pipeline_draw_state zero=s;zero.target=(struct nn_pipeline_image){0};reject(d,p,depth,&zero,&attachment,&controls);}
   CHECK(!nn_depth_attachment_destroy(&attachment));if(target)CHECK(!nn_bo_destroy(target));CHECK(!nn_bo_destroy(vertices)&&!nn_bo_destroy(uniform));
  }
  CHECK(!nn_pipeline_program_destroy(p));
#endif
 }
 CHECK(!d->bos);CHECK(!nn_device_close(d));printf("{\"scope\":\"HOST native depth runtime/faults, no GPU\",\"checks\":%u,\"encoded_jobs\":%u,\"zero_color_jobs\":%u,\"gpu_execution\":false}\n",checks,jobs,zero_color_jobs);return 0;
}
