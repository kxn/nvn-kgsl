/* SPDX-License-Identifier: MIT. Authored native BUFFER TXF/TXS, no NVN/game shader claim. */
#define _POSIX_C_SOURCE 200809L
#include "public_copy_probe.h"
#include "buffer_texture_cases.h"
#include "nvn_native/pipeline.h"
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/pm4.h"
#include "nvn_native/order.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_BUFFER_TEXTURE_HEADER
struct shot {nn_bo*bo;unsigned char*bytes;size_t count,gpu_timestamp_offset;};
static struct {unsigned group;size_t slot,offset;unsigned expected,actual;} metadata_failure;
struct active {struct shot shots[128];size_t shot_count;nn_bo *source,*target,*vertex,*uniform,*prepare,*retained;size_t prepare_bytes;struct nn_pipeline_submission draw;};
struct process {struct process*next;nn_device*d;nn_queue*q;nn_pipeline_program*p[2],*retained;struct active*a;};static struct process*quarantine;
static void free_shots(struct shot*s,size_t*n){while(*n){(*n)--;free(s[*n].bytes);memset(s+*n,0,sizeof(*s));}}
static int take(struct shot*s,size_t*n,size_t capacity,nn_bo*b){for(size_t i=0;i<*n;i++)if(s[i].bo==b)return 0;if(*n==capacity)return EOVERFLOW;struct shot*v=s+*n;v->count=nn_bo_size(b);v->bytes=malloc(v->count);if(!v->bytes)return ENOMEM;v->bo=b;v->gpu_timestamp_offset=SIZE_MAX;memcpy(v->bytes,nn_bo_map(b),v->count);(*n)++;return 0;}
static int check_shots(struct shot*s,size_t n,uint64_t*count,unsigned group){for(size_t i=0;i<n;i++){int e=nn_bo_cache(s[i].bo,0,s[i].count,1);if(e)return e;const unsigned char*actual=nn_bo_map(s[i].bo);for(size_t at=0;at<s[i].count;at++)if(!(at>=s[i].gpu_timestamp_offset&&at-s[i].gpu_timestamp_offset<4)&&s[i].bytes[at]!=actual[at]){metadata_failure.group=group;metadata_failure.slot=i;metadata_failure.offset=at;metadata_failure.expected=s[i].bytes[at];metadata_failure.actual=actual[at];return EIO;}*count+=s[i].count;}return 0;}
static int clear_active(struct active*a)
{nn_bo**v[]={&a->draw.job,&a->retained,&a->prepare,&a->source,&a->target,&a->vertex,&a->uniform};for(unsigned i=0;i<7;i++)if(*v[i]){int e=nn_bo_destroy(*v[i]);if(e)return e;*v[i]=NULL;}free_shots(a->shots,&a->shot_count);free(a);return 0;}
static int release(struct process*o)
{int e;if(o->a){if((e=clear_active(o->a)))return e;o->a=NULL;}if(o->q){if((e=nn_queue_destroy(o->q)))return e;o->q=NULL;}for(unsigned i=0;i<2;i++)if(o->p[i]){if((e=nn_pipeline_program_destroy(o->p[i])))return e;o->p[i]=NULL;}if(o->retained){if((e=nn_pipeline_program_destroy(o->retained)))return e;o->retained=NULL;}if(o->d){if((e=nn_device_close(o->d)))return e;o->d=NULL;}return 0;}
static int prepare(nn_device*d,struct active*a,const struct nn_buffer_texture_case*c,unsigned repeat)
{
 size_t words=nn_bo_size(a->source)/4,packets=(words+16380)/16381,capacity=words+packets*3+6;
 int e=nn_bo_alloc(d,capacity*4,&a->prepare);if(e)return e;uint32_t*out=nn_bo_map(a->prepare);if(!out)return ENOTSUP;memset(out,0,nn_bo_size(a->prepare));size_t done=0,at=0;
 while(done<words){size_t count=words-done;if(count>16381)count=16381;uint64_t addr=nn_bo_address(a->source)+done*4;out[at++]=nvn_pm4_packet7(0x3d,(uint32_t)count+2);out[at++]=(uint32_t)addr;out[at++]=(uint32_t)(addr>>32);
  for(size_t i=0;i<count;i++){uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)nn_buffer_texture_byte(c,repeat,(done+i)*4+b)<<(b*8);out[at++]=word;}done+=count;
 }
 out[at++]=nvn_pm4_packet7(0x12,0); /* WAIT_MEM_WRITES before texture invalidation. */
 size_t tail=0;e=nn_order_words(0x06050002,0,NN_ORDER_INVALIDATE,out+at,capacity-at,&tail);if(e)return e;at+=tail;a->prepare_bytes=at*4;return nn_bo_cache(a->prepare,0,nn_bo_size(a->prepare),0);
}
static int capture(const char*dir,unsigned id,unsigned repeat,unsigned mode,const struct active*a)
{char path[1200];int n=snprintf(path,sizeof(path),"%s/buffer-texture-%03u-r%u-m%u.bin",dir,id,repeat,mode);if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return errno;FILE*f=fdopen(fd,"wb");if(!f){int e=errno;close(fd);return e;}nn_bo*b[]={a->source,a->target};int e=0;for(unsigned i=0;i<2;i++)if(fwrite(nn_bo_map(b[i]),1,nn_bo_size(b[i]),f)!=nn_bo_size(b[i]))e=EIO;if(fflush(f)||fsync(fd))e=EIO;if(fclose(f))e=EIO;return e;}
int nn_public_copy_probe(const char*directory,char*json,size_t capacity)
{
 if(!directory||!json||capacity<4096)return -1;
 metadata_failure.group=0;metadata_failure.slot=metadata_failure.offset=SIZE_MAX;metadata_failure.expected=metadata_failure.actual=0;
 struct process*o=calloc(1,sizeof(*o));int error=o?0:ENOMEM,cleanup=0,pending=0;const char*stage="profile";unsigned prepares=0,draws=0,finishes=0,files=0,busy=0,bad_case=~0u,bad_repeat=~0u,bad_mode=~0u,timestamps=0;uint64_t bytes=0,guard_fill=0,readonly=0,source_readonly=0;uint32_t chip=0;
 const struct nn_pipeline_native_artifact*artifacts[]={&buffer_float_artifact,&buffer_size_artifact};
#define TRY(x) do{if((error=(x)))goto done;}while(0)
 if(error)goto done;
 stage="device";TRY(nn_device_open(&o->d));chip=nn_device_chip(o->d);if(chip!=0x06050002||nn_device_page(o->d)!=4096){error=ENOTSUP;goto done;}TRY(nn_queue_create(o->d,&o->q));for(unsigned m=0;m<2;m++)TRY(nn_pipeline_program_create_native(o->d,artifacts[m],o->p+m,&o->retained));
 for(unsigned n=0;n<NN_BUFFER_CASES;n++){
  bad_case=n;struct nn_buffer_texture_case c;TRY(nn_buffer_texture_case(n,&c));if(memcmp(&c,nn_buffer_texture_cases+n,sizeof(c))){error=EINVAL;goto done;}
  struct active*a=calloc(1,sizeof(*a));if(!a){error=ENOMEM;goto done;}o->a=a;stage="owner";TRY(nn_bo_alloc(o->d,c.source_bytes,&a->source));TRY(nn_bo_alloc(o->d,4096,&a->vertex));TRY(nn_bo_alloc(o->d,4096,&a->uniform));
  const struct nn_tex_native_view*target=&nn_buffer_texture_target;TRY(nn_bo_alloc(o->d,target->required_span_bytes+4096,&a->target));
  for(unsigned r=0;r<2;r++){
   bad_repeat=r;stage="prepare";TRY(prepare(o->d,a,&c,r));uint32_t timestamp=0;nn_bo*prep_refs[]={a->source};pending=1;TRY(nn_queue_submit(o->q,a->prepare,0,a->prepare_bytes,prep_refs,1,&timestamp));prepares++;
   for(unsigned mode=0;mode<2;mode++){
    if(!mode&&!c.float_result)continue;
    bad_mode=mode;stage="draw-prepare";memset(nn_bo_map(a->target),0xa5,nn_bo_size(a->target));guard_fill+=nn_bo_size(a->target);memset(nn_bo_map(a->vertex),0xa5,nn_bo_size(a->vertex));memset(nn_bo_map(a->uniform),0xa5,nn_bo_size(a->uniform));
    const float eps=1.f/1024.f;float start=r?(float)(c.elements-1)+.25f:c.elements>=32767?.25f:eps,end=r||c.elements>=32767?start:2.f*c.elements+eps;const float v[3][8]={{-1,-1,0,1,start,0,0,1},{3,-1,0,1,end,0,0,1},{-1,3,0,1,start,0,0,1}};memcpy(nn_bo_map(a->vertex),v,sizeof(v));float u[16]={.5f,.5f,.5f,.5f,.5f,.5f,.5f,.5f};memcpy(nn_bo_map(a->uniform),u,64);TRY(nn_bo_cache(a->vertex,0,nn_bo_size(a->vertex),0));TRY(nn_bo_cache(a->uniform,0,nn_bo_size(a->uniform),0));TRY(nn_bo_cache(a->target,0,nn_bo_size(a->target),0));
    struct nn_buffer_sampled_binding binding={1,c.format,a->source,0,nn_bo_size(a->source),c.source_offset,(size_t)c.elements*c.cpp};const struct nn_pipeline_native_artifact*artifact=artifacts[mode];struct nn_pipeline_draw_state state={.target={target,{a->target,0,target->required_span_bytes}},.texture_bound=2,.texture_types=artifact->textures};state.sampled_buffer[1]=&binding;TRY(nn_pipeline_full_raster(target->width,target->height,0,1,&state.raster));nn_render_state_defaults(&state.render);state.vertex.attribute_count=32;state.vertex.stream_count=32;state.vertex.streams[0]=(struct nn_pipeline_stream){32,0};state.vertex.buffers[0]=(struct nn_pipeline_buffer){a->vertex,0,96};state.vertex.attributes[0]=(struct nn_pipeline_attribute){0x2e,0,0};state.vertex.attributes[1]=(struct nn_pipeline_attribute){0x2e,16,0};state.draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};state.ubo[1][2]=(struct nn_pipeline_buffer){a->uniform,0,64};TRY(nn_pipeline_encode(o->d,o->p[mode],artifact->recipe,&nn_pipeline_device,&state,&a->draw,&a->retained));
    free_shots(a->shots,&a->shot_count);for(size_t i=0;i<a->draw.resource_count;i++){nn_bo*b=a->draw.resources[i];if(b!=a->source&&b!=a->target&&b!=a->draw.job)TRY(take(a->shots,&a->shot_count,128,b));}TRY(take(a->shots,&a->shot_count,128,a->prepare));TRY(take(a->shots,&a->shot_count,128,a->draw.job));
    struct shot*job=a->shots+a->shot_count-1;const struct nn_pipeline_platform_segment*tail=&nn_pipeline_device.end;size_t start_word=a->draw.command_bytes/4-tail->word_count;const uint32_t*commands=nn_bo_map(a->draw.job);uint64_t seq=0;
    for(size_t i=0;i<tail->patch_count;i++)if(tail->patches[i].kind==NN_PLATFORM_SEQUENCE){size_t word=tail->patches[i].word;uint64_t addr=commands[start_word+word]|(uint64_t)commands[start_word+word+1]<<32,base=nn_bo_address(a->draw.job);if(word<2||word+2>=tail->word_count||tail->words[word-2]!=nvn_pm4_packet7(0x46,4)||!(tail->words[word-1]&(1u<<30))||addr<base||addr-base<a->draw.command_bytes||addr-base>job->count-4||(seq&&seq!=addr)){error=EINVAL;goto done;}seq=addr;job->gpu_timestamp_offset=(size_t)(addr-base);}
    if(!seq){error=EINVAL;goto done;}
    stage="draw-submit";timestamp=0;pending=1;TRY(nn_queue_submit(o->q,a->draw.job,0,a->draw.command_bytes,a->draw.resources,a->draw.resource_count,&timestamp));draws++;if(nn_bo_destroy(a->source)!=EBUSY||nn_pipeline_program_destroy(o->p[mode])!=EBUSY){error=EINVAL;goto done;}busy+=2;stage="finish";TRY(nn_queue_finish(o->q,1000));pending=0;finishes++;stage="metadata-readonly";TRY(check_shots(a->shots,a->shot_count,&readonly,1));readonly-=4;timestamps++;stage="source-readonly";TRY(nn_bo_cache(a->source,0,nn_bo_size(a->source),1));const unsigned char*actual=nn_bo_map(a->source);for(size_t k=0;k<nn_bo_size(a->source);k++)if(actual[k]!=nn_buffer_texture_byte(&c,r,k)){error=EIO;goto done;}source_readonly+=nn_bo_size(a->source);stage="capture";TRY(nn_bo_cache(a->target,0,nn_bo_size(a->target),1));TRY(capture(directory,n,r,mode,a));files++;bytes+=nn_bo_size(a->source)+nn_bo_size(a->target);TRY(nn_bo_destroy(a->draw.job));memset(&a->draw,0,sizeof(a->draw));
   }
   TRY(nn_bo_destroy(a->prepare));a->prepare=NULL;a->prepare_bytes=0;
  }
  stage="retire";TRY(clear_active(a));o->a=NULL;
 }
 stage="complete";
 done:
 if(pending)cleanup=EBUSY;else if(o)cleanup=release(o);if(o){if(cleanup){o->next=quarantine;quarantine=o;}else free(o);}int pass=!error&&!cleanup&&prepares==NN_BUFFER_PREPARES&&draws==NN_BUFFER_DRAWS&&finishes==draws&&files==draws&&busy==2*draws;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"buffer_texture\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"cases\":43,\"formats\":40,\"prepares\":%u,\"draws\":%u,\"submissions\":%u,\"finishes\":%u,\"files\":%u,\"inflight_destroy_rejections\":%u,\"data_bytes\":%llu,\"cpu_guard_initialization_bytes\":%llu,\"metadata_readonly_byte_checks\":%llu,\"gpu_timestamp_write_words\":%u,\"source_readonly_byte_checks\":%llu,\"bad_case\":%u,\"bad_repeat\":%u,\"bad_mode\":%u,\"nvn_api\":false,\"sampling\":true,\"game_shader\":false,\"cpu_image_conversion\":false,\"cpu_source_writes\":0,\"integer_texel_fetch\":false,\"passed\":%s}\n",(unsigned)getuid(),chip,stage,error,cleanup,cleanup?"true":"false",prepares,draws,prepares+draws,finishes,files,busy,(unsigned long long)bytes,(unsigned long long)guard_fill,(unsigned long long)readonly,timestamps,(unsigned long long)source_readonly,pass?~0u:bad_case,pass?~0u:bad_repeat,pass?~0u:bad_mode,pass?"true":"false");
#undef TRY
 return pass&&n>=0&&(size_t)n<capacity?0:-1;
}
