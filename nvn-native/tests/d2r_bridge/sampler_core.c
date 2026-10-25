/* Real core Draw/descriptor publication+encoding, no GPU execution. */
#define main mode_baseline_main
#include NN_MODE_FRONTEND_FIXTURE
#undef main
#undef ENTRY
static uint32_t *table(nn_bo *job,const struct nn_gfx_image *image)
{
 const uint32_t *words=nn_bo_map(job);
 for(unsigned i=0;i<image->patch_count;i++)if(image->patches[i].kind==NN_GFX_SAMPLERS){
  const struct nn_gfx_patch *p=image->patches+i;uint64_t a=words[p->word]|(uint64_t)words[p->word+1]<<32;
  T(a>=nn_bo_address(job)+p->offset);size_t offset=a-nn_bo_address(job)-p->offset;
  T(offset<=nn_bo_size(job)&&32<=nn_bo_size(job)-offset);return (uint32_t *)((char *)nn_bo_map(job)+offset);
 }
 abort();
}
int main(void)
{
 T(!mode_baseline_main());
 NVNdeviceBuilder db;NVNdevice devkey;api_nvnDeviceBuilderSetDefaults(&db);T(api_nvnDeviceInitialize(&devkey,&db));struct object *dev=find(&devkey,DEVICE);
 NVNsamplerBuilder sb;NVNsampler sm;OK(api_nvnSamplerBuilderSetDefaults(&sb));OK(api_nvnSamplerBuilderSetDevice(&sb,&devkey));
 const uint32_t border_bits[4]={0x3f400000u,0x7fc12345u,0xff800000u,0x80000000u};
 float border[4],observed[4];memcpy(border,border_bits,sizeof(border));
 OK(api_nvnSamplerBuilderSetBorderColor(&sb,border));memset(border,0,sizeof(border));
 OK(api_nvnSamplerBuilderGetBorderColor(&sb,observed));T(!memcmp(observed,border_bits,sizeof(observed)));
 T(api_nvnSamplerInitialize(&sm,&sb));
 OK(api_nvnSamplerGetBorderColor(&sm,observed));T(!memcmp(observed,border_bits,sizeof(observed)));
 struct object *s=find(&sm,SAMPLER);T(s&&s->sampler.min==NVN_MIN_FILTER_LINEAR&&s->sampler.wrap[0]==NVN_WRAP_MODE_REPEAT);
 OK(api_nvnSamplerBuilderSetMinMagFilter(&sb,NVN_MIN_FILTER_LINEAR_MIPMAP_LINEAR,NVN_MAG_FILTER_NEAREST));
 OK(api_nvnSamplerBuilderSetWrapMode(&sb,NVN_WRAP_MODE_MIRRORED_REPEAT,NVN_WRAP_MODE_REPEAT,NVN_WRAP_MODE_CLAMP_TO_EDGE));
 OK(api_nvnSamplerBuilderSetMaxAnisotropy(&sb,8));OK(api_nvnSamplerBuilderSetLodBias(&sb,-.5f));
 OK(api_nvnSamplerBuilderSetLodClamp(&sb,1,4));
 float other_border[4]={-2,.125f,8,1};OK(api_nvnSamplerBuilderSetBorderColor(&sb,other_border));
 struct sampler_desc before=find(&sb,SB)->sampler;
 OK(api_nvnSamplerBuilderSetWrapMode(&sb,NVN_WRAP_MODE_CLAMP_TO_BORDER,NVN_WRAP_MODE_REPEAT,NVN_WRAP_MODE_REPEAT));
 T(!sampler_profile(&find(&sb,SB)->sampler));
 OK(api_nvnSamplerBuilderSetWrapMode(&sb,before.wrap[0],before.wrap[1],before.wrap[2]));
 OK(api_nvnSamplerBuilderSetLodBias(&sb,16));T(!sampler_profile(&find(&sb,SB)->sampler));
 OK(api_nvnSamplerBuilderSetLodBias(&sb,before.lod_bias));
 unsigned char keys[7];struct object *p[7];for(unsigned i=0;i<7;i++)p[i]=pool(dev,&keys[i],i==5||i==6?16384:i==1?16384:4096);
 NVNprogram program;T(api_nvnProgramInitialize(&program,&devkey));struct object *pr=find(&program,PROGRAM);
 const struct nn_gfx_image *image=&NVN_GRAPHICS_IMAGE;T(image->profile==NN_GFX_TEXTURE_2D_NORMALIZED);
 struct nn_nvn_compiled_graphics compiled={.image=image};pr->graphics=&compiled;T(!nn_bo_alloc(dev->gpu,image->vs_code_bytes,&pr->bo));T(!nn_bo_alloc(dev->gpu,image->fs_code_bytes,&pr->fs_bo));
 NVNtexture target,source;struct object *rt=texture(dev,&target,p[1],64);texture(dev,&source,p[4],2);
 NVNtexturePool tp;NVNsamplerPool sp;T(api_nvnTexturePoolInitialize(&tp,p[5]->key,0,272));T(api_nvnSamplerPoolInitialize(&sp,p[6]->key,0,272));
 OK(api_nvnTexturePoolRegisterTexture(&tp,257,&source,NULL));OK(api_nvnSamplerPoolRegisterSampler(&sp,259,&sm));
 /* Registration copies immutable public object values; later Builder changes
  * do not change this descriptor entry, and the sampler object may finalize. */
 const struct sampler_desc original_snapshot=find(&sp,SPOOL)->slots[259].snapshot.sampler;
 OK(api_nvnSamplerFinalize(&sm));T(original_snapshot.min==NVN_MIN_FILTER_LINEAR&&original_snapshot.anisotropy==1);
 T(!memcmp(original_snapshot.border,border_bits,sizeof(border_bits)));
 NVNcommandBuffer command;_Alignas(16) unsigned char control[512];T(api_nvnCommandBufferInitialize(&command,&devkey));OK(api_nvnCommandBufferAddCommandMemory(&command,p[0]->key,0,4096));OK(api_nvnCommandBufferAddControlMemory(&command,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&command));
 struct recording *r=find(&command,COMMAND)->record;r->graphics_program=pr;pr->refs++;r->target=rt;rt->refs++;
 OK(api_nvnCommandBufferBindVertexBuffer(&command,0,nn_bo_address(p[2]->bo),96));
 r->graphics_ubos[0][0]=(struct bound){p[3],0,64,nn_bo_address(p[3]->bo)};p[3]->refs++;
 r->graphics_ubos[1][0]=(struct bound){p[3],128,64,nn_bo_address(p[3]->bo)+128};p[3]->refs++;
 NVNvertexAttribState attributes[3];NVNvertexStreamState stream;
 for(unsigned i=0;i<3;i++)OK(api_nvnVertexAttribStateSetDefaults(attributes+i));
 OK(api_nvnVertexAttribStateSetFormat(attributes,NVN_FORMAT_RG32F,0));
 OK(api_nvnVertexAttribStateSetFormat(attributes+1,NVN_FORMAT_RG32F,8));
 OK(api_nvnVertexAttribStateSetFormat(attributes+2,NVN_FORMAT_RGBA32F,16));
 OK(api_nvnVertexStreamStateSetDefaults(&stream));
 OK(api_nvnVertexStreamStateSetStride(&stream,32));
 OK(api_nvnCommandBufferBindVertexAttribState(&command,3,attributes));
 OK(api_nvnCommandBufferBindVertexStreamState(&command,1,&stream));
 OK(api_nvnCommandBufferSetViewport(&command,0,0,64,64));
 OK(api_nvnCommandBufferSetScissor(&command,0,0,64,64));
 OK(api_nvnCommandBufferSetDepthRange(&command,0,1));
 struct nn_nvn_sampled mapping={.stage=NVN_SHADER_STAGE_FRAGMENT,.texture_slot=10,.sampler_slot=0};compiled.sampled=&mapping;compiled.sampled_count=1;
 OK(api_nvnCommandBufferSetTexturePool(&command,&tp));OK(api_nvnCommandBufferSetSamplerPool(&command,&sp));
 OK(api_nvnCommandBufferBindSeparateTexture(&command,NVN_SHADER_STAGE_FRAGMENT,10,api_nvnDeviceGetSeparateTextureHandle(&devkey,257)));OK(api_nvnCommandBufferBindSeparateSampler(&command,NVN_SHADER_STAGE_FRAGMENT,0,api_nvnDeviceGetSeparateSamplerHandle(&devkey,259)));
 OK(api_nvnCommandBufferDrawArrays(&command,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3));NVNcommandHandle handle=api_nvnCommandBufferEndRecording(&command);T(handle);struct handle *h=handles;T(h->id==handle&&h->draws);T(h->draws->texture_id==257&&h->draws->sampler_id==259);
 T(h->cpu_count==2);for(size_t i=0;i<h->count;i++)T(h->resources[i]!=find(&tp,TPOOL)&&h->resources[i]!=find(&sp,SPOOL));
 ERR(api_nvnSamplerPoolFinalize(&sp),EBUSY);
 nn_bo *job1=NULL,*job2=NULL,*sampled=NULL;size_t bytes=0;T(!materialize_draw(dev,h->draws,&job1,&bytes,&sampled));T(sampled==p[4]->bo);
 struct nn_sampler_state native=native_sampler_state(&original_snapshot);const struct nn_sampler_layout layout={2,2,1,1,2,1,0,0};uint32_t expected1[4],expected2[4];T(!nn_sampler_state_encode(0x06050002,&native,&layout,expected1));
 uint32_t *first=table(job1,image);T(!(first[0]|first[1]|first[2]|first[3]));T(!memcmp(first+4,expected1,16));
 /* Native enqueue semantics intentionally resolve the current registered slot:
  * immutable second job differs; first in-flight-version bytes stay unchanged. */
 OK(api_nvnSamplerPoolRegisterSamplerBuilder(&sp,259,&sb));
 T(!materialize_draw(dev,h->draws,&job2,&bytes,&sampled));native=native_sampler_state(&before);T(!nn_sampler_state_encode(0x06050002,&native,&layout,expected2));T(memcmp(expected1,expected2,16));
 T(!memcmp(table(job2,image)+4,expected2,16));T(!memcmp(first+4,expected1,16));
 /* Direct corruption of a software descriptor is rejected atomically. */
 unsigned char *descriptor=(unsigned char *)nn_bo_map(p[6]->bo)+259*32;descriptor[0]^=1;nn_bo *bad=NULL;size_t zero=999;T(materialize_draw(dev,h->draws,&bad,&zero,&sampled)==EINVAL&&bad==NULL);descriptor[0]^=1;
 T(!nn_bo_destroy(job1));T(!nn_bo_destroy(job2));
 OK(api_nvnCommandBufferFinalize(&command));OK(api_nvnDeviceFinalizeCommandHandle(&devkey,handle));
 OK(api_nvnTexturePoolFinalize(&tp));OK(api_nvnSamplerPoolFinalize(&sp));OK(api_nvnTextureFinalize(&source));OK(api_nvnTextureFinalize(&target));OK(api_nvnProgramFinalize(&program));
 for(unsigned i=0;i<7;i++){OK(api_nvnMemoryPoolFinalize((NVNmemoryPool *)p[i]->key));}
 OK(api_nvnDeviceFinalize(&devkey));
 while(objects){T(objects->kind==DB||objects->kind==SB);remove_object(objects);}T(!handles);
 printf("{\"passed\":true,\"checks\":%u,\"gpu_submits\":0,\"scope\":\"actual native sampler registration/Draw/materializer lifecycle\"}\n",checks);return 0;
}
