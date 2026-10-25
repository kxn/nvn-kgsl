/* SPDX-License-Identifier: MIT */
#include "nvn_native/display_shape.h"
#include "window_platform.h"
#include "present.h"
#include "ahb.h"
#include <errno.h>
static void window_acquire(void *p){ANativeWindow_acquire(p);}
static void window_release(void *p){ANativeWindow_release(p);}
static int validate(const struct nn_window_slot *s)
{
    AHardwareBuffer_Desc d={0};AHardwareBuffer_describe(s->buffer,&d);
    struct nn_display_rgba8_shape shape;
    return d.width!=s->width || d.height!=s->height || d.layers!=1 || d.stride>UINT32_MAX/4 ||
        d.stride*4!=s->pitch || d.format!=AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM ||
        d.usage!=NN_AHB_USAGE || s->usage!=NN_AHB_USAGE || s->format!=1 ||
        nn_display_rgba8_shape(d.width,d.height,d.stride*4,&shape) ||
        s->payload_bytes!=shape.storage_bytes?ENOTSUP:0;
}
static void buffer_acquire(void *p){AHardwareBuffer_acquire(p);}
static void buffer_release(void *p){AHardwareBuffer_release(p);}
static int create(void *w,void **out)
{
    nn_present *p=NULL;int e=nn_present_create(w,&p);*out=p;return e;
}
static int show(void *p,void *b){return nn_present_show(p,b);}
static int show_interval(void *p,void *b,int interval){return nn_present_show_interval(p,b,interval);}
static void wake(void *p){nn_present_wake(p);}
static int detach(void *p){return nn_present_detach(p);}
static int destroy(void *p){return nn_present_destroy(p);}
static int stats(void *p,struct nn_window_platform_stats *out)
{
    struct nn_present_stats value;int e=nn_present_get_stats(p,&value);if(e)return e;
    *out=(struct nn_window_platform_stats){value.show_completed,value.detach_completed,value.completion_callbacks,
        value.present_fences_waited,value.release_fences_waited};return 0;
}
const struct nn_window_platform *nn_window_platform_get(void)
{
    static const struct nn_window_platform ops={window_acquire,window_release,validate,buffer_acquire,buffer_release,
        create,show,detach,destroy,stats,nn_present_max_interval,show_interval,wake};return &ops;
}
