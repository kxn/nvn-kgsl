/* HOST ONLY: actual common-IR3 shader recipe relocation, no GPU execution. */
#pragma push_macro("main")
#undef main
#define main original_owner_main
#include "program_owner.c"
#undef main
#pragma pop_macro("main")
#include "nvn_native/pipeline_shader.h"
#include "nvn_native/pm4.h"
#include "nvn_native/pipeline_vertex.h"
#include NN_PIPELINE_ARCHIVE
static void unchanged(nn_bo *job) {for(size_t i=0;i<nn_bo_size(job);i++)CHECK(((unsigned char *)nn_bo_map(job))[i]==0xa5);}
int main(void)
{
 CHECK(!original_owner_main());nn_device *d=NULL;CHECK(!nn_device_open(&d));
 const struct nn_offline_graphics_archive *a=&nn_offline_graphics;
 for(size_t test=0;test<NN_GRAPHICS_PIPELINE_ARCHIVE.pair_count;test++) {
  const struct nn_pipeline_shader_recipe *r=NN_GRAPHICS_PIPELINE_ARCHIVE.pairs[test].recipe;const struct nn_offline_graphics_pair *pair=NULL;
  for(size_t i=0;i<a->pair_count;i++)if(a->pairs[i].vs_bytes==r->code_bytes[0]&&a->pairs[i].fs_bytes==r->code_bytes[1]&&
    !memcmp(a->pairs[i].vs_code,r->code[0],r->code_bytes[0])&&!memcmp(a->pairs[i].fs_code,r->code[1],r->code_bytes[1]))pair=a->pairs+i;
  CHECK(pair);const struct nn_offline_graphics_stage *vs=a->stages+pair->vs_index,*fs=a->stages+pair->fs_index;
  nn_pipeline_program *p=NULL,*retained=NULL;CHECK(!nn_pipeline_program_create(d,a,vs,fs,vs->specialization,fs->specialization,&p,&retained)&&!retained);
  nn_bo *job=NULL,*ubo=NULL;CHECK(!nn_bo_alloc(d,16384,&job));CHECK(!nn_bo_alloc(d,1024,&ubo));
  struct nn_pipeline_shader_bindings b={.job=job,.command_offset=64,.sampler_offset=8192,.texture_offset=12288};
  struct nn_pipeline_stage_view stages[2];for(unsigned s=0;s<2;s++)CHECK(!nn_pipeline_program_stage(p,s,stages+s));
  for(size_t i=0;i<r->patch_count;i++){const struct nn_pipeline_address_patch *patch=r->patches+i;
   if(patch->role==NN_PIPELINE_UBO&&patch->binding!=stages[patch->stage].c1_native_binding)b.ubo[patch->stage][patch->binding]=(struct nn_pipeline_buffer){ubo,0,1024};}
  size_t count=0,words=999;nn_bo *resources[64];CHECK(!nn_pipeline_shader_resources(p,r,&b,resources,64,&count));CHECK(count==d->bos);
  memset(nn_bo_map(job),0xa5,nn_bo_size(job));CHECK(nn_pipeline_shader_encode(p,r,&b,r->word_count-1,&words)==ENOSPC&&!words);unchanged(job);
  CHECK(!nn_pipeline_shader_encode(p,r,&b,r->word_count,&words)&&words==r->word_count);
  uint32_t *encoded=(uint32_t *)((unsigned char *)nn_bo_map(job)+b.command_offset);
  for(size_t i=0;i<r->patch_count;i++) {
   const struct nn_pipeline_address_patch *patch=r->patches+i;uint64_t expected=0;
   if(patch->role==NN_PIPELINE_CODE)expected=nn_bo_address(stages[patch->stage].code);
   else if(patch->role==NN_PIPELINE_UBO)expected=nn_bo_address(patch->binding==stages[patch->stage].c1_native_binding?stages[patch->stage].c1:ubo)+patch->offset;
   else expected=nn_bo_address(job)+(patch->role==NN_PIPELINE_SAMPLERS?b.sampler_offset:b.texture_offset);
   CHECK(encoded[patch->word]==(uint32_t)expected&&encoded[patch->word+1]==(uint32_t)(expected>>32));
  }
  memset(nn_bo_map(job),0xa5,nn_bo_size(job));struct nn_pipeline_shader_recipe bad=*r;bad.chip=0x43030b00;
  CHECK(nn_pipeline_shader_encode(p,&bad,&b,4096,&words)==EINVAL&&!words);unchanged(job);
  uint32_t code[16384];memcpy(code,r->code[0],r->code_bytes[0]);code[0]^=1;bad=*r;bad.code[0]=code;
  CHECK(nn_pipeline_shader_encode(p,&bad,&b,4096,&words)==ENOTSUP&&!words);unchanged(job);
  uint32_t command[4096];memcpy(command,r->words,r->word_count*4);command[0]=nvn_pm4_packet7(0x38,0);bad=*r;bad.words=command;
  CHECK(nn_pipeline_shader_encode(p,&bad,&b,4096,&words)==ENOTSUP&&!words);unchanged(job);
  struct nn_pipeline_address_patch patches[128];memcpy(patches,r->patches,r->patch_count*sizeof(*patches));patches[1].word=patches[0].word;bad=*r;bad.patches=patches;
  CHECK(nn_pipeline_shader_encode(p,&bad,&b,4096,&words)==EINVAL&&!words);unchanged(job);
  struct nn_pipeline_shader_bindings invalid=b;invalid.command_offset=nn_bo_size(job);
  CHECK(nn_pipeline_shader_encode(p,r,&invalid,4096,&words)==EINVAL&&!words);unchanged(job);
  if(r->texture_table_entries){invalid=b;invalid.texture_offset=b.sampler_offset;CHECK(nn_pipeline_shader_encode(p,r,&invalid,4096,&words)==EINVAL&&!words);unchanged(job);}
  for(size_t i=0;i<r->patch_count;i++)if(r->patches[i].role==NN_PIPELINE_UBO) {
   const struct nn_pipeline_address_patch *patch=r->patches+i;invalid=b;
   invalid.ubo[patch->stage][patch->binding]=(struct nn_pipeline_buffer){ubo,0,patch->offset+patch->bytes-1};
   CHECK(nn_pipeline_shader_encode(p,r,&invalid,4096,&words)==EINVAL&&!words);unchanged(job);
   invalid=b;invalid.ubo[patch->stage][patch->binding]=(struct nn_pipeline_buffer){job,0,1024};
   CHECK(nn_pipeline_shader_encode(p,r,&invalid,4096,&words)==EINVAL&&!words);unchanged(job);
  }
  struct nn_pipeline_vertex_state vertex={.attribute_count=32,.stream_count=32};
  for(size_t i=0;i<r->input_count;i++) {
   unsigned location=r->inputs[i].location;vertex.attributes[location]=(struct nn_pipeline_attribute){0x2e,(uint32_t)i*16,7};
  }
  vertex.streams[7]=(struct nn_pipeline_stream){80,3};vertex.buffers[7]=(struct nn_pipeline_buffer){ubo,64,960};
  size_t vwords=0;CHECK(!nn_pipeline_vertex_encode(r,&vertex,NULL,0,&vwords));CHECK(vwords>64&&vwords<=512);
  uint32_t vbuf[512];memset(vbuf,0xa5,sizeof(vbuf));CHECK(nn_pipeline_vertex_encode(r,&vertex,vbuf,vwords-1,&words)==ENOSPC&&!words);
  for(size_t j=0;j<512;j++)CHECK(vbuf[j]==0xa5a5a5a5u);
  CHECK(!nn_pipeline_vertex_encode(r,&vertex,vbuf,512,&words)&&words==vwords);
  unsigned fetches=0,boundstreams=0;for(size_t at=0;at<vwords;){unsigned length=nvn_pm4_packet_words(vbuf[at]);CHECK(length&&length<=vwords-at);
   uint32_t reg=(vbuf[at]>>8)&0x7ffffu;
   if(reg==0xa010+4*7){CHECK(length==3);uint64_t address=nn_bo_address(ubo)+64;CHECK(vbuf[at+1]==(uint32_t)address&&vbuf[at+2]==address>>32);boundstreams++;}
   if(reg>=0xa090&&reg<0xa0d0&&!(reg&1)){unsigned i=(reg-0xa090)/2;CHECK(vbuf[at+1]==(7u|((uint32_t)i*16<<5)|0x20000u|(130u<<20)|0xc0000000u));fetches++;}
   at+=length;}
  CHECK(fetches==r->input_count&&boundstreams==1);
  /* Actual normalized native format fields, without a pixel/GPU claim. */
  const uint32_t formats[]={0x25,0x26,0xa,0x16,0x22,0x2e},native[]={48,50,74,103,116,130};
  for(unsigned f=0;f<6;f++) {
   struct nn_pipeline_vertex_state changed=vertex;
   for(size_t i=0;i<r->input_count;i++)changed.attributes[r->inputs[i].location].format=formats[f];
   CHECK(!nn_pipeline_vertex_encode(r,&changed,vbuf,512,&words));unsigned seen=0;
   for(size_t at=0;at<words;){unsigned length=nvn_pm4_packet_words(vbuf[at]);CHECK(length&&length<=words-at);uint32_t address=(vbuf[at]>>8)&0x7ffffu;
    if(address>=0xa090&&address<0xa0d0&&!(address&1)){CHECK(((vbuf[at+1]>>20)&255)==native[f]);seen++;}at+=length;}
   CHECK(seen==r->input_count);
  }
  struct nn_pipeline_vertex_state badvertex=vertex;unsigned location=r->inputs[0].location;badvertex.attributes[location].format=0xff;
  CHECK(nn_pipeline_vertex_encode(r,&badvertex,vbuf,512,&words)==ENOTSUP&&!words);
  badvertex=vertex;badvertex.attributes[location].offset=4096;CHECK(nn_pipeline_vertex_encode(r,&badvertex,vbuf,512,&words)==EINVAL&&!words);
  badvertex=vertex;badvertex.attributes[location].stream=32;CHECK(nn_pipeline_vertex_encode(r,&badvertex,vbuf,512,&words)==EINVAL&&!words);
  badvertex=vertex;badvertex.buffers[7].bytes=3;CHECK(nn_pipeline_vertex_encode(r,&badvertex,vbuf,512,&words)==EINVAL&&!words);
  badvertex=vertex;badvertex.streams[7].stride=3;CHECK(nn_pipeline_vertex_encode(r,&badvertex,vbuf,512,&words)==EINVAL&&!words);
  CHECK(!nn_bo_destroy(ubo));CHECK(!nn_bo_destroy(job));CHECK(!nn_pipeline_program_destroy(p));CHECK(!d->bos);
 }
 CHECK(!nn_device_close(d));printf("HOST ONLY generic shader recipe checks=%u; GPU execution=false\n",checks);return 0;
}
