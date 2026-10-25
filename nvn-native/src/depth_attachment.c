/* SPDX-License-Identifier: MIT. No pixel accesses or GPU operations. */
#include "nvn_native/depth_attachment.h"
#include <errno.h>
#include <string.h>
int nn_depth_attachment_validate(const struct nn_depth_attachment *a)
{
 if(!a||!a->ready||!a->storage)return EINVAL;
 size_t bytes=nn_bo_size(a->storage);uint64_t address=nn_bo_address(a->storage);
 if(!address||!a->view.required_span||a->view.required_span>bytes||a->view.desc.address!=address)return EINVAL;
 struct nn_depth_fdl_view actual;int e=nn_depth_fdl(&a->view.desc,&actual);
 if(e)return e;
 return memcmp(&actual,&a->view,sizeof(actual))?EINVAL:0;
}
int nn_depth_attachment_destroy(struct nn_depth_attachment *a)
{
 if(!a)return EINVAL;
 if(a->storage){int e=nn_bo_destroy(a->storage);if(e)return e;}
 memset(a,0,sizeof(*a));return 0;
}
int nn_depth_attachment_create(nn_device*d,const struct nn_depth_fdl_desc *desc,struct nn_depth_attachment*out)
{
 if(!out)return EINVAL;
 if(out->storage||out->ready)return EBUSY;
 memset(out,0,sizeof(*out));if(!d||!desc)return EINVAL;
 if(desc->address||nn_device_chip(d)!=desc->chip)return EINVAL;
 struct nn_depth_fdl_view zero;int e=nn_depth_fdl(desc,&zero);if(e)return e;
 if(zero.required_span>SIZE_MAX)return E2BIG;
 e=nn_bo_alloc(d,(size_t)zero.required_span,&out->storage);
 if(e)return e;
 if(!out->storage){return EPROTO;}
 struct nn_depth_fdl_desc actual=*desc;actual.address=nn_bo_address(out->storage);
 if(!actual.address){e=EFAULT;goto failed;}
 if((e=nn_depth_fdl(&actual,&out->view)))goto failed;
 out->ready=1;if((e=nn_depth_attachment_validate(out)))goto failed;
 return 0;
failed:
 /* Preserve the initial failure and any still-live BO on rollback error. */
 out->ready=0;(void)nn_depth_attachment_destroy(out);return e;
}
int nn_depth_attachment_resources(const struct nn_depth_attachment*a,nn_bo**out,size_t capacity,size_t*count)
{
 if(!count)return EINVAL;
 *count=0;int e=nn_depth_attachment_validate(a);if(e)return e;
 if(!out||capacity<1)return E2BIG;
 out[0]=a->storage;*count=1;return 0;
}
int nn_depth_attachment_descriptor(const struct nn_depth_attachment*a,uint32_t out[16])
{
 if(!out)return EINVAL;
 memset(out,0,64);int e=nn_depth_attachment_validate(a);if(e)return e;
 memcpy(out,a->view.descriptor,64);return 0;
}
