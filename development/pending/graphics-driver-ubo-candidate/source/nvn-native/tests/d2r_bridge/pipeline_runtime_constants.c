/* HOST full-original-source/API/runtime-constants/Call ownership, no GPU execution. */
#define main source_identity_fixture
#include NN_SOURCE_FIXTURE
#undef main
static void pool_create(NVNdevice*d,NVNmemoryPoolBuilder*b,NVNmemoryPool*p,void**storage,size_t bytes,int flags){T(!posix_memalign(storage,4096,bytes));memset(*storage,0,bytes);OK(api_nvnMemoryPoolBuilderSetDefaults(b));OK(api_nvnMemoryPoolBuilderSetDevice(b,d));OK(api_nvnMemoryPoolBuilderSetStorage(b,*storage,bytes));OK(api_nvnMemoryPoolBuilderSetFlags(b,flags));T(api_nvnMemoryPoolInitialize(p,b));}
static void full_state(NVNcommandBuffer*c)
{
 NVNcolorState color;NVNchannelMaskState mask;NVNpolygonState polygon;NVNdepthStencilState depth;NVNmultisampleState multi;NVNblendState blend;
 OK(api_nvnColorStateSetDefaults(&color));OK(api_nvnChannelMaskStateSetDefaults(&mask));OK(api_nvnPolygonStateSetDefaults(&polygon));OK(api_nvnDepthStencilStateSetDefaults(&depth));OK(api_nvnMultisampleStateSetDefaults(&multi));OK(api_nvnBlendStateSetDefaults(&blend));
 OK(api_nvnCommandBufferBindColorState(c,&color));OK(api_nvnCommandBufferBindChannelMaskState(c,&mask));OK(api_nvnCommandBufferBindPolygonState(c,&polygon));OK(api_nvnCommandBufferBindDepthStencilState(c,&depth));OK(api_nvnCommandBufferBindMultisampleState(c,&multi));OK(api_nvnCommandBufferBindBlendState(c,&blend));OK(api_nvnCommandBufferSetDepthBounds(c,0,0,1));
}
static void inspect_original_runtime(struct object*d,const struct nn_offline_graphics_stage*const stages[2],unsigned pair_index)
{
 const struct nn_pipeline_runtime_constants*t=NN_GRAPHICS_PIPELINE_RUNTIME_ARCHIVE.pairs[pair_index];
 unsigned programs=0;
 for(struct job*j=d->retired;j;j=j->next)if(j->pipeline_owner){
  nn_pipeline_program*p=j->pipeline_owner;T(nn_pipeline_program_runtime_constants(p)==t);programs++;
  for(unsigned stage=0;stage<2;stage++){
   struct nn_pipeline_stage_view v;T(!nn_pipeline_program_stage(p,stage,&v));
   size_t bytes=stages[stage]->source_bytes-stages[stage]->code_bytes;
   T(v.c1_bytes==bytes);
   if(bytes){T(v.c1&&memcmp(nn_bo_map(v.c1),stages[stage]->source+stages[stage]->code_bytes,bytes)==0);}
   if(t&&t->layout->ubo_mask[stage]){
    T(v.c1&&v.c1_native_binding<16&&(t->layout->ubo_mask[stage]&(1u<<v.c1_native_binding)));
    unsigned refs=0;for(size_t i=0;i<mock_ref_count;i++)refs+=mock_refs[i]==v.c1;T(refs==1);
    uint32_t descriptor[2];T(!nn_pipeline_runtime_ubo_descriptor((struct nn_pipeline_buffer){v.c1,0,bytes},descriptor));
    T(((descriptor[1]>>17)*16u)==bytes);
   }
  }
 }
 T(programs==1);
}
static int one(unsigned pair_index,unsigned call,unsigned mode)
{
 const struct nn_pipeline_archive*a=&NN_GRAPHICS_PIPELINE_ARCHIVE;const struct nn_pipeline_pair_recipe*pair=a->pairs+pair_index;const struct nn_offline_graphics_stage*stages[2]={a->stages->stages+pair->vs_index,a->stages->stages+pair->fs_index};
 unsigned select=0;T(!nn_pipeline_storage_source_selector(0,0,84,&select)&&select==4);T(!nn_pipeline_storage_source_selector(0,0,88,&select)&&select==5);T(nn_pipeline_storage_source_selector(0,0,196,&select)==ENOTSUP);T(nn_pipeline_storage_source_selector(1,0,196,&select)==ENOTSUP);T(nn_pipeline_storage_source_selector(0,1,84,&select)==ENOTSUP);T(nn_pipeline_storage_source_selector(0,0,85,&select)==ENOTSUP);T(nn_pipeline_storage_source_selector(1,0,324+64,&select)==ENOTSUP);
 mock_submits=0;mock_submit_error=0;NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object*d=find(&dk,DEVICE);
 T(!nn_nvn_register_typed_pipeline_archive(&dk,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,reader,ranges));if(mode!=4)T(!nn_nvn_register_pipeline_storage_archive(&dk,&NN_GRAPHICS_PIPELINE_STORAGE_ARCHIVE));if(mode!=5)T(!nn_nvn_register_pipeline_runtime_archive(&dk,&NN_GRAPHICS_PIPELINE_RUNTIME_ARCHIVE));
 struct nn_fdl_image_desc fd={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=64,.height=64,.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=NN_FDL_SAMPLED|NN_FDL_COLOR_TARGET};struct nn_fdl_image_view fv;T(!nn_fdl_image(&fd,&fv));
 struct nn_tex_native_view v={.schema=1,.chip=fv.chip,.format=fv.format,.target=1,.levels=1,.layers=1,.samples=1,.width=64,.height=64,.pitch=fv.level[0].pitch,.layer_bytes=fv.level[0].layer_stride,.bytes=fv.bytes,.alignment=fv.alignment,.required_span_bytes=fv.required_span,.descriptor_address_word=fv.descriptor_address_word,.descriptor_address_mask=fv.descriptor_address_mask,.rb_mrt_buf_info=fv.rb_mrt_buf_info,.rb_mrt_pitch=fv.rb_mrt_pitch,.rb_mrt_array_pitch=fv.rb_mrt_array_pitch,.sp_ps_mrt_reg=fv.sp_ps_mrt_reg};memcpy(v.texture_descriptor,fv.descriptor,64);T(!nn_nvn_register_pipeline_views(&dk,&v,1));
 enum{VS,FS,ARENA,DATA,RT,SB4,SB5,UNUSED,PARENT_ARENA,N};NVNmemoryPoolBuilder pb[N]={0};NVNmemoryPool pools[N]={0};void*storage[N]={0};for(unsigned i=0;i<N;i++)pool_create(&dk,pb+i,pools+i,storage+i,65536,i==UNUSED?0x14:0x24);
 NVNshaderData shaders[2];unsigned char controls[2][0x898];range_count=0;allow(shaders,sizeof(shaders));for(unsigned i=0;i<2;i++){T(stages[i]->source_bytes<=65536);memcpy(storage[i],stages[i]->source,stages[i]->source_bytes);memcpy(controls[i],stages[i]->control,stages[i]->control_bytes);allow(storage[i],65536);allow(controls[i],sizeof(controls[i]));shaders[i]=(NVNshaderData){api_nvnMemoryPoolGetBufferAddress(pools+i),controls[i]};}
 NVNprogram pk={0};T(api_nvnProgramInitialize(&pk,&dk));T(api_nvnProgramSetShaders(&pk,2,shaders));
 /* Original inputs are caller-owned; actual Program source/C1 must survive overwrite. */
 for(unsigned i=0;i<2;i++){memset(storage[i],0xcc,65536);memset(controls[i],0xcc,sizeof(controls[i]));}
 NVNtextureBuilder tb={0};NVNtexture tk={0};OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnTextureBuilderSetDevice(&tb,&dk));OK(api_nvnTextureBuilderSetFlags(&tb,NVN_TEXTURE_FLAGS_LINEAR));OK(api_nvnTextureBuilderSetSize2D(&tb,64,64));OK(api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetStride(&tb,256));OK(api_nvnTextureBuilderSetStorage(&tb,pools+RT,4096));T(api_nvnTextureInitialize(&tk,&tb));
 NVNcommandBuffer child={0},parent={0};_Alignas(16)unsigned char control[2][4096];T(api_nvnCommandBufferInitialize(&child,&dk));OK(api_nvnCommandBufferAddCommandMemory(&child,pools+ARENA,0,65536));OK(api_nvnCommandBufferAddControlMemory(&child,control[0],4096));OK(api_nvnCommandBufferBeginRecording(&child));full_state(&child);
 OK(api_nvnCommandBufferBindProgram(&child,&pk,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));NVNvertexAttribState attrib[16];for(unsigned i=0;i<16;i++){OK(api_nvnVertexAttribStateSetDefaults(attrib+i));OK(api_nvnVertexAttribStateSetFormat(attrib+i,NVN_FORMAT_RGBA32F,i*16));OK(api_nvnVertexAttribStateSetStreamIndex(attrib+i,0));}OK(api_nvnCommandBufferBindVertexAttribState(&child,16,attrib));NVNvertexStreamState stream;OK(api_nvnVertexStreamStateSetDefaults(&stream));OK(api_nvnVertexStreamStateSetStride(&stream,256));OK(api_nvnCommandBufferBindVertexStreamState(&child,1,&stream));uint64_t data=api_nvnMemoryPoolGetBufferAddress(pools+DATA);OK(api_nvnCommandBufferBindVertexBuffer(&child,0,data+4096,4096));
 for(unsigned st=0;st<2;st++)for(unsigned i=0;i<14;i++)OK(api_nvnCommandBufferBindUniformBuffer(&child,(NVNshaderStage)st,i,data+8192+i*4096,4096));
 const NVNtexture*targets[]={&tk};OK(api_nvnCommandBufferSetRenderTargets(&child,1,targets,NULL,NULL,NULL));OK(api_nvnCommandBufferSetViewport(&child,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&child,0,0,64,64));OK(api_nvnCommandBufferSetDepthRange(&child,0,1));
 NVNcommandBuffer*bindings=&child;
 if(call){T(api_nvnCommandBufferInitialize(&parent,&dk));OK(api_nvnCommandBufferAddCommandMemory(&parent,pools+PARENT_ARENA,0,65536));OK(api_nvnCommandBufferAddControlMemory(&parent,control[1],4096));OK(api_nvnCommandBufferBeginRecording(&parent));bindings=&parent;}
 uint64_t unused=api_nvnMemoryPoolGetBufferAddress(pools+UNUSED);for(unsigned st=0;st<2;st++)for(unsigned i=0;i<16;i++)OK(api_nvnCommandBufferBindStorageBuffer(bindings,(NVNshaderStage)st,i,unused,4096));
 for(unsigned st=0;st<2;st++)for(size_t j=0;j<stages[st]->storage_count;j++){unsigned api=0;const struct nn_offline_storage_binding*b=stages[st]->storage+j;T(!nn_pipeline_storage_source_selector(st,b->cbuf_slot,b->cbuf_word,&api));T(api==4||api==5);unsigned pool=api==4?SB4:SB5;if(mode==3)pool=RT;OK(api_nvnCommandBufferBindStorageBuffer(bindings,(NVNshaderStage)st,mode==1?0:(int)api,api_nvnMemoryPoolGetBufferAddress(pools+pool)+4096+(mode==2?4:0),8192));}
 OK(api_nvnCommandBufferDrawArraysInstanced(&child,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3,17,2));NVNcommandHandle h=api_nvnCommandBufferEndRecording(&child);T(h&&!nn_nvn_error());NVNcommandHandle child_handle=h;
 if(call){OK(api_nvnCommandBufferCallCommands(&parent,1,&h));h=api_nvnCommandBufferEndRecording(&parent);T(h&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&dk,child_handle));OK(api_nvnCommandBufferFinalize(&child));}
 NVNqueueBuilder qb={0};NVNqueue q={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&q,&qb));
 struct pipeline_batch batch={0};struct handle*projected=NULL;int admission=0;if(call){struct handle*body=handles;while(body&&body->id!=h)body=body->next;T(body);struct nn_command_state initial={0},final;admission=command_project_sequence(find(&q,QUEUE),body,&initial,&final,&projected);if(!admission)admission=pipeline_batch_prepare_owned(find(&q,QUEUE),1,&projected,&batch);}else admission=pipeline_batch_prepare(find(&q,QUEUE),1,&h,&batch);if(mode){T(admission==ENOTSUP&&!mock_submits);nn_nvn_clear_error();pipeline_batch_discard(&batch);if(projected)command_body_destroy(projected);}else{if(admission)fprintf(stderr,"pair%u call%u admission%d\n",pair_index,call,admission);T(!admission);const struct pipeline_prepared_draw*prepared=batch.draws;T(prepared&&prepared->draw.pipeline.native.storage_types);const struct nn_pipeline_draw_state*state=&prepared->draw.pipeline.native;T(state->runtime_constants==NN_GRAPHICS_PIPELINE_RUNTIME_ARCHIVE.pairs[pair_index]&&state->draw.base_instance==17&&state->draw.instances==2);unsigned active_pool=N;
 for(unsigned st=0;st<2;st++)for(size_t j=0;j<stages[st]->storage_count;j++){const struct nn_offline_storage_binding*b=stages[st]->storage+j;if(!(state->storage_types->mask[st]&(1u<<b->binding)))continue;unsigned api=0;T(!nn_pipeline_storage_source_selector(st,b->cbuf_slot,b->cbuf_word,&api));active_pool=api==4?SB4:SB5;T(state->storage[st][b->binding].bo==find(pools+active_pool,POOL)->bo&&state->storage[st][b->binding].offset==4096);}
 pipeline_batch_discard(&batch);if(projected)command_body_destroy(projected);
 for(unsigned replay=0;replay<2;replay++){OK(api_nvnQueueSubmitCommands(&q,1,&h));T(mock_submits==replay+1);T(!nn_bo_idle(find(pools+UNUSED,POOL)->bo));unsigned sbrefs=0;for(size_t i=0;i<mock_ref_count;i++)sbrefs+=mock_refs[i]==find(pools+SB4,POOL)->bo||mock_refs[i]==find(pools+SB5,POOL)->bo;T(sbrefs>0);inspect_original_runtime(d,stages,pair_index);OK(api_nvnQueueFinish(&q));}
 T(active_pool<N);mock_submit_error=EIO;ERR(api_nvnQueueSubmitCommands(&q,1,&h),EIO);mock_submit_error=0;T(mock_submits==3&&d->retired&&nn_bo_idle(find(pools+active_pool,POOL)->bo)==EBUSY);ERR(api_nvnMemoryPoolFinalize(pools+active_pool),EBUSY);ERR(api_nvnProgramFinalize(&pk),EBUSY);OK(api_nvnQueueFinish(&q));T(!d->retired);}
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,h));OK(api_nvnCommandBufferFinalize(call?&parent:&child));OK(api_nvnQueueFinalize(&q));OK(api_nvnProgramFinalize(&pk));OK(api_nvnTextureFinalize(&tk));for(unsigned i=0;i<N;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}T(!handles);(void)d;return 0;
}
int main(void){T(NN_GRAPHICS_PIPELINE_ARCHIVE.pair_count==25);T(!nn_pipeline_runtime_archive_validate(&NN_GRAPHICS_PIPELINE_RUNTIME_ARCHIVE));T(!nn_pipeline_storage_archive_validate(&NN_GRAPHICS_PIPELINE_STORAGE_ARCHIVE));for(unsigned i=0;i<25;i++)for(unsigned call=0;call<2;call++)T(!one(i,call,0));for(unsigned mode=1;mode<=4;mode++)for(unsigned call=0;call<2;call++)T(!one(0,call,mode));unsigned runtime_pair=0;while(runtime_pair<25&&!NN_GRAPHICS_PIPELINE_RUNTIME_ARCHIVE.pairs[runtime_pair])runtime_pair++;T(runtime_pair<25);for(unsigned call=0;call<2;call++)T(!one(runtime_pair,call,5));printf("{\"checks\":%u,\"original_pairs\":25,\"mock_submits\":150,\"mock_failed_submits\":50,\"negative_cases\":10,\"gpu_execution\":false}\n",checks);return 0;}
