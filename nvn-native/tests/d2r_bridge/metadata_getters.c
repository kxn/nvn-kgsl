/* Authored HOST public getter test, actual core + nonexecuting RAM owners.
 * No queue submission, GPU emulation, original module or image synthesis. */
#define _POSIX_C_SOURCE 200809L
#include NN_METADATA_CORE
#include NN_METADATA_MEMORY_BACKEND
#include <stdio.h>
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s sticky %d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(e) do{T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
#define F(name) ((PFN##name##PROC)nvnBootstrapLoader(#name))
/* Explicit names keep every call through the actual published PFN signature. */
#define LOAD(name,type) type name=(type)nvnBootstrapLoader(#name);T(name!=NULL)
static unsigned checks,callback_calls;
static void callback(NVNcommandBuffer *cb,NVNcommandBufferMemoryEvent e,size_t n,void *u)
{(void)cb;(void)e;(void)n;(void)u;callback_calls++;}
int main(void)
{
#undef ENTRY
#define ENTRY(name,type) LOAD(name,type);
ENTRY(nvnDeviceBuilderGetFlags, PFNNVNDEVICEBUILDERGETFLAGSPROC)
ENTRY(nvnMemoryPoolBuilderGetMemory, PFNNVNMEMORYPOOLBUILDERGETMEMORYPROC)
ENTRY(nvnMemoryPoolBuilderGetSize, PFNNVNMEMORYPOOLBUILDERGETSIZEPROC)
ENTRY(nvnMemoryPoolBuilderGetFlags, PFNNVNMEMORYPOOLBUILDERGETFLAGSPROC)
ENTRY(nvnMemoryPoolGetSize, PFNNVNMEMORYPOOLGETSIZEPROC)
ENTRY(nvnMemoryPoolGetFlags, PFNNVNMEMORYPOOLGETFLAGSPROC)
ENTRY(nvnBufferBuilderGetMemoryOffset, PFNNVNBUFFERBUILDERGETMEMORYOFFSETPROC)
ENTRY(nvnBufferBuilderGetSize, PFNNVNBUFFERBUILDERGETSIZEPROC)
ENTRY(nvnBufferGetMemoryOffset, PFNNVNBUFFERGETMEMORYOFFSETPROC)
ENTRY(nvnBufferGetSize, PFNNVNBUFFERGETSIZEPROC)
ENTRY(nvnTextureBuilderGetMemoryOffset, PFNNVNTEXTUREBUILDERGETMEMORYOFFSETPROC)
ENTRY(nvnTextureGetMemoryOffset, PFNNVNTEXTUREGETMEMORYOFFSETPROC)
ENTRY(nvnQueueBuilderGetFlags, PFNNVNQUEUEBUILDERGETFLAGSPROC)
ENTRY(nvnQueueBuilderGetMemorySize, PFNNVNQUEUEBUILDERGETMEMORYSIZEPROC)
ENTRY(nvnQueueBuilderGetMemory, PFNNVNQUEUEBUILDERGETMEMORYPROC)
ENTRY(nvnCommandBufferGetMemoryCallback, PFNNVNCOMMANDBUFFERGETMEMORYCALLBACKPROC)
ENTRY(nvnCommandBufferGetMemoryCallbackData, PFNNVNCOMMANDBUFFERGETMEMORYCALLBACKDATAPROC)
ENTRY(nvnBufferBuilderGetMemoryPool, PFNNVNBUFFERBUILDERGETMEMORYPOOLPROC)
ENTRY(nvnBufferGetMemoryPool, PFNNVNBUFFERGETMEMORYPOOLPROC)
ENTRY(nvnTextureBuilderGetMemoryPool, PFNNVNTEXTUREBUILDERGETMEMORYPOOLPROC)
ENTRY(nvnTextureGetMemoryPool, PFNNVNTEXTUREGETMEMORYPOOLPROC)
ENTRY(nvnQueueBuilderGetCommandMemorySize, PFNNVNQUEUEBUILDERGETCOMMANDMEMORYSIZEPROC)
ENTRY(nvnQueueBuilderGetComputeMemorySize, PFNNVNQUEUEBUILDERGETCOMPUTEMEMORYSIZEPROC)
ENTRY(nvnQueueBuilderGetControlMemorySize, PFNNVNQUEUEBUILDERGETCONTROLMEMORYSIZEPROC)
ENTRY(nvnQueueBuilderGetCommandFlushThreshold, PFNNVNQUEUEBUILDERGETCOMMANDFLUSHTHRESHOLDPROC)
ENTRY(nvnTextureViewGetLevels, PFNNVNTEXTUREVIEWGETLEVELSPROC)
ENTRY(nvnTextureViewGetFormat, PFNNVNTEXTUREVIEWGETFORMATPROC)
ENTRY(nvnTextureViewGetTarget, PFNNVNTEXTUREVIEWGETTARGETPROC)
ENTRY(nvnVertexAttribStateGetFormat, PFNNVNVERTEXATTRIBSTATEGETFORMATPROC)
ENTRY(nvnVertexAttribStateGetStreamIndex, PFNNVNVERTEXATTRIBSTATEGETSTREAMINDEXPROC)
ENTRY(nvnVertexStreamStateGetStride, PFNNVNVERTEXSTREAMSTATEGETSTRIDEPROC)
ENTRY(nvnVertexStreamStateGetDivisor, PFNNVNVERTEXSTREAMSTATEGETDIVISORPROC)
#undef ENTRY
 NVNdeviceBuilder db={0};NVNdevice d={0};NVNmemoryPoolBuilder pb={0};NVNmemoryPool p={0};NVNbufferBuilder bb={0};NVNbuffer b={0};NVNtextureBuilder tb={0};NVNtexture tex={0};
 OK(api_nvnDeviceBuilderSetDefaults(&db));T(nvnDeviceBuilderGetFlags(&db)==0);OK(api_nvnDeviceBuilderSetFlags(&db,NVN_DEVICE_FLAG_ENABLE_SEPARATE_SAMPLER_TEXTURE_SUPPORT));T(nvnDeviceBuilderGetFlags(&db)==NVN_DEVICE_FLAG_ENABLE_SEPARATE_SAMPLER_TEXTURE_SUPPORT);T(api_nvnDeviceInitialize(&d,&db));T(!nn_nvn_error());
 OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));T(!nvnMemoryPoolBuilderGetMemory(&pb));T(!nvnMemoryPoolBuilderGetSize(&pb));T(nvnMemoryPoolBuilderGetFlags(&pb)==(NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));
 /* Full size_t/pointer width is part of the query ABI. Metadata does not
  * allocate or dereference the sentinel address. */
 void *sentinel=(void *)(uintptr_t)UINT64_C(0x123456789000);size_t wide=((size_t)1<<34)+4096;
 OK(api_nvnMemoryPoolBuilderSetStorage(&pb,sentinel,wide));T(nvnMemoryPoolBuilderGetMemory(&pb)==sentinel);T(nvnMemoryPoolBuilderGetSize(&pb)==wide);
 OK(api_nvnMemoryPoolBuilderSetFlags(&pb,(int)UINT32_C(0x84210022)));T((uint32_t)nvnMemoryPoolBuilderGetFlags(&pb)==UINT32_C(0x84210022));
 void *ram=NULL;T(!posix_memalign(&ram,4096,65536));memset(ram,0x73,65536);
 OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&d));OK(api_nvnMemoryPoolBuilderSetStorage(&pb,ram,65536));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));T(api_nvnMemoryPoolInitialize(&p,&pb));T(nvnMemoryPoolGetSize(&p)==65536);T(nvnMemoryPoolGetFlags(&p)==(NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));T(api_nvnMemoryPoolMap(&p)==ram);
 OK(api_nvnMemoryPoolBuilderSetStorage(&pb,sentinel,wide));T(nvnMemoryPoolGetSize(&p)==65536);T(nvnMemoryPoolBuilderGetSize(&pb)==wide);
 OK(api_nvnBufferBuilderSetDefaults(&bb));T(!nvnBufferBuilderGetMemoryPool(&bb));T(!nvnBufferBuilderGetMemoryOffset(&bb));T(!nvnBufferBuilderGetSize(&bb));OK(api_nvnBufferBuilderSetDevice(&bb,&d));OK(api_nvnBufferBuilderSetStorage(&bb,&p,256,4096));T(nvnBufferBuilderGetMemoryPool(&bb)==&p);T(nvnBufferBuilderGetMemoryOffset(&bb)==256);T(nvnBufferBuilderGetSize(&bb)==4096);T(api_nvnBufferInitialize(&b,&bb));T(nvnBufferGetMemoryPool(&b)==&p);T(nvnBufferGetMemoryOffset(&b)==256);T(nvnBufferGetSize(&b)==4096);T(api_nvnBufferMap(&b)==(char *)ram+256);
 OK(api_nvnBufferBuilderSetStorage(&bb,&p,512,2048));T(nvnBufferGetMemoryOffset(&b)==256);T(nvnBufferGetSize(&b)==4096);T(nvnBufferBuilderGetMemoryOffset(&bb)==512);T(nvnBufferBuilderGetSize(&bb)==2048);
 OK(api_nvnTextureBuilderSetDefaults(&tb));T(!nvnTextureBuilderGetMemoryPool(&tb));T(!nvnTextureBuilderGetMemoryOffset(&tb));OK(api_nvnTextureBuilderSetDevice(&tb,&d));OK(api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetSize2D(&tb,16,8));OK(api_nvnTextureBuilderSetFlags(&tb,NVN_TEXTURE_FLAGS_LINEAR));OK(api_nvnTextureBuilderSetStride(&tb,64));OK(api_nvnTextureBuilderSetStorage(&tb,&p,8192));T(nvnTextureBuilderGetMemoryPool(&tb)==&p);T(nvnTextureBuilderGetMemoryOffset(&tb)==8192);T(api_nvnTextureInitialize(&tex,&tb));T(nvnTextureGetMemoryPool(&tex)==&p);T(nvnTextureGetMemoryOffset(&tex)==8192);OK(api_nvnTextureBuilderSetStorage(&tb,&p,12288));T(nvnTextureGetMemoryOffset(&tex)==8192);T(nvnTextureBuilderGetMemoryOffset(&tb)==12288);
 /* Querying strong owners cannot relax in-flight lifetime rules. */
 api_nvnMemoryPoolFinalize(&p);BAD(EBUSY);T(nvnBufferGetMemoryPool(&b)==&p);T(nvnTextureGetMemoryPool(&tex)==&p);
 NVNqueueBuilder qb={0};OK(api_nvnQueueBuilderSetDefaults(&qb));T(!nvnQueueBuilderGetFlags(&qb));T(!nvnQueueBuilderGetMemory(&qb));T(!nvnQueueBuilderGetMemorySize(&qb));
 typedef NVNboolean(*budget_get)(const NVNqueueBuilder *,int *);budget_get gs[]={nvnQueueBuilderGetCommandMemorySize,nvnQueueBuilderGetComputeMemorySize,nvnQueueBuilderGetControlMemorySize,nvnQueueBuilderGetCommandFlushThreshold};
 for(unsigned i=0;i<4;i++){struct {uint32_t before;int v;uint32_t after;} out={0xabcdefff,-9,0xfedcba98};T(!gs[i](&qb,&out.v));T(!nn_nvn_error());T(out.v==0&&out.before==0xabcdefff&&out.after==0xfedcba98);}
 OK(api_nvnQueueBuilderSetFlags(&qb,3));T(nvnQueueBuilderGetFlags(&qb)==3);OK(api_nvnQueueBuilderSetQueueMemory(&qb,ram,65536));T(nvnQueueBuilderGetMemory(&qb)==ram);T(nvnQueueBuilderGetMemorySize(&qb)==65536);
 OK(api_nvnQueueBuilderSetCommandMemorySize(&qb,131072));OK(api_nvnQueueBuilderSetComputeMemorySize(&qb,0));OK(api_nvnQueueBuilderSetControlMemorySize(&qb,32768));OK(api_nvnQueueBuilderSetCommandFlushThreshold(&qb,17));int expected[]={131072,0,32768,17};for(unsigned i=0;i<4;i++){int v=-9;T(gs[i](&qb,&v)==1);T(v==expected[i]);T(!gs[i](&qb,NULL));BAD(EINVAL);}
 OK(api_nvnQueueBuilderSetDefaults(&qb));for(unsigned i=0;i<4;i++){int v=-9;T(!gs[i](&qb,&v));T(!v);}T(!nvnQueueBuilderGetMemory(&qb)&&!nvnQueueBuilderGetMemorySize(&qb));
 NVNtextureView v={0};OK(api_nvnTextureViewSetDefaults(&v));int base=-9,count=-9;NVNformat fmt=NVN_FORMAT_BGRA8;NVNtextureTarget target=NVN_TEXTURE_TARGET_BUFFER;
 T(!nvnTextureViewGetLevels(&v,&base,&count));T(base==0&&count==0);T(!nvnTextureViewGetFormat(&v,&fmt));T(fmt==NVN_FORMAT_NONE);T(!nvnTextureViewGetTarget(&v,&target));T(target==NVN_TEXTURE_TARGET_1D);T(!nn_nvn_error());
 OK(api_nvnTextureViewSetLevels(&v,2,0));OK(api_nvnTextureViewSetFormat(&v,NVN_FORMAT_RGBA_DXT1));OK(api_nvnTextureViewSetTarget(&v,NVN_TEXTURE_TARGET_3D));NVNtextureView clone=v;memset(&v,0x77,sizeof(v));T(nvnTextureViewGetLevels(&clone,&base,&count));T(base==2&&!count);T(nvnTextureViewGetFormat(&clone,&fmt));T(fmt==NVN_FORMAT_RGBA_DXT1);T(nvnTextureViewGetTarget(&clone,&target));T(target==NVN_TEXTURE_TARGET_3D);
 base=314;count=159;T(!nvnTextureViewGetLevels(&clone,&base,NULL));BAD(EINVAL);T(base==314&&count==159);T(!nvnTextureViewGetFormat(NULL,&fmt));BAD(EINVAL);T(fmt==NVN_FORMAT_RGBA_DXT1);T(!nvnTextureViewGetTarget(&v,&target));BAD(EINVAL);T(target==NVN_TEXTURE_TARGET_3D);
 NVNvertexAttribState a;NVNvertexStreamState vs;OK(api_nvnVertexAttribStateSetDefaults(&a));ptrdiff_t off=-1;fmt=NVN_FORMAT_RGBA_DXT1;nvnVertexAttribStateGetFormat(&a,&fmt,&off);T(!nn_nvn_error());T(fmt==NVN_FORMAT_NONE&&!off&&!nvnVertexAttribStateGetStreamIndex(&a));OK(api_nvnVertexStreamStateSetDefaults(&vs));T(!nvnVertexStreamStateGetStride(&vs)&&!nvnVertexStreamStateGetDivisor(&vs));
 const ptrdiff_t offsets[]={0,1,255,256,65535};for(unsigned i=0;i<5;i++){OK(api_nvnVertexAttribStateSetFormat(&a,NVN_FORMAT_RGBA32F,offsets[i]));OK(api_nvnVertexAttribStateSetStreamIndex(&a,15-(int)i));NVNvertexAttribState acopy=a;nvnVertexAttribStateGetFormat(&acopy,&fmt,&off);T(fmt==NVN_FORMAT_RGBA32F&&off==offsets[i]);T(nvnVertexAttribStateGetStreamIndex(&acopy)==15-(int)i);}
 OK(api_nvnVertexStreamStateSetStride(&vs,(ptrdiff_t)UINT32_MAX));OK(api_nvnVertexStreamStateSetDivisor(&vs,INT_MAX));NVNvertexStreamState vcopy=vs;T(nvnVertexStreamStateGetStride(&vcopy)==(ptrdiff_t)UINT32_MAX);T(nvnVertexStreamStateGetDivisor(&vcopy)==INT_MAX);nvnVertexAttribStateGetFormat(NULL,&fmt,&off);BAD(EINVAL);T(!nvnVertexStreamStateGetStride(NULL));BAD(EINVAL);T(!nvnVertexStreamStateGetDivisor(NULL));BAD(EINVAL);T(!nvnVertexAttribStateGetStreamIndex(NULL));BAD(EINVAL);
 NVNcommandBuffer cb={0};T(api_nvnCommandBufferInitialize(&cb,&d));T(!nvnCommandBufferGetMemoryCallback(&cb));T(!nvnCommandBufferGetMemoryCallbackData(&cb));OK(api_nvnCommandBufferSetMemoryCallback(&cb,callback));OK(api_nvnCommandBufferSetMemoryCallbackData(&cb,sentinel));T(nvnCommandBufferGetMemoryCallback(&cb)==callback);T(nvnCommandBufferGetMemoryCallbackData(&cb)==sentinel);T(!callback_calls);OK(api_nvnCommandBufferSetMemoryCallback(&cb,NULL));OK(api_nvnCommandBufferSetMemoryCallbackData(&cb,NULL));T(!nvnCommandBufferGetMemoryCallback(&cb)&&!nvnCommandBufferGetMemoryCallbackData(&cb));
 /* All query paths are side-effect free: no image bytes touched, no queue. */
 for(size_t i=0;i<65536;i++)T(((unsigned char *)ram)[i]==0x73);
 OK(api_nvnCommandBufferFinalize(&cb));OK(api_nvnTextureFinalize(&tex));OK(api_nvnBufferFinalize(&b));OK(api_nvnMemoryPoolFinalize(&p));T(nvnBufferBuilderGetMemoryPool(&bb)==&p);T(!nn_nvn_error());T(nvnTextureBuilderGetMemoryPool(&tb)==&p);T(!nn_nvn_error());T(!nvnBufferGetSize(&b));BAD(EINVAL);T(!nvnTextureGetMemoryPool(&tex));BAD(EINVAL);free(ram);OK(api_nvnDeviceFinalize(&d));
 while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);printf("{\"passed\":true,\"checks\":%u,\"new_entries\":32,\"GPU_submissions\":0,\"callback_calls\":%u}\n",checks,callback_calls);return 0;
}
