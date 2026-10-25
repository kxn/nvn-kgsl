/* SPDX-License-Identifier: MIT. No shader parser or expected query answer. */
#include "nvn_native/pipeline_depth_archive.h"
#include "nvn_native/depth_native.h"
#include <errno.h>
#ifndef NN_PIPELINE_DEPTH_NATIVE
#define NN_PIPELINE_DEPTH_NATIVE 0
#endif
int nn_pipeline_depth_archive_validate(const struct nn_pipeline_depth_archive*d)
{
#if NN_PIPELINE_DEPTH_NATIVE
 if(!d||d->version!=1||!d->typed||!d->pairs)return EINVAL;
 int e=nn_pipeline_typed_archive_validate(d->typed);if(e)return e;
 const struct nn_pipeline_archive*a=d->typed->archive;
 if(!d->pair_count||d->pair_count!=a->pair_count)return EINVAL;
 for(size_t i=0;i<d->pair_count;i++){
  const struct nn_pipeline_shader_recipe*r=a->pairs[i].recipe;const struct nn_pipeline_depth_export*p=d->pairs[i];
  if(!p){if(r->version!=NN_PIPELINE_RECIPE_VERSION)return ENOTSUP;continue;}
  if(p->recipe!=r)return EINVAL;
  if((e=nn_depth_native_recipe(p)))return e;
 }
 return 0;
#else
 (void)d;return ENOTSUP;
#endif
}
int nn_pipeline_select_depth_export(const struct nn_pipeline_depth_archive*d,const struct nn_offline_graphics_stage*vs,const struct nn_offline_graphics_stage*fs,const struct nn_pipeline_shader_recipe*r,const struct nn_pipeline_depth_export**out)
{
 if(!out)return EINVAL;
 *out=NULL;int e=nn_pipeline_depth_archive_validate(d);if(e)return e;
 const struct nn_pipeline_shader_recipe*selected=NULL;
 if((e=nn_pipeline_select_pair(d->typed->archive,vs,fs,&selected)))return e;
 if(selected!=r)return EINVAL;
 for(size_t i=0;i<d->pair_count;i++)if(d->typed->archive->pairs[i].recipe==r){if(!d->pairs[i])return ENOTSUP;*out=d->pairs[i];return 0;}
 return ENOTSUP;
}
