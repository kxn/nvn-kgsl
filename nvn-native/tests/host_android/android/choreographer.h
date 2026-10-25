/* HOST callback/type declarations only; never replace NDK APIs. */
#ifdef __ANDROID__
#error "HOST Android callback declarations must never replace NDK headers"
#endif
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct AChoreographer AChoreographer;
typedef struct AChoreographerFrameCallbackData AChoreographerFrameCallbackData;
typedef int64_t AVsyncId;
typedef void (*AChoreographer_vsyncCallback)(const AChoreographerFrameCallbackData *,void *);
AChoreographer *AChoreographer_getInstance(void);

void AChoreographer_postVsyncCallback(AChoreographer *,AChoreographer_vsyncCallback,void *);
size_t AChoreographerFrameCallbackData_getFrameTimelinesLength(const AChoreographerFrameCallbackData *);
size_t AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex(const AChoreographerFrameCallbackData *);
AVsyncId AChoreographerFrameCallbackData_getFrameTimelineVsyncId(const AChoreographerFrameCallbackData *,size_t);
int64_t AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos(const AChoreographerFrameCallbackData *,size_t);
