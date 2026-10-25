/* SPDX-License-Identifier: MIT */
#pragma once
#include "nvn_native/graphics.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nn_copy_job nn_copy_job;
enum nn_copy_state { NN_COPY_READY=1, NN_COPY_SUBMITTED, NN_COPY_FINISHED, NN_COPY_RETAINED };
enum nn_copy_order { NN_COPY_SAME_QUEUE=1, NN_COPY_AFTER_FINISH };
enum nn_copy_alias_proof { NN_COPY_ALIAS_UNKNOWN=0, NN_COPY_FRESH_PRIVATE_PAGES_AND_FRESH_AHB=1 };
/* Private, single serialized-owner boundary. These callbacks own BOTH source
 * texture/pool/queue and AHB/imported BO. Pin/validate/prove failure must make
 * no ownership change; unpin failure must preserve the still-held owners.
 * Cookie/callback storage stays live until destroy succeeds. No worker calls.
 * prove_disjoint must establish allocation provenance, never GPUVA/BO inequality.
 * This bounded subset accepts fresh private anonymous source pages and a new
 * independently allocated AHB only; arbitrary aliasing imports are unsupported.
 */
struct nn_copy_owner {
    void *cookie;
    int (*pin)(void *);
    int (*validate_live)(void *);
    int (*prove_disjoint)(void *);
    int (*unpin)(void *);
};
struct nn_copy_desc {
    nn_device *device;
    nn_queue *producer; /* same actual native queue used for the source Draw */
    nn_bo *source, *target;
    size_t source_offset, source_bytes, target_offset, target_bytes;
    uint64_t source_gpu_address; /* original texture address; never rebound */
    uint32_t producer_timestamp, cpu_no_access, alias_proof;
    /* The actual registered, immutable schema3 color artifact. Its explicit
     * A6 sysmem clean/invalidate/WFI/WFM tail is validated at a fixed boundary.
     * Caller has submitted that Draw to producer at producer_timestamp.
     */
    const struct nn_gfx_image *source_image;
    struct nn_copy_owner owner;
};
struct nn_copy_stats {
    uint32_t state, copy_submissions, finish_calls, producer_finish_calls, timestamp;
    uint32_t owner_pinned, resources_retained;
    size_t command_bytes, command_guard_checks, command_readonly_checks;
    int first_error;
};
/* Positive errno. Validation errors: *out=NULL, no pin/alloc/submit. Creation
 * failures unwind if provably safe; if cleanup is uncertain *out is NONNULL
 * and RETAINED, so the caller MUST preserve the entire owner cohort until exit.
 */
int nn_copy_job_create(const struct nn_copy_desc *, nn_copy_job **out);
/* SAME_QUEUE has no intermediate CPU Finish. AFTER_FINISH first waits producer.
 * Never resubmit or retry an uncertain result. No CPU source map/cache calls.
 */
int nn_copy_job_submit(nn_copy_job *, enum nn_copy_order);
int nn_copy_job_finish(nn_copy_job *, uint32_t timeout_ms);
int nn_copy_job_stats(const nn_copy_job *, struct nn_copy_stats *);
/* READY/FINISHED only. Destroy command BO before releasing owner cohort.
 * Any destroy/unpin error poisons the handle; keep it and owners until exit.
 */
int nn_copy_job_destroy(nn_copy_job *);
#ifdef __cplusplus
}
#endif
