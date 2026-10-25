/* SPDX-License-Identifier: MIT. HOST actual common FDL and generated fields. */
#include "freedreno_layout.h"
#include "fd6_hw.h"
#include "util/format/u_format.h"
#include "nvn_native/texture_buffer.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static bool number(const char *s,uint64_t *v)
{
 if(!s||!*s||*s=='-')return false;
 char *end=nullptr;errno=0;unsigned long long x=strtoull(s,&end,0);
 if(errno||!end||*end)return false;
 *v=x;return true;
}
int main(int argc,char **argv)
{
 if(argc!=2&&argc!=4)return EINVAL;
 enum pipe_format fmt;uint32_t nvn;
 if(!strcmp(argv[1],"rgba8")){fmt=PIPE_FORMAT_R8G8B8A8_UNORM;nvn=37;}
 else if(!strcmp(argv[1],"r32f")){fmt=PIPE_FORMAT_R32_FLOAT;nvn=10;}
 else return ENOTSUP;
 uint64_t bytes=0,address=0;
 if(argc==4&&(!number(argv[2],&bytes)||!number(argv[3],&address)||!bytes||
    bytes>UINT32_MAX||bytes%4||address%4||address>UINT64_MAX-bytes||
    address+bytes-1>=(UINT64_C(1)<<49)))return EINVAL;
 nn_tex_buffer_recipe r={};r.schema=1;r.chip=0x06050002;r.format=nvn;r.element_bytes=util_format_get_blocksize(fmt);
 const uint8_t swiz[4]={PIPE_SWIZZLE_X,PIPE_SWIZZLE_Y,PIPE_SWIZZLE_Z,PIPE_SWIZZLE_W};
 fdl6_buffer_view_init<A6XX>(r.descriptor,fmt,swiz,address,(uint32_t)bytes);
 r.width_mask=A6XX_TEX_MEMOBJ_1_WIDTH(~0u);r.height_mask=A6XX_TEX_MEMOBJ_1_HEIGHT(~0u);
 r.height_shift=__builtin_ctz(r.height_mask);r.offset_mask=A6XX_TEX_MEMOBJ_2_STARTOFFSETTEXELS(~0u);r.offset_shift=__builtin_ctz(r.offset_mask);
 r.address_mask=uint64_t(A6XX_TEX_MEMOBJ_4_BASE_LO__MASK)|(uint64_t(A6XX_TEX_MEMOBJ_5_BASE_HI__MASK)<<32);
 printf("{\"schema\":1,\"chip\":%u,\"format\":%u,\"element_bytes\":%u,\"width_mask\":%u,\"height_mask\":%u,\"height_shift\":%u,\"offset_mask\":%u,\"offset_shift\":%u,\"address_mask\":%llu,\"descriptor\":[",r.chip,r.format,r.element_bytes,r.width_mask,r.height_mask,r.height_shift,r.offset_mask,r.offset_shift,(unsigned long long)r.address_mask);
 for(unsigned i=0;i<16;i++)printf("%s%u",i?",":"",r.descriptor[i]);
 printf("],\"request_bytes\":%llu,\"request_address\":%llu}\n",(unsigned long long)bytes,(unsigned long long)address);return 0;
}
