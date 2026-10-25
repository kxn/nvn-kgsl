/* HOST ONLY: real public NVN memory callback rebind/finalize, no GPU execution. */
#define main source_identity_fixture
#include NN_SOURCE_FIXTURE
#undef main
#include NN_PIPELINE_NATIVE_VIEWS
#include "pipeline_fixture_pair.h"
static unsigned callbacks,callback_checks;
struct callback_state {
 NVNcommandBuffer *command;NVNprogram *old_program,*new_program;
 NVNtexture *old_target,*new_target;NVNmemoryPool *arena,*arguments;
 unsigned indirect,mode;int program_error,target_error,argument_error[3];
};
static void refill(NVNcommandBuffer *command,NVNcommandBufferMemoryEvent event,size_t bytes,void *user)
{
 struct callback_state *c=user;T(command==c->command&&event==NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY&&bytes==16);callbacks++;
 OK(api_nvnCommandBufferBindProgram(command,c->new_program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
 const NVNtexture *targets[]={c->new_target};OK(api_nvnCommandBufferSetRenderTargets(command,1,targets,NULL,NULL,NULL));
 api_nvnProgramFinalize(c->old_program);c->program_error=nn_nvn_error();nn_nvn_clear_error();
 api_nvnTextureFinalize(c->old_target);c->target_error=nn_nvn_error();nn_nvn_clear_error();
 if(c->indirect)for(unsigned i=0;i<(c->indirect==2?1u:3u);i++){
  T(find(c->arguments+i,POOL)->refs==1);
  api_nvnMemoryPoolFinalize(c->arguments+i);c->argument_error[i]=nn_nvn_error();nn_nvn_clear_error();
 }
 if(c->mode!=1)OK(api_nvnCommandBufferAddCommandMemory(command,c->arena,4096,4096));
 if(c->mode>=2){metadata_allocations=0;metadata_fail_at=c->mode-1;}
}
static void make_pool(NVNdevice *d,NVNmemoryPoolBuilder *b,NVNmemoryPool *p,void **storage,size_t n)
{
 T(!posix_memalign(storage,4096,n));memset(*storage,0,n);
 OK(api_nvnMemoryPoolBuilderSetDefaults(b));OK(api_nvnMemoryPoolBuilderSetDevice(b,d));
 OK(api_nvnMemoryPoolBuilderSetStorage(b,*storage,n));OK(api_nvnMemoryPoolBuilderSetFlags(b,0x24));T(api_nvnMemoryPoolInitialize(p,b));
}
static int one(unsigned indirect,unsigned mode,unsigned overflow)
{
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *d=find(&dk,DEVICE);
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_pipeline_archive(&dk,archive,reader,ranges));
 T(!nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT));
 struct nn_pipeline_frontend_queries selection={.version=1,.chip=0x06050002,.primitive=4,.point_size=1};nn_render_state_defaults(&selection.render);
 const struct nn_pipeline_pair_recipe *pair=pipeline_fixture_color_pair(archive,&selection);T(pair);
 const struct nn_offline_graphics_stage *stages[]={archive->stages->stages+pair->vs_index,archive->stages->stages+pair->fs_index};
 enum{VS,FS,ARENA,DATA,OLD_TARGET,NEW_TARGET,ARG0,ARG1,ARG2,N};
 NVNmemoryPoolBuilder pb[N]={0};NVNmemoryPool pools[N]={0};void *storage[N]={0};
 for(unsigned i=0;i<N;i++)make_pool(&dk,pb+i,pools+i,storage+i,i==OLD_TARGET||i==NEW_TARGET?32768:16384);
 NVNshaderData shaders[2];unsigned char controls[2][0x898];range_count=0;allow(shaders,sizeof(shaders));
 for(unsigned i=0;i<2;i++){
  memcpy(storage[i],stages[i]->source,stages[i]->source_bytes);memcpy(controls[i],stages[i]->control,stages[i]->control_bytes);
  allow(storage[i],16384);allow(controls[i],sizeof(controls[i]));shaders[i]=(NVNshaderData){api_nvnMemoryPoolGetBufferAddress(pools+i),controls[i]};
 }
 NVNprogram programs[2]={0};for(unsigned i=0;i<2;i++){T(api_nvnProgramInitialize(programs+i,&dk));T(api_nvnProgramSetShaders(programs+i,2,shaders));}
 for(unsigned i=0;i<2;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);storage[i]=NULL;}
 NVNtextureBuilder tb[2]={0};NVNtexture textures[2]={0};
 for(unsigned i=0;i<2;i++){
  OK(api_nvnTextureBuilderSetDefaults(tb+i));OK(api_nvnTextureBuilderSetDevice(tb+i,&dk));OK(api_nvnTextureBuilderSetFlags(tb+i,NVN_TEXTURE_FLAGS_LINEAR));
  OK(api_nvnTextureBuilderSetSize2D(tb+i,64,64));OK(api_nvnTextureBuilderSetFormat(tb+i,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetStride(tb+i,256));
  OK(api_nvnTextureBuilderSetStorage(tb+i,pools+OLD_TARGET+i,4096));T(api_nvnTextureInitialize(textures+i,tb+i));
 }
 NVNcommandBuffer key={0};_Alignas(16)unsigned char control[1024];T(api_nvnCommandBufferInitialize(&key,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&key,pools+ARENA,0,8));OK(api_nvnCommandBufferAddControlMemory(&key,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&key));
 OK(api_nvnCommandBufferBindProgram(&key,programs,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
 NVNvertexAttribState attr[2]={0};for(unsigned i=0;i<2;i++){
  OK(api_nvnVertexAttribStateSetDefaults(attr+i));OK(api_nvnVertexAttribStateSetFormat(attr+i,i?NVN_FORMAT_RGBA8:NVN_FORMAT_RG32F,i?12:4));OK(api_nvnVertexAttribStateSetStreamIndex(attr+i,7));
 }
 OK(api_nvnCommandBufferBindVertexAttribState(&key,2,attr));NVNvertexStreamState streams[8]={0};
 for(unsigned i=0;i<8;i++)OK(api_nvnVertexStreamStateSetDefaults(streams+i));
 OK(api_nvnVertexStreamStateSetStride(streams+7,48));OK(api_nvnCommandBufferBindVertexStreamState(&key,8,streams));
 uint64_t data=api_nvnMemoryPoolGetBufferAddress(pools+DATA);OK(api_nvnCommandBufferBindVertexBuffer(&key,7,data+4096,4096));
 OK(api_nvnCommandBufferBindUniformBuffer(&key,NVN_SHADER_STAGE_VERTEX,0,data+8192,64));
 const NVNtexture *targets[]={textures};OK(api_nvnCommandBufferSetRenderTargets(&key,1,targets,NULL,NULL,NULL));
 OK(api_nvnCommandBufferSetViewport(&key,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&key,0,0,64,64));OK(api_nvnCommandBufferSetDepthRange(&key,0,1));
 struct callback_state state={.command=&key,.old_program=programs,.new_program=programs+1,.old_target=textures,.new_target=textures+1,.arena=pools+ARENA,.arguments=pools+ARG0,.indirect=indirect,.mode=mode};
 OK(api_nvnCommandBufferSetMemoryCallback(&key,refill));OK(api_nvnCommandBufferSetMemoryCallbackData(&key,&state));
 unsigned before_callbacks=callbacks,na=mock_allocations,nc=mock_caches,ns=mock_submits;
 struct object *overflow_pool=find(pools+OLD_TARGET,POOL);size_t old_pool_refs=overflow_pool->refs;
 if(overflow)overflow_pool->refs=SIZE_MAX;
 if(indirect)api_nvnCommandBufferMultiDrawElementsIndirectCount(&key,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,
  api_nvnMemoryPoolGetBufferAddress(pools+ARG0)+64,api_nvnMemoryPoolGetBufferAddress(pools+(indirect==2?ARG0:ARG1))+64,api_nvnMemoryPoolGetBufferAddress(pools+(indirect==2?ARG0:ARG2))+64,1,20);
 else api_nvnCommandBufferDrawArrays(&key,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3);
 int draw_error=nn_nvn_error();metadata_fail_at=0;nn_nvn_clear_error();
 T(callbacks==before_callbacks+(overflow?0u:1u));T(mock_allocations==na&&mock_caches==nc&&mock_submits==ns);
 if(overflow){T(draw_error==EOVERFLOW);overflow_pool->refs=old_pool_refs;}
 else{
  T(state.program_error==EBUSY&&state.target_error==EBUSY);callback_checks+=2;
  if(indirect)for(unsigned i=0;i<(indirect==2?1u:3u);i++){T(state.argument_error[i]==EBUSY);callback_checks++;}
 }
 struct recording *r=find(&key,COMMAND)->record;
 if(overflow){T(!r->current->draws&&!r->current->count);}
 else if(mode==0){
  T(r->current->count==(indirect==2?4u:indirect?6u:3u));
  T(!draw_error&&r->current->draws&&r->current->draws->program==find(programs,PROGRAM)&&r->current->draws->target==find(textures,TEXTURE));
  T(r->graphics_program==find(programs+1,PROGRAM)&&r->target==find(textures+1,TEXTURE));
  ERR(api_nvnProgramFinalize(programs),EBUSY);ERR(api_nvnTextureFinalize(textures),EBUSY);
 }else{
  T(draw_error==(mode==1?ENOSPC:ENOMEM)&&!r->current->draws&&!r->current->count);
  OK(api_nvnProgramFinalize(programs));OK(api_nvnTextureFinalize(textures));
 }
 OK(api_nvnCommandBufferFinalize(&key));
 if(mode==0){OK(api_nvnProgramFinalize(programs));OK(api_nvnTextureFinalize(textures));}
 OK(api_nvnProgramFinalize(programs+1));OK(api_nvnTextureFinalize(textures+1));
 for(unsigned i=ARENA;i<N;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}
 T(!d->gpu->bos);OK(api_nvnDeviceFinalize(&dk));
 while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB);remove_object(objects);}
 return 0;
}
int main(void){for(unsigned indirect=0;indirect<2;indirect++)for(unsigned mode=0;mode<(indirect?8u:5u);mode++)one(indirect,mode,0);
 one(2,0,0);one(0,0,1);T(callbacks==14&&callback_checks==53);
 printf("HOST ONLY public memory callback rebind/finalize cases=15 callbacks=%u callback_checks=%u checks=%u GPU execution=false\n",callbacks,callback_checks,checks);return 0;}
