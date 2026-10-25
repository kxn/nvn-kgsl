/* SPDX-License-Identifier: MIT. Small actual-core typed metadata supplement.
 * Reuses the existing nonexecuting public probe backend and setup/cleanup. */
#define main nn_unused_shared_public_fixture_main
#include "public_copy_probe_failure.c"
#undef main
#include NN_PUBLIC_CORE_SOURCE
static int volume_permissions(struct npc_owner *o)
{
 struct npc_api *p=&o->api;o->active=calloc(1,sizeof(*o->active));CK(o->active);
 CK(!pool_create(o,0,262144));
 const int formats[]={NVN_FORMAT_RGBA8,NVN_FORMAT_RGBA16F};
 for(unsigned f=0;f<2;f++)for(unsigned flags=0;flags<=12;flags+=4){
  NVNtextureBuilder b={0};NVNtexture t={0};
  p->nvnTextureBuilderSetDefaults(&b);p->nvnTextureBuilderSetDevice(&b,&o->device);
  p->nvnTextureBuilderSetTarget(&b,NVN_TEXTURE_TARGET_3D);p->nvnTextureBuilderSetSize3D(&b,32,32,32);
  p->nvnTextureBuilderSetLevels(&b,1);p->nvnTextureBuilderSetFormat(&b,formats[f]);p->nvnTextureBuilderSetFlags(&b,(int)flags);
  CK(p->nvnTextureBuilderGetStorageSize(&b)==((size_t)131072u<<f));CK(p->nvnTextureBuilderGetStorageAlignment(&b)==512);
  p->nvnTextureBuilderSetStorage(&b,o->active->pools,0);CK(p->nvnTextureInitialize(&t,&b));CK(!nn_nvn_error());
  struct object *texture=find(&t,TEXTURE);CK(texture&&texture->source_layout);
  CK(texture->texture.flags==(int)flags&&texture->source_layout->flags==(flags&4));CK(!nn_tex_source_validate(texture->source_layout));
  /* IMAGE is preserved as permission; it does not bypass the unqualified
   * 3D sampled/render/image-consumer gate or enable a CPU pixel path. */
  CK(gpu_object_access(texture,"HOST 3D permission consumer gate")==ENOTSUP);nn_nvn_clear_error();
  p->nvnTextureFinalize(&t);CK(!nn_nvn_error());
 }
 CK(!destroy_active(o));return 0;
}
static int negative_and_metadata(void)
{
 struct npc_owner o={0};fault=7;CK(!api_load(&o.api));struct npc_api *p=&o.api;NVNdeviceBuilder db={0};NVNqueueBuilder qb={0};
 nn_nvn_clear_error();p->nvnDeviceBuilderSetDefaults(&db);CK(p->nvnDeviceInitialize(&o.device,&db));o.device_ready=1;
 p->nvnQueueBuilderSetDefaults(&qb);p->nvnQueueBuilderSetDevice(&qb,&o.device);CK(p->nvnQueueInitialize(&o.queue,&qb));o.queue_ready=1;
 CK(!volume_permissions(&o));
 const struct nn_public_copy_case *c=nn_public_copy_cases;o.active=calloc(1,sizeof(*o.active));CK(o.active);CK(!pool_create(&o,0,c->pool_bytes[0]));CK(!pool_create(&o,1,c->pool_bytes[1]));CK(!pool_create(&o,2,65536));CK(!texture_create(&o,c,0,0,c->canonical_offset));CK(!buffer_create(&o,1,1,c->linear_offset,c->linear_bytes));
 struct npc_active *a=o.active;CK(!posix_memalign((void**)&a->control[0],16,32768));CK(p->nvnCommandBufferInitialize(a->commands,&o.device));a->commands_ready[0]=1;
 p->nvnCommandBufferAddCommandMemory(a->commands,a->pools+2,0,32768);p->nvnCommandBufferAddControlMemory(a->commands,a->control[0],32768);p->nvnCommandBufferBeginRecording(a->commands);CK(!nn_nvn_error());
 struct recording *r=find(a->commands,COMMAND)->record;struct object *dev=find(&o.device,DEVICE);size_t bos=dev->gpu->bos,used=r->used,count=r->current->count;
 uint64_t linear=p->nvnBufferGetAddress(a->buffers+1);p->nvnCommandBufferCopyBufferToTexture(a->commands,linear,a->textures,NULL,c->region,0);CK(nn_nvn_error()==ENOTSUP);nn_nvn_clear_error();CK(!dev->volume_copy&&r->used==used&&!r->current->jobs&&r->current->count==count&&dev->gpu->bos==bos);
 CK(!NN_PUBLIC_COPY_REGISTER(&o.device));NVNtextureView view={0};p->nvnTextureViewSetDefaults(&view);p->nvnTextureViewSetLevels(&view,1,1);p->nvnTextureViewSetLayers(&view,0,1);CK(!nn_nvn_error());
 NVNcopyRegion bad={0,0,3,17,9,2};
 for(unsigned n=0;n<4;n++){
  if(n==1){bad.zoffset=-1;bad.depth=1;}
  if(n==2){bad.zoffset=0;p->nvnTextureViewSetLayers(&view,1,1);}
  if(n==3){p->nvnTextureViewSetLayers(&view,0,1);p->nvnTextureViewSetLevels(&view,3,1);}
  p->nvnCommandBufferCopyTextureToBuffer(a->commands,a->textures,&view,&bad,linear,0);CK(nn_nvn_error()==EINVAL);nn_nvn_clear_error();CK(r->used==used&&!r->current->jobs&&r->current->count==count&&dev->gpu->bos==bos);
 }
 CK(!destroy_active(&o));
 for(unsigned family=0;family<3;family++){
  c=nn_public_copy_cases+3+family;o.active=calloc(1,sizeof(*o.active));CK(o.active);a=o.active;CK(!pool_create(&o,0,c->pool_bytes[0]));CK(!pool_create(&o,2,65536));CK(!texture_create(&o,c,0,0,c->canonical_offset));if(family==2)CK(!texture_create(&o,c,1,0,c->destination_offset));CK(!buffer_create(&o,0,0,c->canonical_offset,c->source.payload_bytes));CK(!buffer_create(&o,1,0,family==2?c->destination_offset:c->linear_offset,family==2?c->source.payload_bytes:c->linear_bytes));
  unsigned clones=0,pins=0;CK(!record_copy(&o,c,&clones,&pins));CK(clones==1&&pins==(family==2?2u:1u));
  struct handle *h=handles;while(h&&h->id!=a->handles[1])h=h->next;struct job *jobs[6];unsigned nj=0;
#if NVN_PUBLIC_COPY_CALL
  CK(h&&h->call_nodes&&!h->jobs&&!h->borrowed_jobs);struct nn_command_state initial={0},final={0};struct handle *projected=NULL;
  CK(!command_project_sequence(find(&o.queue,QUEUE),h,&initial,&final,&projected));CK(projected&&!memcmp(&initial,&final,sizeof(initial)));
  for(struct job *j=projected->jobs;j;j=j->next)if(j->projected_dispatch!=2){CK(nj<6);jobs[nj++]=j;}
#else
  CK(h&&!h->jobs&&h->borrowed_jobs);
  for(struct job_borrow *b=h->borrowed_jobs;b;b=b->next){CK(nj<6);jobs[nj++]=b->job;CK(b->job->owner_retired&&b->job->borrowers==1);}
#endif
  CK(nj==(family==2?6u:3u));CK(!memcmp(nn_bo_map(jobs[0]->bo),NVN_COMPILED_IMAGE.code,NVN_COMPILED_IMAGE.code_bytes));
  struct nn_volume_copy_args saved[2];unsigned uniforms=family==2?2:1;const struct nn_tex_source_level *m=c->source.level+c->level;
  for(unsigned i=0;i<uniforms;i++){
   const struct nn_volume_copy_args *u=nn_bo_map(jobs[1+i]->bo);saved[i]=*u;unsigned dir=family==2?i:!family,row=family==2?c->row_bytes:c->row_stride,image=family==2?row*c->rows:c->image_stride;
   unsigned span=(c->layer_count-1)*image+(c->rows-1)*row+c->row_bytes;
   CK(u->canonical_base==m->offset&&u->canonical_slab_stride==m->pitch*((m->blocks_y+7)&~7u)*(1u<<c->source.level_block_depths[c->level]));
   CK(u->width_gobs==m->pitch/64&&u->block_depth_log2==c->source.level_block_depths[c->level]);CK(u->region_x_bytes==c->x_bytes&&u->region_y_rows==c->y_rows&&u->row_bytes==c->row_bytes&&u->rows==c->rows);CK(u->linear_row_stride==row&&u->linear_image_stride==image&&u->slices==c->layer_count&&u->direction==dir);CK(u->linear_span==span&&u->linear_words==(span+3)/4&&u->base_z==(family==2&&i?0:c->base_layer));
  }
  for(unsigned repeat=0;repeat<2;repeat++){
   p->nvnQueueSubmitCommands(&o.queue,1,a->handles+1);CK(!nn_nvn_error());CK(nn_bo_idle(find(a->textures,TEXTURE)->bo)==EBUSY);p->nvnQueueFinish(&o.queue);CK(!nn_nvn_error());
   for(unsigned i=0;i<uniforms;i++)CK(!memcmp(nn_bo_map(jobs[1+i]->bo),saved+i,sizeof(saved[i])));
  }
#if NVN_PUBLIC_COPY_CALL
  command_body_destroy(projected);
#endif
  CK(!destroy_active(&o));
 }
 p->nvnQueueFinalize(&o.queue);CK(!nn_nvn_error());p->nvnDeviceFinalize(&o.device);CK(!nn_nvn_error());return 0;
}
#if NVN_PUBLIC_COPY_CALL
#define NN_VOLUME_OWNER_FIELD "typed_UBO_and_Call_owner_checked"
#else
#define NN_VOLUME_OWNER_FIELD "typed_UBO_and_copied_owner_checked"
#endif
int main(void)
{
 CK(!negative_and_metadata());printf("{\"passed\":true,\"HOST_volume_profile_checks\":%u,\"actual_GPU_submits\":0,\"missing_volume_registration_rejected\":true,\"mip_Z_view_layer_bounds_rejected\":true,\"" NN_VOLUME_OWNER_FIELD "\":true}\n",checks);return 0;
}
