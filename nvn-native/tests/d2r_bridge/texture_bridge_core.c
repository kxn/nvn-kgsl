/* HOST actual NVN recorder/materializer/ref paths; no shader execution. */
#define main original_modes_fixture_main
#include NN_BRIDGE_FRONTEND
#undef main
#include NN_BRIDGE_BUNDLE
void nn_test_ambiguous_next(void);void nn_test_resolve_unknown(nn_queue *);
unsigned nn_test_accepted(void);int nn_test_last_has(nn_bo *);
static struct object *source_texture(struct object *device,const void *key,struct object *storage,unsigned dim,size_t offset)
{
 NVNtextureBuilder builder;api_nvnTextureBuilderSetDefaults(&builder);api_nvnTextureBuilderSetDevice(&builder,(NVNdevice*)device->key);
 api_nvnTextureBuilderSetFlags(&builder,0);api_nvnTextureBuilderSetFormat(&builder,NVN_FORMAT_RGBA8);api_nvnTextureBuilderSetSize2D(&builder,(int)dim,(int)dim);
 api_nvnTextureBuilderSetStorage(&builder,(NVNmemoryPool*)storage->key,(ptrdiff_t)offset);
 T(api_nvnTextureInitialize((NVNtexture*)key,&builder));T(!nn_nvn_error());
 struct object *t=find(key,TEXTURE);T(t&&t->source_layout);struct object *weak=find(&builder,TB);remove_object(weak);return t;
}
int main(void)
{
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));
 struct object *dev=find(&dk,DEVICE);const struct nn_tex_bridge_bundle *bundle=&NN_TEXTURE_BRIDGE_BUNDLE;
 T(!nn_nvn_register_texture_bridge(&dk,bundle));T(!nn_nvn_register_texture_bridge(&dk,bundle));
 struct nn_tex_bridge_bundle other=*bundle;T(nn_nvn_register_texture_bridge(&dk,&other)==EBUSY);
 unsigned char keys[8];struct object *p[8];for(unsigned i=0;i<8;i++)p[i]=pool(dev,&keys[i],i==3?32768:i==5?8192:i==6||i==7?16384:4096);
 NVNtexture rtkey={0},samplekey={0};struct object *rt=source_texture(dev,&rtkey,p[3],64,512);
 T(rt->bo==p[3]->bo&&rt->offset==512);
 struct object *tx=source_texture(dev,&samplekey,p[5],2,512);
 uint64_t target_address=api_nvnTextureGetTextureAddress(&rtkey),sample_address=api_nvnTextureGetTextureAddress(&samplekey);
 memset(nn_bo_map(p[3]->bo),0x79,p[3]->size);memset(nn_bo_map(p[5]->bo),0x2d,p[5]->size);
 unsigned char before[32768];memcpy(before,nn_bo_map(p[3]->bo),sizeof(before));
 NVNprogram pk={0};T(api_nvnProgramInitialize(&pk,&dk));struct object *pr=find(&pk,PROGRAM);
 const struct nn_gfx_image *image=&NVN_GRAPHICS_IMAGE;T(image->profile==NN_GFX_TEXTURE_2D_NORMALIZED);
 struct nn_nvn_sampled map={.stage=NVN_SHADER_STAGE_FRAGMENT,.texture_slot=10,.sampler_slot=0};
 struct nn_nvn_compiled_graphics compiled={.image=image,.sampled=&map,.sampled_count=1};pr->graphics=&compiled;
 T(!nn_bo_alloc(dev->gpu,image->vs_code_bytes,&pr->bo));T(!nn_bo_alloc(dev->gpu,image->fs_code_bytes,&pr->fs_bo));
 memcpy(nn_bo_map(pr->bo),image->vs_code,image->vs_code_bytes);memcpy(nn_bo_map(pr->fs_bo),image->fs_code,image->fs_code_bytes);
 NVNsamplerBuilder sb={0};NVNsampler sm={0};OK(api_nvnSamplerBuilderSetDefaults(&sb));OK(api_nvnSamplerBuilderSetDevice(&sb,&dk));T(api_nvnSamplerInitialize(&sm,&sb));
 NVNtexturePool tp={0};NVNsamplerPool sp={0};T(api_nvnTexturePoolInitialize(&tp,p[6]->key,0,272));T(api_nvnSamplerPoolInitialize(&sp,p[7]->key,0,272));
 OK(api_nvnTexturePoolRegisterTexture(&tp,257,&samplekey,NULL));OK(api_nvnSamplerPoolRegisterSampler(&sp,259,&sm));
 struct object *table=find(&tp,TPOOL);T(table->slots[257].source_layout&&table->slots[257].source_layout!=tx->source_layout);
 T(table->slots[257].size==512&&table->slots[257].offset==512);T(table->slots[257].pool==p[5]);
 T(((struct texture_slot*)((char*)nn_bo_map(p[6]->bo)+257*32))->address==sample_address);
 OK(api_nvnTextureFinalize(&samplekey));T(table->slots[257].source_layout->level[0].width==2);
 NVNcommandBuffer ck={0};_Alignas(16) unsigned char control[2048];T(api_nvnCommandBufferInitialize(&ck,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&ck,p[0]->key,0,4096));OK(api_nvnCommandBufferAddControlMemory(&ck,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&ck));
 const NVNtexture *targets[]={&rtkey};OK(api_nvnCommandBufferSetRenderTargets(&ck,1,targets,NULL,NULL,NULL));
 struct recording *r=find(&ck,COMMAND)->record;r->graphics_program=pr;pr->refs++;
 OK(api_nvnCommandBufferBindVertexBuffer(&ck,0,nn_bo_address(p[1]->bo),96));
 r->graphics_ubos[0][0]=(struct bound){p[2],0,64,nn_bo_address(p[2]->bo)};p[2]->refs++;
 r->graphics_ubos[1][0]=(struct bound){p[4],0,64,nn_bo_address(p[4]->bo)};p[4]->refs++;
 NVNvertexAttribState attributes[3];NVNvertexStreamState stream;
 for(unsigned i=0;i<3;i++)OK(api_nvnVertexAttribStateSetDefaults(attributes+i));
 OK(api_nvnVertexAttribStateSetFormat(attributes,NVN_FORMAT_RG32F,0));
 OK(api_nvnVertexAttribStateSetFormat(attributes+1,NVN_FORMAT_RG32F,8));
 OK(api_nvnVertexAttribStateSetFormat(attributes+2,NVN_FORMAT_RGBA32F,16));
 OK(api_nvnVertexStreamStateSetDefaults(&stream));OK(api_nvnVertexStreamStateSetStride(&stream,32));
 OK(api_nvnCommandBufferBindVertexAttribState(&ck,3,attributes));OK(api_nvnCommandBufferBindVertexStreamState(&ck,1,&stream));
 OK(api_nvnCommandBufferSetViewport(&ck,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&ck,0,0,64,64));
 OK(api_nvnCommandBufferSetDepthRange(&ck,0,1));
 OK(api_nvnCommandBufferSetTexturePool(&ck,&tp));OK(api_nvnCommandBufferSetSamplerPool(&ck,&sp));
 OK(api_nvnCommandBufferBindSeparateTexture(&ck,NVN_SHADER_STAGE_FRAGMENT,10,(NVNseparateTextureHandle){257}));
 OK(api_nvnCommandBufferBindSeparateSampler(&ck,NVN_SHADER_STAGE_FRAGMENT,0,(NVNseparateSamplerHandle){259ull<<20}));
 float red[4]={1,0,0,1};OK(api_nvnCommandBufferClearColor(&ck,0,red,15));
 NVNcopyRegion region={.xoffset=4,.yoffset=5,.width=11,.height=17,.depth=1};OK(api_nvnCommandBufferClearTexture(&ck,&rtkey,NULL,&region,red,15));
 size_t n=r->used;ERR(api_nvnCommandBufferClearTexture(&ck,&rtkey,NULL,&region,red,7),ENOTSUP);T(r->used==n);
 OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));T(r->current->draws&&!r->current->draws->bridge_clear);
 /* KGSL strips TBI tags before pinning/importing. Different caller tags must
  * never turn the same physical caller storage into independent RT/arena. */
 uint64_t normalized=0;struct object tagged=*p[3];void *saved_storage=p[3]->storage;
 p[3]->storage=(void*)((uintptr_t)saved_storage|UINT64_C(0xa100000000000000));
 tagged.storage=(void*)((uintptr_t)saved_storage|UINT64_C(0xb200000000000000));tagged.bo=p[5]->bo;
 T(!bridge_cpu_range(&tagged,512,512,&normalized));T(normalized==(uintptr_t)saved_storage+512);
 struct descriptor_entry duplicate={.pool=&tagged,.offset=512,.size=512};T(bridge_feedback(rt,&duplicate)==ENOTSUP);
 duplicate.offset=rt->offset+rt->size;T(!bridge_feedback(rt,&duplicate));duplicate.offset=SIZE_MAX;T(bridge_feedback(rt,&duplicate)==EOVERFLOW);
 T(bridge_cpu_range(&tagged,SIZE_MAX,512,&normalized)==EOVERFLOW);
 p[3]->storage=(void*)((uintptr_t)p[0]->storage|UINT64_C(0xc300000000000000));T(bridge_target_arena_alias(rt)==ENOTSUP);p[3]->storage=saved_storage;
 NVNcommandHandle id=api_nvnCommandBufferEndRecording(&ck);T(id);struct handle *h=handles;T(h->id==id&&h->bytes==56);
 T(h->draws->next->bridge_clear&&h->draws->next->clear_region.xoffset==4);T(h->draws->next->next->bridge_clear);
 for(struct late_draw *draw=h->draws;draw;draw=draw->next){
  nn_bo *entry[2]={0},*sampled=NULL;size_t bytes=0;struct job *owned[2]={0};
  for(unsigned replay=0;replay<2;replay++){
   T(!materialize_draw_bridge(dev,draw,&entry[replay],&bytes,&sampled,&owned[replay]));T(entry[replay]&&bytes&&owned[replay]);
   unsigned count=0;for(struct job *j=owned[replay];j;j=j->next)count++;
   T(count==(draw->bridge_clear?6u:11u));
   if(!draw->bridge_clear)T(sampled==p[5]->bo);
  }
  T(nn_bo_address(entry[0])!=nn_bo_address(entry[1]));
  T(owned[0]->bo!=owned[1]->bo);T(!nn_bo_destroy(entry[0]));T(!nn_bo_destroy(entry[1]));retire_jobs(dev,owned[0]);retire_jobs(dev,owned[1]);
 }
 T(!memcmp(before,nn_bo_map(p[3]->bo),sizeof(before)));T(api_nvnTextureGetTextureAddress(&rtkey)==target_address);
 for(unsigned off=0;off<p[5]->size;off++)T(((unsigned char*)nn_bo_map(p[5]->bo))[off]==0x2d);
 /* Clear preserves inline snapshot even after the caller mutates its region. */
 region.xoffset=99;T(h->draws->next->clear_region.xoffset==4);
 /* A known feedback/old descriptor alias rejects before private allocations. */
 struct descriptor_entry saved=table->slots[257];struct texture_slot saved_slot;memcpy(&saved_slot,(char*)nn_bo_map(p[6]->bo)+257*32,32);
 table->slots[257].pool=p[3];table->slots[257].offset=512;table->slots[257].size=512;
 struct texture_slot alias_slot=saved_slot;alias_slot.address=nn_bo_address(p[3]->bo)+512;memcpy((char*)nn_bo_map(p[6]->bo)+257*32,&alias_slot,32);
 nn_bo *bad=NULL,*sampled=NULL;size_t bytes=0;struct job *aux=NULL;
 T(materialize_draw_bridge(dev,h->draws,&bad,&bytes,&sampled,&aux)==ENOTSUP);T(!bad&&!aux);
 table->slots[257]=saved;memcpy((char*)nn_bo_map(p[6]->bo)+257*32,&saved_slot,32);
 NVNqueueBuilder qb={0};NVNqueue qk={0};api_nvnQueueBuilderSetDefaults(&qb);api_nvnQueueBuilderSetDevice(&qb,&dk);T(api_nvnQueueInitialize(&qk,&qb));struct object *q=find(&qk,QUEUE);
 /* Min legal 4KiB queue control: 120 legacy sampled draws must keep their
  * original two-ref upper bound; the same bridge workload genuinely needs
  * more private refs and rejects before materialization/submission. */
 struct handle stress=*h;stress.id=next_handle++;stress.bytes=120*16+8;stress.draws=NULL;stress.chunks=NULL;stress.jobs=NULL;stress.borrowed_jobs=NULL;
 for(unsigned i=0;i<120;i++){struct late_draw *d=malloc(sizeof(*d));T(d);*d=*h->draws;d->command_offset=i*16;d->next=stress.draws;stress.draws=d;}
 unsigned char saved_arena[4096];memcpy(saved_arena,nn_bo_map(p[0]->bo),4096);memset(nn_bo_map(p[0]->bo),0,stress.bytes);
 uint32_t *tail=(uint32_t*)((char*)nn_bo_map(p[0]->bo)+120*16);tail[0]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);tail[1]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0);
 stress.next=handles;handles=&stress;size_t control_capacity=q->queue_config.control;q->queue_config.control=4096;
 T(draw_ref_capacity(stress.draws)==13);unsigned before_submit=nn_test_accepted();ERR(api_nvnQueueSubmitCommands(&qk,1,&stress.id),ENOSPC);T(nn_test_accepted()==before_submit);
 struct nn_tex_source_layout *saved_rt_layout=rt->source_layout;struct texture_desc saved_rt_desc=rt->texture;
 struct descriptor_entry original_entry=table->slots[257];struct texture_slot original_slot;memcpy(&original_slot,(char*)nn_bo_map(p[6]->bo)+257*32,32);
 rt->source_layout=NULL;rt->texture.flags=NVN_TEXTURE_FLAGS_LINEAR;rt->texture.stride=256;
 table->slots[257].source_layout=NULL;table->slots[257].size=4096;table->slots[257].offset=0;table->slots[257].snapshot.texture.flags=NVN_TEXTURE_FLAGS_LINEAR;table->slots[257].snapshot.texture.stride=256;
 struct texture_slot linear_slot=original_slot;linear_slot.stride=256;linear_slot.bytes=4096;linear_slot.address=nn_bo_address(p[5]->bo);memcpy((char*)nn_bo_map(p[6]->bo)+257*32,&linear_slot,32);
 T(draw_ref_capacity(stress.draws)==2);OK(api_nvnQueueSubmitCommands(&qk,1,&stress.id));T(nn_test_accepted()==before_submit+1);OK(api_nvnQueueFinish(&qk));T(!dev->retired);
 rt->source_layout=saved_rt_layout;rt->texture=saved_rt_desc;table->slots[257]=original_entry;memcpy((char*)nn_bo_map(p[6]->bo)+257*32,&original_slot,32);
 q->queue_config.control=control_capacity;handles=stress.next;free_draws(stress.draws);memcpy(nn_bo_map(p[0]->bo),saved_arena,4096);
 unsigned accepted=nn_test_accepted();OK(api_nvnQueueSubmitCommands(&qk,1,&id));T(nn_test_accepted()==accepted+1);T(nn_test_last_has(p[3]->bo)&&nn_test_last_has(p[5]->bo));
 T(dev->retired);T(nn_bo_idle(p[3]->bo)==EBUSY);ERR(api_nvnTextureFinalize(&rtkey),EBUSY);OK(api_nvnQueueFinish(&qk));T(!dev->retired);T(!nn_bo_idle(p[3]->bo));
 /* Real submission is unknown in this HOST seam, not silently rolled back. */
 nn_test_ambiguous_next();ERR(api_nvnQueueSubmitCommands(&qk,1,&id),EIO);T(dev->retired&&nn_bo_idle(p[3]->bo)==EBUSY);ERR(api_nvnQueueFinish(&qk),EBUSY);T(dev->retired);
 nn_test_resolve_unknown(q->queue);/* test-only release; real driver has no escape */
 OK(api_nvnQueueFinish(&qk));T(!dev->retired);
 OK(api_nvnCommandBufferFinalize(&ck));OK(api_nvnDeviceFinalizeCommandHandle(&dk,id));OK(api_nvnQueueFinalize(&qk));
 OK(api_nvnTexturePoolFinalize(&tp));OK(api_nvnSamplerPoolFinalize(&sp));OK(api_nvnSamplerFinalize(&sm));OK(api_nvnTextureFinalize(&rtkey));OK(api_nvnProgramFinalize(&pk));
 for(unsigned i=0;i<8;i++)OK(api_nvnMemoryPoolFinalize((NVNmemoryPool*)p[i]->key));
 OK(api_nvnDeviceFinalize(&dk));
 while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"gpu_execution\":false,\"scope\":\"HOST real NVN canonical registration/Clear/Draw/enqueue/replay/unknown ownership; no pixel execution\"}\n",checks);return 0;
}
