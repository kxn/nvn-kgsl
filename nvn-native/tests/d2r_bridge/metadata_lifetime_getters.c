/* Actual public NVN getter APIs/owner core with RAM-only backend. Object-node
 * allocator reuse is forced to prove ABA safety, not to execute GPU commands. */
#define _POSIX_C_SOURCE 200809L
#include NN_METADATA_LIFETIME_CORE
#include NN_METADATA_LIFETIME_MEMORY_BACKEND
#include <stdio.h>
static unsigned checks,reuses;
static void *reuse_target,*held_node;
void *__real_calloc(size_t,size_t);
void __real_free(void *);
void *__wrap_calloc(size_t count,size_t bytes)
{
 if(held_node&&count==1&&bytes==sizeof(struct object)){
  void *p=held_node;held_node=NULL;reuse_target=NULL;memset(p,0,bytes);reuses++;return p;
 }
 return __real_calloc(count,bytes);
}
void __wrap_free(void *p)
{if(p&&p==reuse_target){assert(!held_node);held_node=p;return;}__real_free(p);}
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s sticky %d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(e) do{T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
#define LOAD(name,type) type name=(type)nvnBootstrapLoader(#name);T(name!=NULL)
int main(void)
{
#undef ENTRY
#define ENTRY(name,type) LOAD(name,type);
ENTRY(nvnQueueBuilderGetDevice, PFNNVNQUEUEBUILDERGETDEVICEPROC)
ENTRY(nvnMemoryPoolBuilderGetDevice, PFNNVNMEMORYPOOLBUILDERGETDEVICEPROC)
ENTRY(nvnBufferBuilderGetDevice, PFNNVNBUFFERBUILDERGETDEVICEPROC)
ENTRY(nvnTextureBuilderGetDevice, PFNNVNTEXTUREBUILDERGETDEVICEPROC)
ENTRY(nvnWindowBuilderGetDevice, PFNNVNWINDOWBUILDERGETDEVICEPROC)
ENTRY(nvnEventBuilderGetStorage, PFNNVNEVENTBUILDERGETSTORAGEPROC)
ENTRY(nvnEventBuilderGetMemoryPool, PFNNVNEVENTBUILDERGETMEMORYPOOLPROC)
ENTRY(nvnEventBuilderGetMemoryOffset, PFNNVNEVENTBUILDERGETMEMORYOFFSETPROC)
ENTRY(nvnEventGetMemoryPool, PFNNVNEVENTGETMEMORYPOOLPROC)
ENTRY(nvnEventGetMemoryOffset, PFNNVNEVENTGETMEMORYOFFSETPROC)
ENTRY(nvnWindowBuilderGetNumTextures, PFNNVNWINDOWBUILDERGETNUMTEXTURESPROC)
ENTRY(nvnWindowBuilderGetTexture, PFNNVNWINDOWBUILDERGETTEXTUREPROC)
ENTRY(nvnWindowGetNumTextures, PFNNVNWINDOWGETNUMTEXTURESPROC)
#undef ENTRY
 NVNdeviceBuilder db={0};NVNdevice d={0},d2={0};NVNqueueBuilder qb={0};NVNmemoryPoolBuilder pb={0};NVNbufferBuilder bb={0};NVNtextureBuilder tb={0};NVNwindowBuilder wb={0};NVNsamplerBuilder sb={0};
 OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&d,&db));
 OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnBufferBuilderSetDefaults(&bb));OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnWindowBuilderSetDefaults(&wb));OK(api_nvnSamplerBuilderSetDefaults(&sb));
 T(!nvnQueueBuilderGetDevice(&qb));T(!nvnMemoryPoolBuilderGetDevice(&pb));T(!nvnBufferBuilderGetDevice(&bb));T(!nvnTextureBuilderGetDevice(&tb));T(!nvnWindowBuilderGetDevice(&wb));
 OK(api_nvnQueueBuilderSetDevice(&qb,&d));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&d));OK(api_nvnBufferBuilderSetDevice(&bb,&d));OK(api_nvnTextureBuilderSetDevice(&tb,&d));OK(api_nvnWindowBuilderSetDevice(&wb,&d));OK(api_nvnSamplerBuilderSetDevice(&sb,&d));
 T(nvnQueueBuilderGetDevice(&qb)==&d);T(nvnMemoryPoolBuilderGetDevice(&pb)==&d);T(nvnBufferBuilderGetDevice(&bb)==&d);T(nvnTextureBuilderGetDevice(&tb)==&d);T(nvnWindowBuilderGetDevice(&wb)==&d);
 OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnWindowBuilderSetDefaults(&wb));T(nvnQueueBuilderGetDevice(&qb)==&d);T(nvnMemoryPoolBuilderGetDevice(&pb)==&d);T(nvnTextureBuilderGetDevice(&tb)==&d);T(nvnWindowBuilderGetDevice(&wb)==&d);T(find(&d,DEVICE)->refs==0);
 /* Force an actual differently-keyed owner to occupy exactly the removed
  * native node address. Getter identities survive, weak nodes do not revive. */
 struct object *old_device=find(&d,DEVICE);reuse_target=old_device;OK(api_nvnDeviceFinalize(&d));T(held_node==old_device);
 T(!find(&qb,QB)->device&&!find(&pb,PB)->device&&!find(&bb,BB)->device&&!find(&tb,TB)->device&&!find(&wb,WB)->device&&!find(&sb,SB)->device);
 T(nvnQueueBuilderGetDevice(&qb)==&d);T(nvnMemoryPoolBuilderGetDevice(&pb)==&d);T(nvnBufferBuilderGetDevice(&bb)==&d);T(nvnTextureBuilderGetDevice(&tb)==&d);T(nvnWindowBuilderGetDevice(&wb)==&d);T(api_nvnSamplerBuilderGetDevice(&sb)==&d);
 T(api_nvnDeviceInitialize(&d2,&db));T(find(&d2,DEVICE)==old_device&&reuses==1&&!held_node);NVNqueue q={0};T(!api_nvnQueueInitialize(&q,&qb));BAD(EINVAL);NVNmemoryPool p={0};T(!api_nvnMemoryPoolInitialize(&p,&pb));BAD(EINVAL);NVNsampler sampler={0};T(!api_nvnSamplerInitialize(&sampler,&sb));BAD(EINVAL);NVNwindow window={0};T(!api_nvnWindowInitialize(&window,&wb));BAD(EINVAL);
 OK(api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetSize2D(&tb,16,16));T(!api_nvnTextureBuilderGetStorageSize(&tb));BAD(ENOTSUP);T(find(&d2,DEVICE)->refs==0);
 /* Defaults must preserve known opaque keys without resurrecting a node. */
 OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnWindowBuilderSetDefaults(&wb));T(nvnQueueBuilderGetDevice(&qb)==&d&&!find(&qb,QB)->device);T(nvnMemoryPoolBuilderGetDevice(&pb)==&d&&!find(&pb,PB)->device);T(nvnTextureBuilderGetDevice(&tb)==&d&&!find(&tb,TB)->device);T(nvnWindowBuilderGetDevice(&wb)==&d&&!find(&wb,WB)->device);
 OK(api_nvnQueueBuilderSetDevice(&qb,&d2));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&d2));OK(api_nvnBufferBuilderSetDevice(&bb,&d2));OK(api_nvnTextureBuilderSetDevice(&tb,&d2));OK(api_nvnWindowBuilderSetDevice(&wb,&d2));T(nvnQueueBuilderGetDevice(&qb)==&d2);T(nvnMemoryPoolBuilderGetDevice(&pb)==&d2);T(nvnBufferBuilderGetDevice(&bb)==&d2);T(nvnTextureBuilderGetDevice(&tb)==&d2);T(nvnWindowBuilderGetDevice(&wb)==&d2);
 void *ram=NULL;T(!posix_memalign(&ram,4096,65536));memset(ram,0x62,65536);OK(api_nvnMemoryPoolBuilderSetStorage(&pb,ram,65536));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));T(api_nvnMemoryPoolInitialize(&p,&pb));
 NVNeventBuilder eb={0};NVNevent ev={0};OK(api_nvnEventBuilderSetDefaults(&eb));struct {uint32_t pre;int64_t offset;uint32_t post;} out={0x1234,-1,0xabcd};T(!nvnEventBuilderGetStorage(&eb,&out.offset));T(out.offset==0&&out.pre==0x1234&&out.post==0xabcd);T(!nvnEventBuilderGetMemoryPool(&eb)&&!nvnEventBuilderGetMemoryOffset(&eb));T(!nn_nvn_error());
 OK(api_nvnEventBuilderSetStorage(&eb,&p,32));out.offset=-1;T(nvnEventBuilderGetStorage(&eb,&out.offset)==&p);T(out.offset==32&&out.pre==0x1234&&out.post==0xabcd);T(nvnEventBuilderGetMemoryPool(&eb)==&p&&nvnEventBuilderGetMemoryOffset(&eb)==32);T(api_nvnEventInitialize(&ev,&eb));T(nvnEventGetMemoryPool(&ev)==&p&&nvnEventGetMemoryOffset(&ev)==32);T(api_nvnEventGetValue(&ev)==0x62626262);
 OK(api_nvnEventBuilderSetStorage(&eb,&p,64));T(nvnEventBuilderGetMemoryOffset(&eb)==64&&nvnEventGetMemoryOffset(&ev)==32);api_nvnMemoryPoolFinalize(&p);BAD(EBUSY);T(nvnEventGetMemoryPool(&ev)==&p);
 T(!nvnEventBuilderGetStorage(&eb,NULL));BAD(EINVAL);T(!nvnEventGetMemoryPool((const NVNevent *)&eb));BAD(EINVAL);
 OK(api_nvnBufferBuilderSetStorage(&bb,&p,256,512));OK(api_nvnTextureBuilderSetFlags(&tb,NVN_TEXTURE_FLAGS_LINEAR));OK(api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetSize2D(&tb,16,8));OK(api_nvnTextureBuilderSetStride(&tb,64));OK(api_nvnTextureBuilderSetStorage(&tb,&p,8192));
 OK(api_nvnEventFinalize(&ev));struct object *old_pool=find(&p,POOL);reuse_target=old_pool;OK(api_nvnMemoryPoolFinalize(&p));T(held_node==old_pool);T(!find(&eb,EVENT_BUILDER)->pool&&!find(&bb,BB)->pool&&!find(&tb,TB)->pool);T(nvnEventBuilderGetMemoryPool(&eb)==&p);T(nvnEventBuilderGetStorage(&eb,&out.offset)==&p&&out.offset==64);
 NVNmemoryPool p2={0};T(api_nvnMemoryPoolInitialize(&p2,&pb));T(find(&p2,POOL)==old_pool&&reuses==2&&!held_node);T(!api_nvnEventInitialize(&ev,&eb));BAD(EINVAL);NVNbuffer buffer={0};T(!api_nvnBufferInitialize(&buffer,&bb));BAD(EINVAL);NVNtexture texture={0};T(!api_nvnTextureInitialize(&texture,&tb));BAD(EINVAL);T(api_nvnBufferBuilderGetMemoryPool(&bb)==&p);T(!nn_nvn_error());T(api_nvnTextureBuilderGetMemoryPool(&tb)==&p);T(!nn_nvn_error());T(find(&p2,POOL)->refs==0);
 /* Returning the old opaque key did not bind the replacement owner or
  * revive the dead original. Repeated Initialize still rejects both. */
 T(!api_nvnBufferInitialize(&buffer,&bb));BAD(EINVAL);
 T(!api_nvnTextureInitialize(&texture,&tb));BAD(EINVAL);
 T(!find(&bb,BB)->pool&&!find(&tb,TB)->pool);
 T(find(&p2,POOL)->refs==0);
 /* Rebinding is explicit and restores the real pool/domain, not a stale node. */
 OK(api_nvnEventBuilderSetStorage(&eb,&p2,128));T(api_nvnEventInitialize(&ev,&eb));T(nvnEventGetMemoryPool(&ev)==&p2&&nvnEventGetMemoryOffset(&ev)==128);OK(api_nvnEventFinalize(&ev));OK(api_nvnEventBuilderSetDefaults(&eb));T(!nvnEventBuilderGetMemoryPool(&eb)&&!nvnEventBuilderGetMemoryOffset(&eb));
 /* Borrowed WindowBuilder array queries have no texture owner side effect. */
 T(!nvnWindowBuilderGetNumTextures(&wb));T(!nvnWindowBuilderGetTexture(&wb,0));BAD(EINVAL);NVNtexture *array[3]={(NVNtexture *)(uintptr_t)0x12345000,(NVNtexture *)(uintptr_t)0x23456000,NULL};OK(api_nvnWindowBuilderSetTextures(&wb,3,array));T(nvnWindowBuilderGetNumTextures(&wb)==3);T(nvnWindowBuilderGetTexture(&wb,0)==array[0]);array[0]=NULL;T(!nvnWindowBuilderGetTexture(&wb,0));T(!nn_nvn_error());T(!nvnWindowBuilderGetTexture(&wb,-1));BAD(EINVAL);T(!nvnWindowBuilderGetTexture(&wb,3));BAD(EINVAL);T(!nvnWindowGetNumTextures(&window));BAD(EINVAL);
 /* Reused opaque storage cannot retain another builder type's identity. */
 union{NVNwindowBuilder w;NVNbufferBuilder b;} reused;OK(api_nvnWindowBuilderSetDevice(&reused.w,&d2));T(nvnWindowBuilderGetDevice(&reused.w)==&d2);OK(api_nvnBufferBuilderSetDefaults(&reused.b));T(!nvnBufferBuilderGetDevice(&reused.b));
 for(size_t i=0;i<65536;i++)T(((unsigned char *)ram)[i]==0x62);
 OK(api_nvnMemoryPoolFinalize(&p2));free(ram);OK(api_nvnDeviceFinalize(&d2));T(!held_node&&!reuse_target);while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"new_entries\":13,\"forced_actual_node_reuses\":%u,\"GPU_submissions\":0}\n",checks,reuses);return 0;
}
