/* SPDX-License-Identifier: MIT.
 * Published Generic16Bx2 volume addressing; normal tileHeight=one GOB,
 * tileDepth chosen from depth. No array-layer substitution, CPU pixel copy,
 * native FDL allocation, NVN public entrypoint, or GPU execution here.
 * NVN53.314 chooser equivalence remains unverified in this candidate. */
static uint64_t align_volume(uint64_t x,uint64_t a){return(x+a-1)&~(a-1);}
int nn_tex_volume_layout(const struct nn_tex_volume_desc *d,struct nn_tex_volume_layout *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));
 if(!d)return EINVAL;
 if(!d->width||!d->height||!d->depth||d->width>2048||d->height>2048||d->depth>2048||
    !d->levels||d->levels>NN_TEX_SOURCE_MAX_LEVELS||d->block_depth_log2< -1||d->block_depth_log2>4)return ENOTSUP;
 uint32_t max=d->width>d->height?d->width:d->height;if(d->depth>max)max=d->depth;
 unsigned levels=1;while(max>>=1)levels++;if(d->levels>levels)return EINVAL;
 struct nn_tex_volume_layout l;memset(&l,0,sizeof(l));l.schema=1;l.format=d->format;l.levels=d->levels;
 int e=format(d->format,&l.block_w,&l.block_h,&l.bytes_per_block);if(e)return e;
 unsigned bd=0;
 if(d->block_depth_log2>=0)bd=(unsigned)d->block_depth_log2;
 else {uint32_t half=d->depth+d->depth/2;while(bd<4&&half>=(2u<<bd))bd++;}
 l.base_block_depth_log2=bd;uint64_t offset=0;
 for(unsigned i=0;i<d->levels;i++){
  uint32_t w=d->width>>i,h=d->height>>i,z=d->depth>>i;if(!w)w=1;if(!h)h=1;if(!z)z=1;
  uint32_t bx=(w+l.block_w-1)/l.block_w,by=(h+l.block_h-1)/l.block_h,row=bx*l.bytes_per_block;
  unsigned sh=bd;while(sh&&z<=(1u<<(sh-1)))sh--;
  uint64_t pitch=align_volume(row,64),bytes=pitch*align_volume(by,8)*align_volume(z,1u<<sh);
  if(bytes>UINT32_MAX||offset>UINT32_MAX-bytes)return EOVERFLOW;
  l.level[i]=(struct nn_tex_volume_level){.offset=offset,.bytes=bytes,.width=w,.height=h,.depth=z,
   .blocks_x=bx,.blocks_y=by,.row_bytes=row,.pitch=(uint32_t)pitch,.block_depth_log2=sh};offset+=bytes;
 }
 l.payload_bytes=offset;*out=l;return 0;
}
int nn_tex_volume_validate(const struct nn_tex_volume_layout *l)
{
 if(!l||l->schema!=1)return EINVAL;
 struct nn_tex_volume_desc d={l->format,l->level[0].width,l->level[0].height,l->level[0].depth,l->levels,(int32_t)l->base_block_depth_log2};
 struct nn_tex_volume_layout expected;
 if(nn_tex_volume_layout(&d,&expected)||memcmp(l,&expected,sizeof(expected)))return EINVAL;
 return 0;
}
int nn_tex_volume_offset(const struct nn_tex_volume_layout *l,uint32_t level,uint32_t z,
 uint32_t bx,uint32_t by,uint32_t byte,uint64_t *out)
{
 if(!out)return EINVAL;
 *out=0;if(nn_tex_volume_validate(l)||level>=l->levels||byte>=l->bytes_per_block)return EINVAL;
 const struct nn_tex_volume_level *m=l->level+level;
 if(z>=m->depth||bx>=m->blocks_x||by>=m->blocks_y)return EINVAL;
 uint64_t x=(uint64_t)bx*l->bytes_per_block+byte,depth=UINT64_C(1)<<m->block_depth_log2;
 uint64_t slab=(uint64_t)m->pitch*align_volume(m->blocks_y,8)*depth;
 uint64_t address=(z/depth)*slab+(uint64_t)(by/8)*m->pitch*8*depth+
  (x/64)*512*depth+(z%depth)*512;
 address+=((x%64)/32)*256+((by%8)/2)*64+((x%32)/16)*32+(by%2)*16+(x%16);
 if(address>=m->bytes||m->offset+address>=l->payload_bytes)return EINVAL;
 *out=m->offset+address;return 0;
}
