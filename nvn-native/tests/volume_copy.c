/* SPDX-License-Identifier: MIT. Pure binding metadata, no executing queue. */
#include "nvn_native/volume_copy.h"
#include NN_VOLUME_COPY_HEADER
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
struct nn_device {uint32_t chip;};struct nn_bo {uint64_t address;size_t size;};
uint32_t nn_device_chip(const nn_device *d){return d->chip;}
uint64_t nn_bo_address(const nn_bo *b){return b->address;}
size_t nn_bo_size(const nn_bo *b){return b->size;}
static unsigned checks;
#define C(x) do{checks++;assert(x);}while(0)
int main(void)
{
 static const unsigned formats[]={1,5,6,7,8,9,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x38,0x9a,0x9c,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50};
 nn_device device={0x06050002};nn_bo cb={0x100000000,4*1024*1024},lb={0x200000000,4*1024*1024},ub={0x300000000,4096};
 struct nn_block_copy_range c={&cb,0,cb.size,512},n={&lb,0,lb.size,64},u={&ub,0,ub.size,0};
 unsigned cases=0;
 for(unsigned f=0;f<37;f++)for(unsigned z=1;z<=33;z++)for(unsigned mip=0;mip<2;mip++)for(unsigned direction=0;direction<2;direction++){
  struct nn_tex_volume_desc d={formats[f],37,21,z,6,-1};struct nn_tex_volume_layout l;C(!nn_tex_volume_layout(&d,&l));
  const struct nn_tex_volume_level *m=l.level+mip;
  unsigned first=m->depth>2?1:0,slices=m->depth>2?m->depth-2:m->depth;
  unsigned x=m->blocks_x>2?l.bytes_per_block:0,row=m->row_bytes-(x?2*l.bytes_per_block:0),y=m->blocks_y>2?1:0,rows=m->blocks_y-(y?2:0);
  struct nn_volume_copy_region r={mip,first,slices,x,y,row,rows,row+3,(rows-1)*(row+3)+row+5,direction};struct nn_volume_copy_plan p;
  C(!nn_volume_copy_plan(&l,&r,&p));C(p.args.base_z==first);C(p.args.canonical_base==m->offset);C(p.args.block_depth_log2==m->block_depth_log2);
  C(p.args.linear_span==(slices-1)*(uint64_t)r.linear_image_stride+(rows-1)*(uint64_t)r.linear_row_stride+row);
  C(p.linear_bytes>=p.args.linear_span&&p.linear_bytes-p.args.linear_span<4);
  C(p.groups[2]==(direction?slices:1));
  struct nn_volume_copy_binding b,zero={0};
  C(!nn_volume_copy_bind(&device,&NVN_COMPILED_IMAGE,&l,&p,&c,&n,&u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));
  C(b.ssbos[direction?1:0].address==cb.address+c.offset);C(b.ssbos[direction?0:1].address==lb.address+n.offset);C(b.ubos[2].address==ub.address&&b.ubos[2].bytes==64);
  C(!memcmp(&b.args,&p.args,sizeof(p.args)));
  C(nn_volume_copy_bind(&device,&NVN_COMPILED_IMAGE,&l,&p,&c,&n,&u,NN_BLOCK_COPY_ALIAS_UNKNOWN,&b)==ENOTSUP);C(!memcmp(&b,&zero,sizeof(b)));
  struct nn_volume_copy_plan bad=p;bad.args.base_z++;C(nn_volume_copy_bind(&device,&NVN_COMPILED_IMAGE,&l,&bad,&c,&n,&u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)==EINVAL);
  bad=p;bad.groups[0]++;C(nn_volume_copy_bind(&device,&NVN_COMPILED_IMAGE,&l,&bad,&c,&n,&u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)==EINVAL);
  struct nn_block_copy_range shared={&cb,0,cb.size,2*1024*1024};C(!nn_volume_copy_bind(&device,&NVN_COMPILED_IMAGE,&l,&p,&c,&shared,&u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));
  shared.offset=512;C(nn_volume_copy_bind(&device,&NVN_COMPILED_IMAGE,&l,&p,&c,&shared,&u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)==EINVAL);
  struct nn_block_copy_range short_u=u;short_u.owner_bytes=63;C(nn_volume_copy_bind(&device,&NVN_COMPILED_IMAGE,&l,&p,&c,&n,&short_u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)==EINVAL);
  struct nn_block_copy_range prefix=n;prefix.offset=68;prefix.owner_offset=68;prefix.owner_bytes=lb.size-68;
  C(nn_volume_copy_bind(&device,&NVN_COMPILED_IMAGE,&l,&p,&c,&prefix,&u,NN_BLOCK_COPY_DISJOINT_PROVEN,&b)==EINVAL);
  r.base_z=m->depth;C(nn_volume_copy_plan(&l,&r,&bad)==EINVAL);cases++;
 }
 printf("{\"passed\":true,\"HOST_checks\":%u,\"volume_regions\":%u,\"formats\":37,\"actual_GPU_submits\":0}\n",checks,cases);
 return 0;
}
