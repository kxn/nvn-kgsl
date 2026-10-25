/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
/* Qualification only: original public NVN Draw/Reset/Report. No wrapper,
 * compositor, game, CPU pixel producer or GPU-work polling gate. directory is a
 * fresh private worker namespace. Unknown submissions retain the entire owner.
 * Return0 requires observed second-context GPU timestamp overlap and exact
 * actual sample values; no-overlap is explicitly NOT isolation qualification. */
int nn_nvn_counter_probe(const char *directory,char *json,size_t capacity);
