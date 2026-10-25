/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
/* STRUCTURAL CANDIDATE ONLY. No submit/ownership/cache-clean implementation.
 * Caller must independently prove source RT clean + finished barrier, unlocked
 * destination, physically distinct backing, imported BO lifetime and queue
 * ownership. GPUVA disjointness below does NOT establish backing identity.
 * Whole byte-addressed 64RGBA image only: no format conversion/tiling/scaling.
 */
struct nn_copy64_region {
    uint64_t gpu_base, bo_bytes, pixel_offset;
    uint32_t pitch, width, height, rgba8, linear, samples, levels, layers;
};
int nn_copy64_body(uint32_t chip, const struct nn_copy64_region *src,
                   const struct nn_copy64_region *dst,
                   uint32_t *words, size_t capacity_words, size_t *written_words);
