/* SPDX-License-Identifier: MIT. Experimental public-writer interop profile.
 * NOT a complete NVN packaged layout specification. Actual NVN association is
 * parent v12 observation; production activation MUST be explicit/defaultOFF. */
#pragma once
#include <stdint.h>
/* Only writer-produced words{BH,0x10007}, BH0..5. Positive errno; out zeroed.
 * Does not choose NVN kind/compression, parse data, copy pixels or enable GPU. */
int nn_packaged_bntx_height(const uint8_t bytes[8],uint32_t *out);
