#pragma once
#include <stddef.h>
/* A bounded bring-up operation: CP write or a verified compiled CS fixture.
 * Returns JSON with execution UID, device, submission and completion evidence.
 * Uses the shared KGSL backend; no reuse after uncertain completion.
 */
int nvn_kgsl_probe(char *json, size_t capacity);
