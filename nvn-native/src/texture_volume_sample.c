/* SPDX-License-Identifier: MIT. No layout/descriptor packing implementation. */
#include "nvn_native/texture_volume_sample.h"
#include <errno.h>
#include <string.h>
static struct nn_fdl_volume_desc desc(const struct nn_fdl_volume_view*v)
{return(struct nn_fdl_volume_desc){.schema=1,.chip=v->chip,.format=v->format,.width=v->width,.height=v->height,.depth=v->depth,.levels=v->levels,.base_level=v->base_level,.level_count=v->level_count,.usage=NN_FDL_SAMPLED};}
int nn_volume_sampled_validate(const struct nn_fdl_volume_view*v)
{
 if(!v||v->schema!=1||v->chip!=0x06050002||(v->format!=37&&v->format!=0x29&&(v->format<0x41||v->format>0x44)))return ENOTSUP;
 struct nn_fdl_volume_desc d=desc(v);struct nn_fdl_volume_view actual;int e=nn_fdl_volume(&d,&actual);if(e)return e;
 return memcmp(v,&actual,sizeof(actual))?EINVAL:0;
}
int nn_volume_sampled_descriptor(const struct nn_fdl_volume_view*v,nn_bo*b,size_t offset,size_t bytes,uint32_t out[16])
{
 if(!out)return EINVAL;
 memset(out,0,64);int e=nn_volume_sampled_validate(v);if(e)return e;
 if(!b||offset>nn_bo_size(b)||bytes>nn_bo_size(b)-offset||v->required_span>bytes)return EINVAL;
 uint64_t a=nn_bo_address(b);if(!a||offset>UINT64_MAX-a||bytes>UINT64_MAX-a-offset)return EOVERFLOW;
 struct nn_fdl_volume_desc d=desc(v);d.address=a+offset;struct nn_fdl_volume_view actual;
 if((e=nn_fdl_volume(&d,&actual)))return e;
 memcpy(out,actual.descriptor,64);return 0;
}
