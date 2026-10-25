/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native host-only API, not guest VI/NVN and not a substitute loader. The
 * controller owns its borrowed SurfaceView ANativeWindow during attach. */
const void *nn_d2r_activity_host_token(void);
int nn_d2r_activity_host_attach(void *native_window,const char *resolution,uint64_t *generation);
int nn_d2r_activity_host_close(uint64_t generation,uint32_t timeout_ms);
/* Called on the original guest-owner worker, ONCE per process. stage1 SDK,
 * stage2 constructors, stage3 nnMain. Return only means original entry returned;
 * zero never asserts game initialization, rendering, display or firstframe. */
int nn_d2r_activity_host_run(const char *modules,int stage);
/* Resolver/ABI lookup only. No DeviceInitialize/GPU/module execution. */
int nn_d2r_activity_host_check_bootstrap(void);
/* Original oe focus values:1 InFocus,2 OutOfFocus,3 Background. Queue/state
 * notification only; safe for UI producers. Zero does not assert consumption,
 * guest pause, save, graphics retirement or Surface/game resume. */
int nn_d2r_activity_host_notify_focus(int state);
#ifdef __cplusplus
}
#endif
