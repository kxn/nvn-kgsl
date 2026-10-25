/* SPDX-License-Identifier: MIT. Authored raw-block GPU qualifier, never NVN/game. */
#define _POSIX_C_SOURCE 200809L
#include "block_copy_probe.h"
#include "block_copy_probe_cases.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_BLOCK_COPY_HEADER
struct block_active {nn_bo *data[2],*ubo,*job;unsigned char *expected[2],*ubo_expected,*job_expected;size_t job_bytes,sequence;};
struct block_owner {struct block_owner *next;nn_device *device;nn_queue *queue;nn_bo *code;unsigned char *code_expected;struct block_active *active;};
static struct block_owner *retained;
static int equal(const void *a,const void *b,size_t n,size_t *bad)
{const unsigned char *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i]){*bad=i;return EIO;}return 0;}
static int job_reference(struct block_active *a,const struct nn_cs_image *image)
{
 size_t bytes=nn_bo_size(a->job);a->job_expected=malloc(bytes);if(!a->job_expected)return ENOMEM;
 memcpy(a->job_expected,nn_bo_map(a->job),bytes);a->sequence=SIZE_MAX;
 for(size_t i=0;i<image->patch_count;i++)if(image->patches[i].kind==NN_CS_SEQUENCE){
  size_t word=image->patches[i].word;if(word+1>=image->command_words)return EINVAL;
  const uint32_t *w=nn_bo_map(a->job);uint64_t address=w[word]|(uint64_t)w[word+1]<<32,base=nn_bo_address(a->job);
  if(address<base||address-base>bytes-4||address%4||address-base<a->job_bytes)return EINVAL;
  size_t offset=address-base;if(a->sequence!=SIZE_MAX&&a->sequence!=offset)return EINVAL;a->sequence=offset;
 }
 if(a->sequence==SIZE_MAX)return EINVAL;
 uint32_t final=2;memcpy(a->job_expected+a->sequence,&final,4);return 0;
}
static int destroy_active(struct block_active *a)
{
 if(!a)return 0;
 nn_bo **slots[]={&a->job,&a->ubo,&a->data[1],&a->data[0]};for(unsigned i=0;i<4;i++)if(*slots[i]){int e=nn_bo_destroy(*slots[i]);if(e)return e;*slots[i]=NULL;}
 free(a->expected[0]);free(a->expected[1]);free(a->ubo_expected);free(a->job_expected);free(a);return 0;
}
static int raw(const char *directory,unsigned n,unsigned repeat,const struct block_active *a)
{
 char path[1200];int z=snprintf(path,sizeof(path),"%s/block-%03u-r%u.bin",directory,n,repeat);if(z<0||(size_t)z>=sizeof(path))return ENAMETOOLONG;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;FILE *f=fdopen(fd,"wb");if(!f){int e=errno;close(fd);return e;}
 int e=0;for(unsigned i=0;i<2;i++)if(a->data[i]&&fwrite(nn_bo_map(a->data[i]),1,nn_bo_size(a->data[i]),f)!=nn_bo_size(a->data[i]))e=EIO;
 if(fflush(f)||fsync(fd))e=EIO;
 if(fclose(f))e=EIO;
 return e;
}
int nn_block_copy_probe(const char *directory,char *json,size_t capacity)
{
 const struct nn_cs_image *image=&NVN_COMPILED_IMAGE;struct block_owner *o=NULL;int error=0,cleanup=0,pending=0;const char *stage="profile",*submission="not_attempted";
 unsigned completed=0,submissions=0,finishes=0,replays=0,files=0,busy=0,sequence_checks=0,bad_case=~0u,bad_repeat=~0u,bad_bo=~0u;size_t bad_offset=SIZE_MAX;
 uint32_t chip=0;uint64_t data_checks=0,active_checks=0,unchanged_checks=0,source_checks=0,aux_checks=0;
#define TRY(x) do{if((error=(x)))goto done;}while(0)
 if(!directory||!json||!capacity)return -1;
 if(NN_BLOCK_PROBE_CASES!=148||image->schema!=1||image->chip!=0x06050002||image->local[0]!=8||image->local[1]!=8||image->local[2]!=1||image->ssbo_slots!=2){error=ENOTSUP;goto done;}
 o=calloc(1,sizeof(*o));if(!o){error=ENOMEM;goto done;}stage="open";TRY(nn_device_open(&o->device));chip=nn_device_chip(o->device);if(chip!=0x06050002||nn_device_page(o->device)!=4096){error=ENOTSUP;goto done;}TRY(nn_queue_create(o->device,&o->queue));
 TRY(nn_bo_alloc(o->device,image->code_bytes,&o->code));memset(nn_bo_map(o->code),0xa5,nn_bo_size(o->code));memcpy(nn_bo_map(o->code),image->code,image->code_bytes);
 o->code_expected=malloc(nn_bo_size(o->code));if(!o->code_expected){error=ENOMEM;goto done;}memcpy(o->code_expected,nn_bo_map(o->code),nn_bo_size(o->code));
 for(unsigned n=0;n<NN_BLOCK_PROBE_CASES;n++){
  bad_case=n;struct nn_tex_source_layout source;struct nn_block_copy_plan p;unsigned shared;stage="case";TRY(nn_block_probe_case(n,&source,&p,&shared));
  struct block_active *a=calloc(1,sizeof(*a));if(!a){error=ENOMEM;goto done;}o->active=a;stage="allocate";
  for(unsigned i=0;i<2;i++){size_t bytes=nn_block_probe_bo_bytes(&p,shared,i);if(!bytes)continue;TRY(nn_bo_alloc(o->device,bytes,a->data+i));if(nn_bo_size(a->data[i])!=bytes){error=EINVAL;goto done;}a->expected[i]=malloc(bytes);if(!a->expected[i]){error=ENOMEM;goto done;}}
  TRY(nn_bo_alloc(o->device,4096,&a->ubo));size_t linear_offset=nn_block_probe_linear_offset(&p,shared);nn_bo *linear=shared?a->data[0]:a->data[1];
  struct nn_block_copy_range canonical={a->data[0],0,nn_bo_size(a->data[0]),NN_BLOCK_PROBE_CANONICAL_OFFSET},l={linear,0,nn_bo_size(linear),linear_offset},u={a->ubo,0,nn_bo_size(a->ubo),0};struct nn_block_copy_binding b;
  stage="bind";
  /* Actual fresh live driver allocations establish independent backing; the
   * shared form is the SAME allocation with proved disjoint full spans. */
  TRY(nn_block_copy_bind(o->device,image,&source,&p,&canonical,&l,&u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));
  memset(nn_bo_map(a->ubo),0xa5,nn_bo_size(a->ubo));memcpy(nn_bo_map(a->ubo),&b.args,64);a->ubo_expected=malloc(nn_bo_size(a->ubo));if(!a->ubo_expected){error=ENOMEM;goto done;}memcpy(a->ubo_expected,nn_bo_map(a->ubo),nn_bo_size(a->ubo));
  stage="encode";TRY(nn_cs_encode(o->device,o->code,image,b.ubos,b.ssbos,b.groups,&a->job,&a->job_bytes));TRY(job_reference(a,image));
  for(unsigned repeat=0;repeat<NN_BLOCK_PROBE_REPEATS;repeat++){
   bad_repeat=repeat;stage="initialize-raw-pattern";
   for(unsigned i=0;i<2;i++)if(a->data[i])memset(nn_bo_map(a->data[i]),0xa5,nn_bo_size(a->data[i]));
   unsigned source_index=p.args.direction&&!shared?1:0;size_t source_offset=p.args.direction?linear_offset:NN_BLOCK_PROBE_CANONICAL_OFFSET,source_bytes=p.args.direction?p.linear_bytes:p.canonical_bytes;
   unsigned char *input=nn_bo_map(a->data[source_index]);for(size_t i=0;i<source_bytes;i++)input[source_offset+i]=nn_block_copy_pattern(source_offset+i,n,repeat,0);
   for(unsigned i=0;i<2;i++)if(a->data[i])memcpy(a->expected[i],nn_bo_map(a->data[i]),nn_bo_size(a->data[i]));
   /* Test-only closed-form expected values, never a CPU-converted GPU input.
    * The source readback is NOT used as the expected data oracle. */
   const struct nn_block_copy_region *r=&p.region;unsigned dest_index=p.args.direction?0:(shared?0:1);uint64_t active=(uint64_t)r->layers*r->rows*r->row_bytes;
   for(unsigned layer=0;layer<r->layers;layer++)for(unsigned y=0;y<r->rows;y++)for(unsigned x=0;x<r->row_bytes;x++){
    uint64_t c=NN_BLOCK_PROBE_CANONICAL_OFFSET+nn_block_copy_oracle_offset(&source,r->level,r->base_layer+layer,r->x_bytes+x,r->y_rows+y),lbyte=linear_offset+(uint64_t)layer*r->linear_image_stride+(uint64_t)y*r->linear_row_stride+x;
    uint64_t destination=p.args.direction?c:lbyte,from=p.args.direction?lbyte:c;if(destination>=nn_bo_size(a->data[dest_index])){error=EINVAL;goto done;}a->expected[dest_index][destination]=nn_block_copy_pattern(from,n,repeat,0);
   }
   stage="publish";TRY(nn_bo_cache(o->code,0,nn_bo_size(o->code),0));TRY(nn_bo_cache(a->ubo,0,nn_bo_size(a->ubo),0));TRY(nn_bo_cache(a->job,0,nn_bo_size(a->job),0));for(unsigned i=0;i<2;i++)if(a->data[i])TRY(nn_bo_cache(a->data[i],0,nn_bo_size(a->data[i]),0));
   nn_bo *refs[4];size_t count=0;refs[count++]=o->code;refs[count++]=a->data[0];if(a->data[1])refs[count++]=a->data[1];refs[count++]=a->ubo;uint32_t timestamp=0;
   stage="submit";submission="unknown";pending=1;TRY(nn_queue_submit(o->queue,a->job,0,a->job_bytes,refs,count,&timestamp));submission="accepted";submissions++;
   if(nn_bo_destroy(a->data[0])!=EBUSY){error=EINVAL;goto done;}busy++;
   stage="finish";TRY(nn_queue_finish(o->queue,1000));pending=0;finishes++;
   stage="invalidate";TRY(nn_bo_cache(o->code,0,nn_bo_size(o->code),1));TRY(nn_bo_cache(a->ubo,0,nn_bo_size(a->ubo),1));TRY(nn_bo_cache(a->job,0,nn_bo_size(a->job),1));for(unsigned i=0;i<2;i++)if(a->data[i])TRY(nn_bo_cache(a->data[i],0,nn_bo_size(a->data[i]),1));
   stage="save-actual-bytes";TRY(raw(directory,n,repeat,a));files++;
   stage="actual-all-bytes";TRY(equal(nn_bo_map(o->code),o->code_expected,nn_bo_size(o->code),&bad_offset));TRY(equal(nn_bo_map(a->ubo),a->ubo_expected,nn_bo_size(a->ubo),&bad_offset));TRY(equal(nn_bo_map(a->job),a->job_expected,nn_bo_size(a->job),&bad_offset));aux_checks+=nn_bo_size(o->code)+nn_bo_size(a->ubo)+nn_bo_size(a->job)-4;sequence_checks++;
   uint64_t bytes=0;for(unsigned i=0;i<2&&!error;i++)if(a->data[i]){bad_bo=i;error=equal(nn_bo_map(a->data[i]),a->expected[i],nn_bo_size(a->data[i]),&bad_offset);bytes+=nn_bo_size(a->data[i]);}
   if(error)goto done;
   data_checks+=bytes;active_checks+=active;unchanged_checks+=bytes-active;source_checks+=source_bytes;completed++;if(repeat)replays++;
  }
  stage="retire-case";cleanup=destroy_active(a);if(cleanup)goto done;o->active=NULL;
 }
 stage="complete";
done:
 if(pending)cleanup=EBUSY;
 else if(o&&!cleanup){if(o->active){cleanup=destroy_active(o->active);if(!cleanup)o->active=NULL;}if(!cleanup&&o->queue){cleanup=nn_queue_destroy(o->queue);if(!cleanup)o->queue=NULL;}if(!cleanup&&o->code){cleanup=nn_bo_destroy(o->code);if(!cleanup)o->code=NULL;}if(!cleanup&&o->device){cleanup=nn_device_close(o->device);if(!cleanup)o->device=NULL;}}
 if(o){if(cleanup){o->next=retained;retained=o;}else{free(o->code_expected);free(o);}}
 int passed=!error&&!cleanup&&completed==296&&submissions==296&&finishes==296&&replays==148&&files==296&&busy==296&&sequence_checks==296&&data_checks&&active_checks&&unchanged_checks&&source_checks&&aux_checks;
 int z=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"block_copy\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"submission\":\"%s\",\"cases\":148,\"formats\":37,\"completed\":%u,\"submissions\":%u,\"finishes\":%u,\"replays\":%u,\"files\":%u,\"inflight_destroy_rejections\":%u,\"sequence_word_checks\":%u,\"data_byte_checks\":%llu,\"active_byte_checks\":%llu,\"unchanged_data_byte_checks\":%llu,\"source_readonly_byte_checks\":%llu,\"aux_readonly_byte_checks\":%llu,\"bad_case\":%u,\"bad_repeat\":%u,\"bad_bo\":%u,\"bad_offset\":%zu,\"nvn_api\":false,\"game_shader\":false,\"cpu_image_conversion\":false,\"BC_decode\":false,\"passed\":%s}\n",(unsigned)getuid(),chip,stage,error,cleanup,cleanup?"true":"false",submission,completed,submissions,finishes,replays,files,busy,sequence_checks,(unsigned long long)data_checks,(unsigned long long)active_checks,(unsigned long long)unchanged_checks,(unsigned long long)source_checks,(unsigned long long)aux_checks,passed?~0u:bad_case,passed?~0u:bad_repeat,passed?~0u:bad_bo,passed?SIZE_MAX:bad_offset,passed?"true":"false");
#undef TRY
 return passed&&z>=0&&(size_t)z<capacity?0:-1;
}
