/* HOST ONLY actual typed NVN record/enqueue/retire. No GPU execution/pixels. */
#define main source_identity_fixture
#include NN_SOURCE_FIXTURE
#undef main
#include NN_PIPELINE_NATIVE_VIEWS
#include NN_PIPELINE_TEXTURE_BRIDGE
#include NN_BLOCK_IMAGE
static int bc_format,metadata_flags;static unsigned selected_base,selected_count;
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
 reset_maps(0);unsigned cache_start=mock_caches;
 T(!pipeline_materialize(device,p,&bo,&bytes,&owned,refs,&cursor,512));T(bo&&bytes&&owned);
 unsigned map_points=map_first_count,cache_points=mock_caches-cache_start;reset_maps(UINT_MAX);
 unsigned allocations=mock_allocations-start;T(allocations>=20);T(!nn_bo_destroy(bo));
 fail_bo_destroy=bo_destroys+1;retire_jobs(device,owned);
 T(nn_nvn_error()==EIO&&device->retired);nn_nvn_clear_error();fail_bo_destroy=0;reap_jobs(device);
 T(!device->retired&&device->gpu->bos==baseline&&device->refs==dc);
 for(unsigned kind=0;kind<2;kind++)for(unsigned fail=1;fail<=(kind?cache_points:map_points);fail++){
  cursor=bytes=0;bo=NULL;owned=NULL;reset_maps(kind?UINT_MAX:fail);
  if(kind)fail_bo_cache=mock_caches+fail;
  fail_bo_destroy=bo_destroys+1;
  int e=pipeline_materialize(device,p,&bo,&bytes,&owned,refs,&cursor,512);
  fail_bo_cache=fail_bo_destroy=0;reset_maps(UINT_MAX);T(e&&(!bo||!bytes));
  if(bo)T(!nn_bo_destroy(bo));
  if(owned)retire_jobs(device,owned);
  T(!nn_nvn_error()&&!device->retired&&device->gpu->bos==baseline&&device->refs==dc);
 }
 for(unsigned fail=1;fail<=allocations;fail++){
  cursor=bytes=0;bo=NULL;owned=NULL;fail_bo_alloc=mock_allocations+fail;fail_bo_destroy=bo_destroys+1;
  int e=pipeline_materialize(device,p,&bo,&bytes,&owned,refs,&cursor,512);
  fail_bo_alloc=fail_bo_destroy=0;T(e&&(!bo||!bytes));
  if(bo)T(!nn_bo_destroy(bo));
  if(owned)retire_jobs(device,owned);
  T(!nn_nvn_error()&&!device->retired&&device->gpu->bos==baseline&&device->refs==dc);
 }
}

static void packaged_mip_metadata(NVNdevice *device,NVNmemoryPool *pool)
{
#if NN_NVN_BNTX_PACKAGED_EXPERIMENT
 static NVNtextureBuilder b;static NVNtexture t; /* Stable opaque keys across inlining/stack reuse. */
 OK(api_nvnTextureBuilderSetDefaults(&b));OK(api_nvnTextureBuilderSetDevice(&b,device));
 OK(api_nvnTextureBuilderSetSize2D(&b,128,128));OK(api_nvnTextureBuilderSetLevels(&b,8));
 OK(api_nvnTextureBuilderSetFormat(&b,NVN_FORMAT_RGBA_DXT5));
 NVNpackagedTextureLayout bytes={{2,0,0,0,7,0,1,0}};
 OK(api_nvnTextureBuilderSetPackagedTextureLayout(&b,&bytes));
 OK(api_nvnTextureBuilderSetStorage(&b,pool,4096));
 struct nn_tex_source_desc d={.target=1,.format=0x44,.width=128,.height=128,.layers=1,.levels=8,.block_height_log2=2};
 struct nn_tex_source_layout want;T(!nn_tex_source_layout(&d,&want));
 T(api_nvnTextureBuilderGetStorageSize(&b)==want.payload_bytes);
 T(api_nvnTextureInitialize(&t,&b));T(!memcmp(find(&t,TEXTURE)->source_layout,&want,sizeof(want)));
 struct nn_bc_shadow_plan p;T(!nn_bc_shadow_plan(&want,2,6,0,1,&p));
 T(!nn_bc_sampled_validate(&p.native)&&p.count==8&&p.native.base_level==2&&p.native.level_count==6);
 OK(api_nvnTextureFinalize(&t));
 size_t refs=find(pool,POOL)->refs;unsigned allocations=mock_allocations;
 bytes.layout[7]=1;OK(api_nvnTextureBuilderSetPackagedTextureLayout(&b,&bytes));
 T(!api_nvnTextureInitialize(&t,&b)&&nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();
 T(find(pool,POOL)->refs==refs&&mock_allocations==allocations);
#else
 (void)device;(void)pool;
#endif
}

static int one(unsigned source)
{
 mock_submits=0;mock_submit_error=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *device=find(&dk,DEVICE);
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_pipeline_archive(&dk,archive,reader,ranges));
 T(!nn_nvn_register_block_copy(&dk,&NVN_COMPILED_IMAGE));
 T(!nn_nvn_register_texture_bridge(&dk,&NN_TEXTURE_BRIDGE_BUNDLE));
 T(nn_nvn_register_pipeline_views(&dk,NULL,1)==EINVAL);T(!nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT));
 T(nn_nvn_register_pipeline_views(&dk,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT)==EEXIST);
 /* Fixture chooses full original source groups only; runtime specialization
  * is selected exclusively from the real enqueue entries below. */
 /* This qualification explicitly supplies a normal checked pair index. The
  * production source resolver still matches full original code/control and
  * answers from actual registered resources; it has no first-pair fallback. */
 T(NN_SAMPLED_PAIR<archive->pair_count);
 const struct nn_pipeline_pair_recipe *selected=archive->pairs+NN_SAMPLED_PAIR;
 const struct nn_offline_graphics_stage *stages[]={archive->stages->stages+selected->vs_index,archive->stages->stages+selected->fs_index};
 T(selected->recipe->texture_mask==2&&selected->recipe->sampler_mask==2);
 enum{VS,FS,ARENA,DATA,RT,UNUSED,META,SAMPLE0,SAMPLE1,SAMPLE_ALT,POOLS};NVNmemoryPoolBuilder pb[POOLS];NVNmemoryPool pools[POOLS];void *storage[POOLS];memset(pb,0,sizeof(pb));memset(pools,0,sizeof(pools));
 for(unsigned i=0;i<POOLS;i++)pool_create(&dk,pb+i,pools+i,storage+i,(i==RT||i==META||i==SAMPLE0||i==SAMPLE1||i==SAMPLE_ALT)?32768:16384,i==META?metadata_flags:i==UNUSED?0x14:0x24);
 packaged_mip_metadata(&dk,pools+SAMPLE_ALT);
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
  OK(api_nvnTextureBuilderSetSize2D(sampled_builder+i,64,64));OK(api_nvnTextureBuilderSetLevels(sampled_builder+i,4));OK(api_nvnTextureBuilderSetFormat(sampled_builder+i,(NVNformat)bc_format));
  OK(api_nvnTextureBuilderSetStride(sampled_builder+i,source?0:256));OK(api_nvnTextureBuilderSetStorage(sampled_builder+i,pools+SAMPLE0+i,4096));
  T(api_nvnTextureInitialize(sampled+i,sampled_builder+i));
  NVNtextureView view={0};OK(api_nvnTextureViewSetDefaults(&view));OK(api_nvnTextureViewSetLevels(&view,(int)selected_base,(int)selected_count));
  OK(api_nvnTexturePoolRegisterTexture(&texture_pool,257+i,sampled+i,&view));memset(&view,0xcc,sizeof(view));
  OK(api_nvnTextureFinalize(sampled+i));
 }
 before_alloc=mock_allocations;before_cache=mock_caches;
 struct pipeline_batch batch={0};T(!pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&batch));
 const struct pipeline_prepared_draw *prepared=pipeline_batch_find(&batch,recorded,0);T(prepared&&prepared->draw.pipeline.recipe);
 T(prepared->draw.pipeline.native.ubo[0][stages[0]->cbufs[0].binding].offset==8192);
 T(prepared->draw.pipeline.recipe->texture_mask==2&&prepared->draw.pipeline.recipe->sampler_mask==2);
 T(prepared->bc_plan[1].count==4&&prepared->bc_plan[1].native.base_level==selected_base&&prepared->bc_plan[1].native.level_count==selected_count);
 T(prepared->draw.pipeline.native.sampled_fdl[1]->view_width==(64u>>selected_base));
 T(prepared->draw.pipeline.native.sampler_layouts[1].width==(64u>>selected_base)&&prepared->draw.pipeline.native.sampler_layouts[1].levels==selected_count);
 T(pipeline_bc_private_nodes(0)==0&&pipeline_bc_private_nodes(16)==0&&pipeline_bc_private_nodes(1)==4&&pipeline_bc_private_nodes(15)==33);
 T(16u*pipeline_bc_private_nodes(15)==528); /* independent of direct graphics128 */
 /* All budget rejection occurs before GPU allocation/cache, preserving pins. */
 struct pipeline_prepared_draw over=*prepared;over.reference_capacity=513;
 nn_bo *budget_refs[512],*budget_bo=NULL;struct job *budget_jobs=NULL;size_t budget_bytes=99,budget_cursor=0;
 unsigned ba0=mock_allocations,bc0=mock_caches;
 T(pipeline_materialize(device,&over,&budget_bo,&budget_bytes,&budget_jobs,budget_refs,&budget_cursor,512)==ENOSPC);
 T(!budget_bo&&!budget_jobs&&!budget_bytes&&mock_allocations==ba0&&mock_caches==bc0);
 size_t saved_device_refs=device->refs;device->refs=SIZE_MAX;
 T(pipeline_materialize(device,prepared,&budget_bo,&budget_bytes,&budget_jobs,budget_refs,&budget_cursor,512)==EOVERFLOW);
 T(!budget_bo&&!budget_jobs&&!budget_bytes&&mock_allocations==ba0&&mock_caches==bc0);
 device->refs=saved_device_refs;


 for(unsigned i=1;i<=1;i++){
  T(prepared->native_entries[i]&&prepared->native_entries[i]->pool==find(pools+SAMPLE0+i-1,POOL));
  T(prepared->native_entries[i]->offset==4096&&prepared->native_entries[i]->snapshot.texture.target==NVN_TEXTURE_TARGET_2D);
 }
 T(mock_allocations==before_alloc&&mock_caches==before_cache);
 T(prepared->bc_images==2&&prepared->draw.pipeline.native.sampled_fdl[1]->format==(unsigned)bc_format);
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
 OK(api_nvnSamplerBuilderSetMinMagFilter(&changed,NVN_MIN_FILTER_LINEAR,NVN_MAG_FILTER_LINEAR));
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

 size_t saved_control=find(&qk,QUEUE)->queue_config.control;
 size_t pins_before=find(pools+SAMPLE0,POOL)->refs;
 find(&qk,QUEUE)->queue_config.control=sizeof(nn_bo*);
 unsigned pre_alloc=mock_allocations,pre_cache=mock_caches;
 ERR(api_nvnQueueSubmitCommands(&qk,1,&handle),ENOSPC);
 T(mock_allocations==pre_alloc&&mock_caches==pre_cache&&find(pools+SAMPLE0,POOL)->refs==pins_before);
 find(&qk,QUEUE)->queue_config.control=saved_control;
 materialize_faults(device,prepared);
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
 /* Re-registration frees the original slot layout. The existing prepared
  * entry must remain a deep value/pinned original backing; next prepare sees
  * the new version from the SAME recorded selector. */
 NVNtextureBuilder replacement_builder={0};NVNtexture replacement={0};
 OK(api_nvnTextureBuilderSetDefaults(&replacement_builder));OK(api_nvnTextureBuilderSetDevice(&replacement_builder,&dk));
 OK(api_nvnTextureBuilderSetFlags(&replacement_builder,source?0:NVN_TEXTURE_FLAGS_LINEAR));
 OK(api_nvnTextureBuilderSetSize2D(&replacement_builder,source?32:64,source?32:64));
 OK(api_nvnTextureBuilderSetFormat(&replacement_builder,(NVNformat)bc_format));
 OK(api_nvnTextureBuilderSetLevels(&replacement_builder,source?4:1));
 OK(api_nvnTextureBuilderSetStride(&replacement_builder,source?0:256));
 OK(api_nvnTextureBuilderSetStorage(&replacement_builder,pools+SAMPLE_ALT,4096));T(api_nvnTextureInitialize(&replacement,&replacement_builder));
 NVNtextureView replacement_view={0};OK(api_nvnTextureViewSetDefaults(&replacement_view));
 OK(api_nvnTextureViewSetLevels(&replacement_view,source?2:0,source?2:1));
 OK(api_nvnTexturePoolRegisterTexture(&texture_pool,257,&replacement,&replacement_view));
 memset(&replacement_view,0xcc,sizeof(replacement_view));OK(api_nvnTextureFinalize(&replacement));
 T(prepared->native_entries[1]->pool==find(pools+SAMPLE0,POOL)&&prepared->native_entries[1]->snapshot.texture.width==64);
 if(source)T(prepared->native_entries[1]->source_layout&&prepared->native_entries[1]->source_layout->level[0].width==64);
 ERR(api_nvnMemoryPoolFinalize(pools+SAMPLE0),EBUSY);
 struct pipeline_batch updated={0};T(!pipeline_batch_prepare(find(&qk,QUEUE),1,&handle,&updated));
 const struct pipeline_prepared_draw *newer=pipeline_batch_find(&updated,recorded,0);
 T(newer&&newer->native_entries[1]->pool==find(pools+SAMPLE_ALT,POOL)&&newer->native_entries[1]->snapshot.texture.width==(source?32:64));
 T(newer->draw.pipeline.native.sampled_fdl[1]->view_width==(source?8u:64u));
 T(newer->draw.pipeline.native.sampled_fdl[1]->level_count==(source?2u:1u)&&newer->draw.pipeline.native.sampled_fdl[1]->base_level==(source?2u:0u)&&prepared->draw.pipeline.native.sampled_fdl[1]->level_count==selected_count);
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
 unsigned queued_baseline=device->gpu->bos;
 OK(api_nvnQueueSubmitCommands(&qk,1,&copied));T(mock_submits==1);
 unsigned queued_nodes=0;for(struct job *j=device->retired;j;j=j->next)if(j->bo){queued_nodes++;T(nn_bo_idle(j->bo)==EBUSY);}
 T(queued_nodes>=18);
 nn_bo *unused_bo=find(pools+UNUSED,POOL)->bo,*metadata_bo=find(pools+META,POOL)->bo;for(size_t i=0;i<mock_ref_count;i++)T(mock_refs[i]!=unused_bo&&mock_refs[i]!=metadata_bo);
 handle=copied;
 /* An accepted but ambiguous mock ioctl keeps every actual BO and owner
  * until a real Finish result; handle destruction cannot declare completion. */
 mock_submit_error=EIO;ERR(api_nvnQueueSubmitCommands(&qk,1,&handle),EIO);mock_submit_error=0;
 T(mock_submits==2&&device->retired);
 /* Replace the live slot while both older four-mip jobs are pending. The
  * old backing must now be held by submission owners, not the slot. */
 OK(api_nvnTextureBuilderSetSize2D(&replacement_builder,16,16));
 OK(api_nvnTextureBuilderSetLevels(&replacement_builder,3));
 OK(api_nvnTextureBuilderSetStorage(&replacement_builder,pools+SAMPLE1,4096));
 T(api_nvnTextureInitialize(&replacement,&replacement_builder));
 OK(api_nvnTexturePoolRegisterTexture(&texture_pool,257,&replacement,NULL));
 OK(api_nvnTextureFinalize(&replacement));
 ERR(api_nvnMemoryPoolFinalize(pools+SAMPLE_ALT),EBUSY);
 /* Submitted actual owners remain pinned even if their public handle is gone. */
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,handle));T(device->retired);
 OK(api_nvnCommandBufferFinalize(&ck));
 OK(api_nvnMemoryPoolFinalize(pools+UNUSED));free(storage[UNUSED]);storage[UNUSED]=NULL;
 ERR(api_nvnProgramFinalize(&pk),EBUSY);ERR(api_nvnTextureFinalize(&tk),EBUSY);
 OK(api_nvnQueueFinish(&qk));T(!device->retired&&device->gpu->bos+1==queued_baseline); /* UNUSED was explicitly finalized above. */
 OK(api_nvnMemoryPoolFinalize(pools+SAMPLE_ALT));free(storage[SAMPLE_ALT]);storage[SAMPLE_ALT]=NULL;
 OK(api_nvnTexturePoolFinalize(&texture_pool));OK(api_nvnSamplerPoolFinalize(&sampler_pool));
 OK(api_nvnProgramFinalize(&pk));OK(api_nvnTextureFinalize(&tk));
 OK(api_nvnQueueFinalize(&qk));
 for(unsigned i=ARENA;i<POOLS;i++)if(storage[i]){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}
 OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
 printf("HOST ONLY actual BC mip-view enqueue owner source=%u checks=%u mock_submits=%u; GPU execution=false\n",source,checks,mock_submits);
 return 0;
}
int main(void)
{
 references();
 for(metadata_flags=0x12;metadata_flags<=0x14;metadata_flags+=2){
  for(bc_format=0x41;bc_format<=0x48;bc_format++){
   selected_base=0;selected_count=4;one(1);
   selected_base=1;selected_count=2;one(1);
   selected_base=3;selected_count=1;one(1);
  }
 }
 return 0;
}
