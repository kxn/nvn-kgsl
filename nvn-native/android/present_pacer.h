/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
struct nn_present_pacer;
/* OS vsync coordination only, not a timer/profiler. API33 symbols are resolved
 * at runtime: an API29 binary never imports unavailable strong symbols. */
int nn_present_pacer_max_interval(void);
int nn_present_pacer_create(struct nn_present_pacer **);
int nn_present_pacer_wait(struct nn_present_pacer *,int,int64_t *,int64_t *);
void nn_present_pacer_wake(struct nn_present_pacer *);
/* Runs only on owning Looper thread. An unreturned one-shot callback retains
 * its context and returns EBUSY. No unregister API exists for vsync callbacks. */
int nn_present_pacer_destroy(struct nn_present_pacer *);
