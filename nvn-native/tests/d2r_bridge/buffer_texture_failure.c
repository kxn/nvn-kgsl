/* SPDX-License-Identifier: MIT. Actual raw probe failures/owner retention, NO GPU. */
#define _POSIX_C_SOURCE 200809L
#define nn_queue_submit ram_submit
#define nn_queue_finish ram_finish
#define nn_bo_alloc ram_alloc
#include NN_MEMORY_BACKEND
#undef nn_queue_submit
#undef nn_queue_finish
#undef nn_bo_alloc
static unsigned submits,finishes,allocations,allocation_failure,mode;
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){*out=NULL;if(++allocations==allocation_failure)return ENOMEM;return ram_alloc(d,n,out);}
int nn_queue_submit(nn_queue*q,nn_bo*b,size_t o,size_t n,nn_bo*const*r,size_t count,uint32_t*t)
{if(!q||!b||!t||!n||o>nn_bo_size(b)||n>nn_bo_size(b)-o||(count&&!r))return EINVAL;submits++;if(mode==1)return EIO;*t=submits;for(size_t i=0;i<=count;i++){nn_bo*x=i==count?b:r[i];unsigned j=0;while(j<q->count&&q->pending[j]!=x)j++;if(j==q->count){assert(q->count<64);q->pending[q->count++]=x;x->busy++;}}return 0;}
int nn_queue_finish(nn_queue*q,uint32_t t){finishes++;if(mode==2)return EIO;return ram_finish(q,t);}
#include "../buffer_texture_probe.c"
static unsigned checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%u %s\n",__LINE__,#x);abort();}checks++;}while(0)
int main(void)
{
 char result[8192];
 for(unsigned test=0;test<4;test++){
  mode=test==0?1:test==1?2:0;submits=finishes=allocations=0;allocation_failure=test==3?5:0;
  CHECK(nn_public_copy_probe("/nonexistent-HOST-no-readback",result,sizeof(result))!=0);CHECK(strstr(result,"\"passed\":false"));CHECK(!strstr(result,"\"stage\":\"capture\""));
  if(test<2){CHECK(quarantine&&quarantine->d&&quarantine->a&&quarantine->q);CHECK(quarantine->d->bos>=9);CHECK(strstr(result,"\"resources_retained\":true"));CHECK(strstr(result,"\"cleanup_errno\":16"));
   if(test==0){CHECK(submits==1&&!finishes);CHECK(strstr(result,"\"stage\":\"prepare\""));}else{CHECK(submits==2&&finishes==1);CHECK(strstr(result,"\"stage\":\"finish\""));CHECK(nn_bo_destroy(quarantine->a->source)==EBUSY);}
   /* Drain only this HOST fake busy ledger; this is not unknown GPU recovery. */
   struct process*o=quarantine;quarantine=o->next;while(o->q->count)o->q->pending[--o->q->count]->busy--;CHECK(!release(o));free(o);
  }else{CHECK(!quarantine);CHECK(strstr(result,"\"resources_retained\":false"));CHECK(strstr(result,"\"cleanup_errno\":0"));if(test==2){CHECK(submits==2&&finishes==1);CHECK(strstr(result,"\"stage\":\"source-readonly\""));}else CHECK(!submits&&!finishes);}
 }
 allocation_failure=allocations=mode=0;
 /* Inspect every real CP preparation packet and full write span; do not
  * execute a command or emulate GPU effects in this HOST metadata test. */
 nn_device*dev=NULL;CHECK(!nn_device_open(&dev));
 for(unsigned n=0;n<NN_BUFFER_CASES;n++)for(unsigned r=0;r<2;r++){
  struct nn_buffer_texture_case c;CHECK(!nn_buffer_texture_case(n,&c));struct active a={0};CHECK(!nn_bo_alloc(dev,c.source_bytes,&a.source));CHECK(!prepare(dev,&a,&c,r));
  const uint32_t*w=nn_bo_map(a.prepare);size_t at=0,written=0,total=a.prepare_bytes/4;
  while(written<c.source_bytes/4){size_t packet=nvn_pm4_packet_words(w[at]);CHECK(packet>=4&&packet<=16384&&packet<=total-at);CHECK(((w[at]>>16)&127)==0x3d);uint64_t addr=w[at+1]|(uint64_t)w[at+2]<<32;CHECK(addr==nn_bo_address(a.source)+written*4);
   for(size_t k=0;k<packet-3;k++){uint32_t expected=0;for(unsigned b=0;b<4;b++)expected|=(uint32_t)nn_buffer_texture_byte(&c,r,(written+k)*4+b)<<(8*b);CHECK(w[at+3+k]==expected);}
   written+=packet-3;at+=packet;
  }
  CHECK(written==c.source_bytes/4&&total-at==6);CHECK(w[at]==nvn_pm4_packet7(0x12,0));CHECK(w[at+1]==nvn_pm4_packet7(0x46,1)&&w[at+2]==0x31);CHECK(w[at+3]==nvn_pm4_packet7(0x12,0)&&w[at+4]==nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0)&&w[at+5]==nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0));
  const unsigned char*source=nn_bo_map(a.source);int untouched=1;for(size_t k=0;k<c.source_bytes;k++)if(source[k])untouched=0;CHECK(untouched);CHECK(!nn_bo_destroy(a.prepare));CHECK(!nn_bo_destroy(a.source));
 }
 CHECK(!nn_device_close(dev));
 allocation_failure=allocations=mode=0;nn_device*d=NULL;nn_bo*b=NULL;CHECK(!nn_device_open(&d));CHECK(!nn_bo_alloc(d,4096,&b));memset(nn_bo_map(b),0xa5,4096);struct shot watch={0};size_t watches=0;CHECK(!take(&watch,&watches,1,b));watch.gpu_timestamp_offset=4092;memset((char*)nn_bo_map(b)+4092,0x73,4);uint64_t compared=0;CHECK(!check_shots(&watch,1,&compared,2));CHECK(compared==4096);((unsigned char*)nn_bo_map(b))[4091]^=1;CHECK(check_shots(&watch,1,&compared,2)==EIO);CHECK(metadata_failure.group==2&&metadata_failure.offset==4091);free_shots(&watch,&watches);CHECK(!nn_bo_destroy(b));CHECK(!nn_device_close(d));
 printf("{\"passed\":true,\"checks\":%u,\"cases\":4,\"gpu_execution\":false,\"no_op_GPU_rejected\":true,\"unknown_owners_retained\":true}\n",checks);return 0;
}
