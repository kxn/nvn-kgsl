/* Authored raw-native22 color formats. Shared Activity/owner/transport above.
 * Pixel correctness is checked offline with actual Mesa HOST pack/unpack plus
 * independent original color closed form, never CPU-rendered/uploaded output. */
#include "nvn_native/color_target.h"
#include "color_target_cases.h"
static void color_inputs(struct owner *o,unsigned state,unsigned repeat)
{
 for(unsigned i=0;i<2;i++)memset(nn_bo_map(o->input[i]),0xa5,nn_bo_size(o->input[i]));
 float matrix[16]={0};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;matrix[3]=repeat?.5f:0;
 memcpy((unsigned char*)nn_bo_map(o->input[0])+64,matrix,64);
 const float pos[3][2]={{-1,-1},{3,-1},{-1,3}};float color[4];nn_color_input(state,repeat,color);
 for(unsigned i=0;i<3;i++){
  unsigned char*p=(unsigned char*)nn_bo_map(o->input[1])+256+i*24;
  memcpy(p,pos[i],8);memcpy(p+8,color,16);
 }
}
int nn_pipeline_probe(const char *directory,char *json,size_t capacity)
{
 struct owner*o=calloc(1,sizeof(*o));if(!o)return ENOMEM;
 const struct nn_pipeline_archive*archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 unsigned submissions=0,finishes=0,files=0,jobs=0,replays=0,completed=0,busy=0;
 uint64_t guards=0,readonly=0,active_bytes=0;uint32_t chip=0,timestamp=0;
 unsigned bad_case=UINT32_MAX;size_t bad_byte=SIZE_MAX;unsigned bad_actual=0;
 int e=0,cleanup=0,unknown=0;const char*stage="contract";char cases[24000]={0};size_t used=0;
 const struct nn_pipeline_pair_recipe*pair=NULL;
 if(NN_PIPELINE_QUAL_PAIR_INDEX>=archive->pair_count){e=ENOTSUP;goto done;}
 pair=archive->pairs+NN_PIPELINE_QUAL_PAIR_INDEX;if((e=contract(archive,pair)))goto done;
 stage="open";if((e=nn_device_open(&o->device)))goto done;chip=nn_device_chip(o->device);
 if(chip!=0x06050002||nn_device_gmem_bytes(o->device)!=archive->platform->gmem_bytes){e=ENOTSUP;goto done;}
 stage="program";const struct nn_offline_graphics_stage *vs=archive->stages->stages+pair->vs_index,*fs=archive->stages->stages+pair->fs_index;
 if((e=nn_pipeline_program_create(o->device,archive->stages,vs,fs,vs->specialization,fs->specialization,&o->program,&o->retained_program)))goto done;
 struct nn_pipeline_stage_view stages[2];for(unsigned i=0;i<2;i++)if((e=nn_pipeline_program_stage(o->program,i,stages+i)))goto done;
 if(stages[0].c1||stages[1].c1){e=ENOTSUP;goto done;}
 stage="queue";if((e=nn_queue_create(o->device,&o->queue)))goto done;
 for(unsigned i=0;i<2;i++)if((e=nn_bo_alloc(o->device,4096,o->input+i)))goto done;
 for(unsigned fi=0;fi<NN_COLOR_FORMATS;fi++)for(unsigned state_id=0;state_id<3;state_id++){
  unsigned test=fi*3+state_id;struct nn_tex_native_view view;struct nn_color_target_info info;
  stage="actual-fdl";if((e=nn_color_target_create(chip,nn_color_formats[fi],NN_COLOR_WIDTH,NN_COLOR_HEIGHT,0,&view,&info)))goto done;
  stage="target";if((e=nn_bo_alloc(o->device,4096+view.required_span_bytes+4096,&o->target)))goto done;
  if(!nn_bo_map(o->target)){e=EFAULT;goto done;}memset(nn_bo_map(o->target),0xa5,nn_bo_size(o->target));
  for(unsigned y=0;y<NN_COLOR_HEIGHT;y++)memset((unsigned char*)nn_bo_map(o->target)+4096+(size_t)y*view.pitch,0,(size_t)NN_COLOR_WIDTH*info.bytes_per_pixel);
  /* Only initial background bytes are CPU-written; every color result comes
   * from the original VS/FS directly into this actual native target. */
  if((e=nn_bo_cache(o->target,0,nn_bo_size(o->target),0)))goto done;
  struct nn_pipeline_draw_state state={.target={&view,{o->target,4096,view.required_span_bytes}}};
  nn_render_state_defaults(&state.render);if((e=nn_pipeline_full_raster(NN_COLOR_WIDTH,NN_COLOR_HEIGHT,0,1,&state.raster)))goto done;
  if(state_id==1){state.render.blend_enable=1;state.render.blend[0].src_rgb=NVN_BLEND_FUNC_SRC_ALPHA;state.render.blend[0].dst_rgb=NVN_BLEND_FUNC_ONE_MINUS_SRC_ALPHA;}
  if(state_id==2)state.render.color_mask=(state.render.color_mask&~15u)|5u;
  state.vertex.attribute_count=state.vertex.stream_count=32;state.vertex.streams[3]=(struct nn_pipeline_stream){24,0};
  state.vertex.buffers[3]=(struct nn_pipeline_buffer){o->input[1],256,72};
  state.vertex.attributes[0]=(struct nn_pipeline_attribute){NVN_FORMAT_RG32F,0,3};
  state.vertex.attributes[1]=(struct nn_pipeline_attribute){NVN_FORMAT_RGBA32F,8,3};
  state.ubo[0][1]=(struct nn_pipeline_buffer){o->input[0],64,64};
  state.draw=(struct nn_gfx_draw){.mode=NN_GFX_DRAW_DIRECT,.primitive=4,.count=3,.instances=1};
  for(unsigned i=0;i<2;i++)memset(nn_bo_map(o->input[i]),0xa5,nn_bo_size(o->input[i]));
  stage="encode";if((e=nn_pipeline_encode(o->device,o->program,pair->recipe,archive->platform,&state,&o->work,&o->retained_job)))goto done;jobs++;
  if(nn_bo_size(o->work.job)<NN_COLOR_SEQUENCE_OFFSET+NN_COLOR_SEQUENCE_BYTES){e=EPROTO;goto done;}
  int n=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"format_index\":%u,\"format\":%u,\"state\":%u,\"width\":%u,\"height\":%u,\"cpp\":%u,\"component_mask\":%u,\"pitch\":%u,\"span\":%u,\"bo_bytes\":%zu,\"job_bytes\":%zu,\"command_bytes\":%zu,\"resource_count\":%zu,\"rb_mrt_buf_info\":%u,\"sp_ps_mrt_reg\":%u,\"srgb\":%u}",used?",":"",test,fi,view.format,state_id,view.width,view.height,info.bytes_per_pixel,info.component_mask,view.pitch,view.required_span_bytes,nn_bo_size(o->target),nn_bo_size(o->work.job),o->work.command_bytes,o->work.resource_count,view.rb_mrt_buf_info,view.sp_ps_mrt_reg,info.srgb);
  if(n<0||(size_t)n>=sizeof(cases)-used){e=EOVERFLOW;goto done;}used+=(size_t)n;
  for(unsigned repeat=0;repeat<2;repeat++){
   color_inputs(o,state_id,repeat);snapshots_clear(o);
   for(unsigned i=0;i<2;i++){if((e=snapshot(o,o->input[i],nn_bo_size(o->input[i]))))goto done;if((e=nn_bo_cache(o->input[i],0,nn_bo_size(o->input[i]),0)))goto done;}
   for(unsigned i=0;i<2;i++)if((e=snapshot(o,stages[i].code,nn_bo_size(stages[i].code))))goto done;
   if((e=snapshot(o,o->work.job,nn_bo_size(o->work.job))))goto done;
   stage="submit";unknown=1;if((e=nn_queue_submit(o->queue,o->work.job,0,o->work.command_bytes,o->work.resources,o->work.resource_count,&timestamp)))goto done;submissions++;
   for(size_t i=0;i<o->work.resource_count;i++){if(nn_bo_idle(o->work.resources[i])!=EBUSY){e=EPROTO;goto done;}busy++;}
   if(nn_bo_idle(o->target)!=EBUSY||nn_bo_idle(o->work.job)!=EBUSY){e=EPROTO;goto done;}busy+=2;
   stage="finish";if((e=nn_queue_finish(o->queue,3000)))goto done;unknown=0;finishes++;
   stage="guards";const unsigned char*map=nn_bo_map(o->target);int bad=0;
   for(size_t off=0;off<nn_bo_size(o->target);off++){
    if(off>=4096){size_t rel=off-4096;if(rel/view.pitch<view.height&&rel%view.pitch<(size_t)view.width*info.bytes_per_pixel)continue;}
    if(map[off]!=0xa5){bad_case=test*2+repeat;bad_byte=off;bad_actual=map[off];e=EIO;bad=1;break;}guards++;
   }
   stage=bad?stage:"readonly";
   for(size_t i=0;i<o->watch_count&&!bad;i++){
    const struct watch*w=o->watches+i;const unsigned char*p=nn_bo_map(w->bo);
    for(size_t off=0;off<w->count;off++){
     /* CP_EVENT_WRITE timestamp is hardware-produced. Exactly its declared
      * DWORD is mutable; command/data pages and adjacent bytes remain RO. */
     if(w->bo==o->work.job&&off>=NN_COLOR_SEQUENCE_OFFSET&&off<NN_COLOR_SEQUENCE_OFFSET+NN_COLOR_SEQUENCE_BYTES)continue;
     if(p[off]!=w->bytes[off]){bad_case=test*2+repeat;bad_byte=off;bad_actual=p[off];e=EIO;bad=1;break;}readonly++;
    }
   }
   int saved=save(directory,test*2+repeat,o->target);if(saved&&!e)e=saved;else if(!saved)files++;
   if(e){goto done;}active_bytes+=(uint64_t)view.width*view.height*info.bytes_per_pixel;completed++;if(repeat)replays++;
  }
  snapshots_clear(o);stage="retire";if((e=nn_bo_destroy(o->work.job)))goto done;memset(&o->work,0,sizeof(o->work));
  if((e=nn_bo_destroy(o->target))){goto done;}o->target=NULL;
 }
 stage="complete";
done:
 if(unknown)cleanup=EBUSY;else cleanup=release(o);
 int retained=unknown||cleanup;if(retained){o->next=quarantine;quarantine=o;}else free(o);
 int passed=!e&&!cleanup&&completed==NN_COLOR_CASES;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"native_color_targets\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"jobs\":%u,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"completed_cases\":%u,\"replays\":%u,\"inflight_owner_checks\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"active_bytes_saved\":%llu,\"pixel_checks\":0,\"pixel_oracle_on_host\":true,\"target_prefix\":4096,\"native_ubo_binding\":1,\"original_maxwell_cb\":3,\"ubo_bytes\":64,\"sequence_offset\":45056,\"sequence_bytes_exempt\":4,\"same_job_replay\":true,\"cpu_image_conversion\":false,\"nvn_api\":false,\"game_modules_executed\":false,\"legacy_image_template\":false,\"bad_case\":%u,\"bad_byte\":%zu,\"bad_actual\":%u,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),chip,stage,e,cleanup,retained?"true":"false",jobs,submissions,finishes,files,completed,replays,busy,(unsigned long long)guards,(unsigned long long)readonly,(unsigned long long)active_bytes,bad_case,bad_byte,bad_actual,cases,passed?"true":"false");
 return passed&&n>0&&(size_t)n<capacity?0:e?e:cleanup?cleanup:EIO;
}
