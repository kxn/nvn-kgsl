/* SPDX-License-Identifier: MIT. HOST-only actual FDL buffer view metadata. */
#include "freedreno_layout.h"
#include "util/format/u_format.h"
#include <cerrno>
#include <cstdint>
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
 uint64_t bytes=0,offset=0;
 if(argc!=4||!number(argv[2],&bytes)||!number(argv[3],&offset)||!bytes||bytes>UINT32_MAX||offset>63)return 2;
 enum pipe_format format;
 if(!strcmp(argv[1],"rgba8"))format=PIPE_FORMAT_R8G8B8A8_UNORM;
 else if(!strcmp(argv[1],"r32f"))format=PIPE_FORMAT_R32_FLOAT;
 else return 3;
 unsigned cpp=util_format_get_blocksize(format);
 if(offset%cpp||bytes%cpp)return 4;
 uint32_t words[FDL6_TEX_CONST_DWORDS];
 const uint8_t swiz[4]={PIPE_SWIZZLE_X,PIPE_SWIZZLE_Y,PIPE_SWIZZLE_Z,PIPE_SWIZZLE_W};
 fdl6_buffer_view_init<A6XX>(words,format,swiz,offset,(uint32_t)bytes);
 printf("{\"schema\":1,\"chip\":100990978,\"pipe_format\":%u,\"format_name\":\"%s\",\"element_bytes\":%u,\"elements\":%llu,\"bytes\":%llu,\"offset\":%llu,\"descriptor\":[",format,argv[1],cpp,(unsigned long long)(bytes/cpp),(unsigned long long)bytes,(unsigned long long)offset);
 for(unsigned i=0;i<FDL6_TEX_CONST_DWORDS;i++)printf("%s%u",i?",":"",words[i]);
 puts("]}");return 0;
}
