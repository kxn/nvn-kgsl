/* HOST-only direct original FDL reference; no GPU or pixel operation. */
#include "nvn_native/depth_fdl.h"
#include "freedreno_layout.h"
#include "freedreno_dev_info.h"
#include "fd6_hw.h"
#include "fd6_format_table.h"
#include "util/format/u_format.h"
#include <nvn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <initializer_list>
static unsigned checks,cases,negative;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);abort();}}while(0)
static bool zero(const void*p,size_t n){const unsigned char*q=(const unsigned char*)p;while(n--)if(*q++)return false;return true;}
static pipe_format pf(unsigned f){switch(f){case 0x32:return PIPE_FORMAT_Z16_UNORM;case 0x33:return PIPE_FORMAT_Z24X8_UNORM;default:return PIPE_FORMAT_Z32_FLOAT;}}
static void compare(const nn_depth_fdl_desc&d)
{
 nn_depth_fdl_view v;CHECK(!nn_depth_fdl(&d,&v));cases++;
 fd_dev_id id={};id.chip_id=d.chip;const struct fd_dev_info*info=fd_dev_info_raw(&id);CHECK(info);
 fdl_image_params p={};p.format=pf(d.format);p.nr_samples=1;p.width0=d.width;p.height0=d.height;p.depth0=1;p.mip_levels=d.levels;p.array_size=d.layers;p.tile_mode=TILE6_3;p.ubwc=false;
 fdl_layout l={};CHECK(fdl6_layout_image(&l,info,&p,nullptr));CHECK(!l.ubwc&&l.base_align==v.alignment&&l.size==v.bytes);
 CHECK(v.view_width==u_minify(d.width,d.base_level)&&v.view_height==u_minify(d.height,d.base_level));
 CHECK(!memcmp(&v.desc,&d,sizeof(d)));CHECK(v.required_span>=v.bytes&&v.bytes_per_pixel==util_format_get_blocksize(p.format));
 for(unsigned i=0;i<d.levels;i++){
  const nn_depth_fdl_level&q=v.level[i];CHECK(q.width==u_minify(d.width,i)&&q.height==u_minify(d.height,i));
  CHECK(q.pitch==fdl_pitch(&l,i)&&q.layer_stride==fdl_layer_stride(&l,i));CHECK(q.offset==fdl_surface_offset(&l,i,0)&&q.span==l.slices[i].size0);
  CHECK(q.tile_mode==TILE6_3&&q.tile_mode==fdl_tile_mode(&l,i));CHECK(q.offset+(uint64_t)q.layer_stride*(d.layers-1)+q.span<=v.required_span);CHECK(!q.reserved);
 }
 CHECK(zero(v.level+d.levels,(NN_DEPTH_FDL_LEVELS-d.levels)*sizeof(v.level[0])));
 fdl_view_args a={};a.iova=d.address;a.format=p.format;a.type=FDL_VIEW_TYPE_2D;a.base_miplevel=d.base_level;a.level_count=1;a.base_array_layer=d.base_layer;a.layer_count=1;a.min_lod_clamp=(float)d.base_level;
 for(unsigned i=0;i<4;i++)a.swiz[i]=d.swizzle[i]==NN_DEPTH_R?PIPE_SWIZZLE_X:d.swizzle[i]==NN_DEPTH_ZERO?PIPE_SWIZZLE_0:PIPE_SWIZZLE_1;
 const fdl_layout*layouts[3]={&l,nullptr,nullptr};fdl6_view view={};fdl6_view_init<A6XX>(&view,layouts,&a,info->props.has_z24uint_s8uint);
 uint32_t descriptor[16];memcpy(descriptor,view.descriptor,sizeof(descriptor));uint64_t mask=uint64_t(A6XX_TEX_MEMOBJ_4_BASE_LO__MASK)|(uint64_t(A6XX_TEX_MEMOBJ_5_BASE_HI__MASK)<<32);
 if(!d.address){descriptor[4]&=~(uint32_t)mask;descriptor[5]&=~(uint32_t)(mask>>32);}
 CHECK(!memcmp(v.descriptor,descriptor,64)&&v.descriptor_address_mask==mask&&v.descriptor_address_word==4);CHECK(v.view_offset==view.offset);
 auto native=fd6_pipe2depth(p.format);CHECK(v.native_depth_format==native);CHECK(v.rb_depth_buffer_info==A6XX_RB_DEPTH_BUFFER_INFO_DEPTH_FORMAT(native));
 CHECK(v.rb_depth_buffer_pitch==A6XX_RB_DEPTH_BUFFER_PITCH(fdl_pitch(&l,d.base_level)));CHECK(v.rb_depth_buffer_array_pitch==A6XX_RB_DEPTH_BUFFER_ARRAY_PITCH(fdl_layer_stride(&l,d.base_level)));CHECK(v.gras_su_depth_buffer_info==A6XX_GRAS_SU_DEPTH_BUFFER_INFO_DEPTH_FORMAT(native));
 nn_depth_fdl_view repeated;memset(&repeated,0xa5,sizeof(repeated));CHECK(!nn_depth_fdl(&d,&repeated)&&!memcmp(&v,&repeated,sizeof(v)));
}
static void bad(nn_depth_fdl_desc d){nn_depth_fdl_view v;memset(&v,0xa5,sizeof(v));CHECK(nn_depth_fdl(&d,&v)!=0&&zero(&v,sizeof(v)));negative++;}
int main()
{
 CHECK(nn_depth_fdl_version()==1);CHECK(sizeof(nn_depth_fdl_desc)==72);CHECK(sizeof(nn_depth_fdl_level)==40);
 for(unsigned f=0x31;f<=0x36;f++){nn_depth_format_info v;CHECK(!nn_depth_fdl_format(f,&v));CHECK(v.schema==1&&v.format==f);CHECK(v.has_depth==(f!=0x31));CHECK(v.has_stencil==(f==0x31||f>=0x35));CHECK(v.roles==((f>=0x32&&f<=0x34)?3u:0u));CHECK(v.depth_bits==(f==0x31?0u:f==0x32?16u:f==0x33||f==0x35?24u:32u));CHECK(v.stencil_bits==((f==0x31||f>=0x35)?8u:0u));CHECK(v.source_bytes==(f==0x31?1u:f==0x32?2u:f==0x36?8u:4u));}
 const unsigned sizes[][2]={{1,1},{8,11},{64,64},{73,41},{257,113},{2880,1620}};
 for(unsigned f=0x32;f<=0x34;f++)for(auto&s:sizes)for(unsigned layers:{1u,3u})for(unsigned levels:{1u,3u}){
  if(levels>1&&s[0]==1)continue;
  for(unsigned mip=0;mip<levels;mip++)for(unsigned layer=0;layer<layers;layer++)for(unsigned address:{0u,1u}){
   nn_depth_fdl_desc d={};d.schema=1;d.chip=0x06050002;d.format=f;d.target=layers==1?1:4;d.width=s[0];d.height=s[1];d.levels=levels;d.layers=layers;d.base_level=mip;d.base_layer=layer;d.roles=3;d.swizzle[0]=0;d.swizzle[1]=1;d.swizzle[2]=1;d.swizzle[3]=2;d.address=address?UINT64_C(0x100000000):0;compare(d);
  }
 }
 nn_depth_fdl_desc d={};d.schema=1;d.chip=0x06050002;d.format=0x32;d.target=1;d.width=64;d.height=64;d.levels=1;d.layers=1;d.roles=1;
 #define BAD(field,value) do{auto v=d;v.field=value;bad(v);}while(0)
 BAD(schema,0);BAD(chip,0x43030b00);BAD(format,0x31);BAD(format,0x35);BAD(format,0x36);BAD(format,37);BAD(target,2);BAD(width,0);BAD(height,16385);BAD(levels,0);BAD(levels,8);BAD(layers,2);BAD(base_level,1);BAD(base_layer,1);BAD(roles,0);BAD(roles,4);BAD(reserved,1);BAD(swizzle[0],3);BAD(address,1);BAD(address,UINT64_C(1)<<49);BAD(address,(UINT64_C(1)<<49)-4096);
 CHECK(nn_depth_fdl(nullptr,nullptr)==EINVAL);nn_depth_fdl_view out;memset(&out,0xa5,sizeof(out));CHECK(nn_depth_fdl(nullptr,&out)==EINVAL&&zero(&out,sizeof(out)));CHECK(nn_depth_fdl_format(0,nullptr)==EINVAL);
 nn_depth_format_info unknown;memset(&unknown,0xa5,sizeof(unknown));CHECK(nn_depth_fdl_format(0,&unknown)==ENOTSUP&&zero(&unknown,sizeof(unknown)));
 for(unsigned test=0;test<2;test++)for(unsigned write=0;write<=test;write++)for(unsigned cmp=1;cmp<=8;cmp++)for(unsigned shader=0;shader<2;shader++){
  nn_depth_controls_desc c={1,0x06050002,test,write,cmp,shader,shader?14u:252u,0};nn_depth_controls v;CHECK(!nn_depth_fdl_controls(&c,&v));
  CHECK(v.rb_depth_cntl==((test?A6XX_RB_DEPTH_CNTL_Z_TEST_ENABLE|A6XX_RB_DEPTH_CNTL_Z_READ_ENABLE:0)|(write?A6XX_RB_DEPTH_CNTL_Z_WRITE_ENABLE:0)|A6XX_RB_DEPTH_CNTL_ZFUNC((adreno_compare_func)(cmp-1))));
  CHECK(v.gras_su_depth_cntl==(test?A6XX_GRAS_SU_DEPTH_CNTL_Z_TEST_ENABLE:0));CHECK(v.rb_ps_output_cntl==(shader?A6XX_RB_PS_OUTPUT_CNTL_FRAG_WRITES_Z:0));CHECK(v.sp_ps_output_cntl==(A6XX_SP_PS_OUTPUT_CNTL_DEPTH_REGID(c.depth_regid)|A6XX_SP_PS_OUTPUT_CNTL_SAMPMASK_REGID(252)|A6XX_SP_PS_OUTPUT_CNTL_STENCILREF_REGID(252)));
  CHECK(v.rb_depth_plane_cntl==A6XX_RB_DEPTH_PLANE_CNTL_Z_MODE(A6XX_LATE_Z)&&v.gras_su_depth_plane_cntl==A6XX_GRAS_SU_DEPTH_PLANE_CNTL_Z_MODE(A6XX_LATE_Z));CHECK(!v.rb_lrz_cntl&&!v.gras_lrz_cntl);
 }
 for(unsigned n=0;n<11;n++){nn_depth_controls_desc c={1,0x06050002,1,1,8,1,14,0};switch(n){case 0:c.schema=0;break;case 1:c.chip=0;break;case 2:c.test_enable=2;break;case 3:c.write_enable=2;break;case 4:c.compare=0;break;case 5:c.compare=9;break;case 6:c.shader_writes_depth=2;break;case 7:c.depth_regid=192;break;case 8:c.test_enable=0;break;case 9:c.shader_writes_depth=0;break;case 10:c.reserved=1;break;}nn_depth_controls v;memset(&v,0xa5,sizeof(v));CHECK(nn_depth_fdl_controls(&c,&v)!=0&&zero(&v,sizeof(v)));negative++;}
 printf("{\"scope\":\"HOST original FDL metadata only\",\"checks\":%u,\"layout_cases\":%u,\"negative_cases\":%u,\"gpu_execution\":false}\n",checks,cases,negative);
}
