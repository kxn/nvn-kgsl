/* SPDX-License-Identifier: MIT. Authored depth GPU qualifier, not NVN/game. */
#define NN_PIPELINE_PROBE_HELPERS_ONLY 1
#define PIPELINE_RAW_NAME "%s/depth-color-%03u.raw"
#include "pipeline_probe.c"
#include "nvn_native/depth_transfer.h"
#include "nvn_native/depth_native.h"
#include "nvn_native/pipeline_views.h"
struct depth_owner {
 struct owner common;struct nn_depth_attachment attachment;struct nn_depth_readback readback;
 nn_bo *clear_job,*read_job;size_t clear_bytes,read_bytes;struct nn_pipeline_views *views;
};
static int depth_release(struct depth_owner*d)
{
 struct owner*o=&d->common;int e;
 if(o->queue){if((e=nn_queue_destroy(o->queue)))return e;o->queue=NULL;}
 if(d->clear_job){if((e=nn_bo_destroy(d->clear_job)))return e;d->clear_job=NULL;}
 if(d->read_job){if((e=nn_bo_destroy(d->read_job)))return e;d->read_job=NULL;}
 if((e=nn_depth_readback_destroy(&d->readback)))return e;
 if((e=nn_depth_attachment_destroy(&d->attachment)))return e;
 e=release(o);if(e)return e;nn_pipeline_views_destroy(d->views);d->views=NULL;return 0;
}
static int depth_save(const char*directory,unsigned number,nn_bo*bo)
{
 char path[1400];int n=snprintf(path,sizeof(path),"%s/depth-z-%03u.raw",directory,number);if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;size_t off=0,bytes=nn_bo_size(bo);const unsigned char*map=nn_bo_map(bo);int e=0;if(!map)e=EFAULT;
 while(!e&&off<bytes){ssize_t w=write(fd,map+off,bytes-off);if(w<0&&errno==EINTR)continue;if(w<=0)e=w<0?errno:EIO;else off+=(size_t)w;}
 if(!e&&fsync(fd))e=errno;
 if(close(fd)&&!e)e=errno;
 return e;
}
static int depth_submit(struct depth_owner*d,nn_bo*job,size_t bytes,nn_bo*const*refs,size_t count,unsigned*submits,unsigned*finishes,unsigned*busy,int*unknown)
{
 uint32_t timestamp;*unknown=1;int e=nn_queue_submit(d->common.queue,job,0,bytes,refs,count,&timestamp);if(e)return e;(*submits)++;
 for(size_t i=0;i<count;i++){if(nn_bo_idle(refs[i])!=EBUSY)return EPROTO;(*busy)++;}
 if(nn_depth_attachment_destroy(&d->attachment)!=EBUSY)return EPROTO;
 (*busy)++;
 if(job==d->read_job){if(nn_depth_readback_destroy(&d->readback)!=EBUSY)return EPROTO;(*busy)++;}
 if(job==d->common.work.job){if(nn_pipeline_program_destroy(d->common.program)!=EBUSY)return EPROTO;(*busy)++;}
 e=nn_queue_finish(d->common.queue,3000);if(e)return e;*unknown=0;(*finishes)++;return 0;
}
static float depth_clear(unsigned mode){return mode==3||mode==4?0.f:1.f;}
static float depth_value(unsigned mode,unsigned repeat){
 if(mode==2||mode==6)return repeat?.25f:1.f;
 if(mode==3)return repeat?.75f:0.f;
 if(mode==4)return repeat?0.f:.75f;
 return repeat?1.f:.25f;
}
int nn_pipeline_probe(const char*directory,char*json,size_t capacity)
{
 struct depth_owner*d=calloc(1,sizeof(*d));if(!d)return ENOMEM;struct owner*o=&d->common;
 unsigned submissions=0,finishes=0,jobs=0,files=0,completed=0,replays=0,busy=0,programs=0;
 uint64_t guard_checks=0,readonly_checks=0;uint32_t chip=0;unsigned bad_case=UINT32_MAX;size_t bad_byte=SIZE_MAX;
 int e=0,cleanup=0,unknown=0;const char*stage="open";char cases[22000]={0};size_t used=0;
 if((e=nn_device_open(&o->device)))goto done;
 chip=nn_device_chip(o->device);
 if(chip!=0x06050002||nn_device_gmem_bytes(o->device)!=nn_pipeline_device.gmem_bytes){e=ENOTSUP;goto done;}
 if((e=nn_queue_create(o->device,&o->queue))||(e=nn_pipeline_views_create(chip,&d->views)))goto done;
 for(unsigned i=0;i<3;i++)if((e=nn_bo_alloc(o->device,4096,o->input+i)))goto done;
 /* Keep all old case/file numbers. Append the six true no-COLOR cases. */
 for(unsigned test=0;test<72;test++){
  unsigned group=test<66?test/11:test-66,format=0x32+group/2,origin=group%2,mode=test<66?test%11:11;
  unsigned zero_color=mode==11,only=mode>=10,width=mode&1?73:64,height=mode&1?41:64;
  const struct nn_pipeline_native_artifact*artifact=only?&depth_only_artifact:&depth_color_artifact;
  const struct nn_pipeline_depth_export*depth=only?&nn_pipeline_depth_only:&nn_pipeline_depth_color;
  if(!o->program||mode==10){
   if(o->program){if((e=nn_pipeline_program_destroy(o->program)))goto done;o->program=NULL;}
   stage="program";if((e=nn_pipeline_program_create_native_depth(o->device,artifact,depth,&o->program,&o->retained_program)))goto done;programs++;
  }
  struct nn_pipeline_stage_view shaders[2];for(unsigned i=0;i<2;i++)if((e=nn_pipeline_program_stage(o->program,i,shaders+i)))goto done;
  const struct nn_tex_native_view*view=NULL;stage="targets";
  if(!zero_color){if((e=nn_pipeline_views_get(d->views,width,height,0,&view))||(e=nn_bo_alloc(o->device,8192+view->required_span_bytes,&o->target)))goto done;}
  struct nn_depth_fdl_desc desc={.schema=1,.chip=chip,.format=format,.target=1,.width=width,.height=height,.levels=1,.layers=1,.roles=NN_DEPTH_ATTACHMENT};
  if((e=nn_depth_attachment_create(o->device,&desc,&d->attachment))||(e=nn_depth_readback_create(o->device,&d->attachment,&d->readback)))goto done;
  struct nn_depth_a2d_region region={0,0,width,height};stage="transfer-encode";
  if((e=nn_depth_transfer_encode(o->device,&d->attachment,NULL,&nn_pipeline_device,0,&region,depth_clear(mode),&d->clear_job,&d->clear_bytes))||(e=nn_depth_transfer_encode(o->device,&d->attachment,&d->readback,&nn_pipeline_device,1,&region,0,&d->read_job,&d->read_bytes)))goto done;
  jobs+=2;
  struct nn_pipeline_draw_state state={.texture_types=artifact->textures};
  if(!zero_color)state.target=(struct nn_pipeline_image){view,{o->target,4096,view->required_span_bytes}};
  nn_render_state_defaults(&state.render);state.render.depth.depth_test=mode!=8;state.render.depth.depth_write=mode!=9;state.render.depth.depth_func=mode<8?mode+1:8;
  if((e=nn_pipeline_full_raster(width,height,origin,1,&state.raster)))goto done;
  state.raster.scissor_x=5;state.raster.scissor_y=3;state.raster.scissor_width=width-12;state.raster.scissor_height=height/2;
  state.vertex.attribute_count=state.vertex.stream_count=32;state.vertex.streams[3]=(struct nn_pipeline_stream){28,0};state.vertex.buffers[3]=(struct nn_pipeline_buffer){o->input[1],256,84};state.vertex.attributes[0]=(struct nn_pipeline_attribute){NVN_FORMAT_RGB32F,0,3};if(!only)state.vertex.attributes[1]=(struct nn_pipeline_attribute){NVN_FORMAT_RGBA32F,12,3};
  state.ubo[0][1]=(struct nn_pipeline_buffer){o->input[0],256,64};state.ubo[1][2]=(struct nn_pipeline_buffer){o->input[2],256,64};state.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
  struct nn_depth_controls_desc controls={.schema=1,.chip=chip,.test_enable=mode!=8,.write_enable=mode!=8&&mode!=9,.compare=mode<8?mode+1:8,.shader_writes_depth=1,.depth_regid=depth->depth_regid};
  stage="draw-encode";if((e=nn_pipeline_encode_depth(o->device,o->program,depth,&nn_pipeline_device,&state,&d->attachment,&controls,&o->work,&o->retained_job)))goto done;jobs++;
  uint64_t ro=nn_bo_size(o->work.job)-4+nn_bo_size(d->clear_job)-4+nn_bo_size(d->read_job)-4;for(unsigned i=0;i<3;i++)ro+=nn_bo_size(o->input[i]);for(unsigned i=0;i<2;i++)ro+=nn_bo_size(shaders[i].code);
  int n=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"format\":%u,\"origin\":%u,\"mode\":%u,\"width\":%u,\"height\":%u,\"color_pitch\":%u,\"color_bytes\":%zu,\"depth_pitch\":%u,\"depth_bytes\":%zu,\"readonly_bytes\":%llu,\"zero_color\":%s,\"dummy_color\":%s}",used?",":"",test,format,origin,mode,width,height,view?view->pitch:0,o->target?nn_bo_size(o->target):0,d->readback.view.linear.pitch,nn_bo_size(d->readback.storage),(unsigned long long)ro,zero_color?"true":"false",mode==10?"true":"false");
  if(n<0||(size_t)n>=sizeof(cases)-used){e=EOVERFLOW;goto done;}used+=(size_t)n;
  for(unsigned repeat=0;repeat<2;repeat++){
   if(o->target){memset(nn_bo_map(o->target),0xa5,nn_bo_size(o->target));for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){static const unsigned char clear[4]={17,29,43,59};memcpy((unsigned char*)nn_bo_map(o->target)+4096+y*view->pitch+x*4,clear,4);}}
   memset(nn_bo_map(d->readback.storage),0xa5,nn_bo_size(d->readback.storage));
   for(unsigned i=0;i<3;i++)memset(nn_bo_map(o->input[i]),0xa5,nn_bo_size(o->input[i]));
   float identity[16]={0};identity[0]=identity[5]=identity[10]=identity[15]=1;memcpy((unsigned char*)nn_bo_map(o->input[0])+256,identity,64);
   const float vertices[3][7]={{-1,-1,0,1,0,0,1},{3,-1,0,0,1,0,1},{-1,3,0,0,0,1,1}};memcpy((unsigned char*)nn_bo_map(o->input[1])+256,vertices,sizeof(vertices));memset((unsigned char*)nn_bo_map(o->input[2])+256,0,64);float z=depth_value(mode,repeat);memcpy((unsigned char*)nn_bo_map(o->input[2])+256,&z,4);
   snapshots_clear(o);for(unsigned i=0;i<3;i++){if((e=snapshot(o,o->input[i],nn_bo_size(o->input[i]))))goto done;if((e=nn_bo_cache(o->input[i],0,nn_bo_size(o->input[i]),0)))goto done;}
   for(unsigned i=0;i<2;i++)if((e=snapshot(o,shaders[i].code,nn_bo_size(shaders[i].code))))goto done;
   nn_bo*joblist[3]={d->clear_job,o->work.job,d->read_job};for(unsigned i=0;i<3;i++)if((e=snapshot(o,joblist[i],nn_bo_size(joblist[i]))))goto done;
   if((o->target&&(e=nn_bo_cache(o->target,0,nn_bo_size(o->target),0)))||(e=nn_bo_cache(d->readback.storage,0,nn_bo_size(d->readback.storage),0)))goto done;
   nn_bo*refs[3];size_t refs_n;stage="clear-submit";if((e=nn_depth_transfer_resources(&d->attachment,NULL,0,d->clear_job,refs,3,&refs_n))||(e=depth_submit(d,d->clear_job,d->clear_bytes,refs,refs_n,&submissions,&finishes,&busy,&unknown)))goto done;
   stage="draw-submit";if((e=depth_submit(d,o->work.job,o->work.command_bytes,o->work.resources,o->work.resource_count,&submissions,&finishes,&busy,&unknown)))goto done;
   stage="read-submit";if((e=nn_depth_transfer_resources(&d->attachment,&d->readback,1,d->read_job,refs,3,&refs_n))||(e=depth_submit(d,d->read_job,d->read_bytes,refs,refs_n,&submissions,&finishes,&busy,&unknown)))goto done;
   stage="guards";if((o->target&&(e=nn_bo_cache(o->target,0,nn_bo_size(o->target),1)))||(e=nn_bo_cache(d->readback.storage,0,nn_bo_size(d->readback.storage),1)))goto done;
   nn_bo*outputs[2]={o->target,d->readback.storage};for(unsigned which=zero_color?1:0;which<2;which++){
    const unsigned char*map=nn_bo_map(outputs[which]);size_t prefix=which?0:4096,pitch=which?d->readback.view.linear.pitch:view->pitch,cpp=which?(format==0x32?2:4):4;
    for(size_t off=0;off<nn_bo_size(outputs[which]);off++){if(off>=prefix){size_t rel=off-prefix;if(rel/pitch<height&&rel%pitch<width*cpp)continue;}if(map[off]!=0xa5){bad_case=test*2+repeat;bad_byte=off;e=EIO;goto done;}guard_checks++;}
   }
   stage="readonly";for(size_t i=0;i<o->watch_count;i++){
    const struct watch*w=o->watches+i;if((e=nn_bo_cache(w->bo,0,w->count,1)))goto done;const unsigned char*map=nn_bo_map(w->bo);
    for(size_t off=0;off<w->count;off++){size_t seq=w->bo==o->work.job?45056:w->bo==d->clear_job||w->bo==d->read_job?40960:SIZE_MAX;if(off>=seq&&off-seq<4)continue;if(map[off]!=w->bytes[off]){bad_case=test*2+repeat;bad_byte=off;e=EIO;goto done;}readonly_checks++;}
   }
   stage="save";if(o->target){if((e=save(directory,test*2+repeat,o->target)))goto done;files++;}if((e=depth_save(directory,test*2+repeat,d->readback.storage)))goto done;files++;completed++;if(repeat)replays++;
  }
  snapshots_clear(o);stage="retire";if((e=nn_bo_destroy(o->work.job)))goto done;memset(&o->work,0,sizeof(o->work));if((e=nn_bo_destroy(d->clear_job)))goto done;d->clear_job=NULL;if((e=nn_bo_destroy(d->read_job)))goto done;d->read_job=NULL;if((e=nn_depth_readback_destroy(&d->readback))||(e=nn_depth_attachment_destroy(&d->attachment))||(o->target&&(e=nn_bo_destroy(o->target))))goto done;o->target=NULL;
  if(mode==10){if((e=nn_pipeline_program_destroy(o->program)))goto done;o->program=NULL;}
 }
 stage="complete";
done:
 if(unknown)cleanup=EBUSY;else cleanup=depth_release(d);int retained=unknown||cleanup;if(retained){o->next=quarantine;quarantine=o;}else free(d);
 int passed=!e&&!cleanup&&completed==144;int n=snprintf(json,capacity,"{\"schema\":2,\"operation\":\"native_depth\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"jobs\":%u,\"programs\":%u,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"completed_cases\":%u,\"replays\":%u,\"inflight_owner_checks\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"pixel_checks\":0,\"pixel_oracle_on_host\":true,\"cpu_image_conversion\":false,\"nvn_api\":false,\"game_modules_executed\":false,\"same_job_replay\":true,\"stencil\":false,\"bad_case\":%u,\"bad_byte\":%zu,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),chip,stage,e,cleanup,retained?"true":"false",jobs,programs,submissions,finishes,files,completed,replays,busy,(unsigned long long)guard_checks,(unsigned long long)readonly_checks,bad_case,bad_byte,cases,passed?"true":"false");
 return passed&&n>0&&(size_t)n<capacity?0:e?e:cleanup?cleanup:EIO;
}
