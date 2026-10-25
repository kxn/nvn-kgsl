/* SPDX-License-Identifier: MIT. Pure descriptor binding; no CPU image copy. */
#include "nvn_native/texture_buffer.h"
#include "nvn_native/compute_image.h"
#include <errno.h>
#include <string.h>
_Static_assert(sizeof(struct nn_tex_buffer_recipe)==112,"fixed ARM64/HOST recipe ABI");
int nn_tex_buffer_recipe_validate(const struct nn_tex_buffer_recipe *r)
{
 if(!r)return EINVAL;
 if(r->schema!=1||r->chip!=0x06050002||
    (r->format!=37&&r->format!=10)||r->element_bytes!=4)return ENOTSUP;
 /* These field boundaries are the A6XX_TEX_MEMOBJ DESC_BUFFER XML contract:
  * word1 WIDTH[14:0]/HEIGHT[29:15], word2 STARTOFFSETTEXELS[21:16],
  * word4 BASE_LO[31:5], word5 BASE_HI[16:0]. No generated header at runtime. */
 if(r->width_mask!=0x7fffu||r->height_mask!=0x3fff8000u||r->height_shift!=15||
    r->offset_mask!=0x3f0000u||r->offset_shift!=16||
    r->address_mask!=UINT64_C(0x1ffffffffffe0))return EINVAL;
 /* Zero-size, zero-GPUVA, identity-swizzled FDL base; variable payload is
  * inserted only by the already-qualified common buffer descriptor helper.
  * TYPE[31:29]=BUFFER(4), STRUCTSIZETEXELS[15:4]=1; no image PITCH/flags. */
 if(r->descriptor[1]||r->descriptor[2]!=0x80000010u)return EINVAL;
 for(unsigned i=3;i<16;i++)if(r->descriptor[i])return EINVAL;
 /* Mips/samples/sRGB/tile are not part of this finite float BUFFER profile.
  * Actual format/swizzle word is FDL-authored and source-hash pinned. */
 if(r->descriptor[0]&0x003f0007u)return ENOTSUP;
 return 0;
}
int nn_tex_buffer_bind(const struct nn_tex_buffer_recipe *r,nn_bo *bo,
 size_t owner_offset,size_t owner_bytes,size_t offset,size_t bytes,
 struct nn_tex_buffer_binding *out)
{
 if(!out)return EINVAL;
 memset(out,0,sizeof(*out));int e=nn_tex_buffer_recipe_validate(r);if(e)return e;
 if(!bo||!owner_bytes||!bytes||bytes%r->element_bytes||bytes>UINT32_MAX)return EINVAL;
 size_t bo_bytes=nn_bo_size(bo);
 if(owner_offset>bo_bytes||owner_bytes>bo_bytes-owner_offset||
    offset<owner_offset||offset-owner_offset>owner_bytes||
    bytes>owner_bytes-(offset-owner_offset))return EINVAL;
 uint64_t base=nn_bo_address(bo);
 if(!base||owner_offset>UINT64_MAX-base||owner_bytes>UINT64_MAX-base-owner_offset||
    offset>UINT64_MAX-base||bytes>UINT64_MAX-base-offset)return EOVERFLOW;
 uint64_t address=base+offset,aligned=address&~UINT64_C(63);
 if(address%r->element_bytes||aligned<base+owner_offset)return EINVAL;
 if((aligned&~r->address_mask)||address+bytes-1>(r->address_mask|UINT64_C(63)))return EOVERFLOW;
 struct nn_cs_image fields={.width_mask=r->width_mask,.height_mask=r->height_mask,
  .height_shift=r->height_shift,.offset_mask=r->offset_mask,.offset_shift=r->offset_shift};
 memcpy(fields.descriptor,r->descriptor,sizeof(fields.descriptor));
 struct nn_tex_buffer_binding value={.format=r->format,.element_bytes=r->element_bytes,
  .elements=(uint32_t)(bytes/r->element_bytes),.resource=bo,.owner_offset=owner_offset,
  .owner_bytes=owner_bytes,.offset=offset,.bytes=bytes,.address=address,.descriptor_base=aligned};
 /* Reuse the existing actual-FDL/compute artifact buffer field encoder. This
  * helper manipulates descriptor bits only; it neither samples nor copies. */
 if(nn_cs_buffer_descriptor(&fields,value.descriptor,address,(uint32_t)bytes))return EINVAL;
 *out=value;return 0;
}
