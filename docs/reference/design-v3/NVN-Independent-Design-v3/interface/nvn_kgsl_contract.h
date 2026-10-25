#ifndef NVN_KGSL_CONTRACT_V3_H
#define NVN_KGSL_CONTRACT_V3_H
/* Normative design boundary v3. No driver implementation. Not public NVN ABI. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NVNN_KGSL_CONTRACT_VERSION 3u
#define NVNN_NO_CHUNK UINT32_MAX

typedef enum NvnnResult {
 NVNN_SUCCESS, NVNN_TIMEOUT, NVNN_INVALID_INPUT, NVNN_UNSUPPORTED,
 NVNN_OUT_OF_MEMORY, NVNN_DEVICE_LOST, NVNN_SUBMIT_FAILURE,
 NVNN_UNCERTAIN_EXECUTION
} NvnnResult;
typedef struct NvnnByteRange { uint64_t offset, length; } NvnnByteRange;
typedef struct NvnnBackingSpan {
 uint64_t backing_id, backing_generation;
 NvnnByteRange bytes; /* physical/canonical alias identity, NOT pool identity */
} NvnnBackingSpan;
typedef struct NvnnLogicalRange {
 uint64_t pool_id, pool_generation;
 uint64_t mapping_snapshot_id; /* resolution token, NOT logical identity equality */
 NvnnByteRange bytes;
} NvnnLogicalRange;

typedef struct NvnnCompletionPoint {
 uint64_t device_generation, queue_id, queue_generation, host_serial;
 uint32_t native_context_id, native_timestamp; /* timestamp zero may be valid */
 uint32_t scope, reserved; /* adapter-defined scope; reserved must be zero */
} NvnnCompletionPoint;
/* An ID refers to an immutable set. Comparisons only within matching queue AND
 * generations; never order different producer queues by the numeric IDs. */
typedef uint64_t NvnnDependencySetId;
typedef uint64_t NvnnPublicationSetId;
/* Only committed ACCEPTED accesses; tentative reservations live in a separate
 * transactional registry and cancellation never publishes their content. */
typedef struct NvnnAccessFrontier {
 NvnnDependencySetId last_writers;
 NvnnDependencySetId outstanding_readers;
 NvnnPublicationSetId content_authority;
} NvnnAccessFrontier;
typedef enum NvnnAccessMode { NVNN_READ, NVNN_WRITE, NVNN_READ_MODIFY_WRITE } NvnnAccessMode;
typedef enum NvnnAccessClass {
 NVNN_DATA_ACCESS, NVNN_ATOMIC_DATA_ACCESS,
 NVNN_SYNC_POLL, NVNN_SYNC_SIGNAL
} NvnnAccessClass;
/* SYNC_POLL must not enter a data-reader frontier that blocks its producer
 * signal. Match token byte range, predicate generation, acquire/release scope.
 * Ordinary aliasing data on adjacent bytes is NOT exempted from hazards. */
typedef struct NvnnSyncTokenUse {
 NvnnBackingSpan token_word;
 uint64_t predicate_generation;
 NvnnDependencySetId release_dependencies, acquire_dependencies;
 NvnnAccessClass access_class;
 uint32_t predicate_mode, value;
} NvnnSyncTokenUse;

/* Resource ownership and wait masks are NOT the same thing. All outstanding
 * consumers contribute to lifetime, even if execution edges can be coalesced. */
typedef struct NvnnLifetime {
 uint64_t host_refs, prepared_refs, cpu_leases, observer_refs;
 NvnnDependencySetId accepted_consumers;
 uint64_t quarantine_id; /* nonzero blocks reclaim until verified resolution */
} NvnnLifetime;
typedef enum NvnnCpuMapKind {
 NVNN_NO_CPU_MAP, NVNN_DRIVER_OWNED_MMAP,
 NVNN_BORROWED_HOST_POINTER, NVNN_BORROWED_ALIAS_VMA
} NvnnCpuMapKind;
typedef enum NvnnFdOwnership { NVNN_NO_FD, NVNN_BORROWED_FD, NVNN_OWNED_DUP_FD } NvnnFdOwnership;
typedef struct NvnnMemoryOwnership {
 uint64_t logical_size, imported_extent;
 void *cpu_map;
 uint64_t cpu_mapping_owner_generation;
 size_t cpu_map_length;
 int import_fd;
 NvnnCpuMapKind cpu_map_kind;
 NvnnFdOwnership fd_ownership;
 uint32_t owns_gpu_import;
} NvnnMemoryOwnership;

typedef enum NvnnChunkState {
 NVNN_NOT_ATTEMPTED, NVNN_ATTEMPTING, NVNN_ACCEPTED,
 NVNN_REJECTED_NO_EXECUTION, NVNN_ACCEPTANCE_UNKNOWN
} NvnnChunkState;
typedef enum NvnnAcceptance {
 NVNN_ACCEPT_NONE, NVNN_ACCEPT_PREFIX, NVNN_ACCEPT_ALL, NVNN_ACCEPT_UNKNOWN
} NvnnAcceptance;
typedef enum NvnnFenceExport {
 NVNN_EXPORT_NOT_REQUESTED, NVNN_EXPORT_PENDING, NVNN_EXPORT_OK, NVNN_EXPORT_FAILED
} NvnnFenceExport;
typedef struct NvnnChunkReceipt {
 uint32_t chunk_index;
 NvnnChunkState state;
 NvnnCompletionPoint completion; /* valid iff accepted */
 uint64_t retained_ref_set;
 NvnnFenceExport export_state; /* failure does not undo GPU acceptance */
 int owned_sync_fd; /* -1 unless export succeeded, ownership explicit */
} NvnnChunkReceipt;
typedef struct NvnnSubmitReceipt {
 uint64_t device_generation, logical_batch_id;
 NvnnAcceptance acceptance;
 uint32_t chunk_count, accepted_prefix_count;
 uint32_t first_unproven_chunk; /* first ACCEPTANCE_UNKNOWN; otherwise NO_CHUNK */
 NvnnChunkReceipt *chunks; /* caller preallocates chunk_count before ANY ioctl */
 uint32_t chunk_capacity;
 NvnnResult operation_result;
} NvnnSubmitReceipt;

/* Aggregate acceptance invariants for nonempty batches:
 * NONE: all chunks NOT_ATTEMPTED or REJECTED_NO_EXECUTION.
 * PREFIX: nonempty contiguous accepted prefix, remaining proven unexecuted.
 * ALL: every chunk ACCEPTED. UNKNOWN: at least one ACCEPTANCE_UNKNOWN,
 * regardless of any accepted prefix. Completion validity follows chunk state,
 * never timestamp != 0. Empty-wait submissions use a separate dependency-set
 * completion contract, not a fabricated zero-chunk GPU timestamp.
 * Success/timeout/failure do not replace the receipt. Backend must fill every
 * chunk state on EVERY return path; ATTEMPTING cannot escape without becoming
 * ACCEPTED, REJECTED_NO_EXECUTION or ACCEPTANCE_UNKNOWN. Unknown chunks and
 * possibly reached resources stay quarantined; no replay merely on error. */
struct NvnnKgslSession;
struct NvnnPreparedBatch;
typedef NvnnResult (*NvnnSubmitPrepared)(struct NvnnKgslSession *,
 const struct NvnnPreparedBatch *, NvnnSubmitReceipt *);

static inline int nvnn_v3_range_valid(NvnnByteRange r) {
 return r.length <= UINT64_MAX-r.offset;
}
static inline int nvnn_v3_range_contains(NvnnByteRange a, NvnnByteRange b) {
 if (!nvnn_v3_range_valid(a)||!nvnn_v3_range_valid(b)||b.offset<a.offset) return 0;
 uint64_t delta=b.offset-a.offset;
 return delta<=a.length && b.length<=a.length-delta;
}
static inline int nvnn_v3_same_timeline(NvnnCompletionPoint a,NvnnCompletionPoint b) {
 return a.device_generation==b.device_generation && a.queue_id==b.queue_id &&
 a.queue_generation==b.queue_generation;
}
#ifdef __cplusplus
}
#endif
#endif
