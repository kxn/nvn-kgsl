/* HOST RAM fixtures only. This is a fail-closed link boundary, not a platform
 * session implementation or proof of Window/GPU behavior. Android must link
 * the actual platform implementation; dedicated fixtures opt out and provide
 * their own strong session provider in another translation unit. */
#if defined(__ANDROID__) && !defined(NN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND)
#error "Never compile the HOST ONLY Window session failure backend for Android"
#endif
#include "nvn_native/window_session.h"
#include <errno.h>
#include <string.h>
#ifndef NN_HOST_TEST_REAL_WINDOW_SESSION
#define HOST_WINDOW_WEAK __attribute__((weak))
HOST_WINDOW_WEAK int nn_window_session_create(const void *token,const struct nn_window_slot *slots,
                                               uint32_t count,nn_window_session **out)
{ (void)token;(void)slots;(void)count;if(out)*out=NULL;return ENOTSUP; }
HOST_WINDOW_WEAK int nn_window_session_acquire(nn_window_session *session,uint32_t timeout,
                                                struct nn_window_ticket *out)
{ (void)session;(void)timeout;if(out)memset(out,0,sizeof(*out));return ENOTSUP; }
HOST_WINDOW_WEAK int nn_window_session_check_acquired(nn_window_session *session,const struct nn_window_ticket *ticket)
{ (void)session;(void)ticket;return ENOTSUP; }
HOST_WINDOW_WEAK int nn_window_session_enqueue_ready(nn_window_session *session,const struct nn_window_ticket *ticket)
{ (void)session;(void)ticket;return ENOTSUP; }
HOST_WINDOW_WEAK int nn_window_session_get_stats(nn_window_session *session,struct nn_window_session_stats *out)
{ (void)session;if(out)memset(out,0,sizeof(*out));return ENOTSUP; }
HOST_WINDOW_WEAK int nn_window_session_retire(nn_window_session *session,uint32_t timeout)
{ (void)session;(void)timeout;return ENOTSUP; }
HOST_WINDOW_WEAK int nn_window_session_destroy(nn_window_session *session)
{ (void)session;return ENOTSUP; }
HOST_WINDOW_WEAK int nn_window_session_interval_supported(int interval)
{ return interval<0||interval>5?EINVAL:ENOTSUP; }
HOST_WINDOW_WEAK int nn_window_session_create_interval(const void *token,const struct nn_window_slot *slots,
    uint32_t count,int interval,nn_window_session **out)
{ (void)token;(void)slots;(void)count;(void)interval;if(out)*out=NULL;return ENOTSUP; }
HOST_WINDOW_WEAK int nn_window_session_set_present_interval(nn_window_session *s,int interval)
{ (void)s;(void)interval;return ENOTSUP; }
#undef HOST_WINDOW_WEAK
#endif

#ifndef NN_HOST_TEST_REAL_WINDOW_DISPLAY
#include "nvn_native/window_display.h"
__attribute__((weak)) int nn_window_display_create(nn_device *d,uint32_t w,uint32_t h,nn_window_display **out)
{ (void)d;(void)w;(void)h;if(out)*out=NULL;return ENOTSUP; }
__attribute__((weak)) int nn_window_display_view(nn_window_display *d,struct nn_external_image_view *out)
{ (void)d;if(out)*out=(struct nn_external_image_view){0};return ENOTSUP; }
__attribute__((weak)) int nn_window_display_destroy(nn_window_display *d)
{ return d?ENOTSUP:0; }
__attribute__((weak)) int nn_window_display_pixel(nn_window_display *d,struct nn_ahb_pixel *p)
{ (void)d;(void)p;return ENOTSUP; }
__attribute__((weak)) int nn_window_display_read_lock(nn_window_display *d,struct nn_ahb_view *v)
{ (void)d;(void)v;return ENOTSUP; }
__attribute__((weak)) int nn_window_display_read_unlock(nn_window_display *d)
{ (void)d;return ENOTSUP; }
#endif
