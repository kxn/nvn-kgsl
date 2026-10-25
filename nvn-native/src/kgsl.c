#define _GNU_SOURCE
#include "nvn_native/kgsl.h"
#define __user
#include "msm_kgsl.h"
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct nn_device { int fd, close_error; uint32_t chip; size_t page, gmem_bytes, bos, queues; struct nn_bo *failed_imports; };
struct nn_bo { nn_device *dev; uint32_t id, flags; uint64_t address; size_t size, mapsize, busy; void *map; int imported; struct nn_bo *failed_next; };
struct pending { struct pending *next; nn_bo **bos; size_t count; uint32_t timestamp; };
struct nn_queue { nn_device *dev; uint32_t id, timestamp; int unknown, submitted; struct pending *pending; };
static int read_ioctl(int fd, unsigned long op, void *arg)
{
    int r; do { r = ioctl(fd, op, arg); } while (r < 0 && errno == EINTR);
    return r ? errno : 0;
}
int nn_device_inspect(struct nn_device_identity *identity)
{
    if (!identity) return EINVAL;
    *identity = (struct nn_device_identity){0};
    int fd = open("/dev/kgsl-3d0", O_RDWR | O_CLOEXEC);
    if (fd < 0) return errno;
    struct kgsl_devinfo info = {0};
    struct kgsl_device_getproperty prop = {.type = KGSL_PROP_DEVICE_INFO, .value = &info, .sizebytes = sizeof(info)};
    int e = read_ioctl(fd, IOCTL_KGSL_DEVICE_GETPROPERTY, &prop);
    int close_error = close(fd) ? errno : 0;
    if (!e) {
        long page = sysconf(_SC_PAGESIZE);
        if (page <= 0) return ENOTSUP;
        *identity = (struct nn_device_identity){info.chip_id, info.gpu_id, info.device_id,
            info.mmu_enabled, (size_t)page, info.gmem_sizebytes};
    }
    return e ? e : close_error;
}
int nn_device_open(nn_device **out)
{
    *out = NULL;
    nn_device *d = calloc(1, sizeof(*d)); if (!d) return ENOMEM;
    d->fd = open("/dev/kgsl-3d0", O_RDWR | O_CLOEXEC);
    if (d->fd < 0) { int e = errno; free(d); return e; }
    long page = sysconf(_SC_PAGESIZE);
    struct kgsl_devinfo info = {0};
    struct kgsl_device_getproperty p = {.type = KGSL_PROP_DEVICE_INFO, .value = &info, .sizebytes = sizeof(info)};
    int e = read_ioctl(d->fd, IOCTL_KGSL_DEVICE_GETPROPERTY, &p);
    /* Exact devices with separately recorded kernel/CP qualification. Shader
     * and graphics images must still match the actual chip and generation. */
    if (!e && (page <= 0 || (info.chip_id != 0x43030b00 && info.chip_id != 0x06050002))) e = ENOTSUP;
    if (e) { close(d->fd); free(d); return e; }
    d->page = page; d->chip = info.chip_id; d->gmem_bytes = info.gmem_sizebytes; *out = d; return 0;
}
int nn_device_close(nn_device *d)
{
    if (d->close_error) return d->close_error;
    size_t failed = 0; int unidentified = 0;
    for (nn_bo *b = d->failed_imports; b; b = b->failed_next) {
        failed++; if (!b->id || (!b->imported && !b->address)) unidentified = 1;
    }
    if (unidentified) {
        /* A failed allocating ioctl may not return an id. Only whole-fd
         * teardown can retire that uncertainty; never free/reuse the device
         * while any ordinary BO or queue still belongs to it. */
        if (d->bos != failed || d->queues) return EBUSY;
        if (close(d->fd)) {
            d->close_error = errno; d->fd = -1; return d->close_error;
        }
        while (d->failed_imports) {
            nn_bo *b = d->failed_imports;
            d->failed_imports = b->failed_next; free(b);
        }
        free(d); return 0;
    }
    /* Failed metadata qualification owns a real kernel allocation even if
     * its original free failed. Preserve its imported/allocated free route,
     * retry only on owner teardown, never repeat the allocating mutation. */
    while (d->failed_imports) {
        nn_bo *b = d->failed_imports, *next = b->failed_next;
        int e = nn_bo_destroy(b); if (e) return e;
        d->failed_imports = next;
    }
    if (d->bos || d->queues) return EBUSY;
    if (close(d->fd)) {
        /* close errors have ambiguous descriptor state: retain a poisoned
         * owner rather than free memory still held by the NVN device. */
        d->close_error = errno; d->fd = -1; return d->close_error;
    }
    free(d); return 0;
}
uint32_t nn_device_chip(const nn_device *d) { return d->chip; }
size_t nn_device_page(const nn_device *d) { return d->page; }
size_t nn_device_gmem_bytes(const nn_device *d) { return d->gmem_bytes; }
/* Successful allocation is already counted in d->bos. A rejected result or
 * failed mmap still owns that allocation if its cleanup fails. Keep the exact
 * allocated route (imported==0/SHAREDMEM_FREE), not import's GPUOBJ_FREE. */
static int reject_allocation(nn_device *d, nn_bo *b, int error)
{
    if (b->id && b->address && !nn_bo_destroy(b)) return error;
    /* Missing identity requires the existing whole-fd teardown protocol;
     * explicit device close must first exclude all ordinary BOs and queues. */
    b->failed_next = d->failed_imports; d->failed_imports = b;
    return error;
}
int nn_bo_alloc(nn_device *d, size_t bytes, nn_bo **out)
{
    *out = NULL;
    if (!bytes || bytes > SIZE_MAX - d->page) return EINVAL;
    size_t aligned = (bytes + d->page - 1) / d->page * d->page;
    nn_bo *b = calloc(1, sizeof(*b)); if (!b) return ENOMEM;
    /* Older arm64 KGSL maps UNCACHED as Device memory, which rejects ordinary
     * compiler-generated unaligned stores. WRITECOMBINE is Normal-NC and needs
     * no CPU cache clean; the submit/finish fences order its writes/reads. */
    uint32_t cachemode = d->chip == 0x06050002 ? KGSL_CACHEMODE_WRITECOMBINE : KGSL_CACHEMODE_UNCACHED;
    struct kgsl_gpumem_alloc_id req = {.size = aligned, .flags = cachemode << KGSL_CACHEMODE_SHIFT};
    if (ioctl(d->fd, IOCTL_KGSL_GPUMEM_ALLOC_ID, &req)) { int e = errno; free(b); return e; }
    b->dev = d; b->id = req.id; b->flags = req.flags; b->size = req.size; b->mapsize = req.mmapsize; b->address = req.gpuaddr;
    d->bos++;
    if (!b->id || !b->address || b->size < aligned || b->mapsize < b->size ||
        b->mapsize > aligned + d->page ||
        (req.flags & KGSL_CACHEMODE_MASK) != (cachemode << KGSL_CACHEMODE_SHIFT)) {
        return reject_allocation(d, b, ENOTSUP);
    }
    b->map = mmap(NULL, b->mapsize, PROT_READ | PROT_WRITE, MAP_SHARED, d->fd, (off_t)b->id * d->page);
    if (b->map == MAP_FAILED) { int e = errno; b->map = NULL; return reject_allocation(d, b, e); }
    *out = b; return 0;
}
static int retain_failed_import(nn_device *d, nn_bo *b, uint32_t id, int error)
{
    b->dev = d; b->id = id; b->imported = 3;
    b->failed_next = d->failed_imports; d->failed_imports = b; d->bos++;
    return error;
}
static int reject_import(nn_device *d, nn_bo *b, uint32_t id, int error)
{
    struct kgsl_gpuobj_free release = {.id = id};
    if (!id || ioctl(d->fd, IOCTL_KGSL_GPUOBJ_FREE, &release)) {
        return retain_failed_import(d, b, id, error);
    } else free(b);
    return error;
}
static int import_pages(nn_device *d, void *storage, size_t bytes, nn_bo **out, int coherent)
{
    if (!out) return EINVAL;
    *out = NULL;
    if (!d || !storage || !bytes || (uintptr_t)storage % d->page || bytes % d->page) return EINVAL;
    if (coherent && d->chip != 0x06050002) return ENOTSUP;
    nn_bo *b = calloc(1, sizeof(*b)); if (!b) return ENOMEM;
    /* Android heap pointers may carry an AArch64 TBI tag. The nested UAPI
     * integer address is not automatically untagged by the syscall boundary. */
    uintptr_t host = (uintptr_t)storage;
#if defined(__aarch64__)
    host &= UINT64_C(0x00ffffffffffffff);
#endif
    if (d->chip == 0x06050002) {
        /* This device's legacy MAP_USER_MEM drops CACHEMODE for anonymous
         * pages; its sync-cache ioctl then treats them as WC and does no clean.
         * GPUOBJ_IMPORT preserves WB. For ADDR, priv_len is the mapping size,
         * despite the generic UAPI field description. Never retry a mutation. */
        struct kgsl_gpuobj_import_useraddr user = {.virtaddr = host};
        uintptr_t private_address = (uintptr_t)&user;
#if defined(__aarch64__)
        private_address &= UINT64_C(0x00ffffffffffffff);
#endif
        struct kgsl_gpuobj_import req = {.priv = private_address, .priv_len = bytes,
            .flags = (KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT) |
                     (coherent ? KGSL_MEMFLAGS_IOCOHERENT : 0),
            .type = KGSL_USER_MEM_TYPE_ADDR};
        if (ioctl(d->fd, IOCTL_KGSL_GPUOBJ_IMPORT, &req)) {
            /* Do not retry the mutation or assume no kernel object exists. */
            return retain_failed_import(d, b, 0, errno);
        }
        struct kgsl_gpumem_get_info info = {.id = req.id};
        int e = read_ioctl(d->fd, IOCTL_KGSL_GPUMEM_GET_INFO, &info);
        uint64_t required = (KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT) |
                            (coherent ? KGSL_MEMFLAGS_IOCOHERENT : 0);
        uint64_t mask = KGSL_CACHEMODE_MASK | (coherent ? KGSL_MEMFLAGS_IOCOHERENT : 0);
        if (e || !req.id || info.id != req.id || !info.gpuaddr || info.gpuaddr % d->page || info.size != bytes ||
            (req.flags & mask) != required || (info.flags & mask) != required ||
            ((req.flags | info.flags) & (KGSL_MEMFLAGS_GPUREADONLY | KGSL_MEMFLAGS_SECURE))) {
            return reject_import(d, b, req.id, e ? e : ENOTSUP);
        }
        b->dev = d; b->address = info.gpuaddr; b->size = bytes; b->map = storage;
        b->id = info.id; b->flags = info.flags; b->imported = 1;
        d->bos++; *out = b; return 0;
    }
    struct kgsl_map_user_mem req = {.fd = -1, .len = bytes, .hostptr = host,
        .memtype = KGSL_USER_MEM_TYPE_ADDR, .flags = KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT};
    if (ioctl(d->fd, IOCTL_KGSL_MAP_USER_MEM, &req)) { int e = errno; free(b); return e; }
    b->dev = d; b->address = req.gpuaddr; b->size = bytes; b->map = storage; b->imported = 1;
    d->bos++;
    struct kgsl_gpumem_get_info info = {.gpuaddr = req.gpuaddr};
    int e = read_ioctl(d->fd, IOCTL_KGSL_GPUMEM_GET_INFO, &info);
    if (e || !info.id || !req.gpuaddr) { nn_bo_destroy(b); return e ? e : ENOTSUP; }
    b->id = info.id; b->flags = info.flags; *out = b; return 0;
}
int nn_bo_import(nn_device *d, void *storage, size_t bytes, nn_bo **out)
{ return import_pages(d, storage, bytes, out, 0); }
int nn_bo_import_coherent(nn_device *d, void *storage, size_t bytes, nn_bo **out)
{ return import_pages(d, storage, bytes, out, 1); }
int nn_bo_import_dmabuf(nn_device *d, int fd, size_t bytes, nn_bo **out)
{
    if (!out) return EINVAL;
    *out = NULL;
    if (!d || fd < 0 || !bytes || bytes % d->page) return EINVAL;
    if (d->chip != 0x06050002) return ENOTSUP;
    struct stat st;
    if (fstat(fd, &st)) return errno;
    if (st.st_size <= 0 || (uint64_t)st.st_size != bytes) return EINVAL;
    nn_bo *b = calloc(1, sizeof(*b));
    if (!b) return ENOMEM;
    struct kgsl_gpuobj_import_dma_buf buffer = {.fd = fd};
    uintptr_t private_address = (uintptr_t)&buffer;
#if defined(__aarch64__)
    private_address &= UINT64_C(0x00ffffffffffffff);
#endif
    /* DMABUF uses the fd structure's size. ADDR instead uses mapping bytes.
     * Cache mode belongs to the exporter; do not force anonymous-memory WB. */
    struct kgsl_gpuobj_import req = {.priv = private_address, .priv_len = sizeof(buffer),
        .type = KGSL_USER_MEM_TYPE_DMABUF};
    if (ioctl(d->fd, IOCTL_KGSL_GPUOBJ_IMPORT, &req)) {
        /* The borrowed fd may already have a kernel reference despite an
         * unsuccessful mutation. Retire that uncertainty with the device fd. */
        return retain_failed_import(d, b, 0, errno);
    }
    struct kgsl_gpumem_get_info info = {.id = req.id};
    int e = read_ioctl(d->fd, IOCTL_KGSL_GPUMEM_GET_INFO, &info);
    if (e || !req.id || info.id != req.id || !info.gpuaddr || info.gpuaddr % d->page ||
        info.size != bytes || ((req.flags | info.flags) & (KGSL_MEMFLAGS_GPUREADONLY | KGSL_MEMFLAGS_SECURE))) {
        return reject_import(d, b, req.id, e ? e : ENOTSUP);
    }
    b->dev = d; b->id = info.id; b->flags = info.flags; b->address = info.gpuaddr;
    b->size = bytes; b->imported = 2;
    d->bos++; *out = b; return 0;
}
int nn_bo_destroy(nn_bo *b)
{
    if (b->busy) return EBUSY;
    if (b->imported == 3) {
        struct kgsl_gpuobj_free release = {.id = b->id};
        if (ioctl(b->dev->fd, IOCTL_KGSL_GPUOBJ_FREE, &release)) return errno;
        b->dev->bos--; free(b); return 0;
    }
    if (b->map && !b->imported) {
        if (munmap(b->map, b->mapsize)) return errno;
        b->map = NULL;
    }
    /* Imported maps are borrowed. A failed release must leave the caller's
     * still-live pool map intact for diagnostics and explicit teardown retry. */
    struct kgsl_sharedmem_free req = {.gpuaddr = b->address};
    if (ioctl(b->dev->fd, IOCTL_KGSL_SHAREDMEM_FREE, &req)) return errno;
    b->dev->bos--; free(b); return 0;
}
void *nn_bo_map(const nn_bo *b) { return b->map; }
uint64_t nn_bo_address(const nn_bo *b) { return b->address; }
size_t nn_bo_size(const nn_bo *b) { return b->size; }
uint32_t nn_bo_flags(const nn_bo *b) { return b->flags; }
int nn_bo_cache(nn_bo *b, size_t offset, size_t size, int from_gpu)
{
    if (offset > b->size || size > b->size - offset) return EINVAL;
    if (!size) return 0;
    if (b->imported == 2) return ENOTSUP; // External allocator owns CPU cache/lock operations.
    atomic_thread_fence(memory_order_seq_cst);
    if (!b->imported || (b->flags & KGSL_MEMFLAGS_IOCOHERENT)) return 0; // NC or hardware-coherent CPU mapping.
    struct kgsl_gpumem_sync_cache req = {.id = b->id, .offset = offset, .length = size,
        .op = KGSL_GPUMEM_CACHE_RANGE | (from_gpu ? KGSL_GPUMEM_CACHE_FROM_GPU : KGSL_GPUMEM_CACHE_TO_GPU)};
    int e = ioctl(b->dev->fd, IOCTL_KGSL_GPUMEM_SYNC_CACHE, &req) ? errno : 0;
    atomic_thread_fence(memory_order_seq_cst); return e;
}
int nn_queue_create(nn_device *d, nn_queue **out)
{
    *out = NULL;
    nn_queue *q = calloc(1, sizeof(*q)); if (!q) return ENOMEM;
    struct kgsl_drawctxt_create req = {.flags = KGSL_CONTEXT_SAVE_GMEM | KGSL_CONTEXT_NO_GMEM_ALLOC |
        KGSL_CONTEXT_PREAMBLE | KGSL_CONTEXT_NO_FAULT_TOLERANCE};
    if (ioctl(d->fd, IOCTL_KGSL_DRAWCTXT_CREATE, &req)) { int e = errno; free(q); return e; }
    q->dev = d; q->id = req.drawctxt_id; d->queues++; *out = q; return 0;
}
int nn_queue_destroy(nn_queue *q)
{
    if (q->pending || q->unknown) return EBUSY;
    struct kgsl_drawctxt_destroy req = {.drawctxt_id = q->id};
    if (ioctl(q->dev->fd, IOCTL_KGSL_DRAWCTXT_DESTROY, &req)) return errno;
    q->dev->queues--; free(q); return 0;
}
int nn_queue_submit_waits(nn_queue *q, nn_bo *ib, size_t offset, size_t bytes,
                    nn_bo *const *resources, size_t count,
                    const struct nn_queue_dependency *deps, size_t dep_count, uint32_t *timestamp)
{
    if (!q || !ib || !timestamp || (count && !resources)) return EINVAL;
    if (q->unknown) return EBUSY;
    if (dep_count>16 || (dep_count&&!deps)) return EINVAL;
    struct kgsl_command_syncpoint syncs[16];
    struct kgsl_cmd_syncpoint_timestamp points[16];
    for (size_t i=0;i<dep_count;i++) {
        if (!deps[i].queue || deps[i].queue->dev!=q->dev || deps[i].queue->unknown ||
            !deps[i].queue->submitted || (int32_t)(deps[i].queue->timestamp-deps[i].timestamp)<0) return EINVAL;
        points[i]=(struct kgsl_cmd_syncpoint_timestamp){deps[i].queue->id,deps[i].timestamp};
        syncs[i]=(struct kgsl_command_syncpoint){.priv=(uintptr_t)&points[i],.size=sizeof(points[i]),
            .type=KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP};
    }
    if (ib->dev != q->dev || offset % 4 || !bytes || bytes % 4 ||
        offset > ib->size || bytes > ib->size - offset || count > 65535) return EINVAL;
    for (size_t i = 0; i < count; i++) if (!resources[i] || resources[i]->dev != q->dev) return EINVAL;
    struct pending *p = calloc(1, sizeof(*p));
    struct kgsl_command_object *objects = calloc(count + 1, sizeof(*objects));
    if (!p || !objects) { free(p); free(objects); return ENOMEM; }
    p->bos = calloc(count + 1, sizeof(*p->bos));
    if (!p->bos) { free(p); free(objects); return ENOMEM; }
    p->count = count + 1; p->bos[0] = ib;
    for (size_t i = 0; i < count; i++) p->bos[i + 1] = resources[i];
    for (size_t i = 0; i <= count; i++) {
        nn_bo *b = p->bos[i]; b->busy++;
        objects[i] = (struct kgsl_command_object){.gpuaddr = b->address, .size = b->size,
            .id = b->id, .flags = KGSL_OBJLIST_MEMOBJ};
    }
    struct kgsl_command_object command = {.gpuaddr = ib->address + offset,
        .size = bytes, .id = ib->id, .flags = KGSL_CMDLIST_IB};
    struct kgsl_gpu_command req = {.flags = KGSL_CMDBATCH_SUBMIT_IB_LIST,
        .cmdlist = (uintptr_t)&command, .cmdsize = sizeof(command), .numcmds = 1,
        .objlist = (uintptr_t)objects, .objsize = sizeof(*objects), .numobjs = count + 1,
        .context_id = q->id, .synclist=dep_count?(uintptr_t)syncs:0,
        .syncsize=dep_count?sizeof(*syncs):0, .numsyncs=(unsigned)dep_count};
    p->next = q->pending; q->pending = p;
    atomic_thread_fence(memory_order_seq_cst);
    int e = ioctl(q->dev->fd, IOCTL_KGSL_GPU_COMMAND, &req) ? errno : 0;
    free(objects);
    if (e) { q->unknown = 1; return e; } // retain every reference; no automatic resubmit
    p->timestamp=req.timestamp; q->submitted=1;
    q->timestamp = req.timestamp; *timestamp = req.timestamp; return 0;
}
int nn_queue_submit(nn_queue *q, nn_bo *ib, size_t offset, size_t bytes,
                    nn_bo *const *resources, size_t count, uint32_t *timestamp)
{ return nn_queue_submit_waits(q,ib,offset,bytes,resources,count,NULL,0,timestamp); }
static void retire_timestamps(nn_queue *q,uint32_t retired)
{
    struct pending **link=&q->pending;
    while (*link) {
        struct pending *p=*link;
        if ((int32_t)(retired-p->timestamp)<0) { link=&p->next;continue; }
        *link=p->next;
        for (size_t i=0;i<p->count;i++)p->bos[i]->busy--;
        free(p->bos);free(p);
    }
    atomic_thread_fence(memory_order_seq_cst);
}
int nn_queue_timestamp_status(nn_queue *q,uint32_t timestamp,int *completed)
{
    if (!q || !completed) return EINVAL;
    *completed=0;
    if (q->unknown) return EBUSY;
    if (!q->submitted) { if(timestamp)return EINVAL;*completed=1;return 0; }
    if ((int32_t)(q->timestamp-timestamp)<0) return EINVAL;
    struct kgsl_cmdstream_readtimestamp_ctxtid req={.context_id=q->id,.type=KGSL_TIMESTAMP_RETIRED};
    int e=read_ioctl(q->dev->fd,IOCTL_KGSL_CMDSTREAM_READTIMESTAMP_CTXTID,&req);
    if(e)return e;
    retire_timestamps(q,req.timestamp);
    *completed=(int32_t)(req.timestamp-timestamp)>=0;return 0;
}
int nn_queue_wait_timestamp(nn_queue *q,uint32_t timestamp,uint32_t timeout)
{
    int complete=0,e=nn_queue_timestamp_status(q,timestamp,&complete);
    if(e || complete)return e;
    if(!timeout)return ETIMEDOUT;
    struct kgsl_device_waittimestamp_ctxtid req={.context_id=q->id,.timestamp=timestamp,.timeout=timeout};
    e=read_ioctl(q->dev->fd,IOCTL_KGSL_DEVICE_WAITTIMESTAMP_CTXTID,&req);
    if(e)return e;
    /* Successful kernel wait proves this timestamp. No later pending BO is
     * retired merely because an older fence completed. */
    retire_timestamps(q,timestamp);return 0;
}
int nn_queue_faults(nn_queue *q,int *faults)
{
    if(!q || !faults)return EINVAL;
    *faults=0;
    if(q->unknown)return EBUSY;
    struct kgsl_context_property_fault value={0};
    struct kgsl_context_property context={.data=(uintptr_t)&value,.size=sizeof(value),
        .type=KGSL_CONTEXT_PROP_FAULTS,.contextid=q->id};
    struct kgsl_device_getproperty req={.type=KGSL_PROP_CONTEXT_PROPERTY,.value=&context,.sizebytes=sizeof(context)};
    int e=read_ioctl(q->dev->fd,IOCTL_KGSL_DEVICE_GETPROPERTY,&req);if(e)return e;
    if(value.faults<0)return EPROTO;
    *faults=value.faults;return 0;
}
int nn_queue_flush(nn_queue *q)
{
    if(!q)return EINVAL;
    if(q->unknown)return EBUSY;
    /* Submit is eager: every accepted batch already passed GPU_COMMAND.
     * There is no unsubmitted shadow queue to pretend to drain here. */
    atomic_thread_fence(memory_order_seq_cst);return 0;
}
int nn_queue_finish(nn_queue *q,uint32_t timeout)
{ if(!q)return EINVAL;return nn_queue_wait_timestamp(q,q->timestamp,timeout); }
uint32_t nn_queue_timestamp(const nn_queue *q) { return q->timestamp; }

int nn_bo_idle(const nn_bo *b) { return !b ? EINVAL : b->busy ? EBUSY : 0; }
