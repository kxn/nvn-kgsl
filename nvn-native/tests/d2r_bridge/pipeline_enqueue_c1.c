/* HOST ONLY actual typed NVN record/enqueue/retire. No GPU execution/pixels. */
#define main source_identity_fixture
#include NN_SOURCE_FIXTURE
#undef main
#include NN_PIPELINE_NATIVE_VIEWS
#include NN_PIPELINE_TEXTURE_BRIDGE
static void pool_create(NVNdevice *device,NVNmemoryPoolBuilder *builder_key,NVNmemoryPool *pool,void **storage,size_t bytes,int flags)
{
 T(!posix_memalign(storage,4096,bytes));memset(*storage,0,bytes);
 OK(api_nvnMemoryPoolBuilderSetDefaults(builder_key));OK(api_nvnMemoryPoolBuilderSetDevice(builder_key,device));
 OK(api_nvnMemoryPoolBuilderSetStorage(builder_key,*storage,bytes));
 OK(api_nvnMemoryPoolBuilderSetFlags(builder_key,flags));
 T(api_nvnMemoryPoolInitialize(pool,builder_key));
}

#include "pipeline_enqueue_fault_checks.h"
static int one(unsigned source)
{
 mock_submits=0;mock_submit_error=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *device=find(&dk,DEVICE);
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_pipeline_archive(&dk,archive,reader,ranges));
 T(!nn_nvn_register_texture_bridge(&dk,&NN_TEXTURE_BRIDGE_BUNDLE));
 T(nn_nvn_register_pipeline_views(&dk,NULL,1)==EINVAL);T(!nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT));
 T(nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT)==EEXIST);
 /* Fixture chooses full original source groups only; runtime specialization
  * is selected exclusively from the real enqueue entries below. */
 const struct nn_offline_graphics_stage *stages[2]={0};
 for(size_t i=0;i<archive->stages->stage_count;i++){
  const struct nn_offline_graphics_stage *v=archive->stages->stages+i;T(v->stage<2);
  if(!stages[v->stage])stages[v->stage]=v;
  else{const struct nn_offline_graphics_stage *old=stages[v->stage];
   T(v->source_bytes==old->source_bytes&&v->control_bytes==old->control_bytes&&
    !memcmp(v->source,old->source,v->source_bytes)&&!memcmp(v->control,old->control,v->control_bytes));}
 }
 T(stages[0]&&stages[1]);
 enum{VS,FS,ARENA,DATA,RT,UNUSED,META,SAMPLE0,SAMPLE1,SAMPLE_ALT,POOLS};NVNmemoryPoolBuilder pb[POOLS];NVNmemoryPool pools[POOLS];void *storage[POOLS];memset(pb,0,sizeof(pb));memset(pools,0,sizeof(pools));
 for(unsigned i=0;i<POOLS;i++)pool_create(&dk,pb+i,pools+i,storage+i,(i==RT||i==SAMPLE0||i==SAMPLE1||i==SAMPLE_ALT)?32768:16384,(i==UNUSED||i==META)?0x14:0x24);
 NVNshaderData shaders[2];unsigned char controls[2][0x898];range_count=0;allow(shaders,sizeof(shaders));
 for(unsigned i=0;i<2;i++){memcpy(storage[i],stages[i]->source,stages[i]->source_bytes);memcpy(controls[i],stages[i]->control,stages[i]->control_bytes);allow(storage[i],16384);allow(controls[i],sizeof(controls[i]));shaders[i]=(NVNshaderData){api_nvnMemoryPoolGetBufferAddress(pools+i),controls[i]};}
 NVNprogram pk={0};PFNNVNPROGRAMINITIALIZEPROC program_init=(PFNNVNPROGRAMINITIALIZEPROC)nvnBootstrapLoader("nvnProgramInitialize");
 PFNNVNPROGRAMSETSHADERSPROC program_set=(PFNNVNPROGRAMSETSHADERSPROC)nvnBootstrapLoader("nvnProgramSetShaders");T(program_init(&pk,&dk));T(program_set(&pk,2,shaders));
 for(unsigned i=0;i<2;i++){memset(storage[i],0xcc,16384);OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);storage[i]=NULL;}
 NVNtextureBuilder tb={0};NVNtexture tk={0};OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnTextureBuilderSetDevice(&tb,&dk));
 OK(api_nvnTextureBuilderSetFlags(&tb,source?0:NVN_TEXTURE_FLAGS_LINEAR));OK(api_nvnTextureBuilderSetSize2D(&tb,64,64));OK(api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetStride(&tb,source?0:256));OK(api_nvnTextureBuilderSetStorage(&tb,pools+RT,4096));T(api_nvnTextureInitialize(&tk,&tb));
 NVNcommandBuffer ck={0};_Alignas(16)unsigned char control[1024];T(api_nvnCommandBufferInitialize(&ck,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&ck,pools+ARENA,0,16384));OK(api_nvnCommandBufferAddControlMemory(&ck,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&ck));
 OK(api_nvnCommandBufferBindProgram(&ck,&pk,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
 PFNNVNVERTEXATTRIBSTATESETFORMATPROC af=(PFNNVNVERTEXATTRIBSTATESETFORMATPROC)nvnBootstrapLoader("nvnVertexAttribStateSetFormat");
 PFNNVNVERTEXATTRIBSTATESETSTREAMINDEXPROC ai=(PFNNVNVERTEXATTRIBSTATESETSTREAMINDEXPROC)nvnBootstrapLoader("nvnVertexAttribStateSetStreamIndex");
 NVNvertexAttribState attr[3];for(unsigned i=0;i<3;i++){
  OK(api_nvnVertexAttribStateSetDefaults(attr+i));OK(af(attr+i,i==2?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,i==2?16:i?8:0));OK(ai(attr+i,7));}
 OK(api_nvnCommandBufferBindVertexAttribState(&ck,3,attr));memset(attr,0xcc,sizeof(attr));
 NVNvertexStreamState streams[8];for(unsigned i=0;i<8;i++)OK(api_nvnVertexStreamStateSetDefaults(streams+i));
 OK(api_nvnVertexStreamStateSetStride(streams+7,32));OK(api_nvnCommandBufferBindVertexStreamState(&ck,8,streams));memset(streams,0xcc,sizeof(streams));
 uint64_t data=api_nvnMemoryPoolGetBufferAddress(pools+DATA);OK(api_nvnCommandBufferBindVertexBuffer(&ck,7,data+4096,4096));
 OK(api_nvnCommandBufferBindUniformBuffer(&ck,NVN_SHADER_STAGE_VERTEX,0,data+8192,64));
 OK(api_nvnCommandBufferBindUniformBuffer(&ck,NVN_SHADER_STAGE_FRAGMENT,0,data+8448,64));
 uint64_t unused_address=api_nvnMemoryPoolGetBufferAddress(pools+UNUSED);
 OK(api_nvnCommandBufferBindVertexBuffer(&ck,15,unused_address,4096));
 OK(api_nvnCommandBufferBindUniformBuffer(&ck,NVN_SHADER_STAGE_FRAGMENT,13,unused_address,64));
 const NVNtexture *targets[]={&tk};OK(api_nvnCommandBufferSetRenderTargets(&ck,1,targets,NULL,NULL,NULL));
 OK(api_nvnCommandBufferSetViewport(&ck,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&ck,0,0,64,64));OK(api_nvnCommandBufferSetDepthRange(&ck,0,1));

 NVNtexturePool texture_pool={0};T(api_nvnTexturePoolInitialize(&texture_pool,pools+META,0,320));
 OK(api_nvnCommandBufferSetTexturePool(&ck,&texture_pool));
 NVNseparateTextureHandle handles2[2]={api_nvnDeviceGetSeparateTextureHandle(&dk,257),api_nvnDeviceGetSeparateTextureHandle(&dk,258)};T(!nn_nvn_error());
 OK(api_nvnCommandBufferBindSeparateTextures(&ck,NVN_SHADER_STAGE_FRAGMENT,10,2,handles2));
 memset(handles2,0xcc,sizeof(handles2));
 struct recording *r=find(&ck,COMMAND)->record;
 NVNqueueBuilder qb={0};NVNqueue qk={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&qk,&qb));
 size_t before_alloc=mock_allocations,before_cache=mock_caches;
 OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
 struct late_draw *recorded=r->current->draws;T(recorded&&recorded->generic_pipeline&&!recorded->pipeline.recipe);
 T(recorded->pipeline.owner&&recorded->pipeline.owner->references==1);
 T(recorded->pipeline.uniforms[0][0].offset==8192);
 T(mock_allocations==before_alloc&&mock_caches==before_cache);
 NVNcommandHandle handle=api_nvnCommandBufferEndRecording(&ck);T(handle&&!nn_nvn_error());
 /* Both slots are registered AFTER recording, then public texture objects
  * are finalized. The original pool backings and entry snapshots remain. */
 NVNtextureBuilder sampled_builder[2]={0};NVNtexture sampled[2]={0};
 for(unsigned i=0;i<2;i++){
  OK(api_nvnTextureBuilderSetDefaults(sampled_builder+i));OK(api_nvnTextureBuilderSetDevice(sampled_builder+i,&dk));
  OK(api_nvnTextureBuilderSetFlags(sampled_builder+i,source?0:NVN_TEXTURE_FLAGS_LINEAR));
  OK(api_nvnTextureBuilderSetSize2D(sampled_builder+i,64,64));OK(api_nvnTextureBuilderSetFormat(sampled_builder+i,NVN_FORMAT_RGBA8));
  OK(api_nvnTextureBuilderSetStride(sampled_builder+i,source?0:256));OK(api_nvnTextureBuilderSetStorage(sampled_builder+i,pools+SAMPLE0+i,4096));
  T(api_nvnTextureInitialize(sampled+i,sampled_builder+i));OK(api_nvnTexturePoolRegisterTexture(&texture_pool,257+i,sampled+i,NULL));
  OK(api_nvnTextureFinalize(sampled+i));
 }
 before_alloc=mock_allocations;before_cache=mock_caches;
 struct pipeline_batch batch={0};T(!pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&batch));
 const struct pipeline_prepared_draw *prepared=pipeline_batch_find(&batch,recorded,0);T(prepared&&prepared->draw.pipeline.recipe);
 T(prepared->draw.pipeline.native.ubo[0][stages[0]->cbufs[0].binding].offset==8192);
 T(prepared->draw.pipeline.recipe->texture_mask==6&&!prepared->draw.pipeline.recipe->sampler_mask);
 for(unsigned i=1;i<=2;i++){
  T(prepared->native_entries[i]&&prepared->native_entries[i]->pool==find(pools+SAMPLE0+i-1,POOL));
  T(prepared->native_entries[i]->offset==4096&&prepared->native_entries[i]->snapshot.texture.target==NVN_TEXTURE_TARGET_2D);
 }
 T(mock_allocations==before_alloc&&mock_caches==before_cache);
 if(!source){
  /* Native API compatibility: an explicit view of the exact immutable C1
   * owner remains valid, while an unrelated BO must fail before job alloc. */
  const struct pipeline_draw_snapshot *p=&prepared->draw.pipeline;
  nn_pipeline_program *native_program=NULL,*failed_program=NULL;
  T(!nn_pipeline_program_create(device->gpu,archive->stages,p->stages[0],p->stages[1],
   p->stages[0]->specialization,p->stages[1]->specialization,&native_program,&failed_program));T(!failed_program);
  struct nn_pipeline_draw_state native_state=p->native;unsigned c1_stage=2,c1_binding=0;
  for(unsigned stage=0;stage<2;stage++){
   struct nn_pipeline_stage_view v;T(!nn_pipeline_program_stage(native_program,stage,&v));
   if(v.c1&&v.c1_native_binding<16){c1_stage=stage;c1_binding=v.c1_native_binding;
    native_state.ubo[stage][c1_binding]=(struct nn_pipeline_buffer){v.c1,0,v.c1_bytes};}
  }
  T(c1_stage<2);struct nn_pipeline_submission submission;nn_bo *failed_job=NULL;
  T(!nn_pipeline_encode(device->gpu,native_program,p->recipe,archive->platform,&native_state,&submission,&failed_job));
  T(submission.job&&!failed_job);T(!nn_bo_destroy(submission.job));
  native_state.ubo[c1_stage][c1_binding]=(struct nn_pipeline_buffer){find(pools+DATA,POOL)->bo,0,256};
  unsigned na=mock_allocations,nc=mock_caches;
  T(nn_pipeline_encode(device->gpu,native_program,p->recipe,archive->platform,&native_state,&submission,&failed_job)==EINVAL);
  T(!submission.job&&!failed_job&&mock_allocations==na&&mock_caches==nc);T(!nn_pipeline_program_destroy(native_program));
 }
 /* Bad published bytes and registered-but-unconsumable resource types must
  * reject the complete two-handle batch before any native resource work. */
 unsigned char *raw_slot=(unsigned char *)storage[META]+258*DESCRIPTOR_BYTES;
 unsigned char saved_slot[DESCRIPTOR_BYTES];memcpy(saved_slot,raw_slot,sizeof(saved_slot));raw_slot[0]^=1;
 unsigned reject_alloc=mock_allocations,reject_cache=mock_caches,reject_submit=mock_submits;
 NVNcommandHandle repeated[2]={handle,handle};
 ERR(api_nvnQueueSubmitCommands(&qk,2,repeated),EINVAL);memcpy(raw_slot,saved_slot,sizeof(saved_slot));
 T(mock_allocations==reject_alloc&&mock_caches==reject_cache&&mock_submits==reject_submit);
 for(unsigned buffer=0;buffer<2;buffer++){
  NVNtextureBuilder unsupported_builder={0};NVNtexture unsupported_texture={0};
  OK(api_nvnTextureBuilderSetDefaults(&unsupported_builder));OK(api_nvnTextureBuilderSetDevice(&unsupported_builder,&dk));
  OK(api_nvnTextureBuilderSetTarget(&unsupported_builder,buffer?NVN_TEXTURE_TARGET_BUFFER:NVN_TEXTURE_TARGET_2D));
  if(buffer){OK(api_nvnTextureBuilderSetSize1D(&unsupported_builder,64));}
  else{OK(api_nvnTextureBuilderSetSize2D(&unsupported_builder,64,64));}
  OK(api_nvnTextureBuilderSetFormat(&unsupported_builder,buffer?NVN_FORMAT_RGBA8:NVN_FORMAT_RGBA_DXT1));
  OK(api_nvnTextureBuilderSetStorage(&unsupported_builder,pools+SAMPLE_ALT,4096));
  T(api_nvnTextureInitialize(&unsupported_texture,&unsupported_builder));
  OK(api_nvnTexturePoolRegisterTexture(&texture_pool,258,&unsupported_texture,NULL));OK(api_nvnTextureFinalize(&unsupported_texture));
  reject_alloc=mock_allocations;reject_cache=mock_caches;reject_submit=mock_submits;
  ERR(api_nvnQueueSubmitCommands(&qk,2,repeated),ENOTSUP);
  T(mock_allocations==reject_alloc&&mock_caches==reject_cache&&mock_submits==reject_submit);
 }
 /* Re-registration frees the original slot layout. The existing prepared
  * entry must remain a deep value/pinned original backing; next prepare sees
  * the new version from the SAME recorded selector. */
 NVNtextureBuilder replacement_builder={0};NVNtexture replacement={0};
 OK(api_nvnTextureBuilderSetDefaults(&replacement_builder));OK(api_nvnTextureBuilderSetDevice(&replacement_builder,&dk));
 OK(api_nvnTextureBuilderSetFlags(&replacement_builder,source?0:NVN_TEXTURE_FLAGS_LINEAR));
 OK(api_nvnTextureBuilderSetSize2D(&replacement_builder,source?2:64,source?2:64));
 OK(api_nvnTextureBuilderSetFormat(&replacement_builder,NVN_FORMAT_RGBA8));
 OK(api_nvnTextureBuilderSetStride(&replacement_builder,source?0:256));
 OK(api_nvnTextureBuilderSetStorage(&replacement_builder,pools+SAMPLE_ALT,4096));T(api_nvnTextureInitialize(&replacement,&replacement_builder));
 OK(api_nvnTexturePoolRegisterTexture(&texture_pool,258,&replacement,NULL));OK(api_nvnTextureFinalize(&replacement));
 T(prepared->native_entries[2]->pool==find(pools+SAMPLE1,POOL)&&prepared->native_entries[2]->snapshot.texture.width==64);
 if(source)T(prepared->native_entries[2]->source_layout&&prepared->native_entries[2]->source_layout->level[0].width==64);
 ERR(api_nvnMemoryPoolFinalize(pools+SAMPLE1),EBUSY);
 struct pipeline_batch updated={0};T(!pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&updated));
 const struct pipeline_prepared_draw *newer=pipeline_batch_find(&updated,recorded,0);
 T(newer&&newer->native_entries[2]->pool==find(pools+SAMPLE_ALT,POOL)&&newer->native_entries[2]->snapshot.texture.width==(source?2:64));
 T(newer->draw.pipeline.native.textures[2].layout->width==(source?2u:64u));
 pipeline_batch_discard(&updated);pipeline_batch_discard(&batch);
 OK(api_nvnMemoryPoolFinalize(pools+SAMPLE1));free(storage[SAMPLE1]);storage[SAMPLE1]=NULL;
 metadata_preflight_oom(&qk,handle);
 /* A forbidden consumed resource in the SECOND handle rejects the ENTIRE
  * batch before code/C1/parent/shadow creation, cache, or first submit. */
 OK(api_nvnCommandBufferBeginRecording(&ck));
 OK(api_nvnCommandBufferBindUniformBuffer(&ck,NVN_SHADER_STAGE_VERTEX,0,unused_address,64));
 OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
 NVNcommandHandle bad=api_nvnCommandBufferEndRecording(&ck);T(bad&&!nn_nvn_error());
 NVNcommandHandle pair_ids[]={handle,bad};
 unsigned allocations_before=mock_allocations,caches_before=mock_caches;size_t refs_before=find(pools+DATA,POOL)->refs;
 ERR(api_nvnQueueSubmitCommands(&qk,2,pair_ids),ENOTSUP);
 T(!mock_submits&&mock_allocations==allocations_before&&mock_caches==caches_before&&find(pools+DATA,POOL)->refs==refs_before);
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,bad));
 OK(api_nvnCommandBufferBeginRecording(&ck));
 OK(api_nvnCommandBufferBindUniformBuffer(&ck,NVN_SHADER_STAGE_VERTEX,0,data+8192,64));
 struct pipeline_record_owner *owner=recorded->pipeline.owner;size_t copy_used=r->used,copy_refs=r->current->count;
 owner->references=SIZE_MAX;
 ERR(api_nvnCommandBufferCopyCommands(&ck,1,&handle),EOVERFLOW);
 T(owner->references==SIZE_MAX&&r->used==copy_used&&r->current->count==copy_refs&&!r->current->draws);
 owner->references=1;
 /* Each real CPU allocation point in Copy must roll back atomically. */
 unsigned copy_failures=0;
 for(size_t fail=1;fail<64;fail++){
  size_t data_refs=find(pools+DATA,POOL)->refs,target_refs=find(&tk,TEXTURE)->refs;
  unsigned na=mock_allocations,nc=mock_caches,ns=mock_submits;
  metadata_allocations=0;metadata_fail_at=fail;
  api_nvnCommandBufferCopyCommands(&ck,1,&handle);int failure=nn_nvn_error();metadata_fail_at=0;
  T(mock_allocations==na&&mock_caches==nc&&mock_submits==ns);
  if(!failure){T(owner->references==2);NVNcommandHandle trial=api_nvnCommandBufferEndRecording(&ck);T(trial);OK(api_nvnDeviceFinalizeCommandHandle(&dk,trial));T(owner->references==1);break;}
  T(failure==ENOMEM);nn_nvn_clear_error();copy_failures++;
  T(owner->references==1&&r->used==copy_used&&r->current->count==copy_refs&&!r->current->draws);
  T(find(pools+DATA,POOL)->refs==data_refs&&find(&tk,TEXTURE)->refs==target_refs);
 }
 T(copy_failures>=6);
 /* A separate public recording is finalized while holding the LAST copied
  * metadata owner; no second borrower masks its retirement path. */
 OK(api_nvnCommandBufferBeginRecording(&ck));
 OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
 struct pipeline_record_owner *abort_owner=r->current->draws->pipeline.owner;
 NVNcommandHandle abort_source=api_nvnCommandBufferEndRecording(&ck);T(abort_source);
 NVNcommandBuffer abort_key={0};_Alignas(16)unsigned char abort_control[128];
 T(api_nvnCommandBufferInitialize(&abort_key,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&abort_key,pools+ARENA,8192,4096));
 OK(api_nvnCommandBufferAddControlMemory(&abort_key,abort_control,sizeof(abort_control)));
 OK(api_nvnCommandBufferBeginRecording(&abort_key));
 OK(api_nvnCommandBufferCopyCommands(&abort_key,1,&abort_source));T(abort_owner->references==2);
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,abort_source));T(abort_owner->references==1);
 size_t abort_refs=find(pools+DATA,POOL)->refs;
 OK(api_nvnCommandBufferFinalize(&abort_key));T(find(pools+DATA,POOL)->refs+1==abort_refs);
 OK(api_nvnCommandBufferBeginRecording(&ck));
 OK(api_nvnCommandBufferCopyCommands(&ck,1,&handle));T(owner->references==2);
 NVNcommandHandle copied=api_nvnCommandBufferEndRecording(&ck);T(copied&&!nn_nvn_error());
 struct handle *original=handles;while(original&&original->id!=handle)original=original->next;T(original);
 size_t original_offset=original->offset,original_bytes=original->bytes;
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,handle));T(owner->references==1);
 memset((char *)storage[ARENA]+original_offset,0xcc,original_bytes);
 ERR(api_nvnMemoryPoolFinalize(pools+UNUSED),EBUSY); /* CPU metadata owner */
 OK(api_nvnQueueSubmitCommands(&qk,1,&copied));T(mock_submits==1);
 nn_bo *unused_bo=find(pools+UNUSED,POOL)->bo;for(size_t i=0;i<mock_ref_count;i++)T(mock_refs[i]!=unused_bo);
 handle=copied;
 /* An accepted but ambiguous mock ioctl keeps every actual BO and owner
  * until a real Finish result; handle destruction cannot declare completion. */
 mock_submit_error=EIO;ERR(api_nvnQueueSubmitCommands(&qk,1,&handle),EIO);mock_submit_error=0;
 T(mock_submits==2&&device->retired);
 /* Submitted actual owners remain pinned even if their public handle is gone. */
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,handle));T(device->retired);
 OK(api_nvnCommandBufferFinalize(&ck));
 OK(api_nvnMemoryPoolFinalize(pools+UNUSED));free(storage[UNUSED]);storage[UNUSED]=NULL;
 ERR(api_nvnProgramFinalize(&pk),EBUSY);ERR(api_nvnTextureFinalize(&tk),EBUSY);
 OK(api_nvnQueueFinish(&qk));T(!device->retired);
 OK(api_nvnTexturePoolFinalize(&texture_pool));
 OK(api_nvnProgramFinalize(&pk));OK(api_nvnTextureFinalize(&tk));
 OK(api_nvnQueueFinalize(&qk));
 for(unsigned i=ARENA;i<POOLS;i++)if(storage[i]){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}
 OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
 printf("HOST ONLY actual C1 samplerless2D enqueue owner source=%u checks=%u mock_submits=%u; GPU execution=false\n",source,checks,mock_submits);
 return 0;
}
int main(void){one(0);one(1);return 0;}
