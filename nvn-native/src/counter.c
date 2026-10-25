/* SPDX-License-Identifier: MIT
 * Pinned Mesa fd6_query.cc73-168: occlusion copy and completion recipe;
 * fd6_query.cc548-559: 64-bit GPU accumulated stop-start; fd6_emit.h257: RB_DONE_TS;
 * fd6_query.cc515-517: atomic 64-bit register read. Exact opcode/register
 * fields are from adreno_pm4.xml/a6xx.xml; source fingerprints in manifest.
 */
#include "nvn_native/counter.h"
#include "nvn_native/pm4.h"
#include "nvn_native/order.h"
#include <errno.h>
#include <string.h>
static void put_address(uint32_t *w,size_t *n,uint64_t a)
{ w[(*n)++]=(uint32_t)a;w[(*n)++]=(uint32_t)(a>>32); }
static void capture_samples(uint32_t *w,size_t *n,uint64_t address)
{
    w[(*n)++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);
    w[(*n)++]=nvn_pm4_packet7(NVN_CP_MEM_WRITE,4);put_address(w,n,address);
    w[(*n)++]=UINT32_MAX;w[(*n)++]=UINT32_MAX;
    w[(*n)++]=nvn_pm4_packet7(0x12,0); /* CP_WAIT_MEM_WRITES */
    w[(*n)++]=nvn_pm4_packet4(0x8891,1);w[(*n)++]=2; /* COPY, !DISABLE */
    w[(*n)++]=nvn_pm4_packet4(0x8927,2);put_address(w,n,address);
    w[(*n)++]=nvn_pm4_packet7(0x46,1);w[(*n)++]=0x15; /* ZPASS_DONE */
    w[(*n)++]=nvn_pm4_packet7(0x3c,6);w[(*n)++]=4u|(1u<<4); /* WRITE_NE, POLL_MEMORY */
    put_address(w,n,address);w[(*n)++]=UINT32_MAX;w[(*n)++]=UINT32_MAX;w[(*n)++]=16;
}
static void record_timestamp(uint32_t *w,size_t *n,uint64_t address)
{
    w[(*n)++]=nvn_pm4_packet7(0x46,4);
    w[(*n)++]=0x16u|(1u<<30); /* RB_DONE_TS, TIMESTAMP */
    put_address(w,n,address);w[(*n)++]=0;
}
int nn_counter_words(uint32_t chip,enum nn_counter_operation op,uint64_t state,uint64_t dst,uint32_t *out,size_t capacity,size_t *count)
{
    if(!count)return EINVAL;
    *count=0;
    if(chip!=0x06050002u || (unsigned)op>NN_COUNTER_END_SAMPLES)return ENOTSUP;
    if((op<=NN_COUNTER_REPORT_SAMPLES || op>=NN_COUNTER_BEGIN_SAMPLES) && (!state || state%16 || state>UINT64_MAX-64))return EINVAL;
    if(op!=NN_COUNTER_RESET_SAMPLES && op<NN_COUNTER_BEGIN_SAMPLES && (!dst || dst%16 || dst>UINT64_MAX-16))return EINVAL;
    if(op==NN_COUNTER_REPORT_SAMPLES && state<dst+16 && dst<state+64)return EINVAL;
    uint32_t w[96];size_t n=0,k=0;int e=0;
    if(op==NN_COUNTER_RESET_SAMPLES || op==NN_COUNTER_BEGIN_SAMPLES) {
        if(op==NN_COUNTER_RESET_SAMPLES){
            w[n++]=nvn_pm4_packet7(NVN_CP_MEM_WRITE,4);put_address(w,&n,state+32);
            w[n++]=0;w[n++]=0;w[n++]=nvn_pm4_packet7(0x12,0);
        }
        capture_samples(w,&n,state);
        w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0);
    } else if(op==NN_COUNTER_REPORT_SAMPLES || op==NN_COUNTER_END_SAMPLES) {
        capture_samples(w,&n,state+16);
        /* GPU accumulated result + stop - start. Per-submit snapshots avoid
         * assuming raw hardware counts persist over idle/context reload. */
        w[n++]=nvn_pm4_packet7(NVN_CP_MEM_TO_MEM,9);w[n++]=(1u<<29)|(1u<<2)|(1u<<30);
        put_address(w,&n,op==NN_COUNTER_REPORT_SAMPLES?dst:state+32);
        put_address(w,&n,state+32);put_address(w,&n,state+16);put_address(w,&n,state);
        if(op==NN_COUNTER_REPORT_SAMPLES){record_timestamp(w,&n,dst+8);w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);}
    } else {
        if(op==NN_COUNTER_REPORT_TIMESTAMP) {
            record_timestamp(w,&n,dst);w[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);
        } else {
            /* CP front-of-pipe always-on read. Atomic two-register read,
             * deliberately no preceding WFI/RB_DONE that would turn it bottom. */
            w[n++]=nvn_pm4_packet7(0x3e,3);
            w[n++]=0x0980u|(2u<<18)|(1u<<30);put_address(w,&n,dst);
            w[n++]=nvn_pm4_packet7(0x12,0);
        }
        /* Timestamp reports expose the same actual captured clock in both
         * fields; they do not invent a count or sample the CPU clock. */
        w[n++]=nvn_pm4_packet7(NVN_CP_MEM_TO_MEM,5);w[n++]=1u<<29;
        put_address(w,&n,dst+8);put_address(w,&n,dst);
    }
    if(op!=NN_COUNTER_RESET_SAMPLES && op!=NN_COUNTER_BEGIN_SAMPLES) {
        e=nn_order_words(chip,0,NN_ORDER_INVALIDATE,w+n,96-n,&k);if(e)return e;n+=k;
    }
    if(out && capacity<n)return ENOSPC;
    if(out)memcpy(out,w,n*4);
    *count=n;return 0;
}
int nn_counter_ticks_to_ns(uint64_t ticks,uint64_t *ns)
{
    if(!ns)return EINVAL;
    __uint128_t value=(__uint128_t)ticks*625u/12u;
    if(value>UINT64_MAX)return EOVERFLOW;
    *ns=(uint64_t)value;return 0;
}
