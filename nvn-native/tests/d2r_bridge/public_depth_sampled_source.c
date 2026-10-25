/* HOST ONLY actual typed NVN record/enqueue/retire. No GPU execution/pixels. */
#define main source_identity_fixture
#include NN_SOURCE_FIXTURE
#undef main
#include NN_PIPELINE_NATIVE_VIEWS
#include NN_PIPELINE_TEXTURE_BRIDGE
#include NN_BLOCK_IMAGE
static int sampled_format,metadata_flags,depth_format;
static unsigned source_levels,view_base,view_levels;

static void references(void){
 struct nn_pipeline_resource_reference r;
 for(unsigned i=0;i<32;i++){
  T(!nn_pipeline_resource_reference((0x168+8*i)/4,-1,&r)&&r.texture_domain==1&&r.texture_selector==i&&!r.has_sampler);
  T(!nn_pipeline_resource_reference((0x20+8*i)/4,-1,&r)&&r.texture_domain==0&&r.texture_selector==i&&r.has_sampler);
  for(unsigned j=0;j<32;j++){
   int32_t h=(int32_t)(((0x168+8*i)/4)|(((0x568+8*j)/4)<<14)|(1u<<28));
   T(!nn_pipeline_resource_reference(h,2|(3<<16),&r)&&r.texture_domain==1&&r.texture_selector==i&&r.sampler_domain==2&&r.sampler_selector==j&&r.has_sampler);
  }
 }
 T(nn_pipeline_resource_reference(110,1,&r)==ENOTSUP&&!r.texture_word);
 T(nn_pipeline_resource_reference(110,-2,&r)==ENOTSUP);
 T(nn_pipeline_resource_reference(-1,-1,&r)==ENOTSUP);
 T(nn_pipeline_resource_reference(110|(1<<28),-1,&r)==ENOTSUP);
 for(unsigned type=2;type<=7;type++)T(nn_pipeline_resource_reference((int32_t)((type<<28)|110),-1,&r)==ENOTSUP);
 T(nn_pipeline_resource_reference(111,-1,&r)==ENOTSUP);
 T(nn_pipeline_resource_reference(110,-1,NULL)==EINVAL);
}
static void pool_create(NVNdevice *device,NVNmemoryPoolBuilder *builder_key,NVNmemoryPool *pool,void **storage,size_t bytes,int flags)
{
 T(!posix_memalign(storage,4096,bytes));memset(*storage,0,bytes);
 OK(api_nvnMemoryPoolBuilderSetDefaults(builder_key));OK(api_nvnMemoryPoolBuilderSetDevice(builder_key,device));
 OK(api_nvnMemoryPoolBuilderSetStorage(builder_key,*storage,bytes));
 OK(api_nvnMemoryPoolBuilderSetFlags(builder_key,flags));
 T(api_nvnMemoryPoolInitialize(pool,builder_key));
}

#include "pipeline_enqueue_fault_checks.h"
static void materialize_faults(struct object *device,const struct pipeline_prepared_draw *p)
{
 unsigned baseline=device->gpu->bos,start=mock_allocations;size_t dc=device->refs;
 nn_bo *refs[512],*bo=NULL;struct job *owned=NULL;size_t bytes=0,cursor=0;
 T(!pipeline_materialize(device,p,&bo,&bytes,&owned,refs,&cursor,512));T(bo&&bytes&&owned);
 unsigned allocations=mock_allocations-start;T(allocations>=12);T(!nn_bo_destroy(bo));
 fail_bo_destroy=bo_destroys+1;retire_jobs(device,owned);
 T(nn_nvn_error()==EIO&&device->retired);nn_nvn_clear_error();fail_bo_destroy=0;reap_jobs(device);
 T(!device->retired&&device->gpu->bos==baseline&&device->refs==dc);
 for(unsigned fail=1;fail<=allocations;fail++){
  cursor=bytes=0;bo=NULL;owned=NULL;fail_bo_alloc=mock_allocations+fail;fail_bo_destroy=bo_destroys+1;
  int e=pipeline_materialize(device,p,&bo,&bytes,&owned,refs,&cursor,512);
  fail_bo_alloc=fail_bo_destroy=0;T(e&&(!bo||!bytes));
  if(bo)T(!nn_bo_destroy(bo));
  if(owned)retire_jobs(device,owned);
  T(!nn_nvn_error()&&!device->retired&&device->gpu->bos==baseline&&device->refs==dc);
 }
}
static void explicit_render_defaults(NVNcommandBuffer *cmd)
{
 NVNcolorState color;NVNchannelMaskState mask;NVNpolygonState polygon;NVNdepthStencilState depth;NVNmultisampleState ms;
 OK(api_nvnColorStateSetDefaults(&color));OK(api_nvnChannelMaskStateSetDefaults(&mask));OK(api_nvnPolygonStateSetDefaults(&polygon));OK(api_nvnDepthStencilStateSetDefaults(&depth));OK(api_nvnMultisampleStateSetDefaults(&ms));
 OK(api_nvnCommandBufferBindColorState(cmd,&color));OK(api_nvnCommandBufferBindChannelMaskState(cmd,&mask));OK(api_nvnCommandBufferBindPolygonState(cmd,&polygon));OK(api_nvnCommandBufferBindDepthStencilState(cmd,&depth));OK(api_nvnCommandBufferBindMultisampleState(cmd,&ms));OK(api_nvnCommandBufferSetDepthBounds(cmd,0,0,1));
}
static int one(unsigned source)
{
 mock_submits=0;mock_submit_error=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *device=find(&dk,DEVICE);
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_typed_pipeline_archive(&dk,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,reader,ranges));
 T(!nn_nvn_register_pipeline_depth_archive(&dk,&NN_GRAPHICS_PIPELINE_DEPTH_ARCHIVE));
 T(!nn_nvn_register_block_copy(&dk,&NVN_COMPILED_IMAGE));
 T(!nn_nvn_register_texture_bridge(&dk,&NN_TEXTURE_BRIDGE_BUNDLE));
 T(nn_nvn_register_pipeline_views(&dk,NULL,1)==EINVAL);T(!nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,1));
 T(nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,1)==EEXIST);
 /* Fixture chooses full original source groups only; runtime specialization
  * is selected exclusively from the real enqueue entries below. */
 /* This qualification explicitly supplies a normal checked pair index. The
  * production source resolver still matches full original code/control and
  * answers from actual registered resources; it has no first-pair fallback. */
 T(NN_SAMPLED_PAIR<archive->pair_count);
 const struct nn_pipeline_pair_recipe *selected=archive->pairs+NN_SAMPLED_PAIR;
 const struct nn_offline_graphics_stage *stages[]={archive->stages->stages+selected->vs_index,archive->stages->stages+selected->fs_index};
 T(selected->recipe->texture_mask==2&&selected->recipe->sampler_mask==2);
 enum{VS,FS,ARENA,DATA,RT,UNUSED,META,SAMPLE0,SAMPLE1,SAMPLE_ALT,DEPTH_POOL,POOLS};NVNmemoryPoolBuilder pb[POOLS];NVNmemoryPool pools[POOLS];void *storage[POOLS];memset(pb,0,sizeof(pb));memset(pools,0,sizeof(pools));
 for(unsigned i=0;i<POOLS;i++)pool_create(&dk,pb+i,pools+i,storage+i,(i==RT||i==SAMPLE0||i==SAMPLE1||i==SAMPLE_ALT||i==DEPTH_POOL)?1048576:i==META?32768:16384,i==META?metadata_flags:i==UNUSED?0x14:0x24);
 NVNshaderData shaders[2];unsigned char controls[2][0x898];range_count=0;allow(shaders,sizeof(shaders));
 for(unsigned i=0;i<2;i++){memcpy(storage[i],stages[i]->source,stages[i]->source_bytes);memcpy(controls[i],stages[i]->control,stages[i]->control_bytes);allow(storage[i],16384);allow(controls[i],sizeof(controls[i]));shaders[i]=(NVNshaderData){api_nvnMemoryPoolGetBufferAddress(pools+i),controls[i]};}
 NVNprogram pk={0};PFNNVNPROGRAMINITIALIZEPROC program_init=(PFNNVNPROGRAMINITIALIZEPROC)nvnBootstrapLoader("nvnProgramInitialize");
 PFNNVNPROGRAMSETSHADERSPROC program_set=(PFNNVNPROGRAMSETSHADERSPROC)nvnBootstrapLoader("nvnProgramSetShaders");T(program_init(&pk,&dk));T(program_set(&pk,2,shaders));
 for(unsigned i=0;i<2;i++){memset(storage[i],0xcc,16384);OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);storage[i]=NULL;}
 NVNtextureBuilder tb={0};NVNtexture tk={0};OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnTextureBuilderSetDevice(&tb,&dk));
 OK(api_nvnTextureBuilderSetFlags(&tb,source?0:NVN_TEXTURE_FLAGS_LINEAR));OK(api_nvnTextureBuilderSetSize2D(&tb,64,64));OK(api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetStride(&tb,source?0:256));OK(api_nvnTextureBuilderSetStorage(&tb,pools+RT,4096));T(api_nvnTextureInitialize(&tk,&tb));
 NVNtextureBuilder depth_builder={0};NVNtexture depth_texture={0};
 OK(api_nvnTextureBuilderSetDefaults(&depth_builder));OK(api_nvnTextureBuilderSetDevice(&depth_builder,&dk));
 OK(api_nvnTextureBuilderSetTarget(&depth_builder,NVN_TEXTURE_TARGET_2D_ARRAY));
 OK(api_nvnTextureBuilderSetSize3D(&depth_builder,128,128,3));OK(api_nvnTextureBuilderSetLevels(&depth_builder,2));
 OK(api_nvnTextureBuilderSetFormat(&depth_builder,(NVNformat)depth_format));
 OK(api_nvnTextureBuilderSetStorage(&depth_builder,pools+DEPTH_POOL,4096));T(api_nvnTextureInitialize(&depth_texture,&depth_builder));
 NVNtextureView depth_view={0};OK(api_nvnTextureViewSetDefaults(&depth_view));OK(api_nvnTextureViewSetLevels(&depth_view,1,1));OK(api_nvnTextureViewSetLayers(&depth_view,2,1));
 NVNcommandBuffer ck={0};_Alignas(16)unsigned char control[1024];T(api_nvnCommandBufferInitialize(&ck,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&ck,pools+ARENA,0,16384));OK(api_nvnCommandBufferAddControlMemory(&ck,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&ck));
 explicit_render_defaults(&ck);
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
 const NVNtexture *targets[]={&tk};OK(api_nvnCommandBufferSetRenderTargets(&ck,1,targets,NULL,&depth_texture,&depth_view));
 OK(api_nvnCommandBufferSetViewport(&ck,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&ck,0,0,64,64));OK(api_nvnCommandBufferSetDepthRange(&ck,0,1));

 NVNsamplerPool sampler_pool={0};T(api_nvnSamplerPoolInitialize(&sampler_pool,pools+META,16384,320));
 OK(api_nvnCommandBufferSetSamplerPool(&ck,&sampler_pool));
 OK(api_nvnCommandBufferBindSeparateSampler(&ck,NVN_SHADER_STAGE_FRAGMENT,0,api_nvnDeviceGetSeparateSamplerHandle(&dk,259)));
 OK(api_nvnCommandBufferBindSeparateSampler(&ck,NVN_SHADER_STAGE_FRAGMENT,1,api_nvnDeviceGetSeparateSamplerHandle(&dk,260)));
 NVNtexturePool texture_pool={0};T(api_nvnTexturePoolInitialize(&texture_pool,pools+META,0,320));
 OK(api_nvnCommandBufferSetTexturePool(&ck,&texture_pool));
 OK(api_nvnCommandBufferBindTexture(&ck,NVN_SHADER_STAGE_FRAGMENT,10,api_nvnDeviceGetTextureHandle(&dk,257,259)));
 NVNseparateTextureHandle handles2[2]={api_nvnDeviceGetSeparateTextureHandle(&dk,257),api_nvnDeviceGetSeparateTextureHandle(&dk,258)};T(!nn_nvn_error());
 OK(api_nvnCommandBufferBindSeparateTextures(&ck,NVN_SHADER_STAGE_FRAGMENT,10,2,handles2));
 memset(handles2,0xcc,sizeof(handles2));
 struct recording *r=find(&ck,COMMAND)->record;
 NVNqueueBuilder qb={0};NVNqueue qk={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&qk,&qb));
 size_t clear_before=find(&ck,COMMAND)->record->used;
 ERR(api_nvnCommandBufferClearDepthStencil(&ck,.25f,1,0,255),ENOTSUP);T(find(&ck,COMMAND)->record->used==clear_before);
 OK(api_nvnCommandBufferClearDepthStencil(&ck,.25f,1,0,0));
 T(find(&ck,COMMAND)->record->current->draws&&find(&ck,COMMAND)->record->current->draws->depth_clear);
 size_t before_alloc=mock_allocations,before_cache=mock_caches;
 OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
 struct late_draw *recorded=r->current->draws;T(recorded&&recorded->generic_pipeline&&!recorded->pipeline.recipe);
 T(recorded->pipeline.owner&&recorded->pipeline.owner->references==1);
 T(recorded->pipeline.uniforms[0][0].offset==8192);
 T(mock_allocations==before_alloc&&mock_caches==before_cache);
 NVNcommandHandle handle=api_nvnCommandBufferEndRecording(&ck);T(handle&&!nn_nvn_error());
 /* Entire batch rejects missing registration before the first GPU alloc. */
 struct pipeline_batch missing={0};T(pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&missing)==ENOTSUP);
 pipeline_batch_discard(&missing);T(mock_allocations==before_alloc&&mock_caches==before_cache);
 NVNsamplerBuilder sb={0};OK(api_nvnSamplerBuilderSetDefaults(&sb));OK(api_nvnSamplerBuilderSetDevice(&sb,&dk));
 OK(api_nvnSamplerBuilderSetMinMagFilter(&sb,NVN_MIN_FILTER_NEAREST,NVN_MAG_FILTER_NEAREST));
 OK(api_nvnSamplerBuilderSetWrapMode(&sb,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE));
 NVNsampler sampler={0};T(api_nvnSamplerInitialize(&sampler,&sb));
 OK(api_nvnSamplerPoolRegisterSampler(&sampler_pool,259,&sampler));OK(api_nvnSamplerFinalize(&sampler));remove_object(find(&sb,SB));
 /* Both slots are registered AFTER recording, then public texture objects
  * are finalized. The original pool backings and entry snapshots remain. */
 NVNtextureBuilder sampled_builder[2]={0};NVNtexture sampled[2]={0};
 for(unsigned i=0;i<2;i++){
  OK(api_nvnTextureBuilderSetDefaults(sampled_builder+i));OK(api_nvnTextureBuilderSetDevice(sampled_builder+i,&dk));
  OK(api_nvnTextureBuilderSetFlags(sampled_builder+i,source?0:NVN_TEXTURE_FLAGS_LINEAR));
  OK(api_nvnTextureBuilderSetSize2D(sampled_builder+i,64,64));OK(api_nvnTextureBuilderSetFormat(sampled_builder+i,(NVNformat)sampled_format));
  OK(api_nvnTextureBuilderSetLevels(sampled_builder+i,source_levels));OK(api_nvnTextureBuilderSetStride(sampled_builder+i,source?0:256));OK(api_nvnTextureBuilderSetStorage(sampled_builder+i,pools+SAMPLE0+i,4096));
  OK(api_nvnTextureBuilderSetSwizzle(sampled_builder+i,NVN_TEXTURE_SWIZZLE_R,NVN_TEXTURE_SWIZZLE_ZERO,NVN_TEXTURE_SWIZZLE_ZERO,NVN_TEXTURE_SWIZZLE_ONE));
  T(api_nvnTextureInitialize(sampled+i,sampled_builder+i));
  NVNtextureView view={0};OK(api_nvnTextureViewSetDefaults(&view));OK(api_nvnTextureViewSetLevels(&view,view_base,view_levels));
  OK(api_nvnTexturePoolRegisterTexture(&texture_pool,257+i,sampled+i,&view));
  OK(api_nvnTextureFinalize(sampled+i));
 }
 before_alloc=mock_allocations;before_cache=mock_caches;
 struct pipeline_batch batch={0};T(!pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&batch));
 const struct pipeline_prepared_draw *prepared=pipeline_batch_find(&batch,recorded,0);T(prepared&&prepared->draw.pipeline.recipe);
 T(prepared->draw.pipeline.native.ubo[0][stages[0]->cbufs[0].binding].offset==8192);
 T(prepared->draw.pipeline.recipe->texture_mask==2&&prepared->draw.pipeline.recipe->sampler_mask==2);
 for(unsigned i=1;i<=1;i++){
  T(prepared->native_entries[i]&&prepared->native_entries[i]->pool==find(pools+SAMPLE0+i-1,POOL));
  T(prepared->native_entries[i]->offset==4096&&prepared->native_entries[i]->snapshot.texture.target==NVN_TEXTURE_TARGET_2D);
 }
 T(mock_allocations==before_alloc&&mock_caches==before_cache);
 T(prepared->depth_export&&!prepared->depth_export->writes_depth&&prepared->depth_export->depth_regid==252);
 T(prepared->depth_plan.aliases.tiled.width==64&&prepared->depth_plan.aliases.tiled.height==64);
 T(prepared->draw.depth==find(&depth_texture,TEXTURE)&&prepared->draw.depth_view.level==1&&prepared->draw.depth_view.layer==2);
 T(prepared->depth_images==2&&prepared->draw.pipeline.native.sampled_depth[1]->desc.format==(unsigned)sampled_format);
 T(prepared->sampled_depth_plan[1].descriptor.levels==source_levels&&prepared->sampled_depth_plan[1].descriptor.base_level==view_base&&view_levels==1);

 /* Generic frontend packed reference, actual registered sampler, no expected
  * query answer. Normalization remains unsupported without independent proof. */
 struct nn_pipeline_resource_reference ref;size_t ti;struct nn_sampler_state sampled_state;
 T(!pipeline_capture_reference((struct pipeline_prepared_draw*)prepared,1,110|(346<<14)|(1<<28),2|(3<<16),&ref,&ti));
 unsigned char *sampler_raw=(unsigned char*)storage[META]+16384+259*DESCRIPTOR_BYTES;
 unsigned char slot_first=*sampler_raw;*sampler_raw^=1;
 T(!pipeline_capture_sampler((struct pipeline_prepared_draw*)prepared,1,&ref,ti,&sampled_state));
 struct pipeline_batch tampered_sampler={0};unsigned audit_alloc=mock_allocations,audit_cache=mock_caches;
 T(pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&tampered_sampler)==EINVAL);pipeline_batch_discard(&tampered_sampler);
 T(mock_allocations==audit_alloc&&mock_caches==audit_cache);*sampler_raw=slot_first;
 struct nn_pipeline_resource_reference absent;size_t at;
 T(!pipeline_capture_reference((struct pipeline_prepared_draw*)prepared,1,110|(348<<14)|(1<<28),2|(3<<16),&absent,&at));
 T(pipeline_capture_sampler((struct pipeline_prepared_draw*)prepared,1,&absent,at,&sampled_state)==ENOTSUP);
 T(!pipeline_capture_sampler((struct pipeline_prepared_draw*)prepared,1,&ref,ti,&sampled_state));
 T(sampled_state.min_filter==NVN_MIN_FILTER_NEAREST&&sampled_state.wrap[0]==NVN_WRAP_MODE_CLAMP_TO_EDGE);
 struct nn_pipeline_resource_reference combined;size_t ct;
 T(!pipeline_capture_reference((struct pipeline_prepared_draw*)prepared,1,(0x20+10*8)/4,-1,&combined,&ct));
 T(combined.texture_domain==0&&ct!=ti);
 struct nn_pipeline_resource_queries wrong_domain=prepared->queries.resources;
 wrong_domain.bindings=prepared->bindings+ct;wrong_domain.binding_count=1;struct nn_pipeline_resolved_texture missing_texture;
 T(nn_pipeline_resource_texture(&wrong_domain,1,110,-1,&missing_texture)==ENOTSUP);
 struct nn_sampler_state same;T(!pipeline_capture_sampler((struct pipeline_prepared_draw*)prepared,1,&combined,ct,&same));T(!memcmp(&same,&sampled_state,sizeof(same)));
 /* Actual Register of the same ID is late-bound on the next enqueue, but
  * cannot mutate this prepared value or force a second table lookup. */
 NVNsamplerBuilder changed={0};OK(api_nvnSamplerBuilderSetDefaults(&changed));OK(api_nvnSamplerBuilderSetDevice(&changed,&dk));
 OK(api_nvnSamplerBuilderSetCompare(&changed,NVN_COMPARE_MODE_COMPARE_R_TO_TEXTURE,NVN_COMPARE_FUNC_LESS));
 OK(api_nvnSamplerPoolRegisterSamplerBuilder(&sampler_pool,259,&changed));remove_object(find(&changed,SB));
 T(!pipeline_capture_sampler((struct pipeline_prepared_draw*)prepared,1,&ref,ti,&same));T(!memcmp(&same,&sampled_state,sizeof(same)));
 T(!pipeline_capture_sampler((struct pipeline_prepared_draw*)prepared,1,&combined,ct,&same));T(!memcmp(&same,&sampled_state,sizeof(same)));
 ERR(api_nvnSamplerPoolFinalize(&sampler_pool),EBUSY);
 struct pipeline_batch unsupported_sampler={0};unsigned ba=mock_allocations,bc=mock_caches,bs=mock_submits;
 T(pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&unsupported_sampler)==ENOTSUP);pipeline_batch_discard(&unsupported_sampler);
 T(mock_allocations==ba&&mock_caches==bc&&mock_submits==bs);
 NVNsamplerBuilder restored={0};OK(api_nvnSamplerBuilderSetDefaults(&restored));OK(api_nvnSamplerBuilderSetDevice(&restored,&dk));
 OK(api_nvnSamplerBuilderSetMinMagFilter(&restored,NVN_MIN_FILTER_NEAREST,NVN_MAG_FILTER_NEAREST));
 OK(api_nvnSamplerBuilderSetWrapMode(&restored,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE));
 OK(api_nvnSamplerPoolRegisterSamplerBuilder(&sampler_pool,259,&restored));remove_object(find(&restored,SB));
 struct nn_pipeline_query nq={.name="QueryTextureCoordNormalized",.arguments=3,.args={110|(346<<14)|(1<<28),2|(3<<16)},.type=NN_PIPELINE_QUERY_BOOL,.count=1};
 uint32_t values[32];size_t vc=99;T(!pipeline_actual_query((void*)prepared,1,&nq,values,&vc)&&vc==1&&values[0]==1);
 /* Same query identity, resource target differs: no expected-answer fallback. */
 struct nn_pipeline_resource_binding target_row=prepared->bindings[ti];struct nn_pipeline_resource_queries target_state=prepared->queries.resources;
 target_state.bindings=&target_row;target_state.binding_count=1;target_row.texture.target=NVN_TEXTURE_TARGET_RECTANGLE;
 vc=99;T(nn_pipeline_resource_query(&target_state,1,&nq,values,&vc)==ENOTSUP&&!vc);
 target_row.texture.target=0x7fffffffu;vc=99;T(nn_pipeline_resource_query(&target_state,1,&nq,values,&vc)==ENOTSUP&&!vc);
 target_row.texture.target=NVN_TEXTURE_TARGET_2D;T(!nn_pipeline_resource_query(&target_state,1,&nq,values,&vc)&&vc==1&&values[0]==1);
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
 /* Ordinary shader + attachment owns a real writes0 sidecar, including its
  * sampled helper. Corrupt roles reject before allocating a Program BO. */
 const struct pipeline_draw_snapshot *role=&prepared->draw.pipeline;
 struct nn_pipeline_depth_export invalid=*prepared->depth_export;
 nn_pipeline_program *rp=NULL,*retained=NULL;unsigned role_alloc=mock_allocations;
 invalid.recipe=archive->pairs[2].recipe;
 T(nn_pipeline_program_create_typed_depth(device->gpu,archive->stages,role->stages[0],role->stages[1],role->native.texture_types,&invalid,&rp,&retained)==EINVAL&&!rp&&!retained&&mock_allocations==role_alloc);
 invalid=*prepared->depth_export;invalid.version++;
 T(nn_pipeline_program_create_typed_depth(device->gpu,archive->stages,role->stages[0],role->stages[1],role->native.texture_types,&invalid,&rp,&retained)&&!rp&&!retained&&mock_allocations==role_alloc);
 invalid=*prepared->depth_export;
 T(!nn_pipeline_program_create_typed_depth(device->gpu,archive->stages,role->stages[0],role->stages[1],role->native.texture_types,&invalid,&rp,&retained)&&rp&&!retained);
 uint32_t semantic=0;T(!nn_sampled_image_program(rp,role->recipe,1,&semantic)&&semantic);
 invalid.depth_regid=14;T(nn_sampled_image_program(rp,role->recipe,1,&semantic)&&!semantic);
 invalid=*prepared->depth_export;T(!nn_sampled_image_program(rp,role->recipe,1,&semantic));T(!nn_pipeline_program_destroy(rp));
 /* Consumed UBO physically overlapping canonical depth rejects the full
  * batch before any code/shadow/cache/submit, independently of native BOs. */
 struct bound saved_ubo=recorded->pipeline.uniforms[0][0];struct object*zpool=find(pools+DEPTH_POOL,POOL);
 recorded->pipeline.uniforms[0][0]=(struct bound){zpool,4096,64,nn_bo_address(zpool->bo)+4096};
 struct pipeline_batch overlap={0};unsigned oa=mock_allocations,oc=mock_caches,os=mock_submits;
 T(pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&overlap)!=0);pipeline_batch_discard(&overlap);
 T(mock_allocations==oa&&mock_caches==oc&&mock_submits==os);recorded->pipeline.uniforms[0][0]=saved_ubo;
 struct nn_depth_fdl_view invalid_view;T(!nn_depth_fdl(&prepared->depth_plan.attachment,&invalid_view));
 unsigned pa=mock_allocations,pc=mock_caches;
 invalid_view.desc.address=64;
 T(nn_pipeline_preflight_depth_planned(device->gpu,role->stages,prepared->depth_export,archive->platform,&role->native,prepared->planned_images,&invalid_view,&prepared->depth_controls)!=0);
 T(!nn_depth_fdl(&prepared->depth_plan.attachment,&invalid_view));invalid_view.rb_depth_buffer_pitch^=64;
 T(nn_pipeline_preflight_depth_planned(device->gpu,role->stages,prepared->depth_export,archive->platform,&role->native,prepared->planned_images,&invalid_view,&prepared->depth_controls)!=0);
 T(mock_allocations==pa&&mock_caches==pc);
 materialize_faults(device,prepared);
 T(recorded->next&&recorded->next->depth_clear);
 const struct pipeline_prepared_draw *clear_prepared=pipeline_batch_find(&batch,recorded->next,0);T(clear_prepared&&clear_prepared->reference_capacity==13);
 materialize_faults(device,clear_prepared);
 /* Public Bind and immutable Draw snapshots: an equal combined selector or
  * another separate selector cannot replace the needed separate T10/S0.
  * Restore current recording state AFTER Draw, before End, to catch a latest-
  * state lookup bug. The second bad handle rejects the entire batch. */
 for(unsigned missing=0;missing<2;missing++){
  OK(api_nvnCommandBufferBeginRecording(&ck));
  OK(api_nvnCommandBufferBindSeparateTexture(&ck,NVN_SHADER_STAGE_FRAGMENT,10,api_nvnDeviceGetSeparateTextureHandle(&dk,missing?257:261)));
  OK(api_nvnCommandBufferBindSeparateSampler(&ck,NVN_SHADER_STAGE_FRAGMENT,0,api_nvnDeviceGetSeparateSamplerHandle(&dk,missing?260:259)));
  OK(api_nvnCommandBufferBindSeparateTexture(&ck,NVN_SHADER_STAGE_FRAGMENT,11,api_nvnDeviceGetSeparateTextureHandle(&dk,257)));
  OK(api_nvnCommandBufferBindSeparateSampler(&ck,NVN_SHADER_STAGE_FRAGMENT,1,api_nvnDeviceGetSeparateSamplerHandle(&dk,259)));
  OK(api_nvnCommandBufferDrawArrays(&ck,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
  OK(api_nvnCommandBufferBindSeparateTexture(&ck,NVN_SHADER_STAGE_FRAGMENT,10,api_nvnDeviceGetSeparateTextureHandle(&dk,257)));
  OK(api_nvnCommandBufferBindSeparateSampler(&ck,NVN_SHADER_STAGE_FRAGMENT,0,api_nvnDeviceGetSeparateSamplerHandle(&dk,259)));
  NVNcommandHandle missing_handle=api_nvnCommandBufferEndRecording(&ck);T(missing_handle&&!nn_nvn_error());
  NVNcommandHandle both[]={handle,missing_handle};unsigned na=mock_allocations,nc=mock_caches,ns=mock_submits;
  ERR(api_nvnQueueSubmitCommands(&qk,2,both),ENOTSUP);
  T(mock_allocations==na&&mock_caches==nc&&mock_submits==ns);OK(api_nvnDeviceFinalizeCommandHandle(&dk,missing_handle));
 }
 /* Bad published bytes and registered-but-unconsumable resource types must
  * reject the complete two-handle batch before any native resource work. */
 unsigned char *raw_slot=(unsigned char *)storage[META]+257*DESCRIPTOR_BYTES;
 unsigned char saved_slot[DESCRIPTOR_BYTES];memcpy(saved_slot,raw_slot,sizeof(saved_slot));raw_slot[0]^=1;
 unsigned reject_alloc=mock_allocations,reject_cache=mock_caches,reject_submit=mock_submits;
 NVNcommandHandle repeated[2]={handle,handle};
 ERR(api_nvnQueueSubmitCommands(&qk,2,repeated),EINVAL);memcpy(raw_slot,saved_slot,sizeof(saved_slot));
 T(mock_allocations==reject_alloc&&mock_caches==reject_cache&&mock_submits==reject_submit);
 for(unsigned buffer=0;buffer<2;buffer++){
  NVNtextureBuilder unsupported_builder={0};NVNtexture unsupported_texture={0};
  OK(api_nvnTextureBuilderSetDefaults(&unsupported_builder));OK(api_nvnTextureBuilderSetDevice(&unsupported_builder,&dk));
  OK(api_nvnTextureBuilderSetTarget(&unsupported_builder,buffer?NVN_TEXTURE_TARGET_BUFFER:NVN_TEXTURE_TARGET_3D));
  if(buffer){OK(api_nvnTextureBuilderSetSize1D(&unsupported_builder,64));}
  else{OK(api_nvnTextureBuilderSetSize3D(&unsupported_builder,32,32,32));}
  OK(api_nvnTextureBuilderSetFormat(&unsupported_builder,buffer?NVN_FORMAT_RGBA8:NVN_FORMAT_RGBA_DXT1));
  OK(api_nvnTextureBuilderSetStorage(&unsupported_builder,pools+SAMPLE_ALT,4096));
  T(api_nvnTextureInitialize(&unsupported_texture,&unsupported_builder));
  OK(api_nvnTexturePoolRegisterTexture(&texture_pool,257,&unsupported_texture,NULL));OK(api_nvnTextureFinalize(&unsupported_texture));
  reject_alloc=mock_allocations;reject_cache=mock_caches;reject_submit=mock_submits;
  ERR(api_nvnQueueSubmitCommands(&qk,2,repeated),ENOTSUP);
  T(mock_allocations==reject_alloc&&mock_caches==reject_cache&&mock_submits==reject_submit);
 }
 /* Actual public re-registration without explicit component definition must
  * not silently infer a default depth tuple for SAMPLE. Old prepare is stable. */
 NVNtextureBuilder implicit_builder={0};NVNtexture implicit_texture={0};
 OK(api_nvnTextureBuilderSetDefaults(&implicit_builder));OK(api_nvnTextureBuilderSetDevice(&implicit_builder,&dk));
 OK(api_nvnTextureBuilderSetSize2D(&implicit_builder,64,64));OK(api_nvnTextureBuilderSetFormat(&implicit_builder,(NVNformat)sampled_format));
 OK(api_nvnTextureBuilderSetStorage(&implicit_builder,pools+SAMPLE_ALT,4096));T(api_nvnTextureInitialize(&implicit_texture,&implicit_builder));
 OK(api_nvnTexturePoolRegisterTexture(&texture_pool,257,&implicit_texture,NULL));OK(api_nvnTextureFinalize(&implicit_texture));
 reject_alloc=mock_allocations;reject_cache=mock_caches;reject_submit=mock_submits;
 ERR(api_nvnQueueSubmitCommands(&qk,2,repeated),ENOTSUP);T(mock_allocations==reject_alloc&&mock_caches==reject_cache&&mock_submits==reject_submit);
 /* Re-registration frees the original slot layout. The existing prepared
  * entry must remain a deep value/pinned original backing; next prepare sees
  * the new version from the SAME recorded selector. */
 NVNtextureBuilder replacement_builder={0};NVNtexture replacement={0};
 OK(api_nvnTextureBuilderSetDefaults(&replacement_builder));OK(api_nvnTextureBuilderSetDevice(&replacement_builder,&dk));
 OK(api_nvnTextureBuilderSetFlags(&replacement_builder,source?0:NVN_TEXTURE_FLAGS_LINEAR));
 OK(api_nvnTextureBuilderSetSize2D(&replacement_builder,64,64));
 OK(api_nvnTextureBuilderSetFormat(&replacement_builder,(NVNformat)sampled_format));
 OK(api_nvnTextureBuilderSetLevels(&replacement_builder,source_levels));OK(api_nvnTextureBuilderSetStride(&replacement_builder,source?0:256));
 OK(api_nvnTextureBuilderSetStorage(&replacement_builder,pools+SAMPLE_ALT,4096));
 OK(api_nvnTextureBuilderSetSwizzle(&replacement_builder,NVN_TEXTURE_SWIZZLE_R,NVN_TEXTURE_SWIZZLE_ZERO,NVN_TEXTURE_SWIZZLE_ZERO,NVN_TEXTURE_SWIZZLE_ONE));T(api_nvnTextureInitialize(&replacement,&replacement_builder));
 NVNtextureView replacement_view={0};OK(api_nvnTextureViewSetDefaults(&replacement_view));OK(api_nvnTextureViewSetLevels(&replacement_view,view_base,view_levels));
 OK(api_nvnTexturePoolRegisterTexture(&texture_pool,257,&replacement,&replacement_view));OK(api_nvnTextureFinalize(&replacement));
 T(prepared->native_entries[1]->pool==find(pools+SAMPLE0,POOL)&&prepared->native_entries[1]->snapshot.texture.width==64);
 if(source)T(prepared->native_entries[1]->source_layout&&prepared->native_entries[1]->source_layout->level[0].width==64);
 ERR(api_nvnMemoryPoolFinalize(pools+SAMPLE0),EBUSY);
 struct pipeline_batch updated={0};T(!pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&updated));
 const struct pipeline_prepared_draw *newer=pipeline_batch_find(&updated,recorded,0);
 T(newer&&newer->native_entries[1]->pool==find(pools+SAMPLE_ALT,POOL)&&newer->native_entries[1]->snapshot.texture.width==64);
 T(newer->draw.pipeline.native.sampled_depth[1]->view_width==(64u>>view_base));
 pipeline_batch_discard(&updated);pipeline_batch_discard(&batch);
 OK(api_nvnMemoryPoolFinalize(pools+SAMPLE0));free(storage[SAMPLE0]);storage[SAMPLE0]=NULL;
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
 nn_bo *unused_bo=find(pools+UNUSED,POOL)->bo,*metadata_bo=find(pools+META,POOL)->bo;for(size_t i=0;i<mock_ref_count;i++)T(mock_refs[i]!=unused_bo&&mock_refs[i]!=metadata_bo);
 handle=copied;
 /* The HOST RAM backend has64 pending slots, unlike KGSL's65535. Complete
  * this accepted replay before the separate ambiguous-submit lifetime case. */
 OK(api_nvnQueueFinish(&qk));T(!device->retired);
 /* Call expansion projects the same depth binding + selected view and depth
  * clear semantic, without GPU allocations until complete enqueue preflight. */
 OK(api_nvnCommandBufferBeginRecording(&ck));
 OK(api_nvnCommandBufferCallCommands(&ck,1,&handle));
 NVNcommandHandle called=api_nvnCommandBufferEndRecording(&ck);T(called&&!nn_nvn_error());
 OK(api_nvnQueueSubmitCommands(&qk,1,&called));T(mock_submits==2);OK(api_nvnQueueFinish(&qk));
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,called));
 /* An accepted but ambiguous mock ioctl keeps every actual BO and owner
  * until a real Finish result; handle destruction cannot declare completion. */
 mock_submit_error=EIO;ERR(api_nvnQueueSubmitCommands(&qk,1,&handle),EIO);mock_submit_error=0;
 T(mock_submits==3&&device->retired);
 /* Submitted actual owners remain pinned even if their public handle is gone. */
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,handle));T(device->retired);
 OK(api_nvnCommandBufferFinalize(&ck));
 OK(api_nvnMemoryPoolFinalize(pools+UNUSED));free(storage[UNUSED]);storage[UNUSED]=NULL;
 ERR(api_nvnProgramFinalize(&pk),EBUSY);ERR(api_nvnTextureFinalize(&tk),EBUSY);
 OK(api_nvnQueueFinish(&qk));T(!device->retired);
 OK(api_nvnTexturePoolFinalize(&texture_pool));OK(api_nvnSamplerPoolFinalize(&sampler_pool));
 OK(api_nvnProgramFinalize(&pk));OK(api_nvnTextureFinalize(&tk));OK(api_nvnTextureFinalize(&depth_texture));
 OK(api_nvnQueueFinalize(&qk));
 for(unsigned i=ARENA;i<POOLS;i++)if(storage[i]){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}
 OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
 printf("HOST ONLY original FLOAT2D sampled format=%u mips=%u view=%u+%u metadata=%x checks=%u mock_submits=%u; GPU execution=false\n",sampled_format,source_levels,view_base,view_levels,metadata_flags,checks,mock_submits);
 return 0;
}
#ifndef NN_PUBLIC_DEPTH_SAMPLED_MAIN
#define NN_PUBLIC_DEPTH_SAMPLED_MAIN main
#endif
int NN_PUBLIC_DEPTH_SAMPLED_MAIN(void)
{
 references();metadata_flags=0x12;sampled_format=NVN_FORMAT_RGBA8;source_levels=1;view_base=0;view_levels=1;
 const int formats[]={NVN_FORMAT_DEPTH16,NVN_FORMAT_DEPTH24,NVN_FORMAT_DEPTH32F};
 for(unsigned i=0;i<3;i++){depth_format=sampled_format=formats[i];one(1);}
 return 0;
}
