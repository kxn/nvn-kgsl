/* SPDX-License-Identifier: MIT. Raw-native owner qualification, not NVN sampling. */
#define _POSIX_C_SOURCE 200809L
#include "public_copy_probe.h"
#include "volume_shadow_cases.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include NVN_VOLUME_SHADOW_HEADER
struct shadow_active {struct nn_volume_shadow owner;nn_bo *canonical;unsigned char *source,*aux[32];size_t aux_bytes[32];nn_bo*refs[33];size_t count;};
struct shadow_process {struct shadow_process*next;nn_device*d;nn_queue*q;struct shadow_active*a;};static struct shadow_process*retained;
static int retire(struct shadow_active*a)
{if(!a)return 0;int e=nn_volume_shadow_destroy(&a->owner);if(e)return e;if(a->canonical){e=nn_bo_destroy(a->canonical);if(e)return e;a->canonical=NULL;}free(a->source);for(unsigned i=0;i<32;i++)free(a->aux[i]);free(a);return 0;}
static int save(const char*directory,unsigned n,unsigned repeat,const struct shadow_active*a)
{char path[1200];int z=snprintf(path,sizeof(path),"%s/volume-shadow-%03u-r%u.bin",directory,n,repeat);if(z<0||(size_t)z>=sizeof(path))return ENAMETOOLONG;int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;FILE*f=fdopen(fd,"wb");if(!f){int e=errno;close(fd);return e;}int e=0;nn_bo*b[]={a->canonical,a->owner.storage};for(unsigned i=0;i<2;i++)if(fwrite(nn_bo_map(b[i]),1,nn_bo_size(b[i]),f)!=nn_bo_size(b[i]))e=EIO;if(fflush(f)||fsync(fd))e=EIO;if(fclose(f))e=EIO;return e;}
static int aux(struct shadow_active*a,const struct nn_cs_image*k)
{
 for(size_t i=2;i<a->count;i++){nn_bo*b=a->refs[i];size_t n=nn_bo_size(b);a->aux[i-2]=malloc(n);if(!a->aux[i-2])return ENOMEM;memcpy(a->aux[i-2],nn_bo_map(b),n);a->aux_bytes[i-2]=n;}
 for(unsigned j=0;j<a->owner.count;j++){
  nn_bo*b=a->owner.dispatch[j];size_t slot=0;while(slot<a->count&&a->refs[slot]!=b)slot++;if(slot<2||slot==a->count)return EINVAL;
  size_t offset=SIZE_MAX;
  for(size_t p=0;p<k->patch_count;p++)if(k->patches[p].kind==NN_CS_SEQUENCE){size_t w=k->patches[p].word;if(w+1>=k->command_words)return EINVAL;const uint32_t*words=nn_bo_map(b);uint64_t addr=words[w]|(uint64_t)words[w+1]<<32,base=nn_bo_address(b);if(addr<base||addr-base>nn_bo_size(b)-4||addr%4||addr-base<a->owner.dispatch_bytes[j])return EINVAL;if(offset!=SIZE_MAX&&offset!=addr-base)return EINVAL;offset=addr-base;}
  if(offset==SIZE_MAX)return EINVAL;
  uint32_t value=2;memcpy(a->aux[slot-2]+offset,&value,4);
 }
 return 0;
}
int nn_public_copy_probe(const char*directory,char*json,size_t capacity)
{
 const struct nn_cs_image*k=&NVN_COMPILED_IMAGE;struct shadow_process*o=NULL;int error=0,cleanup=0,pending=0;const char*stage="profile";unsigned submits=0,finishes=0,completed=0,replays=0,files=0,busy=0,seq=0,owners=0,bad_case=~0u,bad_repeat=~0u;uint64_t bytes=0,active=0,reads=0,readonly=0,guard_init=0;uint32_t chip=0;
#define TRY(x) do{if((error=(x)))goto done;}while(0)
 if(!directory||!json||!capacity)return -1;
 if(k->schema!=1||k->chip!=0x06050002){error=ENOTSUP;goto done;}o=calloc(1,sizeof(*o));if(!o){error=ENOMEM;goto done;}stage="open";TRY(nn_device_open(&o->d));chip=nn_device_chip(o->d);if(chip!=0x06050002||nn_device_page(o->d)!=4096){error=ENOTSUP;goto done;}TRY(nn_queue_create(o->d,&o->q));
 for(unsigned n=0;n<18;n++){
  bad_case=n;const struct nn_volume_shadow_case*c=nn_volume_shadow_cases+n;struct nn_tex_source_desc d;unsigned base,count;TRY(nn_volume_shadow_case_spec(n,&d,&base,&count));struct nn_tex_source_layout s;struct nn_volume_shadow_plan p;stage="plan";TRY(nn_tex_source_layout(&d,&s));TRY(nn_volume_shadow_plan(&s,base,count,&p));
  if(memcmp(&s,c->source,sizeof(s))||memcmp(&p.native,c->native,sizeof(p.native))){error=EINVAL;goto done;}
  struct shadow_active*a=calloc(1,sizeof(*a));if(!a){error=ENOMEM;goto done;}o->a=a;stage="create";TRY(nn_bo_alloc(o->d,c->canonical_bo_bytes,&a->canonical));if(nn_bo_size(a->canonical)!=c->canonical_bo_bytes){error=EINVAL;goto done;}
  struct nn_block_copy_range range={a->canonical,0,nn_bo_size(a->canonical),512};TRY(nn_volume_shadow_create(o->d,k,&s,&p,&range,&a->owner));owners++;if(nn_bo_size(a->owner.storage)!=c->native_bo_bytes){error=EINVAL;goto done;}
  TRY(nn_volume_shadow_resources(&a->owner,a->refs,33,&a->count));struct nn_volume_shadow_dispatch jobs[15];size_t nj;TRY(nn_volume_shadow_dispatches(&a->owner,jobs,15,&nj));if(nj!=s.levels){error=EINVAL;goto done;}
  a->source=malloc(nn_bo_size(a->canonical));if(!a->source){error=ENOMEM;goto done;}TRY(aux(a,k));
  for(unsigned repeat=0;repeat<2;repeat++){
   bad_repeat=repeat;stage="authored-raw-input-and-guard-fill";unsigned char*src=nn_bo_map(a->canonical);memset(src,0xa5,nn_bo_size(a->canonical));for(size_t i=0;i<s.payload_bytes;i++)src[512+i]=nn_volume_shadow_pattern(512+i,n,repeat,0);memcpy(a->source,src,nn_bo_size(a->canonical));memset(nn_bo_map(a->owner.storage),0xa5,nn_bo_size(a->owner.storage));guard_init+=nn_bo_size(a->owner.storage);
   stage="publish";for(size_t i=0;i<a->count;i++)TRY(nn_bo_cache(a->refs[i],0,nn_bo_size(a->refs[i]),0));
   stage="submit";for(size_t i=0;i<nj;i++){uint32_t ts=0;pending=1;TRY(nn_queue_submit(o->q,jobs[i].job,0,jobs[i].bytes,a->refs,a->count,&ts));submits++;}
   if(nn_bo_destroy(a->canonical)!=EBUSY||nn_bo_destroy(a->owner.storage)!=EBUSY){error=EINVAL;goto done;}busy+=2;
   stage="finish";TRY(nn_queue_finish(o->q,1000));pending=0;finishes++;
   stage="invalidate";for(size_t i=0;i<a->count;i++)TRY(nn_bo_cache(a->refs[i],0,nn_bo_size(a->refs[i]),1));stage="capture";TRY(save(directory,n,repeat,a));files++;
   stage="readonly";if(memcmp(nn_bo_map(a->canonical),a->source,nn_bo_size(a->canonical))){error=EIO;goto done;}reads+=nn_bo_size(a->canonical);
   for(size_t i=2;i<a->count;i++){if(memcmp(nn_bo_map(a->refs[i]),a->aux[i-2],a->aux_bytes[i-2])){error=EIO;goto done;}readonly+=a->aux_bytes[i-2];}
   /* Completion words are compared against2, separately from readonly bytes. */
   readonly-=4*s.levels;seq+=s.levels;
   bytes+=nn_bo_size(a->canonical)+nn_bo_size(a->owner.storage);for(unsigned l=0;l<s.levels;l++)active+=(uint64_t)s.level[l].row_bytes*s.level[l].blocks_y*s.level_depths[l];completed++;if(repeat)replays++;
  }
  stage="retire";cleanup=retire(a);if(cleanup)goto done;o->a=NULL;
 }
 stage="complete";
 done:
 if(pending)cleanup=EBUSY;else if(o&&!cleanup){if(o->a){cleanup=retire(o->a);if(!cleanup)o->a=NULL;}if(!cleanup&&o->q){cleanup=nn_queue_destroy(o->q);if(!cleanup)o->q=NULL;}if(!cleanup&&o->d){cleanup=nn_device_close(o->d);if(!cleanup)o->d=NULL;}}
 if(o){if(cleanup){o->next=retained;retained=o;}else free(o);}int pass=!error&&!cleanup&&owners==18&&completed==36&&submits==96&&finishes==36&&replays==18&&files==36&&busy==72&&seq==96;
 int z=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"volume_shadow\",\"uid\":%u,\"chip_id\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"cases\":18,\"formats\":6,\"completed\":%u,\"submissions\":%u,\"finishes\":%u,\"replays\":%u,\"files\":%u,\"owners\":%u,\"inflight_destroy_rejections\":%u,\"sequence_word_checks\":%u,\"data_byte_checks\":%llu,\"active_byte_checks\":%llu,\"canonical_readonly_byte_checks\":%llu,\"metadata_readonly_byte_checks\":%llu,\"native_cpu_guard_initialization_bytes\":%llu,\"bad_case\":%u,\"bad_repeat\":%u,\"nvn_api\":false,\"sampling\":false,\"game_shader\":false,\"cpu_image_conversion\":false,\"passed\":%s}\n",(unsigned)getuid(),chip,stage,error,cleanup,cleanup?"true":"false",completed,submits,finishes,replays,files,owners,busy,seq,(unsigned long long)bytes,(unsigned long long)active,(unsigned long long)reads,(unsigned long long)readonly,(unsigned long long)guard_init,pass?~0u:bad_case,pass?~0u:bad_repeat,pass?"true":"false");
#undef TRY
 return pass&&z>=0&&(size_t)z<capacity?0:-1;
}
