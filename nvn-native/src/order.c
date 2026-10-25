/* SPDX-License-Identifier: MIT */
#include "nvn_native/order.h"
#include "nvn_native/pm4.h"
#include <errno.h>
#include <string.h>
/* Hardware values are from pinned Mesa adreno_pm4.xml (MIT): CP_EVENT_WRITE,
 * CACHE_FLUSH_TS, PC_CCU_* and LRZ_FLUSH_INVALIDATE. The 26-word clean tail
 * matches this project's hardware-qualified A650 graphics/copy profile.
 * No Vulkan runtime or original renderer is linked or called. */
int nn_order_words(uint32_t chip,uint64_t seq,unsigned flags,uint32_t *out,size_t cap,size_t *count)
{
    if (!count) return EINVAL;
    *count=0;
    if (chip!=0x06050002u || flags&~3u) return ENOTSUP;
    if ((flags&NN_ORDER_FULL_CACHE) && (!seq || seq%4 || seq>UINT64_MAX-4)) return EINVAL;
    uint32_t w[32]; size_t n=0;
    if (flags&NN_ORDER_FULL_CACHE) {
        w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);
        const uint32_t events[]={0x1du,0x1cu,0x19u,0x18u,0x04u};
        for (unsigned i=0;i<5;i++) {
            int ts=i<2 || i==4;
            w[n++]=nvn_pm4_packet7(0x46,ts?4:1);
            w[n++]=events[i] | (ts?1u<<30:0);
            if (ts) { w[n++]=(uint32_t)seq;w[n++]=(uint32_t)(seq>>32);w[n++]=i+1; }
        }
        w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);
        w[n++]=nvn_pm4_packet7(0x1d,1);w[n++]=0;
        w[n++]=nvn_pm4_packet7(0x46,1);w[n++]=0x26;
        w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);
        w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0);
    }
    if (flags&NN_ORDER_INVALIDATE) {
        w[n++]=nvn_pm4_packet7(0x46,1);w[n++]=0x31;
        w[n++]=nvn_pm4_packet7(0x12,0);
        w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);
        w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0);
    }
    if (out && cap<n) return ENOSPC;
    if (out) memcpy(out,w,n*sizeof(*w));
    *count=n;return 0;
}
