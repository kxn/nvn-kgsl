#ifndef NN_NVN_GRAPHICS_SOURCE_CACHE
#define NN_NVN_GRAPHICS_SOURCE_CACHE 0
#endif
#if NN_NVN_GRAPHICS_SOURCE_CACHE != 0 && NN_NVN_GRAPHICS_SOURCE_CACHE != 1
#error "NN_NVN_GRAPHICS_SOURCE_CACHE must be explicit0/1"
#endif
#if NN_NVN_GRAPHICS_SOURCE_CACHE
#include "nvn_native/resource_access.h"
/* SPDX-License-Identifier: MIT. Included only by nvn.c after object/range helpers. */
int nn_nvn_register_pipeline_archive(NVNdevice *key,const struct nn_pipeline_archive *archive,
                                   nn_nvn_pipeline_input_reader reader,void *user)
{
    struct object *device=find(key,DEVICE);if(!device)return nn_nvn_error();
    int e=nn_pipeline_archive_validate(archive);if(e)return e;
    if(!reader)return EINVAL;
    if(archive->platform->chip!=nn_device_chip(device->gpu)||
       archive->platform->gmem_bytes!=nn_device_gmem_bytes(device->gpu))return ENOTSUP;
    if(device->pipeline_archive)return EEXIST;
    for(struct object *o=objects;o;o=o->next)if(o->kind==PROGRAM&&o->device==device)return EBUSY;
    /* Reuse the existing full stage envelope validator rather than accepting
     * only query-table shape. Duplicate identical variants are ambiguous. */
    for(size_t i=0;i<archive->stages->stage_count;i++) {
        const struct nn_offline_graphics_stage *s=archive->stages->stages+i,*matched=NULL;
        e=nn_offline_graphics_find_stage(archive->stages,s->stage,s->source,s->source_bytes,
            s->control,s->control_bytes,s->specialization,&matched);
        if(e)return e;
        if(matched!=s)return EINVAL;
    }
    device->pipeline_archive=archive;device->pipeline_reader=reader;device->pipeline_reader_user=user;return 0;
}
static int pipeline_read(struct object *device,void *out,const void *source,size_t bytes)
{
    if(!source||!out||!bytes||bytes>1048576||!device->pipeline_reader)return EINVAL;
    if(bytes>UINTPTR_MAX-(uintptr_t)source)return EOVERFLOW;
    int saved=errno;long n=device->pipeline_reader(device->pipeline_reader_user,out,source,bytes);
    int e=n<0?(errno?errno:EFAULT):(uint64_t)n>bytes?EOVERFLOW:(size_t)n!=bytes?EIO:0;
    errno=saved;return e;
}
static int pipeline_prepare_program(struct object *program,const NVNshaderData *caller,
                                    struct nn_pipeline_source_program *prepared)
{
    memset(prepared,0,sizeof(*prepared));struct object *device=program->device;
    const struct nn_pipeline_archive *archive=device->pipeline_archive;
    if(!archive)return ENOTSUP;
    /* Never dereference the guest shader-data array or control pointers. */
    NVNshaderData shaders[2];int e=pipeline_read(device,shaders,caller,sizeof(shaders));if(e)return e;
    const struct nn_offline_graphics_stage *identity[2]={0};
    for(unsigned input=0;input<2;input++) {
        if(!shaders[input].control)return EINVAL;
        for(size_t i=0;i<archive->stages->stage_count;i++) {
            const struct nn_offline_graphics_stage *s=archive->stages->stages+i;
            if(s->stage>1||!s->source||!s->control||!s->code_bytes||s->source_bytes<s->code_bytes||
               s->source_bytes>1048576||(s->control_bytes!=0x880&&s->control_bytes!=0x898))return EINVAL;
            struct object *pool=address_pool(device,shaders[input].data,s->source_bytes);
            if(!pool)continue;
            size_t offset=shaders[input].data-nn_bo_address(pool->bo);
            if(gpu_pool_access(pool,offset,s->source_bytes,"nvnProgramSetShaders"))return ENOTSUP;
            if(pool->external||(pool->flags&NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS)||!nn_bo_map(pool->bo))return ENOTSUP;
            /* Input read cannot race an unfinished GPU producer. CPU-authored
             * bytes need no destructive FROM_GPU invalidate before copying. */
            if((e=nn_bo_idle(pool->bo)))return e;
            unsigned char *source=malloc(s->source_bytes),*control=malloc(s->control_bytes);
            if(!source||!control){free(source);free(control);return ENOMEM;}
            e=pipeline_read(device,source,(const char *)nn_bo_map(pool->bo)+offset,s->source_bytes);
            int matches=0;
            if(!e&&!memcmp(source,s->source,s->source_bytes)) {
                e=pipeline_read(device,control,shaders[input].control,s->control_bytes);
                if(!e)matches=!memcmp(control,s->control,s->control_bytes);
            }
            free(source);free(control);if(e)return e;if(!matches)continue;
            const struct nn_offline_graphics_stage *old=identity[input];
            if(old&&(old->stage!=s->stage||old->code_bytes!=s->code_bytes||old->source_bytes!=s->source_bytes||
                old->control_bytes!=s->control_bytes||memcmp(old->source,s->source,s->source_bytes)||
                memcmp(old->control,s->control,s->control_bytes)))return EEXIST;
            if(!old)identity[input]=s;
        }
        if(!identity[input])return ENOTSUP;
    }
    struct nn_pipeline_source_input inputs[2];
    for(unsigned i=0;i<2;i++)inputs[i]=(struct nn_pipeline_source_input){identity[i]->stage,
        identity[i]->source,identity[i]->source_bytes,identity[i]->control,identity[i]->control_bytes};
    return nn_pipeline_source_prepare(archive,inputs,2,prepared);
}
#include "nvn_shader_sources.inc"
#else
int nn_nvn_register_pipeline_archive(NVNdevice *key,const struct nn_pipeline_archive *archive,nn_nvn_pipeline_input_reader reader,void *user)
{(void)key;(void)archive;(void)reader;(void)user;return ENOTSUP;}
int nn_nvn_register_shader_source_catalogue(NVNdevice*key,const struct nn_shader_source_archive*archive)
{(void)key;(void)archive;return ENOTSUP;}
#endif

int nn_nvn_register_typed_pipeline_archive(NVNdevice *key,const struct nn_pipeline_typed_archive *typed,
 nn_nvn_pipeline_input_reader reader,void *user)
{
#if NN_NVN_GRAPHICS_SOURCE_CACHE
 struct object *device=find(key,DEVICE);if(!device)return nn_nvn_error();
 int e=nn_pipeline_typed_archive_validate(typed);if(e)return e;
 e=nn_nvn_register_pipeline_archive(key,typed->archive,reader,user);if(e)return e;
 device->pipeline_types=typed;return 0;
#else
 (void)key;(void)typed;(void)reader;(void)user;return ENOTSUP;
#endif
}

int nn_nvn_register_pipeline_depth_archive(NVNdevice*key,const struct nn_pipeline_depth_archive*depth)
{
#if NN_NVN_GRAPHICS_SOURCE_CACHE && NN_NVN_DEPTH_TARGETS
 struct object*d=find(key,DEVICE);if(!d)return nn_nvn_error();
 int e=nn_pipeline_depth_archive_validate(depth);if(e)return e;
 if(depth->typed!=d->pipeline_types||depth->typed->archive!=d->pipeline_archive)return EINVAL;
 if(d->pipeline_depth)return EEXIST;
 for(struct object*o=objects;o;o=o->next)if(o->kind==PROGRAM&&o->device==d)return EBUSY;
 d->pipeline_depth=depth;return 0;
#else
 (void)key;(void)depth;return ENOTSUP;
#endif
}

int nn_nvn_register_pipeline_storage_archive(NVNdevice*key,const struct nn_pipeline_storage_archive*a)
{
#if NN_NVN_GRAPHICS_SOURCE_CACHE && NN_NVN_GRAPHICS_STORAGE
 struct object*d=find(key,DEVICE);if(!d)return nn_nvn_error();
 int e=nn_pipeline_storage_archive_validate(a);if(e)return e;
 if(a->typed!=d->pipeline_types||a->typed->archive!=d->pipeline_archive)return EINVAL;
 if(d->pipeline_storage)return EEXIST;
 for(struct object*o=objects;o;o=o->next)if(o->kind==PROGRAM&&o->device==d)return EBUSY;
 d->pipeline_storage=a;return 0;
#else
 (void)key;(void)a;return ENOTSUP;
#endif
}
