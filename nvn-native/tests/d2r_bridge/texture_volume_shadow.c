/* SPDX-License-Identifier: MIT. HOST no-execution BO backend, real FDL/encoder. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/texture_volume_shadow.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include NN_VOLUME_SHADOW_IMAGE
struct nn_device{unsigned live;};
struct nn_bo{nn_device*d;void*map;size_t bytes;unsigned busy,id;uint64_t address;};
static unsigned checks,allocs,frees,maps,caches,destroys,fail_alloc,fail_map,fail_cache,fail_destroy;
static unsigned plans,owners,retained,native_allocation,native_alias;static nn_bo *canonical,*native;
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
uint32_t nn_device_chip(const nn_device*d){(void)d;return 0x06050002;}
size_t nn_device_page(const nn_device*d){(void)d;return 4096;}
size_t nn_device_gmem_bytes(const nn_device*d){(void)d;return 1179648;}
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out)
{*out=NULL;allocs++;if(allocs==fail_alloc)return ENOMEM;nn_bo*b=calloc(1,sizeof(*b));CK(b);b->bytes=(n+4095)&~(size_t)4095;CK(!posix_memalign(&b->map,4096,b->bytes));memset(b->map,0xa5,b->bytes);b->d=d;b->id=allocs;d->live++;*out=b;if(allocs==native_allocation){native=b;if(native_alias)b->address=nn_bo_address(canonical);}return 0;}
int nn_bo_destroy(nn_bo*b)
{destroys++;if(fail_destroy)return EIO;if(b->busy)return EBUSY;CK(b!=canonical);if(b==native)native=NULL;b->d->live--;free(b->map);free(b);frees++;return 0;}
int nn_bo_idle(const nn_bo*b){return !b?EINVAL:b->busy?EBUSY:0;}
void *nn_bo_map(const nn_bo*b){maps++;CK(b!=canonical&&b!=native);return maps==fail_map?NULL:b->map;}
uint64_t nn_bo_address(const nn_bo*b){return b->address?b->address:(uintptr_t)b->map;}
size_t nn_bo_size(const nn_bo*b){return b->bytes;}
int nn_bo_cache(nn_bo*b,size_t o,size_t n,int from)
{(void)from;caches++;CK(b!=canonical&&b!=native);if(caches==fail_cache)return EIO;return o<=b->bytes&&n<=b->bytes-o?0:EINVAL;}
static void reset(void){fail_alloc=fail_map=fail_cache=fail_destroy=0;}
static struct nn_tex_source_layout source_flags(unsigned f,unsigned w,unsigned h,unsigned z,unsigned levels,unsigned flags)
{struct nn_tex_source_desc d={.flags=flags,.target=2,.format=f,.width=w,.height=h,.layers=1,.levels=levels,.block_height_log2=-1,.depth=z};struct nn_tex_source_layout s;CK(!nn_tex_source_layout(&d,&s));return s;}
static struct nn_tex_source_layout source(unsigned f,unsigned w,unsigned h,unsigned z,unsigned levels)
{return source_flags(f,w,h,z,levels,0);}
static void free_source(void)
{nn_bo*b=canonical;canonical=NULL;CK(!nn_bo_destroy(b));}
static void verify(nn_device*d,const struct nn_tex_source_layout*s,const struct nn_volume_shadow_plan*p)
{
 struct nn_volume_shadow o={0};CK(!nn_bo_alloc(d,s->payload_bytes+128,&canonical));unsigned before=d->live;
 struct nn_block_copy_range c={canonical,64,(size_t)s->payload_bytes+4,68};native_allocation=allocs+1;CK(!nn_volume_shadow_create(d,&NVN_COMPILED_IMAGE,s,p,&c,&o));owners++;native=o.storage;
 CK(o.ready&&o.count==s->levels&&d->live==before+2+2*s->levels);CK(o.storage!=canonical&&o.storage!=o.code);
 for(size_t i=0;i<canonical->bytes;i++)CK(((unsigned char*)canonical->map)[i]==0xa5);
 for(size_t i=0;i<native->bytes;i++)CK(((unsigned char*)native->map)[i]==0xa5);
 struct nn_fdl_volume_desc desc=p->descriptor;desc.address=nn_bo_address(native);struct nn_fdl_volume_view v;
 CK(!nn_fdl_volume(&desc,&v)&&!memcmp(&v,&o.view,sizeof(v)));uint32_t descriptor[16];CK(!nn_volume_shadow_descriptor(&o,descriptor)&&!memcmp(descriptor,v.descriptor,64));
 nn_bo *refs[33],*poison=(void*)1;refs[0]=poison;size_t count=99;
 CK(nn_volume_shadow_resources(&o,refs,2,&count)==ENOSPC&&count==0&&refs[0]==poison);CK(!nn_volume_shadow_resources(&o,refs,33,&count)&&count==3+2*s->levels);
 CK(refs[0]==canonical&&refs[1]==native&&refs[2]==o.code);
 struct nn_volume_shadow_dispatch jobs[15]={{(void*)1,123}};size_t nj=99;
 CK(nn_volume_shadow_dispatches(&o,jobs,s->levels-1,&nj)==ENOSPC&&nj==0&&jobs[0].job==(void*)1&&jobs[0].bytes==123);
 CK(!nn_volume_shadow_dispatches(&o,jobs,15,&nj)&&nj==s->levels);
 for(unsigned i=0;i<s->levels;i++){
  CK(refs[3+2*i]==o.uniform[i]&&refs[4+2*i]==o.dispatch[i]);CK(jobs[i].job==o.dispatch[i]&&jobs[i].bytes==o.dispatch_bytes[i]);
  CK(!memcmp(o.uniform[i]->map,&p->copy[i].args,64));CK(o.dispatch_bytes[i]&&o.dispatch_bytes[i]%4==0&&o.dispatch_bytes[i]<=o.dispatch[i]->bytes);
  uint32_t expected[16];CK(!nn_cs_buffer_descriptor(&NVN_COMPILED_IMAGE,expected,nn_bo_address(canonical)+68,s->payload_bytes));
  CK(!memcmp((uint32_t*)o.dispatch[i]->map+1024,expected,64));
  CK(!nn_cs_buffer_descriptor(&NVN_COMPILED_IMAGE,expected,nn_bo_address(native)+p->native.level[i].offset,p->copy[i].linear_bytes));
  CK(!memcmp((uint32_t*)o.dispatch[i]->map+1024+16,expected,64));
 }
 CK(nn_volume_shadow_create(d,&NVN_COMPILED_IMAGE,s,p,&c,&o)==EBUSY);
 struct nn_fdl_volume_view saved=o.view;o.view.descriptor[0]^=1;CK(nn_volume_shadow_descriptor(&o,descriptor)==EINVAL);for(unsigned i=0;i<16;i++)CK(!descriptor[i]);o.view=saved;
 size_t bytes=o.dispatch_bytes[0];o.dispatch_bytes[0]=o.dispatch[0]->bytes+4;CK(nn_volume_shadow_resources(&o,refs,33,&count)==EINVAL&&count==0);o.dispatch_bytes[0]=bytes;
 unsigned oldf=frees;
 CK(!nn_volume_shadow_resources(&o,refs,33,&count));for(size_t i=0;i<count;i++)refs[i]->busy++;
 CK(nn_volume_shadow_destroy(&o)==EBUSY&&!o.ready&&o.storage&&frees==oldf);
 for(size_t i=0;i<count;i++)refs[i]->busy--;
 native=NULL;CK(!nn_volume_shadow_destroy(&o)&&d->live==before&&!o.canonical);free_source();CK(!d->live);
}
int main(void)
{
 nn_device d={0};const unsigned dims[][3]={{1,1,1},{35,19,9},{32,32,32},{127,73,17},{1,3,17},{197,113,37}};
 const unsigned fs[]={37,0x41,0x42,0x43,0x44,0x29};
 for(unsigned f=0;f<6;f++)for(unsigned dim=0;dim<6;dim++){
  unsigned levels=dim?3:1;struct nn_tex_source_layout s=source(fs[f],dims[dim][0],dims[dim][1],dims[dim][2],levels);struct nn_volume_shadow_plan p;
  for(unsigned base=0;base<levels;base++){
   CK(!nn_volume_shadow_plan(&s,base,levels-base,&p));plans++;CK(p.count==levels&&p.native.depth==s.volume_depth);
   if(fs[f]==0x29){CK(s.block_w==1&&s.block_h==1&&s.bytes_per_block==8);CK(p.native.bytes_per_block==8&&p.native.block_width==1&&p.native.block_height==1);}
   for(unsigned i=0;i<levels;i++){
    CK(p.copy[i].region.level==i&&!p.copy[i].args.direction&&p.copy[i].args.base_z==0);
    CK(p.copy[i].args.slices==s.level_depths[i]&&p.copy[i].args.linear_row_stride==p.native.level[i].pitch);
    CK(p.copy[i].args.linear_image_stride==p.native.level[i].slice_stride&&p.native.level[i].offset+p.copy[i].linear_bytes<=p.native.required_span);
   }
  }
  if(dim<3)verify(&d,&s,&p);
 }
 /* IMAGE is a source storage permission. This owner still emits sampled-only
  * descriptors and does not grant storage/image accesses. No CPU codec. */
 for(unsigned levels=1;levels<=3;levels+=2){
  struct nn_tex_source_layout a=source(0x29,32,32,32,levels),b=source_flags(0x29,32,32,32,levels,4);
  struct nn_volume_shadow_plan plain,image;CK(!nn_volume_shadow_plan(&a,0,levels,&plain));
  CK(!nn_volume_shadow_plan(&b,0,levels,&image));plans++;
  CK(b.flags==4&&image.descriptor.format==0x29&&image.descriptor.usage==NN_FDL_SAMPLED);
  CK(a.payload_bytes==b.payload_bytes&&!memcmp(&plain,&image,sizeof(image)));
  struct nn_tex_source_layout broken=b;broken.bytes_per_block=4;struct nn_volume_shadow_plan rejected;
  CK(nn_volume_shadow_plan(&broken,0,levels,&rejected)!=0&&!rejected.count);
  verify(&d,&b,&image);
 }
 struct nn_tex_source_layout s=source(0x42,35,19,9,3);struct nn_volume_shadow_plan p;CK(!nn_volume_shadow_plan(&s,0,3,&p));CK(!nn_bo_alloc(&d,s.payload_bytes+128,&canonical));
 struct nn_block_copy_range c={canonical,64,(size_t)s.payload_bytes+4,68};unsigned before=d.live;
 for(unsigned kind=0;kind<3;kind++)for(unsigned at=1;at<=(kind==0?8:kind==1?7:7);at++){
  struct nn_volume_shadow o={0};reset();native_allocation=allocs+1;if(kind==0)fail_alloc=allocs+at;else if(kind==1)fail_map=maps+at;else fail_cache=caches+at;
  CK(nn_volume_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)!=0);reset();CK(!nn_volume_shadow_destroy(&o)&&d.live==before);
 }
 for(unsigned n=0;n<3;n++){
  struct nn_volume_shadow o={0};reset();native_allocation=allocs+1;fail_cache=caches+2;fail_destroy=1;
  CK(nn_volume_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EIO&&o.storage&&!o.ready);retained++;unsigned old=frees;
  if(n==1){reset();o.code->busy=1;CK(nn_volume_shadow_destroy(&o)==EBUSY&&frees==old);o.code->busy=0;}
  else CK(nn_volume_shadow_destroy(&o)==EIO&&o.storage&&frees==old);
  reset();CK(!nn_volume_shadow_destroy(&o)&&d.live==before);
 }
 struct nn_volume_shadow o={0};unsigned old=allocs;struct nn_volume_shadow_plan bad=p;bad.native.min_layer_size^=1;
 CK(nn_volume_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&bad,&c,&o)==EINVAL&&allocs==old);
 bad=p;bad.copy[0].args.direction=1;CK(nn_volume_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&bad,&c,&o)==EINVAL&&allocs==old);
 struct nn_block_copy_range shortc=c;shortc.owner_bytes-=5;CK(nn_volume_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&shortc,&o)==EINVAL&&allocs==old);
 shortc=c;shortc.owner_offset=65;CK(nn_volume_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&shortc,&o)==EINVAL&&allocs==old);
 canonical->address=UINT64_MAX-63;CK(nn_volume_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EINVAL&&allocs==old);canonical->address=0;
 struct nn_cs_image wrong=NVN_COMPILED_IMAGE;wrong.patch_count=0;CK(nn_volume_shadow_create(&d,&wrong,&s,&p,&c,&o)==ENOTSUP&&allocs==old);
 native_allocation=allocs+1;native_alias=1;CK(nn_volume_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EINVAL&&d.live==before&&!o.storage);native_alias=0;
 free_source();CK(!d.live);
 struct nn_tex_source_layout unsupported=source(0x2e,8,8,8,1);CK(nn_volume_shadow_plan(&unsupported,0,1,&p)==ENOTSUP&&!p.count);
 CK(nn_volume_shadow_plan(&s,3,1,&p)==EINVAL&&!p.count);
 printf("{\"passed\":true,\"gpu_execution\":false,\"checks\":%u,\"plans\":%u,\"owners\":%u,\"retained_fault_owners\":%u,\"live\":%u,\"CPU_pixel_maps\":0,\"actual_GPU_submits\":0}\n",checks,plans,owners,retained,d.live);return 0;
}
