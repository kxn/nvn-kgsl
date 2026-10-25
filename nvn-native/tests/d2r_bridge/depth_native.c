/* HOST only: real produced v2 recipe/FDL plus RAM ownership fault injection. */
#define main foundation_owner_test_main
#include "depth_attachment.c"
#undef main
#include "nvn_native/depth_native.h"
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/pipeline_shader.h"
#include "nvn_native/pm4.h"
#include NN_DEPTH_RECIPE_HEADER
int nn_bo_idle(const nn_bo*b){return !b?EINVAL:b->busy?EBUSY:0;}
static uint32_t value(const uint32_t*w,size_t n,uint32_t reg){for(size_t i=0;i<n;){uint32_t len=nvn_pm4_packet_words(w[i]);CHECK(len&&len<=n-i);if(w[i]>>28==4){uint32_t first=(w[i]>>8)&0x7ffff;if(reg>=first&&reg-first<len-1)return w[i+1+reg-first];}i+=len;}abort();}
int main(void){
 CHECK(!foundation_owner_test_main());unsigned initial=allocs;
 CHECK(nn_pipeline_depth.recipe==&nn_pipeline_recipe&&nn_pipeline_recipe.version==2);CHECK(!nn_depth_native_recipe(&nn_pipeline_depth));
 struct nn_pipeline_texture_types types={.version=1,.recipe=&nn_pipeline_recipe};struct nn_pipeline_native_artifact artifact={.version=1,.recipe=&nn_pipeline_recipe,.textures=&types};nn_pipeline_program*program=(nn_pipeline_program*)(uintptr_t)1,*retained=(nn_pipeline_program*)(uintptr_t)2;
 CHECK(nn_pipeline_program_create_native(NULL,&artifact,&program,&retained)!=0&&!program&&!retained&&allocs==initial);
 struct nn_device dev={0x06050002};program=(nn_pipeline_program*)(uintptr_t)1;retained=(nn_pipeline_program*)(uintptr_t)2;
 CHECK(nn_pipeline_program_create_native(&dev,&artifact,&program,&retained)==ENOTSUP&&!program&&!retained&&allocs==initial);
 CHECK(nn_pipeline_shader_preflight(NULL,&nn_pipeline_recipe,NULL)!=0&&allocs==initial);
 struct nn_depth_fdl_desc d={0};d.schema=1;d.chip=dev.chip;d.format=0x34;d.target=4;d.width=73;d.height=41;d.levels=3;d.layers=3;d.base_level=1;d.base_layer=2;d.roles=1;struct nn_depth_attachment a={0};CHECK(!nn_depth_attachment_create(&dev,&d,&a));
 struct nn_depth_controls_desc c={1,dev.chip,1,1,8,1,nn_pipeline_depth.depth_regid,0};size_t n=0;CHECK(!nn_depth_native_words(&a,&nn_pipeline_depth,&c,NULL,0,&n)&&n>0&&n<96);
 uint32_t words[96],saved[96];memset(words,0xa5,sizeof(words));memcpy(saved,words,sizeof(words));size_t count=99;CHECK(nn_depth_native_words(&a,&nn_pipeline_depth,&c,words,n-1,&count)==E2BIG&&!count&&!memcmp(words,saved,sizeof(words)));
 CHECK(!nn_depth_native_words(&a,&nn_pipeline_depth,&c,words,96,&count)&&count==n);CHECK(value(words,n,0x8875)==(uint32_t)(a.view.desc.address+a.view.view_offset));CHECK(value(words,n,0x8876)==(uint32_t)((a.view.desc.address+a.view.view_offset)>>32));CHECK(value(words,n,0x8873)==a.view.rb_depth_buffer_pitch);CHECK(value(words,n,0x8874)==a.view.rb_depth_buffer_array_pitch);CHECK(value(words,n,0x8871)==(1u|2u|(7u<<2)|64u));CHECK(value(words,n,0xa98c)==((nn_pipeline_depth.depth_regid<<8)|(252u<<16)|(252u<<24)));CHECK(value(words,n,0x880b)==2);CHECK(!value(words,n,0x8898)&&!value(words,n,0x8100)&&!value(words,n,0x8880)&&!value(words,n,0x8115));
 for(unsigned test=0;test<2;test++)for(unsigned write=0;write<=test;write++)for(unsigned cmp=1;cmp<=8;cmp++){c.test_enable=test;c.write_enable=write;c.compare=cmp;CHECK(!nn_depth_native_words(&a,&nn_pipeline_depth,&c,words,96,&count));CHECK(value(words,count,0x8871)==((test?65u:0u)|(write?2u:0u)|((cmp-1)<<2)));}
 for(unsigned k=0;k<8;k++){struct nn_pipeline_depth_export v=nn_pipeline_depth;switch(k){case 0:v.version=0;break;case 1:v.recipe=NULL;break;case 2:v.writes_depth=0;break;case 3:v.depth_regid=192;break;case 4:v.color_outputs=2;break;case 5:v.late_z_required=0;break;case 6:v.lrz_disabled_required=0;break;case 7:v.depth_regid^=1;break;}memcpy(words,saved,sizeof(words));count=99;CHECK(nn_depth_native_words(&a,&v,&c,words,96,&count)!=0&&!count&&!memcmp(words,saved,sizeof(words)));}
 struct nn_pipeline_shader_recipe modified=nn_pipeline_recipe;struct nn_pipeline_depth_export v=nn_pipeline_depth;v.recipe=&modified;modified.version=1;CHECK(nn_depth_native_recipe(&v)!=0);modified=nn_pipeline_recipe;modified.word_count--;CHECK(nn_depth_native_recipe(&v)!=0);modified=nn_pipeline_recipe;uint32_t copied[4096];memcpy(copied,modified.words,modified.word_count*4);modified.words=copied;for(size_t i=0;i<modified.word_count;i++){if(copied[i]==nvn_pm4_packet4(0x880b,1)){copied[i+1]=0;break;}}CHECK(nn_depth_native_recipe(&v)!=0);
 c.shader_writes_depth=0;CHECK(nn_depth_native_words(&a,&nn_pipeline_depth,&c,words,96,&count)==ENOTSUP&&!count);c.shader_writes_depth=1;c.depth_regid^=1;CHECK(nn_depth_native_words(&a,&nn_pipeline_depth,&c,words,96,&count)==ENOTSUP&&!count);CHECK(!nn_depth_attachment_destroy(&a));CHECK(!maps&&!caches&&!live);
 printf("{\"scope\":\"HOST native depth packet and v2 boundary only\",\"checks\":%u,\"words\":%zu,\"depth_regid\":%u,\"color_outputs\":%u,\"legacy_v2_rejected\":true,\"pixel_map_calls\":%u,\"gpu_execution\":false}\n",checks,n,nn_pipeline_depth.depth_regid,nn_pipeline_depth.color_outputs,maps);
}
