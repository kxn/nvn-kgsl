/* HOST ONLY real public CPU resource/query; no GPU or original module. */
#include NN_NVN_CORE_SOURCE
static unsigned checks,allocations,mappings,caches,submissions,queries;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(x,e) do{x;T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
int __real_nn_bo_alloc(nn_device *,size_t,nn_bo **);
int __wrap_nn_bo_alloc(nn_device*d,size_t n,nn_bo **b){allocations++;return __real_nn_bo_alloc(d,n,b);}
void *__real_nn_bo_map(const nn_bo *);
void *__wrap_nn_bo_map(const nn_bo*b){mappings++;return __real_nn_bo_map(b);}
int __real_nn_bo_cache(nn_bo *,size_t,size_t,int);
int __wrap_nn_bo_cache(nn_bo*b,size_t o,size_t n,int f){caches++;return __real_nn_bo_cache(b,o,n,f);}
int __real_nn_queue_submit(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,uint32_t *);
int __wrap_nn_queue_submit(nn_queue*q,nn_bo*b,size_t o,size_t n,nn_bo *const*r,size_t c,uint32_t*t){submissions++;return __real_nn_queue_submit(q,b,o,n,r,c,t);}
static PFNNVNTEXTUREGETVIEWOFFSETPROC offset_query;
_Static_assert(_Generic(&api_nvnTextureGetViewOffset,PFNNVNTEXTUREGETVIEWOFFSETPROC:1,default:0),"exact signed ptrdiff64 query ABI");
static void query(const NVNtexture *t,const NVNtextureView *v,ptrdiff_t want,int failure)
{
 unsigned a=allocations,m=mappings,c=caches,s=submissions;queries++;
 ptrdiff_t got=offset_query(t,v);T(got==want);T(nn_nvn_error()==failure);nn_nvn_clear_error();
 T(a==allocations&&m==mappings&&c==caches&&s==submissions);
}
static void setup(NVNtextureBuilder*b,NVNdevice*d,int flags,int target,int format,int w,int h,int layers,int levels)
{
 OK(api_nvnTextureBuilderSetDefaults(b));OK(api_nvnTextureBuilderSetDevice(b,d));OK(api_nvnTextureBuilderSetFlags(b,flags));OK(api_nvnTextureBuilderSetTarget(b,(NVNtextureTarget)target));OK(api_nvnTextureBuilderSetFormat(b,(NVNformat)format));OK(api_nvnTextureBuilderSetSize3D(b,w,h,layers));OK(api_nvnTextureBuilderSetLevels(b,levels));
}
int main(void)
{
 offset_query=(PFNNVNTEXTUREGETVIEWOFFSETPROC)nvnBootstrapLoader("nvnTextureGetViewOffset");T(offset_query);
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));T(!nn_nvn_error());
 const size_t pool_bytes=4u*1024*1024;void*storage=aligned_alloc(4096,pool_bytes);T(storage);memset(storage,0xa5,pool_bytes);
 NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool={0};OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&device));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,0xa4));OK(api_nvnMemoryPoolBuilderSetStorage(&pb,storage,pool_bytes));T(api_nvnMemoryPoolInitialize(&pool,&pb));T(!nn_nvn_error());struct object*p=find(&pool,POOL);
 NVNtexturePool table={0};T(api_nvnTexturePoolInitialize(&table,&pool,3u*1024*1024,272));T(!nn_nvn_error());
 NVNtextureBuilder b={0};NVNtexture texture={0};
 const int formats[]={1,5,6,7,8,9,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x38,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x9a,0x9c};
 for(unsigned f=0;f<sizeof(formats)/sizeof(formats[0]);f++)for(unsigned array=0;array<2;array++)for(unsigned compressible=0;compressible<2;compressible++){
  int layers=array?3:1;size_t pool_offset=(f&1)?65536:512;
  setup(&b,&device,compressible?8:0,array?4:1,formats[f],37,19,layers,6);OK(api_nvnTextureBuilderSetStorage(&b,&pool,(ptrdiff_t)pool_offset));T(api_nvnTextureInitialize(&texture,&b));T(!nn_nvn_error());
  struct object*t=find(&texture,TEXTURE);const struct nn_tex_source_layout*l=t->source_layout;T(l&&t->offset==pool_offset&&t->bo==p->bo);size_t refs=p->refs;
  query(&texture,NULL,0,0);NVNtextureView v={0};OK(api_nvnTextureViewSetDefaults(&v));query(&texture,&v,0,0);
  OK(api_nvnTextureViewSetLevels(&v,0,6));OK(api_nvnTextureViewSetLayers(&v,0,layers));query(&texture,&v,0,0);
  for(unsigned level=0;level<6;level++)for(unsigned layer=0;layer<(unsigned)layers;layer++)for(unsigned variant=0;variant<3;variant++){
   OK(api_nvnTextureViewSetDefaults(&v));OK(api_nvnTextureViewSetLevels(&v,(int)level,variant==0?0:variant==1?1:6-(int)level));OK(api_nvnTextureViewSetLayers(&v,(int)layer,variant==0?0:variant==1?1:layers-(int)layer));
   ptrdiff_t want=(ptrdiff_t)(l->level[level].offset+layer*l->layer_stride);query(&texture,&v,want,0);
   NVNtextureView copy=v;memset(&v,0,sizeof(v));query(&texture,&copy,want,0);T(p->refs==refs);
   /* Address remains original-format/original-target even when the public
    * view requests a reinterpretation not admitted for registration/Draw. */
   OK(api_nvnTextureViewSetFormat(&copy,NVN_FORMAT_R32F));OK(api_nvnTextureViewSetTarget(&copy,NVN_TEXTURE_TARGET_BUFFER));query(&texture,&copy,want,0);
  }
  OK(api_nvnTextureViewSetDefaults(&v));OK(api_nvnTextureViewSetLevels(&v,1,1));query(&texture,&v,(ptrdiff_t)l->level[1].offset,0);
  size_t pins=p->refs;struct descriptor_entry before=find(&table,TPOOL)->slots[257];BAD(api_nvnTexturePoolRegisterTexture(&table,257,&texture,&v),ENOTSUP);T(p->refs==pins&&!memcmp(&before,find(&table,TPOOL)->slots+257,sizeof(before)));
  OK(api_nvnTextureFinalize(&texture));
 }
 /* SDK layout facts64x33 RGBA8 array3/mips7: layerStride22528,
  * mip2 offset18432; return63488, never pool_offset65536+63488. */
 setup(&b,&device,0,4,NVN_FORMAT_RGBA8,64,33,3,7);OK(api_nvnTextureBuilderSetStorage(&b,&pool,65536));T(api_nvnTextureInitialize(&texture,&b));T(!nn_nvn_error());
 NVNtextureView v={0};OK(api_nvnTextureViewSetDefaults(&v));OK(api_nvnTextureViewSetLevels(&v,2,0));OK(api_nvnTextureViewSetLayers(&v,2,0));query(&texture,&v,63488,0);
 struct object*t=find(&texture,TEXTURE);T(t->source_layout->layer_stride==22528&&t->source_layout->level[2].offset==18432);
 /* Legal public format/target enums are query metadata, not proof that the
  * setters/registration/GPU support all such views. Whitebox encode here
  * tests this independent getter without enlarging those API contracts. */
 struct resource_view encoded={.levels=1,.base_level=2,.level_count=1,.layers=1,.base_layer=2,.layer_count=1,.format=1,.target=1};
 for(unsigned format=0;format<=NVN_FORMAT_BGRA8_SRGB;format++)for(unsigned target=0;target<=NVN_TEXTURE_TARGET_BUFFER;target++){encoded.format_value=(int)format;encoded.target_value=(int)target;resource_view_write(&v,&encoded);query(&texture,&v,63488,0);}
 const struct resource_view invalid[]={
 {.levels=1,.base_level=-1},{.levels=1,.base_level=7},{.levels=1,.base_level=1,.level_count=INT_MAX},
 {.levels=1,.base_level=1,.level_count=-1},{.layers=1,.base_layer=-1},{.layers=1,.base_layer=3},
 {.layers=1,.base_layer=1,.layer_count=INT_MAX},{.layers=1,.base_layer=1,.layer_count=-1},
 {.format=1,.format_value=-1},{.format=1,.format_value=NVN_FORMAT_LARGE},{.target=1,.target_value=-1},{.target=1,.target_value=11},{.levels=2}};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){resource_view_write(&v,invalid+i);query(&texture,&v,0,EINVAL);}
 struct resource_view unset={.base_level=-1,.level_count=INT_MAX,.base_layer=-1,.layer_count=INT_MAX,.format_value=-1,.target_value=-1};resource_view_write(&v,&unset);query(&texture,&v,0,0);
 OK(api_nvnTextureViewSetDefaults(&v));((unsigned char*)&v)[sizeof(v)-1]=1;query(&texture,&v,0,EINVAL); /* native private-encoding boundary */
 struct nn_tex_source_layout saved=*t->source_layout;t->source_layout->payload_bytes++;query(&texture,NULL,0,EINVAL);*t->source_layout=saved;t->source_layout->layer_stride=UINT64_MAX;query(&texture,NULL,0,EINVAL);*t->source_layout=saved;
 NVNtexture unknown={0};query(&unknown,NULL,0,EINVAL);
 OK(api_nvnTextureViewSetDefaults(&v));OK(api_nvnTextureViewSetLevels(&v,2,1));OK(api_nvnTextureViewSetLayers(&v,2,1));error(EDOM);T(offset_query(&texture,&v)==63488&&nn_nvn_error()==EDOM);nn_nvn_clear_error();
 OK(api_nvnTextureFinalize(&texture));
 /* Actual finite native LINEAR/BUFFER resources retain their distinct byte
  * validators; the valid only-mip/layer start is0. BUF owner package unchanged. */
 for(unsigned buffer=0;buffer<2;buffer++){
  setup(&b,&device,buffer?0:NVN_TEXTURE_FLAGS_LINEAR,buffer?NVN_TEXTURE_TARGET_BUFFER:NVN_TEXTURE_TARGET_2D,NVN_FORMAT_RGBA8,buffer?8:2,buffer?1:2,1,1);if(!buffer)OK(api_nvnTextureBuilderSetStride(&b,256));OK(api_nvnTextureBuilderSetStorage(&b,&pool,512));T(api_nvnTextureInitialize(&texture,&b));T(!nn_nvn_error());
  query(&texture,NULL,0,0);OK(api_nvnTextureViewSetDefaults(&v));OK(api_nvnTextureViewSetLevels(&v,0,0));OK(api_nvnTextureViewSetLayers(&v,0,0));OK(api_nvnTextureViewSetFormat(&v,NVN_FORMAT_R32F));OK(api_nvnTextureViewSetTarget(&v,NVN_TEXTURE_TARGET_2D_ARRAY));query(&texture,&v,0,0);
  OK(api_nvnTextureViewSetLevels(&v,1,0));query(&texture,&v,0,EINVAL);OK(api_nvnTextureFinalize(&texture));
 }
 OK(api_nvnTexturePoolFinalize(&table));OK(api_nvnMemoryPoolFinalize(&pool));free(storage);OK(api_nvnDeviceFinalize(&device));while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"queries\":%u,\"gpu_submits\":0,\"scope\":\"HOST public source mip/layer offsets independent of GPU views; original Texture-relative addresses, no GPU/guest\"}\n",checks,queries);return 0;
}
