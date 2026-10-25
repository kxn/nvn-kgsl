/* HOST ONLY real public descriptors + typed native TG4 projection/encoding.
 * No PM4, shader or CPU pixel execution. Authored native source is not NVN source. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include NN_MEMORY_BACKEND_SOURCE
#include NN_TEST_NVN_SOURCE
#include NN_GATHER_ARTIFACT
#include NN_GATHER_VIEWS
#include NN_GATHER_SOURCE_MAP
#include NN_BLOCK_IMAGE
static unsigned source_format,source_levels,view_base,view_levels;
static unsigned checks;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"Gather line%d: %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define ERR(x,e) do{x;T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static void pool(NVNdevice*d,NVNmemoryPoolBuilder*b,NVNmemoryPool*k,void**memory,int flags)
{
 T(!posix_memalign(memory,4096,1048576));memset(*memory,0,1048576);
 OK(api_nvnMemoryPoolBuilderSetDefaults(b));OK(api_nvnMemoryPoolBuilderSetDevice(b,d));OK(api_nvnMemoryPoolBuilderSetStorage(b,*memory,1048576));OK(api_nvnMemoryPoolBuilderSetFlags(b,flags));T(api_nvnMemoryPoolInitialize(k,b));
}
static void texture(NVNdevice*d,NVNmemoryPool*p,NVNtextureBuilder*b,NVNtexture*t,unsigned sampled)
{
 OK(api_nvnTextureBuilderSetDefaults(b));OK(api_nvnTextureBuilderSetDevice(b,d));if(!sampled){OK(api_nvnTextureBuilderSetFlags(b,NVN_TEXTURE_FLAGS_LINEAR));}OK(api_nvnTextureBuilderSetSize2D(b,64,64));OK(api_nvnTextureBuilderSetFormat(b,sampled?(NVNformat)source_format:NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetLevels(b,sampled?source_levels:1));if(!sampled){OK(api_nvnTextureBuilderSetStride(b,256));}OK(api_nvnTextureBuilderSetStorage(b,p,4096));T(api_nvnTextureInitialize(t,b));
}
static struct pipeline_prepared_draw *prepared(struct late_draw *record,struct object *device,const struct nn_tex_native_view *view)
{
 struct pipeline_prepared_draw *p=calloc(1,sizeof(*p));T(p);p->pins=calloc(1,sizeof(*p->pins));T(p->pins);p->recorded=record;p->draw=*record;
 p->draw.pipeline.recipe=&nn_pipeline_recipe;p->draw.pipeline.native.texture_types=&nn_gather_color_types;
 p->draw.pipeline.native.target=(struct nn_pipeline_image){view,{record->target->pool->bo,record->target->offset,record->target->size}};
 p->queries.resources=(struct nn_pipeline_resource_queries){.version=1,.graphics_texture_cb_known=1,.graphics_texture_cb=2,.selected_texture_pool=record->pipeline.texture_pool,.bindings=p->bindings};
 T(device==record->target->device);return p;
}
static void release(struct pipeline_prepared_draw*p){pipeline_pins_release(p->pins);free(p);}
static int one(void)
{
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object*d=find(&dk,DEVICE);
 const struct nn_tex_native_view *view=NULL;for(size_t i=0;i<sizeof(NN_TEXTURE_NATIVE_VIEWS)/sizeof(*NN_TEXTURE_NATIVE_VIEWS);i++)if(NN_TEXTURE_NATIVE_VIEWS[i].width==64&&NN_TEXTURE_NATIVE_VIEWS[i].height==64)view=NN_TEXTURE_NATIVE_VIEWS+i;T(view);
 d->pipeline_views=view;d->pipeline_view_count=1;T(!nn_nvn_register_block_copy(&dk,&NVN_COMPILED_IMAGE));
 NVNmemoryPoolBuilder pb[5];NVNmemoryPool pools[5];void*memory[5];memset(pb,0,sizeof(pb));memset(pools,0,sizeof(pools));for(unsigned i=0;i<5;i++)pool(&dk,pb+i,pools+i,memory+i,i==3?0x14:0x24);
 NVNtextureBuilder tb[3];NVNtexture tk[3];memset(tb,0,sizeof(tb));memset(tk,0,sizeof(tk));texture(&dk,pools,tb,tk,0);texture(&dk,pools+1,tb+1,tk+1,1);texture(&dk,pools+2,tb+2,tk+2,1);
 NVNtexturePool tp={0};NVNsamplerPool sp={0};T(api_nvnTexturePoolInitialize(&tp,pools+3,0,320));T(api_nvnSamplerPoolInitialize(&sp,pools+3,16384,320));NVNtextureView selection={0};OK(api_nvnTextureViewSetDefaults(&selection));OK(api_nvnTextureViewSetLevels(&selection,view_base,view_levels));OK(api_nvnTexturePoolRegisterTexture(&tp,257,tk+1,&selection));
 NVNsamplerBuilder sb={0};OK(api_nvnSamplerBuilderSetDefaults(&sb));OK(api_nvnSamplerBuilderSetDevice(&sb,&dk));OK(api_nvnSamplerBuilderSetMinMagFilter(&sb,NVN_MIN_FILTER_NEAREST,NVN_MAG_FILTER_NEAREST));OK(api_nvnSamplerBuilderSetWrapMode(&sb,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE));OK(api_nvnSamplerPoolRegisterSamplerBuilder(&sp,289,&sb));
 NVNcommandBuffer cb={0};_Alignas(16)unsigned char control[1024];T(api_nvnCommandBufferInitialize(&cb,&dk));OK(api_nvnCommandBufferAddCommandMemory(&cb,pools+4,0,16384));OK(api_nvnCommandBufferAddControlMemory(&cb,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&cb));OK(api_nvnCommandBufferSetTexturePool(&cb,&tp));OK(api_nvnCommandBufferSetSamplerPool(&cb,&sp));OK(api_nvnCommandBufferBindSeparateTexture(&cb,NVN_SHADER_STAGE_FRAGMENT,10,api_nvnDeviceGetSeparateTextureHandle(&dk,257)));OK(api_nvnCommandBufferBindSeparateSampler(&cb,NVN_SHADER_STAGE_FRAGMENT,3,api_nvnDeviceGetSeparateSamplerHandle(&dk,289)));
 struct recording *r=find(&cb,COMMAND)->record;struct late_draw record={.target=find(tk,TEXTURE)};record.pipeline.texture_pool=r->texture_pool;record.pipeline.sampler_pool=r->sampler_pool;
 memcpy(record.pipeline.separate,r->separate_textures,sizeof(record.pipeline.separate));memcpy(record.pipeline.guest,r->handle_carriers[1].guest,sizeof(record.pipeline.guest));memcpy(record.pipeline.defined,r->handle_carriers[1].low_defined,sizeof(record.pipeline.defined));memcpy(record.pipeline.samplers,r->separate_samplers,sizeof(record.pipeline.samplers));memcpy(record.pipeline.sampler_guest,r->handle_carriers[2].guest,sizeof(record.pipeline.sampler_guest));memcpy(record.pipeline.sampler_defined,r->handle_carriers[2].low_defined,sizeof(record.pipeline.sampler_defined));
 struct nn_pipeline_resource_reference ref;T(!nn_pipeline_resource_reference(NN_GATHER_SOURCE_BINDING.handle_word,NN_GATHER_SOURCE_BINDING.cbuf_slot,&ref));T(ref.texture_selector==10&&ref.sampler_selector==3&&ref.has_sampler&&NN_GATHER_SOURCE_BINDING.binding==1);
 size_t allocations=mock_allocations,caches=mock_caches;struct pipeline_prepared_draw*p=prepared(&record,d,view);int e=pipeline_native_sample(d,p,1,&NN_GATHER_SOURCE_BINDING);
#if !NN_PIPELINE_GATHER
 T(e==ENOTSUP&&!p->entry_count&&!mock_submits&&mock_allocations==allocations&&mock_caches==caches);
 struct nn_pipeline_draw_state state={.texture_types=&nn_gather_color_types,.texture_bound=2,.sampler_bound=2};
 state.target=(struct nn_pipeline_image){view,{find(pools,POOL)->bo,4096,16384}};state.textures[1]=(struct nn_pipeline_image){view,{find(pools+1,POOL)->bo,4096,16384}};state.sampler_layouts[1]=(struct nn_sampler_layout){.width=64,.height=64,.levels=1,.samples=1,.dimensions=2,.normalized=1};T(!nn_pipeline_full_raster(64,64,0,0,&state.raster));
 struct nn_offline_graphics_stage sv[2]={{.spirv=(const unsigned char*)nn_gather_color_vs,.spirv_bytes=sizeof(nn_gather_color_vs)},{.spirv=(const unsigned char*)nn_gather_color_fs,.spirv_bytes=sizeof(nn_gather_color_fs)}};const struct nn_offline_graphics_stage*stages[]={sv,sv+1};T(nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,&state,0)==ENOTSUP);T(mock_allocations==allocations&&mock_caches==caches);release(p);
#else
 T(!e&&p->entry_count==2&&p->sampler_count==1&&p->sampler_ids[0]==289);T(p->bindings[0].selector==10&&p->bindings[1].selector==3);T(p->native_entries[1]->pool==find(pools+1,POOL));T(p->draw.pipeline.native.samplers[1].min_filter==NVN_MIN_FILTER_NEAREST);T(mock_allocations==allocations&&mock_caches==caches&&!mock_submits);
 /* Source public texture lifetime and immutable descriptor versions are real;
  * an old prepared enqueue retains its pool and sampler snapshot. */
 OK(api_nvnTextureFinalize(tk+1));OK(api_nvnTexturePoolRegisterTexture(&tp,257,tk+2,&selection));OK(api_nvnSamplerBuilderSetLodBias(&sb,1));OK(api_nvnSamplerPoolRegisterSamplerBuilder(&sp,289,&sb));
 struct pipeline_prepared_draw*newer=prepared(&record,d,view);T(!pipeline_native_sample(d,newer,1,&NN_GATHER_SOURCE_BINDING));T(newer->native_entries[1]->pool==find(pools+2,POOL));T(newer->draw.pipeline.native.samplers[1].lod_bias==1);T(p->native_entries[1]->pool==find(pools+1,POOL)&&p->draw.pipeline.native.samplers[1].min_filter==NVN_MIN_FILTER_NEAREST);ERR(api_nvnMemoryPoolFinalize(pools+1),EBUSY);
 /* Malformed/unknown other and stale typed facts fail before capture/BO work. */
 struct nn_offline_texture_binding map=NN_GATHER_SOURCE_BINDING;struct nn_pipeline_texture_types bad=nn_gather_color_types;
 for(unsigned k=0;k<8;k++){
  struct pipeline_prepared_draw*q=prepared(&record,d,view);map=NN_GATHER_SOURCE_BINDING;bad=nn_gather_color_types;
  if(k==0){map.other++;}if(k==1){map.offsets=1;}if(k==2){bad.gathers[1].count=0;}if(k==3){bad.gathers[1].sampler_mask=8;}if(k==4){bad.gathers[1].component_mask=0;}if(k==5){bad.dimensions[1]=3;}if(k==6){bad.dimensions[1]=1;}if(k==7){bad.operations[1]=8;}
  q->draw.pipeline.native.texture_types=&bad;allocations=mock_allocations;caches=mock_caches;T(pipeline_native_sample(d,q,1,&map)==ENOTSUP);T(!q->entry_count&&mock_allocations==allocations&&mock_caches==caches&&!mock_submits);release(q);
 }
 /* Actual normal native TG4 code is encoded using the captured sampler and
  * texture, never stale sample bytes or a CPU implementation of gather. */
 struct nn_sampled_image_shadow *shadow=NULL;
 const struct descriptor_entry *entry=p->native_entries[1];
 struct nn_block_copy_range canonical={entry->pool->bo,entry->offset,entry->size,entry->offset};
 T(p->color_images==2&&p->color_plan[1].count==source_levels);
 T(!nn_sampled_image_shadow_create(d->gpu,d->block_copy,entry->source_layout,p->color_plan+1,&canonical,&shadow)&&shadow);
 struct nn_sampled_image_binding actual;T(!nn_sampled_image_shadow_binding(shadow,&actual));
 p->draw.pipeline.native.textures[1].buffer=(struct nn_pipeline_buffer){actual.bo,actual.offset,actual.bytes};
 nn_bo *resources[64];size_t resource_count=0;T(!nn_sampled_image_shadow_resources(shadow,resources,64,&resource_count)&&resource_count==3+2*source_levels);
 nn_pipeline_program*program=NULL,*retained=NULL;T(!nn_pipeline_program_create_native(d->gpu,&NN_GATHER_COLOR_ARTIFACT,&program,&retained)&&program&&!retained);
 struct nn_pipeline_stage_view sv[2];T(!nn_pipeline_program_stage(program,0,sv));T(!nn_pipeline_program_stage(program,1,sv+1));const struct nn_offline_graphics_stage*stages[]={sv[0].source,sv[1].source};
 struct nn_pipeline_draw_state *state=&p->draw.pipeline.native;state->texture_bound=2;state->sampler_bound=2;nn_render_state_defaults(&state->render);T(!nn_pipeline_full_raster(64,64,0,0,&state->raster));state->draw=(struct nn_gfx_draw){.primitive=4,.count=3,.instances=1};
 allocations=mock_allocations;caches=mock_caches;T(!nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,state,0));T(mock_allocations==allocations&&mock_caches==caches);
 struct nn_pipeline_submission submit;nn_bo*quarantine=NULL;T(!nn_pipeline_encode(d->gpu,program,&nn_pipeline_recipe,&nn_pipeline_device,state,&submit,&quarantine)&&submit.job&&!quarantine);T(submit.command_bytes&&submit.resource_count>=3);T(!mock_submits);T(!nn_bo_destroy(submit.job));
 /* Both common preflight and actual sampler emission must use the role's
  * gated encoder. Legacy multi-mip still rejects this richer sampler. */
 struct nn_sampler_state saved_sampler=state->samplers[1];state->samplers[1].min_filter=NVN_MIN_FILTER_LINEAR_MIPMAP_LINEAR;state->samplers[1].mag_filter=NVN_MAG_FILTER_LINEAR;
 for(unsigned j=0;j<3;j++)state->samplers[1].wrap[j]=NVN_WRAP_MODE_REPEAT;
 int richer=nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,state,0);
 #if NN_PIPELINE_SAMPLED_MIP_FILTER
 T(!richer);
 #else
 T(richer==(view_levels==1?0:ENOTSUP));
 #endif
 if(!richer){T(!nn_pipeline_encode(d->gpu,program,&nn_pipeline_recipe,&nn_pipeline_device,state,&submit,&quarantine)&&submit.job&&!quarantine);T(!nn_bo_destroy(submit.job));}
 state->samplers[1]=saved_sampler;
 /* Valid typed metadata cannot open BUFFER/3D/BC or unnormalized resource paths. */
 struct nn_fdl_volume_view volume={0};state->sampled_volume[1]=&volume;T(nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,state,0)==EINVAL);const struct nn_fdl_image_view *color=state->sampled_color[1];state->sampled_color[1]=NULL;T(nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,state,0)==ENOTSUP);state->sampled_color[1]=color;state->sampled_volume[1]=NULL;
 struct nn_fdl_image_view bc={0};state->sampled_fdl[1]=&bc;T(nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,state,0)==EINVAL);state->sampled_color[1]=NULL;T(nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,state,0)==ENOTSUP);state->sampled_color[1]=color;state->sampled_fdl[1]=NULL;
 struct nn_buffer_sampled_binding buf={0};state->sampled_buffer[1]=&buf;T(nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,state,0)==EINVAL);state->sampled_color[1]=NULL;T(nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,state,0)==ENOTSUP);state->sampled_color[1]=color;state->sampled_buffer[1]=NULL;state->sampler_layouts[1].normalized=0;T(nn_pipeline_preflight(d->gpu,stages,&nn_pipeline_recipe,&nn_pipeline_device,state,0)==ENOTSUP);
 T(!nn_pipeline_program_destroy(program));T(!nn_sampled_image_shadow_destroy(&shadow)&&!shadow);release(newer);release(p);
#endif
 NVNcommandHandle handle=api_nvnCommandBufferEndRecording(&cb);T(handle&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&dk,handle));OK(api_nvnCommandBufferFinalize(&cb));OK(api_nvnTexturePoolFinalize(&tp));OK(api_nvnSamplerPoolFinalize(&sp));for(unsigned i=0;i<3;i++)if(!NN_PIPELINE_GATHER||i!=1)OK(api_nvnTextureFinalize(tk+i));
 nn_nvn_clear_error();for(unsigned i=0;i<5;i++){OK(api_nvnMemoryPoolFinalize(pools+i));free(memory[i]);}OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB||objects->kind==PB||objects->kind==TB||objects->kind==SB);remove_object(objects);}T(!handles&&!mock_submits);
 printf("HOST Gather FLOAT2D format%u mips%u view%u+%u checks=%u enabled=%u actual_GPU_submits=0; native1 -> publicT10/S3 snapshot\n",source_format,source_levels,view_base,view_levels,checks,NN_PIPELINE_GATHER);return 0;
}

int main(void)
{
 unsigned formats=0;
 for(unsigned f=0;f<200;f++){
  struct nn_fdl_format_info info;
  if(nn_fdl_image_format_info(0x06050002,f,&info)||info.result_type!=NN_FDL_RESULT_FLOAT||info.block_width!=1||info.block_height!=1)continue;
  formats++;
  for(unsigned scenario=0;scenario<4;scenario++){
   source_format=f;source_levels=scenario==0?1:scenario==3?7:4;view_base=scenario==2?1:scenario==3?4:0;
   view_levels=scenario==2?2:scenario==3?3:source_levels;one();
  }
 }
 T(formats==22);return 0;
}
