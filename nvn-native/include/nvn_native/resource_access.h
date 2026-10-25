/* SPDX-License-Identifier: MIT */
#pragma once
#include "kgsl.h"
/* Single-owner bookkeeping only: 0 means no pending KGSL references; EBUSY
 * includes uncertain submits. This is not a wait or a visibility barrier. */
int nn_bo_idle(const nn_bo *);
