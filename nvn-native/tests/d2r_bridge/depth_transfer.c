/* SPDX-License-Identifier: MIT. Actual common owner+encoder, HOST RAM only. */
#include "nvn_native/depth_transfer.h"
#include "nvn_native/pm4.h"
#include NN_DEPTH_PLATFORM
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct nn_device {uint32_t chip;};
struct nn_bo {uint64_t address;size_t bytes;unsigned busy;void*map;};
static unsigned checks,allocs,destroys,maps,caches,live,jobs;
static int alloc_error,alloc_owner,destroy_error,map_error,cache_error;
static uint64_t next_address=UINT64_C(0x100000000);
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
uint32_t nn_device_chip(const nn_device*d){return d->chip;}
size_t nn_device_gmem_bytes(const nn_device*d){(void)d;return 1179648;}
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){(void)d;allocs++;*out=NULL;if(alloc_error&&!alloc_owner)return alloc_error;nn_bo*b=calloc(1,sizeof(*b));if(!b)return ENOMEM;b->bytes=(n+4095)&~(size_t)4095;b->map=calloc(1,b->bytes);if(!b->map){free(b);return ENOMEM;}b->address=next_address;next_address+=b->bytes+4096;*out=b;live++;return alloc_error;}
int nn_bo_destroy(nn_bo*b){destroys++;if(destroy_error)return destroy_error;if(b->busy)return EBUSY;free(b->map);free(b);live--;return 0;}
size_t nn_bo_size(const nn_bo*b){return b->bytes;}
uint64_t nn_bo_address(const nn_bo*b){return b->address;}
void*nn_bo_map(const nn_bo*b){maps++;return map_error?NULL:b->map;}
int nn_bo_cache(nn_bo*b,size_t offset,size_t n,int from){C(b&&offset<=b->bytes&&n<=b->bytes-offset&&!from);caches++;return cache_error;}
static void encoded(nn_bo*job,size_t bytes){
 C(job&&bytes&&bytes<=32768);const uint32_t*w=job->map;C(w[0]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,3));C(w[1]==(uint32_t)(job->address+40960)&&w[2]==(job->address+40960)>>32&&w[3]==0);C(w[4]==nvn_pm4_packet7(0x12,0));
 unsigned blits=0;for(size_t at=0;at<bytes/4;){size_t n=nvn_pm4_packet_words(w[at]);C(n&&n<=bytes/4-at);if(w[at]==nvn_pm4_packet7(0x2c,1)){C(w[at+1]==3);blits++;}at+=n;}C(blits==1);for(size_t i=bytes;i<job->bytes;i++)C(!((unsigned char*)job->map)[i]);jobs++;
}
int main(void){
 struct nn_device d={0x06050002};
 for(unsigned f=0x32;f<=0x34;f++)for(unsigned k=0;k<2;k++){
  struct nn_depth_fdl_desc desc={0};desc.schema=1;desc.chip=d.chip;desc.format=f;desc.target=k?4:1;desc.width=k?73:64;desc.height=k?41:64;desc.layers=k?3:1;desc.levels=k?3:1;desc.base_level=k?1:0;desc.base_layer=k?2:0;desc.roles=1;
  struct nn_depth_attachment a={0};struct nn_depth_readback r={0};C(!nn_depth_attachment_create(&d,&desc,&a));alloc_error=ENOMEM;C(nn_depth_readback_create(&d,&a,&r)==ENOMEM&&!r.storage&&live==1);alloc_owner=1;C(nn_depth_readback_create(&d,&a,&r)==ENOMEM&&r.storage&&!r.ready&&live==2);alloc_error=alloc_owner=0;C(!nn_depth_readback_destroy(&r)&&live==1);C(!nn_depth_readback_create(&d,&a,&r));C(live==2&&!maps&&!caches);C(!nn_depth_readback_validate(&a,&r));unsigned before=allocs;C(nn_depth_readback_create(&d,&a,&r)==EBUSY&&before==allocs);
  struct nn_depth_a2d_region region={0,0,a.view.view_width,a.view.view_height};
  for(unsigned op=0;op<3;op++)for(unsigned replay=0;replay<2;replay++){
   nn_bo*job=(nn_bo*)(uintptr_t)1;size_t bytes=9;C(!nn_depth_transfer_encode(&d,&a,op?&r:NULL,&nn_pipeline_device,op,&region,replay?.75f:.25f,&job,&bytes));encoded(job,bytes);nn_bo*refs[3]={(nn_bo*)(uintptr_t)1,(nn_bo*)(uintptr_t)2,(nn_bo*)(uintptr_t)3};size_t count=9;
   C(nn_depth_transfer_resources(&a,&r,op,job,refs,op?2:1,&count)==E2BIG&&count==0&&refs[0]==(nn_bo*)(uintptr_t)1);C(!nn_depth_transfer_resources(&a,&r,op,job,refs,3,&count)&&count==(op?3u:2u)&&refs[0]==a.storage&&refs[count-1]==job);
   job->busy=1;C(nn_bo_destroy(job)==EBUSY);job->busy=0;C(!nn_bo_destroy(job));
  }
  nn_bo *absent=NULL;size_t absent_bytes=9;unsigned before_absent=allocs;
  C(nn_depth_transfer_encode(&d,&a,NULL,&nn_pipeline_device,2,&region,0,&absent,&absent_bytes)==EINVAL&&!absent&&!absent_bytes&&before_absent==allocs);
  C(nn_depth_transfer_encode(&d,&a,&r,&nn_pipeline_device,3,&region,0,&absent,&absent_bytes)==EINVAL&&!absent&&!absent_bytes&&before_absent==allocs);
  struct nn_depth_readback saved_r=r;r.view.linear.pitch^=64;unsigned pre=allocs;nn_bo*rejected=NULL;size_t rejected_bytes=9;C(nn_depth_transfer_encode(&d,&a,&r,&nn_pipeline_device,1,&region,0,&rejected,&rejected_bytes)==EINVAL&&!rejected&&!rejected_bytes&&allocs==pre);r=saved_r;
  unsigned current=allocs;nn_bo*job=(nn_bo*)(uintptr_t)1;size_t bytes=9;struct nn_pipeline_platform bad=nn_pipeline_device;bad.gmem_bytes-=4096;C(nn_depth_transfer_encode(&d,&a,&r,&bad,1,&region,0,&job,&bytes)==ENOTSUP&&!job&&!bytes&&current==allocs);
  bad=nn_pipeline_device;bad.begin.patch_count=33;C(nn_depth_transfer_encode(&d,&a,&r,&bad,1,&region,0,&job,&bytes)==EINVAL&&current==allocs);
  uint64_t saved=r.storage->address;r.storage->address=a.storage->address;C(nn_depth_transfer_encode(&d,&a,&r,&nn_pipeline_device,1,&region,0,&job,&bytes)==EINVAL&&current==allocs);r.storage->address=saved;
  for(unsigned op=1;op<=2;op++)for(unsigned fault=0;fault<4;fault++){
   alloc_error=fault<2?ENOMEM:0;alloc_owner=fault==1;map_error=fault==2;cache_error=fault==3?EIO:0;destroy_error=fault>=2?EIO:0;
   int e=nn_depth_transfer_encode(&d,&a,&r,&nn_pipeline_device,op,&region,0,&job,&bytes);C(e==(fault<2?ENOMEM:fault==2?EFAULT:EIO)&&!bytes);C((job!=NULL)==(fault!=0));if(job)C(live==3);
   alloc_error=alloc_owner=map_error=cache_error=destroy_error=0;if(job)C(!nn_bo_destroy(job));C(live==2);
  }
  r.storage->busy=1;nn_bo*keep=r.storage;C(nn_depth_readback_destroy(&r)==EBUSY&&r.storage==keep&&r.ready);r.storage->busy=0;C(!nn_depth_readback_destroy(&r));C(!nn_depth_attachment_destroy(&a));C(!live);maps=caches=0;
 }
 C(!live);printf("{\"scope\":\"HOST RAM owners and PM4 only\",\"checks\":%u,\"encoded_jobs\":%u,\"alloc_calls\":%u,\"destroy_calls\":%u,\"live_owners\":%u,\"gpu_execution\":false}\n",checks,jobs,allocs,destroys,live);return 0;
}
