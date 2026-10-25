/* HOST ONLY: actual packets/addresses/BO refs; never a pixel or GPU oracle. */
#define main original_shader_main
#include "shader_recipe.c"
#undef main
#include "nvn_native/pipeline.h"

#include NN_PIPELINE_NATIVE_VIEWS
int main(void)
{
 CHECK(!original_shader_main());nn_device *d=NULL;CHECK(!nn_device_open(&d));
 const struct nn_offline_graphics_archive *a=&nn_offline_graphics;
 for(size_t test=0;test<NN_GRAPHICS_PIPELINE_ARCHIVE.pair_count;test++) {
  const struct nn_pipeline_shader_recipe *r=NN_GRAPHICS_PIPELINE_ARCHIVE.pairs[test].recipe;const struct nn_offline_graphics_pair *pair=NULL;
  for(size_t i=0;i<a->pair_count;i++)if(a->pairs[i].vs_bytes==r->code_bytes[0]&&a->pairs[i].fs_bytes==r->code_bytes[1]&&
   !memcmp(a->pairs[i].vs_code,r->code[0],r->code_bytes[0])&&!memcmp(a->pairs[i].fs_code,r->code[1],r->code_bytes[1]))pair=a->pairs+i;
  CHECK(pair);const struct nn_offline_graphics_stage *vs=a->stages+pair->vs_index,*fs=a->stages+pair->fs_index;
  nn_pipeline_program *p=NULL,*retained=NULL;CHECK(!nn_pipeline_program_create(d,a,vs,fs,vs->specialization,fs->specialization,&p,&retained));
  const struct nn_tex_native_view *tv=NN_TEXTURE_NATIVE_VIEWS+(test==2?2:0),*sv=NN_TEXTURE_NATIVE_VIEWS+1;
  nn_bo *target=NULL,*ubo=NULL,*texture=NULL,*index=NULL,*indirect=NULL,*count=NULL;
  CHECK(!nn_bo_alloc(d,tv->required_span_bytes,&target));CHECK(!nn_bo_alloc(d,1024,&ubo));
  CHECK(!nn_bo_alloc(d,sv->required_span_bytes,&texture));CHECK(!nn_bo_alloc(d,4096,&index));CHECK(!nn_bo_alloc(d,4096,&indirect));CHECK(!nn_bo_alloc(d,4096,&count));
  struct nn_pipeline_draw_state s={.target={tv,{target,0,tv->required_span_bytes}},.texture_bound=r->texture_mask,.sampler_bound=r->sampler_mask};
  CHECK(!nn_pipeline_full_raster(tv->width,tv->height,1,1,&s.raster));nn_render_state_defaults(&s.render);
  s.vertex.attribute_count=32;s.vertex.stream_count=32;s.vertex.streams[7]=(struct nn_pipeline_stream){80,0};s.vertex.buffers[7]=(struct nn_pipeline_buffer){ubo,64,960};
  for(size_t i=0;i<r->input_count;i++)s.vertex.attributes[r->inputs[i].location]=(struct nn_pipeline_attribute){0x2e,(uint32_t)i*16,7};
  struct nn_pipeline_stage_view stages[2];for(unsigned i=0;i<2;i++)CHECK(!nn_pipeline_program_stage(p,i,stages+i));
  for(size_t i=0;i<r->patch_count;i++){const struct nn_pipeline_address_patch *patch=r->patches+i;
   if(patch->role==NN_PIPELINE_UBO&&patch->binding!=stages[patch->stage].c1_native_binding)s.ubo[patch->stage][patch->binding]=(struct nn_pipeline_buffer){ubo,0,1024};}
  for(unsigned i=0;i<16;i++) {
   if(r->texture_mask&(1u<<i))s.textures[i]=(struct nn_pipeline_image){sv,{texture,0,sv->required_span_bytes}};
   if(r->sampler_mask&(1u<<i)){s.samplers[i]=(struct nn_sampler_state){.version=1,.min_filter=NVN_MIN_FILTER_NEAREST,.mag_filter=NVN_MAG_FILTER_NEAREST,.wrap={NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE},.compare_func=NVN_COMPARE_FUNC_ALWAYS,.anisotropy=1};s.sampler_layouts[i]=(struct nn_sampler_layout){sv->width,sv->height,1,1,2,1,0,0};}
  }
  for(unsigned mode=0;mode<3;mode++)for(unsigned indexed=0;indexed<2;indexed++) {
   s.draw=(struct nn_gfx_draw){.mode=mode,.primitive=4,.indexed=indexed,.index_type=1,.count=3,.instances=2,.base_instance=1,.max_draws=1,.stride=indexed?20:16};
   if(indexed)s.draw.index=(struct nn_gfx_buffer){index,0,6};
   if(mode)s.draw.indirect=(struct nn_gfx_buffer){indirect,0,4096};
   if(mode==2)s.draw.indirect_count=(struct nn_gfx_buffer){count,0,4};
   struct nn_pipeline_submission result;nn_bo *q=(void *)1;CHECK(!nn_pipeline_encode(d,p,r,&nn_pipeline_device,&s,&result,&q)&&!q&&result.job&&result.command_bytes);
   CHECK(result.command_bytes<32768);unsigned draws=0,targets=0,texture_bases=0;uint32_t *w=nn_bo_map(result.job);
   for(size_t at=0;at<result.command_bytes/4;){size_t n=nvn_pm4_packet_words(w[at]);CHECK(n&&n<=result.command_bytes/4-at);
    if((w[at]>>28)==7&&(((w[at]>>16)&127)==0x38||((w[at]>>16)&127)==0x2a))draws++;
    if(w[at]==nvn_pm4_packet4(0x8825,2)){uint64_t addr=(uint64_t)w[at+1]|(uint64_t)w[at+2]<<32;CHECK(addr==nn_bo_address(target));targets++;}
    if(w[at]==nvn_pm4_packet4(0xa9e4,2)){uint64_t addr=(uint64_t)w[at+1]|(uint64_t)w[at+2]<<32;CHECK(addr==nn_bo_address(result.job)+40960);texture_bases++;}
    at+=n;
   }
   CHECK(draws==1&&targets==1&&texture_bases==!!r->texture_mask);
   for(size_t i=0;i<result.resource_count;i++){CHECK(result.resources[i]);for(size_t j=0;j<i;j++)CHECK(result.resources[i]!=result.resources[j]);result.resources[i]->busy++;}
   CHECK(nn_pipeline_program_destroy(p)==EBUSY);for(size_t i=0;i<result.resource_count;i++)result.resources[i]->busy--;
   uint32_t *table=(uint32_t *)((unsigned char *)nn_bo_map(result.job)+40960);
   for(unsigned i=0;i<16;i++)for(unsigned j=0;j<16;j++) {
    if(!(r->texture_mask&(1u<<i)))CHECK(!table[i*16+j]);
    else if(j!=4&&j!=5)CHECK(table[i*16+j]==sv->texture_descriptor[j]);
   }
   CHECK(!nn_bo_destroy(result.job));
  }
  struct nn_pipeline_submission result;nn_bo *q=NULL;struct nn_pipeline_draw_state bad=s;bad.texture_bound^=1;
  CHECK(nn_pipeline_encode(d,p,r,&nn_pipeline_device,&bad,&result,&q)==ENOTSUP&&!result.job&&!result.resource_count&&!q);
  for(unsigned primitive=0;primitive<4;primitive++){bad=s;bad.draw.primitive=primitive;CHECK(nn_pipeline_encode(d,p,r,&nn_pipeline_device,&bad,&result,&q)==ENOTSUP&&!result.job&&!q);}
  bad=s;bad.raster.scissor_width=tv->width+1;CHECK(nn_pipeline_encode(d,p,r,&nn_pipeline_device,&bad,&result,&q)==EINVAL&&!result.job&&!q);
  struct nn_tex_native_view broken=*tv;broken.levels=2;bad=s;bad.target.layout=&broken;
  CHECK(nn_pipeline_encode(d,p,r,&nn_pipeline_device,&bad,&result,&q)==ENOTSUP&&!result.job&&!q);
  struct nn_pipeline_platform invalid=nn_pipeline_device;invalid.gmem_bytes+=4096;
  CHECK(nn_pipeline_encode(d,p,r,&invalid,&s,&result,&q)==EINVAL&&!result.job&&!q);
  fail_cache_at=cache_calls+1;fail_destroy_at=destroy_calls+1;
  CHECK(nn_pipeline_encode(d,p,r,&nn_pipeline_device,&s,&result,&q)==EIO&&!result.job&&!result.resource_count&&q);
  CHECK(!nn_bo_destroy(q));fail_cache_at=fail_destroy_at=0;
  CHECK(!nn_bo_destroy(count));CHECK(!nn_bo_destroy(indirect));CHECK(!nn_bo_destroy(index));CHECK(!nn_bo_destroy(texture));CHECK(!nn_bo_destroy(ubo));CHECK(!nn_bo_destroy(target));CHECK(!nn_pipeline_program_destroy(p));CHECK(!d->bos);
 }
 CHECK(!nn_device_close(d));printf("HOST ONLY dynamic pipeline checks=%u; GPU execution=false\n",checks);return 0;
}
