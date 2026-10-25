/* Qualification only: public NVN calls, original bounded shader profile and
 * authored BC controls. No internal pipeline encode/BO access/pixel converter. */
static int bc_drop_pool(struct owner *o,unsigned index)
{
 struct pool *p=o->pools+index;if(p->live){o->a.nvnMemoryPoolFinalize(&p->key);int e=nn_nvn_error();if(e)return e;p->live=0;}
 free(p->memory);free(p->shadow);memset(p,0,sizeof(*p));return 0;
}
int nn_nvn_pipeline_probe(const char *directory,char *json,size_t capacity)
{
 if(!directory||!json||capacity<4096)return EINVAL;
 struct owner *o=calloc(1,sizeof(*o));if(!o)return ENOMEM;struct api *a=&o->a;
 int e=0,clean=0;const char *stage="loader";
 unsigned submits=0,finishes=0,completed=0,recorded=0,released=0,replays=0,files=0,maxerror=0;
 unsigned registrations=0,late_registrations=0,cpu_finalizes=0,handle_copies=0,bad_case=UINT32_MAX,bad_x=UINT32_MAX,bad_y=UINT32_MAX,bad_c=UINT32_MAX,bad_actual=0,bad_expected=0;
 uint64_t channels=0,guards=0,readonly=0;size_t bad_byte=SIZE_MAX;char cases[4000]={0};size_t used=0;
#define LOAD(name,type) a->name=(type)nvnBootstrapLoader(#name);if(!a->name){e=ENOSYS;goto done;}
 API_LIST(LOAD)
#undef LOAD
 const struct nn_pipeline_archive *archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 CHECK(NN_BC_PUBLIC_PAIR<archive->pair_count);
 const struct nn_pipeline_pair_recipe *pair=archive->pairs+NN_BC_PUBLIC_PAIR;
 CHECK(pair->recipe&&pair->recipe->texture_mask==2&&pair->recipe->sampler_mask==2&&pair->recipe->input_count==3);
 o->stages[0]=archive->stages->stages+pair->vs_index;o->stages[1]=archive->stages->stages+pair->fs_index;
 stage="device";CALL(a->nvnDeviceBuilderSetDefaults(&o->db));BOOL(a->nvnDeviceInitialize(&o->device,&o->db));o->device_live=1;
 if((e=nn_nvn_register_pipeline_archive(&o->device,archive,reader,o))||
    (e=nn_nvn_register_pipeline_views(&o->device,NN_TEXTURE_NATIVE_VIEWS,NN_TEXTURE_NATIVE_VIEW_COUNT))||
    (e=nn_nvn_register_texture_bridge(&o->device,&NN_TEXTURE_BRIDGE_BUNDLE))||
    (e=nn_nvn_register_block_copy(&o->device,&NN_BC_BLOCK_IMAGE)))goto done;
 CALL(a->nvnDeviceSetWindowOriginMode(&o->device,NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT));
 CALL(a->nvnDeviceSetDepthMode(&o->device,NVN_DEPTH_MODE_NEAR_IS_MINUS_W));
 CALL(a->nvnQueueBuilderSetDefaults(&o->qb));CALL(a->nvnQueueBuilderSetDevice(&o->qb,&o->device));BOOL(a->nvnQueueInitialize(&o->queue,&o->qb));o->queue_live=1;
 if((e=create_pool(o,UNIFORM,4096))||(e=create_pool(o,VERTEX,4096))||(e=create_pool(o,ARENA,65536)))goto done;
 size_t source_bytes=8192;for(unsigned i=0;i<2;i++)source_bytes+=(o->stages[i]->source_bytes+4095u)&~(size_t)4095u;
 if((e=create_pool(o,SOURCE,source_bytes)))goto done;
 for(unsigned i=0,off=4096;i<2;i++){
  memcpy(o->pools[SOURCE].memory+off,o->stages[i]->source,o->stages[i]->source_bytes);
  o->shaders[i]=(NVNshaderData){o->pools[SOURCE].address+off,o->stages[i]->control};off+=(o->stages[i]->source_bytes+4095u)&~4095u;
 }
 stage="source-program";BOOL(a->nvnProgramInitialize(&o->program,&o->device));o->program_live=1;o->reader_enabled=1;
 BOOL(a->nvnProgramSetShaders(&o->program,2,o->shaders));o->reader_enabled=0;
 if((e=bc_drop_pool(o,SOURCE)))goto done;
 float vertices[3][8]={{-1,-1,0,0,1,1,1,1},{3,-1,2,0,1,1,1,1},{-1,3,0,2,1,1,1,1}};
 memcpy(o->pools[VERTEX].memory+256,vertices,sizeof(vertices));
 for(unsigned i=0;i<16;i++)put_float(o->pools[UNIFORM].memory+256+4*i,i%5==0?1.f:0.f);
 for(unsigned i=0;i<16;i++)put_float(o->pools[UNIFORM].memory+512+4*i,i==5||i==6?1.f:0.f);
 stage="target";CALL(a->nvnTextureBuilderSetDefaults(&o->tb));CALL(a->nvnTextureBuilderSetDevice(&o->tb,&o->device));
 CALL(a->nvnTextureBuilderSetFlags(&o->tb,NVN_TEXTURE_FLAGS_LINEAR));CALL(a->nvnTextureBuilderSetSize2D(&o->tb,64,64));
 CALL(a->nvnTextureBuilderSetFormat(&o->tb,NVN_FORMAT_RGBA8));CALL(a->nvnTextureBuilderSetStride(&o->tb,256));
 ptrdiff_t target_payload=a->nvnTextureBuilderGetStorageSize(&o->tb);if((e=nn_nvn_error()))goto done;CHECK(target_payload==16384);
 if((e=create_pool(o,TARGET,24576)))goto done;
 CALL(a->nvnTextureBuilderSetStorage(&o->tb,&o->pools[TARGET].key,4096));BOOL(a->nvnTextureInitialize(&o->texture,&o->tb));o->texture_live=1;
 const unsigned dimensions[3][2]={{8,8},{37,21},{127,65}};
 const unsigned first_format=NVN_NVN_PIPELINE_BC_SRGB?0x45:0x41;
 for(unsigned format=first_format;format<first_format+4;format++)for(unsigned shape=0;shape<3;shape++){
  unsigned test=(format-first_format)*3+shape,width=dimensions[shape][0],height=dimensions[shape][1];bad_case=test*2;
  stage="source-layout";struct nn_tex_source_desc sd={.format=format,.target=1,.width=width,.height=height,.levels=1,.layers=1,.block_height_log2=-1};
  struct nn_tex_source_layout layout;if((e=nn_tex_source_layout(&sd,&layout)))goto done;
  CALL(a->nvnTextureBuilderSetDefaults(&o->sample_builder));CALL(a->nvnTextureBuilderSetDevice(&o->sample_builder,&o->device));
  CALL(a->nvnTextureBuilderSetSize2D(&o->sample_builder,width,height));CALL(a->nvnTextureBuilderSetFormat(&o->sample_builder,(NVNformat)format));
  ptrdiff_t payload=a->nvnTextureBuilderGetStorageSize(&o->sample_builder);if((e=nn_nvn_error()))goto done;CHECK(payload>0&&(uint64_t)payload==layout.payload_bytes);
  size_t pool_bytes=8192+(((size_t)payload+4095)&~(size_t)4095);
  if((e=create_pool(o,COLOR,pool_bytes))||(e=create_pool(o,INDEX,pool_bytes)))goto done;
  o->metadata_flags=(format&1)?0x12:0x14;if((e=create_pool(o,ARGUMENT,32768)))goto done;
  BOOL(a->nvnTexturePoolInitialize(&o->texture_pool,&o->pools[ARGUMENT].key,0,320));o->texture_pool_live=1;
  BOOL(a->nvnSamplerPoolInitialize(&o->sampler_pool,&o->pools[ARGUMENT].key,16384,320));o->sampler_pool_live=1;
  BOOL(a->nvnCommandBufferInitialize(&o->command,&o->device));o->command_live=1;
  CALL(a->nvnCommandBufferAddCommandMemory(&o->command,&o->pools[ARENA].key,0,65536));CALL(a->nvnCommandBufferAddControlMemory(&o->command,o->control,sizeof(o->control)));
  CALL(a->nvnCommandBufferBeginRecording(&o->command));CALL(a->nvnCommandBufferBindProgram(&o->command,&o->program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
  NVNvertexAttribState attrs[3];NVNvertexStreamState streams[8];
  for(unsigned i=0;i<8;i++)CALL(a->nvnVertexStreamStateSetDefaults(streams+i));CALL(a->nvnVertexStreamStateSetStride(streams+7,32));
  for(unsigned i=0;i<3;i++){
   CALL(a->nvnVertexAttribStateSetDefaults(attrs+i));CALL(a->nvnVertexAttribStateSetFormat(attrs+i,i==2?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,i==2?16:i?8:0));CALL(a->nvnVertexAttribStateSetStreamIndex(attrs+i,7));}
  CALL(a->nvnCommandBufferBindVertexAttribState(&o->command,3,attrs));CALL(a->nvnCommandBufferBindVertexStreamState(&o->command,8,streams));
  memset(attrs,0xcc,sizeof(attrs));memset(streams,0xcc,sizeof(streams));
  CALL(a->nvnCommandBufferBindVertexBuffer(&o->command,7,o->pools[VERTEX].address+256,sizeof(vertices)));
  CALL(a->nvnCommandBufferBindUniformBuffer(&o->command,NVN_SHADER_STAGE_VERTEX,0,o->pools[UNIFORM].address+256,64));
  CALL(a->nvnCommandBufferBindUniformBuffer(&o->command,NVN_SHADER_STAGE_FRAGMENT,0,o->pools[UNIFORM].address+512,64));
  const NVNtexture *target=&o->texture;CALL(a->nvnCommandBufferSetRenderTargets(&o->command,1,&target,NULL,NULL,NULL));
  CALL(a->nvnCommandBufferSetViewport(&o->command,0,0,64,64));CALL(a->nvnCommandBufferSetScissor(&o->command,0,0,64,64));CALL(a->nvnCommandBufferSetDepthRange(&o->command,0,1));
  CALL(a->nvnCommandBufferSetTexturePool(&o->command,&o->texture_pool));CALL(a->nvnCommandBufferSetSamplerPool(&o->command,&o->sampler_pool));
  NVNseparateTextureHandle th=a->nvnDeviceGetSeparateTextureHandle(&o->device,257);
  NVNseparateSamplerHandle sh=a->nvnDeviceGetSeparateSamplerHandle(&o->device,259);if((e=nn_nvn_error()))goto done;
  CALL(a->nvnCommandBufferBindSeparateTextures(&o->command,NVN_SHADER_STAGE_FRAGMENT,10,1,&th));
  CALL(a->nvnCommandBufferBindSeparateSamplers(&o->command,NVN_SHADER_STAGE_FRAGMENT,0,1,&sh));memset(&th,0xcc,sizeof(th));memset(&sh,0xcc,sizeof(sh));handle_copies+=2;
  CALL(a->nvnCommandBufferDrawArrays(&o->command,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
  o->handle=a->nvnCommandBufferEndRecording(&o->command);if((e=nn_nvn_error()))goto done;CHECK(o->handle);recorded++;
  int n=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"payload_bytes\":%td,\"source_pool_bytes\":%zu,\"metadata_flags\":%d}",used?",":"",test,format,width,height,payload,pool_bytes,o->metadata_flags);
  CHECK(n>0&&(size_t)n<sizeof(cases)-used);used+=(size_t)n;
  for(unsigned repeat=0;repeat<2;repeat++){
   stage="register";unsigned active=repeat?INDEX:COLOR;
   CALL(a->nvnTextureBuilderSetStorage(&o->sample_builder,&o->pools[active].key,4096));BOOL(a->nvnTextureInitialize(&o->sample,&o->sample_builder));o->sample_live=1;
   CALL(a->nvnTexturePoolRegisterTexture(&o->texture_pool,257,&o->sample,NULL));registrations++;late_registrations++;
   CALL(a->nvnTextureFinalize(&o->sample));o->sample_live=0;cpu_finalizes++;
   if(!repeat){
    CALL(a->nvnSamplerBuilderSetDefaults(&o->sb));CALL(a->nvnSamplerBuilderSetDevice(&o->sb,&o->device));
    CALL(a->nvnSamplerBuilderSetMinMagFilter(&o->sb,NVN_MIN_FILTER_NEAREST,NVN_MAG_FILTER_NEAREST));
    CALL(a->nvnSamplerBuilderSetWrapMode(&o->sb,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE,NVN_WRAP_MODE_CLAMP_TO_EDGE));
    BOOL(a->nvnSamplerInitialize(&o->sampler,&o->sb));o->sampler_live=1;
    CALL(a->nvnSamplerPoolRegisterSampler(&o->sampler_pool,259,&o->sampler));CALL(a->nvnSamplerFinalize(&o->sampler));o->sampler_live=0;cpu_finalizes++;
   }
   for(unsigned i=COLOR;i<=INDEX;i++)memset(o->pools[i].memory,0xa5,o->pools[i].bytes);
   for(unsigned by=0;by<layout.level[0].blocks_y;by++)for(unsigned bx=0;bx<layout.level[0].blocks_x;bx++){
    uint8_t block[16];nn_bc_test_block(format,bx,by,repeat,block);
    for(unsigned byte=0;byte<layout.bytes_per_block;byte++){uint64_t at;if((e=nn_tex_source_offset(&layout,0,0,bx,by,byte,&at)))goto done;o->pools[active].memory[4096+at]=block[byte];}
   }
   memset(o->pools[TARGET].memory,0xa5,o->pools[TARGET].bytes);const unsigned char clear[4]={17,29,43,59};
   for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)memcpy(o->pools[TARGET].memory+4096+y*256+x*4,clear,4);
   for(unsigned i=0;i<=ARGUMENT;i++){
    struct pool *p=o->pools+i;if(!p->shadow){p->shadow=malloc(p->bytes);if(!p->shadow){e=ENOMEM;goto done;}}
    memcpy(p->shadow,p->memory,p->bytes);CALL(a->nvnMemoryPoolFlushMappedRange(&p->key,0,p->bytes));}
   CALL(a->nvnMemoryPoolFlushMappedRange(&o->pools[TARGET].key,0,o->pools[TARGET].bytes));
   stage="submit";o->pending=1;CALL(a->nvnQueueSubmitCommands(&o->queue,1,&o->handle));submits++;
   stage="finish";CALL(a->nvnQueueFinish(&o->queue));o->pending=0;finishes++;
   CALL(a->nvnMemoryPoolInvalidateMappedRange(&o->pools[TARGET].key,0,o->pools[TARGET].bytes));stage="oracle";
   for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
    unsigned sx=((2*x+1)*width)/128,sy=((2*y+1)*height)/128;unsigned char expected[4];nn_bc_test_expected(format,sx,sy,repeat,expected);
    for(unsigned c=0;c<4;c++){unsigned actual=o->pools[TARGET].memory[4096+y*256+x*4+c],delta=actual>expected[c]?actual-expected[c]:expected[c]-actual;
     channels++;if(delta>maxerror)maxerror=delta;if(delta>1){bad_case=test*2+repeat;bad_x=x;bad_y=y;bad_c=c;bad_actual=actual;bad_expected=expected[c];e=EILSEQ;goto save_failure;}}
   }
   stage="guards";for(size_t at=0;at<o->pools[TARGET].bytes;at++)if(at<4096||at>=20480){guards++;if(o->pools[TARGET].memory[at]!=0xa5){bad_byte=at;e=EILSEQ;goto save_failure;}}
   stage="readonly";for(unsigned i=0;i<=ARGUMENT;i++){
    CALL(a->nvnMemoryPoolInvalidateMappedRange(&o->pools[i].key,0,o->pools[i].bytes));
    for(size_t at=0;at<o->pools[i].bytes;at++){readonly++;if(o->pools[i].memory[at]!=o->pools[i].shadow[at]){bad_byte=at;e=EILSEQ;goto save_failure;}}}
   save_failure:;
   int saved=save(directory,test*2+repeat,o->pools+TARGET);if(saved&&!e)e=saved;else if(!saved)files++;
   if(e)goto done;completed++;if(repeat)replays++;
  }
  stage="retire";CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->handle));o->handle=0;released++;
  CALL(a->nvnCommandBufferFinalize(&o->command));o->command_live=0;
  CALL(a->nvnTexturePoolFinalize(&o->texture_pool));o->texture_pool_live=0;CALL(a->nvnSamplerPoolFinalize(&o->sampler_pool));o->sampler_pool_live=0;
  if((e=bc_drop_pool(o,COLOR))||(e=bc_drop_pool(o,INDEX))||(e=bc_drop_pool(o,ARGUMENT)))goto done;
 }
 stage="complete";bad_case=UINT32_MAX;
done:
 nn_nvn_clear_error();if(o->pending&&o->queue_live){a->nvnQueueFinish(&o->queue);if(!nn_nvn_error())o->pending=0;}
 clean=cleanup(o);int retained=clean||o->pending;if(retained){o->next=quarantine;quarantine=o;}else free(o);
 int passed=!e&&!clean&&completed==24;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"nvn_bc_sampled_pipeline\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"submissions\":%u,\"finishes\":%u,\"completed_cases\":%u,\"recorded_handles\":%u,\"released_handles\":%u,\"replays\":%u,\"files\":%u,\"channel_checks\":%llu,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"texture_registrations\":%u,\"registrations_after_record\":%u,\"registered_cpu_object_finalizes\":%u,\"binding_array_copies\":%u,\"maximum_channel_error\":%u,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,\"bad_byte\":%zu,\"bad_actual\":%u,\"bad_expected\":%u,\"target_bytes\":24576,\"target_offset\":4096,\"target_width\":64,\"target_height\":64,\"target_pitch\":256,\"graphics_control_profile_bytes\":2176,\"same_handle_replay\":true,\"public_nvn\":true,\"srgb\":%s,\"cpu_image_conversion\":false,\"game_modules_executed\":false,\"cases\":[%s],\"passed\":%s}\n",
  (unsigned)getuid(),stage,e,clean,retained?"true":"false",submits,finishes,completed,recorded,released,replays,files,(unsigned long long)channels,(unsigned long long)guards,(unsigned long long)readonly,registrations,late_registrations,cpu_finalizes,handle_copies,maxerror,bad_case,bad_x,bad_y,bad_c,bad_byte,bad_actual,bad_expected,NVN_NVN_PIPELINE_BC_SRGB?"true":"false",cases,passed?"true":"false");
 return passed&&n>0&&(size_t)n<capacity?0:e?e:clean?clean:EIO;
}
