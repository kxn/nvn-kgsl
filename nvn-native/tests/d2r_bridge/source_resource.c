/* SPDX-License-Identifier: MIT. HOST ONLY typed API / real canonical BO owner
 * and strict GPU-boundary checks. No shader/GPU/CP execution or pixel copy. */
#define _POSIX_C_SOURCE 200809L
#include NN_RESOURCE_MEMORY_BACKEND
static int fail_malloc;
static void *checked_malloc(size_t n){if(fail_malloc){fail_malloc=0;return NULL;}return malloc(n);}
#define malloc checked_malloc
#include NN_RESOURCE_CORE
#undef malloc
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define GOOD() CHECK(!nn_nvn_error())
#define BAD(x) do{CHECK(nn_nvn_error()==(x));nn_nvn_clear_error();}while(0)
static NVNdevice d;static NVNdeviceBuilder db;static NVNmemoryPool p;static NVNmemoryPoolBuilder pb;static NVNtextureBuilder tb;
static void setup(int flags,int target,int w,int h,int layers,int levels,int format)
{
 api_nvnTextureBuilderSetDefaults(&tb);api_nvnTextureBuilderSetDevice(&tb,&d);
 api_nvnTextureBuilderSetFlags(&tb,flags);api_nvnTextureBuilderSetTarget(&tb,target);
 api_nvnTextureBuilderSetSize3D(&tb,w,h,layers);api_nvnTextureBuilderSetLevels(&tb,levels);api_nvnTextureBuilderSetFormat(&tb,format);
}
static int copy_regions(struct object *t)
{
 const struct nn_tex_source_layout *s=t->source_layout;
 size_t refs=t->pool->refs,bos=t->device->gpu->bos;
 for(unsigned level=0;level<s->levels;level++){
  const struct nn_tex_source_level *m=s->level+level;
  NVNtextureView view;api_nvnTextureViewSetDefaults(&view);api_nvnTextureViewSetLevels(&view,(int)level,1);api_nvnTextureViewSetLayers(&view,0,0);GOOD();
  api_nvnTextureViewSetFormat(&view,t->texture.format);GOOD();
  NVNtextureView saved_view=view;
  api_nvnTextureViewSetFormat(&view,(NVNformat)-1);BAD(EINVAL);CHECK(!memcmp(&view,&saved_view,sizeof(view)));
  api_nvnTextureViewSetFormat(&view,(NVNformat)(NVN_FORMAT_BGRA8_SRGB+1));BAD(EINVAL);CHECK(!memcmp(&view,&saved_view,sizeof(view)));
  NVNcopyRegion r={0,0,0,(int)m->width,(int)m->height,2};
  struct canonical_copy_region p;
  CHECK(!canonical_copy_region(t,&view,&r,0,0,&p));
  CHECK(p.level==level&&!p.base_layer&&p.layers==2&&!p.x_bytes&&!p.y_rows&&p.row_bytes==m->row_bytes&&p.rows==m->blocks_y);
  CHECK(p.linear_row_stride==m->row_bytes&&p.linear_image_stride==m->row_bytes*m->blocks_y&&p.linear_span==2u*(uint64_t)m->row_bytes*m->blocks_y);
  uint32_t pitch=m->row_bytes+1,image=(m->blocks_y-1)*pitch+m->row_bytes+3;
  CHECK(!canonical_copy_region(t,&view,&r,pitch,image,&p));
  uint32_t active_pitch=m->blocks_y>1?pitch:m->row_bytes;
  CHECK(p.linear_row_stride==active_pitch&&p.linear_image_stride==image&&p.linear_span==(uint64_t)image+(m->blocks_y-1)*active_pitch+m->row_bytes);
  CHECK(canonical_copy_region(t,&view,&r,-1,0,&p)==EINVAL);
  CHECK(canonical_copy_region(t,&view,&r,PTRDIFF_MAX,0,&p)==EOVERFLOW);
  CHECK(canonical_copy_region(t,&view,&r,0,PTRDIFF_MAX,&p)==EOVERFLOW);
  r.zoffset=1;r.depth=1;CHECK(!canonical_copy_region(t,&view,&r,0,-1,&p)&&p.base_layer==1&&p.layers==1);
  if(m->blocks_y==1)CHECK(!canonical_copy_region(t,&view,&r,-1,-1,&p)&&p.linear_row_stride==m->row_bytes);
  else CHECK(canonical_copy_region(t,&view,&r,-1,0,&p)==EINVAL);
  api_nvnTextureViewSetTarget(&view,NVN_TEXTURE_TARGET_BUFFER);GOOD();
  CHECK(!canonical_copy_region(t,&view,&r,0,0,&p)&&p.base_layer==1&&p.level==level);
  api_nvnTextureViewSetTarget(&view,NVN_TEXTURE_TARGET_2D_ARRAY);GOOD();
  if(m->width>s->block_w&&m->height>s->block_h){
   NVNcopyRegion partial={(int)s->block_w,(int)s->block_h,0,(int)(m->width-s->block_w),(int)(m->height-s->block_h),1};
   CHECK(!canonical_copy_region(t,&view,&partial,0,0,&p));
   CHECK(p.x_bytes==s->bytes_per_block&&p.y_rows==1&&p.row_bytes==m->row_bytes-s->bytes_per_block&&p.rows==m->blocks_y-1);
  }
  if(s->block_w>1&&m->width>1){NVNcopyRegion bad=r;bad.xoffset=1;bad.width=(int)m->width-1;CHECK(canonical_copy_region(t,&view,&bad,0,0,&p)==ENOTSUP);}
  NVNcopyRegion bad=r;bad.xoffset=-1;CHECK(canonical_copy_region(t,&view,&bad,0,0,&p)==EINVAL);
  bad=r;bad.width=INT_MAX;CHECK(canonical_copy_region(t,&view,&bad,0,0,&p)==EINVAL);
  bad=r;bad.depth=2;CHECK(canonical_copy_region(t,&view,&bad,0,0,&p)==EINVAL);
  NVNtextureView sub;memcpy(&sub,&view,sizeof(sub));api_nvnTextureViewSetLayers(&sub,1,1);GOOD();
  r.zoffset=0;CHECK(!canonical_copy_region(t,&sub,&r,0,0,&p)&&p.base_layer==1);
  api_nvnTextureViewSetFormat(&sub,t->texture.format==NVN_FORMAT_RGBA8?NVN_FORMAT_R32F:NVN_FORMAT_RGBA8);GOOD();
  CHECK(canonical_copy_region(t,&sub,&r,0,0,&p)==ENOTSUP);
  memset(&p,0xa6,sizeof(p));CHECK(canonical_copy_region(t,&view,NULL,0,0,&p)==EINVAL);
  struct canonical_copy_region zero={0};CHECK(!memcmp(&p,&zero,sizeof(p)));
  CHECK(t->pool->refs==refs&&t->device->gpu->bos==bos);GOOD();
 }
 return 0;
}
int main(void)
{
 api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&d,&db));GOOD();
 CHECK(find(&d,DEVICE)->texture_source_policy==1);
 size_t poolbytes=40u*1024*1024;void *storage=NULL;CHECK(!posix_memalign(&storage,4096,poolbytes));memset(storage,0x5b,poolbytes);
 api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&d);api_nvnMemoryPoolBuilderSetStorage(&pb,storage,poolbytes);api_nvnMemoryPoolBuilderSetFlags(&pb,0xa4);
 CHECK(api_nvnMemoryPoolInitialize(&p,&pb));GOOD();struct object *pool=find(&p,POOL);CHECK(nn_bo_map(pool->bo)==storage);
 PFNNVNTEXTUREBUILDERGETSTORAGECLASSPROC bc=(PFNNVNTEXTUREBUILDERGETSTORAGECLASSPROC)nvnBootstrapLoader("nvnTextureBuilderGetStorageClass");
 PFNNVNTEXTUREGETSTORAGECLASSPROC tc=(PFNNVNTEXTUREGETSTORAGECLASSPROC)nvnBootstrapLoader("nvnTextureGetStorageClass");CHECK(bc&&tc);
 setup(9,1,2880,1620,1,1,0x25);CHECK(api_nvnTextureBuilderGetStorageSize(&tb)==19169280);CHECK(api_nvnTextureBuilderGetStorageAlignment(&tb)==512);CHECK(bc(&tb)==0x1fe);GOOD();
 CHECK(api_nvnTextureBuilderGetFlags(&tb)==9&&api_nvnTextureBuilderGetStride(&tb)==0);
 api_nvnTextureBuilderSetStorage(&tb,&p,512);GOOD();NVNtexture t={0};size_t refs=pool->refs,bos=pool->device->gpu->bos;
 fail_malloc=1;CHECK(!api_nvnTextureInitialize(&t,&tb));BAD(ENOMEM);CHECK(pool->refs==refs&&pool->device->gpu->bos==bos);
 CHECK(api_nvnTextureInitialize(&t,&tb));GOOD();struct object *source=find(&t,TEXTURE);
 CHECK(source->source_layout&&source->pool==pool&&source->bo==pool->bo&&source->offset==512&&source->size==19169280);
 CHECK(pool->refs==refs+1&&pool->device->gpu->bos==bos);CHECK(api_nvnTextureGetFlags(&t)==9&&tc(&t)==0x1fe);CHECK(api_nvnTextureGetRawStorageClass(&t)==2);GOOD();
 CHECK(api_nvnTextureGetTextureAddress(&t)==(uintptr_t)storage+512);CHECK(api_nvnTextureGetStorageSize(&t)==19169280);CHECK(!api_nvnTextureGetStride(&t));
 CHECK(!api_nvnTextureInitialize(&t,&tb));BAD(EEXIST);CHECK(pool->refs==refs+1);
 api_nvnMemoryPoolFinalize(&p);BAD(EBUSY);api_nvnDeviceFinalize(&d);BAD(EBUSY);
 /* Builder changes cannot mutate an initialized object's canonical layout. */
 api_nvnTextureBuilderSetFlags(&tb,0);api_nvnTextureBuilderSetWidth(&tb,64);CHECK(api_nvnTextureGetFlags(&t)==9&&api_nvnTextureGetWidth(&t)==2880);
 CHECK(source->source_layout->level[0].block_height_log2==4&&source->source_layout->level[0].pitch==11520);
 unsigned policy=find(&d,DEVICE)->texture_source_policy;find(&d,DEVICE)->texture_source_policy=0;CHECK(!api_nvnTextureBuilderGetStorageSize(&tb));BAD(ENOTSUP);find(&d,DEVICE)->texture_source_policy=policy;
 setup(8,1,32,8,1,1,0x25);int flags=pool->flags;pool->flags=0x24;api_nvnTextureBuilderSetStorage(&tb,&p,24u*1024*1024);NVNtexture other={0};CHECK(!api_nvnTextureInitialize(&other,&tb));BAD(ENOTSUP);CHECK(pool->refs==refs+1);pool->flags=flags;
 setup(0,4,64,33,3,7,0x25);CHECK(api_nvnTextureBuilderGetStorageSize(&tb)==67584);CHECK(api_nvnTextureBuilderGetStorageAlignment(&tb)==512);api_nvnTextureBuilderSetStorage(&tb,&p,24u*1024*1024);CHECK(api_nvnTextureInitialize(&other,&tb));GOOD();
 CHECK(find(&other,TEXTURE)->source_layout->layer_stride==22528);CHECK(api_nvnTextureGetDepth(&other)==3&&api_nvnTextureGetLevels(&other)==7);api_nvnTextureFinalize(&other);GOOD();
 /* Exact original-v15 frontier: IMAGE|COMPRESSIBLE packed HDR uses this
  * device's explicit Genericfe uncompressed policy, preserving both flags. */
 setup(12,1,2880,1620,1,1,NVN_FORMAT_R11G11B10F);
 CHECK(api_nvnTextureBuilderGetStorageSize(&tb)==19169280&&api_nvnTextureBuilderGetStorageAlignment(&tb)==512&&bc(&tb)==0x1fe);GOOD();
 api_nvnTextureBuilderSetStorage(&tb,&p,512);CHECK(api_nvnTextureInitialize(&other,&tb));GOOD();
 CHECK(api_nvnTextureGetFlags(&other)==12&&api_nvnTextureGetFormat(&other)==NVN_FORMAT_R11G11B10F);
 CHECK(find(&other,TEXTURE)->source_layout->flags==4&&api_nvnTextureGetRawStorageClass(&other)==2);
 api_nvnTextureFinalize(&other);GOOD();
 /* CPU descriptor registration owns the complete canonical snapshot even
  * when no native conversion/consumer supports its format or subresources. */
 NVNtexturePool tp={0};CHECK(api_nvnTexturePoolInitialize(&tp,&p,30u*1024*1024,260));GOOD();struct object *table=find(&tp,TPOOL);
 const int formats[]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
  0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x38,
  0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x40,
  0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x9a,0x9c};
 for(unsigned i=0;i<sizeof(formats)/sizeof(*formats);i++){
  setup(8,4,20,13,2,5,formats[i]);CHECK(api_nvnTextureBuilderGetStorageSize(&tb)>0&&bc(&tb)==0x1fe);api_nvnTextureBuilderSetStorage(&tb,&p,24u*1024*1024);CHECK(api_nvnTextureInitialize(&other,&tb));CHECK(api_nvnTextureGetFlags(&other)==8&&api_nvnTextureGetRawStorageClass(&other)==2);
  struct object *value=find(&other,TEXTURE);struct nn_tex_source_layout snapshot=*value->source_layout;
  CHECK(!copy_regions(value));
  api_nvnTexturePoolRegisterTexture(&tp,257,&other,NULL);GOOD();
  CHECK(table->slots[257].pool==pool&&table->slots[257].source_layout!=value->source_layout);
  CHECK(!memcmp(table->slots[257].source_layout,&snapshot,sizeof(snapshot)));
  CHECK(table->slots[257].snapshot.texture.format==formats[i]&&table->slots[257].snapshot.texture.levels==5&&table->slots[257].snapshot.texture.depth==2);
  NVNtextureView view={0};api_nvnTextureViewSetDefaults(&view);api_nvnTextureViewSetFormat(&view,(NVNformat)formats[i]);api_nvnTexturePoolRegisterTexture(&tp,258,&other,&view);GOOD();
  struct descriptor_entry saved=table->slots[258];size_t pins=pool->refs;unsigned char raw[32];memcpy(raw,(char *)storage+30u*1024*1024+258*32,32);
  api_nvnTextureViewSetLevels(&view,5,1);api_nvnTexturePoolRegisterTexture(&tp,258,&other,&view);BAD(EINVAL);
  CHECK(!memcmp(&saved,table->slots+258,sizeof(saved))&&pool->refs==pins&&!memcmp(raw,(char *)storage+30u*1024*1024+258*32,32));
  api_nvnTextureViewSetDefaults(&view);api_nvnTextureViewSetLayers(&view,2,1);api_nvnTexturePoolRegisterTexture(&tp,258,&other,&view);BAD(EINVAL);
  CHECK(!memcmp(&saved,table->slots+258,sizeof(saved))&&pool->refs==pins);
  /* Full explicit zero-count views retain the original whole resource. */
  api_nvnTextureViewSetLevels(&view,0,0);api_nvnTextureViewSetLayers(&view,0,0);api_nvnTexturePoolRegisterTexture(&tp,258,&other,&view);GOOD();
  api_nvnTextureViewSetLevels(&view,0,5);api_nvnTextureViewSetLayers(&view,0,2);api_nvnTexturePoolRegisterTexture(&tp,258,&other,&view);GOOD();
  CHECK(!memcmp(table->slots[258].source_layout,&snapshot,sizeof(snapshot)));
  CHECK(gpu_object_access(value,"registered unsupported native resource")==ENOTSUP);BAD(ENOTSUP);
  api_nvnTextureFinalize(&other);GOOD();CHECK(!memcmp(table->slots[257].source_layout,&snapshot,sizeof(snapshot))&&table->slots[257].pool==pool);
 }
 /* SDK table facts for all six depth/stencil storage encodings. No depth
  * clears/draws are enabled by constructor/registration metadata support. */
 const int depth_bytes[]={1,2,4,4,4,8},depth_raw[]={3,3,4,2,4,5};
 for(unsigned i=0;i<6;i++){
  setup(8,4,20,13,2,5,NVN_FORMAT_STENCIL8+(int)i);
  CHECK(api_nvnTextureBuilderGetStorageSize(&tb)>0&&bc(&tb)==0x1fe);GOOD();
  api_nvnTextureBuilderSetStorage(&tb,&p,24u*1024*1024);CHECK(api_nvnTextureInitialize(&other,&tb));GOOD();
  struct object *value=find(&other,TEXTURE);CHECK(value->source_layout->bytes_per_block==(unsigned)depth_bytes[i]);
  CHECK(api_nvnTextureGetRawStorageClass(&other)==depth_raw[i]);GOOD();CHECK(!copy_regions(value));
  api_nvnTexturePoolRegisterTexture(&tp,257,&other,NULL);GOOD();
  CHECK(table->slots[257].snapshot.texture.format==NVN_FORMAT_STENCIL8+(int)i);
  CHECK(gpu_object_access(value,"depth metadata grants no consumer")==ENOTSUP);BAD(ENOTSUP);
  api_nvnTextureFinalize(&other);GOOD();
 }
 /* Public buffer alias names the same canonical bytes, never a native shadow. */
 NVNbufferBuilder bb={0};NVNbuffer alias={0};api_nvnBufferBuilderSetDefaults(&bb);api_nvnBufferBuilderSetDevice(&bb,&d);api_nvnBufferBuilderSetStorage(&bb,&p,512,64);CHECK(api_nvnBufferInitialize(&alias,&bb));CHECK(api_nvnBufferGetAddress(&alias)==api_nvnTextureGetTextureAddress(&t));CHECK(api_nvnBufferMap(&alias)==(unsigned char *)storage+512);api_nvnBufferFinalize(&alias);GOOD();
 /* Explicit sourcekind does not relax malformed, sparse, packaged or MSAA. */
 const int badflags[]={2,16,32,64,128,0x100,0x1000,-1};for(unsigned i=0;i<sizeof(badflags)/sizeof(*badflags);i++){
  setup(badflags[i],1,32,8,1,1,0x25);CHECK(!api_nvnTextureBuilderGetStorageSize(&tb));BAD(ENOTSUP);
 }
 setup(9,4,64,64,2,1,0x25);CHECK(!api_nvnTextureBuilderGetStorageSize(&tb));BAD(ENOTSUP);
 setup(8,1,32,8,1,1,0x25);NVNpackagedTextureLayout packaged={{1}};api_nvnTextureBuilderSetPackagedTextureLayout(&tb,&packaged);CHECK(!api_nvnTextureBuilderGetStorageSize(&tb));BAD(ENOTSUP);api_nvnTextureBuilderSetPackagedTextureLayout(&tb,NULL);
 api_nvnTextureBuilderSetSamples(&tb,2);CHECK(!api_nvnTextureBuilderGetStorageSize(&tb));BAD(ENOTSUP);
 setup(0,1,32,8,1,1,0x25);api_nvnTextureBuilderSetStorage(&tb,&p,24u*1024*1024+64);CHECK(!api_nvnTextureInitialize(&other,&tb));BAD(EINVAL);
 api_nvnTextureBuilderSetStorage(&tb,&p,(ptrdiff_t)poolbytes-512);BAD(EINVAL); /* exact range checked when shape known */
 /* Legacy LINEAR backend sizing/pitch remains unchanged. */
 setup(0x10,1,2,2,1,1,0x25);api_nvnTextureBuilderSetStride(&tb,256);CHECK(api_nvnTextureBuilderGetStorageSize(&tb)==4096&&api_nvnTextureBuilderGetStorageAlignment(&tb)==64);api_nvnTextureBuilderSetStorage(&tb,&p,28u*1024*1024);NVNtexture linear={0};CHECK(api_nvnTextureInitialize(&linear,&tb));CHECK(!find(&linear,TEXTURE)->source_layout);CHECK(api_nvnTextureGetRawStorageClass(&linear)==1);GOOD();CHECK(!tc(&linear));BAD(ENOTSUP);
 /* All unconverted source texture consumers reject without bytes/ref changes. */
 NVNcommandBuffer cmd={0};unsigned char control[4096] __attribute__((aligned(8)));CHECK(api_nvnCommandBufferInitialize(&cmd,&d));api_nvnCommandBufferAddCommandMemory(&cmd,&p,31u*1024*1024,4096);api_nvnCommandBufferAddControlMemory(&cmd,control,sizeof(control));api_nvnCommandBufferBeginRecording(&cmd);GOOD();
 struct recording *r=find(&cmd,COMMAND)->record;size_t used=r->used,count=r->current->count,cpurefs=r->current->cpu_count;
 const NVNtexture *rt[]={&t};api_nvnCommandBufferSetRenderTargets(&cmd,1,rt,NULL,NULL,NULL);BAD(ENOTSUP);CHECK(!r->target&&r->used==used);
 NVNcopyRegion region={.width=32,.height=8,.depth=1};api_nvnCommandBufferCopyTextureToBuffer(&cmd,&t,NULL,&region,api_nvnMemoryPoolGetBufferAddress(&p)+29u*1024*1024,0);BAD(ENOTSUP);
 api_nvnCommandBufferCopyBufferToTexture(&cmd,api_nvnMemoryPoolGetBufferAddress(&p)+29u*1024*1024,&t,NULL,&region,0);BAD(ENOTSUP);
 api_nvnCommandBufferCopyTextureToTexture(&cmd,&t,NULL,&region,&linear,NULL,&region,0);BAD(ENOTSUP);
 const float color[4]={1,0,0,1};api_nvnCommandBufferClearTexture(&cmd,&t,NULL,&region,color,15);BAD(ENOTSUP);
 api_nvnTextureInvalidateTexels(&t,NULL,&region);GOOD(); /* canonical cache invalidation is now supported */unsigned char output[1024];memset(output,0xad,sizeof(output));api_nvnTextureReadTexels(&t,NULL,&region,output);BAD(ENOTSUP);for(unsigned i=0;i<sizeof(output);i++)CHECK(output[i]==0xad);
 CHECK(r->used==used&&r->current->count==count&&r->current->cpu_count==cpurefs);
 struct descriptor_entry before=table->slots[257];refs=pool->refs;unsigned char slot[32];memcpy(slot,(char *)storage+30u*1024*1024+257*32,32);
 fail_malloc=1;api_nvnTexturePoolRegisterTexture(&tp,257,&t,NULL);BAD(ENOMEM);CHECK(!memcmp(&before,table->slots+257,sizeof(before))&&pool->refs==refs);CHECK(!memcmp(slot,(char *)storage+30u*1024*1024+257*32,32));
 api_nvnTexturePoolRegisterTexture(&tp,257,&t,NULL);GOOD();CHECK(table->slots[257].pool==pool&&table->slots[257].source_layout);api_nvnTexturePoolFinalize(&tp);GOOD();
 CHECK(gpu_object_access(source,"host test source gate")==ENOTSUP);BAD(ENOTSUP);
 NVNwindowBuilder wb={0};NVNwindow window={0};NVNtexture *textures[]={&t,&t};api_nvnWindowBuilderSetDefaults(&wb);api_nvnWindowBuilderSetDevice(&wb,&d);api_nvnWindowBuilderSetNativeWindow(&wb,(NVNnativeWindow)(uintptr_t)1);api_nvnWindowBuilderSetTextures(&wb,2,textures);CHECK(!api_nvnWindowInitialize(&window,&wb));BAD(ENOTSUP);CHECK(!source->window_owner);
 NVNcommandHandle handle=api_nvnCommandBufferEndRecording(&cmd);CHECK(handle);api_nvnDeviceFinalizeCommandHandle(&d,handle);api_nvnCommandBufferFinalize(&cmd);GOOD();
 /* No GPU output is synthesized; source canonical bytes remain unchanged. */
 for(size_t off=512;off<19169280+512;off+=4096)CHECK(((unsigned char *)storage)[off]==0x5b);
 api_nvnTextureFinalize(&linear);api_nvnTextureFinalize(&t);GOOD();api_nvnMemoryPoolFinalize(&p);GOOD();api_nvnDeviceFinalize(&d);GOOD();
 while(objects){struct object *o=objects;CHECK(builder_kind(o->kind));remove_object(o);}CHECK(!handles);free(storage);
 printf("{\"passed\":true,\"checks\":%u,\"scope\":\"HOST ONLY canonical source resource and fail-closed GPU boundaries; no GPU execution\"}\n",checks);return 0;
}
