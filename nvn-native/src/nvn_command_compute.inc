/* SPDX-License-Identifier: MIT. Shared native compute encoder over exact
 * Program/UBO/SSBO owners. This is the original independent native mapping,
 * factored for record and enqueue; no shader execution occurs on CPU. */
static int command_compute_encode(struct object *device,struct object *program,
 const struct bound ubind[16],const struct bound sbind[16],const uint32_t groups[3],
 uint64_t indirect,struct handle *destination,struct job **out,size_t *bytes)
{
 *out=NULL;*bytes=0;
 if(!program||program->device!=device||!program->compiled||!program->bo)return ENOTSUP;
 const struct nn_nvn_compiled_cs *compiled=program->compiled;
 struct nn_cs_buffer ubos[16]={{0}},ssbos[16]={{0}};int e=add_resource(destination,program);if(e)return e;
 for(size_t i=0;i<compiled->cbuf_count;i++){
  struct nn_nvn_cbuf map=compiled->cbufs[i];
  if(map.slot<3||map.slot>16||map.binding>=16||!ubind[map.slot-3].pool)return EINVAL;
  const struct bound *bound=ubind+map.slot-3;
  if((e=gpu_bound_access(bound,"NVN compute UBO")))return e;
  ubos[map.binding]=(struct nn_cs_buffer){bound->address,bound->size};
  if((e=add_resource(destination,bound->pool)))return e;
 }
 for(size_t i=0;i<compiled->sbuf_count;i++){
  struct nn_nvn_sbuf map=compiled->sbufs[i];
  if(map.binding>=16||map.cbuf_slot!=0||map.cbuf_word<0x310/4||(map.cbuf_word-0x310/4)%4||(map.cbuf_word-0x310/4)/4>=16)return ENOTSUP;
  const struct bound *bound=sbind+(map.cbuf_word-0x310/4)/4;if(!bound->pool)return EINVAL;
  if((e=gpu_bound_access(bound,"NVN compute SSBO")))return e;
  ssbos[map.binding]=(struct nn_cs_buffer){bound->address,bound->size};
  if((e=add_resource(destination,bound->pool)))return e;
 }
 if(indirect){
  struct object *arguments=address_pool(device,indirect,12);if(!arguments||indirect%4)return EINVAL;
  if((e=gpu_pool_access(arguments,indirect-nn_bo_address(arguments->bo),12,"nvnCommandBufferDispatchComputeIndirect")))return e;
  if((e=add_resource(destination,arguments)))return e;
 }
 struct job *job=calloc(1,sizeof(*job));if(!job)return ENOMEM;
 e=indirect?nn_cs_encode_indirect(device->gpu,program->bo,compiled->image,ubos,ssbos,indirect,&job->bo,bytes):
   nn_cs_encode(device->gpu,program->bo,compiled->image,ubos,ssbos,groups,&job->bo,bytes);
 if(e){
  /* Preserve the native encoder's owner on failed cleanup; the existing
   * device quarantine/reaper alone decides when destruction is possible. */
  if(job->bo){job->next=device->retired;device->retired=job;device->refs++;}else free(job);
  return e;
 }
 job->compute=1;*out=job;return 0;
}
static void command_dispatch_free(struct command_dispatch *d)
{while(d){struct command_dispatch *next=d->next;free(d);d=next;}}
static int command_record_deferred_compute(struct object *o,const uint32_t groups[3],uint64_t indirect)
{
 struct recording *r=o->record;struct command_dispatch *d=calloc(1,sizeof(*d));if(!d)return ENOMEM;
 memcpy(d->groups,groups,sizeof(d->groups));d->indirect=indirect;
 int e=0;
 if(indirect){
  struct object *arguments=address_pool(o->device,indirect,12);if(!arguments||indirect%4){e=EINVAL;goto fail;}
  if((e=gpu_pool_access(arguments,indirect-nn_bo_address(arguments->bo),12,"nvnCommandBufferDispatchComputeIndirect")))goto fail;
  if((e=add_resource(r->current,arguments)))goto fail;
 }
 if((e=command_capture_close(o))||(e=command_reserve(o,24)))goto fail;
 d->command_offset=r->logical_bytes+r->used-r->start;
 memset((char*)nn_bo_map(r->pool->bo)+r->offset+r->used,0,16);r->used+=16;
 d->next=r->current->dispatches;r->current->dispatches=d;return 0;
 fail:free(d);return e;
}
