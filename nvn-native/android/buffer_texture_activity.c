/* SPDX-License-Identifier: MIT. Reuse the detached-owner transport, no Activity lifetime leak. */
#define NN_PUBLIC_COPY_ACTIVITY_FUNCTION nn_buffer_texture_probe_activity_create
#define NN_PUBLIC_COPY_ACTIVITY_PREFIX "buffer-texture"
#define NN_PUBLIC_COPY_ACTIVITY_OPERATION "buffer_texture"
#include "public_copy_activity.c"
