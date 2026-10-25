/* SPDX-License-Identifier: MIT. Device-only restore, no shader/draw template. */
#pragma once
#include <stddef.h>
#include <stdint.h>
enum nn_pipeline_platform_patch_kind {NN_PLATFORM_ZERO,NN_PLATFORM_SEQUENCE};
struct nn_pipeline_platform_patch {uint32_t word,kind,offset,bytes;};
struct nn_pipeline_platform_segment {
    const uint32_t *words;size_t word_count;
    const struct nn_pipeline_platform_patch *patches;size_t patch_count;
};
struct nn_pipeline_platform {
    uint32_t version,chip,gmem_bytes;
    struct nn_pipeline_platform_segment begin,end;
};
