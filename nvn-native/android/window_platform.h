/* SPDX-License-Identifier: MIT */
#pragma once
#include "nvn_native/window_session.h"
struct nn_window_platform_stats {
    uint32_t show_completed, detach_completed, completion_callbacks;
    uint32_t present_fences_waited, release_fences_waited;
};
/* Private adapter interface; callbacks never enter core/NVN/KGSL. Error from
 * show/detach/destroy retains its handle. Buffer/window refs are additional to
 * core allocator ownership. Host tests substitute synchronization only. */
struct nn_window_platform {
    void (*window_acquire)(void *); void (*window_release)(void *);
    int (*validate_buffer)(const struct nn_window_slot *);
    void (*buffer_acquire)(void *); void (*buffer_release)(void *);
    int (*create)(void *,void **);
    int (*show)(void *,void *);
    int (*detach)(void *);
    int (*destroy)(void *);
    int (*stats)(void *,struct nn_window_platform_stats *);
    /* Optional for legacy RAM fixtures. Native uses Choreographer timelines.
     * ECANCELED from show_interval means cancelled BEFORE SC apply only.
     * wake is thread-safe, nonblocking, and cannot free presenter storage. */
    int (*max_interval)(void);
    int (*show_interval)(void *,void *,int);
    void (*wake)(void *);
};
const struct nn_window_platform *nn_window_platform_get(void);
