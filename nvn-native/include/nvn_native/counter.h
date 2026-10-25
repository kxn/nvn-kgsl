/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
enum nn_counter_operation {
    NN_COUNTER_RESET_SAMPLES, NN_COUNTER_REPORT_SAMPLES,
    NN_COUNTER_REPORT_TIMESTAMP, NN_COUNTER_REPORT_TIMESTAMP_TOP,
    NN_COUNTER_BEGIN_SAMPLES, NN_COUNTER_END_SAMPLES
};
/* Pure A650 encoder; no allocation, submit, CPU counter emulation or profiling.
 * samples_state is a retained private 64B region, aligned to 16; baseline at0,
 * stop at16, GPU accumulated result at32. Report destination is original caller's aligned 16B counterData.
 * All memory references stay live until successful producer QueueFinish.
 * Timestamp fields are actual Adreno always-on ticks, NOT CPU-clock samples.
 * Query completion follows Gallium's sentinel/WAIT_REG_MEM recipe verbatim.
 * NULL output queries exact size. Failure leaves output unchanged/count0.
 * This candidate is NOT proof of sample-counter isolation across contexts. */
int nn_counter_words(uint32_t chip, enum nn_counter_operation operation,
                     uint64_t samples_state, uint64_t destination,
                     uint32_t *words, size_t capacity, size_t *count);
/* Exact 19.2MHz-domain conversion; no Vulkan/Metal clock or timing machinery. */
int nn_counter_ticks_to_ns(uint64_t ticks,uint64_t *nanoseconds);

#ifdef __cplusplus
}
#endif
