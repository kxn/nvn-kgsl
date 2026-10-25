/* SPDX-License-Identifier: MIT. Authored public-NVN callers only. Canonical
 * background initialization is raw zeroing, never CPU image conversion. */
static int public_color_save(const char *directory,unsigned number,struct owner *o)
{
 char name[1400];int n=snprintf(name,sizeof(name),"%s/nvn-pipeline-%02u.raw",directory,number);
 if(n<0||(size_t)n>=sizeof(name))return ENAMETOOLONG;
 int fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;
 const unsigned order[]={TARGET,UNIFORM,VERTEX,ARENA,ARENA};int e=0;
 for(unsigned i=0;i<5&&!e;i++){const struct pool*p=o->pools+order[i];size_t at=0;const unsigned char *bytes=i==3?p->shadow:p->memory;
  while(at<p->bytes){ssize_t w=write(fd,bytes+at,p->bytes-at);if(w<0&&errno==EINTR)continue;if(w<=0){e=w<0?errno:EIO;break;}at+=(size_t)w;}}
 if(!e&&fsync(fd))e=errno;
 if(close(fd)&&!e)e=errno;
 return e;
}
int nn_nvn_pipeline_probe(const char *directory,char *json,size_t capacity)
{
 if(!directory||!json||capacity<32768)return EINVAL;
 struct owner*o=calloc(1,sizeof(*o));if(!o)return ENOMEM;struct api*a=&o->a;
 int e=0,clean=0;const char*stage="loader";unsigned submits=0,finishes=0,completed=0,files=0,recorded=0,released=0,copies=0,calls=0,replays=0,busy=0,address_checks=0,shader_finalizes=0;
 uint64_t guards=0,readonly=0,active=0;unsigned bad_case=UINT32_MAX,bad_actual=0;size_t bad_byte=SIZE_MAX;char cases[20000]={0};size_t used=0;
#define LOAD(name,type) a->name=(type)nvnBootstrapLoader(#name);if(!a->name){e=ENOSYS;goto done;}
 API_LIST(LOAD)
#undef LOAD
 nn_nvn_clear_error();const struct nn_pipeline_archive*archive=&NN_GRAPHICS_PIPELINE_ARCHIVE;
 CHECK(NN_PIPELINE_QUAL_PAIR_INDEX<archive->pair_count);const struct nn_pipeline_pair_recipe*pair=archive->pairs+NN_PIPELINE_QUAL_PAIR_INDEX;
 CHECK(pair->recipe&&!pair->recipe->texture_mask&&!pair->recipe->sampler_mask&&pair->recipe->input_count==2);
 o->stages[0]=archive->stages->stages+pair->vs_index;o->stages[1]=archive->stages->stages+pair->fs_index;
 CHECK(o->stages[0]->source_bytes==o->stages[0]->code_bytes&&o->stages[1]->source_bytes==o->stages[1]->code_bytes);
 stage="device";CALL(a->nvnDeviceBuilderSetDefaults(&o->db));BOOL(a->nvnDeviceInitialize(&o->device,&o->db));o->device_live=1;
 if((e=nn_nvn_register_typed_pipeline_archive(&o->device,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,reader,o))||
    (e=nn_nvn_enable_pipeline_runtime_views(&o->device))||(e=nn_nvn_register_block_copy(&o->device,&NN_PUBLIC_COLOR_BLOCK_IMAGE)))goto done;
 CALL(a->nvnDeviceSetWindowOriginMode(&o->device,NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT));CALL(a->nvnDeviceSetDepthMode(&o->device,NVN_DEPTH_MODE_NEAR_IS_MINUS_W));
 CALL(a->nvnQueueBuilderSetDefaults(&o->qb));CALL(a->nvnQueueBuilderSetDevice(&o->qb,&o->device));BOOL(a->nvnQueueInitialize(&o->queue,&o->qb));o->queue_live=1;
 if((e=create_pool(o,UNIFORM,4096))||(e=create_pool(o,VERTEX,4096))||(e=create_pool(o,SOURCE,16384))||(e=create_pool(o,ARENA,131072)))goto done;
 for(unsigned i=0,off=4096;i<2;i++){
  CHECK(off+o->stages[i]->source_bytes<=o->pools[SOURCE].bytes);memcpy(o->pools[SOURCE].memory+off,o->stages[i]->source,o->stages[i]->source_bytes);
  o->shaders[i]=(NVNshaderData){o->pools[SOURCE].address+off,o->stages[i]->control};off+=(unsigned)o->stages[i]->source_bytes;off=(off+255)&~255u;
 }
 stage="program";BOOL(a->nvnProgramInitialize(&o->program,&o->device));o->program_live=1;o->reader_enabled=1;BOOL(a->nvnProgramSetShaders(&o->program,2,o->shaders));o->reader_enabled=0;
 CALL(a->nvnMemoryPoolFinalize(&o->pools[SOURCE].key));o->pools[SOURCE].live=0;shader_finalizes++;free(o->pools[SOURCE].memory);o->pools[SOURCE].memory=NULL;
 for(unsigned fi=0;fi<22;fi++){
  const struct nn_public_color_shape*shape=NN_PUBLIC_COLOR_SHAPES+fi;
  stage="target";CALL(a->nvnTextureBuilderSetDefaults(&o->tb));CALL(a->nvnTextureBuilderSetDevice(&o->tb,&o->device));CALL(a->nvnTextureBuilderSetFlags(&o->tb,9));CALL(a->nvnTextureBuilderSetSize2D(&o->tb,73,41));CALL(a->nvnTextureBuilderSetFormat(&o->tb,(NVNformat)shape->format));
  CHECK(a->nvnTextureBuilderGetStorageSize(&o->tb)==shape->storage_bytes);if((e=nn_nvn_error()))goto done;
  if((e=create_pool(o,TARGET,shape->pool_bytes)))goto done;
  CALL(a->nvnTextureBuilderSetStorage(&o->tb,&o->pools[TARGET].key,4096));BOOL(a->nvnTextureInitialize(&o->texture,&o->tb));o->texture_live=1;
  CHECK(a->nvnTextureGetTextureAddress(&o->texture)==o->pools[TARGET].address+4096);CHECK(a->nvnTextureGetFlags(&o->texture)==9);if((e=nn_nvn_error()))goto done;
  for(unsigned state=0;state<3;state++){
   unsigned test=fi*3+state;bad_case=test*2;
   memset(o->pools[TARGET].memory,0xa5,o->pools[TARGET].bytes);memset(o->pools[TARGET].memory+4096,0,shape->storage_bytes);
   stage="record";BOOL(a->nvnCommandBufferInitialize(&o->command,&o->device));o->command_live=1;CALL(a->nvnCommandBufferAddCommandMemory(&o->command,&o->pools[ARENA].key,0,65536));CALL(a->nvnCommandBufferAddControlMemory(&o->command,o->control,sizeof(o->control)));CALL(a->nvnCommandBufferBeginRecording(&o->command));
   CALL(a->nvnCommandBufferBindProgram(&o->command,&o->program,NVN_SHADER_STAGE_VERTEX_BIT|NVN_SHADER_STAGE_FRAGMENT_BIT));
   NVNcolorState color;NVNchannelMaskState mask;NVNpolygonState poly;NVNdepthStencilState depth;NVNmultisampleState multi;NVNblendState blend;
   CALL(a->nvnColorStateSetDefaults(&color));CALL(a->nvnChannelMaskStateSetDefaults(&mask));CALL(a->nvnPolygonStateSetDefaults(&poly));CALL(a->nvnDepthStencilStateSetDefaults(&depth));CALL(a->nvnMultisampleStateSetDefaults(&multi));CALL(a->nvnBlendStateSetDefaults(&blend));
   if(state==1){CALL(a->nvnColorStateSetBlendEnable(&color,0,1));CALL(a->nvnBlendStateSetBlendFunc(&blend,NVN_BLEND_FUNC_SRC_ALPHA,NVN_BLEND_FUNC_ONE_MINUS_SRC_ALPHA,NVN_BLEND_FUNC_ONE,NVN_BLEND_FUNC_ZERO));}
   if(state==2)CALL(a->nvnChannelMaskStateSetChannelMask(&mask,0,1,0,1,0));
   CALL(a->nvnCommandBufferBindColorState(&o->command,&color));CALL(a->nvnCommandBufferBindChannelMaskState(&o->command,&mask));CALL(a->nvnCommandBufferBindPolygonState(&o->command,&poly));CALL(a->nvnCommandBufferBindDepthStencilState(&o->command,&depth));CALL(a->nvnCommandBufferBindMultisampleState(&o->command,&multi));CALL(a->nvnCommandBufferBindBlendState(&o->command,&blend));CALL(a->nvnCommandBufferSetDepthBounds(&o->command,0,0,1));
   NVNvertexAttribState attrs[2];NVNvertexStreamState streams[4];for(unsigned i=0;i<4;i++)CALL(a->nvnVertexStreamStateSetDefaults(streams+i));CALL(a->nvnVertexStreamStateSetStride(streams+3,24));
   for(unsigned i=0;i<2;i++){CALL(a->nvnVertexAttribStateSetDefaults(attrs+i));CALL(a->nvnVertexAttribStateSetFormat(attrs+i,i?NVN_FORMAT_RGBA32F:NVN_FORMAT_RG32F,i?8:0));CALL(a->nvnVertexAttribStateSetStreamIndex(attrs+i,3));}
   CALL(a->nvnCommandBufferBindVertexAttribState(&o->command,2,attrs));CALL(a->nvnCommandBufferBindVertexStreamState(&o->command,4,streams));memset(attrs,0xcc,sizeof(attrs));memset(streams,0xcc,sizeof(streams));
   CALL(a->nvnCommandBufferBindVertexBuffer(&o->command,3,o->pools[VERTEX].address+256,72));CALL(a->nvnCommandBufferBindUniformBuffer(&o->command,NVN_SHADER_STAGE_VERTEX,0,o->pools[UNIFORM].address+256,64));const NVNtexture*target=&o->texture;
   CALL(a->nvnCommandBufferSetRenderTargets(&o->command,1,&target,NULL,NULL,NULL));CALL(a->nvnCommandBufferSetViewport(&o->command,0,0,73,41));CALL(a->nvnCommandBufferSetScissor(&o->command,0,0,73,41));CALL(a->nvnCommandBufferSetDepthRange(&o->command,0,1));CALL(a->nvnCommandBufferDrawArrays(&o->command,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));
   o->call_child=a->nvnCommandBufferEndRecording(&o->command);if((e=nn_nvn_error()))goto done;CHECK(o->call_child);recorded++;
   CALL(a->nvnCommandBufferBeginRecording(&o->command));CALL(a->nvnCommandBufferCopyCommands(&o->command,1,&o->call_child));copies++;o->copied=a->nvnCommandBufferEndRecording(&o->command);if((e=nn_nvn_error()))goto done;CHECK(o->copied);recorded++;
   BOOL(a->nvnCommandBufferInitialize(&o->call_parent,&o->device));o->call_parent_live=1;CALL(a->nvnCommandBufferAddCommandMemory(&o->call_parent,&o->pools[ARENA].key,65536,65536));CALL(a->nvnCommandBufferAddControlMemory(&o->call_parent,o->call_control,sizeof(o->call_control)));CALL(a->nvnCommandBufferBeginRecording(&o->call_parent));CALL(a->nvnCommandBufferCallCommands(&o->call_parent,1,&o->call_child));calls++;
   o->handle=a->nvnCommandBufferEndRecording(&o->call_parent);if((e=nn_nvn_error()))goto done;CHECK(o->handle);recorded++;CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->call_child));o->call_child=0;released++;
   /* Alter caller state objects after record: only the value snapshots apply. */
   memset(&color,0xcc,sizeof(color));memset(&mask,0xcc,sizeof(mask));memset(&blend,0xcc,sizeof(blend));
   int n=snprintf(cases+used,sizeof(cases)-used,"%s{\"case\":%u,\"format\":%u,\"state\":%u,\"storage_bytes\":%u,\"pool_bytes\":%u,\"cpp\":%u,\"gob_pitch\":%u,\"block_height_log2\":%u,\"handle_kind\":\"%s\"}",used?",":"",test,shape->format,state,shape->storage_bytes,shape->pool_bytes,shape->cpp,shape->gob_pitch,shape->bh,state==1?"call":"copy");
   CHECK(n>0&&(size_t)n<sizeof(cases)-used);used+=(size_t)n;
   NVNcommandHandle selected=state==1?o->handle:o->copied;
   for(unsigned repeat=0;repeat<2;repeat++){
    bad_case=test*2+repeat;stage="prepare";memset(o->pools[UNIFORM].memory,0xa5,4096);memset(o->pools[VERTEX].memory,0xa5,4096);
    float matrix[16]={0};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;matrix[3]=repeat?.5f:0;for(unsigned k=0;k<16;k++)put_float(o->pools[UNIFORM].memory+256+4*k,matrix[k]);
    const float pos[3][2]={{-1,-1},{3,-1},{-1,3}};float rgba[4];nn_color_input(state,repeat,rgba);
    for(unsigned v=0;v<3;v++){memcpy(o->pools[VERTEX].memory+256+v*24,pos[v],8);memcpy(o->pools[VERTEX].memory+264+v*24,rgba,16);}
    for(unsigned i=0;i<POOLS;i++)if(o->pools[i].live){struct pool*p=o->pools+i;if(i!=TARGET){if(!p->shadow){p->shadow=malloc(p->bytes);if(!p->shadow){e=ENOMEM;goto done;}}memcpy(p->shadow,p->memory,p->bytes);}CALL(a->nvnMemoryPoolFlushMappedRange(&p->key,0,p->bytes));}
    CHECK(a->nvnTextureGetTextureAddress(&o->texture)==o->pools[TARGET].address+4096);if((e=nn_nvn_error()))goto done;address_checks++;
    stage="submit";o->pending=1;CALL(a->nvnQueueSubmitCommands(&o->queue,1,&selected));submits++;
    a->nvnTextureFinalize(&o->texture);CHECK(nn_nvn_error()==EBUSY);nn_nvn_clear_error();busy++;a->nvnMemoryPoolFinalize(&o->pools[TARGET].key);CHECK(nn_nvn_error()==EBUSY);nn_nvn_clear_error();busy++;
    stage="finish";CALL(a->nvnQueueFinish(&o->queue));o->pending=0;finishes++;
    stage="guards";CALL(a->nvnMemoryPoolInvalidateMappedRange(&o->pools[TARGET].key,0,o->pools[TARGET].bytes));
    for(size_t at=0;at<o->pools[TARGET].bytes;at++)if(at<4096||at>=4096+shape->storage_bytes){guards++;if(o->pools[TARGET].memory[at]!=0xa5){bad_byte=at;bad_actual=o->pools[TARGET].memory[at];e=EILSEQ;goto save_failure;}}
    stage="readonly";for(unsigned i=0;i<POOLS;i++)if(i!=TARGET&&o->pools[i].live){struct pool*p=o->pools+i;CALL(a->nvnMemoryPoolInvalidateMappedRange(&p->key,0,p->bytes));for(size_t at=0;at<p->bytes;at++){readonly++;if(p->memory[at]!=p->shadow[at]){bad_byte=at;bad_actual=p->memory[at];e=EILSEQ;goto save_failure;}}}
    save_failure:;int saved=public_color_save(directory,test*2+repeat,o);if(saved&&!e)e=saved;else if(!saved)files++;if(e)goto done;completed++;if(repeat)replays++;active+=(uint64_t)73*41*shape->cpp;
   }
   stage="retire-handle";CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->copied));o->copied=0;released++;CALL(a->nvnDeviceFinalizeCommandHandle(&o->device,o->handle));o->handle=0;released++;
   CALL(a->nvnCommandBufferFinalize(&o->command));o->command_live=0;CALL(a->nvnCommandBufferFinalize(&o->call_parent));o->call_parent_live=0;
  }
  stage="retire-target";CALL(a->nvnTextureFinalize(&o->texture));o->texture_live=0;CALL(a->nvnMemoryPoolFinalize(&o->pools[TARGET].key));o->pools[TARGET].live=0;free(o->pools[TARGET].memory);o->pools[TARGET].memory=NULL;
 }
 stage="complete";bad_case=UINT32_MAX;
done:
 nn_nvn_clear_error();clean=cleanup(o);int retained=clean||o->pending;if(retained){o->next=quarantine;quarantine=o;}else free(o);
 int passed=!e&&!clean&&completed==132;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"nvn_gob_color_targets\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"resources_retained\":%s,\"formats\":22,\"submissions\":%u,\"finishes\":%u,\"completed_cases\":%u,\"files\":%u,\"recorded_handles\":%u,\"released_handles\":%u,\"copy_commands\":%u,\"call_commands\":%u,\"replays\":%u,\"source_address_checks\":%u,\"shader_source_finalizes\":%u,\"inflight_finalize_rejections\":%u,\"guard_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"active_bytes_saved\":%llu,\"pixel_checks\":0,\"pixel_oracle_on_host\":true,\"same_handle_replay\":true,\"source_flags\":9,\"pool_flags\":164,\"canonical_prefix\":4096,\"source_kind\":254,\"public_nvn\":true,\"cpu_image_conversion\":false,\"game_modules_executed\":false,\"bad_case\":%u,\"bad_byte\":%zu,\"bad_actual\":%u,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),stage,e,clean,retained?"true":"false",submits,finishes,completed,files,recorded,released,copies,calls,replays,address_checks,shader_finalizes,busy,(unsigned long long)guards,(unsigned long long)readonly,(unsigned long long)active,bad_case,bad_byte,bad_actual,cases,passed?"true":"false");
 return passed&&n>0&&(size_t)n<capacity?0:e?e:clean?clean:EIO;
}
