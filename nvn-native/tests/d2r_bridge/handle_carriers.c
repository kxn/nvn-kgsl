/* Actual native core/pinned SDK carrier semantics; RAM ownership only, no GPU. */
#ifndef NN_NVN_MEMORY_BACKEND_SOURCE
#define NN_NVN_MEMORY_BACKEND_SOURCE "memory_backend.c"
#endif
#ifndef NN_NVN_CORE_SOURCE
#define NN_NVN_CORE_SOURCE "../../src/nvn.c"
#endif
#include NN_NVN_MEMORY_BACKEND_SOURCE
#include NN_NVN_CORE_SOURCE
#include <limits.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define OK(x) do{x;CHECK(!nn_nvn_error());}while(0)
#define BAD(x,e) do{x;CHECK(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
_Static_assert(sizeof(NVNtextureHandle)==8&&sizeof(NVNseparateTextureHandle)==8&&sizeof(NVNseparateSamplerHandle)==8,"public64-bit carriers");
_Static_assert(_Generic((PFNNVNDEVICEGETTEXTUREHANDLEPROC)0,NVNtextureHandle(*)(const NVNdevice*,int,int):1,default:0),"combined typed ABI");
_Static_assert(_Generic((PFNNVNDEVICEGETTEXELFETCHHANDLEPROC)0,NVNtextureHandle(*)(const NVNdevice*,int):1,default:0),"fetch typed ABI");
_Static_assert(_Generic((PFNNVNDEVICEGETSEPARATETEXTUREHANDLEPROC)0,NVNseparateTextureHandle(*)(const NVNdevice*,int):1,default:0),"separate texture typed ABI");
_Static_assert(_Generic((PFNNVNDEVICEGETSEPARATESAMPLERHANDLEPROC)0,NVNseparateSamplerHandle(*)(const NVNdevice*,int):1,default:0),"separate sampler typed ABI");
int main(void)
{
 PFNNVNDEVICEGETTEXTUREHANDLEPROC combined=(PFNNVNDEVICEGETTEXTUREHANDLEPROC)nvnBootstrapLoader("nvnDeviceGetTextureHandle");
 PFNNVNDEVICEGETTEXELFETCHHANDLEPROC fetch=(PFNNVNDEVICEGETTEXELFETCHHANDLEPROC)nvnBootstrapLoader("nvnDeviceGetTexelFetchHandle");
 PFNNVNDEVICEGETSEPARATETEXTUREHANDLEPROC texture=(PFNNVNDEVICEGETSEPARATETEXTUREHANDLEPROC)nvnBootstrapLoader("nvnDeviceGetSeparateTextureHandle");
 PFNNVNDEVICEGETSEPARATESAMPLERHANDLEPROC sampler=(PFNNVNDEVICEGETSEPARATESAMPLERHANDLEPROC)nvnBootstrapLoader("nvnDeviceGetSeparateSamplerHandle");
 CHECK(combined&&fetch&&texture&&sampler);
 NVNdeviceBuilder db;NVNdevice dev,dead;OK(api_nvnDeviceBuilderSetDefaults(&db));CHECK(api_nvnDeviceInitialize(&dev,&db));CHECK(!nn_nvn_error());
 CHECK((PFNNVNDEVICEGETTEXELFETCHHANDLEPROC)api_nvnDeviceGetProcAddress(&dev,"nvnDeviceGetTexelFetchHandle")==fetch);
 struct object *d=find(&dev,DEVICE);CHECK(d&&!d->gpu->bos&&!d->refs);
 /* Independent SDK instruction-derived boundary table, not the runtime helper. */
 const int tids[]={256,257,0xfffff};const int sids[]={256,2047,2048,4095};
 const uint64_t upper[]={UINT64_C(0x100000000),UINT64_C(0x100000000),UINT64_C(0xffffffff00000000),UINT64_C(0xffffffff00000000)};
 for(unsigned ti=0;ti<3;ti++){
  CHECK(texture(&dev,tids[ti]).value==(UINT64_C(0x100000000)|(uint32_t)tids[ti]));CHECK(!nn_nvn_error());
  CHECK(fetch(&dev,tids[ti])==texture(&dev,tids[ti]).value);CHECK(!nn_nvn_error());
  CHECK(api_nvnDeviceGetImageHandle(&dev,tids[ti])==(uint32_t)tids[ti]);CHECK(!nn_nvn_error());
  for(unsigned si=0;si<4;si++){
   uint64_t payload=(uint32_t)tids[ti]|((uint32_t)sids[si]<<20);
   uint64_t full=combined(&dev,tids[ti],sids[si]);CHECK(full==(upper[si]|payload));CHECK(!nn_nvn_error());
   CHECK(sampler(&dev,sids[si]).value==((uint64_t)sids[si]<<20));CHECK(!nn_nvn_error());
   uint32_t output=0xa5a5a5a5;CHECK(!texture_handle_payload(full,0,&output)&&output==(uint32_t)payload);
   CHECK(!texture_handle_payload(payload,0,&output)&&output==(uint32_t)payload);
   CHECK(!texture_handle_payload(sampler(&dev,sids[si]).value,2,&output)&&output==((uint32_t)sids[si]<<20));
  }
 }
 CHECK(!d->gpu->bos&&!d->refs); /* Constructors neither register nor allocate. */
 const int badtid[]={INT_MIN,-1,0,255,0x100000,INT_MAX};
 for(unsigned i=0;i<sizeof(badtid)/sizeof(*badtid);i++){
  CHECK(!texture(&dev,badtid[i]).value);CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
  CHECK(!fetch(&dev,badtid[i]));CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
  CHECK(!combined(&dev,badtid[i],256));CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
 }
 const int badsid[]={INT_MIN,-1,0,255,4096,INT_MAX};
 for(unsigned i=0;i<sizeof(badsid)/sizeof(*badsid);i++){
  CHECK(!sampler(&dev,badsid[i]).value);CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
  CHECK(!combined(&dev,256,badsid[i]));CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
 }
 CHECK(!texture(&dead,256).value);CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
 CHECK(!fetch(NULL,256));CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
 BAD(api_nvnDeviceSetDepthMode(&dev,(NVNdepthMode)99),ENOTSUP);
 api_nvnDeviceSetDepthMode(&dev,(NVNdepthMode)99);CHECK(nn_nvn_error()==ENOTSUP);
 CHECK(fetch(&dev,256)==UINT64_C(0x100000100));CHECK(nn_nvn_error()==ENOTSUP);
 CHECK(!fetch(&dev,-1));CHECK(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();
 /* Reject noncanonical high words without changing output on failure. SDK
  * release ignores them; this stronger native limit is an explicit policy. */
 const struct {uint64_t value;int domain;} invalid[]={
  {UINT64_C(0x200000100),1},{UINT64_C(0x100000100),2},
  {UINT64_C(0x101000100),0},{UINT64_C(0x20010000100),0},
  {UINT64_C(0x180000100),0}, /* bit31 set requires original SDK signextension */
  {UINT64_C(0xffffffff7ff00100),0}, /* positive payload may not signextend */
  {UINT64_C(0xffffffff80000000),2},{UINT64_MAX,1},{UINT64_C(0x100000000),1},
  {UINT64_C(0x100100100),1},{UINT64_C(0x100000101),2},{UINT64_C(0x100),2},
  {UINT64_C(0x00100100),0},{UINT64_C(0xfff000ff),0},{UINT64_C(0x100000),2}};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++){uint32_t output=0xa5a5a5a5;CHECK(texture_handle_payload(invalid[i].value,invalid[i].domain,&output)==EINVAL);CHECK(output==0xa5a5a5a5);}
 uint32_t output=0;CHECK(!texture_handle_payload(fetch(&dev,256),0,&output)&&output==256);
 CHECK(!texture_handle_payload(256,0,&output)&&output==256); /* legacy raw32 */
 for(int domain=0;domain<3;domain++){output=999;CHECK(!texture_handle_payload(0,domain,&output)&&!output);}
 CHECK(texture_handle_payload(0,3,&output)==EINVAL);CHECK(texture_handle_payload(0,0,NULL)==EINVAL);
 /* Actual public scalar/array bind ABI: white-box only marks the initialized
  * recording active; no GPU command or arena fake is involved in handle binds. */
 NVNcommandBuffer cb;CHECK(api_nvnCommandBufferInitialize(&cb,&dev));CHECK(!nn_nvn_error());
 struct recording *r=find(&cb,COMMAND)->record;r->active=1;
 for(unsigned si=0;si<4;si++){
  NVNtextureHandle h=combined(&dev,257,sids[si]);OK(api_nvnCommandBufferBindTexture(&cb,NVN_SHADER_STAGE_FRAGMENT,31,h));CHECK(r->combined[1][31]==(uint32_t)h);
  NVNseparateTextureHandle t=texture(&dev,257);NVNseparateSamplerHandle s=sampler(&dev,sids[si]);
  OK(api_nvnCommandBufferBindSeparateTexture(&cb,NVN_SHADER_STAGE_FRAGMENT,10,t));CHECK(r->separate_textures[1][10]==257);
  OK(api_nvnCommandBufferBindSeparateSampler(&cb,NVN_SHADER_STAGE_FRAGMENT,0,s));CHECK(r->separate_samplers[1][0]==((uint32_t)sids[si]<<20));
 }
 NVNtextureHandle list[]={combined(&dev,257,2048),fetch(&dev,258),0};
 uint32_t first_payload=(uint32_t)list[0];OK(api_nvnCommandBufferBindTextures(&cb,NVN_SHADER_STAGE_VERTEX,2,3,list));memset(list,0,sizeof(list));
 CHECK(r->combined[0][2]==first_payload&&r->combined[0][3]==258&&!r->combined[0][4]);CHECK(!r->combined[1][2]);
 CHECK(r->handle_carriers[0].guest[0][2]==UINT64_C(0xffffffff80000101));
 CHECK(r->handle_carriers[0].high[0][2]==UINT32_MAX);
 CHECK((r->handle_carriers[0].high_defined[0]&28u)==28u&&(r->handle_carriers[0].last_plural[0]&28u)==28u);
 /* Scalar changes last opaque argument/low payload but preserves a prior
  * plural-written high word. Neither metadata lane becomes a GPU address. */
 OK(api_nvnCommandBufferBindTexture(&cb,NVN_SHADER_STAGE_VERTEX,2,fetch(&dev,259)));
 CHECK(r->combined[0][2]==259&&r->handle_carriers[0].guest[0][2]==UINT64_C(0x100000103));
 CHECK(r->handle_carriers[0].high[0][2]==UINT32_MAX&&(r->handle_carriers[0].high_defined[0]&4u));
 CHECK(!(r->handle_carriers[0].last_plural[0]&4u));
 CHECK(!(r->handle_carriers[0].high_defined[1]&(UINT32_C(1)<<31))); /* scalar-only: unknown high */

 NVNseparateTextureHandle ts[]={texture(&dev,257),texture(&dev,258)};
 NVNseparateSamplerHandle ss[]={sampler(&dev,2048),sampler(&dev,4095)};
 OK(api_nvnCommandBufferBindSeparateTextures(&cb,NVN_SHADER_STAGE_VERTEX,4,2,ts));OK(api_nvnCommandBufferBindSeparateSamplers(&cb,NVN_SHADER_STAGE_VERTEX,4,2,ss));
 memset(ts,0,sizeof(ts));memset(ss,0,sizeof(ss));CHECK(r->separate_textures[0][4]==257&&r->separate_textures[0][5]==258);CHECK(r->separate_samplers[0][4]==UINT32_C(0x80000000)&&r->separate_samplers[0][5]==UINT32_C(0xfff00000));
 struct texture_handle_record metadata_before[3];memcpy(metadata_before,r->handle_carriers,sizeof(metadata_before));
 uint64_t before[2][32];memcpy(before,r->combined,sizeof(before));NVNtextureHandle badlist[]={combined(&dev,259,256),UINT64_C(0x200000100)};
 BAD(api_nvnCommandBufferBindTextures(&cb,NVN_SHADER_STAGE_VERTEX,2,2,badlist),EINVAL);CHECK(!memcmp(before,r->combined,sizeof(before)));CHECK(!memcmp(metadata_before,r->handle_carriers,sizeof(metadata_before)));
 BAD(api_nvnCommandBufferBindTexture(&cb,NVN_SHADER_STAGE_GEOMETRY,0,fetch(&dev,256)),ENOTSUP);CHECK(!memcmp(before,r->combined,sizeof(before)));
 BAD(api_nvnCommandBufferBindTexture(&cb,NVN_SHADER_STAGE_VERTEX,32,fetch(&dev,256)),ENOTSUP);CHECK(!memcmp(before,r->combined,sizeof(before)));
 OK(api_nvnCommandBufferBindTexture(&cb,NVN_SHADER_STAGE_FRAGMENT,31,0));CHECK(!r->combined[1][31]);
 r->active=0;BAD(api_nvnCommandBufferBindTexture(&cb,NVN_SHADER_STAGE_VERTEX,1,fetch(&dev,256)),EINVAL);
 OK(api_nvnCommandBufferFinalize(&cb));OK(api_nvnDeviceFinalize(&dev));
 CHECK(!texture(&dev,256).value);CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
 CHECK(!fetch(&dev,256));CHECK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();
 while(objects){CHECK(objects->kind==DB);remove_object(objects);}CHECK(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"new_cpu_constructor_entries\":1,\"gpu_executes\":0,\"legacy_raw32_policy\":true,\"original_sdk_carriers\":true}\n",checks);return 0;
}
