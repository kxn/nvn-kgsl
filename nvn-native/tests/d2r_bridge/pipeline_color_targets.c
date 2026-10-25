/* HOST actual public NVN + actual FDL/PM4, no GPU or pixel evaluation. */
#define main source_identity_fixture
#include NN_SOURCE_FIXTURE
#undef main
#include "pipeline_fixture_pair.h"
#include "../color_target_cases.h"
#include NN_COLOR_TARGET_IMAGE
static void make_pool(NVNdevice*d,NVNmemoryPoolBuilder*b,NVNmemoryPool*p,void**memory,size_t bytes,int flags)
{
 T(!posix_memalign(memory,4096,bytes));memset(*memory,0xa5,bytes);OK(api_nvnMemoryPoolBuilderSetDefaults(b));OK(api_nvnMemoryPoolBuilderSetDevice(b,d));OK(api_nvnMemoryPoolBuilderSetStorage(b,*memory,bytes));OK(api_nvnMemoryPoolBuilderSetFlags(b,flags));T(api_nvnMemoryPoolInitialize(p,b));
}
#if NN_NVN_COLOR_TARGETS
static void full_state(NVNcommandBuffer*c,unsigned test)
{
 NVNcolorState color;NVNchannelMaskState mask;NVNpolygonState polygon;NVNdepthStencilState depth;NVNmultisampleState multi;NVNblendState blend;
 OK(api_nvnColorStateSetDefaults(&color));OK(api_nvnChannelMaskStateSetDefaults(&mask));OK(api_nvnPolygonStateSetDefaults(&polygon));OK(api_nvnDepthStencilStateSetDefaults(&depth));OK(api_nvnMultisampleStateSetDefaults(&multi));OK(api_nvnBlendStateSetDefaults(&blend));
 if(test%3==1)OK(api_nvnChannelMaskStateSetChannelMask(&mask,0,1,0,1,0));
 if(test%3==2){OK(api_nvnColorStateSetBlendEnable(&color,0,1));OK(api_nvnBlendStateSetBlendFunc(&blend,NVN_BLEND_FUNC_ONE,NVN_BLEND_FUNC_ONE,NVN_BLEND_FUNC_ONE,NVN_BLEND_FUNC_ONE));}
 OK(api_nvnCommandBufferBindColorState(c,&color));OK(api_nvnCommandBufferBindChannelMaskState(c,&mask));OK(api_nvnCommandBufferBindPolygonState(c,&polygon));OK(api_nvnCommandBufferBindDepthStencilState(c,&depth));OK(api_nvnCommandBufferBindMultisampleState(c,&multi));OK(api_nvnCommandBufferBindBlendState(c,&blend));OK(api_nvnCommandBufferSetDepthBounds(c,0,0,1));
}
static void recorded_reject(NVNdevice*d,NVNcommandBuffer*c,NVNqueue*q,int expected)
{
 NVNcommandHandle h=api_nvnCommandBufferEndRecording(c);T(h&&!nn_nvn_error());
 struct pipeline_batch b={0};unsigned before=mock_submits;size_t bos=find(d,DEVICE)->gpu->bos;
 T(pipeline_batch_prepare(find(q,QUEUE),1,&h,&b)==expected);pipeline_batch_discard(&b);T(mock_submits==before&&find(d,DEVICE)->gpu->bos==bos);
 OK(api_nvnDeviceFinalizeCommandHandle(d,h));OK(api_nvnCommandBufferBeginRecording(c));
}
static void preflight_reject(NVNdevice*d,NVNcommandBuffer*c,NVNqueue*q,int expected)
{OK(api_nvnCommandBufferDrawArrays(c,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));recorded_reject(d,c,q,expected);}
#endif
static void one(unsigned format,unsigned test)
{
 (void)test;mock_submits=0;mock_submit_error=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object*device=find(&dk,DEVICE);(void)device;
 const struct nn_pipeline_archive*a=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_typed_pipeline_archive(&dk,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,reader,ranges));T(!nn_nvn_enable_pipeline_runtime_views(&dk));T(!nn_nvn_register_block_copy(&dk,&NVN_COMPILED_IMAGE));
 struct nn_pipeline_frontend_queries select={.version=1,.chip=0x06050002,.primitive=4,.point_size=1};nn_render_state_defaults(&select.render);const struct nn_pipeline_pair_recipe*pair=pipeline_fixture_color_pair(a,&select);T(pair);
 const struct nn_offline_graphics_stage*stages[]={a->stages->stages+pair->vs_index,a->stages->stages+pair->fs_index};
 enum{VS,FS,DATA,RT,ARENA,POOLS};NVNmemoryPoolBuilder pb[POOLS];NVNmemoryPool pool[POOLS];void*storage[POOLS];memset(pb,0,sizeof(pb));memset(pool,0,sizeof(pool));
 NVNtextureBuilder tb={0};NVNtexture texture={0};OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnTextureBuilderSetDevice(&tb,&dk));OK(api_nvnTextureBuilderSetFlags(&tb,9));OK(api_nvnTextureBuilderSetSize2D(&tb,73,41));OK(api_nvnTextureBuilderSetFormat(&tb,(NVNformat)format));size_t source_bytes=api_nvnTextureBuilderGetStorageSize(&tb);T(source_bytes&&!nn_nvn_error());size_t rt_bytes=(source_bytes+12287)&~(size_t)4095;
 for(unsigned i=0;i<POOLS;i++)make_pool(&dk,pb+i,pool+i,storage+i,i==RT?rt_bytes:16384,i==RT?0xa4:0x24);
 OK(api_nvnTextureBuilderSetStorage(&tb,pool+RT,4096));T(api_nvnTextureInitialize(&texture,&tb));struct object*target=find(&texture,TEXTURE);T(target->source_layout&&target->texture.flags==9&&target->source_layout->format==format);
 NVNshaderData shaders[2];range_count=0;allow(shaders,sizeof(shaders));for(unsigned i=0;i<2;i++){memcpy(storage[i],stages[i]->source,stages[i]->source_bytes);allow(storage[i],16384);allow(stages[i]->control,stages[i]->control_bytes);shaders[i]=(NVNshaderData){api_nvnMemoryPoolGetBufferAddress(pool+i),stages[i]->control};}
 NVNprogram program={0};T(api_nvnProgramInitialize(&program,&dk));T(api_nvnProgramSetShaders(&program,2,shaders));for(unsigned i=0;i<2;i++){OK(api_nvnMemoryPoolFinalize(pool+i));free(storage[i]);storage[i]=NULL;}
 NVNcommandBuffer command={0};_Alignas(16)unsigned char control[4096];T(api_nvnCommandBufferInitialize(&command,&dk));OK(api_nvnCommandBufferAddCommandMemory(&command,pool+ARENA,0,8192));OK(api_nvnCommandBufferAddControlMemory(&command,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&command));
 const NVNtexture*rt=&texture;
#if !NN_NVN_COLOR_TARGETS
 ERR(api_nvnCommandBufferSetRenderTargets(&command,1,&rt,NULL,NULL,NULL),ENOTSUP);goto cleanup;
#else
 OK(api_nvnCommandBufferSetRenderTargets(&command,1,&rt,NULL,NULL,NULL));
 OK(api_nvnCommandBufferBindProgram(&command,&program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));full_state(&command,test);
 NVNvertexAttribState attrs[2];NVNvertexStreamState streams[1];for(unsigned i=0;i<2;i++){OK(api_nvnVertexAttribStateSetDefaults(attrs+i));OK(api_nvnVertexAttribStateSetFormat(attrs+i,i?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,i?8:0));OK(api_nvnVertexAttribStateSetStreamIndex(attrs+i,0));}OK(api_nvnVertexStreamStateSetDefaults(streams));OK(api_nvnVertexStreamStateSetStride(streams,24));OK(api_nvnCommandBufferBindVertexAttribState(&command,2,attrs));OK(api_nvnCommandBufferBindVertexStreamState(&command,1,streams));
 uint64_t data=api_nvnMemoryPoolGetBufferAddress(pool+DATA),canonical=api_nvnTextureGetTextureAddress(&texture);OK(api_nvnCommandBufferBindVertexBuffer(&command,0,canonical,128));OK(api_nvnCommandBufferBindUniformBuffer(&command,NVN_SHADER_STAGE_VERTEX,0,data+4096,64));OK(api_nvnCommandBufferSetViewport(&command,0,0,73,41));OK(api_nvnCommandBufferSetScissor(&command,0,0,73,41));OK(api_nvnCommandBufferSetDepthRange(&command,0,1));
 NVNqueueBuilder qb={0};NVNqueue queue={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&queue,&qb));
 /* Actually consumed VFD/UBO alias the original canonical pixels, not their
  * later private target storage. Refuse before any native GPU allocation. */
 preflight_reject(&dk,&command,&queue,ENOTSUP);
 OK(api_nvnCommandBufferBindVertexBuffer(&command,0,data+1024,128));OK(api_nvnCommandBufferBindUniformBuffer(&command,NVN_SHADER_STAGE_VERTEX,0,canonical,64));preflight_reject(&dk,&command,&queue,ENOTSUP);
 OK(api_nvnCommandBufferBindUniformBuffer(&command,NVN_SHADER_STAGE_VERTEX,0,data+4096,64));
 /* Index and indirect/count reads also use the original canonical owner,
  * never a substituted native target address. All fail before materialization. */
 OK(api_nvnCommandBufferDrawElements(&command,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,3,canonical));recorded_reject(&dk,&command,&queue,ENOTSUP);
 OK(api_nvnCommandBufferDrawArraysIndirect(&command,NVN_DRAW_PRIMITIVE_TRIANGLES,canonical));recorded_reject(&dk,&command,&queue,ENOTSUP);
 OK(api_nvnCommandBufferMultiDrawArraysIndirectCount(&command,NVN_DRAW_PRIMITIVE_TRIANGLES,data+1024,canonical,1,16));recorded_reject(&dk,&command,&queue,ENOTSUP);
 /* Duplicate-import CPU storage aliases are normalized exactly like KGSL TBI.
  * A distinct synthetic BO/GPUVA is deliberately not used as disjoint proof. */
 struct object alias=*target->pool;alias.bo=find(pool+DATA,POOL)->bo;
 alias.storage=(void*)((uintptr_t)target->pool->storage|UINT64_C(0xab00000000000000));
 struct bound read={.pool=&alias,.offset=target->offset,.size=64};
 T(pipeline_target_bound_alias(target,&read)==ENOTSUP);read.offset=target->offset+source_bytes;T(!pipeline_target_bound_alias(target,&read));
 struct object denied=*target->pool,denied_target=*target;denied.gpu_policy=NN_NVN_GPU_UNCACHED_UNQUALIFIED;denied_target.pool=&denied;
 T(gpu_render_target_access(&denied_target,"HOST typed target policy")==ENOTSUP);T(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();
 /* A new semantic recording for Copy/Call explicitly captures all state. */
 OK(api_nvnCommandBufferSetRenderTargets(&command,1,&rt,NULL,NULL,NULL));OK(api_nvnCommandBufferBindProgram(&command,&program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));full_state(&command,test);
 OK(api_nvnCommandBufferBindVertexAttribState(&command,2,attrs));OK(api_nvnCommandBufferBindVertexStreamState(&command,1,streams));
 OK(api_nvnCommandBufferBindUniformBuffer(&command,NVN_SHADER_STAGE_VERTEX,0,data+4096,64));OK(api_nvnCommandBufferSetViewport(&command,0,0,73,41));OK(api_nvnCommandBufferSetScissor(&command,0,0,73,41));OK(api_nvnCommandBufferSetDepthRange(&command,0,1));
 /* Shared original pool, provably disjoint input window stays legal. */
 OK(api_nvnCommandBufferBindVertexBuffer(&command,0,canonical+source_bytes+256,128));
 OK(api_nvnCommandBufferDrawArrays(&command,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));struct late_draw*draw=find(&command,COMMAND)->record->current->draws;T(draw&&draw->generic_pipeline&&draw->target==target);T(draw->pipeline.native.target.layout->format==format&&draw->pipeline.native.target.layout->width==73);
 T(draw->pipeline.native.render.color_mask==(test%3==1?0xfffffff5u:UINT32_MAX));T(draw->pipeline.native.render.blend_enable==(test%3==2?1u:0u));
 NVNcommandHandle child=api_nvnCommandBufferEndRecording(&command);T(child&&!nn_nvn_error());
 struct pipeline_batch batch={0};T(!pipeline_batch_prepare(find(&queue,QUEUE),1,&child,&batch));const struct pipeline_prepared_draw*p=pipeline_batch_find(&batch,draw,0);T(p&&p->planned_images==1&&p->target_plan.native.format==format);T(p->target_plan.copy[0].args.row_bytes==73*p->target_plan.info.bytes_per_pixel);T(p->target_plan.copy[1].args.direction==1);pipeline_batch_discard(&batch);
 OK(api_nvnCommandBufferBeginRecording(&command));OK(api_nvnCommandBufferCopyCommands(&command,1,&child));NVNcommandHandle copy=api_nvnCommandBufferEndRecording(&command);T(copy&&!nn_nvn_error());
 OK(api_nvnCommandBufferBeginRecording(&command));OK(api_nvnCommandBufferCallCommands(&command,1,&child));NVNcommandHandle call=api_nvnCommandBufferEndRecording(&command);T(call&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&dk,child));
 OK(api_nvnQueueSubmitCommands(&queue,1,&copy));T(mock_submits==1);int has=0;for(size_t i=0;i<mock_ref_count;i++)has|=mock_refs[i]==target->bo;T(has);ERR(api_nvnTextureFinalize(&texture),EBUSY);ERR(api_nvnMemoryPoolFinalize(pool+RT),EBUSY);OK(api_nvnQueueFinish(&queue));
 OK(api_nvnQueueSubmitCommands(&queue,1,&call));T(mock_submits==2);OK(api_nvnQueueFinish(&queue));
 mock_submit_error=EIO;ERR(api_nvnQueueSubmitCommands(&queue,1,&call),EIO);mock_submit_error=0;T(device->retired&&mock_submits==3);OK(api_nvnDeviceFinalizeCommandHandle(&dk,copy));OK(api_nvnDeviceFinalizeCommandHandle(&dk,call));ERR(api_nvnTextureFinalize(&texture),EBUSY);OK(api_nvnQueueFinish(&queue));T(!device->retired);OK(api_nvnQueueFinalize(&queue));goto cleanup;
 #endif
 cleanup:
 OK(api_nvnCommandBufferFinalize(&command));OK(api_nvnTextureFinalize(&texture));OK(api_nvnProgramFinalize(&program));
 for(unsigned i=DATA;i<POOLS;i++){OK(api_nvnMemoryPoolFinalize(pool+i));free(storage[i]);}OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}
 printf("HOST public color format%u checks=%u mock_submits=%u GPU_execution=false\n",format,checks,mock_submits);
}
int main(void){for(unsigned i=0;i<22;i++)one(nn_color_formats[i],i);return 0;}
