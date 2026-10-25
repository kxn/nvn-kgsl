/* SPDX-License-Identifier: MIT */
#pragma once
#include "kgsl.h"
#include "compute_image.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nn_cs_buffer { uint64_t address; uint32_t bytes; };
/* Emit one immutable dispatch IB and descriptor/sequence storage. The caller
 * owns the returned BO, and must retain code and all bound resources as well.
 * On error, a non-NULL output still owns an unsubmitted BO whose cleanup
 * failed. The caller must retain it and retry destruction; bytes remains0. */
int nn_cs_encode(nn_device *device, nn_bo *code, const struct nn_cs_image *image,
                 const struct nn_cs_buffer ubos[16], const struct nn_cs_buffer ssbos[16],
                 const uint32_t groups[3], nn_bo **job, size_t *command_bytes);
/* GPU reads the three group counts when this IB executes; recording never
 * reads the caller buffer. Retain and submit its BO alongside code/resources. */
int nn_cs_encode_indirect(nn_device *device, nn_bo *code, const struct nn_cs_image *image,
                 const struct nn_cs_buffer ubos[16], const struct nn_cs_buffer ssbos[16],
                 uint64_t indirect_address, nn_bo **job, size_t *command_bytes);
#ifdef __cplusplus
}
#endif
