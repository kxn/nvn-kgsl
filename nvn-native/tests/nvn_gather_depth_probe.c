/* SPDX-License-Identifier: MIT. Authored public calls with unchanged original
 * CASC Gather+FragDepth code/control/C1; not actual game Draw/frame evidence. */
#define NVN_NVN_PIPELINE_SAMPLED 1
#ifndef NVN_NVN_PIPELINE_CALL
#define NVN_NVN_PIPELINE_CALL 1
#endif
#if NVN_NVN_PIPELINE_CALL != 1
#error "Public CASC qualifier requires shared Call owner helpers"
#endif
#define NVN_NVN_PIPELINE_HELPERS_ONLY 1
#include "nvn_pipeline_probe.c"
#include "nvn_native/pipeline_depth_archive.h"
#ifndef NN_GRAPHICS_PIPELINE_HAS_DEPTH_ARCHIVE
#error "Normal package must explicitly publish exact depth sidecars"
#endif
struct depth_api {
 PFNNVNDEPTHSTENCILSTATESETDEPTHTESTENABLEPROC test;
 PFNNVNDEPTHSTENCILSTATESETDEPTHWRITEENABLEPROC write;
 PFNNVNDEPTHSTENCILSTATESETDEPTHFUNCPROC compare;
 PFNNVNCOMMANDBUFFERCLEARDEPTHSTENCILPROC clear;
 PFNNVNCOMMANDBUFFERCOPYCOMMANDSPROC copy;
};
static int gd_drop(struct owner*o,unsigned index)
{
 struct pool*p=o->pools+index;if(p->live){o->a.nvnMemoryPoolFinalize(&p->key);int e=nn_nvn_error();if(e)return e;p->live=0;}
 free(p->memory);free(p->shadow);memset(p,0,sizeof(*p));return 0;
}
static int gd_controls(struct depth_api*d)
{
 d->test=(PFNNVNDEPTHSTENCILSTATESETDEPTHTESTENABLEPROC)nvnBootstrapLoader("nvnDepthStencilStateSetDepthTestEnable");
 d->write=(PFNNVNDEPTHSTENCILSTATESETDEPTHWRITEENABLEPROC)nvnBootstrapLoader("nvnDepthStencilStateSetDepthWriteEnable");
 d->compare=(PFNNVNDEPTHSTENCILSTATESETDEPTHFUNCPROC)nvnBootstrapLoader("nvnDepthStencilStateSetDepthFunc");
 d->clear=(PFNNVNCOMMANDBUFFERCLEARDEPTHSTENCILPROC)nvnBootstrapLoader("nvnCommandBufferClearDepthStencil");
 d->copy=(PFNNVNCOMMANDBUFFERCOPYCOMMANDSPROC)nvnBootstrapLoader("nvnCommandBufferCopyCommands");
 return d->test&&d->write&&d->compare&&d->clear&&d->copy?0:ENOSYS;
}
int nn_nvn_pipeline_probe(const char*directory,char*json,size_t capacity)
{
 if(!directory||!json||capacity<32768)return EINVAL;
 struct owner*o=calloc(1,sizeof(*o));if(!o)return ENOMEM;struct api*a=&o->a;struct depth_api d={0};
 int e=0,clean=0;const char*stage="loader";unsigned submits=0,finishes=0,completed=0,files=0,recorded=0,released=0,replays=0,registrations=0,finalizes=0,busy=0,copies=0,calls=0;
 uint64_t readonly=0,guards=0;size_t bad_byte=SIZE_MAX;unsigned bad_case=UINT32_MAX;char cases[20000]={0};size_t used=0;unsigned char controls[8192],call_controls[8192];
#define LOAD(name,type) a->name=(type)nvnBootstrapLoader(#name);if(!a->name){e=ENOSYS;goto done;}
 API_LIST(LOAD)
#undef LOAD
 if((e=gd_controls(&d)))goto done;
 const struct nn_pipeline_archive*archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 CHECK(NN_CASC_GATHER_DEPTH_PAIR<archive->pair_count);const struct nn_pipeline_pair_recipe*pair=archive->pairs+NN_CASC_GATHER_DEPTH_PAIR;
 CHECK(pair->vs_index==NN_CASC_GATHER_DEPTH_VS&&pair->fs_index==NN_CASC_GATHER_DEPTH_FS&&pair->recipe->version==2);
 CHECK(NN_GRAPHICS_PIPELINE_DEPTH_ARCHIVE.pairs[NN_CASC_GATHER_DEPTH_PAIR]&&!NN_GRAPHICS_PIPELINE_DEPTH_ARCHIVE.pairs[NN_CASC_GATHER_DEPTH_PAIR]->color_outputs);
 o->stages[0]=archive->stages->stages+pair->vs_index;o->stages[1]=archive->stages->stages+pair->fs_index;
 stage="device";CALL(a->nvnDeviceBuilderSetDefaults(&o->db));BOOL(a->nvnDeviceInitialize(&o->device,&o->db));o->device_live=1;
 if((e=nn_nvn_register_typed_pipeline_archive(&o->device,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,reader,o))||(e=nn_nvn_register_pipeline_depth_archive(&o->device,&NN_GRAPHICS_PIPELINE_DEPTH_ARCHIVE))||(e=nn_nvn_enable_pipeline_runtime_views(&o->device))||(e=nn_nvn_register_block_copy(&o->device,&NN_CASC_GATHER_DEPTH_BLOCK_IMAGE)))goto done;
 CALL(a->nvnDeviceSetDepthMode(&o->device,NVN_DEPTH_MODE_NEAR_IS_MINUS_W));
 CALL(a->nvnQueueBuilderSetDefaults(&o->qb));CALL(a->nvnQueueBuilderSetDevice(&o->qb,&o->device));BOOL(a->nvnQueueInitialize(&o->queue,&o->qb));o->queue_live=1;
 if((e=create_pool(o,UNIFORM,4096))||(e=create_pool(o,VERTEX,4096))||(e=create_pool(o,ARENA,65536)))goto done;
 size_t source_bytes=8192;for(unsigned i=0;i<2;i++)source_bytes+=(o->stages[i]->source_bytes+4095u)&~(size_t)4095u;
 if((e=create_pool(o,SOURCE,source_bytes)))goto done;
 for(unsigned i=0,off=4096;i<2;i++){memcpy(o->pools[SOURCE].memory+off,o->stages[i]->source,o->stages[i]->source_bytes);o->shaders[i]=(NVNshaderData){o->pools[SOURCE].address+off,o->stages[i]->control};off+=(o->stages[i]->source_bytes+4095u)&~4095u;}
 stage="program";BOOL(a->nvnProgramInitialize(&o->program,&o->device));o->program_live=1;o->reader_enabled=1;BOOL(a->nvnProgramSetShaders(&o->program,2,o->shaders));o->reader_enabled=0;
 if((e=gd_drop(o,SOURCE)))goto done; /* cached complete original source/C1 owner */
 const float vertices[3][6]={{-1,-1,.5f,0,0,99},{3,-1,.5f,2,0,99},{-1,3,.5f,0,2,99}};
 memcpy(o->pools[VERTEX].memory+256,vertices,sizeof(vertices));
 for(unsigned test=0;test<NN_CASC_GATHER_DEPTH_CASE_COUNT;test++){
  const struct nn_casc_gather_depth_case*c=NN_CASC_GATHER_DEPTH_CASES+test;
  struct nn_tex_source_desc td={.target=1,.format=c->format,.width=c->width<<c->mip,.height=c->height<<c->mip,.levels=c->mip+1,.layers=1,.block_height_log2=-1},sd={.target=1,.format=10,.width=17,.height=11,.levels=1,.layers=1,.block_height_log2=-1};
  struct nn_tex_source_layout target,source;if((e=nn_tex_source_layout(&td,&target))||(e=nn_tex_source_layout(&sd,&source)))goto done;
  stage="target";CALL(a->nvnDeviceSetWindowOriginMode(&o->device,(NVNwindowOriginMode)c->origin));
  CALL(a->nvnTextureBuilderSetDefaults(&o->tb));CALL(a->nvnTextureBuilderSetDevice(&o->tb,&o->device));CALL(a->nvnTextureBuilderSetFormat(&o->tb,(NVNformat)c->format));CALL(a->nvnTextureBuilderSetSize2D(&o->tb,td.width,td.height));CALL(a->nvnTextureBuilderSetLevels(&o->tb,td.levels));
  ptrdiff_t payload=a->nvnTextureBuilderGetStorageSize(&o->tb);if((e=nn_nvn_error()))goto done;CHECK(payload>0&&(uint64_t)payload==target.payload_bytes);
  if((e=create_pool(o,TARGET,8192+(((size_t)payload+4095)&~(size_t)4095))))goto done;
  CALL(a->nvnTextureBuilderSetStorage(&o->tb,&o->pools[TARGET].key,4096));BOOL(a->nvnTextureInitialize(&o->texture,&o->tb));o->texture_live=1;
  NVNtextureView depth_view;CALL(a->nvnTextureViewSetDefaults(&depth_view));CALL(a->nvnTextureViewSetLevels(&depth_view,c->mip,1));
  if((e=create_pool(o,COLOR,8192+((source.payload_bytes+4095)&~(size_t)4095)))||(e=create_pool(o,INDEX,o->pools[COLOR].bytes)))goto done;
  o->metadata_flags=0x12;if((e=create_pool(o,ARGUMENT,32768)))goto done;
  BOOL(a->nvnTexturePoolInitialize(&o->texture_pool,&o->pools[ARGUMENT].key,0,320));o->texture_pool_live=1;BOOL(a->nvnSamplerPoolInitialize(&o->sampler_pool,&o->pools[ARGUMENT].key,16384,320));o->sampler_pool_live=1;
  CALL(a->nvnTextureBuilderSetDefaults(&o->sample_builder));CALL(a->nvnTextureBuilderSetDevice(&o->sample_builder,&o->device));CALL(a->nvnTextureBuilderSetSize2D(&o->sample_builder,17,11));CALL(a->nvnTextureBuilderSetFormat(&o->sample_builder,NVN_FORMAT_R32F));
  size_t sample_bytes=a->nvnTextureBuilderGetStorageSize(&o->sample_builder);if((e=nn_nvn_error()))goto done;CHECK(sample_bytes==source.payload_bytes);
  BOOL(a->nvnCommandBufferInitialize(&o->command,&o->device));o->command_live=1;CALL(a->nvnCommandBufferAddCommandMemory(&o->command,&o->pools[ARENA].key,0,32768));CALL(a->nvnCommandBufferAddControlMemory(&o->command,o->control,sizeof(o->control)));CALL(a->nvnCommandBufferBeginRecording(&o->command));
  /* Define the same render state in all three recording forms. Call projects
   * explicit state; it cannot inherit unrecorded internal Draw defaults. */
  NVNcolorState color_state;CALL(a->nvnColorStateSetDefaults(&color_state));CALL(a->nvnCommandBufferBindColorState(&o->command,&color_state));
  NVNchannelMaskState channel_state;CALL(a->nvnChannelMaskStateSetDefaults(&channel_state));CALL(a->nvnCommandBufferBindChannelMaskState(&o->command,&channel_state));
  NVNpolygonState polygon_state;CALL(a->nvnPolygonStateSetDefaults(&polygon_state));CALL(a->nvnCommandBufferBindPolygonState(&o->command,&polygon_state));
  NVNmultisampleState multisample_state;CALL(a->nvnMultisampleStateSetDefaults(&multisample_state));CALL(a->nvnCommandBufferBindMultisampleState(&o->command,&multisample_state));
  NVNblendState blend_state;CALL(a->nvnBlendStateSetDefaults(&blend_state));CALL(a->nvnCommandBufferBindBlendState(&o->command,&blend_state));
  CALL(a->nvnCommandBufferSetDepthBounds(&o->command,0,0,1));
  CALL(a->nvnCommandBufferBindProgram(&o->command,&o->program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
  NVNvertexAttribState attrs[4];NVNvertexStreamState streams[8];for(unsigned i=0;i<8;i++)CALL(a->nvnVertexStreamStateSetDefaults(streams+i));CALL(a->nvnVertexStreamStateSetStride(streams+7,24));
  for(unsigned i=0;i<4;i++){CALL(a->nvnVertexAttribStateSetDefaults(attrs+i));if(i==0||i==3){CALL(a->nvnVertexAttribStateSetFormat(attrs+i,i?NVN_FORMAT_RG32F:NVN_FORMAT_RGB32F,i?12:0));CALL(a->nvnVertexAttribStateSetStreamIndex(attrs+i,7));}}
  CALL(a->nvnCommandBufferBindVertexAttribState(&o->command,4,attrs));CALL(a->nvnCommandBufferBindVertexStreamState(&o->command,8,streams));memset(attrs,0xcc,sizeof(attrs));memset(streams,0xcc,sizeof(streams));
  CALL(a->nvnCommandBufferBindVertexBuffer(&o->command,7,o->pools[VERTEX].address+256,sizeof(vertices)));CALL(a->nvnCommandBufferBindUniformBuffer(&o->command,NVN_SHADER_STAGE_VERTEX,3,o->pools[UNIFORM].address+256,64));
  CALL(a->nvnCommandBufferSetRenderTargets(&o->command,0,NULL,NULL,&o->texture,&depth_view));memset(&depth_view,0xcc,sizeof(depth_view));
  CALL(a->nvnCommandBufferSetViewport(&o->command,0,0,c->width,c->height));CALL(a->nvnCommandBufferSetDepthRange(&o->command,0,1));
  CALL(a->nvnCommandBufferSetScissor(&o->command,0,0,c->width,c->height));CALL(d.clear(&o->command,c->compare==5?0.f:1.f,1,0,0));
  CALL(a->nvnCommandBufferSetScissor(&o->command,5,3,c->width-12,c->height/2));
  NVNdepthStencilState depth_state;CALL(a->nvnDepthStencilStateSetDefaults(&depth_state));CALL(d.test(&depth_state,1));CALL(d.write(&depth_state,1));CALL(d.compare(&depth_state,(NVNdepthFunc)c->compare));CALL(a->nvnCommandBufferBindDepthStencilState(&o->command,&depth_state));memset(&depth_state,0xcc,sizeof(depth_state));
  CALL(a->nvnCommandBufferSetTexturePool(&o->command,&o->texture_pool));CALL(a->nvnCommandBufferSetSamplerPool(&o->command,&o->sampler_pool));NVNseparateTextureHandle th=a->nvnDeviceGetSeparateTextureHandle(&o->device,257);NVNseparateSamplerHandle sh=a->nvnDeviceGetSeparateSamplerHandle(&o->device,259);if((e=nn_nvn_error()))goto done;
  CALL(a->nvnCommandBufferBindSeparateTextures(&o->command,NVN_SHADER_STAGE_FRAGMENT,10,1,&th));CALL(a->nvnCommandBufferBindSeparateSamplers(&o->command,NVN_SHADER_STAGE_FRAGMENT,3,1,&sh));memset(&th,0xcc,sizeof(th));memset(&sh,0xcc,sizeof(sh));
  CALL(a->nvnCommandBufferDrawArrays(&o->command,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));o->handle=a->nvnCommandBufferEndRecording(&o->command);if((e=nn_nvn_error()))goto done;CHECK(o->handle);recorded++;
  if(c->wrap){
   o->call_child=o->handle;o->handle=0;BOOL(a->nvnCommandBufferInitialize(&o->call_parent,&o->device));o->call_parent_live=1;CALL(a->nvnCommandBufferAddCommandMemory(&o->call_parent,&o->pools[ARENA].key,32768,32768));CALL(a->nvnCommandBufferAddControlMemory(&o->call_parent,o->call_control,sizeof(o->call_control)));CALL(a->nvnCommandBufferBeginRecording(&o->call_parent));
   if(c->wrap==1){CALL(d.copy(&o->call_parent,1,&o->call_child));copies++;}else{CALL(a->nvnCommandBufferCallCommands(&o->call_parent,1,&o->call_child));calls++;}
   o->handle=a->nvnCommandBufferEndRecording(&o->call_parent);if((e=nn_nvn_error()))goto done;CHECK(o->handle);recorded++;CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->call_child));o->call_child=0;released++;
  }
  CALL(a->nvnCommandBufferFinalize(&o->command));o->command_live=0;
  int n=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"origin\":%u,\"wrap_index\":%u,\"selected_mip\":%u,\"compare\":%u,\"clear_depth_bits\":%u,\"target_pool_bytes\":%zu,\"target_payload\":%llu,\"source_pool_bytes\":%zu}",used?",":"",test,c->format,c->width,c->height,c->origin,c->wrap,c->mip,c->compare,c->compare==5?0u:0x3f800000u,o->pools[TARGET].bytes,(unsigned long long)target.payload_bytes,o->pools[COLOR].bytes);CHECK(n>0&&(size_t)n<sizeof(cases)-used);used+=(size_t)n;
  for(unsigned repeat=0;repeat<2;repeat++){
   unsigned active=repeat?INDEX:COLOR,other=repeat?COLOR:INDEX;bad_case=test*2+repeat;
   stage="register";CALL(a->nvnTextureBuilderSetStorage(&o->sample_builder,&o->pools[active].key,4096));BOOL(a->nvnTextureInitialize(&o->sample,&o->sample_builder));o->sample_live=1;CALL(a->nvnTexturePoolRegisterTexture(&o->texture_pool,257,&o->sample,NULL));registrations++;CALL(a->nvnTextureFinalize(&o->sample));o->sample_live=0;finalizes++;
   if(!repeat){static const NVNwrapMode wraps[3]={NVN_WRAP_MODE_REPEAT,NVN_WRAP_MODE_MIRRORED_REPEAT,NVN_WRAP_MODE_CLAMP_TO_EDGE};CALL(a->nvnSamplerBuilderSetDefaults(&o->sb));CALL(a->nvnSamplerBuilderSetDevice(&o->sb,&o->device));CALL(a->nvnSamplerBuilderSetMinMagFilter(&o->sb,NVN_MIN_FILTER_NEAREST,NVN_MAG_FILTER_NEAREST));CALL(a->nvnSamplerBuilderSetWrapMode(&o->sb,wraps[c->wrap],wraps[c->wrap],wraps[c->wrap]));BOOL(a->nvnSamplerInitialize(&o->sampler,&o->sb));o->sampler_live=1;CALL(a->nvnSamplerPoolRegisterSampler(&o->sampler_pool,259,&o->sampler));CALL(a->nvnSamplerFinalize(&o->sampler));o->sampler_live=0;finalizes++;}
   for(unsigned i=COLOR;i<=INDEX;i++)memset(o->pools[i].memory,0xa5,o->pools[i].bytes);
   for(unsigned y=0;y<11;y++)for(unsigned x=0;x<17;x++){float value=(1+(x+2*y+5*repeat)%7)/8.f;unsigned char bytes[4];memcpy(bytes,&value,4);for(unsigned b=0;b<4;b++){uint64_t at;if((e=nn_tex_source_offset(&source,0,0,x,y,b,&at)))goto done;o->pools[active].memory[4096+at]=bytes[b];}}
   memset(o->pools[UNIFORM].memory+256,0,64);put_float(o->pools[UNIFORM].memory+256+40,(-13+2*(float)repeat)/17.f);put_float(o->pools[UNIFORM].memory+256+44,(-7-2*(float)repeat)/11.f);put_float(o->pools[UNIFORM].memory+256+48,2);put_float(o->pools[UNIFORM].memory+256+52,2);
   memset(o->pools[TARGET].memory,0xa5,o->pools[TARGET].bytes);
   for(unsigned i=0;i<POOLS;i++)if(o->pools[i].live){struct pool*p=o->pools+i;if(i!=TARGET){if(!p->shadow){p->shadow=malloc(p->bytes);if(!p->shadow){e=ENOMEM;goto done;}}memcpy(p->shadow,p->memory,p->bytes);}CALL(a->nvnMemoryPoolFlushMappedRange(&p->key,0,p->bytes));}
   memcpy(controls,o->control,sizeof(controls));memcpy(call_controls,o->call_control,sizeof(call_controls));
   stage="submit";o->pending=1;CALL(a->nvnQueueSubmitCommands(&o->queue,1,&o->handle));submits++;
   /* Replace the live table's same ID only. Already-enqueued jobs must retain
    * the exact earlier canonical source owner and immutable native view. */
   CALL(a->nvnTextureBuilderSetStorage(&o->sample_builder,&o->pools[other].key,4096));BOOL(a->nvnTextureInitialize(&o->sample,&o->sample_builder));o->sample_live=1;CALL(a->nvnTexturePoolRegisterTexture(&o->texture_pool,257,&o->sample,NULL));registrations++;CALL(a->nvnTextureFinalize(&o->sample));o->sample_live=0;finalizes++;memcpy(o->pools[ARGUMENT].shadow+257*32,o->pools[ARGUMENT].memory+257*32,32);
   a->nvnMemoryPoolFinalize(&o->pools[active].key);e=nn_nvn_error();if(e!=EBUSY){if(!e)o->pools[active].live=0;if(!e)e=EPROTO;goto done;}nn_nvn_clear_error();e=0;busy++;
   stage="finish";CALL(a->nvnQueueFinish(&o->queue));o->pending=0;finishes++;
   CALL(a->nvnMemoryPoolInvalidateMappedRange(&o->pools[TARGET].key,0,o->pools[TARGET].bytes));
   for(size_t at=0;at<o->pools[TARGET].bytes;at++)if(at<4096||at>=4096+target.payload_bytes){if(o->pools[TARGET].memory[at]!=0xa5){bad_byte=at;e=EILSEQ;goto done;}guards++;}
   for(unsigned i=0;i<POOLS;i++)if(i!=TARGET&&o->pools[i].live){struct pool*p=o->pools+i;CALL(a->nvnMemoryPoolInvalidateMappedRange(&p->key,0,p->bytes));for(size_t at=0;at<p->bytes;at++){if(p->memory[at]!=p->shadow[at]){bad_byte=at;e=EILSEQ;goto done;}readonly++;}}
   for(size_t at=0;at<sizeof(controls);at++){if(o->control[at]!=controls[at]||o->call_control[at]!=call_controls[at]){bad_byte=at;e=EILSEQ;goto done;}readonly+=2;}
   stage="save";if((e=save(directory,test*2+repeat,o->pools+TARGET)))goto done;files++;completed++;if(repeat)replays++;
  }
  stage="retire";CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->handle));o->handle=0;released++;
  if(o->call_parent_live){CALL(a->nvnCommandBufferFinalize(&o->call_parent));o->call_parent_live=0;}
  CALL(a->nvnTexturePoolFinalize(&o->texture_pool));o->texture_pool_live=0;CALL(a->nvnSamplerPoolFinalize(&o->sampler_pool));o->sampler_pool_live=0;CALL(a->nvnTextureFinalize(&o->texture));o->texture_live=0;
  for(unsigned i=COLOR;i<=ARGUMENT;i++)if((e=gd_drop(o,i)))goto done;
  if((e=gd_drop(o,TARGET)))goto done;
 }
 stage="complete";bad_case=UINT32_MAX;
done:
 nn_nvn_clear_error();clean=cleanup(o);int retained=clean||o->pending;if(retained){o->next=quarantine;quarantine=o;}else free(o);
 int passed=!e&&!clean&&completed==72;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"public_casc_gather_depth\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"submissions\":%u,\"finishes\":%u,\"completed_cases\":%u,\"files\":%u,\"recorded_handles\":%u,\"released_handles\":%u,\"replays\":%u,\"texture_registrations\":%u,\"registered_cpu_finalizes\":%u,\"busy_source_checks\":%u,\"copy_wrappers\":%u,\"call_wrappers\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"bad_case\":%u,\"bad_byte\":%zu,\"public_nvn\":true,\"same_handle_replay\":true,\"source_finalize_before_enqueue\":true,\"late_descriptor_updates\":true,\"cpu_image_conversion\":false,\"game_modules_executed\":false,\"zero_color\":true,\"original_C1_bytes\":256,\"public_depth_mode\":0,\"texture_selector\":10,\"sampler_selector\":3,\"offline_pixel_oracle_required\":true,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),stage,e,clean,retained?"true":"false",submits,finishes,completed,files,recorded,released,replays,registrations,finalizes,busy,copies,calls,(unsigned long long)guards,(unsigned long long)readonly,bad_case,bad_byte,cases,passed?"true":"false");
 return passed&&n>0&&(size_t)n<capacity?0:e?e:clean?clean:EIO;
}
