/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
enum nn_order_flags { NN_ORDER_FULL_CACHE = 1u, NN_ORDER_INVALIDATE = 2u };
/* Pure native Adreno PM4 encoder, no allocation or core access. A650 subset.
 * FULL_CACHE needs a private live GPU-writable 4-byte sequence region, not an
 * event or guest buffer. Caller retains its BO and command BO to GPU retirement.
 * NULL words queries exact count. Failure leaves words untouched and count=0.
 * Buffer and sequence must not overlap. CPU cache publication is caller-owned.
 * These are correctness candidates until separately qualified on hardware. */
int nn_order_words(uint32_t chip, uint64_t sequence_address, unsigned flags,
                   uint32_t *words, size_t capacity, size_t *count);
