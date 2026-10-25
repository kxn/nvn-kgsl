/* SPDX-License-Identifier: MIT. Authored raw-native mip sampler, no NVN/game. */
#define NN_PIPELINE_PROBE_HELPERS_ONLY 1
#define PIPELINE_RAW_NAME "%s/sampler-mip-%03u.raw"
#include "pipeline_probe.c"
#include "sampler_mip_cases.h"
int nn_pipeline_probe(const char *directory,char *json,size_t capacity)
{
 struct owner*o=calloc(1,sizeof(*o));if(!o)return ENOMEM;
 unsigned submissions=0,finishes=0,files=0,jobs=0,programs=0,replays=0,busy=0,unused=0;
 uint64_t guards=0,readonly=0;uint32_t chip=0,timestamp=0;int e=0,cleanup=0,unknown=0;
 const char*stage="open";unsigned bad_case=UINT32_MAX;size_t bad_byte=SIZE_MAX;
 char*cases=calloc(1,24000);size_t used=0;size_t target_bytes=0,job_bytes=0,command_bytes=0,resource_count=0;uint64_t readonly_per_submit=0;if(!cases){free(o);return ENOMEM;}
 if((e=nn_device_open(&o->device)))goto done;
 chip=nn_device_chip(o->device);
 if(chip!=0x06050002||nn_device_gmem_bytes(o->device)!=nn_pipeline_device.gmem_bytes){e=ENOTSUP;goto done;}
 if((e=nn_queue_create(o->device,&o->queue)))goto done;
 const struct nn_fdl_image_view*whole=&NN_SAMPLER_MIP_VIEW(0);
 if((e=nn_bo_alloc(o->device,8192+whole->required_span,o->input))||
    (e=nn_bo_alloc(o->device,4096,o->input+1))||(e=nn_bo_alloc(o->device,4096,o->input+2))||
    (e=nn_bo_alloc(o->device,8192+NN_SAMPLER_MIP_TARGET.required_span_bytes,&o->target)))goto done;
 const struct nn_pipeline_native_artifact*artifact=&NN_SAMPLER_MIP_ARTIFACT;
 stage="program";if((e=nn_pipeline_program_create_native(o->device,artifact,&o->program,&o->retained_program)))goto done;programs++;
 struct nn_pipeline_stage_view shaders[2];for(unsigned i=0;i<2;i++)if((e=nn_pipeline_program_stage(o->program,i,shaders+i)))goto done;
 for(unsigned test=0;test<NN_SAMPLER_MIP_JOBS;test++){
  const struct nn_sampler_mip_case*c=nn_sampler_mip_cases+test;
  if(c->view>=2||c->wrap_index>=3){e=EPROTO;goto done;}
  const struct nn_fdl_image_view*v=&NN_SAMPLER_MIP_VIEW(c->view);
  static const unsigned modes[]={NVN_WRAP_MODE_REPEAT,NVN_WRAP_MODE_MIRRORED_REPEAT,NVN_WRAP_MODE_CLAMP_TO_EDGE};
  if(c->id!=test||c->view>=2||c->wrap_index>=3||v->required_span!=whole->required_span){e=EPROTO;goto done;}
  struct nn_pipeline_draw_state state={.texture_types=artifact->textures,.texture_bound=2,.sampler_bound=2};
  state.target=(struct nn_pipeline_image){&NN_SAMPLER_MIP_TARGET,{o->target,4096,NN_SAMPLER_MIP_TARGET.required_span_bytes}};
  state.sampled_color[1]=v;state.textures[1]=(struct nn_pipeline_image){NULL,{o->input[0],4096,v->required_span}};
  state.samplers[1]=(struct nn_sampler_state){.version=1,.min_filter=c->min_filter,.mag_filter=c->mag_filter,.wrap={modes[c->wrap_index],modes[c->wrap_index],modes[c->wrap_index]},.compare_func=NVN_COMPARE_FUNC_ALWAYS,.lod_min=nn_sampler_mip_float(c->lod_min_bits),.lod_max=nn_sampler_mip_float(c->lod_max_bits),.lod_bias=nn_sampler_mip_float(c->lod_bias_bits),.anisotropy=1};
  state.sampler_layouts[1]=(struct nn_sampler_layout){.width=v->view_width,.height=v->view_height,.levels=v->level_count,.samples=1,.dimensions=2,.normalized=1};
  state.ubo[1][5]=(struct nn_pipeline_buffer){o->input[1],256,64};nn_render_state_defaults(&state.render);
  if((e=nn_pipeline_full_raster(64,64,0,0,&state.raster)))goto done;
  state.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
  memset(nn_bo_map(o->target),0xa5,nn_bo_size(o->target));for(unsigned y=0;y<64;y++)memset((unsigned char*)nn_bo_map(o->target)+4096+y*NN_SAMPLER_MIP_TARGET.pitch,0,256);
  if((e=nn_bo_cache(o->target,0,nn_bo_size(o->target),0)))goto done;
  stage="encode";if((e=nn_pipeline_encode(o->device,o->program,artifact->recipe,&nn_pipeline_device,&state,&o->work,&o->retained_job)))goto done;jobs++;
  if(o->work.resource_count!=6||nn_bo_size(o->work.job)<45060){e=EPROTO;goto done;}
  for(size_t k=0;k<o->work.resource_count;k++)if(o->work.resources[k]==o->input[2]){e=EPROTO;goto done;}
  uint64_t ro=nn_bo_size(o->input[0])+nn_bo_size(o->input[1])+nn_bo_size(o->input[2])+nn_bo_size(shaders[0].code)+nn_bo_size(shaders[1].code)+nn_bo_size(o->work.job)-4;
  if(!test){target_bytes=nn_bo_size(o->target);job_bytes=nn_bo_size(o->work.job);command_bytes=o->work.command_bytes;resource_count=o->work.resource_count;readonly_per_submit=ro;}
  else if(target_bytes!=nn_bo_size(o->target)||job_bytes!=nn_bo_size(o->work.job)||command_bytes!=o->work.command_bytes||resource_count!=o->work.resource_count||readonly_per_submit!=ro){e=EPROTO;goto done;}
  /* Numeric rows preserve every actual state word compactly so the unchanged
   * normal Activity result buffer can carry the entire matrix, not a sample. */
  int n=snprintf(cases+used,24000-used,"%s[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]",used?",":"",c->id,c->min_filter,c->mag_filter,c->wrap_index,c->view,c->profile,c->scale_bits[0],c->scale_bits[1],c->lod_min_bits,c->lod_max_bits,c->lod_bias_bits);
  if(n<0||(size_t)n>=24000-used){e=EOVERFLOW;goto done;}used+=(size_t)n;
  for(unsigned repeat=0;repeat<2;repeat++){
   for(unsigned i=0;i<3;i++)memset(nn_bo_map(o->input[i]),0xa5,nn_bo_size(o->input[i]));
   /* Authored fixture native-LINEAR raw initialization, no CPU conversion from
    * a guest/canonical image and never a CPU-produced target/output. */
   for(unsigned mip=0;mip<whole->levels;mip++){
    const struct nn_fdl_level*l=whole->level+mip;
    for(unsigned y=0;y<l->height;y++)for(unsigned x=0;x<l->width;x++)for(unsigned channel=0;channel<4;channel++)
     ((unsigned char*)nn_bo_map(o->input[0]))[4096+l->offset+y*l->pitch+x*4+channel]=nn_sampler_mip_source(mip,x,y,channel,repeat);
   }
   float coordinates[]={nn_sampler_mip_float(c->scale_bits[0]),nn_sampler_mip_float(c->scale_bits[1]),-721.f/2048.f+repeat*.125f,-1297.f/4096.f-repeat*.25f};
   memset((unsigned char*)nn_bo_map(o->input[1])+256,0,64);memcpy((unsigned char*)nn_bo_map(o->input[1])+256,coordinates,16);
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
    if(off>=4096){size_t rel=off-4096;if(rel/NN_SAMPLER_MIP_TARGET.pitch<64&&rel%NN_SAMPLER_MIP_TARGET.pitch<256)continue;}
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
 stage="complete";
done:
 if(unknown)cleanup=EBUSY;else cleanup=release(o);int retained=unknown||cleanup;if(retained){o->next=quarantine;quarantine=o;}else free(o);
 int passed=!e&&!cleanup&&files==NN_SAMPLER_MIP_SUBMITS;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"native_sampler_mip\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"jobs\":%u,\"programs\":%u,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"replays\":%u,\"inflight_owner_checks\":%u,\"unused_owner_checks\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"bo_bytes\":%zu,\"job_bytes\":%zu,\"command_bytes\":%zu,\"resource_count\":%zu,\"readonly_bytes_per_submit\":%llu,\"pixel_checks\":0,\"pixel_oracle_on_host\":true,\"target_prefix\":4096,\"coordinate_ubo_binding\":5,\"coordinate_ubo_bytes\":64,\"sequence_offset\":45056,\"sequence_bytes_exempt\":4,\"same_job_replay\":true,\"cpu_fixture_source_initialization\":true,\"cpu_image_conversion\":false,\"nvn_api\":false,\"game_modules_executed\":false,\"bad_case\":%u,\"bad_byte\":%zu,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),chip,stage,e,cleanup,retained?"true":"false",jobs,programs,submissions,finishes,files,replays,busy,unused,(unsigned long long)guards,(unsigned long long)readonly,target_bytes,job_bytes,command_bytes,resource_count,(unsigned long long)readonly_per_submit,bad_case,bad_byte,cases,passed?"true":"false");
 free(cases);return passed&&n>0&&(size_t)n<capacity?0:e?e:cleanup?cleanup:EIO;
}
