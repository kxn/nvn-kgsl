/* SPDX-License-Identifier: MIT. Authored fixture only; no shader execution. */
#pragma once
#include <stdint.h>
enum{NN_STORAGE_GPU_JOBS=96,NN_STORAGE_GPU_SUBMITS=192,NN_STORAGE_GPU_FILES=576};
struct nn_storage_gpu_case{unsigned id,program,offset[2],bytes,shape;};
static inline uint32_t nn_storage_gpu_source(unsigned stage,unsigned index,unsigned repeat)
{
 if(!stage&&index<3)return 64+17*repeat;
 if(stage&&index<8)return 24+7*index+11*repeat;
 return UINT32_C(0x43120000)|((stage*37+index*13+repeat*19)&255u);
}
static inline int nn_storage_gpu_written(unsigned program,unsigned stage,size_t relative)
{
 if(program!=1&&program!=3)return 0;
 size_t word=relative/4;return stage?(word>=16&&word<24):(word>=8&&word<11);
}
