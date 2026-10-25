/* SPDX-License-Identifier: MIT. Actual Mesa FDL supplies all descriptor bits. */
#include "nvn_native/texture_buffer_sample.h"
#include <errno.h>
#include <string.h>
int nn_buffer_sampled_result(uint32_t format,uint32_t *result)
{
 if(!result)return EINVAL;
 *result=0;
 /* Plain scalar/tuple types, from the public NVN source format enum. Native
  * format availability remains actual FDL validation, not this result table. */
 switch(format){
 case 3:case 8:case 11:case 15:case 20:case 23:case 35:case 39:case 44:case 47:*result=NN_BUFFER_UINT;break;
 case 4:case 9:case 12:case 16:case 21:case 24:case 36:case 40:case 45:case 48:*result=NN_BUFFER_SINT;break;
 case 1:case 2:case 5:case 6:case 7:case 10:case 13:case 14:case 17:case 18:case 19:case 22:
 case 34:case 37:case 38:case 41:case 42:case 43:case 46:case 154:*result=NN_BUFFER_FLOAT;break;
 default:return ENOTSUP;
 }
 return 0;
}
int nn_buffer_sampled_view(const struct nn_buffer_sampled_binding *b,struct nn_fdl_buffer_view *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));
 if(!b||b->version!=1||!b->resource||!b->owner_bytes||!b->bytes||b->bytes>UINT32_MAX)return EINVAL;
 uint32_t result;int e=nn_buffer_sampled_result(b->format,&result);if(e)return e;
 size_t n=nn_bo_size(b->resource);
 if(b->owner_offset>n||b->owner_bytes>n-b->owner_offset||b->offset<b->owner_offset||
    b->offset-b->owner_offset>b->owner_bytes||b->bytes>b->owner_bytes-(b->offset-b->owner_offset))return EINVAL;
 uint64_t a=nn_bo_address(b->resource);
 if(!a||b->owner_offset>UINT64_MAX-a||b->owner_bytes>UINT64_MAX-a-b->owner_offset||
    b->offset>UINT64_MAX-a||b->bytes>UINT64_MAX-a-b->offset)return EOVERFLOW;
 struct nn_fdl_buffer_desc d={.schema=1,.chip=0x06050002,.format=b->format,.bytes=(uint32_t)b->bytes,.address=a+b->offset};
 struct nn_fdl_buffer_view v;
 if((e=nn_fdl_buffer(&d,&v)))return e;
 if(v.base_address<a+b->owner_offset||v.required_span>b->owner_bytes-(size_t)(v.base_address-a-b->owner_offset))return EINVAL;
 *out=v;return 0;
}
int nn_buffer_sampled_descriptor(const struct nn_buffer_sampled_binding *b,uint32_t out[16])
{
 if(!out)return EINVAL;
 memset(out,0,64);struct nn_fdl_buffer_view v;int e=nn_buffer_sampled_view(b,&v);if(e)return e;
 memcpy(out,v.descriptor,64);return 0;
}
