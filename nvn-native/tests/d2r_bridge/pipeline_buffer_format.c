/* HOST ONLY pure trusted snapshot contract; no public BUFFER registration/GPU. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "nvn_native/pipeline_resource_queries.h"
#include "nvn.h"
_Static_assert(NVN_FORMAT_RGBA8==37 && NVN_FORMAT_R32F==10,"pinned NVN source enum changed");
static unsigned checks;
#define T(x) do{checks++;assert(x);}while(0)
static int query(struct nn_pipeline_resource_queries *ctx,const char *name,unsigned stage,int word,uint32_t *value)
{
 struct nn_pipeline_query q={name,3,NN_PIPELINE_QUERY_I32,1,{word,-1,0,0}};
 uint32_t out[32];memset(out,0xa5,sizeof(out));size_t count=77;
 int e=nn_pipeline_resource_query(ctx,stage,&q,out,&count);
 if(e){T(!count);for(unsigned i=0;i<32;i++)T(!out[i]);}else{T(count==1);*value=out[0];}
 return e;
}
int main(void)
{
 int selected,backing;
 struct nn_pipeline_resource_binding row={.stage=1,.domain=NN_PIPELINE_SEPARATE_TEXTURE,.selector=11,.low_word_defined=1,.payload=257,.guest_carrier=UINT64_C(0x100000101),
  .texture={.descriptor_pool=&selected,.backing_pool=&backing,.registered=1,.descriptor_id=257,.target=10,.format=37,.width=17,.height=1,.depth=1,.levels=1,.samples=0,.offset=64,.bytes=68}};
 struct nn_pipeline_resource_queries ctx={1,1,2,&selected,&row,1};uint32_t v;
 T(!query(&ctx,"QuerySamplerType",1,112,&v)&&v==2);
 T(!query(&ctx,"QueryTextureFormat",1,112,&v)&&v==25&&v!=NVN_FORMAT_RGBA8);
 row.texture.format=NVN_FORMAT_R32F;T(!query(&ctx,"QueryTextureFormat",1,112,&v)&&v==10);
 row.texture.format=38;T(!query(&ctx,"QueryTextureFormat",1,112,&v)&&v==26);
 for(unsigned i=0;i<2;i++){
  row.texture.format=(uint32_t[]){0,UINT32_MAX}[i];T(query(&ctx,"QueryTextureFormat",1,112,&v)!=0);
 }
 row.texture.format=37;row.texture.target=1;
 T(!query(&ctx,"QueryTextureFormat",1,112,&v)&&v==25);
 T(!query(&ctx,"QuerySamplerType",1,112,&v)&&v==3);
 row.texture.target=10;
 T(query(&ctx,"QueryTextureFormat",0,112,&v)==ENOTSUP);
 T(query(&ctx,"QueryTextureFormat",1,110,&v)==ENOTSUP);
 row.domain=NN_PIPELINE_COMBINED;T(query(&ctx,"QueryTextureFormat",1,112,&v)==ENOTSUP);row.domain=NN_PIPELINE_SEPARATE_TEXTURE;
 row.texture.registered=0;T(query(&ctx,"QueryTextureFormat",1,112,&v)==ENOTSUP);row.texture.registered=1;
 row.texture.height=2;T(query(&ctx,"QueryTextureFormat",1,112,&v)==ENOTSUP);row.texture.height=1;
 row.texture.bytes=67;T(query(&ctx,"QueryTextureFormat",1,112,&v)==EINVAL);row.texture.bytes=68;
 struct nn_pipeline_frontend_resource_queries full={0};full.resources=ctx;
 full.graphics.version=1;full.graphics.chip=0x06050002;full.graphics.point_size=1;nn_render_state_defaults(&full.graphics.render);
 struct nn_pipeline_query cap={"QueryHostSupportsSnormBufferTextureFormat",0,NN_PIPELINE_QUERY_BOOL,1,{0}};uint32_t out[32];size_t count;
 T(!nn_pipeline_frontend_resource_query(&full,1,&cap,out,&count)&&count==1&&!out[0]);
 cap.name="backend/unit_render_scale";T(nn_pipeline_frontend_resource_query(&full,1,&cap,out,&count)==ENOTSUP&&!count);
 cap=(struct nn_pipeline_query){"QueryTextureFormat",3,NN_PIPELINE_QUERY_I32,1,{112,-1,0,0}};
 T(!nn_pipeline_frontend_resource_query(&full,1,&cap,out,&count)&&count==1&&out[0]==25);
 cap.type=NN_PIPELINE_QUERY_BOOL;T(nn_pipeline_frontend_resource_query(&full,1,&cap,out,&count)==EINVAL&&!count);
 cap=(struct nn_pipeline_query){"QueryTextureCoordNormalized",3,NN_PIPELINE_QUERY_BOOL,1,{112,-1,0,0}};
 T(nn_pipeline_frontend_resource_query(&full,1,&cap,out,&count)==ENOTSUP&&!count);
 printf("{\"checks\":%u,\"rgba8_shader_format\":25,\"r32f_shader_format\":10,\"snorm_policy\":false,\"gpu_submits\":0,\"public_buffer_registration\":false}\n",checks);return 0;
}
