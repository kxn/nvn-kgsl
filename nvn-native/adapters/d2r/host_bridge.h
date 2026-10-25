#pragma once
#include <nvn.h>

#ifdef __cplusplus
extern "C" {
#endif

/* D2R caller adapter, not a second NVN implementation. Build host_bridge.c with
 * NN_D2R_NVN_NAMES_HEADER naming the verified generated names.inc.h. Supported
 * declared entries use actual-PFN typed forwarding wrappers. Each call checks
 * the driver's sticky diagnostic before and immediately after forwarding;
 * nonzero diagnostics print API/errno/phase and terminate execution, including
 * void calls with unsupported arguments. Return values (including false/zero)
 * are otherwise unchanged, and diagnostics are never silently cleared.
 * Missing known entries terminate when called. Unknown ABI entries are never
 * forwarded, and unknown/null lookup names terminate immediately. Both initial
 * NULL-device and later DeviceGetProcAddress lookups stay inside this adapter.
 * Regenerate names.inc.h with extractor schema2; it contains names/signatures
 * and forwarding arguments only, never original game binary or code.
 *
 * Define NN_D2R_HOST_HLE when integrating with the original dm/host loader. This
 * registers its existing HLE_FORCE("nvnBootstrapLoader") hook; exclude both
 * graphics_boundary.c and hle_nvn.c from that target. There is no loader, CPU
 * gate, graphics implementation or DeviceFlags acceptance in this adapter. */
#define NN_D2R_NVN_UNSUPPORTED_EXIT 78
PFNNVNGENERICFUNCPTRPROC nn_d2r_nvn_bootstrap(const char *name);
PFNNVNGENERICFUNCPTRPROC nn_d2r_nvn_get_proc_address(const NVNdevice *device,
                                                  const char *name);

#ifdef __cplusplus
}
#endif
