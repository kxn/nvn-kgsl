/* Public API native clear/Call job construction, no GPU/CPU shader execution. */
#define main prior_compute_fixture_main
#include "command_compute.c"
#undef main
#include NN_CLEAR_COLOR_HEADER
static void create_clear_fixture_texture(NVNtexture *key,NVNtextureBuilder *builder,NVNdevice *device,
 NVNmemoryPool *pool,unsigned format,unsigned width,unsigned height,size_t offset)
{
 OK(api_nvnTextureBuilderSetDefaults(builder));OK(api_nvnTextureBuilderSetDevice(builder,device));
 OK(api_nvnTextureBuilderSetSize2D(builder,width,height));OK(api_nvnTextureBuilderSetFormat(builder,(NVNformat)format));
 OK(api_nvnTextureBuilderSetStorage(builder,pool,(ptrdiff_t)offset));T(api_nvnTextureInitialize(key,builder));T(!nn_nvn_error());
}
static void target(NVNcommandBuffer *command,const NVNtexture *texture,int x,int y,int w,int h)
{const NVNtexture *colors[]={texture};OK(api_nvnCommandBufferSetRenderTargets(command,1,colors,NULL,NULL,NULL));OK(api_nvnCommandBufferSetScissor(command,x,y,w,h));}
static void large_ordinary(void)
{
 const struct nn_tex_source_desc desc={.target=1,.format=63,.width=2880,.height=1620,.layers=1,.levels=1,.block_height_log2=-1};
 struct nn_tex_source_layout layout;T(!nn_tex_source_layout(&desc,&layout));size_t size=(512+layout.payload_bytes+4095)&~(size_t)4095;
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));struct object *dev=find(&device,DEVICE);T(!nn_nvn_register_clear_color(&device,&NVN_COMPILED_IMAGE));
 NVNmemoryPoolBuilder pb[2]={0};NVNmemoryPool pools[2]={0};void *ram[2]={0};const size_t sizes[]={size,65536};
 for(unsigned i=0;i<2;i++){T(!posix_memalign(ram+i,4096,sizes[i]));memset(ram[i],0x73,sizes[i]);OK(api_nvnMemoryPoolBuilderSetDefaults(pb+i));OK(api_nvnMemoryPoolBuilderSetDevice(pb+i,&device));OK(api_nvnMemoryPoolBuilderSetFlags(pb+i,0x24));OK(api_nvnMemoryPoolBuilderSetStorage(pb+i,ram[i],sizes[i]));T(api_nvnMemoryPoolInitialize(pools+i,pb+i));}
 NVNtexture texture={0};NVNtextureBuilder builder={0};create_clear_fixture_texture(&texture,&builder,&device,pools,63,2880,1620,512);
 NVNcommandBuffer command={0};_Alignas(16)unsigned char controls[4096];T(api_nvnCommandBufferInitialize(&command,&device));OK(api_nvnCommandBufferAddCommandMemory(&command,pools+1,0,32768));OK(api_nvnCommandBufferAddControlMemory(&command,controls,4096));OK(api_nvnCommandBufferBeginRecording(&command));
 const NVNtexture *colors[]={&texture};NVNtextureView view={0};OK(api_nvnTextureViewSetDefaults(&view));OK(api_nvnTextureViewSetSwizzle(&view,NVN_TEXTURE_SWIZZLE_B,NVN_TEXTURE_SWIZZLE_G,NVN_TEXTURE_SWIZZLE_R,NVN_TEXTURE_SWIZZLE_A));const NVNtextureView *views[]={&view};
 BAD(api_nvnCommandBufferSetRenderTargets(&command,1,colors,views,NULL,NULL),ENOTSUP);T(!find(&command,COMMAND)->record->target);
 target(&command,&texture,127,29,2701,1511);const float color[]={.5f,.25f,1,0};OK(api_nvnCommandBufferClearColor(&command,0,color,10));
 struct command_copy *operation=find(&command,COMMAND)->record->current->copies;T(operation&&operation->job_count==3);struct job *uniform=operation->jobs[1];
 struct nn_clear_color_rect rect={127,29,2701,1511};struct nn_clear_color_plan expected;T(!nn_clear_color_plan(&layout,&rect,color,10,&expected));T(!memcmp(nn_bo_map(uniform->bo),&expected.args,64));T(expected.args.mask[0]==0x003ff800);
 NVNcommandHandle handle=api_nvnCommandBufferEndRecording(&command);T(handle&&!nn_nvn_error());OK(api_nvnCommandBufferFinalize(&command));
 NVNqueueBuilder qb={0};NVNqueue queue={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&device));T(api_nvnQueueInitialize(&queue,&qb));OK(api_nvnQueueSubmitCommands(&queue,1,&handle));T(nn_bo_idle(find(pools,POOL)->bo)==EBUSY);OK(api_nvnQueueFinish(&queue));
 for(size_t i=0;i<size;i++)T(((unsigned char*)ram[0])[i]==0x73);
 OK(api_nvnDeviceFinalizeCommandHandle(&device,handle));OK(api_nvnQueueFinalize(&queue));OK(api_nvnTextureFinalize(&texture));for(unsigned i=0;i<2;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(ram[i]);}T(!dev->gpu->bos);OK(api_nvnDeviceFinalize(&device));while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
}
int main(void)
{
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));struct object *dev=find(&device,DEVICE);
 T(!nn_nvn_register_clear_color(&device,&NVN_COMPILED_IMAGE));T(!nn_nvn_register_clear_color(&device,&NVN_COMPILED_IMAGE));
 struct nn_cs_image wrong=NVN_COMPILED_IMAGE;wrong.ssbo_slots=2;T(nn_nvn_register_clear_color(&device,&wrong)==ENOTSUP);
 wrong=NVN_COMPILED_IMAGE;T(nn_nvn_register_clear_color(&device,&wrong)==EBUSY);
 NVNmemoryPoolBuilder pb[2]={0};NVNmemoryPool pools[2]={0};void *ram[2]={0};
 for(unsigned i=0;i<2;i++){
  T(!posix_memalign(ram+i,4096,1024*1024));memset(ram[i],0x6d,1024*1024);
  OK(api_nvnMemoryPoolBuilderSetDefaults(pb+i));OK(api_nvnMemoryPoolBuilderSetDevice(pb+i,&device));OK(api_nvnMemoryPoolBuilderSetFlags(pb+i,0x24));OK(api_nvnMemoryPoolBuilderSetStorage(pb+i,ram[i],1024*1024));T(api_nvnMemoryPoolInitialize(pools+i,pb+i));
 }
 NVNtexture textures[2]={0};NVNtextureBuilder tb[2]={0};create_clear_fixture_texture(textures,tb,&device,pools,63,64,64,512);create_clear_fixture_texture(textures+1,tb+1,&device,pools,41,37,21,65536);
 struct object *old_target=find(textures,TEXTURE),*new_target=find(textures+1,TEXTURE);T(old_target->source_layout&&new_target->source_layout);
 NVNcommandBuffer child={0},parent={0};_Alignas(16)unsigned char controls[2][4096];T(api_nvnCommandBufferInitialize(&child,&device));T(api_nvnCommandBufferInitialize(&parent,&device));
 OK(api_nvnCommandBufferAddCommandMemory(&child,pools+1,0,32768));OK(api_nvnCommandBufferAddControlMemory(&child,controls[0],4096));
 OK(api_nvnCommandBufferBeginRecording(&child));target(&child,textures,1,2,21,13);NVNcommandHandle seed=api_nvnCommandBufferEndRecording(&child);T(seed&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&device,seed));
 /* This body consumes target/scissor inherited from its actual caller. */
 OK(api_nvnCommandBufferBeginRecording(&child));const float red[4]={1,.5f,.25f,1};OK(api_nvnCommandBufferClearColor(&child,0,red,5));
 struct handle *body=find(&child,COMMAND)->record->current;T(body->copies&&body->copies->kind==COMMAND_COPY_BOUND_CLEAR&&body->copies->job_count==3);
 NVNcommandHandle source=api_nvnCommandBufferEndRecording(&child);T(source&&!nn_nvn_error());
 OK(api_nvnCommandBufferAddCommandMemory(&parent,pools+1,32768,32768));OK(api_nvnCommandBufferAddControlMemory(&parent,controls[1],4096));OK(api_nvnCommandBufferBeginRecording(&parent));target(&parent,textures+1,3,4,17,9);OK(api_nvnCommandBufferCallCommands(&parent,1,&source));
 NVNcommandHandle top=api_nvnCommandBufferEndRecording(&parent);T(top&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&device,source));OK(api_nvnCommandBufferFinalize(&child));OK(api_nvnCommandBufferFinalize(&parent));
 BAD(api_nvnTextureFinalize(textures),EBUSY);BAD(api_nvnTextureFinalize(textures+1),EBUSY);
 NVNqueueBuilder qb={0};NVNqueue queue={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&device));T(api_nvnQueueInitialize(&queue,&qb));struct object *q=find(&queue,QUEUE);
 uint64_t previous_code=0,previous_uniform=0,previous_dispatch=0;unsigned fresh=0;struct handle *previous_projection=NULL;
 for(unsigned repeat=0;repeat<2;repeat++){
  struct nn_command_state initial={0},final={0};struct handle *projected=NULL;T(!command_project_sequence(q,lookup(top),&initial,&final,&projected));T(projected);
  T(final.scissor[0]==3&&final.scissor[1]==4&&final.scissor[2]==17&&final.scissor[3]==9);
  unsigned target_seen=0,old_seen=0;for(size_t i=0;i<projected->count;i++){target_seen+=projected->resources[i]==new_target;old_seen+=projected->resources[i]==old_target;}T(target_seen==1&&!old_seen);
  const struct job *code=projected->jobs,*uniform=code?code->next:NULL,*dispatch=uniform?uniform->next:NULL,*patch=dispatch?dispatch->next:NULL;
  T(code&&uniform&&dispatch&&patch&&!patch->next&&patch->projected_dispatch==2&&patch->command_offset==0&&patch->command_bytes==16);
  T(code->compute&&uniform->compute&&dispatch->compute&&dispatch->copy_destination==new_target&&dispatch->copy_destination_pool==find(pools,POOL));
  T(nn_bo_address(code->bo)!=previous_code&&nn_bo_address(uniform->bo)!=previous_uniform&&nn_bo_address(dispatch->bo)!=previous_dispatch);
  previous_code=nn_bo_address(code->bo);previous_uniform=nn_bo_address(uniform->bo);previous_dispatch=nn_bo_address(dispatch->bo);fresh++;
  struct nn_clear_color_rect rect={3,4,17,9};struct nn_clear_color_plan expected;T(!nn_clear_color_plan(new_target->source_layout,&rect,red,5,&expected));T(!memcmp(nn_bo_map(uniform->bo),&expected.args,64));
  const uint32_t *native=nn_bo_map(dispatch->bo);unsigned found_ubo=0;for(size_t i=0;i<NVN_COMPILED_IMAGE.patch_count;i++)if(NVN_COMPILED_IMAGE.patches[i].kind==NN_CS_UBO){T(pair64(native+NVN_COMPILED_IMAGE.patches[i].word)==nn_bo_address(uniform->bo)+NVN_COMPILED_IMAGE.patches[i].offset);found_ubo++;}T(found_ubo);
  const uint32_t *entry=nn_bo_map(patch->bo);T(pair64(entry+1)==nn_bo_address(dispatch->bo));T(entry[0]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
  /* Both versions coexist while comparing: an allocator may legally reuse
   * an address once an earlier projection is already destroyed. */
  if(previous_projection)command_body_destroy(previous_projection);
  previous_projection=projected;
  /* Unknown legacy jobs remain an atomic95; no successful RAW passthrough. */
  struct job unknown={.next=body->jobs};body->jobs=&unknown;unsigned before=submits;BAD(api_nvnQueueSubmitCommands(&queue,1,&top),ENOTSUP);T(submits==before);body->jobs=unknown.next;
  OK(api_nvnQueueSubmitCommands(&queue,1,&top));T(submitted_bytes==32);T(submitted[0]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));T(nn_bo_idle(find(pools,POOL)->bo)==EBUSY);BAD(api_nvnTextureFinalize(textures+1),EBUSY);
  OK(api_nvnQueueFinish(&queue));T(!dev->retired);for(unsigned i=0;i<1024*1024;i++)T(((unsigned char*)ram[0])[i]==0x6d);
 }
 command_body_destroy(previous_projection);T(submits==2&&fresh==2);OK(api_nvnDeviceFinalizeCommandHandle(&device,top));OK(api_nvnQueueFinalize(&queue));
 for(unsigned i=0;i<2;i++)OK(api_nvnTextureFinalize(textures+i));
 for(unsigned i=0;i<2;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(ram[i]);}
 T(!dev->gpu->bos);OK(api_nvnDeviceFinalize(&device));while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 large_ordinary();
 printf("{\"passed\":true,\"checks\":%u,\"Call_ClearColor_replays\":2,\"source_format\":63,\"effective_format\":41,\"partial_component_mask\":5,\"ordinary_R11_dimensions\":[2880,1620],\"custom_view_rejected\":true,\"GPU_execution\":false,\"CPU_shader_execution\":false}\n",checks);return 0;
}
