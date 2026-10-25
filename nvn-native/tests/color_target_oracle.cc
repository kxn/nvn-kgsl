/* SPDX-License-Identifier: MIT. HOST ONLY original color closed form + actual
 * Mesa pack/unpack utility. No GPU code/ISA interpreter or runtime conversion. */
#include "color_target_cases.h"
#include "nvn_native/color_target.h"
#include "util/format/u_format.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
static enum pipe_format format(unsigned f)
{
 switch(f){
 case NVN_FORMAT_R8:return PIPE_FORMAT_R8_UNORM;case NVN_FORMAT_R8SN:return PIPE_FORMAT_R8_SNORM;
 case NVN_FORMAT_R16F:return PIPE_FORMAT_R16_FLOAT;case NVN_FORMAT_R16:return PIPE_FORMAT_R16_UNORM;case NVN_FORMAT_R16SN:return PIPE_FORMAT_R16_SNORM;case NVN_FORMAT_R32F:return PIPE_FORMAT_R32_FLOAT;
 case NVN_FORMAT_RG8:return PIPE_FORMAT_R8G8_UNORM;case NVN_FORMAT_RG8SN:return PIPE_FORMAT_R8G8_SNORM;
 case NVN_FORMAT_RG16F:return PIPE_FORMAT_R16G16_FLOAT;case NVN_FORMAT_RG16:return PIPE_FORMAT_R16G16_UNORM;case NVN_FORMAT_RG16SN:return PIPE_FORMAT_R16G16_SNORM;case NVN_FORMAT_RG32F:return PIPE_FORMAT_R32G32_FLOAT;
 case NVN_FORMAT_RGBA8:return PIPE_FORMAT_R8G8B8A8_UNORM;case NVN_FORMAT_RGBA8SN:return PIPE_FORMAT_R8G8B8A8_SNORM;
 case NVN_FORMAT_RGBA16F:return PIPE_FORMAT_R16G16B16A16_FLOAT;case NVN_FORMAT_RGBA16:return PIPE_FORMAT_R16G16B16A16_UNORM;case NVN_FORMAT_RGBA16SN:return PIPE_FORMAT_R16G16B16A16_SNORM;case NVN_FORMAT_RGBA32F:return PIPE_FORMAT_R32G32B32A32_FLOAT;
 case NVN_FORMAT_RGBA8_SRGB:return PIPE_FORMAT_R8G8B8A8_SRGB;case NVN_FORMAT_BGRA8:return PIPE_FORMAT_B8G8R8A8_UNORM;case NVN_FORMAT_BGRA8_SRGB:return PIPE_FORMAT_B8G8R8A8_SRGB;
 case NVN_FORMAT_R11G11B10F:return PIPE_FORMAT_R11G11B10_FLOAT;default:return PIPE_FORMAT_NONE;}
}
static void write_file(const std::string&p,const void*data,size_t n)
{FILE*f=fopen(p.c_str(),"wbx");if(!f||fwrite(data,1,n,f)!=n||fclose(f)){perror(p.c_str());exit(1);}}
static int generate(const char*directory)
{
 if(!std::filesystem::create_directory(directory))return 1;
 printf("{\"formats\":[");
 for(unsigned fi=0;fi<NN_COLOR_FORMATS;fi++){
  unsigned f=nn_color_formats[fi];auto pf=format(f);const auto*trait=util_format_description(pf);
  nn_tex_native_view view;nn_color_target_info info;
  if(pf==PIPE_FORMAT_NONE||nn_color_target_create(0x06050002,f,NN_COLOR_WIDTH,NN_COLOR_HEIGHT,0,&view,&info))return 2;
  if(info.bytes_per_pixel!=util_format_get_blocksize(pf)||trait->colorspace==UTIL_FORMAT_COLORSPACE_ZS)return 3;
  printf("%s{\"format\":%u,\"cpp\":%u,\"component_mask\":%u,\"srgb\":%u,\"pitch\":%u,\"span\":%u,\"bytes\":%u,\"alignment\":%u,\"rb_mrt_buf_info\":%u,\"sp_ps_mrt_reg\":%u,\"channel_bits\":[%u,%u,%u,%u],\"floating\":%s,\"snorm\":%s}",fi?",":"",f,info.bytes_per_pixel,info.component_mask,info.srgb,view.pitch,view.required_span_bytes,view.bytes,view.alignment,view.rb_mrt_buf_info,view.sp_ps_mrt_reg,trait->channel[0].size,trait->channel[1].size,trait->channel[2].size,trait->channel[3].size,util_format_is_float(pf)?"true":"false",util_format_is_snorm(pf)?"true":"false");
  unsigned width=NN_COLOR_WIDTH,height=NN_COLOR_HEIGHT,cpp=info.bytes_per_pixel;
  for(unsigned state=0;state<NN_COLOR_STATES;state++){
   std::vector<unsigned char> pixels(size_t(width)*height*cpp,0);
   for(unsigned repeat=0;repeat<2;repeat++){
    float source[4];nn_color_input(state,repeat,source);
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
     if(repeat&&(double(x)+.5)/width<.25)continue;
     auto*p=pixels.data()+(size_t(y)*width+x)*cpp;float previous[4],color[4];util_format_unpack_rgba(pf,previous,p,1);
     for(unsigned c=0;c<4;c++)color[c]=state==1&&c<3?source[c]*source[3]+previous[c]*(1-source[3]):source[c];
     if(state==2){color[1]=previous[1];color[3]=previous[3];}
     util_format_pack_rgba(pf,p,color,1);
    }
    unsigned number=(fi*3+state)*2+repeat;char leaf[80];snprintf(leaf,sizeof(leaf),"/expected-%03u.raw",number);write_file(std::string(directory)+leaf,pixels.data(),pixels.size());
    std::vector<float> unpacked(size_t(width)*height*4);util_format_unpack_rgba(pf,unpacked.data(),pixels.data(),width*height);
    snprintf(leaf,sizeof(leaf),"/expected-%03u.f32",number);write_file(std::string(directory)+leaf,unpacked.data(),unpacked.size()*4);
   }
  }
 }
 puts("],\"gpu_execution\":false,\"cases\":132}");return 0;
}
int main(int argc,char**argv)
{
 if(argc==3&&strcmp(argv[1],"generate")==0)return generate(argv[2]);
 if(argc!=7||strcmp(argv[1],"decode"))return 1;
 unsigned args[5];for(unsigned i=0;i<5;i++){char*end;errno=0;unsigned long x=strtoul(argv[i+2],&end,0);if(errno||*end||x>UINT32_MAX)return 2;args[i]=unsigned(x);}
 unsigned f=args[0],width=args[1],height=args[2],pitch=args[3],offset=args[4];auto pf=format(f);
 if(pf==PIPE_FORMAT_NONE||width!=NN_COLOR_WIDTH||height!=NN_COLOR_HEIGHT||pitch<width*util_format_get_blocksize(pf)||pitch>65536||offset!=4096)return 3;
 std::vector<unsigned char> bytes;unsigned char block[4096];size_t n;while((n=fread(block,1,sizeof(block),stdin))){bytes.insert(bytes.end(),block,block+n);if(bytes.size()>1048576)return 4;}
 if(ferror(stdin)||bytes.size()<offset+size_t(pitch)*height)return 5;
 std::vector<float> unpacked(size_t(width)*height*4);
 for(unsigned y=0;y<height;y++)util_format_unpack_rgba(pf,unpacked.data()+size_t(y)*width*4,bytes.data()+offset+size_t(y)*pitch,width);
 return fwrite(unpacked.data(),4,unpacked.size(),stdout)==unpacked.size()?0:6;
}
