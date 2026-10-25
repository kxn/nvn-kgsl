/* HOST/Android CPU metadata only. No device, BO, pixel data or GPU submit. */
#include "nvn_native/color_target.h"
#include "nvn_native/runtime_fdl.h"
#include "nvn_native/pm4.h"
#include "nvn.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,positive,negative;
static struct nn_tex_native_view first_view[22];
static struct nn_color_target_info first_info[22];
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"color_target:%d check%u\n",__LINE__,checks);return 1;}}while(0)
struct test_format {uint32_t format,cpp,mask,srgb;};
static const struct test_format formats[]={
 {NVN_FORMAT_R8,1,1,0},{NVN_FORMAT_R8SN,1,1,0},{NVN_FORMAT_R16F,2,1,0},
 {NVN_FORMAT_R16,2,1,0},{NVN_FORMAT_R16SN,2,1,0},{NVN_FORMAT_R32F,4,1,0},
 {NVN_FORMAT_RG8,2,3,0},{NVN_FORMAT_RG8SN,2,3,0},{NVN_FORMAT_RG16F,4,3,0},
 {NVN_FORMAT_RG16,4,3,0},{NVN_FORMAT_RG16SN,4,3,0},{NVN_FORMAT_RG32F,8,3,0},
 {NVN_FORMAT_RGBA8,4,15,0},{NVN_FORMAT_RGBA8SN,4,15,0},{NVN_FORMAT_RGBA16F,8,15,0},
 {NVN_FORMAT_RGBA16,8,15,0},{NVN_FORMAT_RGBA16SN,8,15,0},{NVN_FORMAT_RGBA32F,16,15,0},
 {NVN_FORMAT_BGRA8,4,15,0},{NVN_FORMAT_RGBA8_SRGB,4,15,1},{NVN_FORMAT_BGRA8_SRGB,4,15,1},
 {NVN_FORMAT_R11G11B10F,4,7,0}};
static int zero(const void*p,size_t n){const unsigned char*b=p;while(n--)if(*b++)return 0;return 1;}
int main(void)
{
 const uint32_t dims[][2]={{1,1},{3,5},{73,41},{257,113},{1153,641},{2880,1620},{16384,1},{1,16384}};
 const uint32_t regs[]={0x8822,0x8823,0x8824,0xa996,0x8102,0x880f,0xa98a};
 for(unsigned f=0;f<sizeof(formats)/sizeof(formats[0]);f++)for(unsigned i=0;i<sizeof(dims)/sizeof(dims[0]);i++){
  const struct test_format*t=formats+f;struct nn_tex_native_view v;struct nn_color_target_info info,again;
  C(!nn_color_target_create(0x06050002,t->format,dims[i][0],dims[i][1],0,&v,&info));
  C(info.bytes_per_pixel==t->cpp&&info.component_mask==t->mask&&info.srgb==t->srgb);
  C(!nn_color_target_validate(&v,&again)&&!memcmp(&info,&again,sizeof(info)));
  if(!i){first_view[f]=v;first_info[f]=info;}
  struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=t->format,.target=1,.width=dims[i][0],.height=dims[i][1],.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=3};
  struct nn_fdl_image_view expected;C(!nn_fdl_image(&d,&expected));
  C(v.rb_mrt_buf_info==expected.rb_mrt_buf_info&&v.rb_mrt_pitch==expected.rb_mrt_pitch&&v.rb_mrt_array_pitch==expected.rb_mrt_array_pitch&&v.sp_ps_mrt_reg==expected.sp_ps_mrt_reg);
  C(info.color_format==expected.sp_ps_mrt_reg&&!(info.color_format&~255u));
  C(v.required_span_bytes==expected.required_span&&v.bytes==expected.bytes&&v.pitch==expected.level[0].pitch);
  C(!memcmp(v.texture_descriptor,expected.descriptor,sizeof(expected.descriptor)));
  uint32_t words[18],saved[18];for(unsigned k=0;k<18;k++)words[k]=saved[k]=0xa5a5a5a5u;size_t n=777;
  C(!nn_color_target_words(&v,0,0,&n)&&n==14);
  C(nn_color_target_words(&v,words,13,&n)==ENOSPC&&!n&&!memcmp(words,saved,sizeof(words)));
  C(!nn_color_target_words(&v,words,18,&n)&&n==14);
  const uint32_t values[]={expected.rb_mrt_buf_info,expected.rb_mrt_pitch,expected.rb_mrt_array_pitch,expected.sp_ps_mrt_reg,expected.sp_ps_mrt_reg,t->srgb,t->srgb};
  for(unsigned k=0;k<7;k++)C(words[k*2]==nvn_pm4_packet4(regs[k],1)&&words[k*2+1]==values[k]&&nvn_pm4_packet_words(words[k*2])==2);
  for(unsigned k=14;k<18;k++)C(words[k]==saved[k]);
  for(unsigned k=0;k<sizeof(v)/4;k++){
   struct nn_tex_native_view bad=v;uint32_t value;memcpy(&value,(char*)&bad+k*4,4);value^=1;memcpy((char*)&bad+k*4,&value,4);
   memset(&again,0xa5,sizeof(again));C(nn_color_target_validate(&bad,&again)&&zero(&again,sizeof(again)));negative++;
  }
  positive++;
 }
 const uint32_t reject[]={0,NVN_FORMAT_R8UI,NVN_FORMAT_R8I,NVN_FORMAT_R32UI,NVN_FORMAT_RGBA16UI,NVN_FORMAT_RGBA32I,NVN_FORMAT_RGB_DXT1,NVN_FORMAT_RGBA_DXT1_SRGB,NVN_FORMAT_DEPTH16,NVN_FORMAT_DEPTH32F,NVN_FORMAT_STENCIL8,UINT32_MAX};
 for(unsigned i=0;i<sizeof(reject)/sizeof(reject[0]);i++){
  struct nn_tex_native_view v;struct nn_color_target_info info;memset(&v,0xa5,sizeof(v));memset(&info,0xa5,sizeof(info));
  C(nn_color_target_create(0x06050002,reject[i],64,64,0,&v,&info)==ENOTSUP&&zero(&v,sizeof(v))&&zero(&info,sizeof(info)));negative++;
 }
 for(unsigned i=0;i<6;i++){
  struct nn_tex_native_view v;struct nn_color_target_info info;
  uint32_t chip=i==0?0x43030b00:0x06050002,w=i==1?0:i==2?16385:64,h=i==3?0:64,pitch=i==4?257:0;
  int error=nn_color_target_create(chip,i==5?NVN_FORMAT_RGBA16F:NVN_FORMAT_RGBA8,w,h,i==5?512:pitch,&v,&info);
  if(error!=ENOTSUP)fprintf(stderr,"negative%u errno%d\n",i,error);
  C(error==ENOTSUP&&zero(&v,sizeof(v))&&zero(&info,sizeof(info)));negative++;
 }
 /* Actual FDL explicit-pitch profile remains limited to original RGBA8. */
 for(unsigned pitch=256;pitch<=768;pitch+=256){struct nn_tex_native_view v;int create=nn_color_target_create(0x06050002,NVN_FORMAT_RGBA8,64,64,pitch,&v,0);int validate=create?0:nn_color_target_validate(&v,0);if(create||validate)fprintf(stderr,"pitch%u create%d validate%d\n",pitch,create,validate);C(!create);C(!validate);positive++;}
 size_t n=1;struct nn_color_target_info info;C(nn_color_target_validate(0,&info)==EINVAL&&zero(&info,sizeof(info)));C(nn_color_target_words(0,0,0,&n)==EINVAL&&!n);
 C(nn_color_target_words(0,0,1,&n)==EINVAL&&!n);C(nn_color_target_words(0,0,0,0)==EINVAL);
 C(positive==179);
 printf("{\"passed\":true,\"checks\":%u,\"formats\":22,\"positive_views\":%u,\"negative_cases\":%u,\"gpu_execution\":false,\"device_opens\":0,\"gpu_allocations\":0,\"format_metadata\":[",checks,positive,negative);
 for(unsigned i=0;i<22;i++){
  const struct nn_tex_native_view*v=first_view+i;const struct nn_color_target_info*t=first_info+i;
  printf("%s{\"public_format\":%u,\"cpp\":%u,\"component_mask\":%u,\"srgb\":%u,\"rb_mrt_buf_info\":%u,\"sp_ps_mrt_reg\":%u,\"pitch\":%u,\"alignment\":%u}",i?",":"",v->format,t->bytes_per_pixel,t->component_mask,t->srgb,v->rb_mrt_buf_info,v->sp_ps_mrt_reg,v->pitch,v->alignment);
 }
 puts("]}");return 0;
}
