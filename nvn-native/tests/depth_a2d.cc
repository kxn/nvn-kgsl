/* SPDX-License-Identifier: MIT. HOST packet/layout checks, never GPU execution. */
#include "nvn_native/depth_a2d.h"
#include "freedreno_layout.h"
#include "freedreno_dev_info.h"
#include "nvn_native/pm4.h"
#include "adreno/adreno_pm4.xml.h"
#include "fd6_hw.h"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <initializer_list>
static unsigned checks,cases;
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t find(const uint32_t*w,size_t n,uint32_t reg){for(size_t i=0;i<n;){unsigned type=w[i]>>28;if(type==4){unsigned c=w[i]&0x7f,r=(w[i]>>8)&0x3ffff;for(unsigned j=0;j<c;j++)if(r+j==reg)return w[i+1+j];i+=1+c;}else if(type==7)i+=1+(w[i]&0x3fff);else return 0xffffffff;}return 0xffffffff;}
static bool source_coordinates(const uint32_t *w,size_t n,const nn_depth_a2d_region&r)
{
 /* Independently pinned XML fields: int bits8..24, not raw texel integers.
  * Do not form the oracle by reusing the encoder's generated packers. */
 return find(w,n,REG_A6XX_GRAS_A2D_SRC_XMIN)==(r.x<<8)&&
        find(w,n,REG_A6XX_GRAS_A2D_SRC_XMAX)==((r.x+r.width-1)<<8)&&
        find(w,n,REG_A6XX_GRAS_A2D_SRC_YMIN)==(r.y<<8)&&
        find(w,n,REG_A6XX_GRAS_A2D_SRC_YMAX)==((r.y+r.height-1)<<8);
}
int main(void)
{
 C(A6XX_GRAS_A2D_SRC_XMIN__SHIFT==8&&A6XX_GRAS_A2D_SRC_XMAX__SHIFT==8);
 C(A6XX_GRAS_A2D_SRC_YMIN__SHIFT==8&&A6XX_GRAS_A2D_SRC_YMAX__SHIFT==8);
 C(A6XX_GRAS_A2D_SRC_XMIN__MASK==0x01ffff00&&A6XX_GRAS_A2D_SRC_YMAX__MASK==0x01ffff00);
 for(unsigned f:{0x32u,0x33u,0x34u})for(unsigned width:{1u,2u,64u,73u,257u})for(unsigned height:{1u,41u,64u})for(unsigned arrays:{1u,3u})for(unsigned levels:{1u,2u}){
  if(levels==2&&width==1&&height==1)continue;
  for(unsigned base=0;base<levels;base++){
   struct nn_depth_fdl_desc d={};d.schema=1;d.chip=0x06050002;d.format=f;d.target=arrays==1?1:4;d.width=width;d.height=height;d.layers=arrays;d.levels=levels;d.base_level=base;d.base_layer=arrays-1;d.roles=NN_DEPTH_ATTACHMENT;
   struct nn_depth_a2d_view z={};int e=nn_depth_a2d(&d,0,&z);if(e)fprintf(stderr,"format%x dims%ux%u arrays%u levels%u base%u e%d\n",f,width,height,arrays,levels,base,e);C(!e);cases++;
   C(z.tiled.address==0&&z.linear.address==0);C(z.linear.width==z.tiled.width&&z.linear.height==z.tiled.height);C(z.linear.pitch>=z.linear.width*(f==0x32?2:4));C(!z.reserved&&!z.tiled.reserved&&!z.linear.reserved);
   C((z.tiled.source_info&A6XX_TPL1_A2D_SRC_TEXTURE_INFO_TILE_MODE__MASK)==A6XX_TPL1_A2D_SRC_TEXTURE_INFO_TILE_MODE(TILE6_3));C((z.linear.destination_info&A6XX_RB_A2D_DEST_BUFFER_INFO_TILE_MODE__MASK)==0);
   d.address=0x1000000;struct nn_depth_a2d_view v={};C(!nn_depth_a2d(&d,0x8000000,&v));C(v.tiled.offset==z.tiled.offset&&v.linear.bytes==z.linear.bytes);
   nn_depth_a2d_region r={0,0,v.tiled.width,v.tiled.height};
   for(unsigned op:{0u,1u,2u}){
    uint32_t w[128],old[128];memset(w,0xab,sizeof(w));memcpy(old,w,sizeof(w));size_t n=123;C(!nn_depth_a2d_words(&v,op,&r,0.5f,nullptr,0,&n));C(n>0&&n<96);size_t m=123;C(nn_depth_a2d_words(&v,op,&r,0.5f,w,n-1,&m)==E2BIG&&m==0&&!memcmp(w,old,sizeof(w)));C(!nn_depth_a2d_words(&v,op,&r,0.5f,w,128,&m)&&m==n);C(!memcmp(w+n,old+n,sizeof(w)-n*4));
    C(w[0]==nvn_pm4_packet7(CP_SET_MARKER,1)&&w[1]==A6XX_CP_SET_MARKER_0_MODE(RM6_BLIT2DSCALE));C(w[n-4]==nvn_pm4_packet7(CP_EVENT_WRITE,1)&&w[n-3]==CP_EVENT_WRITE_0_EVENT(DEBUG_LABEL));C(w[n-2]==nvn_pm4_packet7(CP_BLIT,1)&&w[n-1]==CP_BLIT_0_OP(BLIT_OP_SCALE));
    const auto&dst=op==1?v.linear:v.tiled;uint64_t a=dst.address+dst.offset;C(find(w,n,REG_A6XX_RB_A2D_DEST_BUFFER_INFO)==dst.destination_info);C(find(w,n,REG_A6XX_RB_A2D_DEST_BUFFER_BASE)==(uint32_t)a);C(find(w,n,REG_A6XX_RB_A2D_DEST_BUFFER_BASE+1)==a>>32);C(find(w,n,REG_A6XX_RB_A2D_DEST_BUFFER_PITCH)==A6XX_RB_A2D_DEST_BUFFER_PITCH(dst.pitch));
    if(op){const auto&src=op==2?v.linear:v.tiled;C(source_coordinates(w,n,r));C(find(w,n,REG_A6XX_TPL1_A2D_SRC_TEXTURE_BASE)==(uint32_t)(src.address+src.offset));C(find(w,n,REG_A6XX_TPL1_A2D_SRC_TEXTURE_BASE+1)==(src.address+src.offset)>>32);C(find(w,n,REG_A6XX_TPL1_A2D_SRC_TEXTURE_INFO)==(op==2?v.linear:v.tiled).source_info);C(find(w,n,REG_A6XX_TPL1_A2D_SRC_TEXTURE_PITCH)==A6XX_TPL1_A2D_SRC_TEXTURE_PITCH_PITCH((op==2?v.linear:v.tiled).pitch));}else if(f==0x33){C(find(w,n,REG_A6XX_RB_A2D_CLEAR_COLOR_DW0)==255);C(find(w,n,REG_A6XX_RB_A2D_CLEAR_COLOR_DW1)==255);C(find(w,n,REG_A6XX_RB_A2D_CLEAR_COLOR_DW2)==127);}
    m=123;auto bad=v;bad.linear.pitch^=64;C(nn_depth_a2d_words(&bad,op,&r,0.5f,w,128,&m)==EINVAL&&m==0);
   }
   if(v.tiled.width>2&&v.tiled.height>2){
    nn_depth_a2d_region sub={1,2,v.tiled.width-1,v.tiled.height-2};uint32_t packet[128];size_t count=0;
    C(!nn_depth_a2d_words(&v,NN_DEPTH_A2D_READBACK,&sub,0,packet,128,&count));
    C(source_coordinates(packet,count,sub));
    /* Recreate each historical raw-integer field independently: all four must
     * be detected, including nonzero source origins and rectangle maxima. */
    const uint32_t regs[]={REG_A6XX_GRAS_A2D_SRC_XMIN,REG_A6XX_GRAS_A2D_SRC_XMAX,REG_A6XX_GRAS_A2D_SRC_YMIN,REG_A6XX_GRAS_A2D_SRC_YMAX};
    const uint32_t raw[]={sub.x,sub.x+sub.width-1,sub.y,sub.y+sub.height-1};
    for(unsigned k=0;k<4;k++){
     uint32_t badpacket[128];memcpy(badpacket,packet,sizeof(packet));bool changed=false;
     for(size_t i=0;i<count;){unsigned type=badpacket[i]>>28;
      if(type==4){unsigned c=badpacket[i]&0x7f,reg=(badpacket[i]>>8)&0x3ffff;if(regs[k]>=reg&&regs[k]<reg+c){badpacket[i+1+regs[k]-reg]=raw[k];changed=true;}i+=1+c;}
      else if(type==7)i+=1+(badpacket[i]&0x3fff);else break;
     }
     C(changed&&!source_coordinates(badpacket,count,sub));
    }
   }
   auto bad=v;size_t n=1;uint32_t w[128];C(nn_depth_a2d_words(&v,3,&r,0,w,128,&n)==EINVAL&&n==0);auto no_linear=v;no_linear.linear_address=0;C(nn_depth_a2d(&v.desc,0,&no_linear)==0);C(nn_depth_a2d_words(&no_linear,2,&r,0,w,128,&n)==EINVAL&&n==0);C(nn_depth_a2d_words(&bad,0,&r,NAN,w,128,&n)==EINVAL&&n==0);auto region=r;region.x=region.width;C(nn_depth_a2d_words(&v,1,&region,0,w,128,&n)==EINVAL&&n==0);
   C(nn_depth_a2d(&d,d.address,&bad)==EINVAL);d.address++;C(nn_depth_a2d(&d,0x8000000,&bad)!=0);nn_depth_a2d_view zero={};C(!memcmp(&bad,&zero,sizeof(bad)));
  }
 }
 /* Maximum admitted texture extent: each packed coordinate remains legal. */
 {nn_depth_fdl_desc d={};d.schema=1;d.chip=0x06050002;d.format=0x32;d.target=1;d.width=d.height=16384;d.levels=d.layers=1;d.roles=NN_DEPTH_ATTACHMENT;d.address=0x100000000;
  nn_depth_a2d_view v={};C(!nn_depth_a2d(&d,0x200000000,&v));
  nn_depth_a2d_region r={16383,16383,1,1};uint32_t w[128];size_t n=0;
  C(!nn_depth_a2d_words(&v,NN_DEPTH_A2D_READBACK,&r,0,w,128,&n));C(source_coordinates(w,n,r));
  r.x=16384;C(nn_depth_a2d_words(&v,NN_DEPTH_A2D_READBACK,&r,0,w,128,&n)==EINVAL&&n==0);
  r.x=16383;r.y=16384;C(nn_depth_a2d_words(&v,NN_DEPTH_A2D_READBACK,&r,0,w,128,&n)==EINVAL&&n==0);
 }
 printf("{\"checks\":%u,\"layout_cases\":%u,\"host_only\":true,\"gpu_execution\":false}\n",checks,cases);return 0;
}
