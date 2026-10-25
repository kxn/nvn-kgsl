/* SPDX-License-Identifier: MIT. No layout arithmetic; original FDL owns it. */
#include "nvn_native/pipeline_views.h"
#include "nvn_native/runtime_fdl.h"
#if NN_PIPELINE_COLOR_TARGETS
#include "nvn_native/color_target.h"
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
struct entry {struct entry *next;uint32_t width,height,pitch;struct nn_tex_native_view value;};
struct nn_pipeline_views {uint32_t chip;size_t count;struct entry *entries;};
int nn_pipeline_views_create(uint32_t chip,struct nn_pipeline_views **out)
{
 if(!out)return EINVAL;
 *out=NULL;
 if(chip!=0x06050002||nn_fdl_metadata_version()!=1)return ENOTSUP;
 struct nn_pipeline_views *p=calloc(1,sizeof(*p));if(!p)return ENOMEM;
 p->chip=chip;*out=p;return 0;
}
int nn_pipeline_views_get(struct nn_pipeline_views *p,uint32_t width,uint32_t height,
 uint32_t pitch,const struct nn_tex_native_view **out)
{
 if(!out)return EINVAL;
 *out=NULL;if(!p)return EINVAL;
 if(!width||width>16384||!height||height>16384)return ENOTSUP;
 for(const struct entry *e=p->entries;e;e=e->next)if(e->value.format==37&&e->width==width&&e->height==height&&e->pitch==pitch){*out=&e->value;return 0;}
 const struct nn_fdl_image_desc d={.schema=1,.chip=p->chip,.format=37,.target=1,
  .width=width,.height=height,.levels=1,.layers=1,.level_count=1,.layer_count=1,
  .usage=NN_FDL_SAMPLED|NN_FDL_COLOR_TARGET,.explicit_pitch=pitch};
 struct nn_fdl_image_view v;int error=nn_fdl_image(&d,&v);if(error)return error;
 if(v.schema!=1||v.chip!=p->chip||v.format!=37||v.target!=1||v.width!=width||v.height!=height||
    v.levels!=1||v.layers!=1||v.base_level||v.base_layer||v.level_count!=1||v.layer_count!=1||
    v.block_width!=1||v.block_height!=1||v.bytes_per_block!=4||!v.color_target||
    v.view_offset||v.bytes>UINT32_MAX||v.required_span>UINT32_MAX||v.descriptor_address_word!=4||
    (pitch&&v.level[0].pitch!=pitch))return EINVAL;
 struct nn_tex_native_view n={.schema=1,.chip=p->chip,.format=37,.target=1,.levels=1,.layers=1,.samples=1,
  .width=width,.height=height,.pitch=v.level[0].pitch,.layer_bytes=v.level[0].layer_stride,
  .bytes=(uint32_t)v.bytes,.alignment=v.alignment,.required_span_bytes=(uint32_t)v.required_span,
  .descriptor_address_word=v.descriptor_address_word,.descriptor_offset=(uint32_t)v.view_offset,
  .descriptor_address_mask=v.descriptor_address_mask,.rb_mrt_buf_info=v.rb_mrt_buf_info,
  .rb_mrt_pitch=v.rb_mrt_pitch,.rb_mrt_array_pitch=v.rb_mrt_array_pitch,.sp_ps_mrt_reg=v.sp_ps_mrt_reg};
 memcpy(n.texture_descriptor,v.descriptor,sizeof(n.texture_descriptor));
 if(p->count==SIZE_MAX)return EOVERFLOW;
 struct entry *e=calloc(1,sizeof(*e));if(!e)return ENOMEM;
 e->width=width;e->height=height;e->pitch=pitch;e->value=n;e->next=p->entries;
 p->entries=e;p->count++;*out=&e->value;return 0;
}
int nn_pipeline_views_get_format(struct nn_pipeline_views *p,uint32_t format,uint32_t width,uint32_t height,
 uint32_t pitch,const struct nn_tex_native_view **out)
{
 if(!out)return EINVAL;
 *out=NULL;if(!p)return EINVAL;
 if(format==37)return nn_pipeline_views_get(p,width,height,pitch,out);
#if NN_PIPELINE_COLOR_TARGETS
 for(const struct entry *e=p->entries;e;e=e->next)
  if(e->value.format==format&&e->width==width&&e->height==height&&e->pitch==pitch){*out=&e->value;return 0;}
 struct nn_tex_native_view value;int error=nn_color_target_create(p->chip,format,width,height,pitch,&value,NULL);if(error)return error;
 if(p->count==SIZE_MAX)return EOVERFLOW;
 struct entry *e=calloc(1,sizeof(*e));if(!e)return ENOMEM;
 e->width=width;e->height=height;e->pitch=pitch;e->value=value;e->next=p->entries;p->entries=e;p->count++;*out=&e->value;return 0;
#else
 (void)width;(void)height;(void)pitch;return ENOTSUP;
#endif
}
size_t nn_pipeline_views_count(const struct nn_pipeline_views *p){return p?p->count:0;}
void nn_pipeline_views_destroy(struct nn_pipeline_views *p)
{if(p){while(p->entries){struct entry *e=p->entries;p->entries=e->next;free(e);}free(p);}}
