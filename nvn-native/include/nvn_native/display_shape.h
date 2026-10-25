/* SPDX-License-Identifier: MIT */
#pragma once
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
/* A650, single-mip/layer RGBA8 LINEAR subset. These are FDL allocation
 * constraints, not a guarantee that arbitrary gralloc usage is linear.
 * width/height and pitch come from actual AHB describe/lock results; direct
 * GPU byte addressing additionally requires verified same-FD shared mapping.
 * Original FDL rules: fd6_layout.c fdl6_layout_image, 64B pitch/base,
 * final level height4, layer4096. Explicit imports can have a smaller FDL
 * size than layer_stride; this bounded contract deliberately requires a full
 * layer_stride in the verified FD/VMA. No padding pixels are read/written
 * by this helper. No UBWC or Tegra source-kind inference. */
struct nn_display_rgba8_shape {
    uint64_t row_bytes, visible_span, storage_bytes;
    uint32_t width, height, pitch, aligned_height;
};
static inline int nn_display_rgba8_shape(uint32_t width,uint32_t height,uint32_t pitch,
                                         struct nn_display_rgba8_shape *out)
{
    if (!out) return EINVAL;
    *out=(struct nn_display_rgba8_shape){0};
    if (!width || !height || width>16384 || height>16384 ||
        pitch>1048576 || pitch%64 || (uint64_t)pitch<(uint64_t)width*4) return ENOTSUP;
    uint32_t aligned=(height+3u)&~3u;
    uint64_t span=(uint64_t)pitch*height, bytes=(uint64_t)pitch*aligned;
    if (bytes>UINT64_MAX-4095) return EOVERFLOW;
    bytes=(bytes+4095u)&~UINT64_C(4095);
    if (bytes>SIZE_MAX || bytes>INT32_MAX) return EOVERFLOW;
    *out=(struct nn_display_rgba8_shape){(uint64_t)width*4,span,bytes,width,height,pitch,aligned};
    return 0;
}
static inline int nn_display_rgba8_span(const struct nn_display_rgba8_shape *shape,
                                        uint64_t offset,uint64_t fd_bytes)
{
    if (!shape || !shape->storage_bytes) return EINVAL;
    if (offset%64 || !fd_bytes || fd_bytes%4096 || offset>fd_bytes ||
        shape->storage_bytes>fd_bytes-offset) return ENOTSUP;
    return 0;
}
