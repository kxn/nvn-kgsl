/* HOST real two-BridgeClear Call/materialization/order/replay. No GPU execution. */
#define main prior_compute_fixture_main
#include "command_compute.c"
#undef main
#include NN_CALL_COPY_BRIDGE_BUNDLE
static void source_clear_texture(NVNtexture *key,NVNtextureBuilder *builder,NVNdevice *device,NVNmemoryPool *pool,size_t offset)
{
 OK(api_nvnTextureBuilderSetDefaults(builder));OK(api_nvnTextureBuilderSetDevice(builder,device));
 OK(api_nvnTextureBuilderSetSize2D(builder,64,64));OK(api_nvnTextureBuilderSetFormat(builder,NVN_FORMAT_RGBA8));
 OK(api_nvnTextureBuilderSetStorage(builder,pool,(ptrdiff_t)offset));T(api_nvnTextureInitialize(key,builder));T(!nn_nvn_error());T(find(key,TEXTURE)->source_layout);
}
int main(void)
{
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));struct object *dev=find(&device,DEVICE);
 T(!nn_nvn_register_texture_bridge(&device,&NN_TEXTURE_BRIDGE_BUNDLE));
 NVNmemoryPoolBuilder pb[2]={0};NVNmemoryPool pools[2]={0};void *ram[2]={0};
 for(unsigned i=0;i<2;i++){
  T(!posix_memalign(ram+i,4096,65536));memset(ram[i],0x6d,65536);
  OK(api_nvnMemoryPoolBuilderSetDefaults(pb+i));OK(api_nvnMemoryPoolBuilderSetDevice(pb+i,&device));OK(api_nvnMemoryPoolBuilderSetFlags(pb+i,0x24));OK(api_nvnMemoryPoolBuilderSetStorage(pb+i,ram[i],65536));T(api_nvnMemoryPoolInitialize(pools+i,pb+i));
 }
 NVNtexture textures[2]={0};NVNtextureBuilder tb[2]={0};source_clear_texture(textures,tb,&device,pools,512);source_clear_texture(textures+1,tb+1,&device,pools,20480);
 NVNcommandBuffer child={0},parent={0};_Alignas(16)unsigned char controls[2][4096];T(api_nvnCommandBufferInitialize(&child,&device));T(api_nvnCommandBufferInitialize(&parent,&device));
 OK(api_nvnCommandBufferAddCommandMemory(&child,pools+1,0,32768));OK(api_nvnCommandBufferAddControlMemory(&child,controls[0],4096));OK(api_nvnCommandBufferBeginRecording(&child));
 const float red[4]={1,0,0,1},green[4]={0,1,0,1};NVNcopyRegion regions[2]={{4,5,0,8,9,1},{12,10,0,11,17,1}};
 OK(api_nvnCommandBufferClearTexture(&child,textures,NULL,regions,red,15));OK(api_nvnCommandBufferClearTexture(&child,textures+1,NULL,regions+1,green,15));
 NVNcommandHandle source=api_nvnCommandBufferEndRecording(&child);T(source&&!nn_nvn_error());
 OK(api_nvnCommandBufferAddCommandMemory(&parent,pools+1,32768,32768));OK(api_nvnCommandBufferAddControlMemory(&parent,controls[1],4096));OK(api_nvnCommandBufferBeginRecording(&parent));OK(api_nvnCommandBufferCallCommands(&parent,1,&source));
 NVNcommandHandle top=api_nvnCommandBufferEndRecording(&parent);T(top&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&device,source));OK(api_nvnCommandBufferFinalize(&child));OK(api_nvnCommandBufferFinalize(&parent));
 memset(regions,0xee,sizeof(regions));unsigned char before[65536];memcpy(before,ram[1],65536);
 NVNqueueBuilder qb={0};NVNqueue queue={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&device));T(api_nvnQueueInitialize(&queue,&qb));struct object *q=find(&queue,QUEUE);
 uint64_t previous[2]={0};unsigned fresh=0;
 for(unsigned repeat=0;repeat<2;repeat++){
  struct nn_command_state initial={0},final={0};struct handle *projected=NULL;T(!command_project_sequence(q,lookup(top),&initial,&final,&projected));T(projected&&!memcmp(&initial,&final,sizeof(initial)));
  const struct late_draw *second=projected->draws,*first=second?second->next:NULL;
  T(first&&second&&!first->next&&first->bridge_clear&&second->bridge_clear);T(first->command_offset==0&&second->command_offset==16);
  T(first->target==find(textures,TEXTURE)&&second->target==find(textures+1,TEXTURE));T(first->clear_word==0xff0000ff&&second->clear_word==0xff00ff00);
  T(first->clear_region.xoffset==4&&first->clear_region.height==9&&second->clear_region.xoffset==12&&second->clear_region.height==17);
  for(const struct late_draw *draw=first;draw;draw=draw==first?second:NULL){
   nn_bo *entry=NULL,*sampled=NULL;size_t bytes=0;struct job *owners=NULL;T(!materialize_draw_bridge(dev,draw,&entry,&bytes,&sampled,&owners));T(entry&&bytes&&owners&&!sampled);
   unsigned count=0;for(struct job *j=owners;j;j=j->next)count++;T(count==6);fresh++;
   T(!nn_bo_destroy(entry));retire_jobs(dev,owners);
  }
  /* A distinct unmodeled legacy Draw is still rejected atomically. */
  struct late_draw unknown={.target=find(textures,TEXTURE),.next=projected->draws};projected->draws=&unknown;
  struct pipeline_batch rejected={0};struct handle *selected[]={projected};unsigned prior_submits=submits;
  T(pipeline_batch_prepare_owned(q,1,selected,&rejected)==ENOTSUP);pipeline_batch_discard(&rejected);T(submits==prior_submits);projected->draws=unknown.next;
  command_body_destroy(projected);
  OK(api_nvnQueueSubmitCommands(&queue,1,&top));T(submitted_bytes==48&&submitted[0]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3)&&submitted[4]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
  uint64_t address[2]={pair64(submitted+1),pair64(submitted+5)};T(address[0]!=address[1]);
  for(unsigned i=0;i<2;i++){
   T(address[i]&&address[i]!=previous[i]);previous[i]=address[i];unsigned found=0;
   for(size_t k=0;k<q->queue->count;k++)if(nn_bo_address(q->queue->pending[k])==address[i])found++;
   T(found==1);
  }
  T(nn_bo_idle(find(pools,POOL)->bo)==EBUSY);BAD(api_nvnTextureFinalize(textures),EBUSY);BAD(api_nvnTextureFinalize(textures+1),EBUSY);
  OK(api_nvnQueueFinish(&queue));T(!dev->retired);T(!memcmp(before,ram[1],65536));for(unsigned i=0;i<65536;i++)T(((unsigned char*)ram[0])[i]==0x6d);
 }
 T(submits==2&&fresh==4);OK(api_nvnDeviceFinalizeCommandHandle(&device,top));OK(api_nvnQueueFinalize(&queue));
 for(unsigned i=0;i<2;i++){OK(api_nvnTextureFinalize(textures+i));}
 for(unsigned i=0;i<2;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(ram[i]);}
 T(!dev->gpu->bos);OK(api_nvnDeviceFinalize(&device));while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"BridgeClear_Call_operations\":2,\"replays\":2,\"separately_materialized_fresh_owner_groups\":4,\"GPU_execution\":false,\"CPU_shader_execution\":false}\n",checks);return 0;
}
