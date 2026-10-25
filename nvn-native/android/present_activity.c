/* SPDX-License-Identifier: MIT */
#include "present_activity.h"
#include "present.h"
#include "ahb_frame.h"
#include <android/window.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The UI thread owns lifecycle transitions and joins its worker before giving
 * up a window reference. Only the worker touches GPU/compositor resources.
 * This is a bounded qualification caller, not a game renderer or frame loop. */
struct activity_state {
    ANativeActivity *activity;
    ANativeWindow *window;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    pthread_t worker;
    int joinable, running, stop, resumed, error, retained;
    const char *phase, *failure_stage;
    unsigned generation, creates, destroys, acquires, releases, resumes, pauses;
    unsigned frames, sessions, buffers_released, shows, detaches, callbacks;
    unsigned present_fences, release_fences;
    int width, height, screen_x, screen_y;
    char gpu[4096];
};

/* mutex held. Atomic rename prevents partial JSON reads by the ADB runner. */
static void publish(struct activity_state *s)
{
    char path[1024], temporary[1024];
    int n = snprintf(path, sizeof(path), "%s/result.json", s->activity->internalDataPath);
    int m = snprintf(temporary, sizeof(temporary), "%s/result.tmp", s->activity->internalDataPath);
    if (n < 0 || (size_t)n >= sizeof(path) || m < 0 || (size_t)m >= sizeof(temporary)) {
        if (!s->error) s->error = ENAMETOOLONG;
        return;
    }
    FILE *file = fopen(temporary, "w");
    if (!file) { if (!s->error) s->error = errno; return; }
    int passed = !s->error && !s->retained && s->frames && s->sessions == s->frames &&
        s->buffers_released == s->frames && !s->running;
    int result = fprintf(file,
        "{\"schema\":1,\"operation\":\"kgsl_ahb_window\",\"uid\":%u,\"phase\":\"%s\","
        "\"errno\":%d,\"failure_stage\":\"%s\",\"generation\":%u,\"worker_running\":%s,\"ready\":%s,"
        "\"window_created\":%u,\"window_destroyed\":%u,\"window_acquired\":%u,\"window_released\":%u,"
        "\"resumes\":%u,\"pauses\":%u,\"frames\":%u,\"completed_sessions\":%u,\"buffers_released\":%u,"
        "\"show_completed\":%u,\"detach_completed\":%u,\"completion_callbacks\":%u,"
        "\"present_fences_waited\":%u,\"release_fences_waited\":%u,"
        "\"window_width\":%d,\"window_height\":%d,\"window_screen_x\":%d,\"window_screen_y\":%d,\"resources_retained\":%s,"
        "\"cpu_image_copy\":false,\"screen_verified_by_caller\":false,\"gpu\":%s,\"passed\":%s}\n",
        (unsigned)getuid(), s->phase, s->error, s->failure_stage ? s->failure_stage : "none", s->generation, s->running ? "true" : "false",
        !s->error && !strcmp(s->phase, "presented") ? "true" : "false",
        s->creates, s->destroys, s->acquires, s->releases, s->resumes, s->pauses,
        s->frames, s->sessions, s->buffers_released, s->shows, s->detaches, s->callbacks,
        s->present_fences, s->release_fences, s->width, s->height, s->screen_x, s->screen_y,
        s->retained ? "true" : "false", s->gpu[0] ? s->gpu : "null", passed ? "true" : "false");
    int close_error = fclose(file);
    if (result < 0 || close_error || rename(temporary, path)) { if (!s->error) s->error = EIO; }
}

static void *render(void *argument)
{
    struct activity_state *s = argument;
    nn_ahb *buffer = NULL;
    nn_present *present = NULL;
    char gpu[4096] = {0};
    int error = 0, retained = 0;
    const char *stage = "gpu-frame";
    struct nn_present_stats stats = {0};
    if (nn_ahb_gpu_frame(s->activity->internalDataPath, gpu, sizeof(gpu), &buffer)) error = EIO;
    pthread_mutex_lock(&s->mutex);
    memcpy(s->gpu, gpu, sizeof(s->gpu));
    if (error && strstr(gpu, "\"resources_retained\":true")) retained = 1;
    int stopped = s->stop;
    pthread_mutex_unlock(&s->mutex);
    if (!error && !stopped) { stage = "create-surface-control"; error = nn_present_create(s->window, &present); }
    if (!error && present) { stage = "show-buffer"; error = nn_present_show(present, nn_ahb_buffer(buffer)); }
    if (!error && present) {
        error = nn_present_get_stats(present, &stats);
        if (!error) {
            pthread_mutex_lock(&s->mutex);
            ++s->frames;
            s->width = stats.window_width; s->height = stats.window_height;
            s->phase = "presented";
            publish(s);
            while (!s->stop) pthread_cond_wait(&s->changed, &s->mutex);
            s->phase = "retiring";
            publish(s);
            pthread_mutex_unlock(&s->mutex);
        }
    }
    if (present) {
        int e = nn_present_detach(present);
        if (e) { if (!error) { error = e; stage = "detach-buffer"; } retained = 1; }
        (void)nn_present_get_stats(present, &stats);
        if (!retained) {
            e = nn_present_destroy(present);
            if (e) { if (!error) { error = e; stage = "destroy-presenter"; } retained = 1; }
        }
    }
    int released = 0;
    if (buffer && !retained) {
        int e = nn_ahb_destroy(buffer);
        if (e) { if (!error) { error = e; stage = "destroy-buffer"; } retained = 1; }
        else released = 1;
    }
    pthread_mutex_lock(&s->mutex);
    if (error && !s->error) { s->error = error; s->failure_stage = stage; }
    s->retained |= retained;
    if (stats.show_completed) {
        ++s->sessions;
        if (released) ++s->buffers_released;
    }
    s->shows += stats.show_completed; s->detaches += stats.detach_completed;
    s->callbacks += stats.completion_callbacks;
    s->present_fences += stats.present_fences_waited;
    s->release_fences += stats.release_fences_waited;
    s->running = 0;
    s->phase = s->error ? "error" : "retired";
    publish(s);
    pthread_mutex_unlock(&s->mutex);
    return NULL;
}

/* UI thread only; window is immutable until this worker is joined. */
static void start(struct activity_state *s)
{
    pthread_mutex_lock(&s->mutex);
    if (!s->resumed || !s->window || s->joinable || s->error) { pthread_mutex_unlock(&s->mutex); return; }
    s->stop = 0; s->running = 1; s->phase = "drawing"; ++s->generation;
    int e = pthread_create(&s->worker, NULL, render, s);
    if (e) { s->error = e; s->running = 0; s->phase = "error"; }
    else s->joinable = 1;
    publish(s);
    pthread_mutex_unlock(&s->mutex);
}
static void stop(struct activity_state *s)
{
    pthread_mutex_lock(&s->mutex);
    s->stop = 1;
    pthread_cond_signal(&s->changed);
    int join = s->joinable;
    pthread_mutex_unlock(&s->mutex);
    if (join) {
        int e = pthread_join(s->worker, NULL);
        pthread_mutex_lock(&s->mutex);
        if (e && !s->error) s->error = e;
        if (!e) s->joinable = 0;
        publish(s);
        pthread_mutex_unlock(&s->mutex);
    }
}
static void resumed(ANativeActivity *activity)
{
    struct activity_state *s = activity->instance;
    pthread_mutex_lock(&s->mutex); s->resumed = 1; ++s->resumes; publish(s); pthread_mutex_unlock(&s->mutex);
    start(s);
}
static void paused(ANativeActivity *activity)
{
    struct activity_state *s = activity->instance;
    pthread_mutex_lock(&s->mutex); s->resumed = 0; ++s->pauses; publish(s); pthread_mutex_unlock(&s->mutex);
    stop(s);
}
static void window_created(ANativeActivity *activity, ANativeWindow *window)
{
    struct activity_state *s = activity->instance;
    pthread_mutex_lock(&s->mutex);
    if (s->window || !window) s->error = EINVAL;
    else {
        JNIEnv *env = activity->env; /* UI thread, as required by NativeActivity. */
        jclass type = (*env)->GetObjectClass(env, activity->clazz);
        jfieldID x = (*env)->GetFieldID(env, type, "probeSurfaceX", "I");
        jfieldID y = (*env)->GetFieldID(env, type, "probeSurfaceY", "I");
        if (x && y && !(*env)->ExceptionCheck(env)) {
            s->screen_x = (*env)->GetIntField(env, activity->clazz, x);
            s->screen_y = (*env)->GetIntField(env, activity->clazz, y);
        } else { (*env)->ExceptionClear(env); s->error = EINVAL; }
        (*env)->DeleteLocalRef(env, type);
        ANativeWindow_acquire(window); s->window = window; ++s->creates; ++s->acquires;
    }
    publish(s);
    pthread_mutex_unlock(&s->mutex);
    start(s);
}
static void window_destroyed(ANativeActivity *activity, ANativeWindow *window)
{
    struct activity_state *s = activity->instance;
    stop(s);
    pthread_mutex_lock(&s->mutex);
    if (s->window != window || s->joinable) { if (!s->error) s->error = EBUSY; }
    else { ANativeWindow_release(s->window); s->window = NULL; ++s->destroys; ++s->releases; }
    publish(s);
    pthread_mutex_unlock(&s->mutex);
}
static void destroyed(ANativeActivity *activity)
{
    struct activity_state *s = activity->instance;
    stop(s);
    if (s->joinable) return; /* Never free a worker's state after uncertain join. */
    if (s->window) ANativeWindow_release(s->window);
    pthread_cond_destroy(&s->changed); pthread_mutex_destroy(&s->mutex);
    activity->instance = NULL;
    free(s);
}
void nn_present_activity_create(ANativeActivity *activity)
{
    struct activity_state *s = calloc(1, sizeof(*s));
    if (!s) { ANativeActivity_finish(activity); return; }
    s->activity = activity; s->phase = "created";
    int e = pthread_mutex_init(&s->mutex, NULL);
    if (e) { free(s); ANativeActivity_finish(activity); return; }
    e = pthread_cond_init(&s->changed, NULL);
    if (e) { pthread_mutex_destroy(&s->mutex); free(s); ANativeActivity_finish(activity); return; }
    activity->instance = s;
    activity->callbacks->onResume = resumed; activity->callbacks->onPause = paused;
    activity->callbacks->onNativeWindowCreated = window_created;
    activity->callbacks->onNativeWindowDestroyed = window_destroyed;
    activity->callbacks->onDestroy = destroyed;
    ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
    pthread_mutex_lock(&s->mutex); publish(s); pthread_mutex_unlock(&s->mutex);
}
