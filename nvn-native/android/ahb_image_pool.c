/* SPDX-License-Identifier: MIT */
#include "ahb_image_pool.h"
#include <errno.h>
#include <string.h>
static int release_owner(void *owner) { return nn_ahb_destroy(owner); }
int nn_nvn_ahb_image_pool_create(NVNdevice *device, NVNmemoryPool *key,
                                 struct nn_ahb_image_pool_report *report)
{
    if (!report) return EINVAL;
    memset(report,0,sizeof(*report)); report->fd_index = -1;
    if (!device || !key) return EINVAL;
    nn_ahb *owner = NULL;
    int error = nn_ahb_create(&owner), locked = 0;
    if (error) return error;
    const AHardwareBuffer_Desc *actual = nn_ahb_description(owner);
    if (!actual || actual->width != 64 || actual->height != 64 || actual->layers != 1 ||
        actual->format != AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM || actual->usage != NN_AHB_USAGE) error = ENOTSUP;
    struct nn_ahb_view view;
    if (!error) { error = nn_ahb_lock(owner,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,&view); locked = !error; }
    if (!error && (view.bytes_per_pixel != 4 || view.bytes_per_stride != 256 ||
        !view.mapping.found || !view.mapping.shared || view.mapping.matches != 1 ||
        strcmp(view.mapping.permissions,"rw-s") || view.mapping.payload_bytes != 16384)) error = ENOTSUP;
    if (locked) { int e = nn_ahb_unlock(owner); if (!error) error = e; }
    struct nn_ahb_pixel pixel;
    if (!error) error = nn_ahb_get_pixel(owner,&pixel);
    if (!error) {
        if (pixel.bytes > SIZE_MAX || pixel.offset > SIZE_MAX) error = EOVERFLOW;
        else {
            struct nn_external_image_desc desc = {pixel.fd,(size_t)pixel.bytes,(size_t)pixel.offset,
                16384,64,64,256,1,NN_EXTERNAL_RGBA8,819,nn_ahb_buffer(owner),owner,release_owner};
            error = nn_nvn_import_image_pool(device,key,&desc);
            if (!error) {
                report->transferred = 1; report->fd_index = pixel.fd_index;
                report->pixel_offset = pixel.offset; report->fd_bytes = pixel.bytes;
                report->borrowed_owner = owner; return 0;
            }
        }
    }
    int cleanup = nn_ahb_destroy(owner);
    if (cleanup) { report->resources_retained = 1; report->retained_owner = owner; }
    return error ? error : cleanup;
}
