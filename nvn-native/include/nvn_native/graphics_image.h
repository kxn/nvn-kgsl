/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

/* All patches write a little-endian GPUVA64 at commands[word..word+1].
 * offset is a byte offset into the selected BO; bytes is the minimum range
 * that must remain accessible from that offset until submission completes.
 * A generated image is an in-process C description, not an on-disk ABI. */
enum nn_gfx_patch_kind {
    NN_GFX_VS_CODE = 0,
    NN_GFX_FS_CODE = 1,
    NN_GFX_TARGET = 2,
    NN_GFX_VERTICES = 3,
    NN_GFX_MATRIX = 4,
    NN_GFX_ZERO = 5,
    NN_GFX_FS_UNIFORM = 6,
    NN_GFX_TEXTURE_DESCRIPTORS = 7,
    NN_GFX_SAMPLERS = 8,
    NN_GFX_TEXTURE = 9,
    NN_GFX_SEQUENCE = 10,
    NN_GFX_PATCH_KIND_COUNT = 11
};
enum nn_gfx_profile { NN_GFX_COLOR = 0, NN_GFX_TEXTURE_2D_NORMALIZED = 1 };
struct nn_gfx_patch {
    uint32_t word, kind, offset, bytes;
};
/* Explicit per-draw snapshot; these are NVN enum values, not shader-query bools.
 * Only full viewport/scissor, depth range0..1, no cull/depth/FragCoord is modeled. */
struct nn_gfx_draw_state { uint32_t window_origin, depth_mode; };
enum nn_gfx_state_kind {
    NN_GFX_STATE_VIEWPORT_YSCALE = 0,
    NN_GFX_STATE_VIEWPORT_ZOFFSET = 1,
    NN_GFX_STATE_VIEWPORT_ZSCALE = 2,
    NN_GFX_STATE_CLIP_HALFZ = 3,
    NN_GFX_STATE_KIND_COUNT = 4
};
/* Schema3 patches a single declared type4 value, never a GPUVA pair. header
 * must be the generated one-register type4 packet at commands[word-1]. Each
 * kind has a fixed sourced register/mask/value pair validated by the encoder.
 * value[0/1] is selected by origin for YSCALE and depth_mode for other kinds. */
struct nn_gfx_state_patch {
    uint32_t word, reg, header, expected, mask, kind, value[2];
};
struct nn_gfx_image {
    uint32_t schema, chip, gmem_bytes;
    uint32_t width, height, target_pitch, target_layer_bytes, target_bytes;
    uint32_t target_alignment, code_alignment, vertex_bytes, matrix_bytes, zero_bytes;
    const uint32_t *vs_code;
    uint32_t vs_code_bytes;
    const uint32_t *fs_code;
    uint32_t fs_code_bytes;
    const uint32_t *commands;
    uint32_t command_words;
    const struct nn_gfx_patch *patches;
    uint32_t patch_count;
    /* Appended schema-2 fields. Schema-1 images keep these zero and retain
     * their original command/code bytes. Texture descriptors include hole 0.
     * texture_patches address words in texture_descriptors, with GPUVA bits
     * selected by texture_address_mask; preserve all non-address bits. */
    /* fs_uniform_binding is an IR3 UBO descriptor number, never an NVN slot.
     * Frontend reflection supplies the API/source buffer mapping separately. */
    uint32_t profile, vertex_stride, fs_uniform_bytes, fs_uniform_binding;
    uint32_t texture_width, texture_height, texture_pitch, texture_layer_bytes;
    uint32_t texture_bytes, texture_alignment, texture_slots;
    const uint32_t *texture_descriptors;
    uint32_t texture_descriptor_words;
    const uint32_t *samplers;
    uint32_t sampler_words;
    const struct nn_gfx_patch *texture_patches;
    uint32_t texture_patch_count;
    uint64_t texture_address_mask;
    /* A6XX timestamp-bearing cache events write this internal control BO. */
    uint32_t sequence_bytes;
    /* Appended schema3 fields. Legacy1/2 keep these zero: they retain their
     * raw fixed +Y/half-Z meaning, not a complete NVN default-mode guarantee. */
    const struct nn_gfx_state_patch *state_patches;
    uint32_t state_patch_count;
};
