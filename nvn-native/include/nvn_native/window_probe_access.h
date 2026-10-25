/* Native-only owner qualification access; never a guest NVN API. */
#pragma once
#include "nvn_native/nvn.h"
#include "nvn_native/window_display.h"
#include "nvn_native/texture_source.h"
#include "nvn_native/external_image.h"
struct nn_ahb_view;
struct nn_ahb_pixel;
int nn_nvn_window_target_info(NVNwindow *,unsigned,struct nn_external_image_view *,struct nn_ahb_pixel *);
/* Only after Acquire or whole-session retirement, with no pending GPU conversion. */
int nn_nvn_window_target_read_lock(NVNwindow *,unsigned,struct nn_ahb_view *);
int nn_nvn_window_target_read_unlock(NVNwindow *,unsigned);
/* Finish all producers/conversion, retire SC, keep private display owners for
 * READ-only qualification. Standard WindowFinalize subsequently destroys them. */
int nn_nvn_window_retire_for_readback(NVNwindow *);
int nn_window_display_pixel(nn_window_display *,struct nn_ahb_pixel *);
int nn_window_display_read_lock(nn_window_display *,struct nn_ahb_view *);
int nn_window_display_read_unlock(nn_window_display *);

struct nn_nvn_window_source_identity { NVNmemoryPool *pool;uint64_t address,entry_address;size_t offset,bytes,entry_bytes; };
int nn_nvn_window_source_identity(NVNwindow *,unsigned,struct nn_nvn_window_source_identity *);

int nn_nvn_window_source_layout(NVNwindow *,unsigned,struct nn_tex_source_layout *);
