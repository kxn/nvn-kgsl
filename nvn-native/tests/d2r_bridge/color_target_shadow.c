/* SPDX-License-Identifier: MIT. HOST no-execution BO backend, real FDL/encoder. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/color_target_shadow.h"
#include "nvn_native/pipeline_views.h"
#include "../color_target_cases.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include NN_COLOR_TARGET_IMAGE
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
static struct nn_tex_source_layout source(unsigned format,unsigned width,unsigned height)
{struct nn_tex_source_desc d={.flags=1,.target=1,.format=format,.width=width,.height=height,.levels=1,.layers=1,.block_height_log2=-1};struct nn_tex_source_layout s;CK(!nn_tex_source_layout(&d,&s));return s;}
static void free_source(void){nn_bo*b=canonical;canonical=NULL;CK(!nn_bo_destroy(b));}
static void verify(nn_device*d,unsigned format,unsigned w,unsigned h)
{
 struct nn_tex_source_layout s=source(format,w,h);struct nn_tex_native_view v;struct nn_color_target_info info;
 CK(!nn_color_target_create(0x06050002,format,w,h,0,&v,&info));struct nn_color_target_shadow_plan p;
 CK(!nn_color_target_shadow_plan(&s,&v,&p));plans++;CK(p.info.bytes_per_pixel==s.bytes_per_block&&p.native.format==format);
 CK(!nn_bo_alloc(d,(size_t)s.payload_bytes+128,&canonical));struct nn_block_copy_range c={canonical,64,(size_t)s.payload_bytes+4,68};
 struct nn_tex_shadow o={0};native_allocation=allocs+1;unsigned before=d->live;CK(!nn_color_target_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o));owners++;
 CK(d->live==before+5&&o.bos[NN_TEX_SHADOW_STORAGE]==native);
 for(size_t i=0;i<canonical->bytes;i++)CK(((unsigned char*)canonical->map)[i]==0xa5);
 for(size_t i=0;i<native->bytes;i++)CK(((unsigned char*)native->map)[i]==0xa5);
 for(unsigned dir=0;dir<2;dir++){
  CK(p.copy[dir].args.direction==dir&&p.copy[dir].args.row_bytes==w*info.bytes_per_pixel&&p.copy[dir].args.rows==h);
  CK(p.copy[dir].args.linear_row_stride==v.pitch&&p.copy[dir].args.linear_image_stride==v.layer_bytes);
  CK(!memcmp((char*)o.bos[NN_TEX_SHADOW_UNIFORM]->map+dir*64,&p.copy[dir].args,64));
  unsigned ix=dir?NN_TEX_SHADOW_POST:NN_TEX_SHADOW_PRE;size_t bytes=dir?o.post_bytes:o.pre_bytes;CK(bytes&&bytes%4==0&&bytes<=o.bos[ix]->bytes);
  uint32_t expected[16];nn_bo*ssbo=dir?native:canonical;uint64_t addr=nn_bo_address(ssbo)+(dir?0:68);size_t span=dir?p.copy[dir].linear_bytes:s.payload_bytes;
  CK(!nn_cs_buffer_descriptor(&NVN_COMPILED_IMAGE,expected,addr,span));CK(!memcmp((uint32_t*)o.bos[ix]->map+1024,expected,64));
 }
 CK(nn_color_target_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EBUSY);
 for(unsigned i=0;i<5;i++)o.bos[i]->busy++;
 unsigned old=frees;CK(nn_tex_shadow_destroy(&o)==EBUSY&&frees==old);
 for(unsigned i=0;i<5;i++)o.bos[i]->busy--;
 native=NULL;CK(!nn_tex_shadow_destroy(&o)&&d->live==before);free_source();CK(!d->live);
}
int main(void)
{
 nn_device d={0};const unsigned sizes[][2]={{1,1},{3,5},{73,41},{127,65}};
 struct nn_pipeline_views*cache=NULL;CK(!nn_pipeline_views_create(0x06050002,&cache));
 for(unsigned i=0;i<22;i++)for(unsigned shape=0;shape<4;shape++){
  unsigned f=nn_color_formats[i],w=sizes[shape][0],h=sizes[shape][1];const struct nn_tex_native_view*v=NULL,*again=NULL;
  CK(!nn_pipeline_views_get_format(cache,f,w,h,0,&v));CK(v->format==f&&v->width==w&&v->height==h);
  CK(!nn_pipeline_views_get_format(cache,f,w,h,0,&again)&&again==v);verify(&d,f,w,h);
 }
 CK(nn_pipeline_views_count(cache)==88);const struct nn_tex_native_view*legacy=NULL;
 CK(!nn_pipeline_views_get(cache,73,41,0,&legacy)&&legacy->format==37);CK(nn_pipeline_views_count(cache)==88);
 for(unsigned f=1;f<=12;f++){
  struct nn_tex_source_layout s=source(NVN_FORMAT_RGBA8,73,41);struct nn_tex_native_view v;CK(!nn_color_target_create(0x06050002,NVN_FORMAT_RGBA8,73,41,0,&v,NULL));
  struct nn_color_target_shadow_plan p;CK(!nn_color_target_shadow_plan(&s,&v,&p));CK(!nn_bo_alloc(&d,(size_t)s.payload_bytes+128,&canonical));struct nn_block_copy_range c={canonical,64,(size_t)s.payload_bytes+4,68};
  struct nn_tex_shadow o={0};unsigned before=d.live;reset();native_allocation=allocs+1;
  if(f<=5)fail_alloc=allocs+f;else if(f<=8)fail_map=maps+f-5;else fail_cache=caches+f-8;
  CK(nn_color_target_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)!=0);reset();native=NULL;CK(!nn_tex_shadow_destroy(&o)&&d.live==before);free_source();
 }
 struct nn_tex_source_layout s=source(NVN_FORMAT_R11G11B10F,73,41);struct nn_tex_native_view v;CK(!nn_color_target_create(0x06050002,s.format,73,41,0,&v,NULL));struct nn_color_target_shadow_plan p;CK(!nn_color_target_shadow_plan(&s,&v,&p));
 CK(!nn_bo_alloc(&d,(size_t)s.payload_bytes+128,&canonical));struct nn_block_copy_range c={canonical,64,(size_t)s.payload_bytes+4,68};unsigned before=d.live;struct nn_tex_shadow o={0};native_allocation=allocs+1;fail_cache=caches+1;fail_destroy=1;
 CK(nn_color_target_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EIO&&o.bos[0]);retained++;reset();native=NULL;CK(!nn_tex_shadow_destroy(&o)&&d.live==before);
 unsigned old=allocs;struct nn_color_target_shadow_plan bad=p;bad.copy[1].args.direction=0;CK(nn_color_target_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&bad,&c,&o)==EINVAL&&allocs==old);
 struct nn_block_copy_range shortc=c;shortc.owner_bytes-=5;CK(nn_color_target_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&shortc,&o)==EINVAL&&allocs==old);
 native_allocation=allocs+1;native_alias=1;CK(nn_color_target_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EINVAL&&d.live==before);native_alias=0;
 free_source();nn_pipeline_views_destroy(cache);CK(!d.live);
 printf("{\"passed\":true,\"checks\":%u,\"plans\":%u,\"owners\":%u,\"retained\":%u,\"live\":%u,\"CPU_pixel_maps\":0,\"GPU_execution\":false}\n",checks,plans,owners,retained,d.live);return 0;
}
