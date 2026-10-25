/* HOST ONLY actual typed NVN record/enqueue/retire. No GPU execution/pixels. */
#define main source_identity_fixture
#include NN_SOURCE_FIXTURE
#undef main
#include "../volume_texture_cases.h"
#include NN_BLOCK_IMAGE
static struct nn_tex_native_view native_views[1];
static unsigned sampled_format,source_levels,view_base,view_levels;
#define NN_TEXTURE_NATIVE_VIEWS native_views
#define NN_TEXTURE_NATIVE_VIEW_COUNT 1
static void pool_create(NVNdevice *device,NVNmemoryPoolBuilder *builder_key,NVNmemoryPool *pool,void **storage,size_t bytes,int flags)
{
 T(!posix_memalign(storage,4096,bytes));memset(*storage,0,bytes);
 OK(api_nvnMemoryPoolBuilderSetDefaults(builder_key));OK(api_nvnMemoryPoolBuilderSetDevice(builder_key,device));
 OK(api_nvnMemoryPoolBuilderSetStorage(builder_key,*storage,bytes));
 OK(api_nvnMemoryPoolBuilderSetFlags(builder_key,flags));
 T(api_nvnMemoryPoolInitialize(pool,builder_key));
}

#include "pipeline_enqueue_fault_checks.h"
static int one(unsigned format,int admit)
{ const unsigned source=0;
 mock_submits=0;mock_submit_error=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *device=find(&dk,DEVICE);
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_typed_pipeline_archive(&dk,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,reader,ranges));
 T(nn_nvn_register_pipeline_views(&dk,NULL,1)==EINVAL);T(!nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT));T(!nn_nvn_register_block_copy(&dk,&NVN_COMPILED_IMAGE));
 T(nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT)==EEXIST);
 /* Fixture chooses full original source groups only; runtime specialization
  * is selected exclusively from the real enqueue entries below. */
 const struct nn_offline_graphics_stage *stages[2]={0};
 for(size_t i=0;i<archive->pair_count;i++){
  const struct nn_pipeline_pair_recipe*pair=archive->pairs+i;const struct nn_offline_graphics_stage*fs=archive->stages->stages+pair->fs_index;
  for(size_t j=0;j<fs->texture_count;j++)if(fs->textures[j].type==2){stages[0]=archive->stages->stages+pair->vs_index;stages[1]=fs;break;}
  if(stages[0])break;
 }
 T(stages[0]&&stages[1]);
 enum{VS,FS,ARENA,DATA,RT,UNUSED,META,SAMPLE0,SAMPLE1,SAMPLE_ALT,POOLS};NVNmemoryPoolBuilder pb[POOLS];NVNmemoryPool pools[POOLS];void *storage[POOLS];memset(pb,0,sizeof(pb));memset(pools,0,sizeof(pools));
 for(unsigned i=0;i<POOLS;i++)pool_create(&dk,pb+i,pools+i,storage+i,i==SAMPLE0?1048576:(i==RT||i==SAMPLE1||i==SAMPLE_ALT)?32768:16384,(i==UNUSED||i==META)?0x14:0x24);
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
 /* Call projection requires explicitly defined active render state. These
  * public value bindings are copied with the child, never invented defaults. */
 NVNcolorState color;NVNchannelMaskState mask;NVNpolygonState poly;NVNdepthStencilState depth;NVNmultisampleState multi;NVNblendState blend;
 OK(api_nvnColorStateSetDefaults(&color));OK(api_nvnChannelMaskStateSetDefaults(&mask));OK(api_nvnPolygonStateSetDefaults(&poly));
 OK(api_nvnDepthStencilStateSetDefaults(&depth));OK(api_nvnMultisampleStateSetDefaults(&multi));OK(api_nvnBlendStateSetDefaults(&blend));
 OK(api_nvnCommandBufferBindColorState(&ck,&color));OK(api_nvnCommandBufferBindChannelMaskState(&ck,&mask));
 OK(api_nvnCommandBufferBindPolygonState(&ck,&poly));OK(api_nvnCommandBufferBindDepthStencilState(&ck,&depth));
 OK(api_nvnCommandBufferBindMultisampleState(&ck,&multi));OK(api_nvnCommandBufferBindBlendState(&ck,&blend));
 OK(api_nvnCommandBufferSetDepthBounds(&ck,0,0,1));
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
 NVNtextureBuilder sampled_builder[2]={0};NVNtexture sampled[2]={0};size_t sample_offset=4096;
 for(unsigned i=0;i<2;i++){
  OK(api_nvnTextureBuilderSetDefaults(sampled_builder+i));OK(api_nvnTextureBuilderSetDevice(sampled_builder+i,&dk));
  if(i){OK(api_nvnTextureBuilderSetTarget(sampled_builder+i,NVN_TEXTURE_TARGET_BUFFER));OK(api_nvnTextureBuilderSetSize1D(sampled_builder+i,17));OK(api_nvnTextureBuilderSetFormat(sampled_builder+i,(NVNformat)format));}
  else{OK(api_nvnTextureBuilderSetSize2D(sampled_builder+i,64,64));OK(api_nvnTextureBuilderSetFormat(sampled_builder+i,(NVNformat)sampled_format));OK(api_nvnTextureBuilderSetLevels(sampled_builder+i,source_levels));}
  if(i){size_t alignment=api_nvnTextureBuilderGetStorageAlignment(sampled_builder+i);T(alignment&&alignment<64&&!nn_nvn_error());sample_offset+=alignment;}
  OK(api_nvnTextureBuilderSetStorage(sampled_builder+i,pools+SAMPLE0+i,i?sample_offset:4096));T(api_nvnTextureInitialize(sampled+i,sampled_builder+i));
  NVNtextureView view={0};OK(api_nvnTextureViewSetDefaults(&view));OK(api_nvnTextureViewSetLevels(&view,view_base,view_levels));
  OK(api_nvnTexturePoolRegisterTexture(&texture_pool,257+i,sampled+i,i?NULL:&view));OK(api_nvnTextureFinalize(sampled+i));
 }
 before_alloc=mock_allocations;before_cache=mock_caches;
 struct pipeline_batch batch={0};int admission=pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&batch);
#if !NN_NVN_BUFFER_SAMPLED || !NN_NVN_SAMPLED_IMAGES
 admit=0;
#endif
 if(!admit){T(admission==ENOTSUP);T(mock_allocations==before_alloc&&mock_caches==before_cache&&!mock_submits);pipeline_batch_discard(&batch);goto cleanup;}
 T(!admission);const struct pipeline_prepared_draw*prepared=pipeline_batch_find(&batch,recorded,0);T(prepared&&prepared->draw.pipeline.recipe&&prepared->draw.pipeline.native.texture_types);
 T(prepared->draw.pipeline.recipe->texture_mask==6&&!prepared->draw.pipeline.recipe->sampler_mask);
#if NN_NVN_SAMPLED_IMAGES
 T(prepared->color_images==2&&prepared->color_plan[1].count==source_levels);
 T(prepared->draw.pipeline.native.sampled_color[1]->base_level==view_base&&prepared->draw.pipeline.native.sampled_color[1]->level_count==view_levels);
 T(prepared->draw.pipeline.native.texture_types->operations[1]==(NN_PIPELINE_TEX_FETCH|NN_PIPELINE_TEX_SIZE));
#endif
 const struct nn_buffer_sampled_binding*b=prepared->draw.pipeline.native.sampled_buffer[2];T(b==prepared->buffers+2&&b->format==format&&b->offset==sample_offset&&b->resource==find(pools+SAMPLE1,POOL)->bo&&b->owner_bytes==32768);
 T(prepared->native_entries[2]->snapshot.texture.target==NVN_TEXTURE_TARGET_BUFFER&&prepared->native_entries[2]->source_layout==NULL);
 T(!prepared->draw.pipeline.native.textures[2].layout&&!prepared->draw.pipeline.native.textures[2].buffer.bo);
 T(mock_allocations==before_alloc&&mock_caches==before_cache);
 struct nn_fdl_buffer_view fd;T(!nn_buffer_sampled_view(b,&fd)&&fd.elements==17);
 T(fd.base_address==nn_bo_address(b->resource)+4096&&fd.required_span==sample_offset-4096+b->bytes);
 /* Same original pool/BO feedback must be rejected before any GPU work.
  * The valid prepared snapshot still owns its old descriptor backing. */
 NVNtexture feedback={0};OK(api_nvnTextureBuilderSetStorage(sampled_builder+1,pools+RT,(ptrdiff_t)sample_offset));
 T(api_nvnTextureInitialize(&feedback,sampled_builder+1));OK(api_nvnTexturePoolRegisterTexture(&texture_pool,258,&feedback,NULL));OK(api_nvnTextureFinalize(&feedback));
 before_alloc=mock_allocations;before_cache=mock_caches;struct pipeline_batch overlap={0};T(pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&overlap)==ENOTSUP);pipeline_batch_discard(&overlap);
 T(mock_allocations==before_alloc&&mock_caches==before_cache&&!mock_submits);
 OK(api_nvnTextureBuilderSetStorage(sampled_builder+1,pools+SAMPLE1,(ptrdiff_t)sample_offset));T(api_nvnTextureInitialize(&feedback,sampled_builder+1));OK(api_nvnTexturePoolRegisterTexture(&texture_pool,258,&feedback,NULL));OK(api_nvnTextureFinalize(&feedback));
 /* Actual immutable registration versions survive both public Texture
  * Finalize and replacement; enqueue never rereads the mutable slot later. */
 NVNtextureBuilder replacement_builder={0};NVNtexture replacement={0};OK(api_nvnTextureBuilderSetDefaults(&replacement_builder));OK(api_nvnTextureBuilderSetDevice(&replacement_builder,&dk));OK(api_nvnTextureBuilderSetTarget(&replacement_builder,NVN_TEXTURE_TARGET_BUFFER));OK(api_nvnTextureBuilderSetSize1D(&replacement_builder,31));OK(api_nvnTextureBuilderSetFormat(&replacement_builder,(NVNformat)format));OK(api_nvnTextureBuilderSetStorage(&replacement_builder,pools+SAMPLE_ALT,4096));T(api_nvnTextureInitialize(&replacement,&replacement_builder));OK(api_nvnTexturePoolRegisterTexture(&texture_pool,258,&replacement,NULL));OK(api_nvnTextureFinalize(&replacement));
 T(b->resource==find(pools+SAMPLE1,POOL)->bo&&b->bytes==17u*fd.element_bytes);ERR(api_nvnMemoryPoolFinalize(pools+SAMPLE1),EBUSY);
 struct pipeline_batch updated={0};T(!pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&updated));const struct pipeline_prepared_draw*newer=pipeline_batch_find(&updated,recorded,0);T(newer&&newer->buffers[2].resource==find(pools+SAMPLE_ALT,POOL)->bo&&newer->buffers[2].bytes==31u*fd.element_bytes);pipeline_batch_discard(&updated);pipeline_batch_discard(&batch);
 metadata_preflight_oom(&qk,handle);
 /* Public Copy and Call preserve selectors; actual descriptor identity is
  * resolved afresh, while source-handle finalization cannot free the owner. */
 OK(api_nvnCommandBufferBeginRecording(&ck));OK(api_nvnCommandBufferCopyCommands(&ck,1,&handle));NVNcommandHandle copied=api_nvnCommandBufferEndRecording(&ck);T(copied&&!nn_nvn_error());NVNcommandHandle original_handle=handle;handle=copied;
 OK(api_nvnQueueSubmitCommands(&qk,1,&handle));T(mock_submits==1);int found=0;for(size_t k=0;k<mock_ref_count;k++)found|=mock_refs[k]==find(pools+SAMPLE_ALT,POOL)->bo;T(found);ERR(api_nvnMemoryPoolFinalize(pools+SAMPLE_ALT),EBUSY);OK(api_nvnQueueFinish(&qk));
 handle=original_handle;OK(api_nvnCommandBufferBeginRecording(&ck));OK(api_nvnCommandBufferCallCommands(&ck,1,&handle));NVNcommandHandle called=api_nvnCommandBufferEndRecording(&ck);T(called&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&dk,handle));handle=called;OK(api_nvnQueueSubmitCommands(&qk,1,&handle));T(mock_submits==2);OK(api_nvnQueueFinish(&qk));
 /* The first Call activates semantic queue continuation. A copied direct
  * body must still replay with its own state/binding deltas after that Call. */
 T(find(&qk,QUEUE)->call_queue_active);OK(api_nvnQueueSubmitCommands(&qk,1,&copied));T(mock_submits==3);OK(api_nvnQueueFinish(&qk));
 OK(api_nvnQueueSubmitCommands(&qk,1,&handle));T(mock_submits==4);OK(api_nvnQueueFinish(&qk));OK(api_nvnDeviceFinalizeCommandHandle(&dk,copied));
 mock_submit_error=EIO;ERR(api_nvnQueueSubmitCommands(&qk,1,&handle),EIO);mock_submit_error=0;T(mock_submits==5&&device->retired);OK(api_nvnDeviceFinalizeCommandHandle(&dk,handle));handle=0;ERR(api_nvnTexturePoolFinalize(&texture_pool),EBUSY);ERR(api_nvnMemoryPoolFinalize(pools+SAMPLE_ALT),EBUSY);OK(api_nvnQueueFinish(&qk));T(!device->retired);
 cleanup:
 if(handle)OK(api_nvnDeviceFinalizeCommandHandle(&dk,handle));
 OK(api_nvnCommandBufferFinalize(&ck));OK(api_nvnTexturePoolFinalize(&texture_pool));OK(api_nvnProgramFinalize(&pk));OK(api_nvnTextureFinalize(&tk));OK(api_nvnQueueFinalize(&qk));
 for(unsigned i=ARENA;i<POOLS;i++)if(storage[i]){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}
 OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
 printf("HOST public FLOAT2D FETCH/SIZE format%u mips%u view%u+%u admit%u checks=%u mock_submits=%u GPU execution=false\n",sampled_format,source_levels,view_base,view_levels,admit,checks,mock_submits);return 0;
}
int main(void)
{
 T(!nn_volume_texture_target(64,64,native_views));unsigned formats=0;
 for(unsigned f=0;f<200;f++){
  struct nn_fdl_format_info info;
  if(nn_fdl_image_format_info(0x06050002,f,&info)||info.result_type!=NN_FDL_RESULT_FLOAT||info.block_width!=1||info.block_height!=1)continue;
  formats++;
  for(unsigned scenario=0;scenario<4;scenario++){
   sampled_format=f;source_levels=scenario==0?1:scenario==3?7:4;view_base=scenario==2?1:scenario==3?4:0;
   view_levels=scenario==2?2:scenario==3?3:source_levels;one(NVN_FORMAT_R32F,1);
  }
 }
 T(formats==22);return 0;
}
