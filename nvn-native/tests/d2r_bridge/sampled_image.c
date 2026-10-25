/* SPDX-License-Identifier: MIT. HOST only: actual FDL/encoder, no GPU execution. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/sampled_image.h"
#include "nvn_native/pipeline_native_program.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nvn.h>
#include NN_SAMPLED_KERNEL
#include NN_SAMPLED_ARCHIVE
struct nn_device{unsigned live;};
struct nn_bo{nn_device*d;void*map;size_t bytes;unsigned busy;uint64_t address;};
static unsigned checks,allocs,frees,maps,caches,destroys,fail_alloc,fail_map,fail_cache,fail_destroy;
static unsigned plans,owners,retained,normal_programs,native_allocation,native_alias;
static nn_bo *canonical,*native;
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
uint32_t nn_device_chip(const nn_device*d){(void)d;return 0x06050002;}
size_t nn_device_page(const nn_device*d){(void)d;return 4096;}
size_t nn_device_gmem_bytes(const nn_device*d){(void)d;return 1179648;}
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out)
{*out=NULL;allocs++;if(allocs==fail_alloc)return ENOMEM;nn_bo*b=calloc(1,sizeof(*b));CK(b);b->bytes=(n+4095)&~(size_t)4095;CK(!posix_memalign(&b->map,4096,b->bytes));memset(b->map,0xa5,b->bytes);b->d=d;d->live++;*out=b;if(allocs==native_allocation){native=b;if(native_alias)b->address=nn_bo_address(canonical);}return 0;}
int nn_bo_destroy(nn_bo*b)
{destroys++;if(fail_destroy)return EIO;if(b->busy)return EBUSY;CK(b!=canonical);if(b==native)native=NULL;b->d->live--;free(b->map);free(b);frees++;return 0;}
int nn_bo_idle(const nn_bo*b){return !b?EINVAL:b->busy?EBUSY:0;}
void *nn_bo_map(const nn_bo*b){maps++;CK(b!=canonical&&b!=native);return maps==fail_map?NULL:b->map;}
uint64_t nn_bo_address(const nn_bo*b){return b->address?b->address:(uintptr_t)b->map;}
size_t nn_bo_size(const nn_bo*b){return b->bytes;}
int nn_bo_cache(nn_bo*b,size_t o,size_t n,int from)
{(void)from;caches++;CK(b!=canonical&&b!=native);if(caches==fail_cache)return EIO;return o<=b->bytes&&n<=b->bytes-o?0:EINVAL;}
static void reset(void){fail_alloc=fail_map=fail_cache=fail_destroy=0;}
static struct nn_tex_source_layout source(unsigned format,unsigned width,unsigned height,unsigned levels)
{struct nn_tex_source_desc d={.flags=0,.target=1,.format=format,.width=width,.height=height,.levels=levels,.layers=1,.block_height_log2=-1};struct nn_tex_source_layout s;int e=nn_tex_source_layout(&d,&s);if(e)fprintf(stderr,"source format%u %ux%u levels%u errno%d\n",format,width,height,levels,e);CK(!e);return s;}
static void free_source(void){nn_bo*b=canonical;canonical=NULL;CK(!nn_bo_destroy(b));}
static void untouched(nn_bo*b){for(size_t i=0;i<b->bytes;i++)CK(((unsigned char*)b->map)[i]==0xa5);}
static void verify(nn_device*d,unsigned format,unsigned w,unsigned h,unsigned levels,unsigned base)
{
 struct nn_tex_source_layout s=source(format,w,h,levels);struct nn_sampled_image_plan p;
 CK(!nn_sampled_image_plan(&s,base,levels-base,&p));plans++;
 CK(p.count==levels&&p.format.result_type==NN_FDL_RESULT_FLOAT&&p.native.base_level==base&&p.native.level_count==levels-base);
 struct nn_fdl_image_view bad=p.native;bad.descriptor[0]^=1;CK(nn_sampled_image_validate(&bad,NULL)==EINVAL);
 bad=p.native;bad.level[0].pitch+=64;CK(nn_sampled_image_validate(&bad,NULL)==EINVAL);
 CK(!nn_bo_alloc(d,(size_t)s.payload_bytes+128,&canonical));struct nn_block_copy_range c={canonical,64,(size_t)s.payload_bytes+4,68};
 struct nn_sampled_image_shadow *o=NULL;native_allocation=allocs+1;unsigned before=d->live;
 CK(!nn_sampled_image_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)&&o);owners++;
 CK(d->live==before+2+2*levels);untouched(canonical);untouched(native);
 size_t count=0;nn_bo*resources[3+2*NN_FDL_LEVELS];for(unsigned i=0;i<3+2*NN_FDL_LEVELS;i++)resources[i]=(void*)1;
 CK(nn_sampled_image_shadow_resources(o,resources,2,&count)==ENOSPC&&!count&&resources[0]==(void*)1);
 CK(!nn_sampled_image_shadow_resources(o,resources,3+2*NN_FDL_LEVELS,&count)&&count==3+2*levels&&resources[0]==canonical&&resources[1]==native);
 const struct nn_fdl_image_view*v=NULL;CK(!nn_sampled_image_shadow_view(o,&v)&&!memcmp(v,&p.native,sizeof(*v)));
 uint32_t words[16],expected[16];CK(!nn_sampled_image_shadow_descriptor(o,words));
 struct nn_fdl_image_desc fd=p.descriptor;struct nn_fdl_image_view real;fd.address=nn_bo_address(native);CK(!nn_fdl_image(&fd,&real));CK(!memcmp(words,real.descriptor,64));
 struct nn_sampled_image_binding b={native,0,nn_bo_size(native),0,nn_bo_size(native)};CK(!nn_sampled_image_descriptor(v,&b,expected)&&!memcmp(words,expected,64));
 b.bytes=v->required_span-1;CK(nn_sampled_image_descriptor(v,&b,words)==EINVAL);for(unsigned i=0;i<16;i++)CK(!words[i]);
 b=(struct nn_sampled_image_binding){native,64,nn_bo_size(native)-64,0,nn_bo_size(native)};CK(nn_sampled_image_descriptor(v,&b,words)==EINVAL);
 struct nn_sampler_state state={.version=1,.min_filter=NVN_MIN_FILTER_NEAREST,.mag_filter=NVN_MAG_FILTER_NEAREST,.wrap={NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE},.compare_func=NVN_COMPARE_FUNC_LESS,.lod_max=1000,.anisotropy=1};
 struct nn_sampler_layout layout={v->view_width,v->view_height,v->level_count,1,2,1,0,0};
 CK(!nn_sampled_image_sampler(v,&state,&layout,words));layout.dimensions=3;CK(nn_sampled_image_sampler(v,&state,&layout,words)==ENOTSUP);
 layout.dimensions=2;layout.integer=1;CK(nn_sampled_image_sampler(v,&state,&layout,words)==ENOTSUP);layout.integer=0;
 state.wrap[0]=NVN_WRAP_MODE_CLAMP_TO_BORDER;CK(nn_sampled_image_sampler(v,&state,&layout,words)==ENOTSUP);
 for(unsigned i=0;i<levels;i++){
  nn_bo *job=NULL;size_t bytes=0;CK(!nn_sampled_image_shadow_dispatch(o,i,&job,&bytes)&&job==resources[4+2*i]&&bytes&&bytes%4==0&&bytes<=nn_bo_size(job));
  CK(!memcmp(resources[3+2*i]->map,&p.copy[i].args,64));
  CK(p.copy[i].args.direction==0&&p.copy[i].args.linear_row_stride==p.native.level[i].pitch&&p.copy[i].args.linear_image_stride==p.native.level[i].layer_stride);
  CK(p.copy[i].args.row_bytes==s.level[i].row_bytes&&p.copy[i].args.rows==s.level[i].blocks_y);
 }
 nn_bo*none=(void*)1;size_t zero=7;CK(nn_sampled_image_shadow_dispatch(o,levels,&none,&zero)==EINVAL&&!none&&!zero);
 CK(nn_sampled_image_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EBUSY);
 for(size_t i=1;i<count;i++)resources[i]->busy++;
 unsigned old=frees;CK(nn_sampled_image_shadow_destroy(&o)==EBUSY&&o&&frees==old);
 for(size_t i=1;i<count;i++)resources[i]->busy--;
 native=NULL;CK(!nn_sampled_image_shadow_destroy(&o)&&!o&&d->live==before);free_source();CK(!d->live);
}
static void negatives(nn_device*d)
{
 struct nn_tex_source_layout s=source(NVN_FORMAT_RGBA16F,73,41,3);struct nn_sampled_image_plan p;CK(!nn_sampled_image_plan(&s,1,2,&p));
 CK(!nn_bo_alloc(d,(size_t)s.payload_bytes+128,&canonical));struct nn_block_copy_range c={canonical,64,(size_t)s.payload_bytes+4,68};unsigned before=d->live;
 for(unsigned mode=0;mode<3;mode++)for(unsigned at=1;at<=8;at++){
  struct nn_sampled_image_shadow*o=NULL;reset();native_allocation=allocs+1;
  if(mode==0)fail_alloc=allocs+at;else if(mode==1)fail_map=maps+at;else fail_cache=caches+at;
  int e=nn_sampled_image_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o);CK(e==0||e==ENOMEM||e==ENOTSUP||e==EIO);reset();native=NULL;
  CK(!nn_sampled_image_shadow_destroy(&o)&&!o&&d->live==before);
 }
 struct nn_sampled_image_shadow*o=NULL;native_allocation=allocs+1;fail_cache=caches+1;fail_destroy=1;
 CK(nn_sampled_image_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EIO&&o);retained++;reset();native=NULL;CK(!nn_sampled_image_shadow_destroy(&o)&&!o&&d->live==before);
 unsigned old=allocs;struct nn_sampled_image_plan bad=p;bad.copy[1].args.direction=1;
 CK(nn_sampled_image_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&bad,&c,&o)==EINVAL&&!o&&allocs==old);
 struct nn_block_copy_range small=c;small.owner_bytes-=5;CK(nn_sampled_image_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&small,&o)==EINVAL&&!o&&allocs==old);
 small=c;small.owner_offset=68;small.owner_bytes=s.payload_bytes;CK(nn_sampled_image_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&small,&o)==EINVAL&&!o&&allocs==old);
 uint64_t ca=canonical->address;canonical->address=UINT64_MAX-63;CK(nn_sampled_image_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EINVAL&&!o&&allocs==old);canonical->address=ca;
 native_allocation=allocs+1;native_alias=1;CK(nn_sampled_image_shadow_create(d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EINVAL&&!o&&d->live==before);native_alias=0;
 free_source();
 for(unsigned f=0;f<200;f++){
  struct nn_fdl_format_info info;int e=nn_fdl_image_format_info(0x06050002,f,&info);if(e)continue;
  if(info.result_type==NN_FDL_RESULT_FLOAT&&info.block_width==1&&info.block_height==1)continue;
  s=source(f,73,41,1);memset(&p,0x5a,sizeof(p));CK(nn_sampled_image_plan(&s,0,1,&p)==ENOTSUP);const unsigned char *b=(const void*)&p;for(size_t i=0;i<sizeof(p);i++)CK(!b[i]);
 }
 struct nn_tex_source_desc a={.target=4,.format=NVN_FORMAT_RGBA8,.width=8,.height=8,.levels=1,.layers=2,.block_height_log2=-1};CK(!nn_tex_source_layout(&a,&s));CK(nn_sampled_image_plan(&s,0,1,&p)==ENOTSUP);
 a.target=2;a.layers=1;a.depth=8;CK(!nn_tex_source_layout(&a,&s));CK(nn_sampled_image_plan(&s,0,1,&p)==ENOTSUP);
}
static void programs(nn_device*d)
{
 const struct nn_offline_graphics_archive*a=&nn_offline_graphics;const struct nn_pipeline_texture_types*const*types=NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE.pairs;
 CK(a->pair_count>=2&&a->pair_count==NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE.pair_count);
 for(size_t i=0;i<a->pair_count;i++){
  const struct nn_offline_graphics_pair*pair=a->pairs+i;const struct nn_offline_graphics_stage*vs=a->stages+pair->vs_index,*fs=a->stages+pair->fs_index;
  nn_pipeline_program*p=NULL,*ret=NULL;CK(!nn_pipeline_program_create_typed(d,a,vs,fs,types[i],&p,&ret)&&p&&!ret);normal_programs++;
  for(unsigned b=0;b<16;b++){
   uint32_t ops=99;int e=nn_sampled_image_program(p,types[i]->recipe,b,&ops);
   if(types[i]->texture_mask&(1u<<b))CK(!e&&ops==types[i]->operations[b]);else CK(e==ENOTSUP&&!ops);
  }
  uint32_t ops=1;CK(nn_sampled_image_program(p,types[(i+1)%a->pair_count]->recipe,1,&ops)==EINVAL&&!ops);
  CK(nn_sampled_image_program(p,types[i]->recipe,16,&ops)==EINVAL&&!ops);CK(!nn_pipeline_program_destroy(p));
 }
 /* Negative facts are held in an independent program. They never modify the
  * immutable normal archive or get mislabeled as compiler facts. */
 const struct nn_offline_graphics_pair*pair=a->pairs;const struct nn_offline_graphics_stage*vs=a->stages+pair->vs_index,*fs=a->stages+pair->fs_index;
 struct nn_pipeline_texture_types bad=*types[0];bad.dimensions[1]=1;nn_pipeline_program*p=NULL,*ret=NULL;CK(!nn_pipeline_program_create_typed(d,a,vs,fs,&bad,&p,&ret));uint32_t ops=7;CK(nn_sampled_image_program(p,bad.recipe,1,&ops)==ENOTSUP&&!ops);CK(!nn_pipeline_program_destroy(p));
 CK(!d->live);
}
int main(void)
{
 nn_device d={0};unsigned formats=0,snorm=0;const unsigned shapes[][3]={{1,1,1},{3,5,2},{73,41,3},{127,65,7}};
 for(unsigned f=0;f<200;f++){
  struct nn_fdl_format_info info;int e=nn_fdl_image_format_info(0x06050002,f,&info);if(e||info.result_type!=NN_FDL_RESULT_FLOAT||info.block_width!=1||info.block_height!=1)continue;
  formats++;snorm+=info.snorm;for(unsigned i=0;i<4;i++)verify(&d,f,shapes[i][0],shapes[i][1],shapes[i][2],i?1:0);
 }
 CK(formats==22&&snorm==6);negatives(&d);programs(&d);CK(!d.live);
 printf("{\"passed\":true,\"formats\":%u,\"snorm_metadata\":%u,\"checks\":%u,\"plans\":%u,\"owners\":%u,\"normal_programs\":%u,\"retained\":%u,\"CPU_pixel_maps\":0,\"live\":%u,\"GPU_execution\":false}\n",formats,snorm,checks,plans,owners,normal_programs,retained,d.live);return 0;
}
