/* SPDX-License-Identifier: MIT. HOST metadata only; no pixel conversion. */
#include "nvn_native/texture_volume.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define C(x) do {checks++;assert(x);}while(0)
static uint64_t address(const struct nn_tex_volume_layout *l,unsigned level,unsigned z,unsigned x,unsigned y,unsigned b)
{uint64_t a=UINT64_MAX;C(!nn_tex_volume_offset(l,level,z,x,y,b,&a));return a;}
int main(void)
{
 struct nn_tex_volume_desc d={0x25,16,16,16,1,-1};struct nn_tex_volume_layout l;
 C(!nn_tex_volume_layout(&d,&l));C(l.base_block_depth_log2==4&&l.payload_bytes==16384);
 /* Independently published tegra_swizzle gob_address_z example: two GOB
  * rows contain columns0..15 and16..31 for 16x16x16 RGBA8. */
 C(address(&l,0,1,0,0,0)==512);C(address(&l,0,15,0,0,0)==7680);
 C(address(&l,0,0,0,8,0)==8192);C(address(&l,0,15,0,8,0)==15872);
 C(address(&l,0,15,15,15,3)==16383);
 d.levels=5;C(!nn_tex_volume_layout(&d,&l));
 const uint64_t offsets[]={0,16384,20480,22528,23552},sizes[]={16384,4096,2048,1024,512};
 for(unsigned i=0;i<5;i++){C(l.level[i].offset==offsets[i]);C(l.level[i].bytes==sizes[i]);C(l.level[i].depth==(16u>>i));C(l.level[i].block_depth_log2==4-i);}
 C(l.payload_bytes==24064);
 d=(struct nn_tex_volume_desc){0x42,32,32,32,1,-1};C(!nn_tex_volume_layout(&d,&l));C(l.payload_bytes==16384&&l.level[0].blocks_x==8&&l.level[0].blocks_y==8);
 /* Published mip_block_depth vectors distinguish base depth33 from a new
  * independently chosen layout at each mip. */
 d=(struct nn_tex_volume_desc){0x25,16,16,33,6,-1};C(!nn_tex_volume_layout(&d,&l));C(l.level[1].depth==16&&l.level[1].block_depth_log2==4);
 static const unsigned formats[]={1,5,6,7,8,9,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x38,0x9a,0x9c,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50};
 for(unsigned f=0;f<sizeof(formats)/sizeof(*formats);f++)for(unsigned depth=1;depth<=33;depth++){
  d=(struct nn_tex_volume_desc){formats[f],37,21,depth,6,-1};C(!nn_tex_volume_layout(&d,&l));C(!nn_tex_volume_validate(&l));
  for(unsigned level=0;level<l.levels;level++){
   const struct nn_tex_volume_level *m=l.level+level;
   uint64_t first=address(&l,level,0,0,0,0),last=address(&l,level,m->depth-1,m->blocks_x-1,m->blocks_y-1,l.bytes_per_block-1);
   C(first==m->offset);C(last>=first&&last<m->offset+m->bytes);
   uint64_t z;C(nn_tex_volume_offset(&l,level,m->depth,0,0,0,&z)==EINVAL&&z==0);
  }
  /* Base dimensions define a different valid layout if padding and all
   * derived fields happen to remain equal; mutate a derived mip instead. */
  struct nn_tex_volume_layout bad=l;bad.level[1].depth++;C(nn_tex_volume_validate(&bad)==EINVAL);
  bad=l;bad.level[1].offset++;C(nn_tex_volume_validate(&bad)==EINVAL);
  bad=l;bad.payload_bytes++;C(nn_tex_volume_validate(&bad)==EINVAL);
 }
 /* Source IMAGE metadata retains its permission while the canonical
  * address equation stays the same. No ImageStore or native view claim. */
 for(unsigned f=0;f<sizeof(formats)/sizeof(*formats);f++){
  struct nn_tex_source_desc source={.target=2,.format=formats[f],.width=37,.height=21,.layers=1,.levels=6,.block_height_log2=-1,.depth=33};
  struct nn_tex_source_layout plain,image;C(!nn_tex_source_layout(&source,&plain));
  source.flags=4;C(!nn_tex_source_layout(&source,&image));C(image.flags==4&&!nn_tex_source_validate(&image));
  image.flags=0;C(!memcmp(&plain,&image,sizeof(image)));
  for(unsigned bit=0;bit<10;bit++)if(bit!=2){source.flags=1u<<bit;memset(&image,0xa5,sizeof(image));C(nn_tex_source_layout(&source,&image)==ENOTSUP);struct nn_tex_source_layout zero={0};C(!memcmp(&image,&zero,sizeof(image)));}
 }
 d=(struct nn_tex_volume_desc){0x25,2048,2048,2048,1,-1};C(nn_tex_volume_layout(&d,&l)==EOVERFLOW);struct nn_tex_volume_layout zero={0};C(!memcmp(&l,&zero,sizeof(l)));
 d.depth=2049;C(nn_tex_volume_layout(&d,&l)==ENOTSUP);C(!memcmp(&l,&zero,sizeof(l)));
 d=(struct nn_tex_volume_desc){0x25,1,1,1,2,-1};C(nn_tex_volume_layout(&d,&l)==EINVAL);
 C(nn_tex_volume_layout(NULL,&l)==EINVAL);C(nn_tex_volume_layout(&d,NULL)==EINVAL);C(nn_tex_volume_validate(NULL)==EINVAL);
 printf("{\"passed\":true,\"metadata_checks\":%u,\"formats\":37,\"GPU_execution\":false,\"public_NVNSDK_chooser_verified\":false}\n",checks);
 return 0;
}
