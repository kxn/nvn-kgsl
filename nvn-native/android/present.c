/* SPDX-License-Identifier: MIT */
#include "nvn_native/display_shape.h"
#include "present.h"
#include "ahb.h"
#include "present_pacer.h"
#include <stdatomic.h>
#include <android/data_space.h>
#include <android/surface_control.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* API29 OnComplete and previous-release-fence contracts are in the installed
 * android/surface_control.h:140-159/282-317. Android14 surface_control.cpp's
 * stats getter aborts if the requested SC is absent, so enumerate first.
 * This is ownership synchronization, not a frame timer/profiler.
 * Source URLs/SHA: .local/nvn-native/parallel/android-present-plan.json. */
enum { CALLBACK_TIMEOUT_MS = 1500, FENCE_TIMEOUT_MS = 1000, OP_SHOW = 1, OP_DETACH = 2 };
struct nn_present {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    ANativeWindow *window;
    ASurfaceControl *control;
    AHardwareBuffer *current, *incoming; /* explicit refs in addition to SF */
    int active, pending, callback_done, callback_errno, detached;
    int needs_surface, present_fd, release_fd;
    struct nn_present_pacer *pacer;
    _Atomic int closing;
    struct nn_present_stats stats;
};

/* Only the lifecycle worker releases these refs. A late callback after a
 * timeout may close its newly received fds, but never frees its context. */
static void complete(void *context, ASurfaceTransactionStats *stats)
{
    nn_present *p = context;
    ASurfaceControl **surfaces = NULL;
    size_t count = 0;
    ASurfaceTransactionStats_getASurfaceControls(stats, &surfaces, &count);
    int found = 0;
    for (size_t i = 0; i < count; ++i) if (surfaces[i] == p->control) found = 1;
    int error = 0;
    errno = 0;
    int present_fd = ASurfaceTransactionStats_getPresentFenceFd(stats);
    if (present_fd < 0 && errno) error = errno;
    int release_fd = -1;
    if (found) {
        errno = 0;
        release_fd = ASurfaceTransactionStats_getPreviousReleaseFenceFd(stats, p->control);
        if (release_fd < 0 && errno && !error) error = errno;
    }
    if (surfaces) ASurfaceTransactionStats_releaseASurfaceControls(surfaces);
    pthread_mutex_lock(&p->mutex);
    ++p->stats.completion_callbacks;
    if (present_fd >= 0) ++p->stats.present_fences;
    if (release_fd >= 0) ++p->stats.release_fences;
    if (!found) {
        ++p->stats.missing_surface_stats;
        if (p->needs_surface && !error) error = ENODATA;
    }
    if (p->stats.unknown_completion) {
        /* The waiting worker has given up and retained the entire owner. */
        if (present_fd >= 0) (void)close(present_fd);
        if (release_fd >= 0) (void)close(release_fd);
    } else {
        p->present_fd = present_fd; p->release_fd = release_fd;
        p->callback_errno = error;
    }
    p->pending = 0; p->callback_done = 1;
    pthread_cond_signal(&p->condition);
    /* This unlock is the callback's final access to p. A successful waiter
     * can subsequently destroy it without leaving a dangling callback. */
    pthread_mutex_unlock(&p->mutex);
}

static int wait_fence(int fd, int *waited)
{
    if (fd < 0) return 0;
    struct pollfd pollfd = {.fd = fd, .events = POLLIN};
    int n = poll(&pollfd, 1, FENCE_TIMEOUT_MS);
    int error = n < 0 ? errno : !n ? ETIMEDOUT :
        (!(pollfd.revents & POLLIN) || (pollfd.revents & (POLLERR | POLLNVAL))) ? EIO : 0;
    if (!error) *waited = 1;
    if (close(fd) && !error) error = errno;
    return error;
}

/* The owner has at most one callback in flight. On any post-apply error it
 * becomes permanently uncertain, keeping the SC/window/current/incoming refs
 * alive for a late callback and safe process teardown. No automatic retry. */
void nn_present_pacer_set_timeline(ASurfaceTransaction *,int64_t);
static int transact(nn_present *p, int operation, AHardwareBuffer *buffer,int64_t expected,int64_t vsync_id)
{
    pthread_mutex_lock(&p->mutex);
    if(operation==OP_SHOW && atomic_load_explicit(&p->closing,memory_order_acquire)){
        pthread_mutex_unlock(&p->mutex);return ECANCELED;
    }
    if (p->stats.unknown_completion || p->active || p->pending) {
        pthread_mutex_unlock(&p->mutex); return EBUSY;
    }
    if (p->detached) {
        pthread_mutex_unlock(&p->mutex); return operation == OP_DETACH ? 0 : EBUSY;
    }
    int width = ANativeWindow_getWidth(p->window), height = ANativeWindow_getHeight(p->window);
    if (operation == OP_SHOW && (width <= 0 || height <= 0)) {
        p->stats.last_errno = EINVAL;
        pthread_mutex_unlock(&p->mutex); return EINVAL;
    }
    ASurfaceTransaction *transaction = ASurfaceTransaction_create();
    if (!transaction) {
        p->stats.last_errno = ENOMEM;
        pthread_mutex_unlock(&p->mutex); return ENOMEM;
    }
    p->active = p->pending = 1; p->callback_done = p->callback_errno = 0;
    p->present_fd = p->release_fd = -1;
    p->needs_surface = operation == OP_SHOW || p->current != NULL;
    if (operation == OP_SHOW) {
        AHardwareBuffer_acquire(buffer); p->incoming = buffer;
        p->stats.current_buffer_held = 1;
        p->stats.window_width = width; p->stats.window_height = height;
        ++p->stats.show_submitted;
    } else ++p->stats.detach_submitted;
    pthread_mutex_unlock(&p->mutex);

    if (operation == OP_SHOW) {
        AHardwareBuffer_Desc actual = {0};
        AHardwareBuffer_describe(buffer,&actual);
        ARect source = {0, 0, (int32_t)actual.width, (int32_t)actual.height}, destination = {0, 0, width, height};
        if(expected>0){ASurfaceTransaction_setDesiredPresentTime(transaction,expected);
            nn_present_pacer_set_timeline(transaction,vsync_id);}
        ASurfaceTransaction_setGeometry(transaction, p->control, &source, &destination, 0);
        ASurfaceTransaction_setVisibility(transaction, p->control, ASURFACE_TRANSACTION_VISIBILITY_SHOW);
        ASurfaceTransaction_setBufferAlpha(transaction, p->control, 1.0f);
        ASurfaceTransaction_setBufferTransparency(transaction, p->control, ASURFACE_TRANSACTION_TRANSPARENCY_OPAQUE);
        ASurfaceTransaction_setBufferDataSpace(transaction, p->control, ADATASPACE_SRGB);
        /* Native KGSL has already finished; bytes and allocation stay intact. */
        ASurfaceTransaction_setBuffer(transaction, p->control, buffer, -1);
    } else {
        /* setBuffer(NULL) is not a public-contract option in the installed NDK
         * (nonnull). Public reparent(NULL) removes the child from the scene.
         * OnComplete alone is insufficient: require this SC's release stats. */
        ASurfaceTransaction_reparent(transaction, p->control, NULL);
    }
    ASurfaceTransaction_setOnComplete(transaction, p, complete);
    ASurfaceTransaction_apply(transaction);
    ASurfaceTransaction_delete(transaction);

    struct timespec deadline;
    int error = clock_gettime(CLOCK_MONOTONIC, &deadline) ? errno : 0;
    if (!error) {
        deadline.tv_sec += CALLBACK_TIMEOUT_MS / 1000;
        deadline.tv_nsec += (CALLBACK_TIMEOUT_MS % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec; deadline.tv_nsec -= 1000000000L;
        }
    }
    pthread_mutex_lock(&p->mutex);
    while (!error && !p->callback_done)
        error = pthread_cond_timedwait(&p->condition, &p->mutex, &deadline);
    if (!error) error = p->callback_errno;
    int present_fd = p->present_fd, release_fd = p->release_fd;
    p->present_fd = p->release_fd = -1;
    if (error) {
        p->stats.last_errno = error; p->stats.unknown_completion = 1; p->active = 0;
        pthread_mutex_unlock(&p->mutex);
        if (present_fd >= 0) (void)close(present_fd);
        if (release_fd >= 0) (void)close(release_fd);
        return error;
    }
    pthread_mutex_unlock(&p->mutex);
    int present_waited = 0, release_waited = 0;
    error = wait_fence(present_fd, &present_waited);
    if (!error) error = wait_fence(release_fd, &release_waited);
    else if (release_fd >= 0) (void)close(release_fd);
    pthread_mutex_lock(&p->mutex);
    p->stats.present_fences_waited += present_waited;
    p->stats.release_fences_waited += release_waited;
    if (error) {
        p->stats.last_errno = error; p->stats.unknown_completion = 1; p->active = 0;
        pthread_mutex_unlock(&p->mutex); return error;
    }
    AHardwareBuffer *previous = p->current;
    if (operation == OP_SHOW) {
        p->current = p->incoming; p->incoming = NULL;
        ++p->stats.show_completed;
    } else {
        p->current = NULL; p->detached = 1;
        ++p->stats.detach_completed;
    }
    p->stats.current_buffer_held = p->current != NULL;
    p->stats.last_errno = 0; p->active = 0;
    pthread_mutex_unlock(&p->mutex);
    if (previous) AHardwareBuffer_release(previous);
    return 0;
}

int nn_present_create(ANativeWindow *window, nn_present **out)
{
    if (!out) return EINVAL;
    *out = NULL;
    if (!window) return EINVAL;
    nn_present *p = calloc(1, sizeof(*p));
    if (!p) return ENOMEM;
    int error = pthread_mutex_init(&p->mutex, NULL);
    if (error) { free(p); return error; }
    pthread_condattr_t attr;
    error = pthread_condattr_init(&attr);
    if (!error) {
        error = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
        if (!error) error = pthread_cond_init(&p->condition, &attr);
        pthread_condattr_destroy(&attr);
    }
    if (error) { pthread_mutex_destroy(&p->mutex); free(p); return error; }
    ANativeWindow_acquire(window); p->window = window;
    p->control = ASurfaceControl_createFromWindow(window, "nvn-native-direct");
    if (!p->control) {
        ANativeWindow_release(window); pthread_cond_destroy(&p->condition);
        pthread_mutex_destroy(&p->mutex); free(p); return ENOTSUP;
    }
    if(nn_present_pacer_max_interval()==5){
        error=nn_present_pacer_create(&p->pacer);
        if(error){ASurfaceControl_release(p->control);ANativeWindow_release(window);
            pthread_cond_destroy(&p->condition);pthread_mutex_destroy(&p->mutex);free(p);return error;}
    }
    p->present_fd = p->release_fd = -1;
    *out = p;
    return 0;
}

int nn_present_max_interval(void){return nn_present_pacer_max_interval();}
void nn_present_wake(nn_present *p)
{if(p){atomic_store_explicit(&p->closing,1,memory_order_release);nn_present_pacer_wake(p->pacer);}}
int nn_present_show(nn_present *p,AHardwareBuffer *buffer)
{ /* Legacy single-owner helper keeps its existing SC-fence pacing. Window
     uses the explicit interval entry; do not retime unrelated raw probes. */
    return nn_present_show_interval(p,buffer,0);
}
int nn_present_show_interval(nn_present *p, AHardwareBuffer *buffer,int interval)
{
    if (!p || !buffer) return EINVAL;
    AHardwareBuffer_Desc d = {0};
    AHardwareBuffer_describe(buffer, &d);
    struct nn_display_rgba8_shape shape;
    if (d.layers != 1 || d.stride>UINT32_MAX/4 ||
        d.format != AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM || d.usage != NN_AHB_USAGE ||
        nn_display_rgba8_shape(d.width,d.height,d.stride*4,&shape)) return ENOTSUP;
    if(interval<0||interval>5)return EINVAL;
    int64_t expected=0,id=-1;
    if(p->pacer){int e=nn_present_pacer_wait(p->pacer,interval,&expected,&id);if(e)return e;}
    else if(interval>1)return ENOTSUP;
    return transact(p, OP_SHOW, buffer,expected,id);
}
int nn_present_detach(nn_present *p)
{
    return p ? transact(p, OP_DETACH, NULL,0,-1) : EINVAL;
}
int nn_present_destroy(nn_present *p)
{
    if (!p) return 0;
    int error = nn_present_detach(p);
    if (error) return EBUSY;
    error=nn_present_pacer_destroy(p->pacer);if(error)return EBUSY;
    p->pacer=NULL;
    ASurfaceControl_release(p->control);
    ANativeWindow_release(p->window);
    pthread_cond_destroy(&p->condition); pthread_mutex_destroy(&p->mutex);
    free(p);
    return 0;
}
int nn_present_get_stats(nn_present *p, struct nn_present_stats *out)
{
    if (!p || !out) return EINVAL;
    pthread_mutex_lock(&p->mutex); *out = p->stats; pthread_mutex_unlock(&p->mutex);
    return 0;
}
