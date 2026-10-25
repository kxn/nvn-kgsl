/* SPDX-License-Identifier: MIT */
#include "nvn_native/window_display.h"
#include "nvn_native/display_shape.h"
#include "ahb.h"
#include <errno.h>
#include <stdlib.h>
struct nn_window_display { nn_ahb *ahb;nn_external_image *image;unsigned pinned; };
static int release_ahb(void *p){return nn_ahb_destroy(p);}
int nn_window_display_destroy(nn_window_display *owner)
{
    if(!owner)return 0;
    int error=0;
    if(owner->image){
        if(owner->pinned){error=nn_external_image_unpin(owner->image);if(error)return error;owner->pinned=0;}
        error=nn_external_image_destroy(owner->image);if(error)return error;
        owner->image=NULL;owner->ahb=NULL; /* release callback consumed AHB */
    }else if(owner->ahb){error=nn_ahb_destroy(owner->ahb);if(error)return error;owner->ahb=NULL;}
    free(owner);return 0;
}
int nn_window_display_create(nn_device *device,uint32_t width,uint32_t height,nn_window_display **out)
{
    if(!out)return EINVAL;
    *out=NULL;if(!device)return EINVAL;
    nn_window_display *owner=calloc(1,sizeof(*owner));if(!owner)return ENOMEM;
    int error=nn_ahb_create_rgba(width,height,&owner->ahb);
    struct nn_ahb_view view;
    if(!error)error=nn_ahb_lock(owner->ahb,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,&view);
    if(!error)error=nn_ahb_unlock(owner->ahb);
    struct nn_ahb_pixel pixel;
    if(!error)error=nn_ahb_get_pixel(owner->ahb,&pixel);
    if(!error){
        struct nn_display_rgba8_shape shape;
        error=nn_display_rgba8_shape(pixel.width,pixel.height,pixel.pitch,&shape);
        if(!error&&(pixel.width!=width||pixel.height!=height||pixel.payload_bytes!=shape.storage_bytes||
           pixel.bytes>SIZE_MAX||pixel.offset>SIZE_MAX))error=ENOTSUP;
        if(!error){
            struct nn_external_image_desc desc={.fd=pixel.fd,.fd_bytes=(size_t)pixel.bytes,.pixel_offset=(size_t)pixel.offset,.payload_bytes=(size_t)shape.storage_bytes,
                .width=width,.height=height,.pitch=pixel.pitch,.layers=1,.format=NN_EXTERNAL_RGBA8,.usage=NN_AHB_USAGE,
                .display_buffer=nn_ahb_buffer(owner->ahb),.owner_cookie=owner->ahb,.release_owner=release_ahb};
            error=nn_external_image_import(device,&desc,&owner->image);
        }
    }
    if(!error){error=nn_external_image_pin(owner->image);if(!error)owner->pinned=1;}
    if(error){int cleanup=nn_window_display_destroy(owner);if(cleanup)*out=owner;return error;}
    *out=owner;return 0;
}
int nn_window_display_view(nn_window_display *owner,struct nn_external_image_view *out)
{
    if(!out)return EINVAL;
    *out=(struct nn_external_image_view){0};
    if(!owner||!owner->image)return EINVAL;
    return nn_external_image_view(owner->image,out);
}

int nn_window_display_pixel(nn_window_display *o,struct nn_ahb_pixel *p){if(!o||!o->ahb)return EINVAL;return nn_ahb_get_pixel(o->ahb,p);}
int nn_window_display_read_lock(nn_window_display *o,struct nn_ahb_view *v){if(!o||!o->ahb)return EINVAL;return nn_ahb_lock(o->ahb,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,v);}
int nn_window_display_read_unlock(nn_window_display *o){if(!o||!o->ahb)return EINVAL;return nn_ahb_unlock(o->ahb);}
