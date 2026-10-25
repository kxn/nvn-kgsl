/* Authored offline adapter. Calls unmodified Khronos CTS math/texture utilities;
 * no GL/Vulkan library, API renderer, guest CPU, or device execution. */
#include "tcuTexLookupVerifier.hpp"
#include "tcuTexVerifierUtil.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <algorithm>
#include <vector>
static float f32(uint32_t u){float v;std::memcpy(&v,&u,4);return v;}
static bool controls(){
 tcu::Texture2D t(tcu::TextureFormat(tcu::TextureFormat::RGBA,tcu::TextureFormat::UNORM_INT8),4,4,3);
 for(int l=0;l<3;l++){t.allocLevel(l);auto a=t.getLevel(l);for(int y=0;y<a.getHeight();y++)for(int x=0;x<a.getWidth();x++)a.setPixel(tcu::Vec4((x+.5f)/4.f,l*.25f,.25f,1.f),x,y);}
 tcu::Sampler s(tcu::Sampler::REPEAT_GL,tcu::Sampler::REPEAT_GL,tcu::Sampler::REPEAT_GL,tcu::Sampler::NEAREST,tcu::Sampler::NEAREST);
 tcu::LookupPrecision p;p.coordBits=tcu::IVec3(20,20,0);p.uvwBits=tcu::IVec3(7,7,0);p.colorThreshold=tcu::Vec4(2.f/255.f);p.colorMask=tcu::BVec4(true);
 auto valid=[&](const tcu::Vec2&uv,const tcu::Vec2&lod,const tcu::Vec4&c){return tcu::isLookupResultValid(t.getView(),s,p,uv,lod,c);};
 const tcu::Vec2 z(0.f),u(.125f,.125f),outside(-.125f,.125f);
 const tcu::Vec4 first(32.f/255.f,0,.25f,1),last(223.f/255.f,0,.25f,1);
 if(!valid(u,z,first)||!valid(outside,z,last))return false;
 if(valid(u,z,tcu::Vec4(1,1,1,1)))return false; // obvious wrong RGBA
 if(valid(tcu::Vec2(.625f,.125f),z,first))return false; // wrong UV
 s.wrapS=tcu::Sampler::CLAMP_TO_EDGE;if(valid(outside,z,last))return false; // wrong wrap
 s.minFilter=tcu::Sampler::NEAREST_MIPMAP_NEAREST;
 if(valid(u,tcu::Vec2(1.f),first))return false; // wrong mip
 s.minFilter=tcu::Sampler::NEAREST_MIPMAP_LINEAR;
 if(valid(u,tcu::Vec2(.25f),tcu::Vec4(first.x(),.5f,.25f,1)))return false; // wrong trilinear pair
 if(valid(u,tcu::Vec2(.25f),first))return false; // interpolation wrongly omitted
 if(valid(u,z,tcu::Vec4(first.x(),first.y(),first.z(),0)))return false; // alpha omitted
 return true;
}
int main(int argc,char**argv){
 if(argc!=2||!controls()){std::fprintf(stderr,"Khronos CTS independent positive/negative controls failed\n");return 2;}
 FILE*f=std::fopen(argv[1],"rb");if(!f)return 3;uint32_t header[2];if(std::fread(header,4,2,f)!=2||header[0]!=0x32535443||!header[1]||header[1]>65536)return 4;
 static const tcu::Sampler::FilterMode filters[]={tcu::Sampler::NEAREST,tcu::Sampler::LINEAR,tcu::Sampler::NEAREST_MIPMAP_NEAREST,tcu::Sampler::LINEAR_MIPMAP_NEAREST,tcu::Sampler::NEAREST_MIPMAP_LINEAR,tcu::Sampler::LINEAR_MIPMAP_LINEAR};
 static const tcu::Sampler::WrapMode wraps[]={tcu::Sampler::REPEAT_GL,tcu::Sampler::MIRRORED_REPEAT_GL,tcu::Sampler::CLAMP_TO_EDGE};
 uint64_t pixels=0,rejected=0,filteredPixels=0,nearestPixels=0;unsigned files=0,firstCase=~0u,firstX=~0u,firstY=~0u;
 std::puts("{\"schema\":1,\"kind\":\"offline-khronos-gles3-HQ-texture-reference-v1\",\"derivateBits\":18,\"lodBits\":6,\"coordBits\":[20,20,0],\"uvwBits\":[7,7,0],\"filtered_storage_units\":2,\"nearest_storage_units\":0,\"independent_negative_controls\":7,\"independent_positive_controls\":2,\"rows\":[");
 for(unsigned n=0;n<header[1];n++){
  uint32_t h[20];if(std::fread(h,4,20,f)!=20)return 5;
  unsigned job=h[0],repeat=h[1],min=h[2],mag=h[3],wrap=h[4],w=h[5],height=h[6],base=h[7],levels=h[8];
  if(min>5||mag>1||wrap>2||!levels||levels>15||!w||w>16384||!height||height>16384||!h[16]||h[16]>16384||!h[17]||h[17]>16384)return 6;
  (void)job;(void)repeat;(void)base;
  tcu::Texture2D texture(tcu::TextureFormat(tcu::TextureFormat::RGBA,tcu::TextureFormat::UNORM_INT8),int(w),int(height),int(levels));
  for(unsigned level=0;level<levels;level++){
   texture.allocLevel(level);auto a=texture.getLevel(level);
   for(int y=0;y<a.getHeight();y++)if(std::fread(static_cast<uint8_t*>(a.getDataPtr())+y*a.getRowPitch(),4,a.getWidth(),f)!=size_t(a.getWidth()))return 5;
  }
  std::vector<uint8_t>image(size_t(h[16])*h[17]*4);if(std::fread(image.data(),1,image.size(),f)!=image.size())return 5;
  float sx=f32(h[11]),sy=f32(h[12]),bias=std::trunc(f32(h[15])*256)/256;
  float low=std::max(0.f,std::min(4095.f/256,f32(h[13]))),high=std::max(0.f,std::min(4095.f/256,f32(h[14])));
  tcu::LodPrecision lod(18,6);auto bounds=tcu::computeLodBoundsFromDerivates(sx*w/float(h[16]),0.f,0.f,sy*height/float(h[17]),lod);bounds+=tcu::Vec2(bias);
  bounds=tcu::clampLodBounds(bounds,tcu::Vec2(low,high),lod);
  tcu::Sampler sampler(wraps[wrap],wraps[wrap],wraps[wrap],filters[min],filters[mag]);
  tcu::LookupPrecision precision;precision.coordBits=tcu::IVec3(20,20,0);precision.uvwBits=tcu::IVec3(7,7,0);precision.colorMask=tcu::BVec4(true);
  float ideal=std::log2(std::max(sx*w/float(h[16]),sy*height/float(h[17])))+bias;bool magged=ideal<0;bool filtered=bool(magged?mag:min&1)||(!magged&&min>=4);
  precision.colorThreshold=tcu::Vec4(filtered?2.f/255.f:0.f);
  uint64_t bad=0;
  for(unsigned y=0;y<h[17];y++)for(unsigned x=0;x<h[16];x++){
   const auto*p=image.data()+(size_t(y)*h[16]+x)*4;tcu::Vec4 actual(p[0]/255.f,p[1]/255.f,p[2]/255.f,p[3]/255.f);
   tcu::Vec2 uv(std::fma((x+.5f)/h[16],sx,f32(h[18])),std::fma((y+.5f)/h[17],sy,f32(h[19])));
   if(!tcu::isLookupResultValid(texture.getView(),sampler,precision,uv,bounds,actual)){bad++;if(firstCase==~0u){firstCase=n;firstX=x;firstY=y;}}
   pixels++;if(filtered)filteredPixels++;else nearestPixels++;
  }
  std::printf("%s{\"raw\":%u,\"failed_pixels\":%llu,\"lod_bounds\":[%.9g,%.9g]}",n?",\n":"",n,(unsigned long long)bad,bounds.x(),bounds.y());std::fflush(stdout);rejected+=bad;files++;
 }
 if(std::fgetc(f)!=EOF||std::fclose(f))return 7;
 std::printf("],\"passed\":%s,\"files\":%u,\"pixels\":%llu,\"channels\":%llu,\"failed_pixels\":%llu,\"filtered_pixels\":%llu,\"nearest_exact_pixels\":%llu,\"first_failed_raw\":%u,\"first_x\":%u,\"first_y\":%u,\"GPU_execution\":false,\"NVN_normative_claim\":false}\n",rejected?"false":"true",files,(unsigned long long)pixels,(unsigned long long)pixels*4,(unsigned long long)rejected,(unsigned long long)filteredPixels,(unsigned long long)nearestPixels,firstCase,firstX,firstY);return rejected?1:0;
}
