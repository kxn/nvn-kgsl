/* Exact effective GPU bindings feed the same native encoder as ordinary
 * compute recording. Previously encoded source jobs are never submitted. */
static int command_project_compute(struct handle *destination,const struct command_dispatch *operation,
 const struct nn_command_state *state,size_t offset)
{
 const struct nn_command_binding *binding=nn_command_state_binding(state,NN_COMMAND_PROGRAM,NVN_SHADER_STAGE_COMPUTE,0);
 struct object *program=command_draw_object(binding,PROGRAM,destination->device);
 if(!program||!program->compiled||!program->bo)return ENOTSUP;
 struct bound ubos[16]={{0}},ssbos[16]={{0}};int e=0;
 for(unsigned i=0;i<16;i++){
  if((e=command_draw_bound(nn_command_state_binding(state,NN_COMMAND_UBO,NVN_SHADER_STAGE_COMPUTE,i),destination->device,ubos+i)))return e;
  if((e=command_draw_bound(nn_command_state_binding(state,NN_COMMAND_SSBO,NVN_SHADER_STAGE_COMPUTE,i),destination->device,ssbos+i)))return e;
 }
 struct job *job=NULL;size_t bytes=0;
 e=command_compute_encode(destination->device,program,ubos,ssbos,operation->groups,operation->indirect,destination,&job,&bytes);if(e)return e;
 job->projected_dispatch=1;job->command_offset=offset;job->command_bytes=bytes;
 job->next=destination->jobs;destination->jobs=job;destination->device->refs++;return 0;
}
static int command_source_jobs_known(const struct handle *source)
{
 if(source->borrowed_jobs)return ENOTSUP;
 for(const struct job *job=source->jobs;job;job=job->next){
  unsigned found=0;for(const struct command_dispatch *d=source->dispatches;d;d=d->next)if(d->recorded_job==job){found=1;break;}
  if(!found&&!command_copy_job_known(source,job))return ENOTSUP;
 }
 return 0;
}
