/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
/* Standalone qualification linkage ONLY: --wrap=nn_bo_import and
 * --wrap=nn_queue_submit. No production core/window hooks. directory already
 * exists; JSON needs8192 bytes. 4 original shader cases × samequeue/afterFinish.
 * Return0 only after all pixels/IB guards/ownership cleanup succeed. Failure
 * retains uncertain NVN/AHB/BO owners; do not invoke again after retention.
 * No compositor present, game, CPU target writes, CPU source reads or raw draw.
 */
int nn_nvn_copy_probe(const char *directory,char *json,size_t capacity);
