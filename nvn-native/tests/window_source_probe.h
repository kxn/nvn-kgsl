/* SPDX-License-Identifier: MIT */
#pragma once
#include "window_probe.h"
/* Authored source bytes -> standard NVN Window Present -> GPU GOB conversion
 * -> private AHB -> SC. No Draw/game; target CPU locks READ only. */
int nn_window_source_probe(const char *,char *,size_t,int,const void *,const struct nn_nvn_window_probe_callbacks *);
