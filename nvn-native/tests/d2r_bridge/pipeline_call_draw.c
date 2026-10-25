/* HOST ONLY actual typed NVN record/enqueue/retire. No GPU execution/pixels. */
#define main source_identity_fixture
#include NN_SOURCE_FIXTURE
#undef main
#include NN_PIPELINE_NATIVE_VIEWS
#include NN_PIPELINE_TEXTURE_BRIDGE
#include "pipeline_fixture_pair.h"
static void pool_create(NVNdevice *device,NVNmemoryPoolBuilder *builder_key,NVNmemoryPool *pool,void **storage,size_t bytes)
{
 T(!posix_memalign(storage,4096,bytes));memset(*storage,0,bytes);
 OK(api_nvnMemoryPoolBuilderSetDefaults(builder_key));OK(api_nvnMemoryPoolBuilderSetDevice(builder_key,device));
 OK(api_nvnMemoryPoolBuilderSetStorage(builder_key,*storage,bytes));
 OK(api_nvnMemoryPoolBuilderSetFlags(builder_key,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));
 T(api_nvnMemoryPoolInitialize(pool,builder_key));
}
/* Specialization-dependent checks now belong to enqueue, while metadata
 * record/Copy stays independent of sparse descriptor registrations. */
static void recorded_draw_reject(NVNcommandBuffer *key,struct object *device,int expected)
{
 unsigned submits=mock_submits;size_t bos=device->gpu->bos;
 api_nvnCommandBufferDrawArrays(key,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3);
 if(nn_nvn_error()){T(nn_nvn_error()==expected);nn_nvn_clear_error();return;}
 NVNcommandHandle handle=api_nvnCommandBufferEndRecording(key);T(handle&&!nn_nvn_error());
 struct object queue={.device=device};struct pipeline_batch batch={0};
 T(pipeline_batch_prepare(&queue,1,&handle,&batch)==expected);pipeline_batch_discard(&batch);
 T(device->gpu->bos==bos&&mock_submits==submits);
 OK(api_nvnDeviceFinalizeCommandHandle((NVNdevice*)device->key,handle));OK(api_nvnCommandBufferBeginRecording(key));
}
static int one(unsigned source)
{
 mock_submits=0;mock_submit_error=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *device=find(&dk,DEVICE);
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_pipeline_archive(&dk,archive,reader,ranges));
 T(!nn_nvn_register_texture_bridge(&dk,&NN_TEXTURE_BRIDGE_BUNDLE));
 T(nn_nvn_register_pipeline_views(&dk,NULL,1)==EINVAL);T(!nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT));
 T(nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT)==EEXIST);
 struct nn_pipeline_frontend_queries selection={.version=1,.chip=0x06050002,.primitive=4,.point_size=1};nn_render_state_defaults(&selection.render);
 const struct nn_pipeline_pair_recipe *pair=pipeline_fixture_color_pair(archive,&selection);
 T(pair);const struct nn_offline_graphics_stage *stages[]={archive->stages->stages+pair->vs_index,archive->stages->stages+pair->fs_index};
 enum{VS,FS,ARENA,DATA,RT,POOLS};NVNmemoryPoolBuilder pb[POOLS];NVNmemoryPool pools[POOLS];void *storage[POOLS];memset(pb,0,sizeof(pb));memset(pools,0,sizeof(pools));
 for(unsigned i=0;i<POOLS;i++)pool_create(&dk,pb+i,pools+i,storage+i,i==RT?32768:16384);
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
 NVNvertexAttribState attr[2];for(unsigned i=0;i<2;i++){OK(api_nvnVertexAttribStateSetDefaults(attr+i));OK(af(attr+i,i?NVN_FORMAT_RGBA8:NVN_FORMAT_RG32F,i?12:4));OK(ai(attr+i,7));}
 OK(api_nvnCommandBufferBindVertexAttribState(&ck,2,attr));memset(attr,0xcc,sizeof(attr));
 NVNvertexStreamState streams[8];for(unsigned i=0;i<8;i++)OK(api_nvnVertexStreamStateSetDefaults(streams+i));
 OK(api_nvnVertexStreamStateSetStride(streams+7,48));OK(api_nvnCommandBufferBindVertexStreamState(&ck,8,streams));memset(streams,0xcc,sizeof(streams));
 uint64_t data=api_nvnMemoryPoolGetBufferAddress(pools+DATA);OK(api_nvnCommandBufferBindVertexBuffer(&ck,7,data+4096,4096));
 OK(api_nvnCommandBufferBindUniformBuffer(&ck,NVN_SHADER_STAGE_VERTEX,0,data+8192,64));
 const NVNtexture *targets[]={&tk};OK(api_nvnCommandBufferSetRenderTargets(&ck,1,targets,NULL,NULL,NULL));
 OK(api_nvnCommandBufferSetViewport(&ck,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&ck,0,0,64,64));OK(api_nvnCommandBufferSetDepthRange(&ck,0,1));
 struct recording *r=find(&ck,COMMAND)->record;size_t used=r->used,refs=r->current->count;
 /* SDK Device properties6/14 are both16; VFD32 is only native capacity. */
 NVNvertexAttribState edge={0};OK(ai(&edge,15));uint32_t edge_value=attribute_read(&edge);
 ERR(ai(&edge,16),EINVAL);T(attribute_read(&edge)==edge_value);
 ERR(api_nvnCommandBufferBindVertexAttribState(&ck,17,attr),EINVAL);
 ERR(api_nvnCommandBufferBindVertexStreamState(&ck,17,streams),EINVAL);
 OK(api_nvnCommandBufferBindVertexBuffers(&ck,16,0,NULL));
 ERR(api_nvnCommandBufferBindVertexBuffers(&ck,17,0,NULL),EINVAL);
 ERR(api_nvnCommandBufferBindVertexBuffer(&ck,16,data,16),EINVAL);
 T(r->used==used&&r->current->count==refs&&!r->current->draws);
 /* SDK53.314 writes disabled tails through slot15: count0 is a reset,
  * and a shorter prefix must not preserve a previously consumed high slot. */
 OK(api_nvnCommandBufferBindVertexAttribState(&ck,0,NULL));T(!r->attributes_set&&!r->attributes[1]);
 recorded_draw_reject(&ck,device,EINVAL);used=r->used;refs=r->current->count;
 for(unsigned i=0;i<2;i++){OK(api_nvnVertexAttribStateSetDefaults(attr+i));OK(af(attr+i,i?NVN_FORMAT_RGBA8:NVN_FORMAT_RG32F,i?12:4));OK(ai(attr+i,7));}
 OK(api_nvnCommandBufferBindVertexAttribState(&ck,2,attr));
 OK(api_nvnCommandBufferBindVertexAttribState(&ck,1,attr));T(r->attributes_set==1&&!r->attributes[1]);
 recorded_draw_reject(&ck,device,EINVAL);used=r->used;refs=r->current->count;
 OK(api_nvnCommandBufferBindVertexAttribState(&ck,2,attr));
 OK(api_nvnCommandBufferBindVertexStreamState(&ck,0,NULL));T(!r->vertex_stream_count&&!r->vertex_streams[7].stride);
 recorded_draw_reject(&ck,device,EINVAL);used=r->used;refs=r->current->count;
 for(unsigned i=0;i<8;i++)OK(api_nvnVertexStreamStateSetDefaults(streams+i));
 OK(api_nvnVertexStreamStateSetStride(streams+7,48));
 OK(api_nvnCommandBufferBindVertexStreamState(&ck,8,streams));
 OK(api_nvnCommandBufferBindVertexStreamState(&ck,1,streams));T(r->vertex_stream_count==1&&!r->vertex_streams[7].stride);
 recorded_draw_reject(&ck,device,EINVAL);used=r->used;refs=r->current->count;
 OK(api_nvnCommandBufferBindVertexStreamState(&ck,8,streams));
 T(r->used==used&&r->current->count==refs&&!r->current->draws);
 NVNvertexStreamState boundary[8];for(unsigned i=0;i<8;i++)OK(api_nvnVertexStreamStateSetDefaults(boundary+i));
 OK(api_nvnVertexStreamStateSetStride(boundary+7,4096));OK(api_nvnCommandBufferBindVertexStreamState(&ck,8,boundary));
 T(r->vertex_streams[7].stride==4096);recorded_draw_reject(&ck,device,ENOTSUP);used=r->used;refs=r->current->count;
 T(r->used==used&&r->current->count==refs&&!r->current->draws);
 OK(api_nvnVertexStreamStateSetStride(boundary+7,48));OK(api_nvnCommandBufferBindVertexStreamState(&ck,8,boundary));
 OK(api_nvnCommandBufferBindUniformBuffer(&ck,NVN_SHADER_STAGE_VERTEX,0,data+7,0));T(!r->graphics_ubos[0][0].pool);
 recorded_draw_reject(&ck,device,EINVAL);used=r->used;refs=r->current->count;T(r->used==used&&r->current->count==refs&&!r->current->draws);
 OK(api_nvnCommandBufferBindUniformBuffer(&ck,NVN_SHADER_STAGE_VERTEX,0,data+8192,64));
 OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,2,3));struct late_draw *draw=r->current->draws;
 T(draw&&draw->generic_pipeline&&draw->pipeline.native.draw.first==2&&draw->pipeline.native.vertex.attributes[0].stream==7&&draw->pipeline.native.vertex.streams[7].stride==48);
 T(draw->pipeline.uniforms[0][0].offset==8192&&!draw->pipeline.recipe&&!draw->pipeline.stages[0]);
 OK(api_nvnDeviceSetWindowOriginMode(&dk,NVN_WINDOW_ORIGIN_MODE_UPPER_LEFT));OK(api_nvnDeviceSetDepthMode(&dk,NVN_DEPTH_MODE_NEAR_IS_ZERO));OK(api_nvnCommandBufferBindVertexBuffer(&ck,7,data+12288,4096));
 T(draw->pipeline.native.raster.window_origin==0&&draw->pipeline.vertices[7].offset==4096);
 PFNNVNCOMMANDBUFFERMULTIDRAWELEMENTSINDIRECTCOUNTPROC indirect=(PFNNVNCOMMANDBUFFERMULTIDRAWELEMENTSINDIRECTCOUNTPROC)nvnBootstrapLoader("nvnCommandBufferMultiDrawElementsIndirectCount");T(indirect);
 OK(indirect(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,data+64,data+256,data+512,1,20));
 struct late_draw *second=r->current->draws;T(second!=draw&&second->generic_pipeline&&second->next==draw&&second->pipeline.native.raster.window_origin==1&&second->pipeline.native.raster.depth_mode==1);
 T(!second->pipeline.stages[0]&&!draw->pipeline.stages[0]);
 T(second->pipeline.native.draw.mode==NN_GFX_DRAW_INDIRECT_COUNT&&second->pipeline.native.draw.indexed&&second->pipeline.native.draw.indirect.offset==256&&second->pipeline.native.draw.indirect_count.offset==512);
 T(draw->pipeline.native.draw.mode==NN_GFX_DRAW_DIRECT&&!draw->pipeline.native.draw.indexed&&draw->pipeline.native.raster.window_origin==0);
 NVNcommandHandle h=api_nvnCommandBufferEndRecording(&ck);T(h&&!nn_nvn_error());ERR(api_nvnProgramFinalize(&pk),EBUSY);
 struct object selected_queue={.device=device};struct pipeline_batch selected={0};
 T(!pipeline_batch_prepare(&selected_queue,1,&h,&selected));
 const struct pipeline_prepared_draw *selected_first=pipeline_batch_find(&selected,draw,0),*selected_second=pipeline_batch_find(&selected,second,0);
 T(selected_first&&selected_second);
 T(memcmp(selected_first->draw.pipeline.stages[0]->specialization,selected_second->draw.pipeline.stages[0]->specialization,32)!=0);
 pipeline_batch_discard(&selected);
 /* Seventeen real simple snapshots fit the actual 16KiB control budget.
  * A separately configured 256B queue must reject before materialization. */
 if(!source){
  NVNcommandBuffer many={0};_Alignas(16)unsigned char manycontrol[4096];
  T(api_nvnCommandBufferInitialize(&many,&dk));
  OK(api_nvnCommandBufferAddCommandMemory(&many,pools+ARENA,8192,4096));
  OK(api_nvnCommandBufferAddControlMemory(&many,manycontrol,sizeof(manycontrol)));
  OK(api_nvnCommandBufferBeginRecording(&many));
  OK(api_nvnCommandBufferBindProgram(&many,&pk,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
  for(unsigned i=0;i<2;i++){OK(api_nvnVertexAttribStateSetDefaults(attr+i));OK(af(attr+i,i?NVN_FORMAT_RGBA8:NVN_FORMAT_RG32F,i?12:4));OK(ai(attr+i,7));}
  OK(api_nvnCommandBufferBindVertexAttribState(&many,2,attr));
  OK(api_nvnCommandBufferBindVertexStreamState(&many,8,boundary));
  OK(api_nvnCommandBufferBindVertexBuffer(&many,7,data+4096,4096));
  OK(api_nvnCommandBufferBindUniformBuffer(&many,NVN_SHADER_STAGE_VERTEX,0,data+8192,64));
  OK(api_nvnCommandBufferSetRenderTargets(&many,1,targets,NULL,NULL,NULL));
  OK(api_nvnCommandBufferSetViewport(&many,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&many,0,0,64,64));
  OK(api_nvnCommandBufferSetDepthRange(&many,0,1));
  for(unsigned i=0;i<17;i++)OK(api_nvnCommandBufferDrawArrays(&many,NVN_DRAW_PRIMITIVE_TRIANGLES,2,3));
  NVNcommandHandle mh=api_nvnCommandBufferEndRecording(&many);T(mh&&!nn_nvn_error());
  NVNqueueBuilder smallb={0},fitb={0};NVNqueue small={0},fit={0};
  OK(api_nvnQueueBuilderSetDefaults(&smallb));OK(api_nvnQueueBuilderSetDevice(&smallb,&dk));
  OK(api_nvnQueueBuilderSetControlMemorySize(&smallb,256));T(api_nvnQueueInitialize(&small,&smallb));
  size_t before=device->gpu->bos;unsigned submits=mock_submits;
  ERR(api_nvnQueueSubmitCommands(&small,1,&mh),ENOSPC);
  T(device->gpu->bos==before&&mock_submits==submits&&!device->retired);
  OK(api_nvnQueueFinalize(&small));
  OK(api_nvnQueueBuilderSetDefaults(&fitb));OK(api_nvnQueueBuilderSetDevice(&fitb,&dk));
  T(api_nvnQueueInitialize(&fit,&fitb));T(find(&fit,QUEUE)->queue_config.control==16384);
  OK(api_nvnQueueSubmitCommands(&fit,1,&mh));T(mock_submits==submits+1&&mock_ref_count<128);
  OK(api_nvnQueueFinish(&fit));T(!device->retired);OK(api_nvnQueueFinalize(&fit));
  OK(api_nvnDeviceFinalizeCommandHandle(&dk,mh));OK(api_nvnCommandBufferFinalize(&many));
  mock_submits=0;printf("HOST ONLY control budget simple_draws=17 fit=16384 reject=256 GPU execution=false\n");
 }
 NVNqueueBuilder qb={0};NVNqueue qk={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&qk,&qb));
 OK(api_nvnQueueSubmitCommands(&qk,1,&h));T(mock_submits==1&&mock_ref_count>=5&&device->retired);
 unsigned owners=0;for(struct job *j=device->retired;j;j=j->next)if(j->pipeline_owner){
  owners++;for(unsigned stage=0;stage<2;stage++){
   struct nn_pipeline_stage_view v;T(!nn_pipeline_program_stage(j->pipeline_owner,stage,&v));unsigned seen=0;
   for(size_t i=0;i<mock_ref_count;i++)if(mock_refs[i]==v.code)seen++;
   T(seen==1&&v.code->busy);
  }
 }T(owners==2);
 struct object *q=find(&qk,QUEUE);T(q->queue->count>0);size_t gpu_bos=device->gpu->bos;
 OK(api_nvnQueueSubmitCommands(&qk,1,&h));T(mock_submits==2&&device->gpu->bos>gpu_bos); /* New immutable version per replay. */
 NVNcommandBuffer copy={0};_Alignas(16)unsigned char copycontrol[1024];T(api_nvnCommandBufferInitialize(&copy,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&copy,pools+ARENA,4096,4096));OK(api_nvnCommandBufferAddControlMemory(&copy,copycontrol,sizeof(copycontrol)));OK(api_nvnCommandBufferBeginRecording(&copy));
 OK(api_nvnCommandBufferCopyCommands(&copy,1,&h));NVNcommandHandle copied=api_nvnCommandBufferEndRecording(&copy);T(copied&&!nn_nvn_error());OK(api_nvnCommandBufferFinalize(&copy));
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,h));OK(api_nvnCommandBufferFinalize(&ck));ERR(api_nvnProgramFinalize(&pk),EBUSY);
 mock_submit_error=EIO;ERR(api_nvnQueueSubmitCommands(&qk,1,&copied),EIO);T(mock_submits==3&&device->retired);mock_submit_error=0;
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,copied));ERR(api_nvnProgramFinalize(&pk),EBUSY);
 ERR(api_nvnTextureFinalize(&tk),EBUSY);ERR(api_nvnMemoryPoolFinalize(pools+RT),EBUSY);
 OK(api_nvnQueueFinish(&qk));T(!device->retired);OK(api_nvnProgramFinalize(&pk));OK(api_nvnTextureFinalize(&tk));OK(api_nvnQueueFinalize(&qk));
 for(unsigned i=2;i<POOLS;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}
 T(!device->gpu->bos);OK(api_nvnDeviceFinalize(&dk));
 while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
 T(!handles);printf("HOST ONLY NVN dynamic Draw source=%u checks=%u mock_submits=%u GPU execution=false\n",source,checks,mock_submits);return 0;
}

static int call_one(unsigned source)
{
 mock_submits=0;mock_submit_error=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *device=find(&dk,DEVICE);
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_pipeline_archive(&dk,archive,reader,ranges));
 T(!nn_nvn_register_texture_bridge(&dk,&NN_TEXTURE_BRIDGE_BUNDLE));
 T(nn_nvn_register_pipeline_views(&dk,NULL,1)==EINVAL);T(!nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT));
 T(nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT)==EEXIST);
 struct nn_pipeline_frontend_queries selection={.version=1,.chip=0x06050002,.primitive=4,.point_size=1};nn_render_state_defaults(&selection.render);
 const struct nn_pipeline_pair_recipe *pair=pipeline_fixture_color_pair(archive,&selection);
 T(pair);const struct nn_offline_graphics_stage *stages[]={archive->stages->stages+pair->vs_index,archive->stages->stages+pair->fs_index};
 enum{VS,FS,ARENA,DATA,RT,POOLS};NVNmemoryPoolBuilder pb[POOLS];NVNmemoryPool pools[POOLS];void *storage[POOLS];memset(pb,0,sizeof(pb));memset(pools,0,sizeof(pools));
 for(unsigned i=0;i<POOLS;i++)pool_create(&dk,pb+i,pools+i,storage+i,i==RT?32768:16384);
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
 NVNvertexAttribState attr[2];for(unsigned i=0;i<2;i++){OK(api_nvnVertexAttribStateSetDefaults(attr+i));OK(af(attr+i,i?NVN_FORMAT_RGBA8:NVN_FORMAT_RG32F,i?12:4));OK(ai(attr+i,7));}
 OK(api_nvnCommandBufferBindVertexAttribState(&ck,2,attr));memset(attr,0xcc,sizeof(attr));
 NVNvertexStreamState streams[8];for(unsigned i=0;i<8;i++)OK(api_nvnVertexStreamStateSetDefaults(streams+i));
 OK(api_nvnVertexStreamStateSetStride(streams+7,48));OK(api_nvnCommandBufferBindVertexStreamState(&ck,8,streams));memset(streams,0xcc,sizeof(streams));
 uint64_t data=api_nvnMemoryPoolGetBufferAddress(pools+DATA);OK(api_nvnCommandBufferBindVertexBuffer(&ck,7,data+4096,4096));

 const NVNtexture *targets[]={&tk};OK(api_nvnCommandBufferSetRenderTargets(&ck,1,targets,NULL,NULL,NULL));
 OK(api_nvnCommandBufferSetViewport(&ck,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&ck,0,0,64,64));OK(api_nvnCommandBufferSetDepthRange(&ck,0,1));

 /* No UBO is recorded by child: enqueue must consume caller's GPU binding,
  * not the child's old CPU recording snapshot. */
 OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,2,3));
 struct late_draw *original=find(&ck,COMMAND)->record->current->draws;
 T(original&&original->generic_pipeline&&!original->pipeline.uniforms[0][0].pool);
 NVNcommandHandle child=api_nvnCommandBufferEndRecording(&ck);T(child&&!nn_nvn_error());
 struct handle *child_body=handles;T(child_body->id==child);
 NVNcommandBuffer parent={0};_Alignas(16) unsigned char pc[4096];T(api_nvnCommandBufferInitialize(&parent,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&parent,pools+ARENA,8192,4096));
 OK(api_nvnCommandBufferAddControlMemory(&parent,pc,sizeof(pc)));OK(api_nvnCommandBufferBeginRecording(&parent));
 NVNcolorState color;NVNchannelMaskState mask;NVNpolygonState poly;NVNdepthStencilState depth;NVNmultisampleState multi;NVNblendState blend;
 OK(api_nvnColorStateSetDefaults(&color));OK(api_nvnChannelMaskStateSetDefaults(&mask));OK(api_nvnPolygonStateSetDefaults(&poly));
 OK(api_nvnDepthStencilStateSetDefaults(&depth));OK(api_nvnMultisampleStateSetDefaults(&multi));OK(api_nvnBlendStateSetDefaults(&blend));
 OK(api_nvnCommandBufferBindColorState(&parent,&color));OK(api_nvnCommandBufferBindChannelMaskState(&parent,&mask));
 OK(api_nvnCommandBufferBindPolygonState(&parent,&poly));OK(api_nvnCommandBufferBindDepthStencilState(&parent,&depth));
 OK(api_nvnCommandBufferBindMultisampleState(&parent,&multi));OK(api_nvnCommandBufferBindBlendState(&parent,&blend));
 OK(api_nvnCommandBufferSetDepthBounds(&parent,0,0,1));
 OK(api_nvnCommandBufferBindUniformBuffer(&parent,NVN_SHADER_STAGE_VERTEX,0,data+8192,64));
 OK(api_nvnCommandBufferCallCommands(&parent,1,&child));
 T(!find(&parent,COMMAND)->record->graphics_program&&!find(&parent,COMMAND)->record->target);
 OK(api_nvnCommandBufferDrawArrays(&parent,NVN_DRAW_PRIMITIVE_TRIANGLES,4,3));
 NVNcommandHandle selected[2];selected[0]=api_nvnCommandBufferEndRecording(&parent);T(selected[0]&&!nn_nvn_error());
 OK(api_nvnCommandBufferBeginRecording(&parent));OK(api_nvnCommandBufferCallCommands(&parent,1,&child));
 selected[1]=api_nvnCommandBufferEndRecording(&parent);T(selected[1]&&!nn_nvn_error());
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,child));OK(api_nvnCommandBufferFinalize(&ck));T(child_body->public_retired&&child_body->call_references==2);
 NVNqueueBuilder qb={0};NVNqueue qk={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&qk,&qb));
 struct object *queue=find(&qk,QUEUE);size_t before=device->gpu->bos;
 /* Undefined active state fails before a native submission. */
 ERR(api_nvnQueueSubmitCommands(&qk,1,selected+1),ENOTSUP);T(!mock_submits&&device->gpu->bos==before&&!queue->call_queue_active);
 struct nn_command_state initial={0},final={0};struct handle *projected=NULL,*first=handles;
 while(first&&first->id!=selected[0])first=first->next;
 T(first);
 T(!command_project_sequence(queue,first,&initial,&final,&projected));
 T(projected&&projected->draws&&projected->draws!=original);
 T(projected->draws->pipeline.uniforms[0][0].pool==find(pools+DATA,POOL));
 T(projected->draws->pipeline.uniforms[0][0].offset==8192&&projected->draws->pipeline.vertices[7].offset==4096);
 T(projected->draws->pipeline.native.draw.first==4&&projected->draws->pipeline.native.raster.window_origin==0);
 T(!(final.defined&NN_COMMAND_STATE_BLEND_COLOR)); /* inactive normalization never invents a GPU write */
 command_body_destroy(projected);
 OK(api_nvnQueueSubmitCommands(&qk,2,selected));T(mock_submits==2&&device->retired&&queue->call_queue_active);
 T(queue->call_queue_state&&nn_command_state_binding(queue->call_queue_state,NN_COMMAND_UBO,0,0)->offset==8192);
 /* The next top-level handle inherits the prior enqueue's effective state. */
 OK(api_nvnQueueSubmitCommands(&qk,1,selected+1));T(mock_submits==3);
 unsigned owners=0;for(struct job *j=device->retired;j;j=j->next)if(j->pipeline_owner)owners++;T(owners==4);
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,selected[0]));OK(api_nvnDeviceFinalizeCommandHandle(&dk,selected[1]));
 OK(api_nvnCommandBufferFinalize(&parent));ERR(api_nvnProgramFinalize(&pk),EBUSY);
 OK(api_nvnQueueFinish(&qk));T(!device->retired);OK(api_nvnProgramFinalize(&pk));OK(api_nvnTextureFinalize(&tk));OK(api_nvnQueueFinalize(&qk));
 for(unsigned i=2;i<POOLS;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}
 T(!device->gpu->bos);OK(api_nvnDeviceFinalize(&dk));
 while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
 T(!handles);printf("HOST ONLY Call Draw source=%u inherited_UBO=8192 selected_handles=2 mock_submits=3 GPU execution=false\n",source);return 0;
}
static int call_after_ordinary(unsigned source)
{
 mock_submits=0;mock_submit_error=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *device=find(&dk,DEVICE);
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_pipeline_archive(&dk,archive,reader,ranges));
 T(!nn_nvn_register_texture_bridge(&dk,&NN_TEXTURE_BRIDGE_BUNDLE));
 T(nn_nvn_register_pipeline_views(&dk,NULL,1)==EINVAL);T(!nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT));
 T(nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT)==EEXIST);
 struct nn_pipeline_frontend_queries selection={.version=1,.chip=0x06050002,.primitive=4,.point_size=1};nn_render_state_defaults(&selection.render);
 const struct nn_pipeline_pair_recipe *pair=pipeline_fixture_color_pair(archive,&selection);
 T(pair);const struct nn_offline_graphics_stage *stages[]={archive->stages->stages+pair->vs_index,archive->stages->stages+pair->fs_index};
 enum{VS,FS,ARENA,DATA,RT,POOLS};NVNmemoryPoolBuilder pb[POOLS];NVNmemoryPool pools[POOLS];void *storage[POOLS];memset(pb,0,sizeof(pb));memset(pools,0,sizeof(pools));
 for(unsigned i=0;i<POOLS;i++)pool_create(&dk,pb+i,pools+i,storage+i,i==RT?32768:16384);
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
 NVNvertexAttribState attr[2];for(unsigned i=0;i<2;i++){OK(api_nvnVertexAttribStateSetDefaults(attr+i));OK(af(attr+i,i?NVN_FORMAT_RGBA8:NVN_FORMAT_RG32F,i?12:4));OK(ai(attr+i,7));}
 OK(api_nvnCommandBufferBindVertexAttribState(&ck,2,attr));memset(attr,0xcc,sizeof(attr));
 NVNvertexStreamState streams[8];for(unsigned i=0;i<8;i++)OK(api_nvnVertexStreamStateSetDefaults(streams+i));
 OK(api_nvnVertexStreamStateSetStride(streams+7,48));OK(api_nvnCommandBufferBindVertexStreamState(&ck,8,streams));memset(streams,0xcc,sizeof(streams));
 uint64_t data=api_nvnMemoryPoolGetBufferAddress(pools+DATA);OK(api_nvnCommandBufferBindVertexBuffer(&ck,7,data+4096,4096));

 const NVNtexture *targets[]={&tk};OK(api_nvnCommandBufferSetRenderTargets(&ck,1,targets,NULL,NULL,NULL));
 OK(api_nvnCommandBufferSetViewport(&ck,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&ck,0,0,64,64));OK(api_nvnCommandBufferSetDepthRange(&ck,0,1));

 /* No UBO is recorded by child: enqueue must consume caller's GPU binding,
  * not the child's old CPU recording snapshot. */
 OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,2,3));
 struct late_draw *original=find(&ck,COMMAND)->record->current->draws;
 T(original&&original->generic_pipeline&&!original->pipeline.uniforms[0][0].pool);
 NVNcommandHandle child=api_nvnCommandBufferEndRecording(&ck);T(child&&!nn_nvn_error());
 struct handle *child_body=handles;T(child_body->id==child);
 NVNcommandBuffer parent={0};_Alignas(16) unsigned char pc[4096];T(api_nvnCommandBufferInitialize(&parent,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&parent,pools+ARENA,8192,4096));
 OK(api_nvnCommandBufferAddControlMemory(&parent,pc,sizeof(pc)));OK(api_nvnCommandBufferBeginRecording(&parent));
 NVNcolorState color;NVNchannelMaskState mask;NVNpolygonState poly;NVNdepthStencilState depth;NVNmultisampleState multi;NVNblendState blend;
 OK(api_nvnColorStateSetDefaults(&color));OK(api_nvnChannelMaskStateSetDefaults(&mask));OK(api_nvnPolygonStateSetDefaults(&poly));
 OK(api_nvnDepthStencilStateSetDefaults(&depth));OK(api_nvnMultisampleStateSetDefaults(&multi));OK(api_nvnBlendStateSetDefaults(&blend));
 OK(api_nvnCommandBufferBindColorState(&parent,&color));OK(api_nvnCommandBufferBindChannelMaskState(&parent,&mask));
 OK(api_nvnCommandBufferBindPolygonState(&parent,&poly));OK(api_nvnCommandBufferBindDepthStencilState(&parent,&depth));
 OK(api_nvnCommandBufferBindMultisampleState(&parent,&multi));OK(api_nvnCommandBufferBindBlendState(&parent,&blend));
 OK(api_nvnCommandBufferSetDepthBounds(&parent,0,0,1));
 OK(api_nvnCommandBufferBindUniformBuffer(&parent,NVN_SHADER_STAGE_VERTEX,0,data+8192,64));
 NVNcommandHandle ordinary=api_nvnCommandBufferEndRecording(&parent);T(ordinary&&!nn_nvn_error());
 OK(api_nvnCommandBufferBeginRecording(&parent));
 OK(api_nvnCommandBufferCallCommands(&parent,1,&child));
 T(!find(&parent,COMMAND)->record->graphics_program&&!find(&parent,COMMAND)->record->target);
 OK(api_nvnCommandBufferDrawArrays(&parent,NVN_DRAW_PRIMITIVE_TRIANGLES,4,3));
 NVNcommandHandle selected[2];selected[0]=api_nvnCommandBufferEndRecording(&parent);T(selected[0]&&!nn_nvn_error());
 OK(api_nvnCommandBufferBeginRecording(&parent));OK(api_nvnCommandBufferCallCommands(&parent,1,&child));
 selected[1]=api_nvnCommandBufferEndRecording(&parent);T(selected[1]&&!nn_nvn_error());
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,child));OK(api_nvnCommandBufferFinalize(&ck));T(child_body->public_retired&&child_body->call_references==2);
 NVNqueueBuilder qb={0};NVNqueue qk={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&qk,&qb));
 struct object *queue=find(&qk,QUEUE);size_t before=device->gpu->bos;
 /* Undefined active state fails before a native submission. */
 ERR(api_nvnQueueSubmitCommands(&qk,1,selected+1),ENOTSUP);T(!mock_submits&&device->gpu->bos==before&&!queue->call_queue_active);
 /* A failed first ordinary submission must not publish its render/UBO writes. */
 mock_submit_error=EIO;ERR(api_nvnQueueSubmitCommands(&qk,1,&ordinary),EIO);mock_submit_error=0;
 T(mock_submits==1&&!queue->call_queue_state&&!queue->call_queue_active);
 ERR(api_nvnQueueSubmitCommands(&qk,1,selected+1),ENOTSUP);T(mock_submits==1&&!queue->call_queue_state);
 OK(api_nvnQueueFinish(&qk));
 /* This submission has no Call nodes, so it must use the ordinary fast path. */
 OK(api_nvnQueueSubmitCommands(&qk,1,&ordinary));T(mock_submits==2&&!queue->call_queue_active&&queue->call_queue_state);
 T(nn_command_state_binding(queue->call_queue_state,NN_COMMAND_UBO,0,0)->offset==8192);
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,ordinary));
 struct nn_command_state initial=*queue->call_queue_state,final={0};struct handle *projected=NULL,*first=handles;
 while(first&&first->id!=selected[0])first=first->next;
 T(first);
 T(!command_project_sequence(queue,first,&initial,&final,&projected));
 T(projected&&projected->draws&&projected->draws!=original);
 T(projected->draws->pipeline.uniforms[0][0].pool==find(pools+DATA,POOL));
 T(projected->draws->pipeline.uniforms[0][0].offset==8192&&projected->draws->pipeline.vertices[7].offset==4096);
 T(projected->draws->pipeline.native.draw.first==4&&projected->draws->pipeline.native.raster.window_origin==0);
 T(!(final.defined&NN_COMMAND_STATE_BLEND_COLOR)); /* inactive normalization never invents a GPU write */
 command_body_destroy(projected);
 OK(api_nvnQueueSubmitCommands(&qk,2,selected));T(mock_submits==4&&device->retired&&queue->call_queue_active);
 T(queue->call_queue_state&&nn_command_state_binding(queue->call_queue_state,NN_COMMAND_UBO,0,0)->offset==8192);
 /* The next top-level handle inherits the prior enqueue's effective state. */
 OK(api_nvnQueueSubmitCommands(&qk,1,selected+1));T(mock_submits==5);
 unsigned owners=0;for(struct job *j=device->retired;j;j=j->next)if(j->pipeline_owner)owners++;T(owners==4);
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,selected[0]));OK(api_nvnDeviceFinalizeCommandHandle(&dk,selected[1]));
 OK(api_nvnCommandBufferFinalize(&parent));ERR(api_nvnProgramFinalize(&pk),EBUSY);
 OK(api_nvnQueueFinish(&qk));T(!device->retired);OK(api_nvnProgramFinalize(&pk));OK(api_nvnTextureFinalize(&tk));OK(api_nvnQueueFinalize(&qk));
 for(unsigned i=2;i<POOLS;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}
 T(!device->gpu->bos);OK(api_nvnDeviceFinalize(&dk));
 while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
 T(!handles);printf("HOST ONLY First Call after ordinary source=%u inherited_UBO=8192 selected_handles=2 mock_submits=5 GPU execution=false\n",source);return 0;
}
int main(void){T(!one(0));T(!one(1));T(!call_one(0));T(!call_one(1));T(!call_after_ordinary(0));T(!call_after_ordinary(1));return 0;}
