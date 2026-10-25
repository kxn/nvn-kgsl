/* SPDX-License-Identifier: MIT */
#include "nvn_native/display_shape.h"
#include "nvn_native/external_image.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
struct nn_external_image {
    struct nn_external_image_desc desc;
    nn_bo *bo;
    unsigned pins;
    uint64_t backing_inode;
    uint32_t backing_dev_major, backing_dev_minor;
    int texture, poisoned;
};
int nn_external_image_import(nn_device *device, const struct nn_external_image_desc *desc,
                             nn_external_image **out)
{
    if (!out) return EINVAL;
    *out = NULL;
    if (!device || !desc || desc->fd < 0 || !desc->owner_cookie || !desc->release_owner ||
        !desc->display_buffer) return EINVAL;
    struct nn_display_rgba8_shape shape;
    if (nn_device_chip(device) != 0x06050002 || desc->layers != 1 || desc->format != NN_EXTERNAL_RGBA8 ||
        desc->usage != 819 || nn_display_rgba8_shape(desc->width,desc->height,desc->pitch,&shape) ||
        desc->payload_bytes != shape.storage_bytes ||
        nn_display_rgba8_span(&shape,desc->pixel_offset,desc->fd_bytes)) return ENOTSUP;
    struct stat backing;
    if (fstat(desc->fd, &backing)) return errno;
    if (backing.st_size <= 0 || (uint64_t)backing.st_size != desc->fd_bytes || !backing.st_ino) return ENOTSUP;
    nn_external_image *image = calloc(1,sizeof(*image));
    if (!image) return ENOMEM;
    /* KGSL import already verifies fstat exact size, nonzero page-aligned GPUVA,
     * writable/nonsecure flags and NULL CPU map. Its successful BO contract is
     * used here; no undocumented allocator/native-handle fields are consumed. */
    int error = nn_bo_import_dmabuf(device, desc->fd, desc->fd_bytes, &image->bo);
    if (error) { free(image); return error; }
    image->desc = *desc;
    image->backing_inode = backing.st_ino;
    image->backing_dev_major = major(backing.st_dev);
    image->backing_dev_minor = minor(backing.st_dev);
    *out = image;
    return 0;
}
int nn_external_image_live(const nn_external_image *image)
{ return image && image->bo && !image->poisoned; }
int nn_external_image_view(nn_external_image *image, struct nn_external_image_view *out)
{
    if (!out) return EINVAL;
    memset(out,0,sizeof(*out));
    if (!nn_external_image_live(image)) return EBUSY;
    *out = (struct nn_external_image_view){image->bo,image->desc.fd_bytes,image->desc.pixel_offset,
        image->desc.payload_bytes,image->desc.width,image->desc.height,image->desc.pitch,
        image->desc.format,image->backing_inode,image->backing_dev_major,
        image->backing_dev_minor,image->desc.display_buffer};
    return 0;
}
int nn_external_image_claim_texture(nn_external_image *image, size_t offset, size_t bytes)
{
    if (!nn_external_image_live(image)) return EBUSY;
    if (image->texture || image->pins) return EBUSY;
    if (offset != image->desc.pixel_offset || bytes != image->desc.payload_bytes) return EINVAL;
    image->texture = 1; return 0;
}
int nn_external_image_release_texture(nn_external_image *image)
{
    if (!image || !image->texture) return EINVAL;
    image->texture = 0; return 0;
}
int nn_external_image_pin(nn_external_image *image)
{
    if (!nn_external_image_live(image)) return EBUSY;
    if (image->pins == UINT_MAX) return EOVERFLOW;
    ++image->pins; return 0;
}
int nn_external_image_unpin(nn_external_image *image)
{
    if (!image || !image->pins) return EINVAL;
    --image->pins; return 0;
}
int nn_external_image_destroy(nn_external_image *image)
{
    if (!image) return 0;
    if (image->texture || image->pins || image->poisoned) return EBUSY;
    int error = nn_bo_destroy(image->bo);
    if (error) { if (error != EBUSY) image->poisoned = 1; return error; }
    image->bo = NULL;
    error = image->desc.release_owner(image->desc.owner_cookie);
    if (error) { image->poisoned = 1; return error; }
    free(image); return 0;
}
