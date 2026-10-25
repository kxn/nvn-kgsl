/* Public NVN registration + actual canonical owner core, RAM backend only.
 * No shader/GPU/CP execution. Private gates are checked before any GPU work. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_cache view_ram_cache
#include NN_VIEW_MEMORY_BACKEND
#undef nn_bo_cache
static int fail_cache,fail_malloc;
static unsigned caches,alloc_failures;
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int from)
{caches++;if(fail_cache){fail_cache=0;return EIO;}return view_ram_cache(b,o,n,from);}
static void *view_malloc(size_t bytes)
{if(fail_malloc){fail_malloc=0;alloc_failures++;return NULL;}return malloc(bytes);}
#define malloc view_malloc
#include NN_VIEW_CORE
#undef malloc
static unsigned checks,registrations,subset_gates;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s sticky %d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(e) do{T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static NVNdevice d;static NVNdeviceBuilder db;static NVNmemoryPool pool,table_pool;static NVNmemoryPoolBuilder pb;static NVNtextureBuilder tb;static NVNtexturePool table;
static PFNNVNTEXTUREPOOLREGISTERTEXTUREPROC reg;
static void setup_texture_builder(int flags,int target,int w,int h,int depth,int levels,int format)
{api_nvnTextureBuilderSetDefaults(&tb);api_nvnTextureBuilderSetDevice(&tb,&d);api_nvnTextureBuilderSetFlags(&tb,flags);api_nvnTextureBuilderSetTarget(&tb,target);api_nvnTextureBuilderSetSize3D(&tb,w,h,depth);api_nvnTextureBuilderSetLevels(&tb,levels);api_nvnTextureBuilderSetFormat(&tb,format);}
static void verify_selection(struct object *t,struct descriptor_entry *entry,unsigned bl,unsigned nl,unsigned ba,unsigned na)
{
 const struct nn_tex_source_layout *s=t->source_layout;
 unsigned layers=t->texture.target==NVN_TEXTURE_TARGET_2D_ARRAY?(unsigned)t->texture.depth:1;
 T(entry->pool==t->pool&&entry->offset==t->offset&&entry->size==t->size);
 T(entry->source_layout!=s&&!memcmp(entry->source_layout,s,sizeof(*s)));
 T(!memcmp(&entry->snapshot.texture,&t->texture,sizeof(t->texture)));
 T(entry->selection.base_level==bl&&entry->selection.level_count==nl&&entry->selection.base_layer==ba&&entry->selection.layer_count==na);
 T(entry->selection.source_offset==s->level[bl].offset+s->layer_stride*ba);
 T(entry->selection.subset==(bl||ba||nl!=(unsigned)t->texture.levels||na!=layers));
}
static void reject_subset_consumer(struct object *t,unsigned id)
{
 struct object *tp=find(&table,TPOOL);size_t refs=t->pool->refs;unsigned bos=t->device->gpu->bos;
 struct late_draw legacy={.texture_pool=tp,.texture_id=id};T(gpu_late_draw_access(&legacy,"subset qualifier")==ENOTSUP);BAD(ENOTSUP);
 NVNprogram program={0};T(api_nvnProgramInitialize(&program,(NVNdevice*)t->device->key));
 struct late_draw recorded={.target=t,.program=find(&program,PROGRAM)};recorded.pipeline.texture_pool=tp;recorded.pipeline.separate[1][0]=id;recorded.pipeline.guest[1][0]=id;recorded.pipeline.defined[1]=1;
 struct pipeline_prepared_draw *p=calloc(1,sizeof(*p));T(p);struct pipeline_resource_pins pins={0};p->recorded=&recorded;p->draw=recorded;p->pins=&pins;size_t index=0;
 T(pipeline_capture_domain(p,1,NN_PIPELINE_SEPARATE_TEXTURE,0,&index)==ENOTSUP);
 T(!p->entry_count&&!pins.count&&t->pool->refs==refs&&t->device->gpu->bos==bos);free(p);OK(api_nvnProgramFinalize(&program));subset_gates++;
}
static void unchanged(struct object *tp,unsigned id,const struct descriptor_entry *saved,const unsigned char raw[32],size_t refs)
{T(!memcmp(tp->slots+id,saved,sizeof(*saved)));T(tp->slots[id].pool->refs==refs);T(!memcmp(raw,(char *)nn_bo_map(tp->pool->bo)+tp->offset+(size_t)id*32,32));}
int main(void)
{
 _Static_assert(NVN_FORMAT_RGBA_DXT1_SRGB==0x46,"real game format70 is BC1 RGBA sRGB");
 OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&d,&db));reg=(PFNNVNTEXTUREPOOLREGISTERTEXTUREPROC)nvnBootstrapLoader("nvnTexturePoolRegisterTexture");T(reg);
 void *ram=NULL,*descriptors=NULL;T(!posix_memalign(&ram,4096,4u*1024*1024));T(!posix_memalign(&descriptors,4096,65536));memset(ram,0x4c,4u*1024*1024);memset(descriptors,0x38,65536);
 OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&d));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));OK(api_nvnMemoryPoolBuilderSetStorage(&pb,ram,4u*1024*1024));T(api_nvnMemoryPoolInitialize(&pool,&pb));OK(api_nvnMemoryPoolBuilderSetStorage(&pb,descriptors,65536));T(api_nvnMemoryPoolInitialize(&table_pool,&pb));T(api_nvnTexturePoolInitialize(&table,&table_pool,0,320));struct object *tp=find(&table,TPOOL);
 const int cases[][7]={{0,4,128,128,84,7,0x46},{0,4,20,13,3,5,0x25},{0,1,127,73,1,7,0x25},{4,2,32,32,32,1,0x29},{4,2,32,32,32,6,0x29}};
 for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);c++){
  const int *g=cases[c];setup_texture_builder(g[0],g[1],g[2],g[3],g[4],g[5],g[6]);T(!nn_nvn_error());size_t bytes=api_nvnTextureBuilderGetStorageSize(&tb);T(bytes>0);if(!c)T(bytes==1204224);if(c==3)T(bytes==262144);OK(api_nvnTextureBuilderSetStorage(&tb,&pool,512));NVNtexture texture={0};T(api_nvnTextureInitialize(&texture,&tb));struct object *t=find(&texture,TEXTURE);unsigned levels=(unsigned)t->texture.levels,layers=t->texture.target==NVN_TEXTURE_TARGET_2D_ARRAY?(unsigned)t->texture.depth:1;
  OK(reg(&table,293,&texture,NULL));registrations++;verify_selection(t,tp->slots+293,0,levels,0,layers);
  NVNtextureView v={0};OK(api_nvnTextureViewSetDefaults(&v));OK(reg(&table,294,&texture,&v));registrations++;verify_selection(t,tp->slots+294,0,levels,0,layers);
  /* The exact new game boundary: base0/count1 on a full84-layer source. */
  OK(api_nvnTextureViewSetLayers(&v,0,1));OK(reg(&table,295,&texture,&v));registrations++;verify_selection(t,tp->slots+295,0,levels,0,1);if(layers>1)reject_subset_consumer(t,295);
  for(unsigned l=0;l<levels;l++)for(unsigned a=0;a<layers;a++)for(unsigned remaining=0;remaining<2;remaining++){
   OK(api_nvnTextureViewSetDefaults(&v));OK(api_nvnTextureViewSetLevels(&v,(int)l,remaining?0:1));OK(api_nvnTextureViewSetLayers(&v,(int)a,remaining?0:1));OK(api_nvnTextureViewSetFormat(&v,(NVNformat)g[6]));OK(api_nvnTextureViewSetTarget(&v,(NVNtextureTarget)g[1]));size_t refs=t->pool->refs;unsigned bos=t->device->gpu->bos;
   OK(reg(&table,295,&texture,&v));registrations++;verify_selection(t,tp->slots+295,l,remaining?levels-l:1,a,remaining?layers-a:1);T(t->pool->refs==refs&&t->device->gpu->bos==bos);
   if(tp->slots[295].selection.subset&&a==layers-1)reject_subset_consumer(t,295);
   struct descriptor_selection stored=tp->slots[295].selection;NVNtextureView moved=v;memset(&v,0x77,sizeof(v));T(!memcmp(&tp->slots[295].selection,&stored,sizeof(stored)));OK(reg(&table,295,&texture,&moved));registrations++;
  }
  /* Valid normalized selected metadata survives mutation of the view and
   * resource builder. Failure publication keeps the prior entire entry. */
  OK(api_nvnTextureViewSetDefaults(&v));OK(api_nvnTextureViewSetLevels(&v,0,1));OK(api_nvnTextureViewSetLayers(&v,0,1));OK(reg(&table,295,&texture,&v));registrations++;
  struct descriptor_entry saved=tp->slots[295];unsigned char raw[32];memcpy(raw,(char *)descriptors+295*32,32);size_t refs=t->pool->refs;
  for(unsigned bad=0;bad<8;bad++){
   OK(api_nvnTextureViewSetDefaults(&v));int expected=EINVAL;
   switch(bad){case 0:api_nvnTextureViewSetLevels(&v,(int)levels,1);break;case 1:api_nvnTextureViewSetLevels(&v,(int)levels-1,2);break;case 2:api_nvnTextureViewSetLayers(&v,(int)layers,1);break;case 3:api_nvnTextureViewSetLayers(&v,(int)layers-1,2);break;case 4:api_nvnTextureViewSetFormat(&v,g[6]==0x25?NVN_FORMAT_R32F:NVN_FORMAT_RGBA8);expected=ENOTSUP;break;case 5:api_nvnTextureViewSetTarget(&v,NVN_TEXTURE_TARGET_BUFFER);expected=ENOTSUP;break;case 6:((unsigned char *)&v)[sizeof(v)-1]=1;break;case 7:api_nvnTextureViewSetLevels(&v,INT_MAX,0);break;}
   T(!nn_nvn_error());reg(&table,295,&texture,&v);BAD(expected);unchanged(tp,295,&saved,raw,refs);
  }
  OK(api_nvnTextureViewSetDefaults(&v));OK(api_nvnTextureViewSetLayers(&v,0,1));fail_malloc=1;reg(&table,295,&texture,&v);BAD(ENOMEM);unchanged(tp,295,&saved,raw,refs);fail_cache=1;reg(&table,295,&texture,&v);BAD(EIO);unchanged(tp,295,&saved,raw,refs);
  OK(api_nvnTextureFinalize(&texture));T(tp->slots[295].pool==find(&pool,POOL)&&tp->slots[295].source_layout);T(!memcmp(&tp->slots[295].selection,&saved.selection,sizeof(saved.selection)));
  api_nvnMemoryPoolFinalize(&pool);BAD(EBUSY);
 }
 unsigned same=1;for(size_t i=0;i<4u*1024*1024;i++)if(((unsigned char *)ram)[i]!=0x4c){same=0;break;}T(same);T(alloc_failures==5);
 OK(api_nvnTexturePoolFinalize(&table));OK(api_nvnMemoryPoolFinalize(&table_pool));OK(api_nvnMemoryPoolFinalize(&pool));OK(api_nvnDeviceFinalize(&d));free(ram);free(descriptors);while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"registrations\":%u,\"subset_consumer_rejections\":%u,\"original_game_shape\":\"RGBA_DXT1_SRGB 128x128 array84 mip7\",\"GPU_submissions\":0,\"caches\":%u}\n",checks,registrations,subset_gates,caches);return 0;
}
