/* SPDX-License-Identifier: MIT
 * Recipe derived from Mesa-26.2.5 fd6_emit.cc:1152-1173 and
 * fd6_const.cc:152-154,490-495; see plan.md/source-sha256.json.
 */
#include "copy_body.h"
#include "nvn_native/pm4.h"
#include <stdbool.h>
#include <errno.h>
#include "adreno_pm4.xml.h" /* pinned existing generated packers/opcodes */
static int bounds(const struct nn_copy64_region *r, uint64_t *start, uint64_t *end)
{
    if (!r || r->width != 64 || r->height != 64 || r->rgba8 != 1 ||
        r->linear != 1 || r->samples != 1 || r->levels != 1 || r->layers != 1)
        return ENOTSUP;
    if (!r->gpu_base || r->gpu_base % 4 || r->pixel_offset % 4 ||
        r->pitch < 256 || r->pitch > 4096 || r->pitch % 4 ||
        !r->bo_bytes || r->gpu_base > UINT64_MAX-r->bo_bytes) return EINVAL;
    uint64_t span = (uint64_t)(r->height-1)*r->pitch + r->width*4u;
    if (r->pixel_offset > r->bo_bytes || span > r->bo_bytes-r->pixel_offset)
        return EINVAL;
    *start = r->gpu_base+r->pixel_offset; *end = *start+span;
    return 0;
}
int nn_copy64_body(uint32_t chip, const struct nn_copy64_region *src,
                   const struct nn_copy64_region *dst,
                   uint32_t *words, size_t capacity, size_t *written)
{
    if (!written) return EINVAL;
    *written = 0;
    if (chip != 0x06050002) return ENOTSUP;
    uint64_t s, se, d, de;
    int e = bounds(src,&s,&se); if (e) return e;
    e = bounds(dst,&d,&de); if (e) return e;
    if (s < de && d < se) return EINVAL;
    /* 4096 DWORD moves * (header+control+DST64+SRC64), then cache
     * invalidate header/payload + WAIT_MEM_WRITES + WFI + WFM.
     * Producer clean/invalidate/WFI/WFM must PRECEDE this entire body.
     */
    const size_t required = 64u*64u*6u + 5u;
    if (!words || capacity < required) return ENOSPC;
    size_t n = 0;
    for (uint32_t y = 0; y < 64; ++y) for (uint32_t x = 0; x < 64; ++x) {
        uint64_t a = s+(uint64_t)y*src->pitch+x*4u;
        uint64_t b = d+(uint64_t)y*dst->pitch+x*4u;
        words[n++] = nvn_pm4_packet7(CP_MEM_TO_MEM,5);
        words[n++] = 0; /* CP_MEM_TO_MEM_0(), no arithmetic or unknown flags */
        words[n++] = CP_MEM_TO_MEM_DST_LO((uint32_t)b);
        words[n++] = CP_MEM_TO_MEM_DST_HI((uint32_t)(b>>32));
        words[n++] = CP_MEM_TO_MEM_SRC_A_LO((uint32_t)a);
        words[n++] = CP_MEM_TO_MEM_SRC_A_HI((uint32_t)(a>>32));
    }
    /* Exact Gallium wait_mem_writes consumer flags expanded by
     * fd6_emit_flushes, plus the existing qualification's trailing WFM.
     * No AHB lock or implicit cache maintenance is relied on for source.
     */
    words[n++] = nvn_pm4_packet7(CP_EVENT_WRITE,1);
    words[n++] = CP_EVENT_WRITE_0_EVENT(CACHE_INVALIDATE);
    words[n++] = nvn_pm4_packet7(CP_WAIT_MEM_WRITES,0);
    words[n++] = nvn_pm4_packet7(CP_WAIT_FOR_IDLE,0);
    words[n++] = nvn_pm4_packet7(CP_WAIT_FOR_ME,0);
    *written = n;
    return 0;
}
