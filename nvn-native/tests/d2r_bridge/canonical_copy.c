/* SPDX-License-Identifier: MIT. Actual public API / common encoder, HOST ONLY.
 * The RAM queue retains owners but executes no CP/GPU/image instructions. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_alloc raw_alloc
#define nn_bo_destroy raw_destroy
#define nn_bo_map raw_map
#define nn_bo_cache raw_cache
#define nn_queue_submit raw_submit
#include NN_RESOURCE_MEMORY_BACKEND
#undef nn_bo_alloc
#undef nn_bo_destroy
#undef nn_bo_map
#undef nn_bo_cache
#undef nn_queue_submit
static unsigned allocations,caches,submits;
static int fail_alloc=-1,fail_destroy,fail_private_map,fail_cache,fail_resize;
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **o){allocations++;if(fail_alloc==0){fail_alloc=-1;*o=NULL;return ENOMEM;}if(fail_alloc>0)fail_alloc--;return raw_alloc(d,n,o);}
int nn_bo_destroy(nn_bo *b){if(fail_destroy)return EIO;return raw_destroy(b);}
void *nn_bo_map(const nn_bo *b){if(fail_private_map&&!b->imported){fail_private_map=0;return NULL;}return raw_map(b);}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int from){caches++;if(fail_cache){fail_cache=0;return EIO;}return raw_cache(b,o,n,from);}
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
 (void)offset;(void)bytes;submits++;if(count+1>64-q->count)return E2BIG;
 q->pending[q->count++]=ib;ib->busy++;
 for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}
 *timestamp=submits;return 0;
}
static void *checked_realloc(void *p,size_t n){if(fail_resize){fail_resize=0;return NULL;}return realloc(p,n);}
#define realloc checked_realloc
#include NN_RESOURCE_CORE
#undef realloc
#include NN_BLOCK_COPY_IMAGE
static unsigned checks;
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define GOOD() CK(!nn_nvn_error())
#define BAD(e) do{CK(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static NVNdevice d;static NVNmemoryPool source,linear,arena;static NVNcommandBuffer cmd,copy_cmd;
static NVNtextureBuilder tb;static NVNqueue q;
static int pool(NVNmemoryPool *p,void **storage,size_t bytes)
{
 CK(!posix_memalign(storage,4096,bytes));memset(*storage,0x69,bytes);
 NVNmemoryPoolBuilder b={0};api_nvnMemoryPoolBuilderSetDefaults(&b);api_nvnMemoryPoolBuilderSetDevice(&b,&d);api_nvnMemoryPoolBuilderSetStorage(&b,*storage,bytes);api_nvnMemoryPoolBuilderSetFlags(&b,0xa4);CK(api_nvnMemoryPoolInitialize(p,&b));GOOD();return 0;
}
static int texture(NVNtexture *t,int format)
{
 api_nvnTextureBuilderSetDefaults(&tb);api_nvnTextureBuilderSetDevice(&tb,&d);api_nvnTextureBuilderSetFlags(&tb,8);api_nvnTextureBuilderSetTarget(&tb,NVN_TEXTURE_TARGET_2D_ARRAY);api_nvnTextureBuilderSetSize3D(&tb,20,13,2);api_nvnTextureBuilderSetLevels(&tb,5);api_nvnTextureBuilderSetFormat(&tb,format);api_nvnTextureBuilderSetStorage(&tb,&source,512);CK(api_nvnTextureInitialize(t,&tb));GOOD();return 0;
}
static int positive(int format,unsigned index)
{
 NVNtexture t={0};CK(!texture(&t,format));struct object *o=find(&t,TEXTURE);size_t before=find(&d,DEVICE)->gpu->bos;
 unsigned level=index%5;const struct nn_tex_source_level *m=o->source_layout->level+level;
 NVNtextureView v={0};api_nvnTextureViewSetDefaults(&v);api_nvnTextureViewSetLevels(&v,(int)level,1);api_nvnTextureViewSetFormat(&v,format);
 NVNcopyRegion region={0,0,0,(int)m->width,(int)m->height,2};
 if(m->width>o->source_layout->block_w&&m->height>o->source_layout->block_h){region.xoffset=(int)o->source_layout->block_w;region.yoffset=(int)o->source_layout->block_h;region.width-=region.xoffset;region.height-=region.yoffset;}
 api_nvnCommandBufferBeginRecording(&cmd);api_nvnCommandBufferSetCopyRowStride(&cmd,m->row_bytes+1);api_nvnCommandBufferSetCopyImageStride(&cmd,(ptrdiff_t)(m->row_bytes+1)*m->blocks_y+3);GOOD();
 uint64_t address=api_nvnMemoryPoolGetBufferAddress(&linear)+516;
 api_nvnCommandBufferCopyBufferToTexture(&cmd,address,&t,&v,&region,0);GOOD();
 api_nvnCommandBufferCopyTextureToBuffer(&cmd,&t,&v,&region,address,0);GOOD();
 CK(find(&d,DEVICE)->gpu->bos==before+6);CK(o->refs==1);
 CK(gpu_object_access(o,"unqualified BC/array Draw gate")==ENOTSUP);BAD(ENOTSUP);
 NVNcommandHandle h=api_nvnCommandBufferEndRecording(&cmd);CK(h);GOOD();
 api_nvnTextureFinalize(&t);BAD(EBUSY);
 api_nvnCommandBufferBeginRecording(&copy_cmd);api_nvnCommandBufferCopyCommands(&copy_cmd,1,&h);GOOD();NVNcommandHandle copy=api_nvnCommandBufferEndRecording(&copy_cmd);CK(copy);GOOD();
 api_nvnDeviceFinalizeCommandHandle(&d,h);GOOD();CK(o->refs==1);
 unsigned old_alloc=allocations,old_cache=caches,old_submit=submits;
 int policy=find(&linear,POOL)->gpu_policy;find(&linear,POOL)->gpu_policy=NN_NVN_GPU_UNCACHED_UNQUALIFIED;
 NVNcommandHandle two[]={copy,copy};api_nvnQueueSubmitCommands(&q,2,two);BAD(ENOTSUP);CK(allocations==old_alloc&&caches==old_cache&&submits==old_submit);find(&linear,POOL)->gpu_policy=policy;
 for(unsigned replay=0;replay<2;replay++){
  api_nvnQueueSubmitCommands(&q,1,&copy);GOOD();CK(submits==old_submit+replay+1);CK(nn_bo_idle(o->bo)==EBUSY);
  api_nvnQueueFinish(&q);GOOD();CK(!nn_bo_idle(o->bo));
 }
 api_nvnDeviceFinalizeCommandHandle(&d,copy);GOOD();CK(!o->refs&&find(&d,DEVICE)->gpu->bos==before);
 api_nvnTextureFinalize(&t);GOOD();return 0;
}
static NVNtexture *callback_texture;static unsigned callback_busy;
static void memory_callback(NVNcommandBuffer *key,NVNcommandBufferMemoryEvent event,size_t minimum,void *user)
{
 (void)event;(void)minimum;(void)user;
 api_nvnTextureFinalize(callback_texture);callback_busy=nn_nvn_error()==EBUSY;nn_nvn_clear_error();
 api_nvnCommandBufferAddCommandMemory(key,&arena,24576,4096);
}
static int arena_cases(void)
{
 NVNtexture t={0};CK(!texture(&t,NVN_FORMAT_RGBA_DXT1));
 NVNcopyRegion region={0,0,0,20,13,2};uint64_t address=api_nvnMemoryPoolGetBufferAddress(&linear)+516;
 NVNcommandBuffer other={0};unsigned char control[1024] __attribute__((aligned(8)));
 CK(api_nvnCommandBufferInitialize(&other,&d));api_nvnCommandBufferAddCommandMemory(&other,&linear,512,4096);api_nvnCommandBufferAddControlMemory(&other,control,sizeof(control));api_nvnCommandBufferBeginRecording(&other);NVNcommandHandle old=api_nvnCommandBufferEndRecording(&other);CK(old);api_nvnCommandBufferFinalize(&other);GOOD();
 /* No COMMAND still names the old linear arena; the published handle does. */
 api_nvnCommandBufferBeginRecording(&cmd);api_nvnCommandBufferSetCopyRowStride(&cmd,0);api_nvnCommandBufferSetCopyImageStride(&cmd,0);GOOD();
 unsigned a=allocations,c=caches,s=submits;api_nvnCommandBufferCopyTextureToBuffer(&cmd,&t,NULL,&region,address,0);BAD(ENOTSUP);CK(a==allocations&&c==caches&&s==submits);
 NVNcommandHandle empty=api_nvnCommandBufferEndRecording(&cmd);CK(empty);api_nvnDeviceFinalizeCommandHandle(&d,empty);api_nvnDeviceFinalizeCommandHandle(&d,old);GOOD();memset(nn_bo_map(find(&linear,POOL)->bo),0x69,65536);
 /* A new command arena bound AFTER copy recording must fail queue preflight. */
 api_nvnCommandBufferBeginRecording(&cmd);api_nvnCommandBufferCopyTextureToBuffer(&cmd,&t,NULL,&region,address,0);GOOD();NVNcommandHandle copy=api_nvnCommandBufferEndRecording(&cmd);CK(copy);GOOD();
 CK(api_nvnCommandBufferInitialize(&other,&d));api_nvnCommandBufferAddCommandMemory(&other,&linear,512,4096);GOOD();a=allocations;c=caches;s=submits;
 api_nvnQueueSubmitCommands(&q,1,&copy);BAD(ENOTSUP);CK(a==allocations&&c==caches&&s==submits);
 api_nvnCommandBufferFinalize(&other);GOOD();api_nvnQueueSubmitCommands(&q,1,&copy);GOOD();CK(submits==s+1);api_nvnQueueFinish(&q);api_nvnDeviceFinalizeCommandHandle(&d,copy);GOOD();
 /* Copy's memory callback cannot finalize the source identity mid-admission. */
 unsigned char callback_control[1024] __attribute__((aligned(8)));
 CK(api_nvnCommandBufferInitialize(&other,&d));api_nvnCommandBufferAddCommandMemory(&other,&arena,24576,8);api_nvnCommandBufferAddControlMemory(&other,callback_control,sizeof(callback_control));api_nvnCommandBufferSetMemoryCallback(&other,memory_callback);callback_texture=&t;api_nvnCommandBufferBeginRecording(&other);GOOD();
 api_nvnCommandBufferCopyBufferToTexture(&other,address,&t,NULL,&region,0);GOOD();CK(callback_busy==1);api_nvnCommandBufferFinalize(&other);GOOD();api_nvnTextureFinalize(&t);GOOD();return 0;
}
static int failures(void)
{
 NVNtexture t={0};CK(!texture(&t,NVN_FORMAT_RGBA_DXT1));struct object *o=find(&t,TEXTURE),*dev=find(&d,DEVICE);
 NVNcopyRegion region={0,0,0,20,13,2};uint64_t address=api_nvnMemoryPoolGetBufferAddress(&linear)+516;
 api_nvnCommandBufferBeginRecording(&cmd);api_nvnCommandBufferSetCopyRowStride(&cmd,0);api_nvnCommandBufferSetCopyImageStride(&cmd,0);GOOD();
 struct recording *r=find(&cmd,COMMAND)->record;size_t used=r->used,resources=r->current->count,bos=dev->gpu->bos,refs=dev->refs;
 for(unsigned failure=0;failure<6;failure++){
  if(failure<3)fail_alloc=(int)failure;
  if(failure==3)fail_private_map=1;
  if(failure==4)fail_cache=1;
  if(failure==5)fail_resize=1;
  api_nvnCommandBufferCopyBufferToTexture(&cmd,address,&t,NULL,&region,0);BAD(failure<3||failure==5?ENOMEM:failure==3?ENOTSUP:EIO);
  CK(r->used==used&&r->current->count==resources&&!o->refs&&dev->gpu->bos==bos&&dev->refs==refs);
 }
 fail_alloc=1;fail_destroy=1;
 api_nvnCommandBufferCopyBufferToTexture(&cmd,address,&t,NULL,&region,0);BAD(EIO);CK(dev->retired&&dev->gpu->bos==bos+1&&dev->refs==refs+1);
 CK(r->used==used&&r->current->count==resources&&!o->refs);
 fail_destroy=0;reap_jobs(dev);GOOD();CK(!dev->retired&&dev->gpu->bos==bos&&dev->refs==refs);
 unsigned a=allocations,c=caches,s=submits;
 api_nvnCommandBufferCopyBufferToTexture(&cmd,address,&t,NULL,&region,1);BAD(ENOTSUP);
 api_nvnCommandBufferCopyBufferToTexture(&cmd,address+1,&t,NULL,&region,0);BAD(EINVAL);
 api_nvnCommandBufferCopyBufferToTexture(&cmd,api_nvnMemoryPoolGetBufferAddress(&source)+512,&t,NULL,&region,0);BAD(EINVAL);
 region.xoffset=1;region.width=19;api_nvnCommandBufferCopyBufferToTexture(&cmd,address,&t,NULL,&region,0);BAD(ENOTSUP);region.xoffset=0;region.width=20;
 size_t hold=o->refs;o->refs=SIZE_MAX;api_nvnCommandBufferCopyBufferToTexture(&cmd,address,&t,NULL,&region,0);BAD(EOVERFLOW);o->refs=hold;
 CK(allocations==a&&caches==c&&submits==s&&r->used==used&&!o->refs);
 NVNcommandHandle empty=api_nvnCommandBufferEndRecording(&cmd);CK(empty);api_nvnDeviceFinalizeCommandHandle(&d,empty);api_nvnTextureFinalize(&t);GOOD();return 0;
}
static NVNmemoryPool destination_pool;
static unsigned texture_copies;
static int destination_texture(NVNtexture *t,int format,int samebo)
{
 api_nvnTextureBuilderSetDefaults(&tb);api_nvnTextureBuilderSetDevice(&tb,&d);api_nvnTextureBuilderSetFlags(&tb,8);api_nvnTextureBuilderSetTarget(&tb,NVN_TEXTURE_TARGET_2D_ARRAY);
 api_nvnTextureBuilderSetSize3D(&tb,samebo?20:40,samebo?13:26,samebo?2:3);api_nvnTextureBuilderSetLevels(&tb,samebo?5:6);api_nvnTextureBuilderSetFormat(&tb,format);
 api_nvnTextureBuilderSetStorage(&tb,samebo?&source:&destination_pool,samebo?32768:512);CK(api_nvnTextureInitialize(t,&tb));GOOD();return 0;
}
static int positive_texture_copy(int format,unsigned index,int samebo)
{
 NVNtexture src,dst;CK(!texture(&src,format));CK(!destination_texture(&dst,format,samebo));
 struct object *s=find(&src,TEXTURE),*t=find(&dst,TEXTURE),*dev=find(&d,DEVICE);size_t before=dev->gpu->bos;
 unsigned level=index%5;const struct nn_tex_source_level *m=s->source_layout->level+level;
 NVNtextureView sv,dv;api_nvnTextureViewSetDefaults(&sv);api_nvnTextureViewSetDefaults(&dv);api_nvnTextureViewSetLevels(&sv,(int)level,1);api_nvnTextureViewSetLayers(&sv,0,2);
 api_nvnTextureViewSetLevels(&dv,(int)level+(samebo?0:1),1);api_nvnTextureViewSetLayers(&dv,samebo?0:1,2);
 NVNcopyRegion region={0,0,0,(int)m->width,(int)m->height,2};
 if(m->width>s->source_layout->block_w&&m->height>s->source_layout->block_h){region.xoffset=(int)s->source_layout->block_w;region.yoffset=(int)s->source_layout->block_h;region.width-=region.xoffset;region.height-=region.yoffset;}
 api_nvnCommandBufferBeginRecording(&cmd);api_nvnCommandBufferSetCopyRowStride(&cmd,1);api_nvnCommandBufferSetCopyImageStride(&cmd,-1);GOOD();
 struct recording *r=find(&cmd,COMMAND)->record;size_t at=r->used;
 api_nvnCommandBufferCopyTextureToTexture(&cmd,&src,&sv,&region,&dst,&dv,&region,0);GOOD();
 CK(r->used==at+32&&dev->gpu->bos==before+6&&s->refs==1&&t->refs==1);
 struct job *jobs[6];jobs[0]=r->current->jobs;for(unsigned i=1;i<6;i++){CK(jobs[i-1]);jobs[i]=jobs[i-1]->next;}CK(jobs[5]&&!jobs[5]->next);
 const uint32_t *words=(const uint32_t*)((const char*)nn_bo_map(r->pool->bo)+r->offset+at);
 for(unsigned i=0;i<2;i++){CK(words[4*i]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));CK(((uint64_t)words[4*i+1]|(uint64_t)words[4*i+2]<<32)==nn_bo_address(jobs[4+i]->bo));}
 const struct nn_block_copy_args *a=nn_bo_map(jobs[1]->bo),*b=nn_bo_map(jobs[2]->bo);CK(a->direction==0&&b->direction==1&&a->row_bytes==b->row_bytes&&a->rows==b->rows&&a->layers==2&&b->layers==2);
 const uint32_t *first=nn_bo_map(jobs[4]->bo),*second=nn_bo_map(jobs[5]->bo);uint64_t mid=nn_bo_address(jobs[3]->bo);
 CK(((uint64_t)first[1024+16+4]|(uint64_t)first[1024+16+5]<<32)==mid);CK(((uint64_t)second[1024+4]|(uint64_t)second[1024+5]<<32)==mid);
 CK(jobs[4]->copy_source==s&&jobs[5]->copy_destination==t&&jobs[5]->copy_destination_pool==t->pool&&jobs[5]->copy_destination_offset==t->offset&&jobs[5]->copy_destination_bytes==t->size);
 for(size_t i=0;i<NVN_COMPILED_IMAGE.patch_count;i++)if(NVN_COMPILED_IMAGE.patches[i].kind==NN_CS_CODE){unsigned k=NVN_COMPILED_IMAGE.patches[i].word;uint64_t code=nn_bo_address(jobs[0]->bo);CK(((uint64_t)first[k]|(uint64_t)first[k+1]<<32)==code&&((uint64_t)second[k]|(uint64_t)second[k+1]<<32)==code);}
 NVNcommandHandle h=api_nvnCommandBufferEndRecording(&cmd);CK(h);GOOD();
 api_nvnCommandBufferBeginRecording(&copy_cmd);api_nvnCommandBufferCopyCommands(&copy_cmd,1,&h);GOOD();NVNcommandHandle copied=api_nvnCommandBufferEndRecording(&copy_cmd);CK(copied);GOOD();api_nvnDeviceFinalizeCommandHandle(&d,h);GOOD();CK(s->refs==1&&t->refs==1);
 for(unsigned replay=0;replay<2;replay++){api_nvnQueueSubmitCommands(&q,1,&copied);GOOD();CK(nn_bo_idle(s->bo)==EBUSY&&nn_bo_idle(t->bo)==EBUSY);api_nvnQueueFinish(&q);GOOD();}
 api_nvnDeviceFinalizeCommandHandle(&d,copied);GOOD();CK(!s->refs&&!t->refs&&dev->gpu->bos==before);api_nvnTextureFinalize(&src);api_nvnTextureFinalize(&dst);GOOD();texture_copies++;return 0;
}
static int texture_copy_failures(void)
{
 NVNtexture src,dst;CK(!texture(&src,NVN_FORMAT_RGBA_DXT1));CK(!destination_texture(&dst,NVN_FORMAT_RGBA_DXT1,0));
 NVNtexture overlap;CK(!texture(&overlap,NVN_FORMAT_RGBA_DXT1));
 struct object *s=find(&src,TEXTURE),*t=find(&dst,TEXTURE),*dev=find(&d,DEVICE);NVNcopyRegion sr={0,0,0,20,13,2},dr=sr;
 NVNtextureView view;api_nvnTextureViewSetDefaults(&view);api_nvnTextureViewSetLevels(&view,1,1);api_nvnTextureViewSetLayers(&view,1,2);
 api_nvnCommandBufferBeginRecording(&cmd);struct recording *r=find(&cmd,COMMAND)->record;size_t used=r->used,refs=r->current->count,bos=dev->gpu->bos,drefs=dev->refs;
 for(unsigned fail=0;fail<9;fail++){
  if(fail<6)fail_alloc=(int)fail;
  if(fail==6)fail_private_map=1;
  if(fail==7)fail_cache=1;
  if(fail==8)fail_resize=1;
  api_nvnCommandBufferCopyTextureToTexture(&cmd,&src,NULL,&sr,&dst,&view,&dr,0);BAD(fail<6||fail==8?ENOMEM:fail==6?ENOTSUP:EIO);
  CK(r->used==used&&r->current->count==refs&&!s->refs&&!t->refs&&dev->gpu->bos==bos&&dev->refs==drefs);
 }
 fail_alloc=3;fail_destroy=1;api_nvnCommandBufferCopyTextureToTexture(&cmd,&src,NULL,&sr,&dst,&view,&dr,0);BAD(EIO);CK(dev->gpu->bos==bos+3&&dev->refs==drefs+3&&dev->retired);fail_destroy=0;reap_jobs(dev);GOOD();CK(dev->gpu->bos==bos&&dev->refs==drefs&&!dev->retired);
 unsigned alloc=allocations;dr.width=19;api_nvnCommandBufferCopyTextureToTexture(&cmd,&src,NULL,&sr,&dst,&view,&dr,0);BAD(ENOTSUP);dr=sr;
 api_nvnCommandBufferCopyTextureToTexture(&cmd,&src,NULL,&sr,&dst,&view,&dr,1);BAD(ENOTSUP);
 api_nvnCommandBufferCopyTextureToTexture(&cmd,&src,NULL,&sr,&src,NULL,&sr,0);BAD(EINVAL);
 api_nvnCommandBufferCopyTextureToTexture(&cmd,&src,NULL,&sr,&overlap,NULL,&sr,0);BAD(EINVAL);
 sr.xoffset=1;sr.width=19;dr=sr;api_nvnCommandBufferCopyTextureToTexture(&cmd,&src,NULL,&sr,&dst,&view,&dr,0);BAD(ENOTSUP);sr=(NVNcopyRegion){0,0,0,20,13,2};dr=sr;
 /* Distinct imported GPU BOs are not evidence of physical disjointness. */
 NVNmemoryPool alias_pool;NVNmemoryPoolBuilder pb={0};api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&d);api_nvnMemoryPoolBuilderSetStorage(&pb,s->pool->storage,s->pool->size);api_nvnMemoryPoolBuilderSetFlags(&pb,0xa4);CK(api_nvnMemoryPoolInitialize(&alias_pool,&pb));GOOD();
 NVNtexture alias;api_nvnTextureBuilderSetDefaults(&tb);api_nvnTextureBuilderSetDevice(&tb,&d);api_nvnTextureBuilderSetFlags(&tb,8);api_nvnTextureBuilderSetTarget(&tb,NVN_TEXTURE_TARGET_2D_ARRAY);api_nvnTextureBuilderSetSize3D(&tb,20,13,2);api_nvnTextureBuilderSetLevels(&tb,5);api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA_DXT1);api_nvnTextureBuilderSetStorage(&tb,&alias_pool,512);CK(api_nvnTextureInitialize(&alias,&tb));GOOD();CK(find(&alias,TEXTURE)->bo!=s->bo);
 api_nvnCommandBufferCopyTextureToTexture(&cmd,&src,NULL,&sr,&alias,NULL,&sr,0);BAD(EINVAL);api_nvnTextureFinalize(&alias);api_nvnMemoryPoolFinalize(&alias_pool);GOOD();
 CK(allocations==alloc&&r->used==used&&r->current->count==refs&&!s->refs&&!t->refs);
 NVNcommandHandle empty=api_nvnCommandBufferEndRecording(&cmd);CK(empty);api_nvnDeviceFinalizeCommandHandle(&d,empty);api_nvnTextureFinalize(&overlap);api_nvnTextureFinalize(&src);api_nvnTextureFinalize(&dst);GOOD();return 0;
}

static int normal_copy_dependency(void)
{
 size_t dispatch=SIZE_MAX,invalid=SIZE_MAX,flush=SIZE_MAX,wait=SIZE_MAX;
 for(size_t i=0;i<NVN_COMPILED_IMAGE.command_words;){
  uint32_t v=NVN_COMPILED_IMAGE.commands[i];unsigned n=nvn_pm4_packet_words(v);CK(n&&n<=NVN_COMPILED_IMAGE.command_words-i);
  if(v==nvn_pm4_packet7(0x33,4)){CK(dispatch==SIZE_MAX);dispatch=i;}
  if(v==nvn_pm4_packet7(0x46,1)&&NVN_COMPILED_IMAGE.commands[i+1]==0x31)invalid=i;
  if(v==nvn_pm4_packet7(0x46,4)&&NVN_COMPILED_IMAGE.commands[i+1]==4)flush=i;
  if(v==nvn_pm4_packet7(0x14,4))wait=i;
  i+=n;
 }
 CK(invalid<dispatch&&dispatch<flush&&flush<wait&&wait<NVN_COMPILED_IMAGE.command_words-2);
 CK(NVN_COMPILED_IMAGE.commands[NVN_COMPILED_IMAGE.command_words-2]==nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0));
 CK(NVN_COMPILED_IMAGE.commands[NVN_COMPILED_IMAGE.command_words-1]==nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0));return 0;
}
int main(void)
{
 CK(!normal_copy_dependency());
 NVNdeviceBuilder db={0};api_nvnDeviceBuilderSetDefaults(&db);CK(api_nvnDeviceInitialize(&d,&db));GOOD();CK(!nn_nvn_register_block_copy(&d,&NVN_COMPILED_IMAGE));
 struct nn_cs_image bad=NVN_COMPILED_IMAGE;bad.local[0]=4;CK(nn_nvn_register_block_copy(&d,&bad)==ENOTSUP);bad=NVN_COMPILED_IMAGE;CK(nn_nvn_register_block_copy(&d,&bad)==EBUSY);
 void *ds;CK(!pool(&destination_pool,&ds,262144));
 void *s,*l,*a;CK(!pool(&source,&s,65536));CK(!pool(&linear,&l,65536));CK(!pool(&arena,&a,65536));
 unsigned char control[32768] __attribute__((aligned(8))),copy_control[32768] __attribute__((aligned(8)));
 CK(api_nvnCommandBufferInitialize(&cmd,&d));CK(api_nvnCommandBufferInitialize(&copy_cmd,&d));api_nvnCommandBufferAddCommandMemory(&cmd,&arena,0,32768);api_nvnCommandBufferAddCommandMemory(&copy_cmd,&arena,32768,32768);api_nvnCommandBufferAddControlMemory(&cmd,control,sizeof(control));api_nvnCommandBufferAddControlMemory(&copy_cmd,copy_control,sizeof(copy_control));
 NVNqueueBuilder qb={0};api_nvnQueueBuilderSetDefaults(&qb);api_nvnQueueBuilderSetDevice(&qb,&d);CK(api_nvnQueueInitialize(&q,&qb));GOOD();
 const int formats[]={1,5,6,7,8,9,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x38,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x9a,0x9c};
 for(unsigned i=0;i<sizeof(formats)/sizeof(*formats);i++)CK(!positive(formats[i],i));
 CK(!arena_cases());for(unsigned i=0;i<sizeof(formats)/sizeof(*formats);i++)CK(!positive_texture_copy(formats[i],i,0));
 CK(!positive_texture_copy(1,0,1));CK(!texture_copy_failures());
 CK(!failures());for(unsigned i=0;i<65536;i++)CK(((unsigned char *)s)[i]==0x69&&((unsigned char *)l)[i]==0x69);
 api_nvnCommandBufferFinalize(&cmd);api_nvnCommandBufferFinalize(&copy_cmd);api_nvnQueueFinalize(&q);api_nvnMemoryPoolFinalize(&destination_pool);api_nvnMemoryPoolFinalize(&source);api_nvnMemoryPoolFinalize(&linear);api_nvnMemoryPoolFinalize(&arena);GOOD();api_nvnDeviceFinalize(&d);GOOD();free(s);free(l);free(a);free(ds);
 CK(submits==151&&texture_copies==38);printf("{\"passed\":true,\"checks\":%u,\"formats\":37,\"mock_submits\":%u,\"texture_copy_cases\":38,\"gpu_execution\":false}\n",checks,submits);return 0;
}
