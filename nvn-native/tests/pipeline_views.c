/* HOST ONLY cache ownership/failure and actual original runtime FDL values. */
#include "nvn_native/pipeline_views.h"
#include "nvn_native/runtime_fdl.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,allocations,frees,fail_allocation;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
static void *fixture_calloc(size_t count,size_t size)
{allocations++;if(fail_allocation){fail_allocation=0;return NULL;}return calloc(count,size);}
static void fixture_free(void *p){if(p)frees++;free(p);}
#define calloc fixture_calloc
#define free fixture_free
#include NN_PIPELINE_VIEWS_SOURCE
#undef calloc
#undef free
static void check_view(struct nn_pipeline_views *cache,uint32_t w,uint32_t h,uint32_t pitch)
{
 const struct nn_tex_native_view *v=NULL,*again=NULL;size_t before=nn_pipeline_views_count(cache);
 T(!nn_pipeline_views_get(cache,w,h,pitch,&v)&&v);unsigned calls=allocations;
 T(!nn_pipeline_views_get(cache,w,h,pitch,&again)&&again==v&&allocations==calls);
 T(nn_pipeline_views_count(cache)>=before);
 struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=w,.height=h,.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=3,.explicit_pitch=pitch};
 struct nn_fdl_image_view f;T(!nn_fdl_image(&d,&f));
 T(v->schema==1&&v->chip==d.chip&&v->format==37&&v->target==1&&v->levels==1&&v->layers==1&&v->samples==1&&!v->tile_mode&&!v->ubwc);
 T(v->width==w&&v->height==h&&v->pitch==f.level[0].pitch&&v->layer_bytes==f.level[0].layer_stride);
 T(v->bytes==f.bytes&&v->required_span_bytes==f.required_span&&v->alignment==f.alignment);
 T(v->descriptor_address_word==f.descriptor_address_word&&v->descriptor_address_mask==f.descriptor_address_mask&&v->descriptor_offset==f.view_offset);
 T(!memcmp(v->texture_descriptor,f.descriptor,sizeof(v->texture_descriptor)));
 T(v->rb_mrt_buf_info==f.rb_mrt_buf_info&&v->rb_mrt_pitch==f.rb_mrt_pitch&&v->rb_mrt_array_pitch==f.rb_mrt_array_pitch&&v->sp_ps_mrt_reg==f.sp_ps_mrt_reg);
}
int main(void)
{
 struct nn_pipeline_views *cache=(void*)1;
 T(nn_pipeline_views_create(0x43030b00,&cache)==ENOTSUP&&!cache);T(nn_pipeline_views_create(0x06050002,NULL)==EINVAL);
 fail_allocation=1;T(nn_pipeline_views_create(0x06050002,&cache)==ENOMEM&&!cache);
 T(!nn_pipeline_views_create(0x06050002,&cache));T(!nn_pipeline_views_count(cache));
 const struct nn_tex_native_view *v=(void*)1;T(nn_pipeline_views_get(NULL,1,1,0,&v)==EINVAL&&!v);
 T(nn_pipeline_views_get(cache,1,1,0,NULL)==EINVAL);
 fail_allocation=1;T(nn_pipeline_views_get(cache,73,41,0,&v)==ENOMEM&&!v&&!nn_pipeline_views_count(cache));
 const uint32_t widths[]={1,2,7,16,31,37,63,64,65,73,127,128,129,257,1025,2880,4096,16384};
 const uint32_t heights[]={1,3,8,21,41,65,73,80,129,1620,16384};
 for(unsigned i=0;i<sizeof(widths)/sizeof(widths[0]);i++)for(unsigned j=0;j<sizeof(heights)/sizeof(heights[0]);j++)check_view(cache,widths[i],heights[j],0);
 check_view(cache,73,41,512);check_view(cache,2880,1620,11520);check_view(cache,2880,1620,11776);
 size_t count=nn_pipeline_views_count(cache);const struct nn_tex_native_view *saved=NULL;T(!nn_pipeline_views_get(cache,73,41,0,&saved));struct nn_tex_native_view copy=*saved;
 for(unsigned i=0;i<7;i++){v=(void*)1;int e=nn_pipeline_views_get(cache,i==0?0:i==1?16385:73,i==2?0:i==3?16385:41,i==4?1:i==5?291:i==6?0x400000:0,&v);T(e&&!v&&nn_pipeline_views_count(cache)==count);}
 T(!memcmp(saved,&copy,sizeof(copy)));nn_pipeline_views_destroy(cache);nn_pipeline_views_destroy(NULL);T(frees+2==allocations);
 printf("HOST ONLY runtimeFDL immutable views checks=%u entries=%zu allocator_failures=2 noGPU=true\n",checks,count);return 0;
}
