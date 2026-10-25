/* Authored qualification caller of PUBLIC NVN only. Original C1 unchanged;
 * real RGBA8 index image R=0 intentionally restricts observed indices to0. */
static int buffer_register(struct owner*o,unsigned pool,unsigned format)
{
 struct api*a=&o->a;int e;
 a->nvnTextureBuilderSetStorage(&o->buffer_builder,&o->pools[pool].key,4096+nn_public_buffer_cpp(format));if((e=nn_nvn_error()))return e;
 int ok=a->nvnTextureInitialize(&o->buffer,&o->buffer_builder);if((e=nn_nvn_error()))return e;if(!ok)return EIO;o->buffer_live=1;
 a->nvnTexturePoolRegisterTexture(&o->texture_pool,258,&o->buffer,NULL);if((e=nn_nvn_error()))return e;
 a->nvnTextureFinalize(&o->buffer);if((e=nn_nvn_error()))return e;o->buffer_live=0;return 0;
}
static int buffer_save(const char*directory,unsigned number,struct owner*o)
{
 char name[1400];int n=snprintf(name,sizeof(name),"%s/nvn-pipeline-%02u.raw",directory,number);if(n<0||(size_t)n>=sizeof(name))return ENAMETOOLONG;
 int fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;
 const unsigned order[]={TARGET,COLOR,INDEX,BUFFER_ALT};int e=0;
 for(unsigned k=0;k<4&&!e;k++){struct pool*p=o->pools+order[k];size_t at=0;while(at<p->bytes){ssize_t w=write(fd,p->memory+at,p->bytes-at);if(w<0&&errno==EINTR)continue;if(w<=0){e=w<0?errno:EIO;break;}at+=(size_t)w;}}
 if(!e&&fsync(fd))e=errno;
 if(close(fd)&&!e)e=errno;
 return e;
}
int nn_nvn_pipeline_probe(const char *directory,char *json,size_t capacity)
{
 if(!directory||!json||capacity<4096)return EINVAL;
 struct owner*o=calloc(1,sizeof(*o));if(!o)return ENOMEM;struct api*a=&o->a;
 int e=0,clean=0;const char*stage="loader";unsigned submits=0,finishes=0,completed=0,files=0,recorded=0,released=0,registrations=0,finalizes=0,busy_rejects=0;
 unsigned copies=0,calls=0,replays=0,shader_finalizes=0;
 unsigned maxerror=0,bad_case=UINT32_MAX,bad_x=UINT32_MAX,bad_y=UINT32_MAX,bad_c=UINT32_MAX,bad_actual=0,bad_expected=0;uint64_t channels=0,guards=0,readonly=0;size_t bad_byte=SIZE_MAX;
#define LOAD(name,type) a->name=(type)nvnBootstrapLoader(#name);if(!a->name){e=ENOSYS;goto done;}
 API_LIST(LOAD)
#undef LOAD
 const struct nn_pipeline_archive*archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;CHECK(NN_PUBLIC_BUFFER_PAIR<archive->pair_count);
 const struct nn_pipeline_pair_recipe*pair=archive->pairs+NN_PUBLIC_BUFFER_PAIR;
 CHECK(pair->recipe&&pair->recipe->texture_mask==6&&!pair->recipe->sampler_mask&&pair->recipe->input_count==3);
 o->stages[0]=archive->stages->stages+pair->vs_index;o->stages[1]=archive->stages->stages+pair->fs_index;
 CHECK(o->stages[1]->source_bytes-o->stages[1]->code_bytes==256);
 stage="device";CALL(a->nvnDeviceBuilderSetDefaults(&o->db));BOOL(a->nvnDeviceInitialize(&o->device,&o->db));o->device_live=1;
 if((e=nn_nvn_register_typed_pipeline_archive(&o->device,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,reader,o))||(e=nn_nvn_register_pipeline_views(&o->device,NN_TEXTURE_NATIVE_VIEWS+NN_PUBLIC_BUFFER_VIEW_INDEX,1)))goto done;
 CALL(a->nvnDeviceSetWindowOriginMode(&o->device,NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT));CALL(a->nvnDeviceSetDepthMode(&o->device,NVN_DEPTH_MODE_NEAR_IS_MINUS_W));
 CALL(a->nvnQueueBuilderSetDefaults(&o->qb));CALL(a->nvnQueueBuilderSetDevice(&o->qb,&o->device));BOOL(a->nvnQueueInitialize(&o->queue,&o->qb));o->queue_live=1;
 o->metadata_flags=0x14;
 const size_t sizes[POOLS]={4096,4096,24576,8192,32768,8192,65536,24576,8192};
 for(unsigned i=0;i<POOLS;i++)if((e=create_pool(o,i,sizes[i])))goto done;
 for(unsigned i=0,off=4096;i<2;i++){CHECK(off+o->stages[i]->source_bytes<=o->pools[SOURCE].bytes);memcpy(o->pools[SOURCE].memory+off,o->stages[i]->source,o->stages[i]->source_bytes);o->shaders[i]=(NVNshaderData){o->pools[SOURCE].address+off,o->stages[i]->control};off+=o->stages[i]->source_bytes;off=(off+255)&~255u;}
 stage="source-program";BOOL(a->nvnProgramInitialize(&o->program,&o->device));o->program_live=1;o->reader_enabled=1;BOOL(a->nvnProgramSetShaders(&o->program,2,o->shaders));o->reader_enabled=0;
 /* Actual Program source/C1 snapshot outlives caller shader-pool storage. */
 CALL(a->nvnMemoryPoolFinalize(&o->pools[SOURCE].key));o->pools[SOURCE].live=0;shader_finalizes++;free(o->pools[SOURCE].memory);o->pools[SOURCE].memory=NULL;
 const float vertices[3][8]={{-1,-1,.125f,.125f,.25f,.375f,.5f,1},{3,-1,1.625f,.125f,.75f,.375f,.75f,1},{-1,3,.125f,1.625f,.25f,.875f,.75f,1}};
 memcpy(o->pools[VERTEX].memory+256,vertices,sizeof(vertices));
 for(unsigned i=0;i<16;i++){put_float(o->pools[UNIFORM].memory+256+4*i,i%5==0?1:0);put_float(o->pools[UNIFORM].memory+512+4*i,i==5||i==6?1:0);}
 stage="target";CALL(a->nvnTextureBuilderSetDefaults(&o->tb));CALL(a->nvnTextureBuilderSetDevice(&o->tb,&o->device));CALL(a->nvnTextureBuilderSetFlags(&o->tb,NVN_TEXTURE_FLAGS_LINEAR));CALL(a->nvnTextureBuilderSetSize2D(&o->tb,64,64));CALL(a->nvnTextureBuilderSetFormat(&o->tb,NVN_FORMAT_RGBA8));CALL(a->nvnTextureBuilderSetStride(&o->tb,256));CHECK(a->nvnTextureBuilderGetStorageSize(&o->tb)==16384);if((e=nn_nvn_error()))goto done;CALL(a->nvnTextureBuilderSetStorage(&o->tb,&o->pools[TARGET].key,4096));BOOL(a->nvnTextureInitialize(&o->texture,&o->tb));o->texture_live=1;
 BOOL(a->nvnTexturePoolInitialize(&o->texture_pool,&o->pools[ARGUMENT].key,0,320));o->texture_pool_live=1;
 for(unsigned test=0;test<NN_PUBLIC_BUFFER_FORMATS;test++){
  unsigned format=nn_public_buffer_formats[test];bad_case=test*4;stage="record";
  BOOL(a->nvnCommandBufferInitialize(&o->command,&o->device));o->command_live=1;CALL(a->nvnCommandBufferAddCommandMemory(&o->command,&o->pools[ARENA].key,0,32768));CALL(a->nvnCommandBufferAddControlMemory(&o->command,o->control,sizeof(o->control)));CALL(a->nvnCommandBufferBeginRecording(&o->command));
  CALL(a->nvnCommandBufferBindProgram(&o->command,&o->program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
  NVNcolorState color;NVNchannelMaskState mask;NVNpolygonState poly;NVNdepthStencilState depth;NVNmultisampleState multi;NVNblendState blend;
  CALL(a->nvnColorStateSetDefaults(&color));CALL(a->nvnChannelMaskStateSetDefaults(&mask));CALL(a->nvnPolygonStateSetDefaults(&poly));CALL(a->nvnDepthStencilStateSetDefaults(&depth));CALL(a->nvnMultisampleStateSetDefaults(&multi));CALL(a->nvnBlendStateSetDefaults(&blend));
  CALL(a->nvnCommandBufferBindColorState(&o->command,&color));CALL(a->nvnCommandBufferBindChannelMaskState(&o->command,&mask));CALL(a->nvnCommandBufferBindPolygonState(&o->command,&poly));CALL(a->nvnCommandBufferBindDepthStencilState(&o->command,&depth));CALL(a->nvnCommandBufferBindMultisampleState(&o->command,&multi));CALL(a->nvnCommandBufferBindBlendState(&o->command,&blend));CALL(a->nvnCommandBufferSetDepthBounds(&o->command,0,0,1));
  NVNvertexAttribState attrs[3];NVNvertexStreamState streams[8];for(unsigned i=0;i<8;i++)CALL(a->nvnVertexStreamStateSetDefaults(streams+i));CALL(a->nvnVertexStreamStateSetStride(streams+7,32));for(unsigned i=0;i<3;i++){CALL(a->nvnVertexAttribStateSetDefaults(attrs+i));CALL(a->nvnVertexAttribStateSetFormat(attrs+i,i==2?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,i==2?16:i?8:0));CALL(a->nvnVertexAttribStateSetStreamIndex(attrs+i,7));}CALL(a->nvnCommandBufferBindVertexAttribState(&o->command,3,attrs));CALL(a->nvnCommandBufferBindVertexStreamState(&o->command,8,streams));memset(attrs,0xcc,sizeof(attrs));memset(streams,0xcc,sizeof(streams));
  CALL(a->nvnCommandBufferBindVertexBuffer(&o->command,7,o->pools[VERTEX].address+256,sizeof(vertices)));CALL(a->nvnCommandBufferBindUniformBuffer(&o->command,NVN_SHADER_STAGE_VERTEX,0,o->pools[UNIFORM].address+256,64));CALL(a->nvnCommandBufferBindUniformBuffer(&o->command,NVN_SHADER_STAGE_FRAGMENT,0,o->pools[UNIFORM].address+512,64));const NVNtexture*target=&o->texture;CALL(a->nvnCommandBufferSetRenderTargets(&o->command,1,&target,NULL,NULL,NULL));CALL(a->nvnCommandBufferSetViewport(&o->command,0,0,64,64));CALL(a->nvnCommandBufferSetScissor(&o->command,0,0,64,64));CALL(a->nvnCommandBufferSetDepthRange(&o->command,0,1));CALL(a->nvnCommandBufferSetTexturePool(&o->command,&o->texture_pool));NVNseparateTextureHandle bindings[2]={a->nvnDeviceGetSeparateTextureHandle(&o->device,257),a->nvnDeviceGetSeparateTextureHandle(&o->device,258)};if((e=nn_nvn_error()))goto done;CALL(a->nvnCommandBufferBindSeparateTextures(&o->command,NVN_SHADER_STAGE_FRAGMENT,10,2,bindings));memset(bindings,0xcc,sizeof(bindings));CALL(a->nvnCommandBufferDrawArrays(&o->command,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));o->call_child=a->nvnCommandBufferEndRecording(&o->command);if((e=nn_nvn_error()))goto done;CHECK(o->call_child);recorded++;
  CALL(a->nvnCommandBufferBeginRecording(&o->command));CALL(a->nvnCommandBufferCopyCommands(&o->command,1,&o->call_child));copies++;o->copied=a->nvnCommandBufferEndRecording(&o->command);if((e=nn_nvn_error()))goto done;CHECK(o->copied);recorded++;
  BOOL(a->nvnCommandBufferInitialize(&o->call_parent,&o->device));o->call_parent_live=1;CALL(a->nvnCommandBufferAddCommandMemory(&o->call_parent,&o->pools[ARENA].key,32768,32768));CALL(a->nvnCommandBufferAddControlMemory(&o->call_parent,o->call_control,sizeof(o->call_control)));CALL(a->nvnCommandBufferBeginRecording(&o->call_parent));CALL(a->nvnCommandBufferCallCommands(&o->call_parent,1,&o->call_child));calls++;o->handle=a->nvnCommandBufferEndRecording(&o->call_parent);if((e=nn_nvn_error()))goto done;CHECK(o->handle);recorded++;CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->call_child));o->call_child=0;released++;
  /* Both descriptors are real and first registered AFTER all recordings. */
  CALL(a->nvnTextureBuilderSetDefaults(&o->sample_builder));CALL(a->nvnTextureBuilderSetDevice(&o->sample_builder,&o->device));CALL(a->nvnTextureBuilderSetFlags(&o->sample_builder,NVN_TEXTURE_FLAGS_LINEAR));CALL(a->nvnTextureBuilderSetSize2D(&o->sample_builder,64,64));CALL(a->nvnTextureBuilderSetFormat(&o->sample_builder,NVN_FORMAT_RGBA8));CALL(a->nvnTextureBuilderSetStride(&o->sample_builder,256));CALL(a->nvnTextureBuilderSetStorage(&o->sample_builder,&o->pools[COLOR].key,4096));BOOL(a->nvnTextureInitialize(&o->sample,&o->sample_builder));o->sample_live=1;CALL(a->nvnTexturePoolRegisterTexture(&o->texture_pool,257,&o->sample,NULL));registrations++;CALL(a->nvnTextureFinalize(&o->sample));o->sample_live=0;finalizes++;
  CALL(a->nvnTextureBuilderSetDefaults(&o->buffer_builder));CALL(a->nvnTextureBuilderSetDevice(&o->buffer_builder,&o->device));CALL(a->nvnTextureBuilderSetTarget(&o->buffer_builder,NVN_TEXTURE_TARGET_BUFFER));CALL(a->nvnTextureBuilderSetSize1D(&o->buffer_builder,17));CALL(a->nvnTextureBuilderSetFormat(&o->buffer_builder,(NVNformat)format));CHECK(a->nvnTextureBuilderGetStorageSize(&o->buffer_builder)==17u*nn_public_buffer_cpp(format));CHECK(a->nvnTextureBuilderGetStorageAlignment(&o->buffer_builder)==nn_public_buffer_cpp(format));if((e=nn_nvn_error()))goto done;
  for(unsigned phase=0;phase<4;phase++){
   stage="prepare";bad_case=test*4+phase;
   if(!(phase&1)){
    for(size_t at=0;at<8192;at++){o->pools[INDEX].memory[at]=nn_public_buffer_byte(format,phase,at);o->pools[BUFFER_ALT].memory[at]=nn_public_buffer_byte(format,phase+1,at);}
    memset(o->pools[COLOR].memory,0xa5,24576);for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){unsigned char*pixel=o->pools[COLOR].memory+4096+y*256+x*4;pixel[0]=0;pixel[1]=(unsigned char)(x+phase);pixel[2]=(unsigned char)(y+phase);pixel[3]=255;}
    put_float(o->pools[UNIFORM].memory+512+28,phase>=2?1:0);
    if((e=buffer_register(o,INDEX,format)))goto done;
    registrations++;finalizes++;
   }
   memset(o->pools[TARGET].memory,0xa5,24576);const unsigned char clear[4]={17,29,43,59};for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)memcpy(o->pools[TARGET].memory+4096+y*256+x*4,clear,4);
   for(unsigned i=0;i<POOLS;i++)if(o->pools[i].live){struct pool*p=o->pools+i;if(i!=TARGET){if(!p->shadow){p->shadow=malloc(p->bytes);if(!p->shadow){e=ENOMEM;goto done;}}memcpy(p->shadow,p->memory,p->bytes);}CALL(a->nvnMemoryPoolFlushMappedRange(&p->key,0,p->bytes));}
   NVNcommandHandle selected=(phase&1)?o->handle:o->copied;stage="submit";o->pending=1;CALL(a->nvnQueueSubmitCommands(&o->queue,1,&selected));submits++;
   /* Replace the mutable slot while the COPY version is in flight. Actual
    * native private descriptor/old pool owners must still read INDEX. */
   if(!(phase&1)){if((e=buffer_register(o,BUFFER_ALT,format)))goto done;registrations++;finalizes++;memcpy(o->pools[ARGUMENT].shadow,o->pools[ARGUMENT].memory,o->pools[ARGUMENT].bytes);}
   unsigned active=(phase&1)?BUFFER_ALT:INDEX;a->nvnMemoryPoolFinalize(&o->pools[active].key);CHECK(nn_nvn_error()==EBUSY);nn_nvn_clear_error();busy_rejects++;a->nvnTexturePoolFinalize(&o->texture_pool);CHECK(nn_nvn_error()==EBUSY);nn_nvn_clear_error();busy_rejects++;
   stage="finish";CALL(a->nvnQueueFinish(&o->queue));o->pending=0;finishes++;CALL(a->nvnMemoryPoolInvalidateMappedRange(&o->pools[TARGET].key,0,24576));stage="oracle";
   for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){unsigned char expected[4];nn_public_buffer_pixel(format,phase,x,y,expected);for(unsigned k=0;k<4;k++){unsigned actual=o->pools[TARGET].memory[4096+y*256+x*4+k],delta=actual>expected[k]?actual-expected[k]:expected[k]-actual;channels++;if(delta>maxerror)maxerror=delta;if(delta>1){bad_x=x;bad_y=y;bad_c=k;bad_actual=actual;bad_expected=expected[k];e=EILSEQ;goto save_failure;}}}
   stage="guards";for(size_t at=0;at<24576;at++)if(at<4096||at>=20480){guards++;if(o->pools[TARGET].memory[at]!=0xa5){bad_byte=at;e=EILSEQ;goto save_failure;}}
   stage="readonly";for(unsigned i=0;i<POOLS;i++)if(i!=TARGET&&o->pools[i].live){struct pool*p=o->pools+i;CALL(a->nvnMemoryPoolInvalidateMappedRange(&p->key,0,p->bytes));for(size_t at=0;at<p->bytes;at++){readonly++;if(p->memory[at]!=p->shadow[at]){bad_byte=at;e=EILSEQ;goto save_failure;}}}
   save_failure:;int saved=buffer_save(directory,test*4+phase,o);if(saved&&!e)e=saved;else if(!saved)files++;if(e)goto done;completed++;if(phase>=2)replays++;
  }
  stage="retire";CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->copied));o->copied=0;released++;CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->handle));o->handle=0;released++;CALL(a->nvnCommandBufferFinalize(&o->command));o->command_live=0;CALL(a->nvnCommandBufferFinalize(&o->call_parent));o->call_parent_live=0;
 }
 stage="complete";bad_case=UINT32_MAX;
done:
 nn_nvn_clear_error();if(o->pending&&o->queue_live){a->nvnQueueFinish(&o->queue);if(!nn_nvn_error())o->pending=0;}
 clean=cleanup(o);int retained=clean||o->pending;if(retained){o->next=quarantine;quarantine=o;}else free(o);int passed=!e&&!clean&&completed==48;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"nvn_buffer_sampled_pipeline\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"submissions\":%u,\"finishes\":%u,\"completed_cases\":%u,\"files\":%u,\"formats\":12,\"recorded_handles\":%u,\"released_handles\":%u,\"copy_commands\":%u,\"call_commands\":%u,\"replays\":%u,\"shader_source_finalizes\":%u,\"texture_registrations\":%u,\"registered_cpu_object_finalizes\":%u,\"inflight_destroy_rejections\":%u,\"channel_checks\":%llu,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"maximum_channel_error\":%u,\"bad_case\":%u,\"bad_x\":%u,\"bad_y\":%u,\"bad_channel\":%u,\"bad_byte\":%zu,\"bad_actual\":%u,\"bad_expected\":%u,\"index_domain\":[0],\"full_c1_bytes\":256,\"graphics_control_profile_bytes\":2176,\"public_nvn\":true,\"cpu_image_conversion\":false,\"game_modules_executed\":false,\"passed\":%s}\n",(unsigned)getuid(),stage,e,clean,retained?"true":"false",submits,finishes,completed,files,recorded,released,copies,calls,replays,shader_finalizes,registrations,finalizes,busy_rejects,(unsigned long long)channels,(unsigned long long)guards,(unsigned long long)readonly,maxerror,bad_case,bad_x,bad_y,bad_c,bad_byte,bad_actual,bad_expected,passed?"true":"false");return passed&&n>0&&(size_t)n<capacity?0:e?e:clean?clean:EIO;
}
