/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum nn_cs_patch_kind {
    NN_CS_CODE, NN_CS_DESCRIPTORS, NN_CS_SEQUENCE, NN_CS_UBO,
    NN_CS_GROUP_COUNT, NN_CS_GLOBAL_SIZE
};
struct nn_cs_patch { uint32_t word, kind, index, offset; };
struct nn_cs_image {
    uint32_t schema, chip, local[3], ssbo_slots;
    const uint32_t *code; size_t code_bytes;
    const uint32_t *commands; size_t command_words;
    const struct nn_cs_patch *patches; size_t patch_count;
    uint32_t descriptor[16], width_mask, height_mask, height_shift, offset_mask, offset_shift;
};
/* R32_UINT bindful buffer layout. The fixed descriptor and field definitions
 * come from FDL/generated register headers in the pinned compiler build. */
static inline int nn_cs_buffer_descriptor(const struct nn_cs_image *image, uint32_t out[16],
                                          uint64_t address, uint32_t bytes)
{
    if (!bytes || bytes % 4 || address % 4 || address > UINT64_MAX - bytes) return -1;
    uint32_t elements = bytes / 4, offset = (address & 63) / 4;
    if ((elements >> 15) > (image->height_mask >> image->height_shift) ||
        offset > (image->offset_mask >> image->offset_shift)) return -1;
    memcpy(out, image->descriptor, sizeof(image->descriptor));
    out[1] = (elements & image->width_mask) |
             (((elements >> 15) << image->height_shift) & image->height_mask);
    out[2] = (out[2] & ~image->offset_mask) | ((offset << image->offset_shift) & image->offset_mask);
    address &= ~UINT64_C(63);
    out[4] = address; out[5] = address >> 32;
    return 0;
}
