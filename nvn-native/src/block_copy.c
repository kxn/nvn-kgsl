/* SPDX-License-Identifier: MIT. Parameter/owner admission, no copy emulation. */
#include "nvn_native/block_copy.h"
#include "nvn_native/volume_copy.h"
#include <errno.h>
#include <string.h>
_Static_assert(sizeof(struct nn_block_copy_args)==64,"existing UBO2 contract");
int nn_block_copy_plan(const struct nn_tex_source_layout *s,const struct nn_block_copy_region *r,struct nn_block_copy_plan *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=nn_tex_source_validate(s);if(e)return e;
 if(!r||s->target==2||s->linear||r->level>=s->levels||r->base_layer>=s->layers||!r->layers||r->layers>s->layers-r->base_layer||
    !r->row_bytes||!r->rows||r->direction>1)return ENOTSUP;
 const struct nn_tex_source_level *m=s->level+r->level;
 if(r->x_bytes%s->bytes_per_block||r->row_bytes%s->bytes_per_block||r->x_bytes>m->row_bytes||r->row_bytes>m->row_bytes-r->x_bytes||
    r->y_rows>m->blocks_y||r->rows>m->blocks_y-r->y_rows||r->linear_row_stride<r->row_bytes)return EINVAL;
 uint64_t last=(uint64_t)(r->rows-1)*r->linear_row_stride+r->row_bytes;
 if(!r->linear_image_stride||last>r->linear_image_stride)return EINVAL;
 uint64_t span=(uint64_t)(r->layers-1)*r->linear_image_stride+last;
 if(span>UINT32_MAX-3u||s->payload_bytes>UINT32_MAX||s->payload_bytes%4||s->layer_stride>UINT32_MAX)return EOVERFLOW;
 uint64_t base=m->offset+s->layer_stride*r->base_layer;
 if(base>UINT32_MAX||!m->pitch||m->pitch%64||m->block_height_log2>5)return EINVAL;
 struct nn_block_copy_plan p={0};p.args=(struct nn_block_copy_args){(uint32_t)base,(uint32_t)s->layer_stride,m->pitch/64,m->block_height_log2,r->x_bytes,r->y_rows,r->row_bytes,r->rows,r->linear_row_stride,r->linear_image_stride,r->layers,r->direction,(uint32_t)span,(uint32_t)((span+3)/4),0,0};
 if(!r->direction){uint32_t gx=(p.args.linear_words+63u)/64u;if(gx>65535)gx=65535;p.groups[0]=gx;p.args.dispatch_width=gx*8;p.groups[1]=(p.args.linear_words+(uint64_t)gx*64-1)/((uint64_t)gx*64);p.groups[2]=1;}
 else{uint64_t words=((r->x_bytes&3u)+(uint64_t)r->row_bytes+3)/4;p.groups[0]=(words+7)/8;p.groups[1]=(r->rows+7)/8;p.groups[2]=r->layers;}
 for(unsigned i=0;i<3;i++)if(!p.groups[i]||p.groups[i]>65535)return E2BIG;
 p.region=*r;p.canonical_bytes=s->payload_bytes;p.linear_bytes=(span+3)&~UINT64_C(3);*out=p;return 0;
}
static int range(const struct nn_block_copy_range *r,uint64_t bytes,int descriptor,uint64_t *address)
{
 if(!r||!r->bo||!bytes||r->owner_offset>nn_bo_size(r->bo)||r->owner_bytes>nn_bo_size(r->bo)-r->owner_offset||
    r->offset<r->owner_offset||r->offset-r->owner_offset>r->owner_bytes||bytes>r->owner_bytes-(r->offset-r->owner_offset))return EINVAL;
 uint64_t base=nn_bo_address(r->bo);if(!base||r->offset>UINT64_MAX-base||bytes>UINT64_MAX-base-r->offset)return EOVERFLOW;
 uint64_t a=base+r->offset;if(a%4||a+bytes-1>=(UINT64_C(1)<<49))return EINVAL;
 if(descriptor&&(a&~UINT64_C(63))<base+r->owner_offset)return EINVAL;
 *address=a;return 0;
}
/* Shared actual BO owner/descriptor-window admission. Geometry and the
 * appropriate 2D/array or true3D shader ABI remain separate typed plans. */
static int raw_copy_image(nn_device *d,const struct nn_cs_image *image)
{
 if(!d||!image||image->schema!=1||image->chip!=0x06050002||nn_device_chip(d)!=image->chip||image->ssbo_slots!=2||
    image->local[0]!=8||image->local[1]!=8||image->local[2]!=1||!image->commands||!image->command_words||!image->code||!image->code_bytes)return ENOTSUP;
 return 0;
}
static int raw_copy_ranges(const struct nn_block_copy_range *c,const struct nn_block_copy_range *l,const struct nn_block_copy_range *u,
 uint64_t canonical_bytes,uint64_t linear_bytes,unsigned direction,struct nn_cs_buffer ubos[16],struct nn_cs_buffer ssbos[16],nn_bo *resources[3])
{
 if(!c||!l||!u||u->bo==c->bo||u->bo==l->bo)return EINVAL;
 uint64_t ca,la,ua;int e;
 if((e=range(c,canonical_bytes,1,&ca))||(e=range(l,linear_bytes,1,&la))||(e=range(u,64,0,&ua)))return e;
 if(c->bo==l->bo&&c->offset<l->offset+linear_bytes&&l->offset<c->offset+canonical_bytes)return EINVAL;
 ubos[2]=(struct nn_cs_buffer){ua,64};
 struct nn_cs_buffer cb={ca,(uint32_t)canonical_bytes},lb={la,(uint32_t)linear_bytes};
 ssbos[0]=direction?lb:cb;ssbos[1]=direction?cb:lb;
 resources[0]=c->bo;resources[1]=l->bo;resources[2]=u->bo;return 0;
}
int nn_block_copy_bind(nn_device *d,const struct nn_cs_image *image,const struct nn_tex_source_layout *source,const struct nn_block_copy_plan *p,
 const struct nn_block_copy_range *c,const struct nn_block_copy_range *l,const struct nn_block_copy_range *u,
 enum nn_block_copy_alias proof,struct nn_block_copy_binding *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));
 int e=raw_copy_image(d,image);if(e)return e;
 if(!p||!p->canonical_bytes||!p->linear_bytes||proof!=NN_BLOCK_COPY_DISJOINT_PROVEN)return ENOTSUP;
 if(!c||!l||!u||u->bo==c->bo||u->bo==l->bo)return EINVAL;
 struct nn_block_copy_plan expected;int pe=nn_block_copy_plan(source,&p->region,&expected);
 if(pe)return pe;
 if(memcmp(p,&expected,sizeof(expected)))return EINVAL;
 const struct nn_block_copy_args *a=&p->args;
 uint64_t last=(uint64_t)(a->rows?a->rows-1:0)*a->linear_row_stride+a->row_bytes,span=(uint64_t)(a->layers?a->layers-1:0)*a->linear_image_stride+last;
 if(!a->rows||!a->layers||!a->row_bytes||!a->linear_row_stride||a->linear_row_stride<a->row_bytes||a->linear_image_stride<last||
    a->direction>1||a->reserved||a->linear_span!=span||a->linear_words!=(span+3)/4||p->linear_bytes!=((span+3)&~UINT64_C(3))||
    p->canonical_bytes%4||p->canonical_bytes>UINT32_MAX||p->linear_bytes>UINT32_MAX)return EINVAL;
 for(unsigned i=0;i<3;i++)if(!p->groups[i]||p->groups[i]>65535)return EINVAL;
 struct nn_block_copy_binding b={0};
 if((e=raw_copy_ranges(c,l,u,p->canonical_bytes,p->linear_bytes,a->direction,b.ubos,b.ssbos,b.resources)))return e;
 b.args=*a;memcpy(b.groups,p->groups,sizeof(b.groups));*out=b;return 0;
}

#include "volume_copy.inc"
