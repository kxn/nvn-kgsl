/* HOST ONLY real public SetPool/Bind/Register snapshots; no GPU execution. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef NN_TEST_NVN_SOURCE
#define NN_TEST_NVN_SOURCE "../../src/nvn.c"
#endif
#ifndef NN_MEMORY_BACKEND_SOURCE
#define NN_MEMORY_BACKEND_SOURCE "memory_backend.c"
#endif
#include NN_MEMORY_BACKEND_SOURCE
#include NN_TEST_NVN_SOURCE
#include "nvn_native/pipeline_resource_queries.h"
#ifdef NN_IMAGE_FORMAT_EXPECTED_HEADER
#include NN_IMAGE_FORMAT_EXPECTED_HEADER
#endif
static unsigned checks,image_format_cases;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define ERR(x,n) do{x;T(nn_nvn_error()==(n));nn_nvn_clear_error();}while(0)
struct captured {struct nn_pipeline_resource_queries state;struct nn_pipeline_resource_binding bindings[192];struct object *pins[193];size_t pins_count;};
static void unpin(struct captured *c)
{while(c->pins_count)c->pins[--c->pins_count]->refs--;}
static int pin(struct captured *c,struct object *o)
{for(size_t i=0;i<c->pins_count;i++)if(c->pins[i]==o)return 0;if(c->pins_count==193)return EOVERFLOW;c->pins[c->pins_count++]=o;o->refs++;return 0;}
/* Test-only adapter uses actual core's already validated registry and existing
 * texture_slot definition. Production must refactor/share this same integrity
 * gate with materializers; it must not trust caller supplied serialized fields. */
static int capture(struct recording *r,struct captured *c)
{
 memset(c,0,sizeof(*c));c->state=(struct nn_pipeline_resource_queries){.version=1,.graphics_texture_cb_known=1,.graphics_texture_cb=2,.selected_texture_pool=r->texture_pool,.bindings=c->bindings};
 if(r->texture_pool){int e=cpu_descriptor_access(r->texture_pool);if(e)return e;if((e=pin(c,r->texture_pool)))return e;}
 for(unsigned stage=0;stage<2;stage++)for(unsigned domain=0;domain<3;domain++)for(unsigned index=0;index<32;index++){
  uint64_t value=domain==0?r->combined[stage][index]:domain==1?r->separate_textures[stage][index]:r->separate_samplers[stage][index];if(!value)continue;
  struct nn_pipeline_resource_binding *b=c->bindings+c->state.binding_count++;
  *b=(struct nn_pipeline_resource_binding){.stage=stage,.domain=domain,.selector=index,.low_word_defined=1,.payload=(uint32_t)value,.guest_carrier=r->handle_carriers[domain].guest[stage][index]};
  struct object *pool=r->texture_pool;
  if(domain!=1||!pool||value>=pool->slot_count)continue;
  struct descriptor_entry *entry=pool->slots+value;if(!entry->valid)continue;
  if(!entry->pool||gpu_pool_access(entry->pool,entry->offset,entry->size,"HOST resource query capture")){unpin(c);return ENOTSUP;}
  const struct texture_desc *t=&entry->snapshot.texture;
  struct texture_slot expected={t->target==NVN_TEXTURE_TARGET_BUFFER?0x42584e4eu:0x54584e4eu,(uint32_t)t->format,(uint32_t)t->width,(uint32_t)t->height,(uint32_t)t->stride,(uint32_t)entry->size,nn_bo_address(entry->pool->bo)+entry->offset};
  const void *raw=(const char *)nn_bo_map(pool->pool->bo)+pool->offset+value*DESCRIPTOR_BYTES;
  if(memcmp(raw,&expected,sizeof(expected))){unpin(c);return EINVAL;}
  int e=pin(c,entry->pool);if(e){unpin(c);return e;}
  b->texture=(struct nn_pipeline_registered_texture){pool,entry->pool,1,(uint32_t)value,(uint32_t)t->target,(uint32_t)t->format,(uint32_t)t->width,(uint32_t)t->height,(uint32_t)t->depth,(uint32_t)t->levels,(uint32_t)t->samples,entry->offset,entry->size};
 }
 return 0;
}
static void pool(NVNdevice *d,NVNmemoryPoolBuilder *b,NVNmemoryPool *p,void **memory,size_t bytes,int flags)
{
 T(!posix_memalign(memory,4096,bytes));memset(*memory,0xa5,bytes);
 OK(api_nvnMemoryPoolBuilderSetDefaults(b));OK(api_nvnMemoryPoolBuilderSetDevice(b,d));OK(api_nvnMemoryPoolBuilderSetStorage(b,*memory,bytes));OK(api_nvnMemoryPoolBuilderSetFlags(b,flags));T(api_nvnMemoryPoolInitialize(p,b));
}
static void texture(NVNdevice *d,NVNmemoryPool *p,NVNtextureBuilder *b,NVNtexture *t,unsigned width)
{
 OK(api_nvnTextureBuilderSetDefaults(b));OK(api_nvnTextureBuilderSetDevice(b,d));OK(api_nvnTextureBuilderSetFlags(b,NVN_TEXTURE_FLAGS_LINEAR));
 OK(api_nvnTextureBuilderSetSize2D(b,width,width));OK(api_nvnTextureBuilderSetFormat(b,NVN_FORMAT_RGBA8));OK(api_nvnTextureBuilderSetStride(b,256));OK(api_nvnTextureBuilderSetStorage(b,p,4096));T(api_nvnTextureInitialize(t,b));
}
static int query(struct captured *c,unsigned stage,int word,int slot,uint32_t *result)
{
 struct nn_pipeline_query q={"QuerySamplerType",3,NN_PIPELINE_QUERY_I32,1,{word,slot,0,0}};uint32_t out[32];memset(out,0xa5,sizeof(out));size_t count=17;
 int e=nn_pipeline_resource_query(&c->state,stage,&q,out,&count);
 if(e){T(!count);for(unsigned i=0;i<32;i++)T(!out[i]);}else{T(count==1);*result=out[0];}
 return e;
}
int main(void)
{
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));OK(api_nvnDeviceBuilderSetFlags(&db,0x100));T(api_nvnDeviceInitialize(&device,&db));
 enum{META_A,META_B,DATA_A,DATA_B,ARENA,COUNT};NVNmemoryPoolBuilder pb[COUNT]={0};NVNmemoryPool pools[COUNT]={0};void *memory[COUNT]={0};
 for(unsigned i=0;i<COUNT;i++)pool(&device,pb+i,pools+i,memory+i,65536,i<2?0x14:0x24);
 NVNtexturePool pa={0},pbb={0};T(api_nvnTexturePoolInitialize(&pa,pools+META_A,0,300));T(api_nvnTexturePoolInitialize(&pbb,pools+META_B,0,300));
 NVNtextureBuilder tb[2]={0};NVNtexture tex[2]={0};texture(&device,pools+DATA_A,tb,tex,2);texture(&device,pools+DATA_B,tb+1,tex+1,64);
 NVNcommandBuffer command={0};_Alignas(16)unsigned char control[4096];T(api_nvnCommandBufferInitialize(&command,&device));
 OK(api_nvnCommandBufferAddCommandMemory(&command,pools+ARENA,0,65536));OK(api_nvnCommandBufferAddControlMemory(&command,control,sizeof(control)));OK(api_nvnCommandBufferBeginRecording(&command));
 struct recording *r=find(&command,COMMAND)->record;struct captured old,newer;uint32_t value=0;
 NVNseparateTextureHandle h=api_nvnDeviceGetSeparateTextureHandle(&device,257);T(!nn_nvn_error()&&(h.value>>32)==1);
 OK(api_nvnCommandBufferSetTexturePool(&command,&pa));OK(api_nvnCommandBufferBindSeparateTexture(&command,NVN_SHADER_STAGE_FRAGMENT,10,h));
 T(!capture(r,&old));T(query(&old,1,110,-1,&value)==ENOTSUP);unpin(&old);
 OK(api_nvnTexturePoolRegisterTexture(&pa,257,tex,NULL));T(!capture(r,&old));T(!query(&old,1,110,-1,&value)&&value==3);
 T(!old.bindings[0].texture.registered||old.bindings[0].texture.width==2);T(!r->handle_carriers[1].high_defined[1]);
 T(query(&old,1,112,-1,&value)==ENOTSUP);T(query(&old,0,110,-1,&value)==ENOTSUP);
 T(query(&old,1,111,-1,&value)==ENOTSUP);T(!query(&old,1,110,2,&value)&&value==3);T(query(&old,1,-1,-1,&value)==ENOTSUP);
 struct nn_pipeline_resolved_texture resolved;T(!nn_pipeline_resource_texture(&old.state,1,110,-1,&resolved));
 T(resolved.selector==10&&resolved.descriptor_id==257&&resolved.word==110&&resolved.texture.width==2);
 /* Registered entry owns backing after TextureFinalize; snapshot adds a pin. */
 OK(api_nvnTextureFinalize(tex));ERR(api_nvnMemoryPoolFinalize(pools+DATA_A),EBUSY);
 OK(api_nvnTexturePoolRegisterTexture(&pa,257,tex+1,NULL));T(!capture(r,&newer));T(!query(&newer,1,110,-1,&value)&&value==3);
 T(!nn_pipeline_resource_texture(&old.state,1,110,-1,&resolved)&&resolved.texture.width==2);
 T(!nn_pipeline_resource_texture(&newer.state,1,110,-1,&resolved)&&resolved.texture.width==64);
 ERR(api_nvnMemoryPoolFinalize(pools+DATA_A),EBUSY);unpin(&old);OK(api_nvnMemoryPoolFinalize(pools+DATA_A));unpin(&newer);
 /* SetPool selects identity; same descriptor ID in another pool is not global. */
 OK(api_nvnCommandBufferSetTexturePool(&command,&pbb));T(!capture(r,&old));T(query(&old,1,110,-1,&value)==ENOTSUP);unpin(&old);
 OK(api_nvnTexturePoolRegisterTexture(&pbb,257,tex+1,NULL));T(!capture(r,&old));
 T(!nn_pipeline_resource_texture(&old.state,1,110,-1,&resolved)&&resolved.texture.descriptor_pool==find(&pbb,TPOOL));unpin(&old);
 /* Samplerless source does not require a sampler; wrong-domain/selector fails. */
 OK(api_nvnCommandBufferBindSeparateTexture(&command,NVN_SHADER_STAGE_FRAGMENT,10,(NVNseparateTextureHandle){0}));
 NVNtextureHandle combined=api_nvnDeviceGetTexelFetchHandle(&device,257);T(!nn_nvn_error());OK(api_nvnCommandBufferBindTexture(&command,NVN_SHADER_STAGE_FRAGMENT,10,combined));
 OK(api_nvnCommandBufferBindSeparateTexture(&command,NVN_SHADER_STAGE_FRAGMENT,11,h));T(!capture(r,&old));
 T(query(&old,1,110,-1,&value)==ENOTSUP);T(!query(&old,1,112,-1,&value)&&value==3);unpin(&old);
 OK(api_nvnCommandBufferBindSeparateTexture(&command,NVN_SHADER_STAGE_FRAGMENT,10,h));T(!capture(r,&old));
 old.state.graphics_texture_cb_known=0;T(query(&old,1,110,-1,&value)==ENOTSUP);old.state.graphics_texture_cb_known=1;
 old.state.graphics_texture_cb=1;T(query(&old,1,110,-1,&value)==ENOTSUP);old.state.graphics_texture_cb=2;
 struct nn_pipeline_resource_binding *binding=NULL;for(size_t i=0;i<old.state.binding_count;i++)if(old.bindings[i].domain==1&&old.bindings[i].selector==10)binding=old.bindings+i;T(binding);
 struct nn_pipeline_registered_texture saved=binding->texture;
 for(unsigned type=0;type<11;type++){if(type!=1){binding->texture.target=type;T(query(&old,1,110,-1,&value)==ENOTSUP);}}
 binding->texture=saved;
 binding->texture.descriptor_id=259;T(query(&old,1,110,-1,&value)==EINVAL);binding->texture=saved;
 binding->texture.descriptor_pool=find(&pa,TPOOL);T(query(&old,1,110,-1,&value)==EINVAL);binding->texture=saved;
 binding->texture=saved;binding->texture.levels=4;T(!query(&old,1,110,-1,&value)&&value==3);binding->texture=saved;
 for(unsigned field=0;field<3;field++){binding->texture=saved;if(field==0)binding->texture.depth=2;if(field==1)binding->texture.levels=0;if(field==2)binding->texture.samples=4;T(query(&old,1,110,-1,&value)==ENOTSUP);}binding->texture=saved;
 struct nn_pipeline_query q={"QueryTextureFormat",3,NN_PIPELINE_QUERY_I32,1,{110,-1,0,0}};uint32_t out[32];size_t n=77;
 T(!nn_pipeline_resource_query(&old.state,1,&q,out,&n)&&n==1&&out[0]==25);q.name="QueryTextureCoordNormalized";q.type=NN_PIPELINE_QUERY_BOOL;T(!nn_pipeline_resource_query(&old.state,1,&q,out,&n)&&n==1&&out[0]==1);

 /* Queries are observation-only: sticky error and actual binding state survive. */
 error(ERANGE);T(!query(&old,1,110,-1,&value)&&value==3);T(nn_nvn_error()==ERANGE);nn_nvn_clear_error();
 struct nn_pipeline_resource_binding duplicate=*binding;
 old.bindings[old.state.binding_count++]=duplicate;T(query(&old,1,110,-1,&value)==EINVAL);old.state.binding_count--;
 uint32_t saved_payload=binding->payload;binding->payload=110;T(query(&old,1,110,-1,&value)==EINVAL);binding->payload=saved_payload;
 binding->guest_carrier^=1;T(query(&old,1,110,-1,&value)==EINVAL);binding->guest_carrier^=1;
 binding->low_word_defined=0;T(query(&old,1,110,-1,&value)==ENOTSUP);binding->low_word_defined=1;
 binding->texture.offset=UINT64_MAX;T(query(&old,1,110,-1,&value)==EINVAL);binding->texture=saved;
 size_t saved_count=old.state.binding_count;old.state.binding_count=193;T(query(&old,1,110,-1,&value)==EINVAL);old.state.binding_count=saved_count;
 /* Re-capture rather than making assumptions about unrelated domain row order. */
 unpin(&old);T(!capture(r,&old));
 struct nn_pipeline_query malformed={"QuerySamplerType",3,NN_PIPELINE_QUERY_I32,1,{110,-1,0,0}};
 for(unsigned shape=0;shape<5;shape++){
  struct nn_pipeline_query bad=malformed;if(shape==0)bad.arguments=1;if(shape==1)bad.type=NN_PIPELINE_QUERY_BOOL;if(shape==2)bad.count=2;if(shape==3)bad.args[2]=1;if(shape==4)bad.args[3]=1;
  memset(out,0xa5,sizeof(out));n=99;T(nn_pipeline_resource_query(&old.state,1,&bad,out,&n)==EINVAL&&!n);for(unsigned i=0;i<32;i++)T(!out[i]);
 }
 struct nn_pipeline_frontend_resource_queries composed={0};composed.resources=old.state;
 composed.graphics.version=1;composed.graphics.chip=0x06050002;composed.graphics.depth_mode=1;composed.graphics.point_size=1;
 nn_render_state_defaults(&composed.graphics.render);
 T(!nn_pipeline_frontend_resource_query(&composed,1,&malformed,out,&n)&&n==1&&out[0]==3);
 struct nn_pipeline_query depth={"graphics/depth_mode",0,NN_PIPELINE_QUERY_BOOL,1,{0}};
 T(!nn_pipeline_frontend_resource_query(&composed,0,&depth,out,&n)&&n==1&&!out[0]);
 depth.name="unknown/support_buffer";T(nn_pipeline_frontend_resource_query(&composed,0,&depth,out,&n)==ENOTSUP&&!n);
 unpin(&old);
 /* Plural input is copied by value; caller mutation cannot change the selected
  * carrier. Scalar fetch ignores the unconsumed high word in either API form. */
 NVNseparateTextureHandle handles_in[1]={h};
 OK(api_nvnCommandBufferBindSeparateTextures(&command,NVN_SHADER_STAGE_FRAGMENT,10,1,handles_in));handles_in[0].value=0;
 T(r->handle_carriers[1].high_defined[1]&(1u<<10));T(!capture(r,&old));T(!query(&old,1,110,-1,&value)&&value==3);unpin(&old);
 /* Published32B descriptor tampering cannot become a trusted typed snapshot. */
 unsigned char *raw=(unsigned char*)memory[META_B]+257*32;raw[0]^=1;T(capture(r,&old)==EINVAL&&!old.pins_count);raw[0]^=1;
 /* Actual public BUFFER construction/registration determines its source type.
  * An old capture retains that type after the same slot becomes a2D image. */
 for(unsigned format_index=0;format_index<2;format_index++){
  NVNtextureBuilder buffer_builder={0};NVNtexture buffer={0};unsigned format=format_index?NVN_FORMAT_R32F:NVN_FORMAT_RGBA8;
  OK(api_nvnTextureBuilderSetDefaults(&buffer_builder));OK(api_nvnTextureBuilderSetDevice(&buffer_builder,&device));
  OK(api_nvnTextureBuilderSetTarget(&buffer_builder,NVN_TEXTURE_TARGET_BUFFER));OK(api_nvnTextureBuilderSetFormat(&buffer_builder,(NVNformat)format));
  OK(api_nvnTextureBuilderSetSize1D(&buffer_builder,7));OK(api_nvnTextureBuilderSetStorage(&buffer_builder,pools+DATA_B,12));T(api_nvnTextureInitialize(&buffer,&buffer_builder));
  OK(api_nvnTexturePoolRegisterTexture(&pbb,258,&buffer,NULL));
  NVNseparateTextureHandle bh=api_nvnDeviceGetSeparateTextureHandle(&device,258);T(!nn_nvn_error());OK(api_nvnCommandBufferBindSeparateTexture(&command,NVN_SHADER_STAGE_FRAGMENT,11,bh));
  T(!capture(r,&old));T(!query(&old,1,110,-1,&value)&&value==3);T(!query(&old,1,112,-1,&value)&&value==2);
  T(!nn_pipeline_resource_texture(&old.state,1,112,-1,&resolved)&&resolved.texture.format==format&&resolved.texture.offset==12&&resolved.texture.bytes==28);
  struct nn_pipeline_resource_binding *bb=NULL;for(size_t i=0;i<old.state.binding_count;i++)if(old.bindings[i].domain==1&&old.bindings[i].selector==11)bb=old.bindings+i;T(bb);
  bb->texture.bytes=27;T(query(&old,1,112,-1,&value)==EINVAL);bb->texture.bytes=28;
  bb->texture.height=2;T(query(&old,1,112,-1,&value)==ENOTSUP);bb->texture.height=1;
  OK(api_nvnTextureFinalize(&buffer));T(!query(&old,1,112,-1,&value)&&value==2);
  OK(api_nvnTexturePoolRegisterTexture(&pbb,258,tex+1,NULL));T(!capture(r,&newer));T(!query(&newer,1,112,-1,&value)&&value==3);
  T(!query(&old,1,112,-1,&value)&&value==2);unpin(&old);unpin(&newer);
  remove_object(find(&buffer_builder,TB));
 }
#ifdef NN_IMAGE_FORMAT_EXPECTED_HEADER
 /* CPU semantics across the complete representable color family. Expectations
  * are generated from actual NVN names + the independent translator enum,
  * not queried from the runtime mapping. Public creation/registration remains
  * distinct from a GPU consumer. Re-register each slot and retain old capture. */
 for(size_t fi=0;fi<sizeof(nn_image_formats)/sizeof(*nn_image_formats);fi++)for(unsigned mip=1;mip<=3;mip+=2){
  NVNtextureBuilder ib={0};NVNtexture image={0};
  OK(api_nvnTextureBuilderSetDefaults(&ib));OK(api_nvnTextureBuilderSetDevice(&ib,&device));
  OK(api_nvnTextureBuilderSetSize2D(&ib,17,9));OK(api_nvnTextureBuilderSetLevels(&ib,(int)mip));
  OK(api_nvnTextureBuilderSetFormat(&ib,nn_image_formats[fi].nvn));OK(api_nvnTextureBuilderSetStorage(&ib,pools+DATA_B,4096));
  T(api_nvnTextureInitialize(&image,&ib));OK(api_nvnTexturePoolRegisterTexture(&pbb,258,&image,NULL));
  T(!capture(r,&old));struct nn_pipeline_query fq={"QueryTextureFormat",3,NN_PIPELINE_QUERY_I32,1,{112,-1,0,0}};
  memset(out,0xa5,sizeof(out));n=99;T(!nn_pipeline_resource_query(&old.state,1,&fq,out,&n)&&n==1&&out[0]==nn_image_formats[fi].shader);
  for(unsigned i=1;i<32;i++)T(!out[i]);
  OK(api_nvnTextureFinalize(&image));T(!nn_pipeline_resource_query(&old.state,1,&fq,out,&n)&&n==1&&out[0]==nn_image_formats[fi].shader);
  OK(api_nvnTexturePoolRegisterTexture(&pbb,258,tex+1,NULL));T(!capture(r,&newer));
  T(!nn_pipeline_resource_query(&newer.state,1,&fq,out,&n)&&n==1&&out[0]==25);
  T(!nn_pipeline_resource_query(&old.state,1,&fq,out,&n)&&n==1&&out[0]==nn_image_formats[fi].shader);
  unpin(&old);unpin(&newer);remove_object(find(&ib,TB));image_format_cases++;
 }
 const NVNformat unknown_formats[]={NVN_FORMAT_RGBA8_SRGB,NVN_FORMAT_BGRA8,NVN_FORMAT_BGRA8_SRGB,NVN_FORMAT_RGBA_DXT1,NVN_FORMAT_DEPTH16,NVN_FORMAT_RGB9E5F,NVN_FORMAT_RGB565};
 for(unsigned fi=0;fi<sizeof(unknown_formats)/sizeof(*unknown_formats);fi++){
  NVNtextureBuilder ib={0};NVNtexture image={0};OK(api_nvnTextureBuilderSetDefaults(&ib));OK(api_nvnTextureBuilderSetDevice(&ib,&device));
  OK(api_nvnTextureBuilderSetSize2D(&ib,17,9));OK(api_nvnTextureBuilderSetFormat(&ib,unknown_formats[fi]));OK(api_nvnTextureBuilderSetStorage(&ib,pools+DATA_B,4096));
  T(api_nvnTextureInitialize(&image,&ib));OK(api_nvnTexturePoolRegisterTexture(&pbb,258,&image,NULL));T(!capture(r,&old));
  struct nn_pipeline_query fq={"QueryTextureFormat",3,NN_PIPELINE_QUERY_I32,1,{112,-1,0,0}};memset(out,0xa5,sizeof(out));n=99;
  T(nn_pipeline_resource_query(&old.state,1,&fq,out,&n)==ENOTSUP&&!n);for(unsigned i=0;i<32;i++)T(!out[i]);
  OK(api_nvnTextureFinalize(&image));unpin(&old);remove_object(find(&ib,TB));
 }
#endif
 NVNcommandHandle handle=api_nvnCommandBufferEndRecording(&command);T(handle&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&device,handle));OK(api_nvnCommandBufferFinalize(&command));
 OK(api_nvnTextureFinalize(tex+1));OK(api_nvnTexturePoolFinalize(&pa));OK(api_nvnTexturePoolFinalize(&pbb));
 for(unsigned i=0;i<COUNT;i++){if(i!=DATA_A)OK(api_nvnMemoryPoolFinalize(pools+i));free(memory[i]);}
 OK(api_nvnDeviceFinalize(&device));while(objects){T(builder_kind(objects->kind));remove_object(objects);}
 T(!handles);printf("HOST ONLY resource-query actual NVN snapshot checks=%u image-format-cases=%u; GPU execution=false\n",checks,image_format_cases);return 0;
}
