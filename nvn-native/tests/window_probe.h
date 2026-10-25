/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct nn_nvn_window_probe_callbacks {
    void *user;
    /* Called ONLY after stats prove this frame is CURRENT/shown. Zero means
     * continue, positive errno aborts. Controller may hold until HOME/command.
     * Exactly slots*4 calls on success; each (slot,case_index) occurs once. */
    int (*presented)(void *user,unsigned slot,unsigned case_index);
    /* Nonzero means lifecycle closing. UI only requests registry_close(0),
     * then signals this flag; it must never call NVN core from that thread. */
    int (*closing)(void *user);
};
/* Owner thread only. Host registered/attached token to actual SurfaceView ANW
 * before entry. Qualified original untextured shader/profile, N=2/3 slots.
 * Presents exactly4*N frames; readbacks only after reacquire or final safe
 * retire. Final callback closing after all frames is normal; early closing
 * aborts. Return0 only for every oracle, release and cleanup success; other
 * returns are positive errno. Uncertain owners are retained until process
 * teardown. directory exists and is unique for this lifecycle generation;
 * diagnostic files window-slot-I-case-J.rgba. Provide >=8192 JSON bytes. */
int nn_nvn_window_probe(const char *directory,char *json,size_t capacity,int slots,
                       const void *token,const struct nn_nvn_window_probe_callbacks *callbacks);
#ifdef __cplusplus
}
#endif
