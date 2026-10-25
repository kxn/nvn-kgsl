/* HOST ONLY real core, RAM ownership backend; no GPU/guest execution. */
#define main mode_baseline_main
#include NN_MODE_FRONTEND_FIXTURE
#undef main
static unsigned allocations,caches,submissions;
int __real_nn_bo_alloc(nn_device *,size_t,nn_bo **);
int __wrap_nn_bo_alloc(nn_device *d,size_t n,nn_bo **out)
{allocations++;return __real_nn_bo_alloc(d,n,out);}
int __real_nn_bo_cache(nn_bo *,size_t,size_t,int);
int __wrap_nn_bo_cache(nn_bo *b,size_t o,size_t n,int from)
{caches++;return __real_nn_bo_cache(b,o,n,from);}
int __real_nn_queue_submit(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,uint32_t *);
int __wrap_nn_queue_submit(nn_queue *q,nn_bo *b,size_t o,size_t n,nn_bo *const *r,size_t c,uint32_t *t)
{submissions++;return __real_nn_queue_submit(q,b,o,n,r,c,t);}
int __real_nn_queue_submit_waits(nn_queue *,nn_bo *,size_t,size_t,nn_bo *const *,size_t,const struct nn_queue_dependency *,size_t,uint32_t *);
int __wrap_nn_queue_submit_waits(nn_queue *q,nn_bo *b,size_t o,size_t n,nn_bo *const *r,size_t c,const struct nn_queue_dependency *d,size_t k,uint32_t *t)
{submissions++;return __real_nn_queue_submit_waits(q,b,o,n,r,c,d,k,t);}
static void rejected_before_work(NVNqueue *q,const NVNcommandHandle *ids,int expected)
{
 unsigned a=allocations,c=caches,s=submissions;
 ERR(api_nvnQueueSubmitCommands(q,2,ids),expected);
 T(allocations==a&&caches==c&&submissions==s);
}
int main(void)
{
 T(!mode_baseline_main());
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));
 struct object *dev=find(&device,DEVICE);NVNsamplerBuilder sb={0};NVNsampler sampler={0};
 /* Only the changed enum is checked: SetDevice/min/mag need not initialize
  * unrelated compare fields before SetDefaults. Defaults preserves device. */
 OK(api_nvnSamplerBuilderSetDevice(&sb,&device));
 OK(api_nvnSamplerBuilderSetMinMagFilter(&sb,NVN_MIN_FILTER_NEAREST,NVN_MAG_FILTER_NEAREST));
 OK(api_nvnSamplerBuilderSetDefaults(&sb));T(find(&sb,SB)->device==dev);
 unsigned char keys[2];struct object *arena=pool(dev,keys,4096),*descriptors=pool(dev,keys+1,16384);
 NVNsamplerPool sp={0};T(api_nvnSamplerPoolInitialize(&sp,descriptors->key,0,272));
 NVNqueueBuilder qb={0};NVNqueue queue={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&device));T(api_nvnQueueInitialize(&queue,&qb));
 NVNprogram program={0};T(api_nvnProgramInitialize(&program,&device));struct object *pr=find(&program,PROGRAM);
 struct nn_nvn_compiled_graphics compiled={.image=&NVN_GRAPHICS_IMAGE};pr->graphics=&compiled;
 struct handle first={.id=100,.device=dev,.commands=arena,.bytes=8},second={.id=101,.device=dev,.commands=arena,.bytes=8};
 struct late_draw draw={.program=pr,.sampler_pool=find(&sp,SPOOL),.sampler_id=259};
 first.next=&second;second.draws=&draw;T(!handles);handles=&first;
 const NVNcommandHandle ids[2]={100,101};unsigned unsupported=0;
 for(unsigned wrap=0;wrap<8;wrap++)for(unsigned mode=0;mode<2;mode++)for(unsigned func=1;func<=8;func++)for(unsigned reduction=0;reduction<3;reduction++){
  OK(api_nvnSamplerBuilderSetWrapMode(&sb,(NVNwrapMode)wrap,(NVNwrapMode)wrap,(NVNwrapMode)wrap));
  OK(api_nvnSamplerBuilderSetCompare(&sb,(NVNcompareMode)mode,(NVNcompareFunc)func));
  OK(api_nvnSamplerBuilderSetReductionFilter(&sb,(NVNsamplerReduction)reduction));
  NVNwrapMode x,y,z;NVNcompareMode m;NVNcompareFunc f;
  OK(api_nvnSamplerBuilderGetWrapMode(&sb,&x,&y,&z));T(x==(NVNwrapMode)wrap&&y==x&&z==x);
  OK(api_nvnSamplerBuilderGetCompare(&sb,&m,&f));T(m==(NVNcompareMode)mode&&f==(NVNcompareFunc)func);
  T(api_nvnSamplerBuilderGetReductionFilter(&sb)==(NVNsamplerReduction)reduction);
  const struct sampler_desc snapshot=find(&sb,SB)->sampler;
  T(api_nvnSamplerInitialize(&sampler,&sb));T(!memcmp(&find(&sampler,SAMPLER)->sampler,&snapshot,sizeof(snapshot)));
  OK(api_nvnSamplerPoolRegisterSampler(&sp,259,&sampler));OK(api_nvnSamplerFinalize(&sampler));
  T(!memcmp(&find(&sp,SPOOL)->slots[259].snapshot.sampler,&snapshot,sizeof(snapshot)));
  const struct nn_sampler_state state=native_sampler_state(&snapshot);int e=nn_sampler_state_validate(&state);
  if(e){unsupported++;rejected_before_work(&queue,ids,e);}
 }
 T(unsupported>0);
 struct object *builder=find(&sb,SB);struct sampler_desc before=builder->sampler;
 ERR(api_nvnSamplerBuilderSetMinMagFilter(&sb,(NVNminFilter)6,NVN_MAG_FILTER_LINEAR),EINVAL);T(!memcmp(&before,&builder->sampler,sizeof(before)));
 ERR(api_nvnSamplerBuilderSetMinMagFilter(&sb,NVN_MIN_FILTER_LINEAR,(NVNmagFilter)-1),EINVAL);T(!memcmp(&before,&builder->sampler,sizeof(before)));
 ERR(api_nvnSamplerBuilderSetWrapMode(&sb,NVN_WRAP_MODE_REPEAT,(NVNwrapMode)8,NVN_WRAP_MODE_REPEAT),EINVAL);T(!memcmp(&before,&builder->sampler,sizeof(before)));
 ERR(api_nvnSamplerBuilderSetCompare(&sb,(NVNcompareMode)2,NVN_COMPARE_FUNC_ALWAYS),EINVAL);T(!memcmp(&before,&builder->sampler,sizeof(before)));
 ERR(api_nvnSamplerBuilderSetCompare(&sb,NVN_COMPARE_MODE_NONE,(NVNcompareFunc)0),EINVAL);T(!memcmp(&before,&builder->sampler,sizeof(before)));
 ERR(api_nvnSamplerBuilderSetReductionFilter(&sb,(NVNsamplerReduction)3),EINVAL);T(!memcmp(&before,&builder->sampler,sizeof(before)));
 /* SDK float setters are stores, not clamps/rounding. Native consumption
  * remains strict; the full bit snapshot survives builder mutation/finalize. */
 const uint32_t bits[]={0x80000000u,0x7fc12345u,0x7f800000u,0xff800000u,0x40400000u,0x3f000001u};
 for(unsigned i=0;i<sizeof(bits)/sizeof(bits[0]);i++){
  float value,out;memcpy(&value,bits+i,4);OK(api_nvnSamplerBuilderSetDefaults(&sb));
  OK(api_nvnSamplerBuilderSetLodClamp(&sb,value,value));
  OK(api_nvnSamplerBuilderSetLodBias(&sb,value));OK(api_nvnSamplerBuilderSetMaxAnisotropy(&sb,value));OK(api_nvnSamplerBuilderSetLodSnap(&sb,value));
  float hi;OK(api_nvnSamplerBuilderGetLodClamp(&sb,&out,&hi));T(!memcmp(&out,&value,4)&&!memcmp(&hi,&value,4));
  out=api_nvnSamplerBuilderGetLodBias(&sb);T(!memcmp(&out,&value,4));out=api_nvnSamplerBuilderGetMaxAnisotropy(&sb);T(!memcmp(&out,&value,4));out=api_nvnSamplerBuilderGetLodSnap(&sb);T(!memcmp(&out,&value,4));
  T(api_nvnSamplerInitialize(&sampler,&sb));const struct sampler_desc snapshot=find(&sampler,SAMPLER)->sampler;
  OK(api_nvnSamplerPoolRegisterSamplerBuilder(&sp,259,&sb));OK(api_nvnSamplerBuilderSetDefaults(&sb));
  T(!memcmp(&find(&sp,SPOOL)->slots[259].snapshot.sampler,&snapshot,sizeof(snapshot)));
  OK(api_nvnSamplerGetLodClamp(&sampler,&out,&hi));T(!memcmp(&out,&value,4)&&!memcmp(&hi,&value,4));
  OK(api_nvnSamplerFinalize(&sampler));
  struct nn_sampler_state native=native_sampler_state(&snapshot);int e=nn_sampler_state_validate(&native);T(e);rejected_before_work(&queue,ids,e);
 }
 /* Missing slot remains a preflight error; registering the same slot after
  * recording is visible and is not rejected until actual native use. */
 draw.sampler_id=260;rejected_before_work(&queue,ids,EINVAL);draw.sampler_id=259;
 handles=NULL;OK(api_nvnProgramFinalize(&program));OK(api_nvnQueueFinalize(&queue));OK(api_nvnSamplerPoolFinalize(&sp));
 OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)arena->key));OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)descriptors->key));OK(api_nvnDeviceFinalize(&device));
 while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"gpu_submits\":0,\"native_rejections_before_work\":%u,\"scope\":\"HOST real CPU metadata and enqueue preflight; no GPU or original module execution\"}\n",checks,unsupported+7);return 0;
}
