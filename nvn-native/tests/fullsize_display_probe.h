/* SPDX-License-Identifier: MIT */
#pragma once
#include "window_probe.h"
/* Raw authored CS -> explicit AHB -> real window session, not an NVN Draw or
 * game frame. Owner only; exact2880x1620 RGBA8, N2/3, four distinct cases per
 * slot, same immutable per-slot IB replay. CPU initializes source/sentinels;
 * target verification only after GPU Finish and display release. */
int nn_fullsize_display_probe(const char *,char *,size_t,int,const void *,
                              const struct nn_nvn_window_probe_callbacks *);
