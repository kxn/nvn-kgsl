/* HOST declaration fixture only. Never add this directory to Android builds. */
#if defined(__ANDROID__)
#error "HOST AHardwareBuffer declarations must never replace NDK headers"
#endif
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct AHardwareBuffer AHardwareBuffer;
typedef struct AHardwareBuffer_Desc {uint32_t width,height,layers,format;uint64_t usage;uint32_t stride,rfu0;uint64_t rfu1;} AHardwareBuffer_Desc;
enum {AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM=1,AHARDWAREBUFFER_USAGE_CPU_READ_RARELY=2,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN=3,AHARDWAREBUFFER_USAGE_CPU_READ_MASK=15,AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY=32,AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN=48,AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK=240,AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE=256,AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT=512};
int AHardwareBuffer_isSupported(const AHardwareBuffer_Desc*);
int AHardwareBuffer_allocate(const AHardwareBuffer_Desc*,AHardwareBuffer**);
void AHardwareBuffer_describe(const AHardwareBuffer*,AHardwareBuffer_Desc*);
void AHardwareBuffer_acquire(AHardwareBuffer*);
void AHardwareBuffer_release(AHardwareBuffer*);
int AHardwareBuffer_lockAndGetInfo(AHardwareBuffer*,uint64_t,int32_t,const void*,void**,int32_t*,int32_t*);
int AHardwareBuffer_unlock(AHardwareBuffer*,int32_t*);
