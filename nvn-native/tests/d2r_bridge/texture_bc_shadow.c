/* HOST ONLY actual FDL/common PM4 encoding; RAM does not execute shaders. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/texture_bc_shadow.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nvn.h>
#include NN_BC_BLOCK_IMAGE
#include "nvn_native/pipeline.h"
#include NN_BC_FETCH_RECIPE
#include NN_BC_FETCH_PLATFORM
struct nn_device {unsigned live;};
struct nn_bo {nn_device *d;void *map;size_t bytes;unsigned busy,id;};
static unsigned checks,allocs,frees,maps,caches,destroys,fail_alloc,fail_map,fail_cache,fail_destroy,retained;
static unsigned cases,plans;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"check %u line %d: %s\n",checks,__LINE__,#x);abort();}}while(0)
uint32_t nn_device_chip(const nn_device *d){(void)d;return 0x06050002;}
size_t nn_device_page(const nn_device *d){(void)d;return 4096;}
size_t nn_device_gmem_bytes(const nn_device *d){(void)d;return 1179648;}
int nn_bo_alloc(nn_device *d,size_t bytes,nn_bo **out)
{
 *out=NULL;allocs++;if(allocs==fail_alloc)return ENOMEM;
 nn_bo *b=calloc(1,sizeof(*b));CHECK(b);b->bytes=(bytes+4095)&~(size_t)4095;
 CHECK(!posix_memalign(&b->map,4096,b->bytes));memset(b->map,0xa5,b->bytes);
 b->d=d;b->id=allocs;d->live++;*out=b;return 0;
}
int nn_bo_destroy(nn_bo *b)
{destroys++;if(destroys==fail_destroy)return EIO;if(b->busy)return EBUSY;b->d->live--;free(b->map);free(b);frees++;return 0;}
int nn_bo_idle(const nn_bo *b){return !b?EINVAL:b->busy?EBUSY:0;}
void *nn_bo_map(const nn_bo *b){maps++;return maps==fail_map?NULL:b->map;}
uint64_t nn_bo_address(const nn_bo *b){return (uintptr_t)b->map;}
size_t nn_bo_size(const nn_bo *b){return b->bytes;}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int from)
{(void)from;caches++;if(caches==fail_cache)return EIO;return o<=b->bytes&&n<=b->bytes-o?0:EINVAL;}
static void reset_faults(void){fail_alloc=fail_map=fail_cache=fail_destroy=0;}
static struct nn_tex_source_layout layout(unsigned format,unsigned target,unsigned width,unsigned height,unsigned levels)
{
 /* Already-selected Generic/no-NVIDIA-compression payload. Public COMPRESSIBLE
  * permission stays on the NVN object and is resolved by device policy first. */
 struct nn_tex_source_desc d={.flags=0,.format=format,.target=target,.width=width,.height=height,
  .layers=target==4?3:1,.levels=levels,.block_height_log2=-1};struct nn_tex_source_layout s;
 CHECK(!nn_tex_source_layout(&d,&s));return s;
}
static void verify(nn_device *d,nn_bo *canonical,const struct nn_tex_source_layout *s,
 const struct nn_bc_shadow_plan *p,struct nn_bc_shadow *o)
{
 unsigned native_maps_before=maps;
 struct nn_block_copy_range c={canonical,64,(size_t)s->payload_bytes,64};
 CHECK(!nn_bc_shadow_create(d,&NVN_COMPILED_IMAGE,s,p,&c,o));CHECK(o->ready&&o->count==s->levels);
 CHECK(o->storage!=canonical&&o->storage!=o->code);
 /* Code+uniforms+CS metadata are mapped, but native bytes remain original
  * allocation fill and canonical is untouched: no host codec/copy can hide. */
 CHECK(maps>native_maps_before);
 for(size_t b=0;b<o->storage->bytes;b++)CHECK(((unsigned char*)o->storage->map)[b]==0xa5);
 for(size_t b=0;b<canonical->bytes;b++)CHECK(((unsigned char*)canonical->map)[b]==0xa5);
 uint32_t descriptor[16];CHECK(!nn_bc_shadow_descriptor(o,descriptor));
 struct nn_fdl_image_desc desc=p->descriptor;desc.address=nn_bo_address(o->storage);struct nn_fdl_image_view actual;
 CHECK(!nn_fdl_image(&desc,&actual));CHECK(!memcmp(&o->view,&actual,sizeof(actual))&&!memcmp(descriptor,actual.descriptor,64));
 nn_bo *resources[33],*poison=(void*)1;size_t n=999;resources[0]=poison;
 CHECK(nn_bc_shadow_resources(o,resources,2,&n)==ENOSPC&&n==0&&resources[0]==poison);
 CHECK(!nn_bc_shadow_resources(o,resources,33,&n)&&n==3+2*s->levels);
 CHECK(resources[0]==canonical&&resources[1]==o->storage&&resources[2]==o->code);
 for(unsigned i=0;i<s->levels;i++){
  CHECK(resources[3+2*i]==o->uniform[i]&&resources[4+2*i]==o->dispatch[i]);
  CHECK(o->dispatch_bytes[i]&&o->dispatch_bytes[i]%4==0);
  CHECK(!memcmp(o->uniform[i]->map,&p->copy[i].args,64));
  /* Actual source/middle descriptors remain in each original immutable job;
   * qualifier must submit every returned BO and all IBs in mip order. */
  for(size_t j=0;j<i;j++)CHECK(o->dispatch[i]!=o->dispatch[j]);
 }
 CHECK(nn_bc_shadow_create(d,&NVN_COMPILED_IMAGE,s,p,&c,o)==EBUSY);
 for(size_t i=0;i<n;i++)resources[i]->busy++;
 CHECK(nn_bc_shadow_destroy(o)==EBUSY&&!o->ready&&o->storage);
 for(size_t i=0;i<n;i++)resources[i]->busy--;
 CHECK(!nn_bc_shadow_destroy(o)&&!o->storage&&!o->canonical);
}
static void sampled_pipeline(nn_device *d)
{
 struct nn_tex_source_layout source=layout(0x42,1,37,21,3);struct nn_bc_shadow_plan p;
 CHECK(!nn_bc_shadow_plan(&source,1,1,0,1,&p));CHECK(!nn_bc_sampled_validate(&p.native));
 struct nn_fdl_image_view bad=p.native;bad.descriptor[0]^=1;CHECK(nn_bc_sampled_validate(&bad)==EINVAL);
 bad=p.native;bad.base_level=3;CHECK(nn_bc_sampled_validate(&bad));
 bad=p.native;bad.level_count=2;CHECK(nn_bc_sampled_validate(&bad)==EINVAL);
 nn_bo *native=NULL,*target=NULL,*vertices=NULL;
 CHECK(!nn_bo_alloc(d,p.native.required_span,&native));CHECK(!nn_bo_alloc(d,65536,&target));CHECK(!nn_bo_alloc(d,4096,&vertices));
 uint32_t descriptor[16];CHECK(!nn_bc_sampled_descriptor(&p.native,native,0,p.native.required_span,descriptor));
 struct nn_fdl_image_desc desc=p.descriptor;desc.address=nn_bo_address(native);struct nn_fdl_image_view actual;
 CHECK(!nn_fdl_image(&desc,&actual)&&!memcmp(descriptor,actual.descriptor,64));
 struct nn_sampler_state sampler={.version=1,.min_filter=NVN_MIN_FILTER_NEAREST,.mag_filter=NVN_MAG_FILTER_NEAREST,
  .wrap={NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE},.compare_func=NVN_COMPARE_FUNC_ALWAYS,.anisotropy=1};
 struct nn_sampler_layout sampler_layout={p.native.view_width,p.native.view_height,1,1,2,1,0,0};uint32_t encoded[4],reference[4];
 CHECK(!nn_bc_sampled_sampler(&p.native,&sampler,&sampler_layout,encoded));
 CHECK(!nn_sampler_state_encode(p.native.chip,&sampler,&sampler_layout,reference)&&!memcmp(encoded,reference,16));
 sampler_layout.width++;CHECK(nn_bc_sampled_sampler(&p.native,&sampler,&sampler_layout,encoded)==ENOTSUP);sampler_layout.width--;
 sampler.mag_filter=NVN_MAG_FILTER_LINEAR;CHECK(nn_bc_sampled_sampler(&p.native,&sampler,&sampler_layout,encoded)==ENOTSUP);
 sampler.mag_filter=NVN_MAG_FILTER_NEAREST;sampler.wrap[0]=NVN_WRAP_MODE_REPEAT;
 CHECK(nn_bc_sampled_sampler(&p.native,&sampler,&sampler_layout,encoded)==ENOTSUP);
 CHECK(nn_bc_sampled_descriptor(&p.native,native,0,p.native.required_span-1,descriptor)==EINVAL);
 for(unsigned i=0;i<16;i++)CHECK(!descriptor[i]);
 desc=(struct nn_fdl_image_desc){.schema=1,.chip=0x06050002,.format=37,.target=1,.width=64,.height=64,.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=NN_FDL_COLOR_TARGET|NN_FDL_SAMPLED};
 struct nn_fdl_image_view rt;CHECK(!nn_fdl_image(&desc,&rt));
 struct nn_tex_native_view target_layout={.schema=1,.chip=rt.chip,.format=rt.format,.target=rt.target,.levels=1,.layers=1,.samples=1,
  .width=rt.width,.height=rt.height,.pitch=rt.level[0].pitch,.layer_bytes=rt.level[0].layer_stride,.bytes=(uint32_t)rt.bytes,
  .alignment=rt.alignment,.required_span_bytes=(uint32_t)rt.required_span,.descriptor_address_word=rt.descriptor_address_word,
  .descriptor_offset=(uint32_t)rt.view_offset,.descriptor_address_mask=rt.descriptor_address_mask,.rb_mrt_buf_info=rt.rb_mrt_buf_info,
  .rb_mrt_pitch=rt.rb_mrt_pitch,.rb_mrt_array_pitch=rt.rb_mrt_array_pitch,.sp_ps_mrt_reg=rt.sp_ps_mrt_reg};
 memcpy(target_layout.texture_descriptor,rt.descriptor,64);
 struct nn_pipeline_draw_state state={.target={&target_layout,{target,0,rt.required_span}},.texture_bound=2,
  .draw={.primitive=4,.count=3,.instances=1}};
 state.textures[1].buffer=(struct nn_pipeline_buffer){native,0,p.native.required_span};state.sampled_fdl[1]=&p.native;
 CHECK(!nn_pipeline_full_raster(64,64,0,1,&state.raster));nn_render_state_defaults(&state.render);
 state.vertex.attribute_count=2;state.vertex.stream_count=1;state.vertex.streams[0].stride=16;
 state.vertex.buffers[0]=(struct nn_pipeline_buffer){vertices,0,4096};
 state.vertex.attributes[0]=(struct nn_pipeline_attribute){0x16,0,0};state.vertex.attributes[1]=(struct nn_pipeline_attribute){0x16,8,0};
 /* Pure preflight only needs the zero-C1 size relation, not Maxwell matching.
  * These HOST native-code views are never passed to the source archive,
  * ProgramSetShaders, or any fabricated Maxwell/control envelope. */
 struct nn_offline_graphics_stage stages[2]={{.stage=0},{.stage=1}};
 const struct nn_offline_graphics_stage *stage_ptrs[2]={stages,stages+1};
 for(unsigned i=0;i<2;i++){stages[i].source=(const unsigned char*)nn_pipeline_recipe.code[i];stages[i].source_bytes=stages[i].code_bytes=nn_pipeline_recipe.code_bytes[i];}
 unsigned before=allocs;
#if !NN_PIPELINE_BC_SAMPLED
 CHECK(nn_pipeline_preflight(d,stage_ptrs,&nn_pipeline_recipe,&nn_pipeline_device,&state,0)==ENOTSUP&&allocs==before);
 CHECK(!nn_bo_destroy(vertices)&&!nn_bo_destroy(target)&&!nn_bo_destroy(native)&&!d->live);return;
#endif
 CHECK(!nn_pipeline_preflight(d,stage_ptrs,&nn_pipeline_recipe,&nn_pipeline_device,&state,0));CHECK(allocs==before);
 struct nn_pipeline_draw_state wrong=state;wrong.sampled_fdl[0]=&p.native;
 CHECK(nn_pipeline_preflight(d,stage_ptrs,&nn_pipeline_recipe,&nn_pipeline_device,&wrong,0)==EINVAL&&allocs==before);
 wrong=state;wrong.textures[1].layout=&target_layout;
 CHECK(nn_pipeline_preflight(d,stage_ptrs,&nn_pipeline_recipe,&nn_pipeline_device,&wrong,0)==ENOTSUP&&allocs==before);
 wrong=state;wrong.sampled_fdl[1]=&bad;CHECK(nn_pipeline_preflight(d,stage_ptrs,&nn_pipeline_recipe,&nn_pipeline_device,&wrong,0)&&allocs==before);
 wrong=state;wrong.textures[1].buffer.bytes--;
 CHECK(nn_pipeline_preflight(d,stage_ptrs,&nn_pipeline_recipe,&nn_pipeline_device,&wrong,0)==EINVAL&&allocs==before);
 wrong=state;wrong.textures[1].buffer=(struct nn_pipeline_buffer){NULL,0,p.native.required_span};
 CHECK(!nn_pipeline_preflight(d,stage_ptrs,&nn_pipeline_recipe,&nn_pipeline_device,&wrong,4)&&allocs==before);
 wrong=state;wrong.textures[1].buffer.bo=target;
 CHECK(nn_pipeline_preflight(d,stage_ptrs,&nn_pipeline_recipe,&nn_pipeline_device,&wrong,0)==ENOTSUP&&allocs==before);
 wrong=state;struct nn_tex_native_view fake_rt=target_layout;fake_rt.format=0x42;wrong.target.layout=&fake_rt;
 CHECK(nn_pipeline_preflight(d,stage_ptrs,&nn_pipeline_recipe,&nn_pipeline_device,&wrong,0)==ENOTSUP&&allocs==before);
 CHECK(!nn_bo_destroy(vertices)&&!nn_bo_destroy(target)&&!nn_bo_destroy(native)&&!d->live);
}
/* Original FDL alone determines descriptor dimensions, base/mip range and
 * GPUVA words. These are CPU metadata checks, not texture-unit execution. */
static void sampled_mip_ranges(void)
{
 struct nn_tex_source_layout maximum=layout(0x41,1,16384,16384,15);
 struct nn_bc_shadow_plan maximum_plan;CHECK(!nn_bc_shadow_plan(&maximum,14,1,0,1,&maximum_plan));
 CHECK(maximum_plan.count==15&&maximum_plan.native.view_width==1&&maximum_plan.native.view_height==1);
 CHECK(!nn_bc_sampled_validate(&maximum_plan.native));

 for(unsigned format=0x41;format<=0x48;format++){
  struct nn_tex_source_layout s=layout(format,1,127,73,7);
  for(unsigned base=0;base<7;base++)for(unsigned count=1;count<=7-base;count++){
   struct nn_bc_shadow_plan p;CHECK(!nn_bc_shadow_plan(&s,base,count,0,1,&p));
   CHECK(!nn_bc_sampled_validate(&p.native));CHECK(p.native.base_level==base&&p.native.level_count==count);
   CHECK(p.native.view_width==s.level[base].width&&p.native.view_height==s.level[base].height);
   struct nn_sampler_state sampler={.version=1,.min_filter=NVN_MIN_FILTER_NEAREST,.mag_filter=NVN_MAG_FILTER_NEAREST,
    .wrap={NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE},
    .compare_func=NVN_COMPARE_FUNC_ALWAYS,.anisotropy=1,.lod_max=1000};
   struct nn_sampler_layout l={p.native.view_width,p.native.view_height,count,1,2,1,0,0};uint32_t words[4],single[4];
   CHECK(!nn_bc_sampled_sampler(&p.native,&sampler,&l,words));l.levels=1;
   CHECK(!nn_sampler_state_encode(0x06050002,&sampler,&l,single)&&!memcmp(words,single,16));
   l.levels=count;sampler.min_filter=NVN_MIN_FILTER_NEAREST_MIPMAP_NEAREST;
   CHECK(nn_bc_sampled_sampler(&p.native,&sampler,&l,words)==ENOTSUP);
  }
 }
}
int main(void)
{
 nn_device d={0};
 const unsigned dims[][2]={{1,1},{3,5},{8,8},{37,21},{127,65},{256,129}};
 uint32_t rgb_descriptor[16]={0},rgba_descriptor[16]={0};
 for(unsigned f=0x41;f<=0x44;f++)for(unsigned t=1;t<=4;t+=3)for(unsigned dim=0;dim<6;dim++){
  unsigned levels=dim<2?1:3;struct nn_tex_source_layout s=layout(f,t,dims[dim][0],dims[dim][1],levels);
  struct nn_bc_shadow_plan p;
  for(unsigned base=0;base<levels;base++){
   CHECK(!nn_bc_shadow_plan(&s,base,levels-base,t==4?1:0,t==4?2:1,&p));plans++;
   CHECK(p.count==levels&&p.native.format==f&&p.native.block_width==4&&p.native.block_height==4);
   for(unsigned m=0;m<levels;m++){
    CHECK(p.copy[m].region.level==m&&p.copy[m].region.direction==0&&p.copy[m].region.layers==s.layers);
    CHECK(p.copy[m].args.row_bytes==s.level[m].row_bytes&&p.copy[m].args.rows==s.level[m].blocks_y);
    CHECK(p.copy[m].args.linear_row_stride==p.native.level[m].pitch);
    CHECK(p.copy[m].args.linear_image_stride==p.native.level[m].layer_stride);
    CHECK(p.native.level[m].offset+p.copy[m].linear_bytes<=p.native.required_span);
   }
  }
  if(t==1&&dim==2){if(f==0x41)memcpy(rgb_descriptor,p.native.descriptor,64);if(f==0x42)memcpy(rgba_descriptor,p.native.descriptor,64);}
  if(dim==3){
   nn_bo *canonical=NULL;CHECK(!nn_bo_alloc(&d,(size_t)s.payload_bytes+128,&canonical));struct nn_bc_shadow o={0};
   verify(&d,canonical,&s,&p,&o);cases++;CHECK(!nn_bo_destroy(canonical)&&!d.live);
  }
 }
 /* Actual FDL encodes BC1 RGB alpha=ONE independently from punch-through
  * RGBA. Do not edit descriptor swizzles or decode compressed pixels here. */
 CHECK(memcmp(rgb_descriptor,rgba_descriptor,64));
 struct nn_tex_source_layout s=layout(0x42,4,37,21,3);struct nn_bc_shadow_plan p;CHECK(!nn_bc_shadow_plan(&s,0,3,0,3,&p));
 nn_bo *canonical=NULL;CHECK(!nn_bo_alloc(&d,s.payload_bytes+128,&canonical));
 struct nn_block_copy_range c={canonical,64,(size_t)s.payload_bytes,64};
 for(unsigned kind=0;kind<3;kind++)for(unsigned at=1;at<=(kind==0?8:kind==1?7:7);at++){
  struct nn_bc_shadow o={0};reset_faults();
  if(kind==0)fail_alloc=allocs+at;else if(kind==1)fail_map=maps+at;else fail_cache=caches+at;
  unsigned before=d.live;int e=nn_bc_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o);
  CHECK(e);reset_faults();CHECK(!nn_bc_shadow_destroy(&o)&&d.live==before);
 }
 for(unsigned error=0;error<3;error++){
  struct nn_bc_shadow o={0};reset_faults();fail_cache=caches+2;fail_destroy=destroys+1;
  CHECK(nn_bc_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&c,&o)==EIO&&o.storage&&!o.ready);retained++;
  if(error==1)o.storage->busy=1;
  if(error==2)fail_destroy=destroys+1;
  if(error){CHECK(nn_bc_shadow_destroy(&o)==(error==1?EBUSY:EIO)&&o.storage);o.storage->busy=0;}
  reset_faults();CHECK(!nn_bc_shadow_destroy(&o)&&d.live==1);
 }
 struct nn_bc_shadow o={0};struct nn_bc_shadow_plan bad=p;bad.native.descriptor[0]^=1;
 unsigned before=allocs;CHECK(nn_bc_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&bad,&c,&o)==EINVAL&&allocs==before);
 struct nn_block_copy_range shortc=c;shortc.owner_bytes--;CHECK(nn_bc_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&shortc,&o)==EINVAL&&allocs==before);
 shortc=c;shortc.owner_offset=65;shortc.offset=68;shortc.owner_bytes-=4;
 CHECK(nn_bc_shadow_create(&d,&NVN_COMPILED_IMAGE,&s,&p,&shortc,&o)==EINVAL&&allocs==before);
 CHECK(!nn_bo_destroy(canonical)&&!d.live);
 struct nn_tex_source_layout rgba=layout(37,1,8,8,1);CHECK(nn_bc_shadow_plan(&rgba,0,1,0,1,&p)==ENOTSUP&&!p.count);
 CHECK(nn_bc_shadow_plan(&s,3,1,0,1,&p)==EINVAL&&!p.count);
 sampled_pipeline(&d);sampled_mip_ranges();
 printf("{\"passed\":true,\"gpu_execution\":false,\"pipeline_bc_enabled\":%s,\"checks\":%u,\"metadata_plans\":%u,\"owners\":%u,\"retained_fault_owners\":%u,\"live\":%u}\n",NN_PIPELINE_BC_SAMPLED?"true":"false",checks,plans,cases,retained,d.live);return 0;
}
