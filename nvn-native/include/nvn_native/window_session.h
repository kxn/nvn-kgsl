/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pure C platform boundary. No NVN, KGSL, Android headers or core callbacks. */
typedef struct nn_window_session nn_window_session;
enum nn_window_slot_state {
    NN_WINDOW_FREE, NN_WINDOW_ACQUIRED, NN_WINDOW_QUEUED,
    NN_WINDOW_SHOWING, NN_WINDOW_CURRENT, NN_WINDOW_RETAINED
};
struct nn_window_slot {
    void *buffer;             /* borrowed AHardwareBuffer* on Android */
    const void *owner;        /* immutable opaque identity; never dereferenced */
    uint64_t pixel_offset, payload_bytes;
    uint64_t backing_inode; /* fstat of VERIFIED pixel FD, never GPUVA/owner ID */
    uint32_t backing_dev_major, backing_dev_minor;
    uint32_t width, height, pitch, format, usage;
};
struct nn_window_ticket { uint64_t generation, serial; uint32_t index; };
struct nn_window_session_stats {
    uint64_t generation, acquires, enqueued, shown, releases, cancelled;
    uint32_t slots, queued, state[3];
    uint32_t ready, closing, worker_done, compositor_retired, retired, resources_retained;
    uint32_t show_completed, detach_completed, completion_callbacks;
    uint32_t present_fences_waited, release_fences_waited;
    int32_t current_index, first_error;
    /* Successful platform show arguments, semantic diagnostics only. These
     * are NOT measured cadence, GPU timestamps or compositor presentation times. */
    int32_t last_shown_interval;
    uint64_t shown_intervals[6];
};
/* Trusted host registers the EXACT VI token identity. Never dereferences it.
 * attach retains its ANativeWindow; duplicate live attach/session is EBUSY.
 * Reattach only after previous session destroy; generation never wraps/reuses. */
int nn_window_registry_register(const void *token);
int nn_window_registry_attach(const void *token, void *native_window, uint64_t *generation);
/* UI may call close. It immediately closes that generation and wakes Acquire.
 * timeout0 only requests stop. Nonzero timeout<=1500ms is ONE total deadline,
 * independent of the present helper's internal waits. Timeout poisons/retains
 * the session; UI must not join/free it or retry. Old-generation close is ESTALE.
 * A successful wait is compositor acknowledgement only, NOT GPU retirement. */
int nn_window_registry_close(const void *token, uint64_t generation, uint32_t timeout_ms);
/* Owner has already pinned every caller texture/pool and qualified independent
 * external AHB backings; dev/ino comes from importer fstat, not caller guesses.
 * Rejects overlapping payload ranges on the same backing (also duplicate AHB/owner). Copies N=2/3 immutable slots; no ownership transfer.
 * Worker initialization is asynchronous; Acquire observes initialization error.
 * On success retain core pins until retire succeeds and destroy succeeds. */
int nn_window_session_create(const void *token, const struct nn_window_slot *slots,
                             uint32_t count, nn_window_session **out);
/* One serialized core owner. timeout0 polls; otherwise<=1500ms. Never calls
 * core from worker/UI. Failure clears output. Closing wakes blocked Acquire.
 * All slots caller-acquired with no queued/current progress => EBUSY. */
int nn_window_session_acquire(nn_window_session *, uint32_t timeout_ms, struct nn_window_ticket *out);
int nn_window_session_check_acquired(nn_window_session *, const struct nn_window_ticket *);
/* Precondition: owner successfully finished ALL GPU work for this ticket.
 * No GPU/SC wait here. Worker alone submits the unchanged AHB. Old CURRENT
 * becomes FREE only after helper show returns with previous release proof. */
int nn_window_session_enqueue_ready(nn_window_session *, const struct nn_window_ticket *);
int nn_window_session_get_stats(nn_window_session *, struct nn_window_session_stats *out);
/* Owner MUST first Finish the producer, including unpresented ACQUIRED writes.
 * Signals stop/GPU retirement, waits at most one total timeout<=1500ms. Any
 * timeout/error keeps session and all core pins alive. UI close alone cannot
 * satisfy this precondition or make ACQUIRED buffers reusable. */
int nn_window_session_retire(nn_window_session *, uint32_t timeout_ms);
/* Owner only, after successful retire. No join or platform wait. EBUSY retains.
 * Unbinds registry, allowing next attach/generation; NULL is a no-op. */
int nn_window_session_destroy(nn_window_session *);
/* Native interval profile: 0 mailbox (only unshown GPU-complete queued slots),
 * 1 FIFO, 2..5 FIFO paced by actual OS vsyncs. Each enqueue snapshots interval.
 * Capability is checked before worker creation / setter publication. */
int nn_window_session_interval_supported(int interval);
int nn_window_session_create_interval(const void *token,const struct nn_window_slot *slots,
    uint32_t count,int interval,nn_window_session **out);
int nn_window_session_set_present_interval(nn_window_session *,int interval);

#ifdef __cplusplus
}
#endif
