/* Actual public PFNs/core. HOST RAM only; no GPU or shader execution. */
#define _POSIX_C_SOURCE 200809L
#include NN_VIEW_MEMORY_BACKEND
#include NN_VIEW_CORE
static unsigned checks,combinations,consumer_rejections;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(e) do{T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
#define PFN(name,type) type p_##name=(type)nvnBootstrapLoader(#name);T(p_##name)
static void swiz_equal(const NVNtextureSwizzle out[4],unsigned r,unsigned g,unsigned b,unsigned a)
{T((unsigned)out[0]==r&&(unsigned)out[1]==g&&(unsigned)out[2]==b&&(unsigned)out[3]==a);}
static void consumer(struct object *texture,struct object *table,unsigned id)
{
 size_t refs=texture->pool->refs;unsigned bos=texture->device->gpu->bos;
 struct late_draw legacy={.texture_pool=table,.texture_id=id};T(gpu_late_draw_access(&legacy,"swizzle qualifier")==ENOTSUP);BAD(ENOTSUP);
 NVNprogram program={0};T(api_nvnProgramInitialize(&program,(NVNdevice*)texture->device->key));
 struct late_draw recorded={.target=texture,.program=find(&program,PROGRAM)};recorded.pipeline.texture_pool=table;recorded.pipeline.separate[1][0]=id;recorded.pipeline.guest[1][0]=id;recorded.pipeline.defined[1]=1;
 struct pipeline_prepared_draw *p=calloc(1,sizeof(*p));T(p);struct pipeline_resource_pins pins={0};p->recorded=&recorded;p->draw=recorded;p->pins=&pins;size_t index=0;
 T(pipeline_capture_domain(p,1,NN_PIPELINE_SEPARATE_TEXTURE,0,&index)==ENOTSUP);T(!p->entry_count&&!pins.count&&texture->pool->refs==refs&&texture->device->gpu->bos==bos);free(p);OK(api_nvnProgramFinalize(&program));consumer_rejections++;
}
int main(void)
{
 PFN(nvnTextureBuilderSetSwizzle,PFNNVNTEXTUREBUILDERSETSWIZZLEPROC);PFN(nvnTextureBuilderSetDepthStencilMode,PFNNVNTEXTUREBUILDERSETDEPTHSTENCILMODEPROC);
 PFN(nvnTextureBuilderGetSwizzle,PFNNVNTEXTUREBUILDERGETSWIZZLEPROC);PFN(nvnTextureBuilderGetDepthStencilMode,PFNNVNTEXTUREBUILDERGETDEPTHSTENCILMODEPROC);
 PFN(nvnTextureGetSwizzle,PFNNVNTEXTUREGETSWIZZLEPROC);PFN(nvnTextureGetDepthStencilMode,PFNNVNTEXTUREGETDEPTHSTENCILMODEPROC);
 PFN(nvnTextureViewSetSwizzle,PFNNVNTEXTUREVIEWSETSWIZZLEPROC);PFN(nvnTextureViewSetDepthStencilMode,PFNNVNTEXTUREVIEWSETDEPTHSTENCILMODEPROC);
 PFN(nvnTextureViewGetSwizzle,PFNNVNTEXTUREVIEWGETSWIZZLEPROC);PFN(nvnTextureViewGetDepthStencilMode,PFNNVNTEXTUREVIEWGETDEPTHSTENCILMODEPROC);PFN(nvnTextureViewCompare,PFNNVNTEXTUREVIEWCOMPAREPROC);
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));
 void *ram=NULL;T(!posix_memalign(&ram,4096,262144));memset(ram,0x67,262144);NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool={0};OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&device));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));OK(api_nvnMemoryPoolBuilderSetStorage(&pb,ram,262144));T(api_nvnMemoryPoolInitialize(&pool,&pb));
 NVNtexturePool tp={0};T(api_nvnTexturePoolInitialize(&tp,&pool,65536,320));struct object *table=find(&tp,TPOOL);
 NVNtextureBuilder tb={0};OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnTextureBuilderSetDevice(&tb,&device));NVNtextureSwizzle out[4];
 OK(p_nvnTextureBuilderGetSwizzle(&tb,out,out+1,out+2,out+3));swiz_equal(out,2,3,4,5);T(p_nvnTextureBuilderGetDepthStencilMode(&tb)==NVN_TEXTURE_DEPTH_STENCIL_MODE_DEPTH);
 OK(api_nvnTextureBuilderSetSize2D(&tb,2,2));OK(api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetStorage(&tb,&pool,0));NVNtexture texture={0};T(api_nvnTextureInitialize(&texture,&tb));struct object *t=find(&texture,TEXTURE);size_t bytes=t->size;
 /* Observation distinguishes unsupported packaged input from mip geometry,
  * while preserving diagnostics, private ownership and failed output. */
 struct nn_nvn_texture_builder_observation observation,saved_observation;
 size_t saved_refs=t->pool->refs;unsigned saved_bos=t->device->gpu->bos;
 T(!nn_nvn_get_texture_builder_observation(&tb,&observation));
 T(observation.fields==TEX_COMPLETE&&observation.device_live&&!observation.packaged&&
   observation.width==2&&observation.height==2&&!observation.source_layout_errno);
 NVNpackagedTextureLayout packaged={{1,2,3,4,5,6,7,8}};
 OK(api_nvnTextureBuilderSetPackagedTextureLayout(&tb,&packaged));
 T(!nn_nvn_get_texture_builder_observation(&tb,&observation)&&observation.packaged&&
   observation.source_layout_errno==ENOTSUP&&!memcmp(observation.packaged_layout,packaged.layout,8));
 error(EDOM);T(!nn_nvn_get_texture_builder_observation(&tb,&observation));BAD(EDOM);
 saved_observation=observation;NVNtextureBuilder missing_builder={0};
 T(nn_nvn_get_texture_builder_observation(&missing_builder,&observation)==ENOENT&&
   !memcmp(&observation,&saved_observation,sizeof(observation))&&!nn_nvn_error());
 T(nn_nvn_get_texture_builder_observation(&tb,NULL)==EINVAL&&!nn_nvn_error());
 OK(api_nvnTextureBuilderSetPackagedTextureLayout(&tb,NULL));
 T(t->pool->refs==saved_refs&&t->device->gpu->bos==saved_bos);
 NVNtextureView view={0},copy={0};OK(api_nvnTextureViewSetDefaults(&view));NVNtextureDepthStencilMode mode=99;
 T(!p_nvnTextureViewGetSwizzle(&view,out,out+1,out+2,out+3));swiz_equal(out,0,0,0,0);T(!p_nvnTextureViewGetDepthStencilMode(&view,&mode)&&mode==0);T(p_nvnTextureViewCompare(&view,&copy));T(!nn_nvn_error());
 /* All public swizzle components and both depth/stencil metadata values.
  * Each variant survives builder mutation, texture initialization, view copy,
  * override registration, independent source owner, and lifetime changes. */
 for(unsigned r=0;r<6;r++)for(unsigned g=0;g<6;g++)for(unsigned b=0;b<6;b++)for(unsigned a=0;a<6;a++){
  unsigned identity=r==2&&g==3&&b==4&&a==5;
  OK(p_nvnTextureBuilderSetSwizzle(&tb,r,g,b,a));OK(p_nvnTextureBuilderGetSwizzle(&tb,out,out+1,out+2,out+3));swiz_equal(out,r,g,b,a);
  for(unsigned m=0;m<2;m++){
   OK(p_nvnTextureBuilderSetDepthStencilMode(&tb,m));T((unsigned)p_nvnTextureBuilderGetDepthStencilMode(&tb)==m);
   NVNtexture declared={0};T(api_nvnTextureInitialize(&declared,&tb));T(api_nvnTextureGetStorageSize(&declared)==(int)bytes);OK(p_nvnTextureGetSwizzle(&declared,out,out+1,out+2,out+3));swiz_equal(out,r,g,b,a);T((unsigned)p_nvnTextureGetDepthStencilMode(&declared)==m);
   OK(p_nvnTextureBuilderSetSwizzle(&tb,2,3,4,5));OK(p_nvnTextureBuilderSetDepthStencilMode(&tb,0));OK(p_nvnTextureGetSwizzle(&declared,out,out+1,out+2,out+3));swiz_equal(out,r,g,b,a);T((unsigned)p_nvnTextureGetDepthStencilMode(&declared)==m);
   OK(api_nvnTexturePoolRegisterTexture(&tp,257,&declared,NULL));struct descriptor_entry *entry=table->slots+257;
   for(unsigned k=0;k<4;k++){T(texture_swizzle_component(&entry->snapshot.texture,k)==(unsigned)out[k]);}T(entry->snapshot.texture.depth_mode==m);
   if(!identity||m)consumer(t,table,257);
   T(api_nvnTextureCompare(&texture,&declared)==(identity&&!m));T(!nn_nvn_error());
   /* Explicit view overrides inherited custom resource metadata completely. */
   OK(api_nvnTextureViewSetDefaults(&view));OK(p_nvnTextureViewSetSwizzle(&view,2,3,4,5));OK(p_nvnTextureViewSetDepthStencilMode(&view,0));OK(api_nvnTexturePoolRegisterTexture(&tp,257,&declared,&view));T(texture_sampling_identity(&entry->snapshot.texture));int base=-1,count=-1;T(!api_nvnTextureViewGetLayers(&view,&base,&count));T(!base&&!count);T(!nn_nvn_error());
   OK(p_nvnTextureViewSetDepthStencilMode(&view,m));mode=99;T(p_nvnTextureViewGetDepthStencilMode(&view,&mode)&&(unsigned)mode==m);OK(api_nvnTexturePoolRegisterTexture(&tp,257,&declared,&view));T(entry->snapshot.texture.depth_mode==m);if(m)consumer(t,table,257);
   OK(api_nvnTextureFinalize(&declared));
   /* Reset custom builder for next mode after mutation above. */
   OK(p_nvnTextureBuilderSetSwizzle(&tb,r,g,b,a));
  }
  OK(api_nvnTextureViewSetDefaults(&view));OK(p_nvnTextureViewSetSwizzle(&view,r,g,b,a));T(p_nvnTextureViewGetSwizzle(&view,out,out+1,out+2,out+3));swiz_equal(out,r,g,b,a);copy=view;T(p_nvnTextureViewCompare(&view,&copy));OK(p_nvnTextureViewSetSwizzle(&copy,(r+1)%6,g,b,a));T(!p_nvnTextureViewCompare(&view,&copy));copy=view;
  OK(api_nvnTexturePoolRegisterTexture(&tp,258,&texture,&copy));struct descriptor_entry *entry=table->slots+258;T(entry->pool==t->pool&&entry->source_layout&&entry->offset==t->offset&&entry->size==t->size);
  memset(&view,0x89,sizeof(view));OK(p_nvnTextureViewGetSwizzle(&copy,out,out+1,out+2,out+3));swiz_equal(out,r,g,b,a);for(unsigned k=0;k<4;k++)T(texture_swizzle_component(&entry->snapshot.texture,k)==(unsigned)out[k]);if(!identity)consumer(t,table,258);
  combinations++;
 }
 /* Every independent presence class compares specified values only. */
 for(unsigned field=0;field<6;field++){
  OK(api_nvnTextureViewSetDefaults(&view));OK(api_nvnTextureViewSetDefaults(&copy));
  switch(field){
   case 0:api_nvnTextureViewSetLevels(&view,0,1);api_nvnTextureViewSetLevels(&copy,1,1);break;
   case 1:api_nvnTextureViewSetLayers(&view,0,1);api_nvnTextureViewSetLayers(&copy,1,1);break;
   case 2:api_nvnTextureViewSetFormat(&view,NVN_FORMAT_RGBA8);api_nvnTextureViewSetFormat(&copy,NVN_FORMAT_R32F);break;
   case 3:api_nvnTextureViewSetTarget(&view,NVN_TEXTURE_TARGET_2D);api_nvnTextureViewSetTarget(&copy,NVN_TEXTURE_TARGET_2D_ARRAY);break;
   case 4:p_nvnTextureViewSetSwizzle(&view,2,3,4,5);p_nvnTextureViewSetSwizzle(&copy,0,3,4,5);break;
   case 5:p_nvnTextureViewSetDepthStencilMode(&view,0);p_nvnTextureViewSetDepthStencilMode(&copy,1);break;
  }
  T(!nn_nvn_error());T(!p_nvnTextureViewCompare(&view,&copy));copy=view;T(p_nvnTextureViewCompare(&view,&copy));
  ((unsigned char *)&copy)[39]=0xdf;T(p_nvnTextureViewCompare(&view,&copy));T(!nn_nvn_error());
 }
 /* Unspecified payload and private padding are not comparison semantics. */
 OK(api_nvnTextureViewSetDefaults(&view));copy=view;struct resource_view dirty={0};dirty.base_level=19;dirty.level_count=7;dirty.base_layer=3;dirty.layer_count=4;dirty.format_value=0x9c;dirty.target_value=10;memset(dirty.swizzle_value,5,4);dirty.mode_value=1;memcpy(&copy,&dirty,sizeof(dirty));((unsigned char *)&copy)[39]=0xa9;T(p_nvnTextureViewCompare(&view,&copy));T(!nn_nvn_error());
 OK(api_nvnTextureViewSetDefaults(&copy));OK(p_nvnTextureViewSetSwizzle(&copy,0,0,0,0));T(!p_nvnTextureViewCompare(&view,&copy));T(!p_nvnTextureViewGetSwizzle(&view,out,out+1,out+2,out+3));swiz_equal(out,0,0,0,0);T(p_nvnTextureViewGetSwizzle(&copy,out,out+1,out+2,out+3));swiz_equal(out,0,0,0,0);
 OK(api_nvnTextureViewSetDefaults(&view));OK(p_nvnTextureViewSetDepthStencilMode(&copy,0));T(!p_nvnTextureViewCompare(&view,&copy));
 /* Atomic invalid-enum rejection and exact presence retention. */
 NVNtextureView before=copy;for(unsigned i=0;i<4;i++){NVNtextureSwizzle s[4]={2,3,4,5};s[i]=(NVNtextureSwizzle)(i?6:-1);p_nvnTextureViewSetSwizzle(&copy,s[0],s[1],s[2],s[3]);BAD(EINVAL);T(!memcmp(&before,&copy,sizeof(copy)));}
 p_nvnTextureViewSetDepthStencilMode(&copy,(NVNtextureDepthStencilMode)2);BAD(EINVAL);T(!memcmp(&before,&copy,sizeof(copy)));
 OK(api_nvnTextureBuilderSetDefaults(&tb));OK(p_nvnTextureBuilderGetSwizzle(&tb,out,out+1,out+2,out+3));swiz_equal(out,2,3,4,5);p_nvnTextureBuilderSetSwizzle(&tb,6,0,0,0);BAD(EINVAL);OK(p_nvnTextureBuilderGetSwizzle(&tb,out,out+1,out+2,out+3));swiz_equal(out,2,3,4,5);p_nvnTextureBuilderSetDepthStencilMode(&tb,(NVNtextureDepthStencilMode)-1);BAD(EINVAL);T(p_nvnTextureBuilderGetDepthStencilMode(&tb)==0);
 p_nvnTextureBuilderGetSwizzle(&tb,NULL,out+1,out+2,out+3);BAD(EINVAL);T(!p_nvnTextureViewGetSwizzle(&copy,NULL,out+1,out+2,out+3));BAD(EINVAL);T(!p_nvnTextureViewCompare(NULL,&copy));BAD(EINVAL);
 OK(api_nvnTexturePoolFinalize(&tp));OK(api_nvnTextureFinalize(&texture));OK(api_nvnMemoryPoolFinalize(&pool));OK(api_nvnDeviceFinalize(&device));for(size_t i=0;i<65536;i++)T(((unsigned char *)ram)[i]==0x67);free(ram);while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"swizzle_combinations\":%u,\"subset_consumer_rejections\":%u,\"GPU_submissions\":0}\n",checks,combinations,consumer_rejections);return 0;
}
