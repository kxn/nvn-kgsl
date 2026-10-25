/* SPDX-License-Identifier: MIT. Input data only; no expected pixels on device. */
#pragma once
#define NN_GATHER_WIDTH 32u
#define NN_GATHER_HEIGHT 16u
#define NN_GATHER_SOURCE_WIDTH 8u
#define NN_GATHER_SOURCE_HEIGHT 4u
#define NN_GATHER_JOBS 48u
#define NN_GATHER_SUBMITS 96u
#define NN_GATHER_SEQUENCE_OFFSET 45056u
static inline unsigned char nn_gather_source(unsigned x,unsigned y,unsigned c,unsigned repeat)
{return (unsigned char)(x*29u+y*17u+c*53u+repeat*71u);}
