/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
/* Bounded NVN→explicit AHB image-pool qualification, not Window/presentation.
 * N must be2 or3. Worker/owner thread only, no concurrent NVN/AHB calls.
 * Return0 only when all4*N cases, readback and cleanup pass. Failure retains
 * uncertain state until process teardown; do not invoke again after failure.
 * directory exists; output files external-slot-I-case-J.rgba are diagnostics.
 * Supply >=4096 JSON bytes; build must verify NVN_GRAPHICS_API_HEADER bundle. */
int nn_nvn_external_image_probe(const char *directory, char *json, size_t capacity, unsigned slots);
