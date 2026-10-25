/* SPDX-License-Identifier: MIT. Authored native storage family, no NVN/game. */
#define NN_PIPELINE_PROBE_HELPERS_ONLY 1
#define PIPELINE_RAW_NAME "%s/storage-%03u.raw"
#include "pipeline_probe.c"
#include "graphics_storage_cases.h"
int nn_pipeline_probe(const char*directory,char*json,size_t capacity)
{
 struct owner*o=calloc(1,sizeof(*o));if(!o)return ENOMEM;
 unsigned submits=0,finishes=0,files=0,jobs=0,programs=0,replays=0,busy=0,unused=0;uint64_t guards=0,readonly=0;int e=0,clean=0,unknown=0;uint32_t chip=0,timestamp=0;const char*stage="open";unsigned bad_case=UINT32_MAX;size_t bad_byte=SIZE_MAX;size_t target_bytes=0,job_bytes=0;unsigned current=UINT32_MAX;
 if((e=nn_device_open(&o->device)))goto done;
 chip=nn_device_chip(o->device);if(chip!=0x06050002||nn_device_gmem_bytes(o->device)!=nn_pipeline_device.gmem_bytes){e=ENOTSUP;goto done;}if((e=nn_queue_create(o->device,&o->queue)))goto done;
 for(unsigned i=0;i<INPUTS;i++)if((e=nn_bo_alloc(o->device,i<2?10240:4096,o->input+i)))goto done;
 size_t maximum=0;for(unsigned i=0;i<2;i++)if(nn_storage_gpu_targets[i].required_span_bytes>maximum)maximum=nn_storage_gpu_targets[i].required_span_bytes;
 if((e=nn_bo_alloc(o->device,8192+maximum,&o->target)))goto done;
 target_bytes=nn_bo_size(o->target);
 for(unsigned test=0;test<NN_STORAGE_GPU_JOBS;test++){
  const struct nn_storage_gpu_case*c=nn_storage_gpu_cases+test;const struct nn_tex_native_view*v=nn_storage_gpu_targets+c->shape;
  if(c->id!=test||c->program>=4||c->shape>=2||c->bytes<128||c->bytes>2048||c->offset[0]>12||c->offset[1]>12){e=EPROTO;goto done;}
  if(current!=c->program){if(o->program){if((e=nn_pipeline_program_destroy(o->program)))goto done;o->program=NULL;}stage="program";if((e=nn_pipeline_program_create_native_storage(o->device,nn_storage_gpu_artifacts[c->program],NULL,nn_storage_gpu_types[c->program],&o->program,&o->retained_program)))goto done;current=c->program;programs++;}
  struct nn_pipeline_stage_view shaders[2];for(unsigned i=0;i<2;i++)if((e=nn_pipeline_program_stage(o->program,i,shaders+i)))goto done;
  const struct nn_pipeline_native_artifact*a=nn_storage_gpu_artifacts[c->program];struct nn_pipeline_draw_state s={.texture_types=a->textures,.storage_types=nn_storage_gpu_types[c->program],.target={v,{o->target,4096,v->required_span_bytes}}};
  nn_render_state_defaults(&s.render);if((e=nn_pipeline_full_raster(v->width,v->height,0,1,&s.raster)))goto done;
  s.vertex.attribute_count=s.vertex.stream_count=1;s.vertex.streams[0]=(struct nn_pipeline_stream){8,0};s.vertex.buffers[0]=(struct nn_pipeline_buffer){o->input[2],256,24};s.vertex.attributes[0]=(struct nn_pipeline_attribute){0x16,0,0};s.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};unsigned binding=c->program==3?15:1;
  for(unsigned st=0;st<2;st++)s.storage[st][binding]=(struct nn_pipeline_buffer){o->input[st],4096+c->offset[st],c->bytes};
  stage="encode";if((e=nn_pipeline_encode(o->device,o->program,a->recipe,&nn_pipeline_device,&s,&o->work,&o->retained_job)))goto done;jobs++;job_bytes=nn_bo_size(o->work.job);
  if(o->work.resource_count!=7||job_bytes<47168){e=EPROTO;goto done;}for(size_t i=0;i<o->work.resource_count;i++)if(o->work.resources[i]==o->input[3]||o->work.resources[i]==o->input[4]){e=EPROTO;goto done;}
  for(unsigned repeat=0;repeat<2;repeat++){
   memset(nn_bo_map(o->target),0xa5,nn_bo_size(o->target));if((e=nn_bo_cache(o->target,0,nn_bo_size(o->target),0)))goto done;
   for(unsigned i=0;i<INPUTS;i++)memset(nn_bo_map(o->input[i]),0xa5,nn_bo_size(o->input[i]));
   for(unsigned st=0;st<2;st++)for(unsigned word=0;word<c->bytes/4;word++){uint32_t value=nn_storage_gpu_source(st,word,repeat);memcpy((char*)nn_bo_map(o->input[st])+4096+c->offset[st]+word*4,&value,4);}
   const float vertices[]={-1,-1,3,-1,-1,3};memcpy((char*)nn_bo_map(o->input[2])+256,vertices,sizeof(vertices));
   snapshots_clear(o);for(unsigned i=0;i<INPUTS;i++){if((e=snapshot(o,o->input[i],nn_bo_size(o->input[i])))||(e=nn_bo_cache(o->input[i],0,nn_bo_size(o->input[i]),0)))goto done;}
   for(unsigned i=0;i<2;i++)if((e=snapshot(o,shaders[i].code,nn_bo_size(shaders[i].code))))goto done;
   if((e=snapshot(o,o->work.job,job_bytes)))goto done;
   stage="submit";unknown=1;if((e=nn_queue_submit(o->queue,o->work.job,0,o->work.command_bytes,o->work.resources,o->work.resource_count,&timestamp)))goto done;submits++;
   for(size_t i=0;i<o->work.resource_count;i++){if(nn_bo_idle(o->work.resources[i])!=EBUSY){e=EPROTO;goto done;}busy++;}
   if(nn_bo_idle(o->target)!=EBUSY||nn_bo_idle(o->work.job)!=EBUSY||nn_pipeline_program_destroy(o->program)!=EBUSY){e=EPROTO;goto done;}busy+=3;
   for(unsigned i=3;i<INPUTS;i++){if(nn_bo_idle(o->input[i])){e=EPROTO;goto done;}unused++;}
   stage="finish";if((e=nn_queue_finish(o->queue,3000)))goto done;unknown=0;finishes++;
   for(unsigned i=3;i<INPUTS;i++){if(nn_bo_idle(o->input[i])){e=EPROTO;goto done;}unused++;}
   stage="guards";for(size_t off=0;off<target_bytes;off++){if(off>=4096){size_t r=off-4096;if(r/v->pitch<v->height&&r%v->pitch<v->width*4u)continue;}if(((unsigned char*)nn_bo_map(o->target))[off]!=0xa5){bad_case=test*2+repeat;bad_byte=off;e=EIO;goto done;}guards++;}
   stage="readonly";for(size_t i=0;i<o->watch_count;i++){const struct watch*w=o->watches+i;for(size_t off=0;off<w->count;off++){
    if(w->bo==o->work.job&&off>=45056&&off<45060)continue;
    int written=0;for(unsigned st=0;st<2;st++)if(w->bo==o->input[st]&&off>=4096+c->offset[st]&&off<4096+c->offset[st]+c->bytes)written=nn_storage_gpu_written(c->program,st,off-4096-c->offset[st]);if(written)continue;
    if(((unsigned char*)nn_bo_map(w->bo))[off]!=w->bytes[off]){bad_case=test*2+repeat;bad_byte=off;e=EIO;goto done;}readonly++;
   }}
   stage="save";if((e=save(directory,(test*2+repeat)*3,o->target))||(e=save(directory,(test*2+repeat)*3+1,o->input[0]))||(e=save(directory,(test*2+repeat)*3+2,o->input[1])))goto done;files+=3;if(repeat)replays++;
  }
  snapshots_clear(o);stage="job-retire";if((e=nn_bo_destroy(o->work.job)))goto done;memset(&o->work,0,sizeof(o->work));
 }
 stage="complete";
done:
 if(unknown)clean=EBUSY;else clean=release(o);int retained=unknown||clean;if(retained){o->next=quarantine;quarantine=o;}else free(o);
 int passed=!e&&!clean&&files==NN_STORAGE_GPU_FILES;int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"native_graphics_storage\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"jobs\":%u,\"programs\":%u,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"replays\":%u,\"inflight_owner_checks\":%u,\"unused_owner_checks\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"target_bo_bytes\":%zu,\"storage_bo_bytes\":12288,\"job_bytes\":%zu,\"resource_count\":7,\"target_prefix\":4096,\"storage_prefix\":4096,\"sequence_offset\":45056,\"sequence_bytes_exempt\":4,\"same_job_replay\":true,\"cpu_fixture_initialization\":true,\"cpu_shader_execution\":false,\"cpu_image_conversion\":false,\"pixel_oracle_on_host\":true,\"storage_oracle_on_host\":true,\"nvn_api\":false,\"game_modules_executed\":false,\"bad_case\":%u,\"bad_byte\":%zu,\"passed\":%s}\n",(unsigned)getuid(),chip,stage,e,clean,retained?"true":"false",jobs,programs,submits,finishes,files,replays,busy,unused,(unsigned long long)guards,(unsigned long long)readonly,target_bytes,job_bytes,bad_case,bad_byte,passed?"true":"false");return passed&&n>0&&(size_t)n<capacity?0:e?e:clean?clean:EIO;
}
