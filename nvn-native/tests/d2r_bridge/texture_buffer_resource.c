/* HOST ONLY real public core lifecycle + actual FDL buffer binding, no GPU. */
#include NN_NVN_CORE_SOURCE
#include "nvn_native/texture_buffer.h"
#include "nvn_native/pipeline_resource_queries.h"
#include NN_TEX_BUFFER_RECIPES
static unsigned checks,allocations,caches,submissions,fail_cache,fail_object_alloc;
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t n,size_t size){if(fail_object_alloc && n==1 && size==sizeof(struct object))return NULL;return __real_calloc(n,size);}
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(x,e) do{x;T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
int __real_nn_bo_alloc(nn_device *,size_t,nn_bo **);
int __wrap_nn_bo_alloc(nn_device *d,size_t n,nn_bo **b){allocations++;return __real_nn_bo_alloc(d,n,b);}
int __real_nn_bo_cache(nn_bo *,size_t,size_t,int);
int __wrap_nn_bo_cache(nn_bo *b,size_t o,size_t n,int from){caches++;if(fail_cache)return EIO;return __real_nn_bo_cache(b,o,n,from);}
int __real_nn_queue_submit(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,uint32_t *);
int __wrap_nn_queue_submit(nn_queue *q,nn_bo *b,size_t o,size_t n,nn_bo *const *r,size_t c,uint32_t *t){submissions++;return __real_nn_queue_submit(q,b,o,n,r,c,t);}
int __real_nn_queue_submit_waits(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,const struct nn_queue_dependency *,size_t,uint32_t *);
int __wrap_nn_queue_submit_waits(nn_queue *q,nn_bo *b,size_t o,size_t n,nn_bo *const *r,size_t c,const struct nn_queue_dependency *d,size_t k,uint32_t *t){submissions++;return __real_nn_queue_submit_waits(q,b,o,n,r,c,d,k,t);}
static void *make_pool(NVNdevice *device,NVNmemoryPool *pool,NVNmemoryPoolBuilder *pb,size_t bytes,int flags)
{
 void *p=aligned_alloc(4096,bytes);T(p);memset(p,0xa5,bytes);
 OK(api_nvnMemoryPoolBuilderSetDefaults(pb));OK(api_nvnMemoryPoolBuilderSetDevice(pb,device));OK(api_nvnMemoryPoolBuilderSetFlags(pb,flags));OK(api_nvnMemoryPoolBuilderSetStorage(pb,p,bytes));T(api_nvnMemoryPoolInitialize(pool,pb));T(!nn_nvn_error());return p;
}
static void setup(NVNtextureBuilder *b,NVNdevice *device,unsigned format,int width,int flags)
{
 OK(api_nvnTextureBuilderSetDefaults(b));OK(api_nvnTextureBuilderSetDevice(b,device));OK(api_nvnTextureBuilderSetTarget(b,NVN_TEXTURE_TARGET_BUFFER));OK(api_nvnTextureBuilderSetFlags(b,flags));OK(api_nvnTextureBuilderSetFormat(b,(NVNformat)format));OK(api_nvnTextureBuilderSetSize1D(b,width));
}
static int buffer_format_family(void);
int main(void)
{
 T(!buffer_format_family());
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));T(!nn_nvn_error());struct object *dev=find(&device,DEVICE);
 NVNmemoryPoolBuilder pb[4];NVNmemoryPool pools[4];void *memory[4];memory[0]=make_pool(&device,pools,pb,1048576,0x24);memory[1]=make_pool(&device,pools+1,pb+1,16384,0x24);memory[2]=make_pool(&device,pools+2,pb+2,4096,0x24);memory[3]=make_pool(&device,pools+3,pb+3,4096,0xa1);
 NVNtexturePool table={0};T(api_nvnTexturePoolInitialize(&table,pools+1,0,272));T(!nn_nvn_error());struct object *tp=find(&table,TPOOL),*p=find(pools,POOL);
 NVNtextureBuilder b={0};NVNtexture texture={0};const int widths[]={1,7,32767,32768,65537};
 for(unsigned recipe=0;recipe<NN_TEXTURE_BUFFER_RECIPE_COUNT;recipe++)for(unsigned k=0;k<5;k++){
  unsigned format=NN_TEXTURE_BUFFER_RECIPES[recipe].format;int width=widths[k];size_t bytes=(size_t)width*4;size_t offset=k&1?32:4096;
  setup(&b,&device,format,width,NVN_TEXTURE_FLAGS_LINEAR);T(api_nvnTextureBuilderGetStorageSize(&b)==bytes);T(api_nvnTextureBuilderGetStorageAlignment(&b)==32);T(!nn_nvn_error());OK(api_nvnTextureBuilderSetStorage(&b,pools,(ptrdiff_t)offset));size_t refs=p->refs;unsigned a=allocations;
  fail_object_alloc=1;BAD((void)api_nvnTextureInitialize(&texture,&b),ENOMEM);fail_object_alloc=0;T(p->refs==refs&&allocations==a);
  T(api_nvnTextureInitialize(&texture,&b));T(!nn_nvn_error());T(allocations==a&&p->refs==refs+1);struct object *t=find(&texture,TEXTURE);T(t->bo==p->bo&&t->pool==p&&!t->source_layout&&t->offset==offset&&t->size==bytes);T(!texture_profile(&t->texture));
  T(api_nvnTextureGetTarget(&texture)==NVN_TEXTURE_TARGET_BUFFER);T(api_nvnTextureGetFormat(&texture)==(NVNformat)format);T(api_nvnTextureGetWidth(&texture)==width);T(api_nvnTextureGetHeight(&texture)==1&&api_nvnTextureGetDepth(&texture)==1);T(api_nvnTextureGetStride(&texture)==0);T(api_nvnTextureGetStorageSize(&texture)==(int)bytes);T(api_nvnTextureGetTextureAddress(&texture)==nn_bo_address(p->bo)+offset);T(api_nvnTextureGetRawStorageClass(&texture)==NVN_RAW_STORAGE_CLASS_LINEAR);T(!nn_nvn_error());
  T(api_nvnTextureGetStorageClass(&texture)==0x100);T(api_nvnTextureBuilderGetStorageClass(&b)==0x1fe);T(!nn_nvn_error());
  struct nn_tex_buffer_binding binding;T(!nn_tex_buffer_bind(NN_TEXTURE_BUFFER_RECIPES+recipe,t->bo,0,p->size,t->offset,t->size,&binding));T(binding.resource==p->bo&&binding.elements==(unsigned)width&&binding.offset==offset&&binding.bytes==bytes&&binding.format==format);T(binding.descriptor[2]>>29==4); /* actual FDL TEX_BUFFER */
  NVNtextureView view={0};OK(api_nvnTextureViewSetDefaults(&view));OK(api_nvnTextureViewSetLevels(&view,0,1));OK(api_nvnTextureViewSetLayers(&view,0,1));OK(api_nvnTextureViewSetFormat(&view,(NVNformat)format));OK(api_nvnTextureViewSetTarget(&view,NVN_TEXTURE_TARGET_BUFFER));NVNtextureView copied=view;
  OK(api_nvnTexturePoolRegisterTexture(&table,257,&texture,k&1?&copied:NULL));T(tp->slots[257].valid&&tp->slots[257].pool==p&&!tp->slots[257].source_layout&&tp->slots[257].offset==offset&&tp->slots[257].size==bytes);T(tp->slots[257].snapshot.texture.target==NVN_TEXTURE_TARGET_BUFFER&&tp->slots[257].snapshot.texture.width==width);
  const struct descriptor_entry saved=tp->slots[257];unsigned char previous[32];memcpy(previous,(char*)nn_bo_map(tp->pool->bo)+257*32,32);
  OK(api_nvnTextureViewSetLevels(&view,1,1));BAD(api_nvnTexturePoolRegisterTexture(&table,257,&texture,&view),ENOTSUP);T(!memcmp(previous,(char*)nn_bo_map(tp->pool->bo)+257*32,32)&&!memcmp(&saved,tp->slots+257,sizeof(saved)));
  fail_cache=1;BAD(api_nvnTexturePoolRegisterTexture(&table,257,&texture,NULL),EIO);fail_cache=0;T(!memcmp(previous,(char*)nn_bo_map(tp->pool->bo)+257*32,32)&&!memcmp(&saved,tp->slots+257,sizeof(saved)));
  BAD((void)api_nvnTextureInitialize(&texture,&b),EEXIST);T(p->refs==2); /* one texture plus the single retained registration */
  OK(api_nvnTextureBuilderSetWidth(&b,9));T(api_nvnTextureGetWidth(&texture)==width);OK(api_nvnTextureFinalize(&texture));T(!memcmp(&saved,tp->slots+257,sizeof(saved)));BAD(api_nvnMemoryPoolFinalize(pools),EBUSY);
 }
 /* Same canonical bytes can be independently named by an NVNbuffer; no AHB,
  * 2D row pitch, GOB layout, private shadow or replacement address is used. */
 NVNbufferBuilder bb={0};NVNbuffer buffer={0};OK(api_nvnBufferBuilderSetDefaults(&bb));OK(api_nvnBufferBuilderSetDevice(&bb,&device));OK(api_nvnBufferBuilderSetStorage(&bb,pools,32,64));T(api_nvnBufferInitialize(&buffer,&bb));T(api_nvnBufferGetAddress(&buffer)==nn_bo_address(p->bo)+32);T(api_nvnBufferMap(&buffer)==(char*)memory[0]+32);OK(api_nvnBufferFinalize(&buffer));
 setup(&b,&device,NVN_FORMAT_RGBA8,8,NVN_TEXTURE_FLAGS_LINEAR);OK(api_nvnTextureBuilderSetStorage(&b,pools+3,32));T(api_nvnTextureInitialize(&texture,&b));T(api_nvnTextureGetTextureAddress(&texture)==nn_bo_address(find(pools+3,POOL)->bo)+32);OK(api_nvnTexturePoolRegisterTexture(&table,257,&texture,NULL));T(p->refs==0);OK(api_nvnTextureFinalize(&texture));BAD(api_nvnMemoryPoolFinalize(pools+3),EBUSY);
 /* A current BUFFER slot is rejected before a two-handle submit can allocate
  * a parent/cache/submit either handle. Recording selectors is still legal. */
 NVNqueueBuilder qb={0};NVNqueue queue={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&device));T(api_nvnQueueInitialize(&queue,&qb));
 NVNtextureBuilder render_builder={0};NVNtexture render_target={0};
 OK(api_nvnTextureBuilderSetDefaults(&render_builder));OK(api_nvnTextureBuilderSetDevice(&render_builder,&device));
 OK(api_nvnTextureBuilderSetFlags(&render_builder,NVN_TEXTURE_FLAGS_LINEAR));OK(api_nvnTextureBuilderSetTarget(&render_builder,NVN_TEXTURE_TARGET_2D));
 OK(api_nvnTextureBuilderSetSize2D(&render_builder,64,64));OK(api_nvnTextureBuilderSetFormat(&render_builder,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetStride(&render_builder,256));
 OK(api_nvnTextureBuilderSetStorage(&render_builder,pools,4096));T(api_nvnTextureInitialize(&render_target,&render_builder));T(!nn_nvn_error());
 struct handle h0={.id=100,.device=dev,.commands=find(pools+2,POOL),.bytes=8},h1={.id=101,.device=dev,.commands=find(pools+2,POOL),.bytes=8};struct late_draw d={.target=find(&render_target,TEXTURE),.texture_pool=tp,.texture_id=257};h0.next=&h1;h1.draws=&d;T(!handles);handles=&h0;const NVNcommandHandle ids[2]={100,101};unsigned a=allocations,c=caches,s=submissions;BAD(api_nvnQueueSubmitCommands(&queue,2,ids),ENOTSUP);T(a==allocations&&c==caches&&s==submissions);handles=NULL;OK(api_nvnQueueFinalize(&queue));OK(api_nvnTextureFinalize(&render_target));
 for(unsigned recipe=0;recipe<NN_TEXTURE_BUFFER_RECIPE_COUNT;recipe++)for(unsigned k=0;k<3;k++){
  const ptrdiff_t offsets[]={4,12,60};setup(&b,&device,NN_TEXTURE_BUFFER_RECIPES[recipe].format,7,0);
  T(api_nvnTextureBuilderGetStorageSize(&b)==28);T(api_nvnTextureBuilderGetStorageAlignment(&b)==4);T(api_nvnTextureBuilderGetStorageClass(&b)==0x1fe);T(!nn_nvn_error());
  OK(api_nvnTextureBuilderSetStorage(&b,pools,offsets[k]));T(api_nvnTextureInitialize(&texture,&b));T(!nn_nvn_error());
  struct object *t=find(&texture,TEXTURE);struct nn_tex_buffer_binding binding;
  T(t->bo==p->bo && t->offset==(size_t)offsets[k] && t->size==28 && !t->source_layout);
  T(!nn_tex_buffer_bind(NN_TEXTURE_BUFFER_RECIPES+recipe,p->bo,0,p->size,t->offset,t->size,&binding));T(binding.address==nn_bo_address(p->bo)+(size_t)offsets[k] && binding.descriptor_base>=nn_bo_address(p->bo) && binding.offset==t->offset && binding.resource==p->bo);
  T(api_nvnTextureGetStorageClass(&texture)==0x100);T(api_nvnTextureGetRawStorageClass(&texture)==NVN_RAW_STORAGE_CLASS_COLOR_AND_ZF32);T(!nn_nvn_error());
  OK(api_nvnTexturePoolRegisterTexture(&table,257,&texture,NULL));OK(api_nvnTextureFinalize(&texture));
 }
 setup(&b,&device,NVN_FORMAT_RGBA8,8,0);OK(api_nvnTextureBuilderSetStorage(&b,pools,4));enum native_source_policy policy=dev->texture_source_policy;dev->texture_source_policy=0;
 T(api_nvnTextureBuilderGetStorageSize(&b)==32);BAD((void)api_nvnTextureBuilderGetStorageAlignment(&b),ENOTSUP);BAD((void)api_nvnTextureBuilderGetStorageClass(&b),ENOTSUP);BAD((void)api_nvnTextureInitialize(&texture,&b),ENOTSUP);dev->texture_source_policy=policy;
 setup(&b,&device,NVN_FORMAT_RGBA8,8,NVN_TEXTURE_FLAGS_LINEAR);BAD(api_nvnTextureBuilderSetStorage(&b,pools,4),EINVAL);BAD(api_nvnTextureBuilderSetStorage(&b,pools,1048576),EINVAL);
 OK(api_nvnTextureBuilderSetSamples(&b,1));BAD((void)api_nvnTextureBuilderGetStorageSize(&b),ENOTSUP);OK(api_nvnTextureBuilderSetSamples(&b,0));OK(api_nvnTextureBuilderSetLevels(&b,2));BAD((void)api_nvnTextureBuilderGetStorageSize(&b),ENOTSUP);OK(api_nvnTextureBuilderSetLevels(&b,1));OK(api_nvnTextureBuilderSetStride(&b,32));BAD((void)api_nvnTextureBuilderGetStorageSize(&b),ENOTSUP);OK(api_nvnTextureBuilderSetStride(&b,0));OK(api_nvnTextureBuilderSetFormat(&b,NVN_FORMAT_DEPTH16));BAD((void)api_nvnTextureBuilderGetStorageSize(&b),ENOTSUP);OK(api_nvnTextureBuilderSetFormat(&b,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetWidth(&b,INT_MAX));BAD((void)api_nvnTextureBuilderGetStorageSize(&b),EOVERFLOW);
 setup(&b,&device,NVN_FORMAT_R32F,8,0);OK(api_nvnTextureBuilderSetStorage(&b,pools,4));
 OK(api_nvnTexturePoolFinalize(&table));for(unsigned i=0;i<4;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(memory[i]);}OK(api_nvnDeviceFinalize(&device));
 a=allocations;BAD((void)api_nvnTextureInitialize(&texture,&b),EINVAL);T(a==allocations);BAD((void)api_nvnTextureBuilderGetStorageAlignment(&b),ENOTSUP);BAD((void)api_nvnTextureBuilderGetStorageClass(&b),ENOTSUP);T(api_nvnTextureBuilderGetStorageSize(&b)==32);T(!nn_nvn_error());
 while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"gpu_submits\":0,\"scope\":\"HOST public BUFFER resource, canonical refs, real FDL descriptor and strict GPU rejection; no guest/GPU\"}\n",checks);return 0;
}

/* Independent metadata expectations; no actual native BUFFER consumer opened.
 * CPU fields/alias values are observable without pretending GPU fetch ran. */
static int buffer_format_family(void)
{
 const struct {unsigned format,cpp;} formats[]={
 {1,1},{2,1},{3,1},{4,1},{5,2},{6,2},{7,2},{8,2},{9,2},{10,4},{11,4},{12,4},
 {13,2},{14,2},{15,2},{16,2},{17,4},{18,4},{19,4},{20,4},{21,4},{22,8},{23,8},{24,8},
 {34,12},{35,12},{36,12},{37,4},{38,4},{39,4},{40,4},{41,8},{42,8},{43,8},{44,8},{45,8},{46,16},{47,16},{48,16},
 {57,2},{58,2},{59,2},{60,2},{61,4},{62,4},{63,4},{64,4},{154,4}};
 unsigned before=checks;NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));
 NVNmemoryPoolBuilder pb[3]={0};NVNmemoryPool pools[3]={0};
 void *memory=make_pool(&dk,pools,pb,20*1024*1024,0xa4),*descriptors=make_pool(&dk,pools+1,pb+1,16384,0x24);
 void *noaccess=make_pool(&dk,pools+2,pb+2,2*1024*1024,0xa1);
 struct object *pool=find(pools,POOL);NVNtexturePool table={0};T(api_nvnTexturePoolInitialize(&table,pools+1,0,272));
 struct object *tp=find(&table,TPOOL);NVNtextureBuilder b={0};NVNtexture texture={0};
 for(unsigned i=0;i<sizeof(formats)/sizeof(*formats);i++)for(unsigned flag=0;flag<3;flag++)for(unsigned width=1;width<=4096;width*=16){
  unsigned f=formats[i].format,cpp=formats[i].cpp,alignment=flag==1?32:flag==2?128:cpp==12?4:cpp;
  int flags=flag==1?NVN_TEXTURE_FLAGS_LINEAR:flag==2?NVN_TEXTURE_FLAGS_LINEAR_RENDER_TARGET:0;
  size_t offset=flag?4096:alignment,bytes=(size_t)width*cpp;
  setup(&b,&dk,f,(int)width,flags);
  T(api_nvnTextureBuilderGetStorageSize(&b)==bytes&&api_nvnTextureBuilderGetStorageAlignment(&b)==alignment&&!nn_nvn_error());
  size_t refs=pool->refs;OK(api_nvnTextureBuilderSetStorage(&b,pools,(ptrdiff_t)offset));T(api_nvnTextureInitialize(&texture,&b));T(!nn_nvn_error());
  struct object *t=find(&texture,TEXTURE);T(t->bo==pool->bo&&t->pool==pool&&t->offset==offset&&t->size==bytes&&!t->source_layout&&pool->refs==refs+1);
  T(api_nvnTextureGetFormat(&texture)==(NVNformat)f&&api_nvnTextureGetWidth(&texture)==(int)width&&api_nvnTextureGetStorageSize(&texture)==(int)bytes);
  T(api_nvnTextureGetTextureAddress(&texture)==nn_bo_address(pool->bo)+offset);
  T(api_nvnTextureGetRawStorageClass(&texture)==(flag?NVN_RAW_STORAGE_CLASS_LINEAR:NVN_RAW_STORAGE_CLASS_COLOR_AND_ZF32));
  NVNtextureView view={0};OK(api_nvnTextureViewSetDefaults(&view));OK(api_nvnTextureViewSetFormat(&view,(NVNformat)f));OK(api_nvnTextureViewSetTarget(&view,NVN_TEXTURE_TARGET_BUFFER));
  OK(api_nvnTexturePoolRegisterTexture(&table,256,&texture,&view));T(tp->slots[256].pool==pool&&tp->slots[256].offset==offset&&tp->slots[256].size==bytes&&tp->slots[256].snapshot.texture.format==(int)f);
  const struct descriptor_entry snapshot=tp->slots[256];
  struct nn_pipeline_resource_binding binding={.stage=1,.domain=NN_PIPELINE_SEPARATE_TEXTURE,.selector=10,.low_word_defined=1,.payload=256,.guest_carrier=256};
  binding.texture=(struct nn_pipeline_registered_texture){.registered=1,.descriptor_pool=tp,.descriptor_id=256,.backing_pool=pool,.offset=snapshot.offset,.bytes=snapshot.size,
   .target=(uint32_t)snapshot.snapshot.texture.target,.format=(uint32_t)snapshot.snapshot.texture.format,.width=(uint32_t)snapshot.snapshot.texture.width,.height=1,.depth=1,.levels=1};
  struct nn_pipeline_resource_queries resources={.version=1,.graphics_texture_cb_known=1,.graphics_texture_cb=2,.selected_texture_pool=tp,.bindings=&binding,.binding_count=1};
  struct nn_pipeline_query query={.name="QuerySamplerType",.arguments=3,.type=NN_PIPELINE_QUERY_I32,.count=1,.args={110,-1}};
  uint32_t result[32];size_t count=0;T(!nn_pipeline_resource_query(&resources,1,&query,result,&count)&&count==1&&result[0]==2);
  query.name="QueryTextureFormat";unsigned expected=f<=24?f:f>=37&&f<=48?f-12:f>=61&&f<=63?f-24:0;
  if(expected)T(!nn_pipeline_resource_query(&resources,1,&query,result,&count)&&count==1&&result[0]==expected);
  else T(nn_pipeline_resource_query(&resources,1,&query,result,&count)==ENOTSUP&&count==0&&result[0]==0);
  binding.texture.bytes++;query.name="QuerySamplerType";T(nn_pipeline_resource_query(&resources,1,&query,result,&count)==EINVAL&&count==0);binding.texture.bytes--;

  OK(api_nvnTextureFinalize(&texture));T(!memcmp(&snapshot,tp->slots+256,sizeof(snapshot)));BAD(api_nvnMemoryPoolFinalize(pools),EBUSY);
  if(alignment>1){setup(&b,&dk,f,(int)width,flags);BAD(api_nvnTextureBuilderSetStorage(&b,pools,1),EINVAL);}
  setup(&b,&dk,f,INT_MAX,0);if(cpp>1)BAD((void)api_nvnTextureBuilderGetStorageSize(&b),EOVERFLOW);else T(api_nvnTextureBuilderGetStorageSize(&b)==INT_MAX);
 }
 /* Actual observed v14 shape is one general metadata case, not a game branch. */
 setup(&b,&dk,NVN_FORMAT_RGBA32F,65536,0);T(api_nvnTextureBuilderGetStorageSize(&b)==1048576);T(api_nvnTextureBuilderGetStorageAlignment(&b)==16);
 OK(api_nvnTextureBuilderSetStorage(&b,pools,18002624));T(api_nvnTextureInitialize(&texture,&b));
 T(api_nvnTextureGetTextureAddress(&texture)==nn_bo_address(pool->bo)+18002624&&api_nvnTextureGetStorageSize(&texture)==1048576);OK(api_nvnTextureFinalize(&texture));
 setup(&b,&dk,NVN_FORMAT_RGBA32F,65536,0);OK(api_nvnTextureBuilderSetStorage(&b,pools+2,4096));T(api_nvnTextureInitialize(&texture,&b));
 T(api_nvnTextureGetStorageSize(&texture)==1048576&&api_nvnTextureGetTextureAddress(&texture)==nn_bo_address(find(pools+2,POOL)->bo)+4096);
 OK(api_nvnTexturePoolRegisterTexture(&table,256,&texture,NULL));T(tp->slots[256].pool==find(pools+2,POOL)&&tp->slots[256].snapshot.texture.format==NVN_FORMAT_RGBA32F);
 OK(api_nvnTextureFinalize(&texture));BAD(api_nvnMemoryPoolFinalize(pools+2),EBUSY);
 for(unsigned f=0;f<=156;f++){
  unsigned found=0;for(unsigned i=0;i<sizeof(formats)/sizeof(*formats);i++)if(formats[i].format==f)found=1;
  if(found)continue;
  setup(&b,&dk,f,8,0);BAD((void)api_nvnTextureBuilderGetStorageSize(&b),ENOTSUP);
 }
 OK(api_nvnTexturePoolFinalize(&table));T(!pool->refs);OK(api_nvnMemoryPoolFinalize(pools));OK(api_nvnMemoryPoolFinalize(pools+1));OK(api_nvnMemoryPoolFinalize(pools+2));free(noaccess);free(memory);free(descriptors);OK(api_nvnDeviceFinalize(&dk));
 while(objects){T(builder_kind(objects->kind));remove_object(objects);}
 printf("HOST public BUFFER format family checks=%u formats=48 submits=0\n",checks-before);return 0;
}
