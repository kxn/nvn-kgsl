/* HOST ONLY original complete CASC stages, public target/Call/Copy, no GPU. */
#define NN_PUBLIC_DEPTH_SAMPLED_MAIN public_depth_sampled_fixture
#include "shader_source_enqueue_sampled.c"
#undef NN_PUBLIC_DEPTH_SAMPLED_MAIN
#ifndef NN_DEPTH_COLOR_PAIR
#define NN_DEPTH_COLOR_PAIR 2
#endif
#ifndef NN_DEPTH_GATHER_PAIR
#define NN_DEPTH_GATHER_PAIR 3
#endif
static unsigned depth_cases;
static void complete_depth(unsigned pair_index,int format,unsigned color)
{
 mock_submits=0;range_count=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object*d=find(&dk,DEVICE);
 const struct nn_pipeline_archive*a=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 T(!nn_nvn_register_typed_pipeline_archive(&dk,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,reader,ranges));
 T(!nn_nvn_register_pipeline_depth_archive(&dk,&NN_GRAPHICS_PIPELINE_DEPTH_ARCHIVE));
 T(!nn_nvn_register_shader_source_catalogue(&dk,&NN_COMPLETE_GRAPHICS_SOURCE_ARCHIVE));
 T(nn_nvn_register_pipeline_depth_archive(&dk,&NN_GRAPHICS_PIPELINE_DEPTH_ARCHIVE)==EEXIST);
 T(!nn_nvn_register_block_copy(&dk,&NVN_COMPILED_IMAGE));T(!nn_nvn_enable_pipeline_runtime_views(&dk));
 enum{VS,FS,ARENA,DATA,Z,TEX,META,COLOR,COUNT};NVNmemoryPoolBuilder pb[COUNT]={0};NVNmemoryPool pools[COUNT]={0};void*storage[COUNT]={0};
 for(unsigned i=0;i<COUNT;i++)pool_create(&dk,pb+i,pools+i,storage+i,i==Z||i==TEX||i==COLOR?1048576:i==META?32768:16384,i==META?0x12:0x24);
 const struct nn_pipeline_pair_recipe*pair=a->pairs+pair_index;T(pair_index==NN_DEPTH_COLOR_PAIR||pair_index==NN_DEPTH_GATHER_PAIR);
 const struct nn_offline_graphics_stage*stages[]={a->stages->stages+pair->vs_index,a->stages->stages+pair->fs_index};
 NVNshaderData shader[2];unsigned char control[2][0x898];allow(shader,sizeof(shader));
 for(unsigned i=0;i<2;i++){T(stages[i]->control_bytes<=sizeof(control[i]));memcpy(storage[i],stages[i]->source,stages[i]->source_bytes);memcpy(control[i],stages[i]->control,stages[i]->control_bytes);allow(storage[i],16384);allow(control[i],sizeof(control[i]));shader[i]=(NVNshaderData){api_nvnMemoryPoolGetBufferAddress(pools+i),control[i]};}
 NVNprogram program={0};T(api_nvnProgramInitialize(&program,&dk));T(api_nvnProgramSetShaders(&program,2,shader));
 for(unsigned i=0;i<2;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);storage[i]=NULL;}
 NVNtextureBuilder zb={0},cb={0},sb={0};NVNtexture z={0},c={0},sample={0};
 OK(api_nvnTextureBuilderSetDefaults(&zb));OK(api_nvnTextureBuilderSetDevice(&zb,&dk));OK(api_nvnTextureBuilderSetFormat(&zb,(NVNformat)format));
 OK(api_nvnTextureBuilderSetTarget(&zb,NVN_TEXTURE_TARGET_2D_ARRAY));OK(api_nvnTextureBuilderSetSize3D(&zb,128,128,3));OK(api_nvnTextureBuilderSetLevels(&zb,2));OK(api_nvnTextureBuilderSetStorage(&zb,pools+Z,4096));T(api_nvnTextureInitialize(&z,&zb));
 NVNtextureView zv={0};OK(api_nvnTextureViewSetDefaults(&zv));OK(api_nvnTextureViewSetLevels(&zv,1,1));OK(api_nvnTextureViewSetLayers(&zv,2,1));
 if(color){OK(api_nvnTextureBuilderSetDefaults(&cb));OK(api_nvnTextureBuilderSetDevice(&cb,&dk));OK(api_nvnTextureBuilderSetFlags(&cb,NVN_TEXTURE_FLAGS_LINEAR));OK(api_nvnTextureBuilderSetSize2D(&cb,64,64));OK(api_nvnTextureBuilderSetFormat(&cb,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetStride(&cb,256));OK(api_nvnTextureBuilderSetStorage(&cb,pools+COLOR,4096));T(api_nvnTextureInitialize(&c,&cb));}
 NVNtexturePool tp={0};NVNsamplerPool sp={0};
 if(pair_index==NN_DEPTH_GATHER_PAIR){
  T(api_nvnTexturePoolInitialize(&tp,pools+META,0,320));T(api_nvnSamplerPoolInitialize(&sp,pools+META,16384,320));
  OK(api_nvnTextureBuilderSetDefaults(&sb));OK(api_nvnTextureBuilderSetDevice(&sb,&dk));OK(api_nvnTextureBuilderSetSize2D(&sb,17,11));OK(api_nvnTextureBuilderSetFormat(&sb,NVN_FORMAT_R32F));OK(api_nvnTextureBuilderSetStorage(&sb,pools+TEX,4096));T(api_nvnTextureInitialize(&sample,&sb));
 }
 NVNcommandBuffer cmd={0};_Alignas(16)unsigned char cpu[2048];T(api_nvnCommandBufferInitialize(&cmd,&dk));OK(api_nvnCommandBufferAddCommandMemory(&cmd,pools+ARENA,0,16384));OK(api_nvnCommandBufferAddControlMemory(&cmd,cpu,sizeof(cpu)));OK(api_nvnCommandBufferBeginRecording(&cmd));
 explicit_render_defaults(&cmd);
 OK(api_nvnCommandBufferBindProgram(&cmd,&program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
 NVNvertexAttribState attrs[4];for(unsigned i=0;i<4;i++)OK(api_nvnVertexAttribStateSetDefaults(attrs+i));
 if(pair_index==NN_DEPTH_GATHER_PAIR){OK(api_nvnVertexAttribStateSetFormat(attrs,NVN_FORMAT_RGB32F,0));OK(api_nvnVertexAttribStateSetFormat(attrs+3,NVN_FORMAT_RG32F,12));}
 else{OK(api_nvnVertexAttribStateSetFormat(attrs,NVN_FORMAT_RG32F,0));OK(api_nvnVertexAttribStateSetFormat(attrs+1,NVN_FORMAT_RGBA32F,8));}
 for(unsigned i=0;i<4;i++){OK(api_nvnVertexAttribStateSetStreamIndex(attrs+i,3));}OK(api_nvnCommandBufferBindVertexAttribState(&cmd,4,attrs));
 NVNvertexStreamState streams[4];for(unsigned i=0;i<4;i++)OK(api_nvnVertexStreamStateSetDefaults(streams+i));OK(api_nvnVertexStreamStateSetStride(streams+3,pair_index==NN_DEPTH_GATHER_PAIR?20:24));OK(api_nvnCommandBufferBindVertexStreamState(&cmd,4,streams));
 uint64_t data=api_nvnMemoryPoolGetBufferAddress(pools+DATA);OK(api_nvnCommandBufferBindVertexBuffer(&cmd,3,data+4096,4096));OK(api_nvnCommandBufferBindUniformBuffer(&cmd,NVN_SHADER_STAGE_VERTEX,pair_index==NN_DEPTH_GATHER_PAIR?3:0,data+8192,64));
 if(pair_index==NN_DEPTH_GATHER_PAIR){OK(api_nvnCommandBufferSetTexturePool(&cmd,&tp));OK(api_nvnCommandBufferSetSamplerPool(&cmd,&sp));OK(api_nvnCommandBufferBindSeparateTexture(&cmd,NVN_SHADER_STAGE_FRAGMENT,10,api_nvnDeviceGetSeparateTextureHandle(&dk,257)));OK(api_nvnCommandBufferBindSeparateSampler(&cmd,NVN_SHADER_STAGE_FRAGMENT,3,api_nvnDeviceGetSeparateSamplerHandle(&dk,259)));}
 const NVNtexture*colors[]={&c};OK(api_nvnCommandBufferSetRenderTargets(&cmd,color,color?colors:NULL,NULL,&z,&zv));
 memset(&zv,0xcc,sizeof(zv));OK(api_nvnCommandBufferSetViewport(&cmd,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&cmd,3,5,57,51));OK(api_nvnCommandBufferSetDepthRange(&cmd,0,1));
 NVNdepthStencilState ds={0};OK(api_nvnDepthStencilStateSetDefaults(&ds));OK(api_nvnDepthStencilStateSetDepthTestEnable(&ds,1));OK(api_nvnDepthStencilStateSetDepthWriteEnable(&ds,1));OK(api_nvnDepthStencilStateSetDepthFunc(&ds,NVN_DEPTH_FUNC_LESS));OK(api_nvnCommandBufferBindDepthStencilState(&cmd,&ds));
 OK(api_nvnCommandBufferClearDepthStencil(&cmd,.75f,1,0,0));OK(api_nvnCommandBufferDrawArrays(&cmd,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
 struct late_draw*recorded=find(&cmd,COMMAND)->record->current->draws;T(recorded&&recorded->depth_view.level==1&&recorded->depth_view.layer==2&&!!recorded->target==!!color);
 NVNcommandHandle h=api_nvnCommandBufferEndRecording(&cmd);T(h&&!nn_nvn_error());ERR(api_nvnTextureFinalize(&z),EBUSY);
 NVNqueueBuilder qb={0};NVNqueue q={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&q,&qb));
 if(pair_index==NN_DEPTH_GATHER_PAIR){
  unsigned ma=mock_allocations,mc=mock_caches;struct pipeline_batch missing={0};T(pipeline_batch_prepare(find(&q,QUEUE),1,&h,&missing)==ENOTSUP);pipeline_batch_discard(&missing);T(mock_allocations==ma&&mock_caches==mc);
  OK(api_nvnTexturePoolRegisterTexture(&tp,257,&sample,NULL));OK(api_nvnTextureFinalize(&sample));
  NVNsamplerBuilder b={0};OK(api_nvnSamplerBuilderSetDefaults(&b));OK(api_nvnSamplerBuilderSetDevice(&b,&dk));OK(api_nvnSamplerBuilderSetMinMagFilter(&b,NVN_MIN_FILTER_NEAREST,NVN_MAG_FILTER_NEAREST));OK(api_nvnSamplerBuilderSetWrapMode(&b,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE));OK(api_nvnSamplerPoolRegisterSamplerBuilder(&sp,259,&b));remove_object(find(&b,SB));
 }
 struct pipeline_batch batch={0};unsigned before=mock_allocations;int prepare_error=pipeline_batch_prepare(find(&q,QUEUE),1,&h,&batch);if(prepare_error)fprintf(stderr,"pair%u color%u format%d prepare=%d\n",pair_index,color,format,prepare_error);T(!prepare_error&&mock_allocations==before);
 const struct pipeline_prepared_draw*p=pipeline_batch_find(&batch,recorded,0);T(p&&p->depth_export&&p->depth_export->writes_depth==(pair_index==NN_DEPTH_GATHER_PAIR));T(p->depth_export->recipe==pair->recipe);T(p->depth_controls.test_enable&&p->depth_controls.write_enable);
 if(pair_index==NN_DEPTH_GATHER_PAIR){T(p->native_entries[1]&&p->draw.pipeline.native.texture_types->gathers[1].count==1);T(p->draw.pipeline.native.ubo[0][1].offset==8192);T(p->depth_export->depth_regid==14);T(!!p->draw.pipeline.native.target.layout==!!color);}
 pipeline_batch_discard(&batch);
 if(pair_index==NN_DEPTH_COLOR_PAIR){
  OK(api_nvnCommandBufferBeginRecording(&cmd));OK(api_nvnCommandBufferSetRenderTargets(&cmd,0,NULL,NULL,&z,NULL));
  /* Use a matching default-view128 viewport so the actual reason is DATA0
   * without a COLOR role, not an accidental old selected-view mismatch. */
  OK(api_nvnCommandBufferSetViewport(&cmd,0,0,128,128));OK(api_nvnCommandBufferSetScissor(&cmd,0,0,128,128));
  OK(api_nvnCommandBufferDrawArrays(&cmd,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));NVNcommandHandle no_color=api_nvnCommandBufferEndRecording(&cmd);T(no_color&&!nn_nvn_error());
  unsigned na=mock_allocations,nc=mock_caches,ns=mock_submits;NVNcommandHandle both[]={h,no_color};
  ERR(api_nvnQueueSubmitCommands(&q,2,both),ENOTSUP);T(mock_allocations==na&&mock_caches==nc&&mock_submits==ns);OK(api_nvnDeviceFinalizeCommandHandle(&dk,no_color));
 }
 metadata_preflight_oom(&q,h);
 OK(api_nvnCommandBufferBeginRecording(&cmd));OK(api_nvnCommandBufferCallCommands(&cmd,1,&h));NVNcommandHandle call=api_nvnCommandBufferEndRecording(&cmd);T(call&&!nn_nvn_error());
 OK(api_nvnQueueSubmitCommands(&q,1,&call));T(mock_submits==1);OK(api_nvnQueueFinish(&q));OK(api_nvnDeviceFinalizeCommandHandle(&dk,call));
 OK(api_nvnCommandBufferBeginRecording(&cmd));OK(api_nvnCommandBufferCopyCommands(&cmd,1,&h));NVNcommandHandle copy=api_nvnCommandBufferEndRecording(&cmd);T(copy&&!nn_nvn_error());
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,h));mock_submit_error=EIO;ERR(api_nvnQueueSubmitCommands(&q,1,&copy),EIO);mock_submit_error=0;T(d->retired&&mock_submits==2);
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,copy));OK(api_nvnCommandBufferFinalize(&cmd));ERR(api_nvnTextureFinalize(&z),EBUSY);ERR(api_nvnProgramFinalize(&program),EBUSY);OK(api_nvnQueueFinish(&q));T(!d->retired);
 if(pair_index==NN_DEPTH_GATHER_PAIR){OK(api_nvnTexturePoolFinalize(&tp));OK(api_nvnSamplerPoolFinalize(&sp));}
 OK(api_nvnTextureFinalize(&z));if(color)OK(api_nvnTextureFinalize(&c));OK(api_nvnProgramFinalize(&program));OK(api_nvnQueueFinalize(&q));
 for(unsigned i=2;i<COUNT;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}
 OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
 depth_cases++;printf("HOST ONLY complete-source public depth pair=%u color=%u format=%d checks=%u mock_submits=2 GPU execution=false\n",pair_index,color,format,checks);
}
int main(void)
{
 int formats[]={NVN_FORMAT_DEPTH16,NVN_FORMAT_DEPTH24,NVN_FORMAT_DEPTH32F};
 for(unsigned i=0;i<3;i++){complete_depth(NN_DEPTH_COLOR_PAIR,formats[i],1);complete_depth(NN_DEPTH_GATHER_PAIR,formats[i],0);complete_depth(NN_DEPTH_GATHER_PAIR,formats[i],1);}
 T(depth_cases==9);return 0;
}
