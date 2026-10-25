/* SPDX-License-Identifier: MIT */
/* Real external_image.c with only KGSL allocation mocked. fstat/dup/file
 * identity are real host syscalls; no DMA-BUF, GPU or Android claim. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/external_image.h"
#include "nvn_native/window_session.h"
#include "window_platform.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
static unsigned checks,imports,destroys,releases,fstat_calls;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);abort();}}while(0)
static int fail_stat,bad_size,zero_inode;
int __real_fstat(int,struct stat *);
int __wrap_fstat(int fd,struct stat *out)
{
    fstat_calls++;if(fail_stat){errno=EIO;return -1;}
    int e=__real_fstat(fd,out);if(!e){if(bad_size)out->st_size++;if(zero_inode)out->st_ino=0;}return e;
}
struct nn_device {int unused;};struct nn_bo {size_t bytes;};
uint32_t nn_device_chip(const nn_device *d){(void)d;return 0x06050002;}
int nn_bo_import_dmabuf(nn_device *d,int fd,size_t bytes,nn_bo **out)
{(void)d;(void)fd;imports++;*out=malloc(sizeof(**out));if(!*out)return ENOMEM;(*out)->bytes=bytes;return 0;}
int nn_bo_destroy(nn_bo *b){destroys++;free(b);return 0;}
static int release_owner(void *p){(void)p;releases++;return 0;}
static int validate(const struct nn_window_slot *s){(void)s;return 0;}
const struct nn_window_platform *nn_window_platform_get(void)
{static const struct nn_window_platform p={.validate_buffer=validate};return &p;}
static int fixture(void)
{
    char path[]="/tmp/nvn-native-identity-XXXXXX";int fd=mkstemp(path);T(fd>=0);T(!unlink(path));T(!ftruncate(fd,65536));return fd;
}
static struct nn_window_slot slot(const struct nn_external_image_view *v,const void *owner)
{
    return (struct nn_window_slot){.buffer=v->display_buffer,.owner=owner,.pixel_offset=v->pixel_offset,.payload_bytes=v->payload_bytes,
        .backing_inode=v->backing_inode,.backing_dev_major=v->backing_dev_major,.backing_dev_minor=v->backing_dev_minor,
        .width=v->width,.height=v->height,.pitch=v->pitch,.format=v->format,.usage=819};
}
int main(void)
{
    struct nn_device device={0};int fd=fixture(),duplicate=dup(fd),other_fd=fixture();T(duplicate>=0);
    struct stat actual,other;T(!__real_fstat(fd,&actual));T(!__real_fstat(other_fd,&other));
    int owners[4]={0};struct nn_external_image_desc desc={fd,65536,4096,16384,64,64,256,1,NN_EXTERNAL_RGBA8,819,owners,owners,release_owner};
    nn_external_image *images[4]={0};
    fail_stat=1;T(nn_external_image_import(&device,&desc,images)==EIO&&!images[0]);T(!imports&&!releases);fail_stat=0;
    bad_size=1;T(nn_external_image_import(&device,&desc,images)==ENOTSUP&&!images[0]);T(!imports&&!releases);bad_size=0;
    zero_inode=1;T(nn_external_image_import(&device,&desc,images)==ENOTSUP&&!images[0]);T(!imports&&!releases);zero_inode=0;
    T(!nn_external_image_import(&device,&desc,images));
    desc.fd=duplicate;desc.owner_cookie=owners+1;desc.display_buffer=owners+1;
    T(!nn_external_image_import(&device,&desc,images+1));
    desc.pixel_offset=32768;desc.owner_cookie=owners+2;desc.display_buffer=owners+2;
    T(!nn_external_image_import(&device,&desc,images+2));
    desc.fd=other_fd;desc.pixel_offset=4096;desc.owner_cookie=owners+3;desc.display_buffer=owners+3;
    T(!nn_external_image_import(&device,&desc,images+3));
    struct nn_external_image_view views[4];for(unsigned i=0;i<4;i++)T(!nn_external_image_view(images[i],views+i));
    T(views[0].backing_inode==(uint64_t)actual.st_ino);T(views[0].backing_dev_major==major(actual.st_dev));
    T(views[0].backing_dev_minor==minor(actual.st_dev));
    T(views[0].backing_inode==views[1].backing_inode&&views[0].backing_dev_major==views[1].backing_dev_major&&views[0].backing_dev_minor==views[1].backing_dev_minor);
    T(views[3].backing_inode==(uint64_t)other.st_ino&&views[3].backing_inode!=views[0].backing_inode);
    struct nn_window_slot slots[2]={slot(views,owners),slot(views+1,owners+1)};nn_window_session *session=(void *)1;int token;
    T(nn_window_session_create(&token,slots,2,&session)==EEXIST&&!session);
    slots[1]=slot(views+2,owners+2);T(nn_window_session_create(&token,slots,2,&session)==EAGAIN&&!session);
    slots[1]=slot(views+3,owners+3);T(nn_window_session_create(&token,slots,2,&session)==EAGAIN&&!session);
    for(unsigned i=0;i<4;i++)T(!nn_external_image_destroy(images[i]));
    T(destroys==4&&releases==4&&imports==4);
    T(!close(fd));T(!close(duplicate));T(!close(other_fd));
    printf("{\"scope\":\"host fstat and ownership only\",\"checks\":%u,\"fstat_calls\":%u,\"dup_identity_rejected_by_session\":true,\"gpu_executes\":0}\n",checks,fstat_calls);return 0;
}
