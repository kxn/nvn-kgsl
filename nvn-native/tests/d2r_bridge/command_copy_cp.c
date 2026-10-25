/* HOST native CP/clear/ownership checks. Uses the existing nonexecuting
 * retained queue; no shader, PM4 packet, or image computation is executed. */
#define main previous_compute_fixture_main
#include "command_compute.c"
#undef main
static void linear_texture(NVNtexture *key,NVNtextureBuilder *builder,NVNdevice *device,NVNmemoryPool *pool,size_t offset,int width,int height)
{
 OK(api_nvnTextureBuilderSetDefaults(builder));OK(api_nvnTextureBuilderSetDevice(builder,device));
 OK(api_nvnTextureBuilderSetTarget(builder,NVN_TEXTURE_TARGET_2D));OK(api_nvnTextureBuilderSetSize2D(builder,width,height));
 OK(api_nvnTextureBuilderSetFlags(builder,NVN_TEXTURE_FLAGS_LINEAR));OK(api_nvnTextureBuilderSetFormat(builder,NVN_FORMAT_RGBA8));
 OK(api_nvnTextureBuilderSetStride(builder,width*4));OK(api_nvnTextureBuilderSetStorage(builder,pool,(ptrdiff_t)offset));T(api_nvnTextureInitialize(key,builder));T(!nn_nvn_error());
}
static void cp_cases(void)
{
 submits=submit_error=0;NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));struct object *dev=find(&device,DEVICE);
 NVNmemoryPoolBuilder pb[2]={0};NVNmemoryPool pools[2]={0};void *ram[2]={0};size_t sizes[2]={131072,65536};
 for(unsigned i=0;i<2;i++){T(!posix_memalign(ram+i,4096,sizes[i]));memset(ram[i],0x6a,sizes[i]);OK(api_nvnMemoryPoolBuilderSetDefaults(pb+i));OK(api_nvnMemoryPoolBuilderSetDevice(pb+i,&device));OK(api_nvnMemoryPoolBuilderSetFlags(pb+i,0x24));OK(api_nvnMemoryPoolBuilderSetStorage(pb+i,ram[i],sizes[i]));T(api_nvnMemoryPoolInitialize(pools+i,pb+i));}
 uint64_t base=api_nvnMemoryPoolGetBufferAddress(pools);NVNtexture textures[4]={0};NVNtextureBuilder tb[4]={0};
 linear_texture(textures,tb,&device,pools,0,64,64);linear_texture(textures+1,tb+1,&device,pools,16384,64,64);
 linear_texture(textures+2,tb+2,&device,pools,32768,16,8);linear_texture(textures+3,tb+3,&device,pools,40960,16,8);
 NVNcommandBuffer child={0},parent={0};_Alignas(16)unsigned char controls[2][4096];T(api_nvnCommandBufferInitialize(&child,&device));T(api_nvnCommandBufferInitialize(&parent,&device));
 OK(api_nvnCommandBufferAddCommandMemory(&child,pools+1,0,32768));OK(api_nvnCommandBufferAddControlMemory(&child,controls[0],4096));
 OK(api_nvnCommandBufferBeginRecording(&child));const NVNtexture *old_target=textures;
 OK(api_nvnCommandBufferSetRenderTargets(&child,1,&old_target,NULL,NULL,NULL));OK(api_nvnCommandBufferSetScissor(&child,0,0,64,64));
 NVNcommandHandle seed=api_nvnCommandBufferEndRecording(&child);T(seed&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&device,seed));
 OK(api_nvnCommandBufferBeginRecording(&child));OK(api_nvnCommandBufferSetCopyRowStride(&child,80));OK(api_nvnCommandBufferSetCopyImageStride(&child,640));
 OK(api_nvnCommandBufferCopyBufferToBuffer(&child,base+81920,base+73728,64,0));
 float clear[4]={.25f,.5f,.75f,1};NVNcopyRegion region={0,0,0,16,8,1};OK(api_nvnCommandBufferClearTexture(&child,textures+2,NULL,&region,clear,15));
 unsigned char uniform[17];for(unsigned i=0;i<17;i++)uniform[i]=(unsigned char)(i+19);uint32_t expected_uniform[4];memcpy(expected_uniform,uniform+1,16);
 OK(api_nvnCommandBufferUpdateUniformBuffer(&child,base+65536,64,4,16,uniform+1));memset(uniform,0xee,sizeof(uniform));
 OK(api_nvnCommandBufferCopyBufferToTexture(&child,base+81920,textures+2,NULL,&region,0));OK(api_nvnCommandBufferCopyTextureToBuffer(&child,textures+2,NULL,&region,base+86016,0));
 OK(api_nvnCommandBufferCopyTextureToTexture(&child,textures+2,NULL,&region,textures+3,NULL,&region,0));
 OK(api_nvnCommandBufferClearColor(&child,0,clear,15));OK(api_nvnCommandBufferClearBuffer(&child,base+74240,64,0x12345678));
 NVNcommandHandle source=api_nvnCommandBufferEndRecording(&child);T(source&&!nn_nvn_error());struct handle *source_owner=lookup(source);T(source_owner&&source_owner->copies);
 unsigned operations=0;const struct command_copy *raw=NULL;for(const struct command_copy *c=source_owner->copies;c;c=c->next){operations++;if(c->kind==COMMAND_COPY_RAW_CLEAR)raw=c;}T(operations==8&&raw&&raw->command_bytes==76);
 OK(api_nvnCommandBufferAddCommandMemory(&parent,pools+1,32768,32768));OK(api_nvnCommandBufferAddControlMemory(&parent,controls[1],4096));OK(api_nvnCommandBufferBeginRecording(&parent));const NVNtexture *new_target=textures+1;
 OK(api_nvnCommandBufferSetRenderTargets(&parent,1,&new_target,NULL,NULL,NULL));OK(api_nvnCommandBufferSetScissor(&parent,0,0,64,64));OK(api_nvnCommandBufferCallCommands(&parent,1,&source));
 /* No parent CPU target from the child is fabricated. This operation is
  * resolved at the same inherited-state boundary as deferred Draw/Dispatch. */
 float second[4]={1,0,.5f,1};OK(api_nvnCommandBufferClearColor(&parent,0,second,15));OK(api_nvnCommandBufferSetCopyRowStride(&parent,1));OK(api_nvnCommandBufferSetCopyImageStride(&parent,1));
 NVNcommandHandle top=api_nvnCommandBufferEndRecording(&parent);T(top&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&device,source));OK(api_nvnCommandBufferFinalize(&child));OK(api_nvnCommandBufferFinalize(&parent));
 unsigned char *before=malloc(sizes[1]);T(before);memcpy(before,ram[1],sizes[1]);
 NVNqueueBuilder qb={0};NVNqueue queue={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&device));T(api_nvnQueueInitialize(&queue,&qb));struct object *q=find(&queue,QUEUE);
 struct nn_command_state initial={0},final={0};struct handle *projected=NULL;T(!command_project_sequence(q,lookup(top),&initial,&final,&projected));
 T(nn_command_state_binding(&final,NN_COMMAND_TARGET,0,0)->owner==find(textures+1,TEXTURE));T((final.defined&NN_COMMAND_STATE_SCISSOR)&&final.scissor[2]==64);
 unsigned jobs=0,patches=0,new_clear=0,ubo=0;
 for(struct job *j=projected->jobs;j;j=j->next){
  jobs++;patches+=j->projected_dispatch==2;
  for(const struct job *old=source_owner->jobs;old;old=old->next)T(old->bo!=j->bo);
  if(j->copy_destination_pool==find(pools,POOL)&&j->copy_destination_offset==16384){
   new_clear++;const uint32_t *words=nn_bo_map(j->bo);unsigned seen=0;
   for(size_t i=0;i+4<nn_bo_size(j->bo)/4;i++)if(words[i]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,66)){
    if(words[i]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,66)){T(pair64(words+i+1)>=base+16384&&pair64(words+i+1)<base+32768);T(words[i+3]==0xffbf8040||words[i+3]==0xff8000ff);seen++;}
   }T(seen==64);
  }
  if(j->copy_destination_pool==find(pools,POOL)&&j->copy_destination_offset==65540){
   ubo++;const uint32_t *words=nn_bo_map(j->bo);unsigned seen=0;
   for(size_t i=0;i+7<nn_bo_size(j->bo)/4;i++)if(words[i]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,6)){T(pair64(words+i+1)==base+65540);T(!memcmp(words+i+3,expected_uniform,16));seen++;}
   T(seen==1);
  }
 }
 T(jobs==16&&patches==8&&new_clear==2&&ubo==1);
 void *staged=NULL;T(!queue_stage_commands(q,projected,&staged));const uint32_t *raw_words=(const uint32_t*)((char*)staged+raw->command_offset);
 T(raw_words[0]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,18)&&pair64(raw_words+1)==base+74240);for(unsigned i=0;i<16;i++)T(raw_words[i+3]==0x12345678);
 size_t expanded=projected->bytes;command_body_destroy(projected);T(expanded<sizeof(submitted));
 size_t bos=dev->gpu->bos,capacity=q->queue_config.command;q->queue_config.command=expanded-1;
 BAD(api_nvnQueueSubmitCommands(&queue,1,&top),ENOSPC);T(!submits&&!q->call_queue_poisoned&&dev->gpu->bos==bos);q->queue_config.command=capacity;
 NVNcommandBuffer alias={0};T(api_nvnCommandBufferInitialize(&alias,&device));OK(api_nvnCommandBufferAddCommandMemory(&alias,pools,73728,4096));
 BAD(api_nvnQueueSubmitCommands(&queue,1,&top),ENOTSUP);T(!submits&&!q->call_queue_poisoned&&dev->gpu->bos==bos);OK(api_nvnCommandBufferFinalize(&alias));
 for(unsigned repeat=0;repeat<2;repeat++){OK(api_nvnQueueSubmitCommands(&queue,1,&top));T(submitted_bytes==expanded);OK(api_nvnQueueFinish(&queue));T(!memcmp(before,ram[1],sizes[1]));for(size_t i=0;i<sizes[0];i++)T(((unsigned char*)ram[0])[i]==0x6a);}
 submit_error=EIO;BAD(api_nvnQueueSubmitCommands(&queue,1,&top),EIO);submit_error=0;T(q->call_queue_poisoned&&dev->retired);
 OK(api_nvnDeviceFinalizeCommandHandle(&device,top));
 /* CPU texture metadata is no longer needed after encoding. The imported
  * storage owner remains busy on the actual submitted BO until Finish. */
 for(unsigned i=0;i<4;i++){OK(api_nvnTextureFinalize(textures+i));}
 T(find(pools,POOL)->refs==0&&nn_bo_idle(find(pools,POOL)->bo)==EBUSY);
 BAD(api_nvnMemoryPoolFinalize(pools),EBUSY);T(find(pools,POOL)&&dev->retired);
 OK(api_nvnQueueFinish(&queue));T(!dev->retired);
 OK(api_nvnQueueFinalize(&queue));for(unsigned i=0;i<2;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(ram[i]);}free(before);T(!dev->gpu->bos);OK(api_nvnDeviceFinalize(&device));while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
}
int main(void){cp_cases();printf("{\"passed\":true,\"checks\":%u,\"copy_operations\":9,\"fresh_CP_jobs\":8,\"inherited_bound_clears\":2,\"GPU_execution\":false,\"CPU_shader_execution\":false}\n",checks);return 0;}
