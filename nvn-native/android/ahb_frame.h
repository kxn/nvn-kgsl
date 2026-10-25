/* SPDX-License-Identifier: MIT */
#ifndef NN_ANDROID_AHB_FRAME_H
#define NN_ANDROID_AHB_FRAME_H
#include "ahb.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Run the existing CP + four original VS/FS cases and release the AHB.
 * Return 0 only for a complete passing JSON result; otherwise return -1. */
int nn_ahb_gpu_probe(const char *directory, char *json, size_t capacity);

/* Same qualification, but on success transfer the last GPU-rendered AHB
 * (case3: original interpolated RGB triangle) into *out. All KGSL queues/BOs
 * have finished and been safely destroyed; no CPU pixel copy is performed.
 * JSON operation is kgsl_ahb_present_frame. This function does not present.
 * The caller owns the returned nn_ahb and must keep it alive through display
 * use and compositor release; only then call nn_ahb_destroy. The borrowed
 * AHardwareBuffer from nn_ahb_buffer must not be released independently.
 *
 * out must be non-NULL and is cleared before any work. Return 0 only with a
 * complete passing JSON result and a non-NULL owner; all failures return -1
 * with *out=NULL. Unknown GPU/unlock/cleanup outcomes are retained internally
 * until process teardown, never exported. A 4096-byte JSON buffer suffices for
 * this profile. Single caller/owner thread; do not run concurrently. */
int nn_ahb_gpu_frame(const char *directory, char *json, size_t capacity, nn_ahb **out);

#ifdef __cplusplus
}
#endif
#endif
