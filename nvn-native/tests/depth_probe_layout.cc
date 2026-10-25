/* SPDX-License-Identifier: MIT. HOST actual FDL reference, no CPU pixels/GPU. */
#include "nvn_native/depth_a2d.h"
#include <cstdio>
#include <initializer_list>
int main(){bool first=true;puts("[");for(unsigned f:{0x32u,0x33u,0x34u})for(unsigned width:{64u,73u}){nn_depth_fdl_desc d={};d.schema=1;d.chip=0x06050002;d.format=f;d.target=1;d.width=width;d.height=width==64?64:41;d.levels=d.layers=d.roles=1;nn_depth_a2d_view v={};int e=nn_depth_a2d(&d,0,&v);if(e)return e;printf("%s{\"format\":%u,\"width\":%u,\"height\":%u,\"pitch\":%u,\"bytes\":%llu,\"alignment\":%u,\"tiled_bytes\":%llu}",first?"":",\n",f,width,d.height,v.linear.pitch,(unsigned long long)v.linear.bytes,v.linear.alignment,(unsigned long long)v.tiled.bytes);first=false;}puts("\n]");return 0;}
