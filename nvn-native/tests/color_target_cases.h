/* SPDX-License-Identifier: MIT. Authored inputs, never an original game frame. */
#pragma once
#include "nvn.h"
enum {NN_COLOR_FORMATS=22,NN_COLOR_STATES=3,NN_COLOR_REPEATS=2,
 NN_COLOR_WIDTH=73,NN_COLOR_HEIGHT=41,NN_COLOR_CASES=132,
 NN_COLOR_SEQUENCE_OFFSET=45056,NN_COLOR_SEQUENCE_BYTES=4};
static const unsigned nn_color_formats[NN_COLOR_FORMATS]={
 NVN_FORMAT_RGBA8_SRGB,NVN_FORMAT_RGBA8,NVN_FORMAT_BGRA8_SRGB,NVN_FORMAT_BGRA8,
 NVN_FORMAT_R11G11B10F,NVN_FORMAT_RGBA16F,NVN_FORMAT_RGBA32F,
 NVN_FORMAT_R8,NVN_FORMAT_R8SN,NVN_FORMAT_R16F,NVN_FORMAT_R16,NVN_FORMAT_R16SN,NVN_FORMAT_R32F,
 NVN_FORMAT_RG8,NVN_FORMAT_RG8SN,NVN_FORMAT_RG16F,NVN_FORMAT_RG16,NVN_FORMAT_RG16SN,NVN_FORMAT_RG32F,
 NVN_FORMAT_RGBA8SN,NVN_FORMAT_RGBA16,NVN_FORMAT_RGBA16SN};
static inline void nn_color_input(unsigned state,unsigned repeat,float color[4])
{
 static const float first[4]={.25f,.5f,.75f,.5f};
 static const float second[4]={-.5f,1.5f,2.f,.5f};
 static const float blend[4]={.75f,.25f,.5f,.5f};
 for(unsigned i=0;i<4;i++)color[i]=repeat?(state==1?blend[i]:second[i]):first[i];
}
