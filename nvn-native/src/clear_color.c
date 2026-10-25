/* SPDX-License-Identifier: MIT. Layout/owner admission, never CPU fill. */
#include "nvn_native/clear_color.h"
#include <errno.h>
#include <string.h>
_Static_assert(sizeof(struct nn_clear_color_args)==64,"UBO2 constant contract");
int nn_clear_color_plan(const struct nn_tex_source_layout *s,const struct nn_clear_color_rect *r,
 const float rgba[4],uint32_t mask,struct nn_clear_color_plan *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=nn_tex_source_validate(s);if(e)return e;
 if(!r||r->width<0||r->height<0)return EINVAL;
 if(s->target!=1||s->levels!=1||s->layers!=1||s->block_w!=1||s->block_h!=1)return ENOTSUP;
 struct nn_clear_color_pattern c;if((e=nn_clear_color_pattern(s->format,rgba,mask,&c)))return e;
 if(c.bytes_per_pixel!=s->bytes_per_block||s->payload_bytes>UINT32_MAX||s->payload_bytes%4)return ENOTSUP;
 const struct nn_tex_source_level *m=s->level;
 if(m->offset||!m->pitch||m->pitch%4||(!s->linear&&(m->pitch%64||m->block_height_log2>5)))return EINVAL;
 int64_t left=r->x,top=r->y,right=left+r->width,bottom=top+r->height;
 if(left<0)left=0;
 if(top<0)top=0;
 if(right>m->width)right=m->width;
 if(bottom>m->height)bottom=m->height;
 struct nn_clear_color_plan p={0};p.destination_bytes=s->payload_bytes;p.requested=*r;p.component_mask=mask;memcpy(p.rgba,rgba,16);
 p.args.linear=s->linear;p.args.pitch=m->pitch;p.args.block_height_log2=s->linear?0:m->block_height_log2;
 memcpy(p.args.pattern,c.pattern,16);memcpy(p.args.mask,c.mask,16);
 if(right<=left||bottom<=top||!(c.mask[0]|c.mask[1]|c.mask[2]|c.mask[3])){p.empty=1;*out=p;return 0;}
 p.args.x_bytes=(uint32_t)left*c.bytes_per_pixel;p.args.y_rows=(uint32_t)top;
 p.args.row_bytes=(uint32_t)(right-left)*c.bytes_per_pixel;p.args.rows=(uint32_t)(bottom-top);
 uint64_t words=((p.args.x_bytes&3u)+(uint64_t)p.args.row_bytes+3)/4;
 p.groups[0]=(uint32_t)((words+7)/8);p.groups[1]=(p.args.rows+7)/8;p.groups[2]=1;
 if(!p.groups[0]||p.groups[0]>65535||p.groups[1]>65535)return E2BIG;
 *out=p;return 0;
}
static int range(const struct nn_clear_color_range *r,uint64_t bytes,int descriptor,uint64_t *address)
{
 if(!r||!r->bo||!bytes||r->owner_offset>nn_bo_size(r->bo)||r->owner_bytes>nn_bo_size(r->bo)-r->owner_offset||
    r->offset<r->owner_offset||r->offset-r->owner_offset>r->owner_bytes||bytes>r->owner_bytes-(r->offset-r->owner_offset))return EINVAL;
 uint64_t b=nn_bo_address(r->bo);if(!b||r->offset>UINT64_MAX-b||bytes>UINT64_MAX-b-r->offset)return EOVERFLOW;
 uint64_t a=b+r->offset;if(a%4||a+bytes-1>=(UINT64_C(1)<<49))return EINVAL;
 if(descriptor&&(a&~UINT64_C(63))<b+r->owner_offset)return EINVAL;
 *address=a;return 0;
}
int nn_clear_color_bind(nn_device *d,const struct nn_cs_image *image,const struct nn_tex_source_layout *s,
 const struct nn_clear_color_plan *p,const struct nn_clear_color_range *destination,
 const struct nn_clear_color_range *uniform,struct nn_clear_color_binding *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));
 if(!d||!image||image->schema!=1||image->chip!=0x06050002||nn_device_chip(d)!=image->chip||image->ssbo_slots!=1||
    image->local[0]!=8||image->local[1]!=8||image->local[2]!=1||!image->commands||!image->command_words||!image->code||!image->code_bytes)return ENOTSUP;
 if(!p||p->empty)return ENOTSUP;
 struct nn_clear_color_plan expected;int e=nn_clear_color_plan(s,&p->requested,p->rgba,p->component_mask,&expected);
 if(e)return e;
 if(memcmp(p,&expected,sizeof(expected)))return EINVAL;
 if(!destination||!uniform||destination->bo==uniform->bo)return EINVAL;
 uint64_t da,ua;if((e=range(destination,p->destination_bytes,1,&da))||(e=range(uniform,64,0,&ua)))return e;
 struct nn_clear_color_binding b={0};b.args=p->args;memcpy(b.groups,p->groups,sizeof(b.groups));
 b.ubos[2]=(struct nn_cs_buffer){ua,64};b.ssbos[0]=(struct nn_cs_buffer){da,(uint32_t)p->destination_bytes};
 b.resources[0]=destination->bo;b.resources[1]=uniform->bo;*out=b;return 0;
}
