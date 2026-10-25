/* SPDX-License-Identifier: MIT
 * Authored raw-native dynamic pipeline caller. Original color VS/FS only;
 * native VS binding1 is explicitly supplied with original CB3's matrix64B.
 * This does not infer an NVN API uniform slot or an observed game pipeline.
 * No nn_gfx_image, CPU image conversion, runtime compiler or game execution. */
#define _POSIX_C_SOURCE 200809L
#include "pipeline_probe.h"
#include <nvn.h>
#include "nvn_native/resource_access.h"
#include "nvn_native/pipeline.h"
#include "nvn_native/pipeline_archive.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_PIPELINE_QUALIFICATION_HEADER
#ifndef NVN_NATIVE_COLOR_TARGET_PROBE
#define NVN_NATIVE_COLOR_TARGET_PROBE 0
#endif
#ifndef PIPELINE_RAW_NAME
#if NVN_NATIVE_COLOR_TARGET_PROBE
#define PIPELINE_RAW_NAME "%s/color-target-%03u.raw"
#else
#define PIPELINE_RAW_NAME "%s/pipeline-%02u.rgba"
#endif
#endif

enum {LAYOUTS=3,MODES=4,REPEATS=2,SHAPES=3,INPUTS=5};
struct watch {nn_bo *bo;unsigned char *bytes;size_t count;};
struct owner {
 nn_device *device;nn_queue *queue;nn_pipeline_program *program,*retained_program;
 nn_bo *input[INPUTS],*target,*retained_job;struct nn_pipeline_submission work;
 struct watch watches[16];size_t watch_count;struct owner *next;
};
static struct owner *quarantine;
static int save(const char *directory,unsigned number,nn_bo *bo)
{
 char path[1400];int n=snprintf(path,sizeof(path),PIPELINE_RAW_NAME,directory,number);
 if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;
 const unsigned char *p=nn_bo_map(bo);size_t bytes=nn_bo_size(bo),off=0;int e=0;
 while(off<bytes){ssize_t w=write(fd,p+off,bytes-off);if(w<0&&errno==EINTR)continue;if(w<=0){e=w<0?errno:EIO;break;}off+=(size_t)w;}
 if(!e&&fsync(fd))e=errno;
 if(close(fd)&&!e)e=errno;
 return e;
}
static int release(struct owner *o)
{
 int e;if(o->queue){e=nn_queue_destroy(o->queue);if(e)return e;o->queue=NULL;}
 if(o->work.job){e=nn_bo_destroy(o->work.job);if(e)return e;o->work.job=NULL;}
 if(o->retained_job){e=nn_bo_destroy(o->retained_job);if(e)return e;o->retained_job=NULL;}
 if(o->target){e=nn_bo_destroy(o->target);if(e)return e;o->target=NULL;}
 for(unsigned i=0;i<INPUTS;i++)if(o->input[i]){e=nn_bo_destroy(o->input[i]);if(e)return e;o->input[i]=NULL;}
 if(o->program){e=nn_pipeline_program_destroy(o->program);if(e)return e;o->program=NULL;}
 if(o->retained_program){e=nn_pipeline_program_destroy(o->retained_program);if(e)return e;o->retained_program=NULL;}
 if(o->device){e=nn_device_close(o->device);if(e)return e;o->device=NULL;}
 for(size_t i=0;i<o->watch_count;i++){free(o->watches[i].bytes);o->watches[i].bytes=NULL;}o->watch_count=0;
 return 0;
}
static int snapshot(struct owner *o,nn_bo *bo,size_t bytes)
{
 for(size_t i=0;i<o->watch_count;i++)if(o->watches[i].bo==bo)return 0;
 if(o->watch_count==16||bytes>nn_bo_size(bo)||!nn_bo_map(bo))return EINVAL;
 unsigned char *p=malloc(bytes);if(!p)return ENOMEM;memcpy(p,nn_bo_map(bo),bytes);
 o->watches[o->watch_count++]=(struct watch){bo,p,bytes};return 0;
}
static void snapshots_clear(struct owner *o)
{for(size_t i=0;i<o->watch_count;i++)free(o->watches[i].bytes);o->watch_count=0;}
#ifndef NN_PIPELINE_PROBE_HELPERS_ONLY
#if !NVN_NATIVE_COLOR_TARGET_PROBE
static unsigned quantize(double f)
{if(f<0)f=0;if(f>1)f=1;return (unsigned)floor(f*255.0+.5);}
/* Independent original shader closed form: fullscreen triangle barycentrics.
 * Repeat moves clip x by .75, i.e.3/8 viewport width; the left region retains
 * prior actual GPU output. No pixel is read to generate an expected value. */
static void pixel(unsigned width,unsigned height,unsigned mode,unsigned repeat,unsigned x,unsigned y,unsigned rgba[4])
{
 double u=((double)x+.5)/(double)width,v=((double)y+.5)/(double)height;
 int shifted=repeat&&u>=.375;
 if(shifted)u-=.375;
 if(shifted&&mode==3){rgba[0]=255;rgba[1]=0;rgba[2]=255;rgba[3]=255;return;}
 double a=1-(u+v)*.5,b=u*.5,c=v*.5;
 rgba[0]=quantize(shifted?c:a);rgba[1]=quantize(shifted?a:b);rgba[2]=quantize(shifted?b:c);rgba[3]=255;
}
static void put_float(unsigned char *p,float v){memcpy(p,&v,4);}
static void inputs(struct owner *o,struct nn_pipeline_draw_state *s,unsigned layout,unsigned mode,unsigned repeat)
{
 for(unsigned i=0;i<INPUTS;i++)memset(nn_bo_map(o->input[i]),0xa5,nn_bo_size(o->input[i]));
 float matrix[16]={0};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;matrix[3]=repeat?.75f:0;
 memcpy((unsigned char *)nn_bo_map(o->input[0])+64,matrix,64);
 static const float pos[3][2]={{-1,-1},{3,-1},{-1,3}};
 for(unsigned i=0;i<6;i++){
  unsigned v=i%3;unsigned char color[4]={0,0,0,255};color[repeat?(v+1)%3:v]=255;
  if(i>=3){color[0]=255;color[1]=0;color[2]=255;}
  unsigned char *p=(unsigned char *)nn_bo_map(o->input[1])+256+i*(layout==0?24:layout==1?64:16);
  unsigned po=layout==0?0:layout==1?8:4;
  put_float(p+po,pos[v][0]);put_float(p+po+4,pos[v][1]);
  if(layout<2){unsigned co=layout==0?8:32;for(unsigned c=0;c<4;c++)put_float(p+co+c*4,color[c]/255.0f);}
  else memcpy((unsigned char *)nn_bo_map(o->input[2])+128+i*12+4,color,4);
 }
 uint16_t indices[6]={0,1,2,3,4,5};memcpy((unsigned char *)nn_bo_map(o->input[3])+64,indices,sizeof(indices));
 uint32_t args[10]={3,1,0,0,0,3,1,3,0,0};
 if(mode==2){args[0]=3;args[1]=1;args[2]=0;args[3]=0;}
 memcpy((unsigned char *)nn_bo_map(o->input[4])+64,args,sizeof(args));
 uint32_t count=repeat?2:1;memcpy((unsigned char *)nn_bo_map(o->input[4])+256,&count,4);
 (void)s;
}
#endif
static int contract(const struct nn_pipeline_archive *a,const struct nn_pipeline_pair_recipe *pair)
{
 const struct nn_pipeline_shader_recipe *r=pair->recipe;
 if(nn_pipeline_archive_validate(a)||!r||r->chip!=0x06050002||r->texture_mask||r->sampler_mask||r->input_count!=2||r->has_kill||r->fragcoord_mask||
    pair->vs_index>=a->stages->stage_count||pair->fs_index>=a->stages->stage_count)return ENOTSUP;
 if(r->inputs[0].location!=0||r->inputs[0].compmask!=3||r->inputs[1].location!=1||r->inputs[1].compmask!=15)return ENOTSUP;
 unsigned ubos=0;
 for(size_t i=0;i<r->patch_count;i++)if(r->patches[i].role==NN_PIPELINE_UBO){const struct nn_pipeline_address_patch *p=r->patches+i;if(p->stage||p->binding!=1||p->offset||p->bytes!=64)return ENOTSUP;ubos++;}
 return ubos==1?0:ENOTSUP;
}
#if NVN_NATIVE_COLOR_TARGET_PROBE
#include "color_target_probe.inc"
#else
int nn_pipeline_probe(const char *directory,char *json,size_t capacity)
{
 struct owner *o=calloc(1,sizeof(*o));if(!o)return ENOMEM;
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 const struct nn_pipeline_pair_recipe *pair=NULL;
 unsigned submissions=0,finishes=0,files=0,jobs=0,replays=0,busy_checks=0,completed=0;
 uint64_t channel_checks=0,guard_checks=0,readonly_checks=0;unsigned max_error=0;
 unsigned bad_case=UINT32_MAX,bad_x=UINT32_MAX,bad_y=UINT32_MAX,bad_channel=UINT32_MAX,bad_actual=0,bad_expected=0;
 size_t bad_byte=SIZE_MAX;uint32_t chip=0,timestamp=0;int e=0,cleanup=0,unknown=0;const char *stage="contract";
 char cases[16000];size_t used=0;cases[0]=0;
 if(NN_PIPELINE_QUAL_PAIR_INDEX>=archive->pair_count){e=ENOTSUP;goto done;}pair=archive->pairs+NN_PIPELINE_QUAL_PAIR_INDEX;
 if((e=contract(archive,pair)))goto done;
 stage="open";if((e=nn_device_open(&o->device)))goto done;chip=nn_device_chip(o->device);
 if(chip!=0x06050002||nn_device_gmem_bytes(o->device)!=archive->platform->gmem_bytes){e=ENOTSUP;goto done;}
 stage="program";const struct nn_offline_graphics_stage *vs=archive->stages->stages+pair->vs_index,*fs=archive->stages->stages+pair->fs_index;
 if((e=nn_pipeline_program_create(o->device,archive->stages,vs,fs,vs->specialization,fs->specialization,&o->program,&o->retained_program)))goto done;
 struct nn_pipeline_stage_view stages[2];for(unsigned i=0;i<2;i++)if((e=nn_pipeline_program_stage(o->program,i,stages+i)))goto done;
 if(stages[0].c1||stages[1].c1){e=ENOTSUP;goto done;}
 stage="queue";if((e=nn_queue_create(o->device,&o->queue)))goto done;
 stage="inputs";for(unsigned i=0;i<INPUTS;i++)if((e=nn_bo_alloc(o->device,4096,o->input+i)))goto done;
 for(unsigned shape=0;shape<SHAPES;shape++)for(unsigned layout=0;layout<LAYOUTS;layout++)for(unsigned mode=0;mode<MODES;mode++){
  const struct nn_tex_native_view *view=NN_PIPELINE_QUAL_VIEWS[shape];unsigned test=(shape*LAYOUTS+layout)*MODES+mode;
  stage="target";if((e=nn_bo_alloc(o->device,4096+view->required_span_bytes+4096,&o->target)))goto done;
  memset(nn_bo_map(o->target),0xa5,nn_bo_size(o->target));
  struct nn_pipeline_draw_state state={.target={view,{o->target,4096,view->required_span_bytes}}};
  nn_render_state_defaults(&state.render);if((e=nn_pipeline_full_raster(view->width,view->height,0,1,&state.raster)))goto done;
  state.vertex.attribute_count=state.vertex.stream_count=32;
  unsigned ps=layout==2?1:layout==1?7:3,cs=layout==2?11:ps;
  state.vertex.streams[ps]=(struct nn_pipeline_stream){layout==0?24:layout==1?64:16,0};
  state.vertex.buffers[ps]=(struct nn_pipeline_buffer){o->input[1],256,6*(layout==0?24:layout==1?64:16)};
  state.vertex.attributes[0]=(struct nn_pipeline_attribute){NVN_FORMAT_RG32F,layout==0?0:layout==1?8:4,ps};
  if(layout==2){state.vertex.streams[cs]=(struct nn_pipeline_stream){12,0};state.vertex.buffers[cs]=(struct nn_pipeline_buffer){o->input[2],128,72};}
  state.vertex.attributes[1]=(struct nn_pipeline_attribute){layout==2?NVN_FORMAT_RGBA8:NVN_FORMAT_RGBA32F,layout==0?8:layout==1?32:4,cs};
  state.ubo[0][1]=(struct nn_pipeline_buffer){o->input[0],64,64};
  state.draw=(struct nn_gfx_draw){.mode=mode<2?NN_GFX_DRAW_DIRECT:mode==2?NN_GFX_DRAW_INDIRECT:NN_GFX_DRAW_INDIRECT_COUNT,.primitive=4,
    .indexed=mode==1||mode==3,.index_type=1,.count=3,.instances=1,.max_draws=mode==3?2:1,.stride=mode==3?20:16};
  if(state.draw.indexed)state.draw.index=(struct nn_gfx_buffer){o->input[3],64,12};
  if(mode>=2)state.draw.indirect=(struct nn_gfx_buffer){o->input[4],64,40};
  if(mode==3)state.draw.indirect_count=(struct nn_gfx_buffer){o->input[4],256,4};
  /* Encode with sentinel argument data. Actual count/index/VBO/UBO contents
   * are supplied only afterwards and read by hardware at execution. */
  for(unsigned i=0;i<INPUTS;i++)memset(nn_bo_map(o->input[i]),0xa5,nn_bo_size(o->input[i]));
  stage="encode";if((e=nn_pipeline_encode(o->device,o->program,pair->recipe,archive->platform,&state,&o->work,&o->retained_job)))goto done;jobs++;
  int z=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"shape\":%u,\"layout\":%u,\"mode\":%u,\"width\":%u,\"height\":%u,\"pitch\":%u,\"span\":%u,\"bo_bytes\":%zu,\"command_bytes\":%zu,\"resource_count\":%zu}",used?",":"",test,shape,layout,mode,view->width,view->height,view->pitch,view->required_span_bytes,nn_bo_size(o->target),o->work.command_bytes,o->work.resource_count);
  if(z<0||(size_t)z>=sizeof(cases)-used){e=EOVERFLOW;goto done;}used+=(size_t)z;
  for(unsigned repeat=0;repeat<REPEATS;repeat++){
   inputs(o,&state,layout,mode,repeat);snapshots_clear(o);
   for(unsigned i=0;i<INPUTS;i++){if((e=snapshot(o,o->input[i],nn_bo_size(o->input[i]))))goto done;if((e=nn_bo_cache(o->input[i],0,nn_bo_size(o->input[i]),0)))goto done;}
   for(unsigned i=0;i<2;i++)if((e=snapshot(o,stages[i].code,nn_bo_size(stages[i].code))))goto done;
   if((e=snapshot(o,o->work.job,o->work.command_bytes)))goto done;
   stage="submit";unknown=1;
   if((e=nn_queue_submit(o->queue,o->work.job,0,o->work.command_bytes,o->work.resources,o->work.resource_count,&timestamp))){goto done;}submissions++;
   if(nn_bo_idle(o->target)!=EBUSY||nn_bo_idle(o->work.job)!=EBUSY){e=EPROTO;goto done;}busy_checks+=2;
   for(size_t i=0;i<o->work.resource_count;i++){if(nn_bo_idle(o->work.resources[i])!=EBUSY){e=EPROTO;goto done;}busy_checks++;}
   stage="finish";if((e=nn_queue_finish(o->queue,3000)))goto done;unknown=0;finishes++;
   stage="oracle";const unsigned char *map=nn_bo_map(o->target);const unsigned char *active=map+4096;int bad=0;
   for(unsigned y=0;y<view->height&&!bad;y++)for(unsigned x=0;x<view->width&&!bad;x++){
    unsigned expected[4];pixel(view->width,view->height,mode,repeat,x,y,expected);
    for(unsigned c=0;c<4;c++){unsigned actual=active[(size_t)y*view->pitch+x*4+c],delta=actual>expected[c]?actual-expected[c]:expected[c]-actual;
     if(delta>max_error){max_error=delta;}if(delta>(c==3?0u:1u)){bad_case=test*2+repeat;bad_x=x;bad_y=y;bad_channel=c;bad_actual=actual;bad_expected=expected[c];bad=1;e=EIO;break;}channel_checks++;}
   }
   stage=bad?"oracle":"guards";
   for(size_t off=0;off<nn_bo_size(o->target)&&!bad;off++){
    if(off>=4096){size_t rel=off-4096;if(rel/view->pitch<view->height&&rel%view->pitch<(size_t)view->width*4)continue;}
    if(map[off]!=0xa5){bad_case=test*2+repeat;bad_byte=off;bad_actual=map[off];bad_expected=0xa5;bad=1;e=EIO;break;}guard_checks++;
   }
   stage=bad?stage:"readonly";
   for(size_t i=0;i<o->watch_count&&!bad;i++){const struct watch *w=o->watches+i;const unsigned char *p=nn_bo_map(w->bo);
    for(size_t j=0;j<w->count;j++){if(p[j]!=w->bytes[j]){bad_case=test*2+repeat;bad_byte=j;bad_actual=p[j];bad_expected=w->bytes[j];bad=1;e=EIO;break;}readonly_checks++;}}
   int saved=save(directory,test*2+repeat,o->target);if(saved&&!e)e=saved;else if(!saved)files++;
   if(e){goto done;}completed++;if(repeat)replays++;
  }
  snapshots_clear(o);stage="retire-job";if((e=nn_bo_destroy(o->work.job)))goto done;memset(&o->work,0,sizeof(o->work));
  if((e=nn_bo_destroy(o->target))){goto done;}o->target=NULL;
 }
 stage="complete";
done:
 if(unknown)cleanup=EBUSY;else cleanup=release(o);
 int retained=unknown||cleanup;
 if(retained){o->next=quarantine;quarantine=o;}else free(o);
 int passed=!e&&!cleanup&&completed==SHAPES*LAYOUTS*MODES*REPEATS;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"native_pipeline\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"jobs\":%u,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"completed_cases\":%u,\"replays\":%u,\"inflight_owner_checks\":%u,\"channel_checks\":%llu,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"maximum_channel_error\":%u,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,\"bad_byte\":%zu,\"bad_actual\":%u,\"bad_expected\":%u,\"target_prefix\":4096,\"native_ubo_binding\":1,\"original_maxwell_cb\":3,\"ubo_bytes\":64,\"same_job_replay\":true,\"cpu_image_conversion\":false,\"nvn_api\":false,\"game_modules_executed\":false,\"legacy_image_template\":false,\"cases\":[%s],\"passed\":%s}\n",
 (unsigned)getuid(),chip,stage,e,cleanup,retained?"true":"false",jobs,submissions,finishes,files,completed,replays,busy_checks,(unsigned long long)channel_checks,(unsigned long long)guard_checks,(unsigned long long)readonly_checks,max_error,bad_case,bad_x,bad_y,bad_channel,bad_byte,bad_actual,bad_expected,cases,passed?"true":"false");
 return passed&&n>0&&(size_t)n<capacity?0:e?e:cleanup?cleanup:EIO;
}

#endif

#endif /* !NN_PIPELINE_PROBE_HELPERS_ONLY */
