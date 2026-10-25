/* HOST ONLY actual core/RAM backend; no GPU or original module execution. */
#include NN_NVN_MEMORY_BACKEND_SOURCE
#include NN_NVN_CORE_SOURCE
static unsigned checks;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(x,e) do{x;T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
#define PFN(name,type) type name=(type)nvnBootstrapLoader(#name);T(name)
#define ABI(name,type) _Static_assert(_Generic(&api_##name,type:1,default:0),#name " exact PFN")
ABI(nvnSamplerBuilderGetDevice,PFNNVNSAMPLERBUILDERGETDEVICEPROC);
ABI(nvnSamplerBuilderSetBorderColori,PFNNVNSAMPLERBUILDERSETBORDERCOLORIPROC);
ABI(nvnSamplerBuilderSetBorderColorui,PFNNVNSAMPLERBUILDERSETBORDERCOLORUIPROC);
ABI(nvnSamplerBuilderGetBorderColori,PFNNVNSAMPLERBUILDERGETBORDERCOLORIPROC);
ABI(nvnSamplerBuilderGetBorderColorui,PFNNVNSAMPLERBUILDERGETBORDERCOLORUIPROC);
ABI(nvnSamplerGetBorderColori,PFNNVNSAMPLERGETBORDERCOLORIPROC);
ABI(nvnSamplerGetBorderColorui,PFNNVNSAMPLERGETBORDERCOLORUIPROC);
int main(void)
{
 PFN(nvnSamplerBuilderGetDevice,PFNNVNSAMPLERBUILDERGETDEVICEPROC);
 PFN(nvnSamplerBuilderSetBorderColori,PFNNVNSAMPLERBUILDERSETBORDERCOLORIPROC);
 PFN(nvnSamplerBuilderSetBorderColorui,PFNNVNSAMPLERBUILDERSETBORDERCOLORUIPROC);
 PFN(nvnSamplerBuilderGetBorderColori,PFNNVNSAMPLERBUILDERGETBORDERCOLORIPROC);
 PFN(nvnSamplerBuilderGetBorderColorui,PFNNVNSAMPLERBUILDERGETBORDERCOLORUIPROC);
 PFN(nvnSamplerGetBorderColori,PFNNVNSAMPLERGETBORDERCOLORIPROC);
 PFN(nvnSamplerGetBorderColorui,PFNNVNSAMPLERGETBORDERCOLORUIPROC);
 T(!nvnBootstrapLoader("nvnSamplerGetDevice"));T(!nn_nvn_error());
 NVNdeviceBuilder db={0};NVNdevice device={0},invalid_device={0};
 OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));T(!nn_nvn_error());
 NVNsamplerBuilder sb={0},unknown={0};NVNsampler sm={0},unknown_sampler={0};
 OK(api_nvnSamplerBuilderSetDefaults(&sb));T(nvnSamplerBuilderGetDevice(&sb)==NULL);T(!nn_nvn_error());
 OK(api_nvnSamplerBuilderSetDevice(&sb,&device));OK(api_nvnSamplerBuilderSetDefaults(&sb));
 T(nvnSamplerBuilderGetDevice(&sb)==&device);T(!nn_nvn_error());
 BAD(api_nvnSamplerBuilderSetDevice(&sb,&invalid_device),EINVAL);T(nvnSamplerBuilderGetDevice(&sb)==&device);T(!nn_nvn_error());
 const uint32_t patterns[][4]={{0,0x80000000u,0x7fffffffu,0xffffffffu},{0x3f800000u,0x40000000u,0x7fc12345u,0xff812345u},{1,0x7f800000u,0xff800000u,0xdeadbeefu},{0x01020304u,0x89abcdefu,0x76543210u,0xfedcba98u}};
 for(unsigned i=0;i<4;i++)for(unsigned setter=0;setter<3;setter++){
  uint32_t words[4],uout[4];int ints[4],iout[4];float floats[4],fout[4];
  memcpy(words,patterns[i],16);memcpy(ints,patterns[i],16);memcpy(floats,patterns[i],16);
  if(setter==0)OK(nvnSamplerBuilderSetBorderColori(&sb,ints));else if(setter==1)OK(nvnSamplerBuilderSetBorderColorui(&sb,words));else OK(api_nvnSamplerBuilderSetBorderColor(&sb,floats));
  memset(words,0,16);memset(ints,0,16);memset(floats,0,16);
  OK(nvnSamplerBuilderGetBorderColori(&sb,iout));T(!memcmp(iout,patterns[i],16));OK(nvnSamplerBuilderGetBorderColorui(&sb,uout));T(!memcmp(uout,patterns[i],16));OK(api_nvnSamplerBuilderGetBorderColor(&sb,fout));T(!memcmp(fout,patterns[i],16));
  T(api_nvnSamplerInitialize(&sm,&sb));T(!nn_nvn_error());const uint32_t changed[4]={1,2,3,4};OK(nvnSamplerBuilderSetBorderColorui(&sb,changed));
  OK(nvnSamplerGetBorderColori(&sm,iout));T(!memcmp(iout,patterns[i],16));OK(nvnSamplerGetBorderColorui(&sm,uout));T(!memcmp(uout,patterns[i],16));OK(api_nvnSamplerGetBorderColor(&sm,fout));T(!memcmp(fout,patterns[i],16));
  BAD(api_nvnDeviceFinalize(&device),EBUSY);OK(api_nvnSamplerFinalize(&sm));
 }
 uint32_t saved[4],out[4];OK(nvnSamplerBuilderGetBorderColorui(&sb,saved));BAD(nvnSamplerBuilderSetBorderColori(&sb,NULL),EINVAL);BAD(nvnSamplerBuilderSetBorderColorui(&sb,NULL),EINVAL);OK(nvnSamplerBuilderGetBorderColorui(&sb,out));T(!memcmp(saved,out,16));
 BAD(nvnSamplerBuilderGetBorderColori(&sb,NULL),EINVAL);BAD(nvnSamplerBuilderGetBorderColorui(&sb,NULL),EINVAL);T(api_nvnSamplerInitialize(&sm,&sb));T(!nn_nvn_error());BAD(nvnSamplerGetBorderColori(&sm,NULL),EINVAL);BAD(nvnSamplerGetBorderColorui(&sm,NULL),EINVAL);OK(api_nvnSamplerFinalize(&sm));
 memset(out,0xa5,16);uint32_t untouched[4];memcpy(untouched,out,16);int signed_saved[4],signed_out[4];memcpy(signed_saved,saved,16);memcpy(signed_out,out,16);
 BAD(nvnSamplerBuilderSetBorderColori(&unknown,signed_saved),EINVAL);BAD(nvnSamplerBuilderSetBorderColorui(&unknown,saved),EINVAL);
 BAD(nvnSamplerBuilderGetBorderColori(&unknown,signed_out),EINVAL);T(!memcmp(signed_out,untouched,16));BAD(nvnSamplerBuilderGetBorderColorui(&unknown,out),EINVAL);T(!memcmp(out,untouched,16));
 BAD(nvnSamplerGetBorderColori(&unknown_sampler,signed_out),EINVAL);T(!memcmp(signed_out,untouched,16));BAD(nvnSamplerGetBorderColorui(&unknown_sampler,out),EINVAL);T(!memcmp(out,untouched,16));T(nvnSamplerBuilderGetDevice(&unknown)==NULL);T(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
 error(EDOM);T(nvnSamplerBuilderGetDevice(&sb)==&device);T(nn_nvn_error()==EDOM);nvnSamplerBuilderGetBorderColorui(&sb,out);T(nn_nvn_error()==EDOM);T(!memcmp(out,saved,16));nvnSamplerBuilderSetBorderColorui(&sb,patterns[0]);T(nn_nvn_error()==EDOM);nvnSamplerBuilderGetBorderColorui(&unknown,out);T(nn_nvn_error()==EDOM);nn_nvn_clear_error();OK(nvnSamplerBuilderGetBorderColorui(&sb,out));T(!memcmp(out,patterns[0],16));
 /* No private-device dereference after Finalize; Defaults retains weak key. */
 OK(api_nvnDeviceFinalize(&device));T(nvnSamplerBuilderGetDevice(&sb)==&device);T(!nn_nvn_error());OK(api_nvnSamplerBuilderSetDefaults(&sb));T(nvnSamplerBuilderGetDevice(&sb)==&device);T(!nn_nvn_error());OK(nvnSamplerBuilderSetBorderColorui(&sb,patterns[1]));OK(nvnSamplerBuilderGetBorderColorui(&sb,out));T(!memcmp(out,patterns[1],16));T(!api_nvnSamplerInitialize(&sm,&sb));T(nn_nvn_error()==EINVAL);nn_nvn_clear_error();for(struct object *o=objects;o;o=o->next)T(o->key!=&sm);
 /* A new Device may reuse the freed private allocation. The original public
  * binding must not silently retarget to a different caller-owned Device. */
 NVNdevice replacement={0};T(api_nvnDeviceInitialize(&replacement,&db));T(!nn_nvn_error());
 T(!api_nvnSamplerInitialize(&sm,&sb));T(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
 T(nvnSamplerBuilderGetDevice(&sb)==&device);T(!nn_nvn_error());
 OK(api_nvnSamplerBuilderSetDevice(&sb,&replacement));T(nvnSamplerBuilderGetDevice(&sb)==&replacement);T(api_nvnSamplerInitialize(&sm,&sb));T(!nn_nvn_error());OK(api_nvnSamplerFinalize(&sm));OK(api_nvnDeviceFinalize(&replacement));
 OK(api_nvnBufferBuilderSetDefaults((NVNbufferBuilder*)&sb));OK(api_nvnSamplerBuilderSetDefaults(&sb));T(nvnSamplerBuilderGetDevice(&sb)==NULL);T(!nn_nvn_error());
 while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"new_entries\":7,\"gpu_submits\":0,\"scope\":\"HOST exact typed aliases/raw32 snapshot and public weak Device lifetime; no GPU/guest\"}\n",checks);return 0;
}
