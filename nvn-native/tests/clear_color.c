/* SPDX-License-Identifier: MIT. HOST metadata/real Mesa reference only. */
#include "nvn_native/clear_color.h"
#include "util/format/u_format.h"
#include NN_CLEAR_COLOR_HEADER
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
struct nn_device {uint32_t chip;};struct nn_bo {uint64_t address;size_t size;};
uint32_t nn_device_chip(const nn_device *d){return d->chip;}
uint64_t nn_bo_address(const nn_bo *b){return b->address;}
size_t nn_bo_size(const nn_bo *b){return b->size;}
static unsigned checks,cases;
#define C(x) do{checks++;assert(x);}while(0)
static const struct {unsigned n,cpp,channels;enum pipe_format pipe;} formats[]={
 {1,1,1,PIPE_FORMAT_R8_UNORM},{2,1,1,PIPE_FORMAT_R8_SNORM},{5,2,1,PIPE_FORMAT_R16_FLOAT},
 {6,2,1,PIPE_FORMAT_R16_UNORM},{7,2,1,PIPE_FORMAT_R16_SNORM},{10,4,1,PIPE_FORMAT_R32_FLOAT},
 {13,2,2,PIPE_FORMAT_R8G8_UNORM},{14,2,2,PIPE_FORMAT_R8G8_SNORM},{17,4,2,PIPE_FORMAT_R16G16_FLOAT},
 {18,4,2,PIPE_FORMAT_R16G16_UNORM},{19,4,2,PIPE_FORMAT_R16G16_SNORM},{22,8,2,PIPE_FORMAT_R32G32_FLOAT},
 {37,4,4,PIPE_FORMAT_R8G8B8A8_UNORM},{38,4,4,PIPE_FORMAT_R8G8B8A8_SNORM},{41,8,4,PIPE_FORMAT_R16G16B16A16_FLOAT},
 {42,8,4,PIPE_FORMAT_R16G16B16A16_UNORM},{43,8,4,PIPE_FORMAT_R16G16B16A16_SNORM},{46,16,4,PIPE_FORMAT_R32G32B32A32_FLOAT},
 {154,4,4,PIPE_FORMAT_B8G8R8A8_UNORM},{56,4,4,PIPE_FORMAT_R8G8B8A8_SRGB},
 {156,4,4,PIPE_FORMAT_B8G8R8A8_SRGB},{63,4,3,PIPE_FORMAT_R11G11B10_FLOAT}
};
static void patterns(void)
{
 const float values[][4]={{0,0,0,0},{1,1,1,1},{-1,-.5f,.5f,2},{.1234f,.4567f,.9876f,.3333f},{-65504,65504,10,-10}};
 for(unsigned f=0;f<22;f++)for(unsigned v=0;v<5;v++)for(unsigned mask=0;mask<16;mask++){
  struct nn_clear_color_pattern p;C(!nn_clear_color_pattern(formats[f].n,values[v],mask,&p));C(p.bytes_per_pixel==formats[f].cpp);
  uint32_t reference[4]={0},expected_mask[4]={0};
  util_format_pack_description(formats[f].pipe)->pack_rgba_float((uint8_t*)reference,formats[f].cpp,values[v],16,1,1);
  for(unsigned logical=0;logical<formats[f].channels;logical++)if(mask&(1u<<logical)){
   unsigned storage=(formats[f].n==154||formats[f].n==156)&&logical<3?2-logical:logical;
   unsigned bits=formats[f].n==63?(logical==2?10:11):formats[f].cpp*8/formats[f].channels;
   unsigned shift=formats[f].n==63?logical*11:storage*bits;
   for(unsigned bit=shift;bit<shift+bits;bit++)expected_mask[bit/32]|=1u<<(bit%32);
  }
  for(unsigned byte=0;byte<16;byte++){
   C(((uint8_t*)p.pattern)[byte]==((uint8_t*)reference)[byte%formats[f].cpp]);
   C(((uint8_t*)p.mask)[byte]==((uint8_t*)expected_mask)[byte%formats[f].cpp]);
  }
 }
 struct nn_clear_color_pattern p,zero={0};float rgba[]={1,0,.5f,1};
 C(!nn_clear_color_pattern(37,rgba,15,&p));C(p.pattern[0]==0xff8000ffu);
 C(!nn_clear_color_pattern(154,rgba,1,&p));C(p.pattern[0]==0xffff0080u&&p.mask[0]==0x00ff0000);
 C(!nn_clear_color_pattern(63,rgba,2,&p));C(p.mask[0]==0x003ff800);
 for(unsigned bad=0;bad<160;bad++){
  unsigned found=0;for(unsigned f=0;f<22;f++)if(formats[f].n==bad)found=1;
  if(!found){C(nn_clear_color_pattern(bad,rgba,15,&p)==ENOTSUP);C(!memcmp(&p,&zero,sizeof(p)));}
 }
 C(nn_clear_color_pattern(37,rgba,16,&p)==EINVAL);C(!memcmp(&p,&zero,sizeof(p)));
 rgba[1]=NAN;C(nn_clear_color_pattern(37,rgba,15,&p)==ENOTSUP);rgba[1]=INFINITY;C(nn_clear_color_pattern(37,rgba,15,&p)==ENOTSUP);
}
int main(void)
{
 patterns();nn_device d={0x06050002};nn_bo db={0x100000000,512*1024*1024},ub={0x200000000,4096};
 struct nn_clear_color_range destination={&db,0,db.size,512},uniform={&ub,0,ub.size,0};
 const unsigned dims[][2]={{1,1},{37,21},{2880,1620},{16384,2}};
 const float rgba[4]={.125f,.5f,.75f,1};
 for(unsigned f=0;f<22;f++)for(unsigned dim=0;dim<4;dim++)for(unsigned linear=0;linear<2;linear++){
  unsigned w=dims[dim][0],h=dims[dim][1];uint64_t pitch=((uint64_t)w*formats[f].cpp+31)&~UINT64_C(31);
  struct nn_tex_source_desc desc={.flags=linear?0x10:0,.target=1,.format=formats[f].n,.width=w,.height=h,.layers=1,.levels=1,.stride=linear?pitch:0,.block_height_log2=-1};
  struct nn_tex_source_layout s;C(!nn_tex_source_layout(&desc,&s));
  const struct nn_clear_color_rect rectangles[]={{0,0,w,h},{1,1,w>2?(int)w-2:0,h>2?(int)h-2:0},{-5,-7,(int)w+10,(int)h+14},{(int)w-1,(int)h-1,100,100},{-100,-100,5,5},{0,0,0,5}};
  for(unsigned mask=0;mask<16;mask++)for(unsigned rect=0;rect<6;rect++){
   struct nn_clear_color_plan p;C(!nn_clear_color_plan(&s,rectangles+rect,rgba,mask,&p));cases++;
   struct nn_clear_color_binding b,zero={0};
   if(p.empty){C(!p.groups[0]&&!p.groups[1]&&!p.groups[2]);C(nn_clear_color_bind(&d,&NVN_COMPILED_IMAGE,&s,&p,&destination,&uniform,&b)==ENOTSUP);C(!memcmp(&b,&zero,sizeof(b)));continue;}
   C(!nn_clear_color_bind(&d,&NVN_COMPILED_IMAGE,&s,&p,&destination,&uniform,&b));C(b.ssbos[0].address==db.address+512);C(b.ssbos[0].bytes==s.payload_bytes);
   C(b.ubos[2].address==ub.address&&b.ubos[2].bytes==64);C(!memcmp(&b.args,&p.args,64));
   C(p.args.x_bytes+p.args.row_bytes<=s.level[0].row_bytes);C(p.args.y_rows+p.args.rows<=h);
   C(p.groups[0]*8>=((p.args.x_bytes&3)+p.args.row_bytes+3)/4);C(p.groups[1]*8>=p.args.rows);
   C(p.groups[2]==1&&p.args.linear==linear&&p.destination_bytes==s.payload_bytes);
   struct nn_clear_color_plan bad=p;bad.args.mask[0]^=1;C(nn_clear_color_bind(&d,&NVN_COMPILED_IMAGE,&s,&bad,&destination,&uniform,&b)==EINVAL);C(!memcmp(&b,&zero,sizeof(b)));
   bad=p;bad.groups[0]++;C(nn_clear_color_bind(&d,&NVN_COMPILED_IMAGE,&s,&bad,&destination,&uniform,&b)==EINVAL);
   struct nn_clear_color_range short_dest=destination;short_dest.owner_bytes=512+s.payload_bytes-1;C(nn_clear_color_bind(&d,&NVN_COMPILED_IMAGE,&s,&p,&short_dest,&uniform,&b)==EINVAL);
   struct nn_clear_color_range prefix=destination;prefix.offset=516;prefix.owner_offset=516;prefix.owner_bytes=db.size-516;C(nn_clear_color_bind(&d,&NVN_COMPILED_IMAGE,&s,&p,&prefix,&uniform,&b)==EINVAL);
   struct nn_clear_color_range short_u=uniform;short_u.owner_bytes=63;C(nn_clear_color_bind(&d,&NVN_COMPILED_IMAGE,&s,&p,&destination,&short_u,&b)==EINVAL);
   C(nn_clear_color_bind(&d,&NVN_COMPILED_IMAGE,&s,&p,&destination,&destination,&b)==EINVAL);
  }
  struct nn_clear_color_plan bad;struct nn_clear_color_rect negative={0,0,-1,4};C(nn_clear_color_plan(&s,&negative,rgba,15,&bad)==EINVAL);
 }
 printf("{\"passed\":true,\"HOST_checks\":%u,\"cases\":%u,\"formats\":22,\"actual_GPU_submits\":0}\n",checks,cases);
 return 0;
}
