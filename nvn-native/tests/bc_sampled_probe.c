/* SPDX-License-Identifier: MIT. Actual raw-native BC GPU qualifier candidate.
 * No NVN calls/game frame/CPU decoding; original strictly identified VS/FS.
 * Root alone deploys/runs. A cross-link does not claim GPU execution.
 */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/pipeline_archive.h"
#include "nvn_native/pipeline.h"
#include "nvn_native/texture_bc_shadow.h"
#include "nvn_native/texture_bridge.h"
#include "bc_sampled_cases.h"
#include <nvn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NN_BC_SAMPLED_QUALIFICATION
struct owner {
 nn_device *device;nn_queue *queue;nn_pipeline_program *program,*retained_program;
 nn_bo *canonical,*target,*vertex,*uniform,*wrapper,*retained;
 unsigned char *source_snapshot;
 struct nn_pipeline_submission graphics;struct nn_bc_shadow bc;
 struct owner *next;
};
static struct owner *quarantine;
static int release(struct owner *o)
{
 int e;if(o->queue){e=nn_queue_destroy(o->queue);if(e)return e;o->queue=NULL;}
 if(o->wrapper){e=nn_bo_destroy(o->wrapper);if(e)return e;o->wrapper=NULL;}
 if(o->graphics.job){e=nn_bo_destroy(o->graphics.job);if(e)return e;memset(&o->graphics,0,sizeof(o->graphics));}
 if(o->retained){e=nn_bo_destroy(o->retained);if(e)return e;o->retained=NULL;}
 if((e=nn_bc_shadow_destroy(&o->bc)))return e;
 nn_bo **list[]={&o->target,&o->canonical,&o->vertex,&o->uniform};
 for(unsigned i=0;i<4;i++)if(*list[i]){e=nn_bo_destroy(*list[i]);if(e)return e;*list[i]=NULL;}
 if(o->program){e=nn_pipeline_program_destroy(o->program);if(e)return e;o->program=NULL;}
 if(o->retained_program){e=nn_pipeline_program_destroy(o->retained_program);if(e)return e;o->retained_program=NULL;}
 if(o->device){e=nn_device_close(o->device);if(e)return e;o->device=NULL;}
 free(o->source_snapshot);o->source_snapshot=NULL;return 0;
}
static int save(const char *dir,unsigned test,unsigned repeat,const unsigned char *data,unsigned width,unsigned height,unsigned pitch)
{
 char name[1024];int n=snprintf(name,sizeof(name),"%s/bc-%02u-%u.rgba",dir,test,repeat);
 if(n<0||(size_t)n>=sizeof(name))return ENAMETOOLONG;
 int fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return errno;
 int e=0;for(unsigned y=0;y<height&&!e;y++)for(size_t at=0;at<width*4;){ssize_t b=write(fd,data+y*pitch+at,width*4-at);
  if(b<0&&errno==EINTR)continue;if(b<=0){e=b<0?errno:EIO;break;}at+=(size_t)b;}
 if(close(fd)&&!e)e=errno;return e;
}
static int refs_add(nn_bo **list,size_t *count,nn_bo *bo)
{if(!bo)return EINVAL;for(size_t i=0;i<*count;i++)if(list[i]==bo)return 0;if(*count==64)return EOVERFLOW;list[(*count)++]=bo;return 0;}
static int target_layout(unsigned width,unsigned height,struct nn_tex_native_view *v)
{
 struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=width,.height=height,
  .levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=NN_FDL_COLOR_TARGET|NN_FDL_SAMPLED};struct nn_fdl_image_view f;
 int e=nn_fdl_image(&d,&f);if(e)return e;
 *v=(struct nn_tex_native_view){.schema=1,.chip=f.chip,.format=37,.target=1,.levels=1,.layers=1,.samples=1,
  .width=width,.height=height,.pitch=f.level[0].pitch,.layer_bytes=f.level[0].layer_stride,.bytes=(uint32_t)f.bytes,
  .alignment=f.alignment,.required_span_bytes=(uint32_t)f.required_span,.descriptor_address_word=f.descriptor_address_word,
  .descriptor_offset=(uint32_t)f.view_offset,.descriptor_address_mask=f.descriptor_address_mask,.rb_mrt_buf_info=f.rb_mrt_buf_info,
  .rb_mrt_pitch=f.rb_mrt_pitch,.rb_mrt_array_pitch=f.rb_mrt_array_pitch,.sp_ps_mrt_reg=f.sp_ps_mrt_reg};
 memcpy(v->texture_descriptor,f.descriptor,64);return 0;
}
int nn_bc_sampled_probe(const char *directory,char *json,size_t capacity)
{
 if(!directory||!json||capacity<4096)return EINVAL;
 struct owner *o=calloc(1,sizeof(*o));if(!o)return ENOMEM;
 unsigned submits=0,finishes=0,completed=0,files=0,replays=0,max_error=0,bad_case=~0u,bad_x=~0u,bad_y=~0u,bad_channel=~0u,bad_actual=0,bad_expected=0;
 uint64_t channels=0,guards=0,readonly=0;int e=0,cleanup=0,unknown=0;const char *stage="contract";
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 if(NN_BC_SAMPLED_PAIR>=archive->pair_count){e=ENOTSUP;goto done;}
 const struct nn_pipeline_pair_recipe *pair=archive->pairs+NN_BC_SAMPLED_PAIR;const struct nn_pipeline_shader_recipe *recipe=pair->recipe;
 if(!recipe||recipe->texture_mask!=2||recipe->sampler_mask!=2||recipe->input_count!=3||!recipe->has_kill||recipe->fragcoord_mask){e=ENOTSUP;goto done;}
 const struct nn_offline_graphics_stage *vs=archive->stages->stages+pair->vs_index,*fs=archive->stages->stages+pair->fs_index;
 stage="device";if((e=nn_device_open(&o->device)))goto done;
 if(nn_device_chip(o->device)!=0x06050002||nn_device_gmem_bytes(o->device)!=archive->platform->gmem_bytes){e=ENOTSUP;goto done;}
 stage="program";if((e=nn_pipeline_program_create(o->device,archive->stages,vs,fs,vs->specialization,fs->specialization,&o->program,&o->retained_program)))goto done;
 if((e=nn_queue_create(o->device,&o->queue))||(e=nn_bo_alloc(o->device,4096,&o->vertex))||(e=nn_bo_alloc(o->device,4096,&o->uniform)))goto done;
 memset(nn_bo_map(o->vertex),0xa5,4096);memset(nn_bo_map(o->uniform),0xa5,4096);
 const float vertices[3][8]={{-1,-1,0,0,1,1,1,1},{3,-1,2,0,1,1,1,1},{-1,3,0,2,1,1,1,1}};
 memcpy((char*)nn_bo_map(o->vertex)+64,vertices,sizeof(vertices));float matrix[16]={0},constants[16]={0};
 for(unsigned i=0;i<4;i++)matrix[i*4+i]=1;constants[5]=constants[6]=1;
 memcpy((char*)nn_bo_map(o->uniform)+64,matrix,64);memcpy((char*)nn_bo_map(o->uniform)+128,constants,64);
 if((e=nn_bo_cache(o->vertex,0,4096,0))||(e=nn_bo_cache(o->uniform,0,4096,0)))goto done;
 const unsigned dimensions[3][2]={{8,8},{37,21},{128,65}};
 for(unsigned format=0x41;format<=0x44;format++)for(unsigned shape=0;shape<3;shape++){
  unsigned test=(format-0x41)*3+shape,width=dimensions[shape][0],height=dimensions[shape][1];bad_case=test;
  struct nn_tex_source_desc sd={.format=format,.target=1,.width=width,.height=height,.levels=1,.layers=1,.block_height_log2=-1};
  struct nn_tex_source_layout source;struct nn_bc_shadow_plan plan;struct nn_tex_native_view rt;
  stage="metadata";if((e=nn_tex_source_layout(&sd,&source))||(e=nn_bc_shadow_plan(&source,0,1,0,1,&plan))||(e=target_layout(width,height,&rt)))goto done;
  if((e=nn_bo_alloc(o->device,512+(size_t)source.payload_bytes+512,&o->canonical))||(e=nn_bo_alloc(o->device,4096+rt.required_span_bytes+4096,&o->target)))goto done;
  o->source_snapshot=malloc(nn_bo_size(o->canonical));if(!o->source_snapshot){e=ENOMEM;goto done;}
  struct nn_block_copy_range canonical={o->canonical,512,(size_t)source.payload_bytes,512};
  stage="shadow";if((e=nn_bc_shadow_create(o->device,&NN_BC_BLOCK_IMAGE,&source,&plan,&canonical,&o->bc)))goto done;
  struct nn_pipeline_draw_state state={.target={&rt,{o->target,4096,rt.required_span_bytes}},.texture_bound=2,.sampler_bound=2,.draw={.primitive=4,.count=3,.instances=1}};
  state.sampled_fdl[1]=&plan.native;state.textures[1].buffer=(struct nn_pipeline_buffer){o->bc.storage,0,plan.native.required_span};
  state.samplers[1]=(struct nn_sampler_state){.version=1,.min_filter=NVN_MIN_FILTER_NEAREST,.mag_filter=NVN_MAG_FILTER_NEAREST,
    .wrap={NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE},.compare_func=NVN_COMPARE_FUNC_ALWAYS,.anisotropy=1};
  state.sampler_layouts[1]=(struct nn_sampler_layout){width,height,1,1,2,1,0,0};
  if((e=nn_pipeline_full_raster(width,height,0,1,&state.raster)))goto done;nn_render_state_defaults(&state.render);
  state.vertex.attribute_count=3;state.vertex.stream_count=1;state.vertex.streams[0].stride=32;state.vertex.buffers[0]=(struct nn_pipeline_buffer){o->vertex,64,sizeof(vertices)};
  state.vertex.attributes[0]=(struct nn_pipeline_attribute){NVN_FORMAT_RG32F,0,0};state.vertex.attributes[1]=(struct nn_pipeline_attribute){NVN_FORMAT_RG32F,8,0};state.vertex.attributes[2]=(struct nn_pipeline_attribute){NVN_FORMAT_RGBA32F,16,0};
  for(size_t i=0;i<recipe->patch_count;i++)if(recipe->patches[i].role==NN_PIPELINE_UBO){const struct nn_pipeline_address_patch *p=recipe->patches+i;
   if(p->offset||p->bytes!=64||p->binding!=(p->stage?2u:1u)){e=ENOTSUP;goto done;}
   state.ubo[p->stage][p->binding]=(struct nn_pipeline_buffer){o->uniform,p->stage?128:64,64};}
  stage="graphics";if((e=nn_pipeline_encode(o->device,o->program,recipe,archive->platform,&state,&o->graphics,&o->retained)))goto done;
  struct nn_tex_shadow pre={.pre_bytes=o->bc.dispatch_bytes[0]};pre.bos[NN_TEX_SHADOW_PRE]=o->bc.dispatch[0];const struct nn_tex_shadow *inputs[]={&pre};size_t wrapper_bytes=0;
  stage="chain";if((e=nn_tex_shadow_chain(o->device,inputs,1,o->graphics.job,o->graphics.command_bytes,NULL,&o->wrapper,&wrapper_bytes)))goto done;
  nn_bo *refs[64],*bc_refs[33];size_t count=0,bc_count=0;
  if((e=nn_bc_shadow_resources(&o->bc,bc_refs,33,&bc_count)))goto done;
  for(size_t i=0;i<bc_count;i++)if((e=refs_add(refs,&count,bc_refs[i])))goto done;
  for(size_t i=0;i<o->graphics.resource_count;i++)if((e=refs_add(refs,&count,o->graphics.resources[i])))goto done;
  if((e=refs_add(refs,&count,o->graphics.job)))goto done;
  for(unsigned repeat=0;repeat<2;repeat++){
   unsigned char *source_bytes=nn_bo_map(o->canonical),*target_bytes=nn_bo_map(o->target);
   memset(source_bytes,0xa5,nn_bo_size(o->canonical));memset(target_bytes,0xa5,nn_bo_size(o->target));
   for(unsigned by=0;by<source.level[0].blocks_y;by++)for(unsigned bx=0;bx<source.level[0].blocks_x;bx++){
    uint8_t block[16];nn_bc_test_block(format,bx,by,repeat,block);
    for(unsigned b=0;b<source.bytes_per_block;b++){uint64_t at;if((e=nn_tex_source_offset(&source,0,0,bx,by,b,&at)))goto done;source_bytes[512+at]=block[b];}
   }
   memcpy(o->source_snapshot,source_bytes,nn_bo_size(o->canonical));
   const uint8_t clear[4]={17,29,43,59};
   for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)memcpy(target_bytes+4096+y*rt.pitch+x*4,clear,4);
   if((e=nn_bo_cache(o->canonical,0,nn_bo_size(o->canonical),0))||(e=nn_bo_cache(o->target,0,nn_bo_size(o->target),0)))goto done;
   stage="submit";uint32_t ts;unknown=1;
   if((e=nn_queue_submit(o->queue,o->wrapper,0,wrapper_bytes,refs,count,&ts)))goto done;submits++;
   stage="finish";if((e=nn_queue_finish(o->queue,10000)))goto done;unknown=0;finishes++;if(repeat)replays++;
   if((e=nn_bo_cache(o->target,0,nn_bo_size(o->target),1))||(e=nn_bo_cache(o->canonical,0,nn_bo_size(o->canonical),1)))goto done;
   stage="readback";
   for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
    uint8_t expected[4];nn_bc_test_expected(format,x,y,repeat,expected);
    for(unsigned c=0;c<4;c++){unsigned actual=target_bytes[4096+y*rt.pitch+x*4+c],delta=actual>expected[c]?actual-expected[c]:expected[c]-actual;channels++;if(delta>max_error)max_error=delta;
     if(delta>1){bad_x=x;bad_y=y;bad_channel=c;bad_actual=actual;bad_expected=expected[c];e=EILSEQ;goto done;}}
   }
   for(size_t at=0;at<nn_bo_size(o->target);at++){
    size_t relative=at>=4096?at-4096:SIZE_MAX;int active=relative<rt.required_span_bytes&&relative/rt.pitch<height&&relative%rt.pitch<width*4;
    if(!active){guards++;if(target_bytes[at]!=0xa5){e=EILSEQ;goto done;}}
   }
   /* Full source allocation, including mip/GOB padding, must remain immutable.
    * This byte snapshot does not generate expected sampled pixels; that oracle
    * comes only from the authored controls' closed form above. */
   for(size_t at=0;at<nn_bo_size(o->canonical);at++){readonly++;if(source_bytes[at]!=o->source_snapshot[at]){e=EILSEQ;goto done;}}
   for(size_t at=0;at<512;at++){guards++;if(source_bytes[at]!=0xa5){e=EILSEQ;goto done;}}
   for(size_t at=512+source.payload_bytes;at<nn_bo_size(o->canonical);at++){guards++;if(source_bytes[at]!=0xa5){e=EILSEQ;goto done;}}
   if((e=nn_bo_cache(o->vertex,0,4096,1))||(e=nn_bo_cache(o->uniform,0,4096,1)))goto done;
   for(unsigned at=0;at<4096;at++){
    unsigned char v=at>=64&&at<64+sizeof(vertices)?((const unsigned char*)vertices)[at-64]:0xa5;
    unsigned char u=at>=64&&at<128?((const unsigned char*)matrix)[at-64]:at>=128&&at<192?((const unsigned char*)constants)[at-128]:0xa5;
    readonly+=2;if(((unsigned char*)nn_bo_map(o->vertex))[at]!=v||((unsigned char*)nn_bo_map(o->uniform))[at]!=u){e=EILSEQ;goto done;}}
   if((e=save(directory,test,repeat,target_bytes+4096,width,height,rt.pitch)))goto done;files++;completed++;
  }
  if((e=nn_bo_destroy(o->wrapper)))goto done;o->wrapper=NULL;
  if((e=nn_bo_destroy(o->graphics.job)))goto done;memset(&o->graphics,0,sizeof(o->graphics));
  if((e=nn_bc_shadow_destroy(&o->bc)))goto done;
  if((e=nn_bo_destroy(o->target)))goto done;o->target=NULL;
  if((e=nn_bo_destroy(o->canonical)))goto done;o->canonical=NULL;
  free(o->source_snapshot);o->source_snapshot=NULL;
 }
 bad_case=~0u;stage="complete";
done:
 if(!unknown)cleanup=release(o);
 int retained_owner=unknown||cleanup,passed=!e&&!cleanup&&!unknown&&completed==24&&submits==24&&finishes==24&&files==24&&replays==12;
 if(retained_owner){o->next=quarantine;quarantine=o;}else free(o);
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"raw_bc_sampled\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"submissions\":%u,\"finishes\":%u,\"completed_cases\":%u,\"readbacks\":%u,\"same_job_replays\":%u,\"channel_checks\":%llu,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"max_error\":%u,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,\"bad_actual\":%u,\"bad_expected\":%u,\"cpu_decompression\":false,\"cpu_image_conversion\":false,\"public_nvn\":false,\"game_frame\":false,\"authored_bc_controls\":true,\"passed\":%s}\n",(unsigned)getuid(),stage,e,cleanup,retained_owner?"true":"false",submits,finishes,completed,files,replays,(unsigned long long)channels,(unsigned long long)guards,(unsigned long long)readonly,max_error,bad_case,bad_x,bad_y,bad_channel,bad_actual,bad_expected,passed?"true":"false");
 return passed&&n>0&&(size_t)n<capacity?0:e?e:cleanup?cleanup:EIO;
}
#ifdef NN_BC_SAMPLED_STANDALONE
int main(int argc,char **argv){if(argc!=2)return 2;char json[8192];int e=nn_bc_sampled_probe(argv[1],json,sizeof(json));fputs(json,stdout);return e?1:0;}
#endif
