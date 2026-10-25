/* SPDX-License-Identifier: MIT */
#ifndef NN_ANDROID_PRESENT_H
#define NN_ANDROID_PRESENT_H
#include <android/hardware_buffer.h>
#include <android/native_window.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct nn_present nn_present;
struct nn_present_stats {
    uint32_t show_submitted, show_completed, detach_submitted, detach_completed;
    uint32_t completion_callbacks, present_fences, present_fences_waited;
    uint32_t release_fences, release_fences_waited, missing_surface_stats;
    int32_t window_width, window_height;
    int last_errno, unknown_completion, current_buffer_held;
};

/* Positive errno. Single lifecycle worker owns calls; never wait on Android's
 * UI/callback thread. create acquires the window and creates an API29 child. */
int nn_present_create(ANativeWindow *window, nn_present **out);
/* Directly submits the same validated RGBA8 AHB, no copy/lock/reallocation. Caller
 * must first finish native KGSL work and provide opaque pixel contents. This
 * sets layer alpha1/opaque metadata without changing any RGBA byte.
 * Success waits for OnComplete, optional present fence, and previous buffer's
 * release fence. The NEW buffer is still displayed: keep its owner alive and
 * do not write it until detach succeeds (or a later show replaces it).
 * An uncertain result poisons this helper and retains every owner/reference. */
int nn_present_show(nn_present *present, AHardwareBuffer *buffer);
int nn_present_max_interval(void);
int nn_present_show_interval(nn_present *,AHardwareBuffer *,int interval);
/* Thread-safe nonblocking close notification, no SC/GPU calls or join. */
void nn_present_wake(nn_present *);
/* Reparents child to NULL, waits for OnComplete and the last buffer's release
 * fence. Missing per-surface stats is not release proof: fail closed/retain.
 * A detached helper cannot show again; create a fresh helper for a new window. */
int nn_present_detach(nn_present *present);
/* Calls detach if necessary. Unknown completion returns EBUSY without freeing
 * callback context, SurfaceControl, AHB or window. Caller must likewise retain
 * the nn_ahb/GPU owner until process teardown after such a failure. */
int nn_present_destroy(nn_present *present);
int nn_present_get_stats(nn_present *present, struct nn_present_stats *out);
#ifdef __cplusplus
}
#endif
#endif
