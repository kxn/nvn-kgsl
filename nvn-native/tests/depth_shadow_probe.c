/* SPDX-License-Identifier: MIT. Raw native GPU storage-chain qualification only. */
#define NN_PIPELINE_PROBE_HELPERS_ONLY 1
#define PIPELINE_RAW_NAME "%s/depth-shadow-color-%03u.raw"
#include "pipeline_probe.c"
#include "depth_shadow_cases.h"
#include "nvn_native/depth_shadow.h"
#include "nvn_native/depth_native.h"
#include "nvn_native/pipeline_views.h"
struct shadow_owner {struct owner common;struct nn_depth_shadow shadow;nn_bo *canonical,*clear_job;size_t clear_bytes;struct nn_pipeline_views *views;};
static int shadow_release(struct shadow_owner*d)
{
 struct owner*o=&d->common;int e;
 if(o->queue){if((e=nn_queue_destroy(o->queue)))return e;o->queue=NULL;}
 if(d->clear_job){if((e=nn_bo_destroy(d->clear_job)))return e;d->clear_job=NULL;}
 if((e=nn_depth_shadow_destroy(&d->shadow)))return e;
 if(d->canonical){if((e=nn_bo_destroy(d->canonical)))return e;d->canonical=NULL;}
 e=release(o);if(e)return e;nn_pipeline_views_destroy(d->views);d->views=NULL;return 0;
}
static size_t shadow_sequence(const struct shadow_owner*d,const nn_bo*b)
{
 if(b==d->shadow.copy[0].job||b==d->shadow.copy[1].job)return 6144;
 if(b==d->clear_job||b==d->shadow.a2d[0].job||b==d->shadow.a2d[1].job)return 40960;
 if(b==d->common.work.job)return 45056;
 return SIZE_MAX;
}
static int shadow_save(const char*directory,unsigned number,nn_bo*bo)
{
 char path[1400];int n=snprintf(path,sizeof(path),"%s/depth-shadow-gob-%03u.raw",directory,number);if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;const unsigned char*map=nn_bo_map(bo);size_t size=nn_bo_size(bo),off=0;int e=map?0:EFAULT;
 while(!e&&off<size){ssize_t w=write(fd,map+off,size-off);if(w<0&&errno==EINTR)continue;if(w<=0)e=w<0?errno:EIO;else off+=(size_t)w;}if(!e&&fsync(fd))e=errno;if(close(fd)&&!e)e=errno;return e;
}
static int shadow_refs(struct shadow_owner*d,nn_bo**out,size_t*count)
{
 size_t n=0;int e=nn_depth_shadow_resources(&d->shadow,out,32,&n);if(e)return e;
 for(size_t i=0;i<d->common.work.resource_count+1;i++){
  nn_bo*b=i==d->common.work.resource_count?d->clear_job:d->common.work.resources[i];if(!b)return EINVAL;size_t j=0;for(;j<n;j++)if(out[j]==b)break;if(j==n){if(n==32)return ENOSPC;out[n++]=b;}}
 *count=n;return 0;
}
int nn_pipeline_probe(const char*directory,char*json,size_t capacity)
{
 if(!directory||!json||capacity<32768)return EINVAL;
 struct shadow_owner*d=calloc(1,sizeof(*d));if(!d)return ENOMEM;struct owner*o=&d->common;
 unsigned submissions=0,finishes=0,jobs=0,files=0,completed=0,replays=0,busy=0,programs=0,zero_color=0;
 uint64_t guards=0,readonly=0;uint32_t chip=0;unsigned bad_case=UINT32_MAX;size_t bad_byte=SIZE_MAX;char cases[22000]={0};size_t used=0;
 int e=0,cleanup=0,unknown=0;const char*stage="open";
 if((e=nn_device_open(&o->device)))goto done;
 chip=nn_device_chip(o->device);
 if(chip!=0x06050002||nn_device_gmem_bytes(o->device)!=nn_pipeline_device.gmem_bytes){e=ENOTSUP;goto done;}
 if((e=nn_queue_create(o->device,&o->queue))||(e=nn_pipeline_views_create(chip,&d->views)))goto done;
 for(unsigned i=0;i<3;i++)if((e=nn_bo_alloc(o->device,4096,o->input+i)))goto done;
 for(unsigned test=0;test<NN_DEPTH_SHADOW_CASES;test++){
  struct nn_depth_shadow_case c;stage="case";if((e=nn_depth_shadow_case(test,&c)))goto done;
  unsigned width=c.source.level[c.level].width,height=c.source.level[c.level].height;const struct nn_tex_native_view*view=NULL;
  const struct nn_pipeline_native_artifact*artifact=c.role?&depth_only_artifact:&depth_color_artifact;
  const struct nn_pipeline_depth_export*depth=c.role?&nn_pipeline_depth_only:&nn_pipeline_depth_color;
  stage="program";if((e=nn_pipeline_program_create_native_depth(o->device,artifact,depth,&o->program,&o->retained_program)))goto done;programs++;
  struct nn_pipeline_stage_view shaders[2];for(unsigned i=0;i<2;i++)if((e=nn_pipeline_program_stage(o->program,i,shaders+i)))goto done;
  stage="canonical-owner";size_t canonical_bytes=(8192+(size_t)c.source.payload_bytes+4095)&~(size_t)4095;
  if((e=nn_bo_alloc(o->device,canonical_bytes,&d->canonical)))goto done;
  struct nn_depth_shadow_plan plan;if((e=nn_depth_shadow_plan(&c.source,c.level,c.layer,&plan)))goto done;
  struct nn_block_copy_range canonical={d->canonical,4096,(size_t)c.source.payload_bytes,4096};
  if((e=nn_depth_shadow_create(o->device,&NN_DEPTH_SHADOW_BLOCK_IMAGE,&nn_pipeline_device,&c.source,&plan,&canonical,32,&d->shadow)))goto done;
  struct nn_depth_shadow_step load[2],store[2];if((e=nn_depth_shadow_sequence(&d->shadow,load,store)))goto done;
  if(c.role<2){if((e=nn_pipeline_views_get(d->views,width,height,0,&view))||(e=nn_bo_alloc(o->device,8192+view->required_span_bytes,&o->target)))goto done;}else zero_color++;
  struct nn_pipeline_draw_state state={.texture_types=artifact->textures};if(view)state.target=(struct nn_pipeline_image){view,{o->target,4096,view->required_span_bytes}};
  nn_render_state_defaults(&state.render);state.render.depth.depth_test=state.render.depth.depth_write=1;state.render.depth.depth_func=c.compare;
  if((e=nn_pipeline_full_raster(width,height,c.origin,1,&state.raster)))goto done;
  state.raster.scissor_x=3;state.raster.scissor_y=2;state.raster.scissor_width=width-7;state.raster.scissor_height=height-5;
  state.vertex.attribute_count=state.vertex.stream_count=32;state.vertex.streams[3]=(struct nn_pipeline_stream){28,0};state.vertex.buffers[3]=(struct nn_pipeline_buffer){o->input[1],256,84};
  state.vertex.attributes[0]=(struct nn_pipeline_attribute){NVN_FORMAT_RGB32F,0,3};if(!c.role)state.vertex.attributes[1]=(struct nn_pipeline_attribute){NVN_FORMAT_RGBA32F,12,3};
  state.ubo[0][1]=(struct nn_pipeline_buffer){o->input[0],256,64};state.ubo[1][2]=(struct nn_pipeline_buffer){o->input[2],256,64};state.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
  struct nn_depth_controls_desc controls={.schema=1,.chip=chip,.test_enable=1,.write_enable=1,.compare=c.compare,.shader_writes_depth=1,.depth_regid=depth->depth_regid};
  stage="encode";if((e=nn_pipeline_encode_depth(o->device,o->program,depth,&nn_pipeline_device,&state,&d->shadow.attachment,&controls,&o->work,&o->retained_job)))goto done;
  struct nn_depth_a2d_region region={0,0,width,height};if((e=nn_depth_transfer_encode(o->device,&d->shadow.attachment,NULL,&nn_pipeline_device,NN_DEPTH_A2D_CLEAR,&region,.9375f,&d->clear_job,&d->clear_bytes)))goto done;jobs+=6;
  nn_bo*refs[32];size_t refs_n=0;if((e=shadow_refs(d,refs,&refs_n)))goto done;
  /* All six jobs use precisely the SAME complete owner+Draw ledger. Even
   * CLEAR/untile pending owns canonical+native+staging+every future job. */
  struct nn_depth_shadow_step steps[6]={{d->clear_job,d->clear_bytes},load[0],load[1],{o->work.job,o->work.command_bytes},store[0],store[1]};
  uint64_t ro=0;for(unsigned i=3;i<10;i++)ro+=nn_bo_size(refs[i]);ro+=nn_bo_size(d->clear_job)+nn_bo_size(o->work.job);
  for(unsigned i=0;i<3;i++)ro+=nn_bo_size(o->input[i]);
  for(unsigned i=0;i<2;i++)ro+=nn_bo_size(shaders[i].code);
  ro-=24;
  int n=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"format\":%u,\"profile\":%u,\"selected\":%u,\"role\":%u,\"origin\":%u,\"level\":%u,\"layer\":%u,\"compare\":%u,\"width\":%u,\"height\":%u,\"canonical_bytes\":%zu,\"color_pitch\":%u,\"color_bytes\":%zu,\"refs\":%zu,\"readonly_bytes\":%llu}",used?",":"",test,c.format,c.profile,c.selected,c.role,c.origin,c.level,c.layer,c.compare,width,height,nn_bo_size(d->canonical),view?view->pitch:0,o->target?nn_bo_size(o->target):0,refs_n,(unsigned long long)ro);
  if(n<0||(size_t)n>=sizeof(cases)-used){e=EOVERFLOW;goto done;}used+=(size_t)n;
  for(unsigned repeat=0;repeat<2;repeat++){
   bad_case=test*2+repeat;stage="authored-input";
   /* Only qualification canonical input and finished readback boundaries are
    * CPU-visible. The private native/staging payloads are NEVER mapped/cached. */
   if((e=nn_depth_fixture_input(&c,test,repeat,nn_bo_map(d->canonical),nn_bo_size(d->canonical)))||(e=nn_bo_cache(d->canonical,0,nn_bo_size(d->canonical),0)))goto done;
   if(view){memset(nn_bo_map(o->target),0xa5,nn_bo_size(o->target));for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){static const unsigned char clear[4]={17,29,43,59};memcpy((unsigned char*)nn_bo_map(o->target)+4096+y*view->pitch+x*4,clear,4);}if((e=nn_bo_cache(o->target,0,nn_bo_size(o->target),0)))goto done;}
   for(unsigned i=0;i<3;i++)memset(nn_bo_map(o->input[i]),0xa5,nn_bo_size(o->input[i]));
   float identity[16]={0};identity[0]=identity[5]=identity[10]=identity[15]=1;memcpy((unsigned char*)nn_bo_map(o->input[0])+256,identity,64);
   const float vertices[3][7]={{-1,-1,0,1,0,0,1},{3,-1,0,0,1,0,1},{-1,3,0,0,0,1,1}};memcpy((unsigned char*)nn_bo_map(o->input[1])+256,vertices,sizeof(vertices));memset((unsigned char*)nn_bo_map(o->input[2])+256,0,64);float z=repeat?.25f:.5f;memcpy((unsigned char*)nn_bo_map(o->input[2])+256,&z,4);
   snapshots_clear(o);for(unsigned i=0;i<3;i++){if((e=snapshot(o,o->input[i],nn_bo_size(o->input[i])))||(e=nn_bo_cache(o->input[i],0,nn_bo_size(o->input[i]),0)))goto done;}
   for(unsigned i=0;i<2;i++)if((e=snapshot(o,shaders[i].code,nn_bo_size(shaders[i].code))))goto done;
   for(unsigned i=3;i<10;i++)if((e=snapshot(o,refs[i],nn_bo_size(refs[i]))))goto done;
   if((e=snapshot(o,d->clear_job,nn_bo_size(d->clear_job)))||(e=snapshot(o,o->work.job,nn_bo_size(o->work.job))))goto done;
   for(unsigned step=0;step<6;step++){
    static const char*names[6]={"clear","untile","upload","draw","readback","retile"};stage=names[step];uint32_t timestamp;unknown=1;
    if((e=nn_queue_submit(o->queue,steps[step].job,0,steps[step].bytes,refs,refs_n,&timestamp)))goto done;
    submissions++;
    for(size_t i=0;i<refs_n;i++){if(nn_bo_idle(refs[i])!=EBUSY){e=EPROTO;goto done;}busy++;}
    struct nn_depth_shadow before=d->shadow;if(nn_depth_shadow_destroy(&d->shadow)!=EBUSY||memcmp(&before,&d->shadow,sizeof(before))){e=EPROTO;goto done;}busy++;
    if(nn_bo_destroy(d->canonical)!=EBUSY||nn_pipeline_program_destroy(o->program)!=EBUSY){e=EPROTO;goto done;}busy+=2;
   }
   stage="finish";if((e=nn_queue_finish(o->queue,3000)))goto done;unknown=0;finishes++;
   stage="canonical-guards";if((e=nn_bo_cache(d->canonical,0,nn_bo_size(d->canonical),1)))goto done;const unsigned char*map=nn_bo_map(d->canonical);
   for(size_t at=0;at<nn_bo_size(d->canonical);at++)if(at<4096||at>=4096+c.source.payload_bytes){if(map[at]!=nn_depth_guard_byte(at,test,repeat)){bad_byte=at;e=EILSEQ;goto done;}guards++;}
   if(view){if((e=nn_bo_cache(o->target,0,nn_bo_size(o->target),1)))goto done;map=nn_bo_map(o->target);for(size_t at=0;at<nn_bo_size(o->target);at++){if(at>=4096){size_t rel=at-4096;if(rel/view->pitch<height&&rel%view->pitch<width*4)continue;}if(map[at]!=0xa5){bad_byte=at;e=EILSEQ;goto done;}guards++;}}
   stage="readonly";for(size_t i=0;i<o->watch_count;i++){const struct watch*w=o->watches+i;if((e=nn_bo_cache(w->bo,0,w->count,1)))goto done;map=nn_bo_map(w->bo);size_t seq=shadow_sequence(d,w->bo);
    for(size_t at=0;at<w->count;at++){if(at>=seq&&at-seq<4)continue;if(map[at]!=w->bytes[at]){bad_byte=at;e=EILSEQ;goto done;}readonly++;}}
   stage="save";if((e=shadow_save(directory,test*2+repeat,d->canonical)))goto done;files++;if(view){if((e=save(directory,test*2+repeat,o->target)))goto done;files++;}completed++;if(repeat)replays++;
  }
  snapshots_clear(o);stage="retire";if((e=nn_bo_destroy(o->work.job)))goto done;memset(&o->work,0,sizeof(o->work));if((e=nn_bo_destroy(d->clear_job)))goto done;d->clear_job=NULL;
  if((e=nn_depth_shadow_destroy(&d->shadow))||(e=nn_bo_destroy(d->canonical)))goto done;
  d->canonical=NULL;
  if(o->target){if((e=nn_bo_destroy(o->target)))goto done;o->target=NULL;}if((e=nn_pipeline_program_destroy(o->program)))goto done;o->program=NULL;
 }
 stage="complete";bad_case=UINT32_MAX;
done:
 if(unknown)cleanup=EBUSY;else cleanup=shadow_release(d);int retained=unknown||cleanup;if(retained){o->next=quarantine;quarantine=o;}else free(d);
 int passed=!e&&!cleanup&&completed==108;int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"native_canonical_depth\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"jobs\":%u,\"programs\":%u,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"completed_cases\":%u,\"replays\":%u,\"inflight_owner_checks\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"true_zero_color_cases\":%u,\"pixel_checks\":0,\"pixel_oracle_on_host\":true,\"canonical_fixture_input_only\":true,\"private_depth_cpu_access\":false,\"cpu_image_conversion\":false,\"nvn_api\":false,\"game_modules_executed\":false,\"same_job_replay\":true,\"stencil\":false,\"bad_case\":%u,\"bad_byte\":%zu,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),chip,stage,e,cleanup,retained?"true":"false",jobs,programs,submissions,finishes,files,completed,replays,busy,(unsigned long long)guards,(unsigned long long)readonly,zero_color,bad_case,bad_byte,cases,passed?"true":"false");
 return passed&&n>0&&(size_t)n<capacity?0:e?e:cleanup?cleanup:EIO;
}
