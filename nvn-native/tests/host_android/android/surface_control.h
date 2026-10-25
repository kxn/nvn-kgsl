/* HOST callback/type declarations only; never replace NDK APIs. */
#ifdef __ANDROID__
#error "HOST Android callback declarations must never replace NDK headers"
#endif
#pragma once
#include <stdint.h>
typedef struct ASurfaceTransaction ASurfaceTransaction;
void ASurfaceTransaction_setFrameTimeline(ASurfaceTransaction *,int64_t);
