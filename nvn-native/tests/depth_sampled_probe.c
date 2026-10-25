/* SPDX-License-Identifier: MIT. Raw native sampled-depth qualification only. */
#define NN_PIPELINE_PROBE_HELPERS_ONLY 1
#define PIPELINE_RAW_NAME "%s/depth-sampled-%03u.raw"
#include "pipeline_probe.c"
#include "depth_shadow_cases.h"
#include "nvn_native/depth_sampled_native.h"
#include "nvn_native/pipeline_views.h"
struct sampled_owner {struct owner common;struct nn_depth_sampled_shadow sampled;nn_bo*canonical;struct nn_pipeline_views*views;};
static int sampled_release(struct sampled_owner*d)
{
 struct owner*o=&d->common;int e;if(o->queue){if((e=nn_queue_destroy(o->queue)))return e;o->queue=NULL;}
 if((e=nn_depth_sampled_destroy(&d->sampled)))return e;
 if(d->canonical){if((e=nn_bo_destroy(d->canonical)))return e;d->canonical=NULL;}
 if((e=release(o)))return e;
 nn_pipeline_views_destroy(d->views);d->views=NULL;return 0;
}
static size_t sampled_seq(const struct sampled_owner*d,const nn_bo*b)
{return b==d->sampled.copy.job?6144:b==d->sampled.upload.job?40960:b==d->common.work.job?45056:SIZE_MAX;}
int nn_pipeline_probe(const char*directory,char*json,size_t capacity)
{
 if(!directory||!json||capacity<32768)return EINVAL;
 struct sampled_owner*d=calloc(1,sizeof(*d));if(!d)return ENOMEM;struct owner*o=&d->common;
 unsigned submissions=0,finishes=0,completed=0,files=0,replays=0,programs=0,owners=0,busy=0;uint64_t guards=0,readonly=0;
 int e=0,cleanup=0,unknown=0;const char*stage="open";unsigned bad_case=UINT32_MAX;size_t bad_byte=SIZE_MAX;char cases[16000]={0};size_t used=0;uint32_t chip=0;
 if((e=nn_device_open(&o->device)))goto done;
 chip=nn_device_chip(o->device);if(chip!=0x06050002||nn_device_gmem_bytes(o->device)!=nn_pipeline_device.gmem_bytes){e=ENOTSUP;goto done;}
 if((e=nn_queue_create(o->device,&o->queue))||(e=nn_pipeline_views_create(chip,&d->views))||(e=nn_bo_alloc(o->device,4096,o->input)))goto done;
 for(unsigned format=50;format<=52;format++)for(unsigned role=0;role<5;role++)for(unsigned shape=0;shape<2;shape++){
  unsigned test=(format-50)*10+role*2+shape;
  const struct nn_pipeline_native_artifact*artifacts[]={&depth_sampled_sample_artifact,&depth_sampled_sample_artifact,&depth_sampled_fetch_artifact,&depth_sampled_size_artifact,&depth_sampled_gather_artifact};const struct nn_pipeline_native_artifact*a=artifacts[role];
  struct nn_depth_shadow_case c={.format=format,.level=shape,.layer=shape};struct nn_tex_source_desc sd={.format=format,.target=shape?4:1,.width=shape?73:17,.height=shape?41:11,.levels=shape?3:1,.layers=shape?2:1,.block_height_log2=-1};
  stage="source-metadata";if((e=nn_tex_source_layout(&sd,&c.source)))goto done;
  uint32_t swizzle[4]={NN_DEPTH_R,NN_DEPTH_ZERO,NN_DEPTH_ZERO,NN_DEPTH_ONE};struct nn_depth_sampled_plan plan;if((e=nn_depth_sampled_plan(&c.source,c.level,c.layer,swizzle,&plan)))goto done;
  unsigned width=shape?41:33,height=shape?23:19;const struct nn_tex_native_view*view=NULL;
  if((e=nn_pipeline_views_get(d->views,width,height,0,&view))||(e=nn_bo_alloc(o->device,8192+view->required_span_bytes,&o->target))||(e=nn_bo_alloc(o->device,8192+c.source.payload_bytes,&d->canonical)))goto done;
  struct nn_block_copy_range canonical={d->canonical,4096,(size_t)c.source.payload_bytes,4096};
  stage="sampled-owner";if((e=nn_depth_sampled_create(o->device,&NN_DEPTH_SAMPLED_BLOCK_IMAGE,&nn_pipeline_device,&c.source,&plan,&canonical,32,&d->sampled)))goto done;owners++;
  stage="program";if((e=nn_pipeline_program_create_native(o->device,a,&o->program,&o->retained_program)))goto done;programs++;
  struct nn_pipeline_stage_view shaders[2];for(unsigned s=0;s<2;s++)if((e=nn_pipeline_program_stage(o->program,s,shaders+s)))goto done;
  struct nn_pipeline_draw_state state={.texture_types=a->textures,.texture_bound=2,.sampler_bound=a->textures->sampler_mask};state.target=(struct nn_pipeline_image){view,{o->target,4096,view->required_span_bytes}};
  state.textures[1].buffer=(struct nn_pipeline_buffer){d->sampled.native.storage,0,plan.native.required_span};state.sampled_depth[1]=&plan.native;
  if(state.sampler_bound){state.samplers[1]=(struct nn_sampler_state){.version=1,.min_filter=role==1?NVN_MIN_FILTER_LINEAR:NVN_MIN_FILTER_NEAREST,.mag_filter=role==1?NVN_MAG_FILTER_LINEAR:NVN_MAG_FILTER_NEAREST,.wrap={7,7,7},.compare_func=NVN_COMPARE_FUNC_ALWAYS,.anisotropy=1};state.sampler_layouts[1]=(struct nn_sampler_layout){.width=plan.native.view_width,.height=plan.native.view_height,.levels=1,.samples=1,.dimensions=2,.normalized=1,.depth=1};}
  if(role<2)state.ubo[1][5]=(struct nn_pipeline_buffer){o->input[0],256,64};
  nn_render_state_defaults(&state.render);if((e=nn_pipeline_full_raster(width,height,shape,0,&state.raster)))goto done;state.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
  stage="encode";if((e=nn_pipeline_encode(o->device,o->program,a->recipe,&nn_pipeline_device,&state,&o->work,&o->retained_job)))goto done;
  struct nn_depth_shadow_step load[2];if((e=nn_depth_sampled_sequence(&d->sampled,load)))goto done;
  nn_bo*refs[32];size_t refs_n=0;if((e=nn_depth_sampled_resources(&d->sampled,refs,32,&refs_n)))goto done;
  for(size_t i=0;i<o->work.resource_count;i++){nn_bo*b=o->work.resources[i];size_t j=0;for(;j<refs_n;j++)if(refs[j]==b)break;if(j==refs_n){if(refs_n==32){e=ENOSPC;goto done;}refs[refs_n++]=b;}}
  uint64_t ro=nn_bo_size(d->canonical)+nn_bo_size(d->sampled.code)+nn_bo_size(d->sampled.uniform)+nn_bo_size(d->sampled.copy.job)+nn_bo_size(d->sampled.upload.job)+nn_bo_size(o->work.job)+nn_bo_size(o->input[0])+nn_bo_size(shaders[0].code)+nn_bo_size(shaders[1].code)-12;
  int n=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"format\":%u,\"role\":%u,\"shape\":%u,\"level\":%u,\"layer\":%u,\"source_width\":%u,\"source_height\":%u,\"width\":%u,\"height\":%u,\"origin\":%u,\"pitch\":%u,\"span\":%u,\"bo_bytes\":%zu,\"canonical_bytes\":%zu,\"resource_count\":%zu,\"readonly_bytes\":%llu}",used?",":"",test,format,role,shape,c.level,c.layer,plan.native.view_width,plan.native.view_height,width,height,shape,view->pitch,view->required_span_bytes,nn_bo_size(o->target),nn_bo_size(d->canonical),refs_n,(unsigned long long)ro);
  if(n<0||(size_t)n>=sizeof(cases)-used){e=EOVERFLOW;goto done;}used+=(size_t)n;
  for(unsigned repeat=0;repeat<2;repeat++){
   bad_case=test*2+repeat;stage="authored-input";if((e=nn_depth_fixture_input(&c,test,repeat,nn_bo_map(d->canonical),nn_bo_size(d->canonical)))||(e=nn_bo_cache(d->canonical,0,nn_bo_size(d->canonical),0)))goto done;
   memset(nn_bo_map(o->target),0xa5,nn_bo_size(o->target));for(unsigned y=0;y<height;y++)memset((unsigned char*)nn_bo_map(o->target)+4096+y*view->pitch,0,width*4);if((e=nn_bo_cache(o->target,0,nn_bo_size(o->target),0)))goto done;
   memset(nn_bo_map(o->input[0]),0xa5,4096);float coordinates[4]={role==1?(float)width/(2*plan.native.view_width):1.f,role==1?(float)height/(2*plan.native.view_height):1.f,0,0};memset((unsigned char*)nn_bo_map(o->input[0])+256,0,64);memcpy((unsigned char*)nn_bo_map(o->input[0])+256,coordinates,16);if((e=nn_bo_cache(o->input[0],0,4096,0)))goto done;
   snapshots_clear(o);nn_bo*watch[]={d->canonical,d->sampled.code,d->sampled.uniform,d->sampled.copy.job,d->sampled.upload.job,o->work.job,o->input[0],shaders[0].code,shaders[1].code};for(unsigned i=0;i<9;i++)if((e=snapshot(o,watch[i],nn_bo_size(watch[i]))))goto done;
   struct nn_depth_shadow_step steps[3]={load[0],load[1],{o->work.job,o->work.command_bytes}};
   for(unsigned s=0;s<3;s++){stage=s==0?"untile":s==1?"upload":"sample";uint32_t timestamp;unknown=1;if((e=nn_queue_submit(o->queue,steps[s].job,0,steps[s].bytes,refs,refs_n,&timestamp)))goto done;submissions++;
    for(size_t i=0;i<refs_n;i++){if(nn_bo_idle(refs[i])!=EBUSY){e=EPROTO;goto done;}busy++;}
    struct nn_depth_sampled_shadow before=d->sampled;if(nn_depth_sampled_destroy(&d->sampled)!=EBUSY||memcmp(&before,&d->sampled,sizeof(before))){e=EPROTO;goto done;}busy++;
    if(nn_bo_destroy(d->canonical)!=EBUSY||nn_pipeline_program_destroy(o->program)!=EBUSY){e=EPROTO;goto done;}busy+=2;
   }
   stage="finish";if((e=nn_queue_finish(o->queue,3000)))goto done;unknown=0;finishes++;
   stage="guards";if((e=nn_bo_cache(o->target,0,nn_bo_size(o->target),1)))goto done;const unsigned char*map=nn_bo_map(o->target);
   for(size_t at=0;at<nn_bo_size(o->target);at++){if(at>=4096){size_t r=at-4096;if(r/view->pitch<height&&r%view->pitch<width*4)continue;}if(map[at]!=0xa5){bad_byte=at;e=EILSEQ;goto done;}guards++;}
   stage="readonly";for(size_t i=0;i<o->watch_count;i++){const struct watch*w=o->watches+i;if((e=nn_bo_cache(w->bo,0,w->count,1)))goto done;map=nn_bo_map(w->bo);size_t seq=sampled_seq(d,w->bo);for(size_t at=0;at<w->count;at++){if(at>=seq&&at-seq<4)continue;if(map[at]!=w->bytes[at]){bad_byte=at;e=EILSEQ;goto done;}readonly++;}}
   stage="save";if((e=save(directory,test*2+repeat,o->target)))goto done;files++;completed++;if(repeat)replays++;
  }
  snapshots_clear(o);stage="retire";if((e=nn_bo_destroy(o->work.job)))goto done;memset(&o->work,0,sizeof(o->work));if((e=nn_depth_sampled_destroy(&d->sampled))||(e=nn_bo_destroy(d->canonical)))goto done;d->canonical=NULL;if((e=nn_bo_destroy(o->target)))goto done;o->target=NULL;if((e=nn_pipeline_program_destroy(o->program)))goto done;o->program=NULL;
 }
 stage="complete";bad_case=UINT32_MAX;
done:
 if(unknown)cleanup=EBUSY;else cleanup=sampled_release(d);int retained=unknown||cleanup;if(retained){o->next=quarantine;quarantine=o;}else free(d);
 int passed=!e&&!cleanup&&completed==60;int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"native_depth_sampled\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"programs\":%u,\"owners\":%u,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"completed_cases\":%u,\"replays\":%u,\"inflight_owner_checks\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"private_depth_cpu_access\":false,\"cpu_image_conversion\":false,\"nvn_api\":false,\"game_modules_executed\":false,\"compare_sampler\":false,\"same_job_replay\":true,\"pixel_oracle_on_host\":true,\"bad_case\":%u,\"bad_byte\":%zu,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),chip,stage,e,cleanup,retained?"true":"false",programs,owners,submissions,finishes,files,completed,replays,busy,(unsigned long long)guards,(unsigned long long)readonly,bad_case,bad_byte,cases,passed?"true":"false");return passed&&n>0&&(size_t)n<capacity?0:e?e:cleanup?cleanup:EIO;
}
