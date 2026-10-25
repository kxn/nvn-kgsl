/* SPDX-License-Identifier: MIT. Authored raw-native TG4, not public NVN/game. */
#define NN_PIPELINE_PROBE_HELPERS_ONLY 1
#define PIPELINE_RAW_NAME "%s/gather-%03u.raw"
#include "pipeline_probe.c"
#include "gather_probe_cases.h"
int nn_pipeline_probe(const char *directory,char *json,size_t capacity)
{
 struct owner*o=calloc(1,sizeof(*o));if(!o)return ENOMEM;
 unsigned submissions=0,finishes=0,files=0,jobs=0,programs=0,replays=0,busy=0,unused=0,holes=0;
 uint64_t guards=0,readonly=0;uint32_t chip=0,timestamp=0;int e=0,cleanup=0,unknown=0;
 const char*stage="open";unsigned bad_case=UINT32_MAX;size_t bad_byte=SIZE_MAX;char cases[18000]={0};size_t used=0;
 if((e=nn_device_open(&o->device)))goto done;
 chip=nn_device_chip(o->device);
 if(chip!=0x06050002||nn_device_gmem_bytes(o->device)!=nn_pipeline_device.gmem_bytes){e=ENOTSUP;goto done;}
 if((e=nn_queue_create(o->device,&o->queue)))goto done;
 if((e=nn_bo_alloc(o->device,8192+NN_GATHER_TEXTURE.required_span_bytes,o->input))||
    (e=nn_bo_alloc(o->device,4096,o->input+1))||(e=nn_bo_alloc(o->device,4096,o->input+2))||
    (e=nn_bo_alloc(o->device,8192+NN_GATHER_TARGET.required_span_bytes,&o->target)))goto done;
 for(unsigned component=0;component<4;component++)for(unsigned sparse=0;sparse<2;sparse++){
  unsigned index=component*2+sparse,binding=sparse?13:1;const struct nn_pipeline_native_artifact*artifact=NN_GATHER_PROBE_ARTIFACTS+index;
  stage="program";if((e=nn_pipeline_program_create_native(o->device,artifact,&o->program,&o->retained_program)))goto done;programs++;
  struct nn_pipeline_stage_view shaders[2];for(unsigned i=0;i<2;i++)if((e=nn_pipeline_program_stage(o->program,i,shaders+i)))goto done;
  for(unsigned wrap=0;wrap<3;wrap++)for(unsigned origin=0;origin<2;origin++){
   unsigned test=((component*2+sparse)*3+wrap)*2+origin;static const unsigned modes[]={NVN_WRAP_MODE_REPEAT,NVN_WRAP_MODE_MIRRORED_REPEAT,NVN_WRAP_MODE_CLAMP_TO_EDGE};
   struct nn_pipeline_draw_state state={.texture_types=artifact->textures,.texture_bound=1u<<binding,.sampler_bound=1u<<binding};
   state.target=(struct nn_pipeline_image){&NN_GATHER_TARGET,{o->target,4096,NN_GATHER_TARGET.required_span_bytes}};
   state.textures[binding]=(struct nn_pipeline_image){&NN_GATHER_TEXTURE,{o->input[0],4096,NN_GATHER_TEXTURE.required_span_bytes}};
   state.samplers[binding]=(struct nn_sampler_state){.version=1,.min_filter=wrap==1?NVN_MIN_FILTER_LINEAR:NVN_MIN_FILTER_NEAREST,.mag_filter=wrap==1?NVN_MAG_FILTER_LINEAR:NVN_MAG_FILTER_NEAREST,.wrap={modes[wrap],modes[wrap],modes[wrap]},.compare_func=NVN_COMPARE_FUNC_ALWAYS,.lod_min=0,.lod_max=0,.anisotropy=1};
   state.sampler_layouts[binding]=(struct nn_sampler_layout){.width=8,.height=4,.levels=1,.samples=1,.dimensions=2,.normalized=1};
   state.ubo[1][5]=(struct nn_pipeline_buffer){o->input[1],256,64};nn_render_state_defaults(&state.render);
   if((e=nn_pipeline_full_raster(32,16,origin,0,&state.raster)))goto done;
   state.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
   memset(nn_bo_map(o->target),0xa5,nn_bo_size(o->target));for(unsigned y=0;y<16;y++)memset((unsigned char*)nn_bo_map(o->target)+4096+y*NN_GATHER_TARGET.pitch,0,128);
   if((e=nn_bo_cache(o->target,0,nn_bo_size(o->target),0)))goto done;
   stage="encode";if((e=nn_pipeline_encode(o->device,o->program,artifact->recipe,&nn_pipeline_device,&state,&o->work,&o->retained_job)))goto done;jobs++;
   if(o->work.resource_count!=6||nn_bo_size(o->work.job)<45060){e=EPROTO;goto done;}
   const unsigned char*job_map=nn_bo_map(o->work.job);for(unsigned slot=0;slot<16;slot++)if(slot!=binding){
    for(unsigned b=0;b<64;b++){if(job_map[40960+slot*64+b]){e=EPROTO;goto done;}holes++;}
    for(unsigned b=0;b<16;b++){if(job_map[36864+slot*16+b]){e=EPROTO;goto done;}holes++;}
   }
   for(size_t k=0;k<o->work.resource_count;k++)if(o->work.resources[k]==o->input[2]){e=EPROTO;goto done;}
   uint64_t ro=nn_bo_size(o->input[0])+nn_bo_size(o->input[1])+nn_bo_size(o->input[2])+nn_bo_size(shaders[0].code)+nn_bo_size(shaders[1].code)+nn_bo_size(o->work.job)-4;
   int n=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"component\":%u,\"binding\":%u,\"wrap_index\":%u,\"wrap\":%u,\"origin\":%u,\"width\":32,\"height\":16,\"pitch\":%u,\"span\":%u,\"bo_bytes\":%zu,\"source_pitch\":%u,\"source_span\":%u,\"source_bo_bytes\":%zu,\"job_bytes\":%zu,\"command_bytes\":%zu,\"resource_count\":%zu,\"readonly_bytes\":%llu}",used?",":"",test,component,binding,wrap,modes[wrap],origin,NN_GATHER_TARGET.pitch,NN_GATHER_TARGET.required_span_bytes,nn_bo_size(o->target),NN_GATHER_TEXTURE.pitch,NN_GATHER_TEXTURE.required_span_bytes,nn_bo_size(o->input[0]),nn_bo_size(o->work.job),o->work.command_bytes,o->work.resource_count,(unsigned long long)ro);
   if(n<0||(size_t)n>=sizeof(cases)-used){e=EOVERFLOW;goto done;}used+=(size_t)n;
   for(unsigned repeat=0;repeat<2;repeat++){
    for(unsigned i=0;i<3;i++)memset(nn_bo_map(o->input[i]),0xa5,nn_bo_size(o->input[i]));
    for(unsigned y=0;y<4;y++)for(unsigned x=0;x<8;x++)for(unsigned c=0;c<4;c++)((unsigned char*)nn_bo_map(o->input[0]))[4096+y*NN_GATHER_TEXTURE.pitch+x*4+c]=nn_gather_source(x,y,c,repeat);
    float coordinates[]={2.f,2.f,-.5f+repeat*.25f,-.5f-repeat*.25f};memset((unsigned char*)nn_bo_map(o->input[1])+256,0,64);memcpy((unsigned char*)nn_bo_map(o->input[1])+256,coordinates,16);
    snapshots_clear(o);for(unsigned i=0;i<3;i++){if((e=snapshot(o,o->input[i],nn_bo_size(o->input[i]))))goto done;if((e=nn_bo_cache(o->input[i],0,nn_bo_size(o->input[i]),0)))goto done;}
    for(unsigned i=0;i<2;i++)if((e=snapshot(o,shaders[i].code,nn_bo_size(shaders[i].code))))goto done;
    if((e=snapshot(o,o->work.job,nn_bo_size(o->work.job))))goto done;
    stage="submit";unknown=1;if((e=nn_queue_submit(o->queue,o->work.job,0,o->work.command_bytes,o->work.resources,o->work.resource_count,&timestamp)))goto done;submissions++;
    for(size_t i=0;i<o->work.resource_count;i++){if(nn_bo_idle(o->work.resources[i])!=EBUSY){e=EPROTO;goto done;}busy++;}
    if(nn_bo_idle(o->target)!=EBUSY||nn_bo_idle(o->work.job)!=EBUSY||nn_pipeline_program_destroy(o->program)!=EBUSY){e=EPROTO;goto done;}busy+=3;
    if(nn_bo_idle(o->input[2])){e=EPROTO;goto done;}unused++;
    stage="finish";if((e=nn_queue_finish(o->queue,3000)))goto done;unknown=0;finishes++;
    if(nn_bo_idle(o->input[2])){e=EPROTO;goto done;}unused++;
    stage="guards";const unsigned char*bytes=nn_bo_map(o->target);
    for(size_t off=0;off<nn_bo_size(o->target);off++){
     if(off>=4096){size_t rel=off-4096;if(rel/NN_GATHER_TARGET.pitch<16&&rel%NN_GATHER_TARGET.pitch<128)continue;}
     if(bytes[off]!=0xa5){bad_case=test*2+repeat;bad_byte=off;e=EIO;goto done;}guards++;
    }
    stage="readonly";for(size_t i=0;i<o->watch_count;i++){
     const struct watch*w=o->watches+i;const unsigned char*actual=nn_bo_map(w->bo);
     for(size_t off=0;off<w->count;off++){
      if(w->bo==o->work.job&&off>=45056&&off<45060)continue;
      if(actual[off]!=w->bytes[off]){bad_case=test*2+repeat;bad_byte=off;e=EIO;goto done;}readonly++;
     }
    }
    stage="save";if((e=save(directory,test*2+repeat,o->target)))goto done;files++;if(repeat)replays++;
   }
   snapshots_clear(o);stage="job-retire";if((e=nn_bo_destroy(o->work.job)))goto done;memset(&o->work,0,sizeof(o->work));
  }
  stage="program-retire";if((e=nn_pipeline_program_destroy(o->program)))goto done;o->program=NULL;
 }
 stage="complete";
done:
 if(unknown)cleanup=EBUSY;else cleanup=release(o);int retained=unknown||cleanup;if(retained){o->next=quarantine;quarantine=o;}else free(o);
 int passed=!e&&!cleanup&&files==NN_GATHER_SUBMITS;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"native_gather\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"jobs\":%u,\"programs\":%u,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"replays\":%u,\"inflight_owner_checks\":%u,\"unused_owner_checks\":%u,\"sparse_hole_checks\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"pixel_checks\":0,\"pixel_oracle_on_host\":true,\"target_prefix\":4096,\"coordinate_ubo_binding\":5,\"coordinate_ubo_bytes\":64,\"sequence_offset\":45056,\"sequence_bytes_exempt\":4,\"same_job_replay\":true,\"cpu_image_conversion\":false,\"nvn_api\":false,\"game_modules_executed\":false,\"bad_case\":%u,\"bad_byte\":%zu,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),chip,stage,e,cleanup,retained?"true":"false",jobs,programs,submissions,finishes,files,replays,busy,unused,holes,(unsigned long long)guards,(unsigned long long)readonly,bad_case,bad_byte,cases,passed?"true":"false");
 return passed&&n>0&&(size_t)n<capacity?0:e?e:cleanup?cleanup:EIO;
}
