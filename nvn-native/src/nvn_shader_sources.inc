/* Certified original sources do not imply a compiled native specialization. */
int nn_nvn_register_shader_source_catalogue(NVNdevice*key,const struct nn_shader_source_archive*archive)
{
#if NN_NVN_SHADER_SOURCE_CATALOGUE
 struct object*d=find(key,DEVICE);if(!d)return nn_nvn_error();
 if(!d->pipeline_archive||!d->pipeline_reader)return EINVAL;
 if(d->shader_sources)return EEXIST;
 for(struct object*o=objects;o;o=o->next)if(o->kind==PROGRAM&&o->device==d)return EBUSY;
 struct nn_shader_source_catalogue*owned=NULL;int e=nn_shader_source_catalogue_create(archive,&owned);if(e)return e;
 d->shader_sources=owned;return 0;
#else
 (void)key;(void)archive;return ENOTSUP;
#endif
}
#if NN_NVN_SHADER_SOURCE_CATALOGUE
static int pipeline_prepare_bound_source(struct object*program,const NVNshaderData*caller,struct nn_pipeline_source_program*out)
{
 memset(out,0,sizeof(*out));struct object*d=program->device;
 if(!d->pipeline_archive||!d->shader_sources)return ENOTSUP;
 NVNshaderData shaders[2];int e=pipeline_read(d,shaders,caller,sizeof(shaders));if(e)return e;
 const struct nn_shader_source_entry*matched[2]={NULL,NULL};size_t sizes[2],size_count=nn_shader_source_control_sizes(d->shader_sources,sizes);
 for(unsigned input=0;input<2;input++){
  if(!shaders[input].control||((uintptr_t)shaders[input].control&7))return EINVAL;
  const struct nn_shader_source_entry*found=NULL;int read_error=0;
  for(size_t size=0;size<size_count;size++){
   unsigned char control[0x898];size_t first=0,end=0;
   e=pipeline_read(d,control,shaders[input].control,sizes[size]);if(e){if(!read_error)read_error=e;continue;}
   e=nn_shader_source_candidates(d->shader_sources,control,sizes[size],&first,&end);if(e==ENOTSUP)continue;if(e)return e;
   const struct nn_shader_source_entry*s=nn_shader_source_entry_at(d->shader_sources,first);
   /* Full identical controls encode identical source/code byte lengths. The
    * independently validated catalogue sorts remaining candidates by source
    * bytes, so read the actual pool once and binary-search, not N copies. */
   struct object*pool=address_pool(d,shaders[input].data,s->source_bytes);if(!pool)continue;
   size_t offset=shaders[input].data-nn_bo_address(pool->bo);
   if(gpu_pool_access(pool,offset,s->source_bytes,"nvnProgramSetShaders"))return ENOTSUP;
   if(pool->external||(pool->flags&NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS)||!nn_bo_map(pool->bo))return ENOTSUP;
   if((e=nn_bo_idle(pool->bo)))return e;
   unsigned char*source=malloc(s->source_bytes);if(!source)return ENOMEM;
   e=pipeline_read(d,source,(const char*)nn_bo_map(pool->bo)+offset,s->source_bytes);
   size_t low=first,high=end;
   while(!e&&low<high){size_t mid=low+(high-low)/2;const struct nn_shader_source_entry*x=nn_shader_source_entry_at(d->shader_sources,mid);int cmp=memcmp(source,x->source,s->source_bytes);if(cmp>0)low=mid+1;else high=mid;}
   const struct nn_shader_source_entry*match=low<end?nn_shader_source_entry_at(d->shader_sources,low):NULL;
   int equal=!e&&match&&!memcmp(source,match->source,s->source_bytes);free(source);if(e)return e;if(!equal)continue;
   if(found)return EEXIST;
   found=match;
  }
  if(!found)return read_error?read_error:ENOTSUP;
  if(matched[found->stage])return EINVAL;
  matched[found->stage]=found;
 }
 if(!matched[0]||!matched[1])return ENOTSUP;
 struct nn_shader_source_program*owned=NULL;e=nn_shader_source_program_create(d->shader_sources,matched[0],matched[1],&owned);if(e)return e;
 e=nn_pipeline_source_prepare_bound(d->pipeline_archive,owned,out);if(e)nn_shader_source_program_destroy(owned);return e;
}
#endif
