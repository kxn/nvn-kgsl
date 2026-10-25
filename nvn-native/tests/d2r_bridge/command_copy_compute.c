/* Existing four-original direct/indirect compute fixture, with actual fixed
 * copies on both sides of a Call. Verify inherited bindings after private copy
 * hardware jobs, not a fabricated NVN Program/UBO/SSBO state update. */
#define NN_CALL_COPY_COMPUTE_MIXED 1
#include "command_compute.c"
