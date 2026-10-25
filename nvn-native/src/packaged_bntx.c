/* SPDX-License-Identifier: MIT. Explicit narrow experiment; unknowns reject. */
#include "nvn_native/packaged_bntx.h"
#include <errno.h>
static uint32_t le32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int nn_packaged_bntx_height(const uint8_t bytes[8],uint32_t *out)
{
 if(!out)return EINVAL;
 *out=0;if(!bytes)return EINVAL;
 uint32_t first=le32(bytes),second=le32(bytes+4);
 if((first&~7u)||first>5||second!=0x00010007u)return ENOTSUP;
 *out=first;return 0;
}
