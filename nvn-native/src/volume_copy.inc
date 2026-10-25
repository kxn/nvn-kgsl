/* SPDX-License-Identifier: MIT. CPU metadata only. */
_Static_assert(sizeof(struct nn_volume_copy_args)==64,"volume UBO2 contract");
int nn_volume_copy_plan(const struct nn_tex_volume_layout *l,const struct nn_volume_copy_region *r,struct nn_volume_copy_plan *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=nn_tex_volume_validate(l);if(e)return e;
 if(!r||r->level>=l->levels||!r->slices||!r->row_bytes||!r->rows||r->direction>1)return EINVAL;
 const struct nn_tex_volume_level *m=l->level+r->level;
 if(r->base_z>=m->depth||r->slices>m->depth-r->base_z||r->x_bytes%l->bytes_per_block||r->row_bytes%l->bytes_per_block||
    r->x_bytes>m->row_bytes||r->row_bytes>m->row_bytes-r->x_bytes||r->y_rows>m->blocks_y||r->rows>m->blocks_y-r->y_rows||r->linear_row_stride<r->row_bytes)return EINVAL;
 uint64_t last=(uint64_t)(r->rows-1)*r->linear_row_stride+r->row_bytes;
 if(!r->linear_image_stride||last>r->linear_image_stride)return EINVAL;
 uint64_t span=(uint64_t)(r->slices-1)*r->linear_image_stride+last;
 uint64_t slab=(uint64_t)m->pitch*((m->blocks_y+7u)&~7u)*(1u<<m->block_depth_log2);
 if(span>UINT32_MAX-3u||slab>UINT32_MAX||m->offset>UINT32_MAX||l->payload_bytes>UINT32_MAX||l->payload_bytes%4)return EOVERFLOW;
 struct nn_volume_copy_plan p={0};p.args=(struct nn_volume_copy_args){(uint32_t)m->offset,(uint32_t)slab,m->pitch/64,m->block_depth_log2,
  r->x_bytes,r->y_rows,r->row_bytes,r->rows,r->linear_row_stride,r->linear_image_stride,r->slices,r->direction,
  (uint32_t)span,(uint32_t)((span+3)/4),0,r->base_z};
 if(!r->direction){
  uint32_t gx=(p.args.linear_words+63u)/64u;if(gx>65535)gx=65535;
  p.groups[0]=gx;p.args.dispatch_width=gx*8;p.groups[1]=(p.args.linear_words+(uint64_t)gx*64-1)/((uint64_t)gx*64);p.groups[2]=1;
 }else{
  uint64_t words=((r->x_bytes&3u)+(uint64_t)r->row_bytes+3)/4;p.groups[0]=(words+7)/8;p.groups[1]=(r->rows+7)/8;p.groups[2]=r->slices;
 }
 for(unsigned i=0;i<3;i++)if(!p.groups[i]||p.groups[i]>65535)return E2BIG;
 p.canonical_bytes=l->payload_bytes;p.linear_bytes=(span+3)&~UINT64_C(3);p.region=*r;*out=p;return 0;
}
int nn_volume_copy_bind(nn_device *d,const struct nn_cs_image *image,const struct nn_tex_volume_layout *l,const struct nn_volume_copy_plan *p,
 const struct nn_block_copy_range *c,const struct nn_block_copy_range *n,const struct nn_block_copy_range *u,
 enum nn_block_copy_alias proof,struct nn_volume_copy_binding *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=raw_copy_image(d,image);if(e)return e;
 if(!p||proof!=NN_BLOCK_COPY_DISJOINT_PROVEN)return ENOTSUP;
 struct nn_volume_copy_plan expected;if((e=nn_volume_copy_plan(l,&p->region,&expected)))return e;
 if(memcmp(p,&expected,sizeof(expected)))return EINVAL;
 if((e=raw_copy_ranges(c,n,u,p->canonical_bytes,p->linear_bytes,p->args.direction,out->ubos,out->ssbos,out->resources)))return e;
 out->args=p->args;memcpy(out->groups,p->groups,sizeof(out->groups));return 0;
}
