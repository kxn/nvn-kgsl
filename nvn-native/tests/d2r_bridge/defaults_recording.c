/* HOST ONLY actual native core and RAM ownership backend; no GPU/guest. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include NN_CORE
static unsigned checks;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define ERR(x,e) do{x;T(nn_nvn_error()==e);nn_nvn_clear_error();}while(0)
int main(void)
{
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));T(!nn_nvn_error());
 NVNsamplerBuilder sb={0};NVNsampler sampler={0};
 PFNNVNSAMPLERBUILDERSETDEFAULTSPROC defaults=(PFNNVNSAMPLERBUILDERSETDEFAULTSPROC)nvnBootstrapLoader("nvnSamplerBuilderSetDefaults");T(defaults);
 OK(api_nvnSamplerBuilderSetDevice(&sb,&device));struct object*builder_object=find(&sb,SB),*dev=find(&device,DEVICE);T(builder_object->device==dev);
 OK(defaults(&sb));T(builder_object->device==dev);
 NVNminFilter min;NVNmagFilter mag;OK(api_nvnSamplerBuilderGetMinMagFilter(&sb,&min,&mag));T(min==NVN_MIN_FILTER_LINEAR&&mag==NVN_MAG_FILTER_LINEAR);
 NVNwrapMode s,t,r;OK(api_nvnSamplerBuilderGetWrapMode(&sb,&s,&t,&r));T(s==NVN_WRAP_MODE_REPEAT&&t==s&&r==s);
 float lo=-7,hi=-7;OK(api_nvnSamplerBuilderGetLodClamp(&sb,&lo,&hi));T(lo==0&&hi==1000);
 T(api_nvnSamplerBuilderGetLodBias(&sb)==0);T(api_nvnSamplerBuilderGetMaxAnisotropy(&sb)==1);T(api_nvnSamplerBuilderGetLodSnap(&sb)==0);T(api_nvnSamplerBuilderGetReductionFilter(&sb)==NVN_SAMPLER_REDUCTION_AVERAGE);T(!nn_nvn_error());
 NVNcompareMode mode;NVNcompareFunc func;OK(api_nvnSamplerBuilderGetCompare(&sb,&mode,&func));T(mode==NVN_COMPARE_MODE_NONE&&func==NVN_COMPARE_FUNC_LESS);
 float border[4]={7,7,7,7};OK(api_nvnSamplerBuilderGetBorderColor(&sb,border));for(unsigned i=0;i<4;i++)T(border[i]==0);
 /* Default LINEAR/REPEAT now has a native Adreno descriptor implementation. */
 T(api_nvnSamplerInitialize(&sampler,&sb));T(!nn_nvn_error());OK(api_nvnSamplerFinalize(&sampler));
 OK(api_nvnSamplerBuilderSetMinMagFilter(&sb,NVN_MIN_FILTER_NEAREST,NVN_MAG_FILTER_NEAREST));
 OK(api_nvnSamplerBuilderSetWrapMode(&sb,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE));
 OK(api_nvnSamplerBuilderSetLodClamp(&sb,lo,hi));T(api_nvnSamplerInitialize(&sampler,&sb));T(!nn_nvn_error());
 lo=hi=-7;OK(api_nvnSamplerGetLodClamp(&sampler,&lo,&hi));T(lo==0&&hi==1000);OK(api_nvnSamplerFinalize(&sampler));
 float clamps[][2]={{0,0},{-1000,1000},{0,1000}};
 for(unsigned i=0;i<3;i++){OK(api_nvnSamplerBuilderSetLodClamp(&sb,clamps[i][0],clamps[i][1]));T(api_nvnSamplerInitialize(&sampler,&sb));T(!nn_nvn_error());OK(api_nvnSamplerGetLodClamp(&sampler,&lo,&hi));T(lo==clamps[i][0]&&hi==clamps[i][1]);OK(api_nvnSamplerFinalize(&sampler));}
 OK(api_nvnSamplerBuilderSetLodClamp(&sb,NAN,1000));T(!sampler_profile(&builder_object->sampler));
 OK(api_nvnSamplerBuilderSetLodClamp(&sb,0,INFINITY));T(!sampler_profile(&builder_object->sampler));
 OK(api_nvnSamplerBuilderSetLodClamp(&sb,1001,1000));T(!sampler_profile(&builder_object->sampler));
 OK(defaults(&sb));OK(api_nvnSamplerBuilderGetMinMagFilter(&sb,&min,&mag));OK(api_nvnSamplerBuilderGetLodClamp(&sb,&lo,&hi));T(min==NVN_MIN_FILTER_LINEAR&&lo==0&&hi==1000);T(builder_object->device==dev);
 /* Real empty and CPU-only state recording, including ordinary arena pins. */
 void*storage=NULL;T(!posix_memalign(&storage,4096,4096));NVNmemoryPool pool={0};NVNmemoryPoolBuilder pb={0};
 OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&device));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,0x24));OK(api_nvnMemoryPoolBuilderSetStorage(&pb,storage,4096));T(api_nvnMemoryPoolInitialize(&pool,&pb));T(!nn_nvn_error());
 NVNcommandBuffer command={0};_Alignas(16) unsigned char control[256];T(api_nvnCommandBufferInitialize(&command,&device));T(!nn_nvn_error());
 OK(api_nvnCommandBufferAddCommandMemory(&command,&pool,0,4096));OK(api_nvnCommandBufferAddControlMemory(&command,control,sizeof(control)));
 for(unsigned state=0;state<2;state++){
  OK(api_nvnCommandBufferBeginRecording(&command));T(api_nvnCommandBufferIsRecording(&command));
  if(state){OK(api_nvnCommandBufferSetCopyRowStride(&command,256));OK(api_nvnCommandBufferSetCopyImageStride(&command,1024));OK(api_nvnCommandBufferSetMemoryCallbackData(&command,&sb));}
  NVNcommandHandle h=api_nvnCommandBufferEndRecording(&command);T(h);T(!nn_nvn_error());T(!api_nvnCommandBufferIsRecording(&command));
  struct handle*handle=handles;while(handle&&handle->id!=h)handle=handle->next;T(handle&&handle->bytes==8&&!handle->draws&&!handle->jobs&&!handle->count&&!handle->counter_ops);
  ERR(api_nvnMemoryPoolFinalize(&pool),EBUSY);OK(api_nvnDeviceFinalizeCommandHandle(&device,h));
 }
 OK(api_nvnCommandBufferFinalize(&command));OK(api_nvnMemoryPoolFinalize(&pool));free(storage);OK(api_nvnDeviceFinalize(&device));
 while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"scope\":\"HOST native metadata/empty recording; original SDK behavior statically proven, PM4 not executed\",\"gpu_submits\":0,\"game_modules_executed\":false}\n",checks);
 return 0;
}
