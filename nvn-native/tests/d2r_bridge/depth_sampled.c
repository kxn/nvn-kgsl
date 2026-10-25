/* SPDX-License-Identifier: MIT. HOST RAM faults/packets, no GPU execution. */
#include "nvn_native/depth_sampled.h"
#include "nvn_native/pm4.h"
#include NN_DEPTH_PLATFORM
#include NN_DEPTH_BLOCK_HEADER
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct nn_device {uint32_t chip;};
struct nn_bo {uint64_t address;size_t bytes;unsigned busy;void*map;};
static unsigned checks,allocs,destroys,maps,caches,live,owners,retained;
static unsigned alloc_fail,map_fail,cache_fail;
static int destroy_error;
static struct nn_depth_sampled_shadow *observed;
static nn_bo *canonical;
static uint64_t next_address=UINT64_C(0x100000000);
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
uint32_t nn_device_chip(const nn_device*d){return d->chip;}
size_t nn_device_gmem_bytes(const nn_device*d){(void)d;return 1179648;}
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){(void)d;allocs++;*out=NULL;if(allocs==alloc_fail)return ENOMEM;nn_bo*b=calloc(1,sizeof(*b));if(!b)return ENOMEM;b->bytes=(n+4095)&~(size_t)4095;b->map=malloc(b->bytes);if(!b->map){free(b);return ENOMEM;}memset(b->map,0xa5,b->bytes);b->address=next_address;next_address+=b->bytes+4096;*out=b;live++;return 0;}
int nn_bo_idle(const nn_bo*b){return !b?EINVAL:b->busy?EBUSY:0;}
int nn_bo_destroy(nn_bo*b){destroys++;if(destroy_error)return destroy_error;if(b->busy)return EBUSY;free(b->map);free(b);live--;return 0;}
size_t nn_bo_size(const nn_bo*b){return b->bytes;}
uint64_t nn_bo_address(const nn_bo*b){return b->address;}
void*nn_bo_map(const nn_bo*b){maps++;C(b!=canonical);if(observed)C(b!=observed->native.storage&&b!=observed->linear.storage);return maps==map_fail?NULL:b->map;}
int nn_bo_cache(nn_bo*b,size_t offset,size_t n,int from){caches++;C(b&&offset<=b->bytes&&n<=b->bytes-offset&&!from&&b!=canonical);if(observed)C(b!=observed->native.storage&&b!=observed->linear.storage);return caches==cache_fail?EIO:0;}
static void image_untouched(const nn_bo*b){for(size_t i=0;i<b->bytes;i++)C(((unsigned char*)b->map)[i]==0xa5);}
static void packet(const struct nn_depth_shadow_step*s){C(s->job&&s->bytes&&!((s->bytes)%4)&&s->bytes<=nn_bo_size(s->job));const uint32_t*w=s->job->map;for(size_t i=0;i<s->bytes/4;){size_t n=nvn_pm4_packet_words(w[i]);C(n&&n<=s->bytes/4-i);i+=n;}}
static void reset_faults(void){alloc_fail=map_fail=cache_fail=0;destroy_error=0;}
int main(void){struct nn_device d={0x06050002};
 for(unsigned f=0x32;f<=0x34;f++)for(unsigned variant=0;variant<3;variant++){
  struct nn_tex_source_desc desc={.target=variant==2?4:1,.format=f,.width=variant==0?73:129,.height=variant==0?41:65,.layers=variant==2?3:1,.levels=variant?3:1,.block_height_log2=-1};
  struct nn_tex_source_layout source;C(!nn_tex_source_layout(&desc,&source));const uint32_t swiz[]={NN_DEPTH_R,NN_DEPTH_ZERO,NN_DEPTH_ZERO,NN_DEPTH_ONE};struct nn_depth_sampled_plan plan;C(!nn_depth_sampled_plan(&source,variant?2:0,variant==2?2:0,swiz,&plan));C(plan.descriptor.roles==NN_DEPTH_SAMPLED&&plan.copy.region.direction==0&&plan.copy.region.base_layer==(variant==2?2u:0u));C(plan.aliases.linear.pitch>=plan.aliases.linear.width*(f==0x32?2u:4u));
  C(!nn_bo_alloc(&d,source.payload_bytes+8192,&canonical));struct nn_block_copy_range range={canonical,4096,(size_t)source.payload_bytes,4096};
  struct nn_depth_sampled_shadow s={0};observed=&s;unsigned before=allocs;struct nn_pipeline_platform bad_platform=nn_pipeline_device;bad_platform.gmem_bytes-=4096;
  C(nn_depth_sampled_create(&d,&NVN_COMPILED_IMAGE,&nn_pipeline_device,&source,&plan,&range,6,&s)==ENOSPC&&allocs==before);
  C(nn_depth_sampled_create(&d,&NVN_COMPILED_IMAGE,&bad_platform,&source,&plan,&range,7,&s)==ENOTSUP&&allocs==before);
  struct nn_block_copy_range bad_range=range;bad_range.owner_bytes--;C(nn_depth_sampled_create(&d,&NVN_COMPILED_IMAGE,&nn_pipeline_device,&source,&plan,&bad_range,7,&s)==EINVAL&&allocs==before);
  struct nn_depth_sampled_plan bad=plan;bad.copy.args.direction=1;C(nn_depth_sampled_create(&d,&NVN_COMPILED_IMAGE,&nn_pipeline_device,&source,&bad,&range,7,&s)==EINVAL&&allocs==before);
  C(!nn_depth_sampled_create(&d,&NVN_COMPILED_IMAGE,&nn_pipeline_device,&source,&plan,&range,7,&s));owners++;C(s.ready&&live==7&&allocs==before+6);image_untouched(canonical);image_untouched(s.native.storage);image_untouched(s.linear.storage);
  nn_bo *refs[7];memset(refs,0x7f,sizeof(refs));nn_bo *refs_before[7];memcpy(refs_before,refs,sizeof(refs));size_t count=9;C(nn_depth_sampled_resources(&s,refs,6,&count)==ENOSPC&&!count&&!memcmp(refs,refs_before,sizeof(refs)));C(!nn_depth_sampled_resources(&s,refs,7,&count)&&count==7&&refs[0]==canonical);
  struct nn_depth_shadow_step load[2];C(!nn_depth_sampled_sequence(&s,load));C(load[0].job==s.copy.job&&load[1].job==s.upload.job);for(unsigned i=0;i<2;i++)packet(load+i);C(!memcmp(s.uniform->map,&plan.copy.args,64));
  struct nn_depth_attachment binding;C(!nn_depth_sampled_binding(&s,&binding)&&binding.storage==s.native.storage&&!memcmp(&binding.view,&s.native.view,sizeof(binding.view)));
  struct nn_sampler_state sampler={.version=1,.wrap={1,1,1},.compare_func=8,.lod_max=1000,.anisotropy=1};
  struct nn_sampler_layout layout={.width=plan.native.view_width,.height=plan.native.view_height,.levels=1,.samples=1,.dimensions=2,.normalized=1,.depth=1};uint32_t words[4],old[4];
  C(!nn_depth_sampled_sampler(&plan.native,&sampler,&layout,words));layout.depth=0;C(!nn_sampler_state_encode(0x06050002,&sampler,&layout,old)&&!memcmp(words,old,16));C(nn_depth_sampled_sampler(&plan.native,&sampler,&layout,words)==ENOTSUP);layout.depth=1;
  sampler.compare_mode=1;C(nn_depth_sampled_sampler(&plan.native,&sampler,&layout,words)==ENOTSUP);sampler.compare_mode=0;
  struct nn_depth_fdl_view invalid=plan.native;invalid.desc.roles=NN_DEPTH_ATTACHMENT;C(nn_depth_sampled_validate(&invalid)==ENOTSUP);invalid=plan.native;invalid.descriptor[0]^=1;C(nn_depth_sampled_validate(&invalid)==EINVAL);
  struct nn_pipeline_shader_recipe recipe={.version=1,.chip=0x06050002,.texture_mask=2,.sampler_mask=2};struct nn_pipeline_texture_types types={.version=1,.recipe=&recipe,.texture_mask=2,.sampler_mask=2,.dimensions={[1]=2},.operations={[1]=NN_PIPELINE_TEX_SAMPLE}};uint32_t op;
  C(!nn_depth_sampled_types(&types,1,&op)&&op==NN_PIPELINE_TEX_SAMPLE);recipe.sampler_mask=types.sampler_mask=0;types.operations[1]=NN_PIPELINE_TEX_FETCH|NN_PIPELINE_TEX_SIZE;C(!nn_depth_sampled_types(&types,1,&op));types.operations[1]=NN_PIPELINE_TEX_GATHER;recipe.sampler_mask=types.sampler_mask=2;types.gathers[1]=(struct nn_pipeline_gather_usage){1,1,2,0};C(!nn_depth_sampled_types(&types,1,&op));types.gathers[1].offsets=1;C(nn_depth_sampled_types(&types,1,&op)==ENOTSUP&&!op);types.gathers[1].offsets=0;types.gathers[1].component_mask=2;C(nn_depth_sampled_types(&types,1,&op)==ENOTSUP&&!op);
  /* The exact full ledger holds all six owned BOs: every single pending
   * reference must reject destruction before any handle/state mutation. */
  for(unsigned i=1;i<7;i++){struct nn_depth_sampled_shadow keep=s;unsigned freed=destroys;refs[i]->busy=1;C(nn_depth_sampled_destroy(&s)==EBUSY&&!memcmp(&s,&keep,sizeof(s))&&destroys==freed);refs[i]->busy=0;}
  before=allocs;C(nn_depth_sampled_create(&d,&NVN_COMPILED_IMAGE,&nn_pipeline_device,&source,&plan,&range,7,&s)==EBUSY&&allocs==before);
  destroy_error=EIO;nn_bo *held=s.upload.job;C(nn_depth_sampled_destroy(&s)==EIO&&!s.ready&&s.upload.job==held&&live==7);destroy_error=0;
  C(!nn_depth_sampled_destroy(&s)&&live==1);observed=NULL;
  for(unsigned i=1;i<=6;i++){memset(&s,0,sizeof(s));observed=&s;alloc_fail=allocs+i;int e=nn_depth_sampled_create(&d,&NVN_COMPILED_IMAGE,&nn_pipeline_device,&source,&plan,&range,7,&s);C(e==ENOMEM&&!s.ready);reset_faults();C(!nn_depth_sampled_destroy(&s)&&live==1);}
  for(unsigned kind=0;kind<2;kind++)for(unsigned i=1;i<=4;i++){
   memset(&s,0,sizeof(s));observed=&s;if(kind)cache_fail=caches+i;else map_fail=maps+i;destroy_error=EIO;
   int e=nn_depth_sampled_create(&d,&NVN_COMPILED_IMAGE,&nn_pipeline_device,&source,&plan,&range,7,&s);C(e&&!s.ready&&live>1);retained++;reset_faults();C(!nn_depth_sampled_destroy(&s)&&live==1);
  }
  observed=NULL;C(!nn_bo_destroy(canonical));canonical=NULL;C(!live);
 }
 C(owners==9&&retained==72&&!live);
 printf("{\"passed\":true,\"host_only\":true,\"gpu_execution\":false,\"checks\":%u,\"owners\":%u,\"retained_fault_owners\":%u,\"live\":%u,\"allocs\":%u,\"destroys\":%u}\n",checks,owners,retained,live,allocs,destroys);return 0;}
