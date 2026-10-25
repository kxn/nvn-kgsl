/* SPDX-License-Identifier: MIT */
#pragma once
#include "external_image.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Trusted native display allocation, never an ordinary NVN pool replacement.
 * No NVN, guest keys or renderer logic crosses this boundary. */
typedef struct nn_window_display nn_window_display;
/* Allocate/identify/unlock/import/pin an independent RGBA8 AHB. CPU lock is
 * READ-only: no pixel initialization, image copy or CPU conversion. No GPU
 * submit or compositor call. On error *out may retain a non-NULL owner ONLY
 * when cleanup could not safely complete; caller must retain it. */
int nn_window_display_create(nn_device *,uint32_t width,uint32_t height,nn_window_display **out);
int nn_window_display_view(nn_window_display *,struct nn_external_image_view *out);
/* Caller first proves producer+conversion Finish and compositor retire. A
 * failed destroy preserves the owner and remaining references. */
int nn_window_display_destroy(nn_window_display *);

/* Native-only READ diagnostics, after the owner proves GPU/SC exclusion. */
struct nn_ahb_pixel;
struct nn_ahb_view;
int nn_window_display_pixel(nn_window_display *,struct nn_ahb_pixel *);
int nn_window_display_read_lock(nn_window_display *,struct nn_ahb_view *);
int nn_window_display_read_unlock(nn_window_display *);

#ifdef __cplusplus
}
#endif
