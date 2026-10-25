/* HOST actual source acceptance and explicit unavailable-native Draw rejection. */
#define main compiled_source_identity_fixture
#include "shader_source_core.c"
#undef main
#include NN_PIPELINE_NATIVE_VIEWS
static void source_pool(NVNdevice*d,NVNmemoryPoolBuilder*b,NVNmemoryPool*p,void**bytes,size_t size)
{T(!posix_memalign(bytes,4096,size));memset(*bytes,0,size);OK(api_nvnMemoryPoolBuilderSetDefaults(b));OK(api_nvnMemoryPoolBuilderSetDevice(b,d));OK(api_nvnMemoryPoolBuilderSetStorage(b,*bytes,size));OK(api_nvnMemoryPoolBuilderSetFlags(b,0x24));T(api_nvnMemoryPoolInitialize(p,b));}
static int available(const struct nn_pipeline_archive*a,const struct nn_shader_source_entry*s)
{
 for(size_t i=0;i<a->stages->stage_count;i++){const struct nn_offline_graphics_stage*x=a->stages->stages+i;if(x->stage==s->stage&&x->source_bytes==s->source_bytes&&x->control_bytes==s->control_bytes&&!memcmp(x->source,s->source,s->source_bytes)&&!memcmp(x->control,s->control,s->control_bytes))return 1;}return 0;
}
static int no_query(void*u,uint32_t stage,const struct nn_pipeline_query*q,uint32_t out[32],size_t*n)
{(void)stage;(void)q;(void)out;(*(unsigned*)u)++;*n=0;return ENOTSUP;}
static void render_defaults(NVNcommandBuffer*c)
{
 NVNcolorState color;NVNchannelMaskState mask;NVNpolygonState polygon;NVNdepthStencilState depth;NVNmultisampleState ms;
 OK(api_nvnColorStateSetDefaults(&color));OK(api_nvnChannelMaskStateSetDefaults(&mask));OK(api_nvnPolygonStateSetDefaults(&polygon));OK(api_nvnDepthStencilStateSetDefaults(&depth));OK(api_nvnMultisampleStateSetDefaults(&ms));
 OK(api_nvnCommandBufferBindColorState(c,&color));OK(api_nvnCommandBufferBindChannelMaskState(c,&mask));OK(api_nvnCommandBufferBindPolygonState(c,&polygon));OK(api_nvnCommandBufferBindDepthStencilState(c,&depth));OK(api_nvnCommandBufferBindMultisampleState(c,&ms));OK(api_nvnCommandBufferSetDepthBounds(c,0,0,1));
}
int main(void)
{
 T(!compiled_source_identity_fixture());range_count=0;
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object*d=find(&dk,DEVICE);
 const struct nn_pipeline_archive*a=&NN_GRAPHICS_PIPELINE_ARCHIVE;T(!nn_nvn_register_pipeline_archive(&dk,a,reader,ranges));T(!nn_nvn_register_shader_source_catalogue(&dk,&NN_COMPLETE_GRAPHICS_SOURCE_ARCHIVE));
 const struct nn_tex_native_view*view=NULL;for(size_t i=0;i<NN_TEXTURE_NATIVE_VIEW_COUNT;i++)if(NN_TEXTURE_NATIVE_VIEWS[i].width==64&&NN_TEXTURE_NATIVE_VIEWS[i].height==64)view=NN_TEXTURE_NATIVE_VIEWS+i;T(view);T(!nn_nvn_register_pipeline_views(&dk,view,1));
 const struct nn_shader_source_entry*stage[2]={0};for(size_t i=0;i<nn_shader_source_catalogue_count(d->shader_sources);i++){const struct nn_shader_source_entry*s=nn_shader_source_entry_at(d->shader_sources,i);if(!stage[s->stage]&&!available(a,s)&&(!s->stage||s->source_bytes>s->code_bytes))stage[s->stage]=s;}T(stage[0]&&stage[1]);
 enum{VS,FS,ARENA,TARGET,META,SAMPLE,VBO,COUNT};NVNmemoryPoolBuilder pb[COUNT]={0};NVNmemoryPool pools[COUNT]={0};void*storage[COUNT]={0};size_t sizes[COUNT];
 for(unsigned i=0;i<COUNT;i++){sizes[i]=i<2?(stage[i]->source_bytes+8191)&~(size_t)4095:32768;source_pool(&dk,pb+i,pools+i,storage+i,sizes[i]);}
 _Alignas(8)unsigned char controls[2][0x898];NVNshaderData shaders[2];allow(shaders,sizeof(shaders));for(unsigned i=0;i<2;i++){memcpy(storage[i],stage[i]->source,stage[i]->source_bytes);memcpy(controls[i],stage[i]->control,stage[i]->control_bytes);allow(storage[i],sizes[i]);allow(controls[i],stage[i]->control_bytes);shaders[i]=(NVNshaderData){api_nvnMemoryPoolGetBufferAddress(pools+i),controls[i]};}
 NVNprogram pk={0};T(api_nvnProgramInitialize(&pk,&dk));T(api_nvnProgramSetShaders(&pk,2,shaders));struct object*pr=find(&pk,PROGRAM);T(pr->pipeline_source.binding&&!pr->bo&&!pr->fs_bo&&!pr->graphics&&!pr->compiled);
 struct nn_pipeline_source_program before=pr->pipeline_source;const struct nn_offline_graphics_stage*selected[2]={(void*)1,(void*)1};const struct nn_pipeline_shader_recipe*r=(void*)1;unsigned queries=0;
 T(nn_pipeline_source_resolve(&before,no_query,&queries,selected,selected+1,&r)==ENOTSUP&&!selected[0]&&!selected[1]&&!r&&!queries);
#if NN_NVN_GRAPHICS_STORAGE
 struct pipeline_prepared_draw unknown_storage={0};unknown_storage.draw.pipeline.source=before;uint32_t alignment=UINT32_MAX;
 T(pipeline_storage_alignment_query(&unknown_storage,0,&alignment)==ENOTSUP&&alignment==0);
 T(pipeline_storage_alignment_query(&unknown_storage,1,&alignment)==ENOTSUP);
#endif

 const struct nn_shader_source_entry*owned=nn_shader_source_program_stage(before.binding,1);T(owned&&owned->source_bytes>owned->code_bytes&&!memcmp(owned->source,stage[1]->source,owned->source_bytes));
 read_error=EACCES;ERR(T(!api_nvnProgramSetShaders(&pk,2,shaders)),EACCES);read_error=0;T(pr->pipeline_source.binding==before.binding);
 controls[1][stage[1]->control_bytes-1]^=1;ERR(T(!api_nvnProgramSetShaders(&pk,2,shaders)),ENOTSUP);controls[1][stage[1]->control_bytes-1]^=1;
 ((unsigned char*)storage[1])[stage[1]->code_bytes]^=1;ERR(T(!api_nvnProgramSetShaders(&pk,2,shaders)),ENOTSUP);((unsigned char*)storage[1])[stage[1]->code_bytes]^=1;
 T(pr->pipeline_source.binding==before.binding);
 for(unsigned i=0;i<2;i++){memset(storage[i],0xcc,sizes[i]);memset(controls[i],0xcc,sizeof(controls[i]));OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);storage[i]=NULL;}
 T(!memcmp(owned->source,stage[1]->source,owned->source_bytes));
 NVNtextureBuilder tb={0};NVNtexture target={0};OK(api_nvnTextureBuilderSetDefaults(&tb));OK(api_nvnTextureBuilderSetDevice(&tb,&dk));OK(api_nvnTextureBuilderSetFlags(&tb,NVN_TEXTURE_FLAGS_LINEAR));OK(api_nvnTextureBuilderSetSize2D(&tb,64,64));OK(api_nvnTextureBuilderSetFormat(&tb,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetStride(&tb,256));OK(api_nvnTextureBuilderSetStorage(&tb,pools+TARGET,4096));T(api_nvnTextureInitialize(&target,&tb));
 NVNcommandBuffer cmd={0};_Alignas(16)unsigned char control[2048];T(api_nvnCommandBufferInitialize(&cmd,&dk));OK(api_nvnCommandBufferAddCommandMemory(&cmd,pools+ARENA,0,32768));OK(api_nvnCommandBufferAddControlMemory(&cmd,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&cmd));
 render_defaults(&cmd);OK(api_nvnCommandBufferBindProgram(&cmd,&pk,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
 NVNvertexAttribState attr={0};NVNvertexStreamState stream={0};OK(api_nvnVertexAttribStateSetDefaults(&attr));OK(api_nvnVertexAttribStateSetFormat(&attr,NVN_FORMAT_RG32F,0));OK(api_nvnCommandBufferBindVertexAttribState(&cmd,1,&attr));OK(api_nvnVertexStreamStateSetDefaults(&stream));OK(api_nvnVertexStreamStateSetStride(&stream,8));OK(api_nvnCommandBufferBindVertexStreamState(&cmd,1,&stream));OK(api_nvnCommandBufferBindVertexBuffer(&cmd,0,api_nvnMemoryPoolGetBufferAddress(pools+VBO),4096));
 const NVNtexture*targets[]={&target};OK(api_nvnCommandBufferSetRenderTargets(&cmd,1,targets,NULL,NULL,NULL));OK(api_nvnCommandBufferSetViewport(&cmd,0,0,64,64));OK(api_nvnCommandBufferSetScissor(&cmd,0,0,64,64));OK(api_nvnCommandBufferSetDepthRange(&cmd,0,1));
 OK(api_nvnCommandBufferDrawArrays(&cmd,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));struct recording*rec=find(&cmd,COMMAND)->record;T(rec->current->draws&&rec->current->draws->generic_pipeline&&rec->current->draws->pipeline.source.binding==before.binding);
 NVNcommandHandle h=api_nvnCommandBufferEndRecording(&cmd);T(h&&!nn_nvn_error());ERR(api_nvnProgramFinalize(&pk),EBUSY);
 NVNqueueBuilder qb={0};NVNqueue q={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&q,&qb));unsigned bos=d->gpu->bos;ERR(api_nvnQueueSubmitCommands(&q,1,&h),ENOTSUP);T(d->gpu->bos==bos&&!d->retired);
 OK(api_nvnCommandBufferBeginRecording(&cmd));OK(api_nvnCommandBufferCallCommands(&cmd,1,&h));NVNcommandHandle call=api_nvnCommandBufferEndRecording(&cmd);T(call&&!nn_nvn_error());ERR(api_nvnQueueSubmitCommands(&q,1,&call),ENOTSUP);T(d->gpu->bos==bos&&!d->retired);
 OK(api_nvnCommandBufferBeginRecording(&cmd));OK(api_nvnCommandBufferCopyCommands(&cmd,1,&h));NVNcommandHandle copy=api_nvnCommandBufferEndRecording(&cmd);T(copy&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&dk,h));ERR(api_nvnQueueSubmitCommands(&q,1,&copy),ENOTSUP);T(d->gpu->bos==bos&&!d->retired);
 OK(api_nvnCommandBufferFinalize(&cmd));ERR(api_nvnProgramFinalize(&pk),EBUSY);OK(api_nvnDeviceFinalizeCommandHandle(&dk,call));ERR(api_nvnProgramFinalize(&pk),EBUSY);OK(api_nvnDeviceFinalizeCommandHandle(&dk,copy));OK(api_nvnProgramFinalize(&pk));OK(api_nvnTextureFinalize(&target));OK(api_nvnQueueFinalize(&q));
 for(unsigned i=2;i<COUNT;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(storage[i]);}OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==QB);remove_object(objects);}T(!handles);
 printf("HOST ONLY certified sources accepted; unavailable native direct/Call/Copy rejects=3; checks=%u GPU submissions=0\n",checks);return 0;
}
