/* HOST ONLY: public typed NVN entries and real core, no GPU execution. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef NN_MEMORY_BACKEND_SOURCE
#define NN_MEMORY_BACKEND_SOURCE "memory_backend.c"
#endif
#ifndef NN_TEST_NVN_SOURCE
#define NN_TEST_NVN_SOURCE "../../src/nvn.c"
#endif
#include NN_MEMORY_BACKEND_SOURCE
#include NN_TEST_NVN_SOURCE
#include NN_PIPELINE_ARCHIVE
static unsigned checks,read_calls;static int read_error,read_short;static size_t control_read;
struct readable {const void *base;size_t size;};static struct readable ranges[16];static size_t range_count;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define ERR(x,n) do{x;T(nn_nvn_error()==(n));nn_nvn_clear_error();}while(0)
static long reader(void *user,void *out,const void *source,size_t bytes)
{
 T(user==ranges);read_calls++;
 if(read_error){errno=read_error;return -1;}
 for(size_t i=0;i<range_count;i++) {
  uintptr_t b=(uintptr_t)ranges[i].base,s=(uintptr_t)source;
  if(s>=b&&s-b<=ranges[i].size&&bytes<=ranges[i].size-(s-b)) {
   if(read_short){if(bytes)memcpy(out,source,bytes-1);return (long)bytes-1;}
   if(bytes==0x880||bytes==0x898)control_read=bytes;
   memcpy(out,source,bytes);errno=ERANGE;return (long)bytes;
  }
 }
 errno=EFAULT;return -1;
}
static void allow(const void *p,size_t n){T(range_count<16);ranges[range_count++]=(struct readable){p,n};}
#ifndef NN_EXPECT_DEFERRED_DEPTH
#define NN_EXPECT_DEFERRED_DEPTH 0
#endif
#if NN_EXPECT_DEFERRED_DEPTH
/* HOST negative only: satisfy every recorded requirement to prove that a
 * deferred source pair still cannot acquire a Draw recipe. These values are
 * never runtime state, an NVN query implementation, or game execution proof. */
struct deferred_queries {const struct nn_pipeline_archive *archive;uint32_t index[2];};
static int deferred_query(void *user,uint32_t stage,const struct nn_pipeline_query *q,uint32_t values[32],size_t *count)
{
 struct deferred_queries *f=user;if(stage>1)return EINVAL;
 const struct nn_pipeline_stage_requirements *s=f->archive->requirements+f->index[stage];
 T(s->stage_index==f->index[stage]);
 for(size_t i=0;i<s->query_count;i++){
  const struct nn_pipeline_query_requirement *r=s->queries+i;
  if(strcmp(q->name,r->query.name)||q->arguments!=r->query.arguments||q->type!=r->query.type||q->count!=r->query.count||memcmp(q->args,r->query.args,sizeof(q->args)))continue;
  memcpy(values,r->values,sizeof(r->values));*count=q->count;return 0;
 }
 return ENOTSUP;
}
#endif
int main(void)
{
 const struct nn_pipeline_archive *a=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 PFNNVNPROGRAMINITIALIZEPROC init=(PFNNVNPROGRAMINITIALIZEPROC)nvnBootstrapLoader("nvnProgramInitialize");
 PFNNVNPROGRAMSETSHADERSPROC set=(PFNNVNPROGRAMSETSHADERSPROC)nvnBootstrapLoader("nvnProgramSetShaders");
 PFNNVNPROGRAMFINALIZEPROC fini=(PFNNVNPROGRAMFINALIZEPROC)nvnBootstrapLoader("nvnProgramFinalize");
 PFNNVNCOMMANDBUFFERBINDPROGRAMPROC bind=(PFNNVNCOMMANDBUFFERBINDPROGRAMPROC)nvnBootstrapLoader("nvnCommandBufferBindProgram");
 PFNNVNCOMMANDBUFFERDRAWARRAYSPROC draw=(PFNNVNCOMMANDBUFFERDRAWARRAYSPROC)nvnBootstrapLoader("nvnCommandBufferDrawArrays");
 T(init&&set&&fini&&bind&&draw);
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *dev=find(&dk,DEVICE);T(dev);
 if(!NN_NVN_GRAPHICS_SOURCE_CACHE) {
  T(nn_nvn_register_pipeline_archive(&dk,a,reader,ranges)==ENOTSUP&&!dev->pipeline_archive);
  NVNprogram pk={0};T(init(&pk,&dk));NVNshaderData sh[2]={{1,a->stages->stages[0].control},{1,a->stages->stages[1].control}};unsigned before=read_calls;
  ERR(T(!set(&pk,2,sh)),ENOTSUP);T(read_calls==before);OK(fini(&pk));OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB);remove_object(objects);}
  printf("HOST ONLY NVN graphics source cache disabled checks=%u; GPU submits=0\n",checks);return 0;
 }
 T(nn_nvn_register_pipeline_archive(&dk,a,NULL,NULL)==EINVAL&&!dev->pipeline_archive);
 struct nn_pipeline_platform wrong=*a->platform;wrong.gmem_bytes+=4096;struct nn_pipeline_archive bad=*a;bad.platform=&wrong;
 T(nn_nvn_register_pipeline_archive(&dk,&bad,reader,ranges)==ENOTSUP&&!dev->pipeline_archive);
 struct nn_offline_graphics_stage invalid_stages[128];T(a->stages->stage_count<=128);memcpy(invalid_stages,a->stages->stages,a->stages->stage_count*sizeof(*invalid_stages));invalid_stages[0].code_bytes=1;
 struct nn_offline_graphics_archive invalid_source=*a->stages;invalid_source.stages=invalid_stages;bad=*a;bad.stages=&invalid_source;
 T(nn_nvn_register_pipeline_archive(&dk,&bad,reader,ranges)==EINVAL&&!dev->pipeline_archive);
 T(!nn_nvn_register_pipeline_archive(&dk,a,reader,ranges)&&dev->pipeline_archive==a);
 T(nn_nvn_register_pipeline_archive(&dk,a,reader,ranges)==EEXIST);
 unsigned deferred=0;
 for(size_t pair_index=0;pair_index<a->stages->pair_count;pair_index++) {
  const struct nn_offline_graphics_pair *pair=a->stages->pairs+pair_index;const struct nn_offline_graphics_stage *stages[]={a->stages->stages+pair->vs_index,a->stages->stages+pair->fs_index};
  NVNprogram pk={0};T(init(&pk,&dk));struct object *pr=find(&pk,PROGRAM);T(pr&&!pr->bo&&!pr->fs_bo&&!pr->pipeline_source.archive);
  NVNmemoryPoolBuilder pb[3];NVNmemoryPool pool[3];memset(pb,0,sizeof(pb));memset(pool,0,sizeof(pool));void *storage[3]={0};
  NVNshaderData shaders[2];unsigned char controls[2][0x898];range_count=0;allow(shaders,sizeof(shaders));
  for(unsigned i=0;i<3;i++) {
   T(!posix_memalign(storage+i,4096,8192));memset(storage[i],0xa5,8192);
   OK(api_nvnMemoryPoolBuilderSetDefaults(pb+i));OK(api_nvnMemoryPoolBuilderSetDevice(pb+i,&dk));
   OK(api_nvnMemoryPoolBuilderSetStorage(pb+i,storage[i],8192));OK(api_nvnMemoryPoolBuilderSetFlags(pb+i,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));
   T(api_nvnMemoryPoolInitialize(pool+i,pb+i));allow(storage[i],8192);
   if(i<2){memcpy(storage[i],stages[i]->source,stages[i]->source_bytes);memcpy(controls[i],stages[i]->control,stages[i]->control_bytes);allow(controls[i],stages[i]->control_bytes);shaders[i]=(NVNshaderData){api_nvnMemoryPoolGetBufferAddress(pool+i),controls[i]};}
  }
  unsigned reads=read_calls;errno=E2BIG;T(set(&pk,2,shaders)&&errno==E2BIG&&read_calls>reads&&!nn_nvn_error());
  T(pr->pipeline_source.archive==a&&!pr->bo&&!pr->fs_bo&&!pr->compiled&&!pr->graphics&&control_read==stages[1]->control_bytes);
#if NN_EXPECT_DEFERRED_DEPTH
  const struct nn_pipeline_shader_recipe *recipe=(const void*)1;
  int pair_error=nn_pipeline_select_pair(a,stages[0],stages[1],&recipe);
  if(pair_error==ENOTSUP){
   deferred++;T(!recipe);
   struct deferred_queries query={a,{pair->vs_index,pair->fs_index}};
   const struct nn_offline_graphics_stage *selected[2]={0};
   for(unsigned i=0;i<2;i++)T(!nn_pipeline_select_stage(a,i,stages[i]->source,stages[i]->source_bytes,stages[i]->control,stages[i]->control_bytes,deferred_query,&query,selected+i)&&selected[i]==stages[i]);
   recipe=(const void*)1;selected[0]=selected[1]=(const void*)1;
   T(nn_pipeline_source_resolve(&pr->pipeline_source,deferred_query,&query,selected,selected+1,&recipe)==ENOTSUP&&!selected[0]&&!selected[1]&&!recipe);
  }else T(!pair_error&&recipe);
#endif
  error(EDOM);T(set(&pk,2,shaders)&&nn_nvn_error()==EDOM);nn_nvn_clear_error();
  ERR(api_nvnDeviceFinalize(&dk),EBUSY);
  struct nn_pipeline_source_program saved=pr->pipeline_source;
  read_error=EACCES;errno=EDOM;T(!set(&pk,2,shaders)&&nn_nvn_error()==EACCES&&errno==EDOM&&!memcmp(&saved,&pr->pipeline_source,sizeof(saved)));nn_nvn_clear_error();read_error=0;
  read_short=1;ERR(T(!set(&pk,2,shaders)),EIO);read_short=0;T(!memcmp(&saved,&pr->pipeline_source,sizeof(saved)));
  const void *valid_control=shaders[0].control;shaders[0].control=(const void *)(UINTPTR_MAX-127);ERR(T(!set(&pk,2,shaders)),EOVERFLOW);shaders[0].control=valid_control;
  uint64_t valid_address=shaders[0].data;shaders[0].data=UINT64_MAX-127;ERR(T(!set(&pk,2,shaders)),ENOTSUP);shaders[0].data=valid_address;
  ERR(T(!set(&pk,2,(const NVNshaderData *)(uintptr_t)1)),EFAULT);T(!memcmp(&saved,&pr->pipeline_source,sizeof(saved)));
  ((unsigned char *)storage[1])[stages[1]->source_bytes-1]^=1;ERR(T(!set(&pk,2,shaders)),ENOTSUP);((unsigned char *)storage[1])[stages[1]->source_bytes-1]^=1;
  ((unsigned char *)storage[1])[0]^=1;ERR(T(!set(&pk,2,shaders)),ENOTSUP);((unsigned char *)storage[1])[0]^=1;
  if(stages[1]->source_bytes>stages[1]->code_bytes){((unsigned char *)storage[1])[stages[1]->code_bytes]^=1;ERR(T(!set(&pk,2,shaders)),ENOTSUP);((unsigned char *)storage[1])[stages[1]->code_bytes]^=1;}
  controls[1][stages[1]->control_bytes-1]^=1;ERR(T(!set(&pk,2,shaders)),ENOTSUP);controls[1][stages[1]->control_bytes-1]^=1;
  struct object *source_pool=find(pool,POOL);source_pool->bo->busy++;reads=read_calls;ERR(T(!set(&pk,2,shaders)),EBUSY);source_pool->bo->busy--;T(!memcmp(&saved,&pr->pipeline_source,sizeof(saved)));
  int old_flags=source_pool->flags;source_pool->flags|=NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS;ERR(T(!set(&pk,2,shaders)),ENOTSUP);source_pool->flags=old_flags;
  source_pool->gpu_policy=NN_NVN_GPU_UNCACHED_UNQUALIFIED;ERR(T(!set(&pk,2,shaders)),ENOTSUP);source_pool->gpu_policy=NN_NVN_GPU_POOL_CACHED;
  T(!memcmp(&saved,&pr->pipeline_source,sizeof(saved)));
  /* A match is a complete CPU snapshot; caller may overwrite/release source
   * pools and controls immediately, no source pointer survives in Program. */
  for(unsigned i=0;i<2;i++){memset(storage[i],0xcc,8192);memset(controls[i],0xcc,sizeof(controls[i]));OK(api_nvnMemoryPoolFinalize(pool+i));free(storage[i]);storage[i]=NULL;}
  T(!memcmp(&saved,&pr->pipeline_source,sizeof(saved)));
  NVNcommandBuffer ck={0};_Alignas(16)unsigned char command_control[512];T(api_nvnCommandBufferInitialize(&ck,&dk));
  OK(api_nvnCommandBufferAddCommandMemory(&ck,pool+2,0,8192));OK(api_nvnCommandBufferAddControlMemory(&ck,command_control,sizeof(command_control)));OK(api_nvnCommandBufferBeginRecording(&ck));
  OK(bind(&ck,&pk,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));struct recording *r=find(&ck,COMMAND)->record;T(r->graphics_program==pr&&pr->refs==3); /* CPU binding + VS/FS recorded state pins. */
  reads=read_calls;ERR(T(!set(&pk,2,(const NVNshaderData *)(uintptr_t)1)),EBUSY);T(read_calls==reads&&!memcmp(&saved,&pr->pipeline_source,sizeof(saved)));
  ERR(fini(&pk),EBUSY);size_t used=r->used,refs=r->current->count,cpu_refs=r->current->cpu_count;
  ERR(draw(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3),ENOTSUP);T(r->used==used&&r->current->count==refs&&r->current->cpu_count==cpu_refs&&!r->current->draws&&!r->current->jobs);
  OK(draw(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,0));T(r->used==used);NVNcommandHandle h=api_nvnCommandBufferEndRecording(&ck);T(h&&!nn_nvn_error());
  OK(api_nvnCommandBufferFinalize(&ck));T(pr->refs==2);ERR(fini(&pk),EBUSY);
  OK(api_nvnDeviceFinalizeCommandHandle(&dk,h));T(!pr->refs);OK(fini(&pk));OK(api_nvnMemoryPoolFinalize(pool+2));free(storage[2]);
  for(unsigned i=0;i<3;i++){struct object *builder=find(pb+i,PB);T(builder);remove_object(builder);}T(!dev->gpu->bos);
 }
 T(deferred==NN_EXPECT_DEFERRED_DEPTH);
 T(!handles);OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB);remove_object(objects);}
 printf("HOST ONLY typed NVN source-program checks=%u pairs=%zu deferred=%u; GPU submits=0; nonempty Draw unsupported=true\n",checks,a->stages->pair_count,deferred);return 0;
}
