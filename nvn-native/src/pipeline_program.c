/* SPDX-License-Identifier: MIT. Immutable shader/C1 upload only, no GPU Draw. */
#include "nvn_native/pipeline_program.h"
#include "nvn_native/pipeline_native_program.h"
#include "nvn_native/resource_access.h"
#include "nvn_native/pipeline_storage.h"
#ifndef NN_PIPELINE_GRAPHICS_STORAGE
#define NN_PIPELINE_GRAPHICS_STORAGE 0
#endif
#include "nvn_native/depth_native.h"
#ifndef NN_PIPELINE_DEPTH_NATIVE
#define NN_PIPELINE_DEPTH_NATIVE 0
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
struct nn_pipeline_program {
    nn_device *device;
    const struct nn_offline_graphics_stage *stage[2];
    const struct nn_offline_graphics_pair *pair;
    nn_bo *code[2],*c1[2];
    size_t code_bytes[2],c1_bytes[2];
    uint32_t c1_binding[2];
    int ready,cleanup_error;
    struct nn_offline_graphics_stage native_stage[2];
    struct nn_offline_graphics_pair native_pair;
    const struct nn_pipeline_texture_types *texture_types;
    const struct nn_pipeline_depth_export *depth_export;
    const struct nn_pipeline_storage_types *storage_types;
};
static int release(nn_pipeline_program *p)
{
    nn_bo **bos[]={p->code,p->code+1,p->c1,p->c1+1};
    /* Preflight every BO before mutating any owner on the ordinary busy path. */
    for(unsigned i=0;i<4;i++)if(*bos[i]){int e=nn_bo_idle(*bos[i]);if(e)return e;}
    p->ready=0;
    for(unsigned i=4;i-->0;)if(*bos[i]) {
        int e=nn_bo_destroy(*bos[i]);if(e){p->cleanup_error=e;return e;}*bos[i]=NULL;
    }
    return 0;
}
static int upload(nn_device *d,const void *bytes,size_t count,nn_bo **out)
{
    if(!bytes||!count||count>1048576)return EINVAL;
    int e=nn_bo_alloc(d,count,out);if(e)return e;
    void *map=nn_bo_map(*out);
    if(!map || nn_bo_size(*out)<count || nn_bo_address(*out)%128)return ENOTSUP;
    memset(map,0,nn_bo_size(*out));memcpy(map,bytes,count);
    return nn_bo_cache(*out,0,nn_bo_size(*out),0);
}
int nn_pipeline_program_create(nn_device *device,const struct nn_offline_graphics_archive *archive,
    const struct nn_offline_graphics_stage *vs,const struct nn_offline_graphics_stage *fs,
    const unsigned char vk[32],const unsigned char fk[32],nn_pipeline_program **out,nn_pipeline_program **retained)
{
    if(out)*out=NULL;
    if(retained)*retained=NULL;
    if(!device||!archive||!vs||!fs||!vk||!fk||!out||!retained||out==retained)return EINVAL;
    uint32_t chip=nn_device_chip(device);if(chip!=0x06050002)return ENOTSUP;
    const struct nn_offline_graphics_pair *pair=NULL;
    int e=nn_offline_graphics_find_pair(archive,chip,vs,fs,&pair);if(e)return e;
    const struct nn_offline_graphics_stage *stages[]={vs,fs};const unsigned char *keys[]={vk,fk};
    for(unsigned i=0;i<2;i++) {
        const struct nn_offline_graphics_stage *found=NULL,*s=stages[i];
        e=nn_offline_graphics_find_stage(archive,i,s->source,s->source_bytes,s->control,s->control_bytes,keys[i],&found);
        if(e)return e;
        if(found!=s)return EINVAL;
    }
    if(pair->vs_bytes>65536||pair->fs_bytes>65536)return ENOTSUP;
    nn_pipeline_program *p=calloc(1,sizeof(*p));if(!p)return ENOMEM;
    p->device=device;p->pair=pair;
    for(unsigned i=0;i<2;i++) {
        p->stage[i]=stages[i];p->code_bytes[i]=i?pair->fs_bytes:pair->vs_bytes;
        p->c1_bytes[i]=stages[i]->source_bytes-stages[i]->code_bytes;p->c1_binding[i]=UINT32_MAX;
        if(p->c1_bytes[i])for(size_t j=0;j<stages[i]->cbuf_count;j++)if(stages[i]->cbufs[j].slot==1) {
            if(stages[i]->cbufs[j].binding>=16||p->c1_binding[i]!=UINT32_MAX){e=EINVAL;goto failed;}
            p->c1_binding[i]=stages[i]->cbufs[j].binding;
        }
        e=upload(device,i?pair->fs_code:pair->vs_code,p->code_bytes[i],p->code+i);if(e)goto failed;
        if(p->c1_bytes[i]) {
            e=upload(device,stages[i]->source+stages[i]->code_bytes,p->c1_bytes[i],p->c1+i);if(e)goto failed;
        }
    }
    p->ready=1;*out=p;return 0;
failed:
    {int cleanup=release(p);if(cleanup){p->ready=0;p->cleanup_error=cleanup;*retained=p;}else free(p);}
    return e;
}
static int create_native(nn_device*d,const struct nn_pipeline_native_artifact*a,
 const struct nn_pipeline_depth_export*depth,const struct nn_pipeline_storage_types*storage,nn_pipeline_program**out,nn_pipeline_program**retained)
{
 if(out)*out=NULL;
 if(retained)*retained=NULL;
 if(!d||!a||!out||!retained||out==retained)return EINVAL;
 if(nn_device_chip(d)!=0x06050002||a->version!=1||!a->recipe||a->recipe->version!=(depth&&depth->writes_depth?NN_PIPELINE_DEPTH_RECIPE_VERSION:NN_PIPELINE_RECIPE_VERSION)||a->recipe->chip!=0x06050002||
    !a->textures||a->textures->version!=1||a->textures->recipe!=a->recipe||a->textures->texture_mask!=a->recipe->texture_mask||a->textures->sampler_mask!=a->recipe->sampler_mask)return ENOTSUP;
 if(depth){
#if NN_PIPELINE_DEPTH_NATIVE
  int e=nn_depth_native_recipe(depth);if(e)return e;
  if(depth->recipe!=a->recipe)return EINVAL;
#else
  return ENOTSUP;
#endif
 }
 for(unsigned i=0;i<2;i++){
  if(!a->spirv[i]||a->spirv_bytes[i]<20||a->spirv_bytes[i]%4||a->spirv_bytes[i]>1048576||a->spirv[i][0]!=0x07230203||
     a->textures->spirv[i]!=a->spirv[i]||a->textures->spirv_bytes[i]!=a->spirv_bytes[i]||!a->recipe->code[i]||
     !a->recipe->code_bytes[i]||a->recipe->code_bytes[i]%4||a->recipe->code_bytes[i]>65536)return EINVAL;
 }
 for(unsigned i=0;i<16;i++){int e=nn_pipeline_texture_type_validate(a->textures,i);if(e)return e;}
 unsigned needs_storage=0;
 for(size_t i=0;i<a->recipe->patch_count;i++)needs_storage|=a->recipe->patches[i].role==NN_PIPELINE_STORAGE;
 if(needs_storage&&!storage)return ENOTSUP;
 if(storage){
#if NN_PIPELINE_GRAPHICS_STORAGE
  struct nn_offline_graphics_stage owned[2]={{.stage=0,.spirv=(const unsigned char*)a->spirv[0],.spirv_bytes=a->spirv_bytes[0]},
   {.stage=1,.spirv=(const unsigned char*)a->spirv[1],.spirv_bytes=a->spirv_bytes[1]}};
  const struct nn_offline_graphics_stage *stages[2]={owned,owned+1};uint32_t entries=0;
  int e=nn_pipeline_storage_types_validate(storage,a->recipe,stages,&entries);if(e)return e;
  if(!entries)return EINVAL;
#else
  return ENOTSUP;
#endif
 }
 nn_pipeline_program*p=calloc(1,sizeof(*p));if(!p)return ENOMEM;
 p->device=d;p->texture_types=a->textures;p->depth_export=depth;p->storage_types=storage;p->native_pair=(struct nn_offline_graphics_pair){.chip=0x06050002,.vs_index=0,.fs_index=1,
  .vs_code=(const unsigned char*)a->recipe->code[0],.fs_code=(const unsigned char*)a->recipe->code[1],
  .vs_bytes=a->recipe->code_bytes[0],.fs_bytes=a->recipe->code_bytes[1],.compiler_json="{\"basis\":\"normal native SPIR-V artifact; not Maxwell\"}"};
 p->pair=&p->native_pair;int e=0;
 for(unsigned i=0;i<2;i++){
  p->native_stage[i]=(struct nn_offline_graphics_stage){.stage=i,.source=(const unsigned char*)a->spirv[i],
   .source_bytes=a->spirv_bytes[i],.code_bytes=a->spirv_bytes[i],.spirv=(const unsigned char*)a->spirv[i],.spirv_bytes=a->spirv_bytes[i],
   .reflection_json="{\"basis\":\"actual raw-native SPIR-V; no guest resource domains\"}"};
  p->stage[i]=p->native_stage+i;p->code_bytes[i]=a->recipe->code_bytes[i];p->c1_binding[i]=UINT32_MAX;
  if((e=upload(d,a->recipe->code[i],p->code_bytes[i],p->code+i)))goto failed;
 }
 p->ready=1;*out=p;return 0;
failed:
 {int cleanup=release(p);if(cleanup){p->cleanup_error=cleanup;*retained=p;}else free(p);}return e;
}
int nn_pipeline_program_destroy(nn_pipeline_program *p)
{if(!p)return EINVAL;int e=release(p);if(!e)free(p);return e;}
int nn_pipeline_program_stage(const nn_pipeline_program *p,uint32_t stage,struct nn_pipeline_stage_view *out)
{
    if(!out)return EINVAL;
    memset(out,0,sizeof(*out));
    if(!p||stage>1)return EINVAL;
    if(!p->ready)return EPIPE;
    *out=(struct nn_pipeline_stage_view){p->stage[stage],p->pair,p->code[stage],p->c1[stage],p->code_bytes[stage],p->c1_bytes[stage],p->c1_binding[stage]};return 0;
}
int nn_pipeline_program_resources(const nn_pipeline_program *p,nn_bo **bos,size_t capacity,size_t *count)
{
    if(!count)return EINVAL;
    *count=0;
    if(!p||(!bos&&capacity))return EINVAL;
    if(!p->ready)return EPIPE;
    nn_bo *values[]={p->code[0],p->code[1],p->c1[0],p->c1[1]};size_t needed=0;
    for(unsigned i=0;i<4;i++)needed+=values[i]!=NULL;
    *count=needed;if(capacity<needed)return ENOSPC;
    for(unsigned i=0;i<4;i++)if(values[i])*bos++=values[i];
    return 0;
}
int nn_pipeline_program_cleanup_error(const nn_pipeline_program *p){return p?p->cleanup_error:EINVAL;}

nn_device *nn_pipeline_program_device(const nn_pipeline_program *p){return p&&p->ready?p->device:NULL;}

const struct nn_pipeline_texture_types *nn_pipeline_program_texture_types(const nn_pipeline_program*p)
{return p&&p->ready?p->texture_types:NULL;}

static int create_typed(nn_device *d,const struct nn_offline_graphics_archive *archive,
 const struct nn_offline_graphics_stage *vs,const struct nn_offline_graphics_stage *fs,
 const struct nn_pipeline_texture_types *types,const struct nn_pipeline_depth_export *depth,const struct nn_pipeline_storage_types *storage,nn_pipeline_program **out,nn_pipeline_program **retained)
{
 if(out)*out=NULL;
 if(retained)*retained=NULL;
 if(!d||!out||!retained||out==retained||!types||!vs||!fs)return EINVAL;
 const struct nn_offline_graphics_pair *pair=NULL;
 int e=nn_offline_graphics_find_pair(archive,nn_device_chip(d),vs,fs,&pair);if(e)return e;
 const struct nn_pipeline_shader_recipe *r=types->recipe;
 if(depth){
#if NN_PIPELINE_DEPTH_NATIVE
  if(depth->recipe!=r)return EINVAL;
  e=nn_depth_native_recipe(depth);if(e)return e;
#else
  return ENOTSUP;
#endif
 }else if(!r||r->version!=NN_PIPELINE_RECIPE_VERSION)return ENOTSUP;

 if(types->version!=1||!r||r->chip!=pair->chip||r->texture_mask>65535||r->sampler_mask>65535||
    types->texture_mask!=r->texture_mask||types->sampler_mask!=r->sampler_mask)return EINVAL;
 const struct nn_offline_graphics_stage *stages[]={vs,fs};
 const unsigned char *code[]={pair->vs_code,pair->fs_code};size_t sizes[]={pair->vs_bytes,pair->fs_bytes};
 for(unsigned i=0;i<2;i++){
  if(!r->code[i]||r->code_bytes[i]!=sizes[i]||memcmp(r->code[i],code[i],sizes[i])||
     !types->spirv[i]||!stages[i]->spirv||types->spirv_bytes[i]!=stages[i]->spirv_bytes||
     types->spirv_bytes[i]<20||types->spirv_bytes[i]%4||types->spirv_bytes[i]>1048576||
     memcmp(types->spirv[i],stages[i]->spirv,types->spirv_bytes[i]))return EINVAL;
 }
 for(unsigned i=0;i<16;i++){
  int valid=nn_pipeline_texture_type_validate(types,i);if(valid)return valid;
 }
 unsigned needs_storage=0;for(size_t i=0;i<r->patch_count;i++)needs_storage|=r->patches[i].role==NN_PIPELINE_STORAGE;
 if(needs_storage||storage){
#if NN_PIPELINE_GRAPHICS_STORAGE
  uint32_t entries=0;if((e=nn_pipeline_storage_types_validate(storage,r,stages,&entries)))return e;
#else
  return ENOTSUP;
#endif
 }
 /* Source-cache caller first selects this immutable table from the exact
  * typed archive. This constructor additionally verifies both original stage
  * owners/native code/SPIR-V before publishing any Program metadata. It does
  * not answer actual resource queries or promote a source dimension. */
 e=nn_pipeline_program_create(d,archive,vs,fs,vs->specialization,fs->specialization,out,retained);
 if(!e){(*out)->texture_types=types;(*out)->depth_export=depth;(*out)->storage_types=storage;}
 return e;
}

int nn_pipeline_program_create_typed(nn_device*d,const struct nn_offline_graphics_archive*a,const struct nn_offline_graphics_stage*vs,const struct nn_offline_graphics_stage*fs,const struct nn_pipeline_texture_types*t,nn_pipeline_program**out,nn_pipeline_program**retained)
{return create_typed(d,a,vs,fs,t,NULL,NULL,out,retained);}
int nn_pipeline_program_create_typed_depth(nn_device*d,const struct nn_offline_graphics_archive*a,const struct nn_offline_graphics_stage*vs,const struct nn_offline_graphics_stage*fs,const struct nn_pipeline_texture_types*t,const struct nn_pipeline_depth_export*depth,nn_pipeline_program**out,nn_pipeline_program**retained)
{if(!depth){if(out)*out=NULL;if(retained)*retained=NULL;return EINVAL;}return create_typed(d,a,vs,fs,t,depth,NULL,out,retained);}

int nn_pipeline_program_create_native(nn_device*d,const struct nn_pipeline_native_artifact*a,nn_pipeline_program**out,nn_pipeline_program**retained)
{return create_native(d,a,NULL,NULL,out,retained);}
int nn_pipeline_program_create_native_depth(nn_device*d,const struct nn_pipeline_native_artifact*a,const struct nn_pipeline_depth_export*depth,nn_pipeline_program**out,nn_pipeline_program**retained)
{if(!depth){if(out)*out=NULL;if(retained)*retained=NULL;return EINVAL;}return create_native(d,a,depth,NULL,out,retained);}
const struct nn_pipeline_depth_export *nn_pipeline_program_depth_export(const nn_pipeline_program*p)
{return p&&p->ready?p->depth_export:NULL;}

const struct nn_pipeline_storage_types *nn_pipeline_program_storage_types(const nn_pipeline_program*p)
{return p&&p->ready?p->storage_types:NULL;}
int nn_pipeline_program_create_native_storage(nn_device*d,const struct nn_pipeline_native_artifact*a,
 const struct nn_pipeline_depth_export*depth,const struct nn_pipeline_storage_types*storage,nn_pipeline_program**out,nn_pipeline_program**retained)
{if(!storage){if(out)*out=NULL;if(retained)*retained=NULL;return EINVAL;}return create_native(d,a,depth,storage,out,retained);}

int nn_pipeline_program_create_typed_storage(nn_device*d,const struct nn_offline_graphics_archive*a,
 const struct nn_offline_graphics_stage*vs,const struct nn_offline_graphics_stage*fs,const struct nn_pipeline_texture_types*t,
 const struct nn_pipeline_depth_export*depth,const struct nn_pipeline_storage_types*storage,nn_pipeline_program**out,nn_pipeline_program**retained)
{if(!storage){if(out)*out=NULL;if(retained)*retained=NULL;return EINVAL;}return create_typed(d,a,vs,fs,t,depth,storage,out,retained);}
