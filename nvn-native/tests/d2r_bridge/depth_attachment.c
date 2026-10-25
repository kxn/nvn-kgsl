/* HOST RAM ownership/fault injection only. No GPU execution or pixel storage. */
#include "nvn_native/depth_attachment.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,allocs,destroys,maps,caches,live;
static int alloc_error,destroy_error,alloc_owner,invalid_address;
struct nn_device{uint32_t chip;};
struct nn_bo{size_t bytes;uint64_t address;unsigned busy;};
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);abort();}}while(0)
uint32_t nn_device_chip(const nn_device*d){return d->chip;}
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){(void)d;allocs++;*out=NULL;if(alloc_error&&!alloc_owner)return alloc_error;nn_bo*b=calloc(1,sizeof(*b));if(!b)return ENOMEM;b->bytes=(n+4095)&~(size_t)4095;b->address=invalid_address?0:UINT64_C(0x100000000);*out=b;live++;return alloc_error;}
int nn_bo_destroy(nn_bo*b){destroys++;if(destroy_error)return destroy_error;if(b->busy)return EBUSY;free(b);live--;return 0;}
size_t nn_bo_size(const nn_bo*b){return b->bytes;}
uint64_t nn_bo_address(const nn_bo*b){return b->address;}
void*nn_bo_map(const nn_bo*b){(void)b;maps++;return NULL;}
int nn_bo_cache(nn_bo*b,size_t o,size_t n,int from){(void)b;(void)o;(void)n;(void)from;caches++;return EIO;}
static int zero(const void*p,size_t n){const unsigned char*q=p;while(n--)if(*q++)return 0;return 1;}
int main(void){
 struct nn_device dev={0x06050002};struct nn_depth_fdl_desc d={0};d.schema=1;d.chip=dev.chip;d.format=0x32;d.target=1;d.width=73;d.height=41;d.levels=3;d.layers=1;d.roles=3;
 struct nn_depth_attachment a={0};CHECK(nn_depth_attachment_create(NULL,&d,&a)==EINVAL);CHECK(!allocs);CHECK(nn_depth_attachment_create(&dev,NULL,&a)==EINVAL);CHECK(nn_depth_attachment_create(&dev,&d,NULL)==EINVAL);
 for(unsigned f=0x32;f<=0x34;f++)for(unsigned i=0;i<3;i++){
  d.format=f;d.base_level=i;CHECK(!nn_depth_attachment_create(&dev,&d,&a));CHECK(live==1&&a.ready&&a.storage&&a.view.desc.address==nn_bo_address(a.storage));CHECK(!nn_depth_attachment_validate(&a));
  unsigned before=allocs;CHECK(nn_depth_attachment_create(&dev,&d,&a)==EBUSY&&allocs==before);
  nn_bo*refs[2]={(nn_bo*)(uintptr_t)1,(nn_bo*)(uintptr_t)2};size_t count=9;CHECK(nn_depth_attachment_resources(&a,refs,0,&count)==E2BIG&&count==0&&refs[0]==(nn_bo*)(uintptr_t)1&&refs[1]==(nn_bo*)(uintptr_t)2);
  CHECK(!nn_depth_attachment_resources(&a,refs,2,&count)&&count==1&&refs[0]==a.storage&&refs[1]==(nn_bo*)(uintptr_t)2);
  uint32_t desc[16];CHECK(!nn_depth_attachment_descriptor(&a,desc)&&!memcmp(desc,a.view.descriptor,sizeof(desc)));
  struct nn_depth_attachment saved=a;a.view.level[i].pitch++;CHECK(nn_depth_attachment_validate(&a)==EINVAL);memset(desc,0xa5,sizeof(desc));CHECK(nn_depth_attachment_descriptor(&a,desc)==EINVAL&&zero(desc,sizeof(desc)));a=saved;
  /* Every returned native word/extent/reserved byte is immutable owner metadata. */
  for(size_t byte=sizeof(a.view.desc);byte<sizeof(a.view);byte++){((unsigned char*)&a.view)[byte]^=1;CHECK(nn_depth_attachment_validate(&a)==EINVAL);a=saved;}
  a.storage->busy=1;CHECK(nn_depth_attachment_destroy(&a)==EBUSY&&a.storage==saved.storage&&a.ready&&live==1);a.storage->busy=0;CHECK(!nn_depth_attachment_destroy(&a)&&zero(&a,sizeof(a))&&!live);
 }
 unsigned before=allocs;d.format=0x35;CHECK(nn_depth_attachment_create(&dev,&d,&a)==ENOTSUP&&allocs==before);d.format=0x32;dev.chip=0;CHECK(nn_depth_attachment_create(&dev,&d,&a)==EINVAL&&allocs==before);dev.chip=d.chip;d.address=4096;CHECK(nn_depth_attachment_create(&dev,&d,&a)==EINVAL&&allocs==before);d.address=0;
 alloc_error=ENOMEM;CHECK(nn_depth_attachment_create(&dev,&d,&a)==ENOMEM&&!a.storage&&!live);alloc_owner=1;CHECK(nn_depth_attachment_create(&dev,&d,&a)==ENOMEM&&a.storage&&!a.ready&&live==1);alloc_error=alloc_owner=0;CHECK(!nn_depth_attachment_destroy(&a)&&!live);
 for(unsigned n=0;n<2;n++){invalid_address=1;destroy_error=n?EBUSY:EIO;before=destroys;CHECK(nn_depth_attachment_create(&dev,&d,&a)==EFAULT&&a.storage&&!a.ready&&live==1&&destroys==before+1);CHECK(nn_depth_attachment_destroy(&a)==destroy_error&&live==1);destroy_error=invalid_address=0;CHECK(!nn_depth_attachment_destroy(&a)&&!live);}
 invalid_address=1;CHECK(nn_depth_attachment_create(&dev,&d,&a)==EFAULT&&!a.storage&&!live);invalid_address=0;CHECK(nn_depth_attachment_validate(&a)==EINVAL);CHECK(nn_depth_attachment_destroy(NULL)==EINVAL);CHECK(!nn_depth_attachment_destroy(&a));CHECK(nn_depth_attachment_descriptor(&a,NULL)==EINVAL);CHECK(nn_depth_attachment_resources(&a,NULL,0,NULL)==EINVAL);CHECK(!maps&&!caches&&!live);
 printf("{\"scope\":\"HOST ownership fault injection only\",\"checks\":%u,\"alloc_calls\":%u,\"destroy_calls\":%u,\"pixel_map_calls\":%u,\"cache_calls\":%u,\"live_owners\":%u,\"gpu_execution\":false}\n",checks,allocs,destroys,maps,caches,live);
 return 0;
}
