/* SPDX-License-Identifier: MIT. Canonical BUFFER format metadata only. */
#pragma once
#include <stdint.h>
#include <errno.h>
/* Public NVN format names/values, independent Maxwell FormatTraits element
 * sizes, and SDK53.314's BUFFER width*cpp / alignment cpp(12->4) contract.
 * These values are not a sampled/storage-image/render capability table.
 * Only scalar uncompressed color encodings with independently known storage
 * are admitted. RGB8/16, depth/stencil, compressed, sRGB, UI2F/vertex-only and
 * unknown encodings remain outside this metadata subset. */
static inline int nn_tex_buffer_element_bytes(uint32_t format,uint32_t *out)
{
 if(!out)return EINVAL;
 *out=0;
 switch(format){
 case 1:case 2:case 3:case 4:*out=1;break;
 case 5:case 6:case 7:case 8:case 9:
 case 13:case 14:case 15:case 16:
 case 0x39:case 0x3a:case 0x3b:case 0x3c:*out=2;break;
 case 10:case 11:case 12:case 17:case 18:case 19:case 20:case 21:
 case 37:case 38:case 39:case 40:
 case 0x3d:case 0x3e:case 0x3f:case 0x40:case 0x9a:*out=4;break;
 case 22:case 23:case 24:case 41:case 42:case 43:case 44:case 45:*out=8;break;
 case 34:case 35:case 36:*out=12;break;
 case 46:case 47:case 48:*out=16;break;
 default:return ENOTSUP;
 }
 return 0;
}
/* Vendored translator TextureFormat values, NOT native Adreno encodings.
 * Querying dimension/element extent does not require this optional scalar
 * format representation. A missing representation is ENOTSUP, never R32F.
 * No shader or GPU consumer is enabled by this CPU metadata conversion. */
static inline int nn_tex_buffer_query_format(uint32_t format,uint32_t *out)
{
 if(!out)return EINVAL;
 *out=0;
 if(format>=1&&format<=24){*out=format;return 0;}
 if(format>=37&&format<=48){*out=format-12;return 0;}
 if(format>=0x3d&&format<=0x3f){*out=format-0x3d+37;return 0;}
 return ENOTSUP;
}
