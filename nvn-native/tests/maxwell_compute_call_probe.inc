/* SPDX-License-Identifier: MIT. Explicit qualification only. Reuses the four
 * original programs/oracles and existing NativeActivity result collector.
 * Calls only public NVN; no native encode/submit hook or CPU shader execution. */
struct compute_call_fixture {
 struct fixture original; /* first: existing quarantine owner header */
 GUARDED(NVNqueue) prep_queue;
 GUARDED(NVNcommandBuffer) parent,prep;
 GUARDED(NVNmemoryPool) old_pool;
 _Alignas(16)unsigned char parent_control[512],prep_control[512];
 unsigned char *old_data,old_expected[DATA_BYTES];
};
int nn_maxwell_compute_probe(char *json,size_t capacity)
{
 struct compute_call_fixture *owner=NULL;struct fixture *f=NULL;
 const char *stage="resolver",*submission="not_attempted";
 int error=0,cleanup=0,main_pending=0,prep_pending=0;
 unsigned device_ready=0,queue_ready=0,prep_queue_ready=0,data_ready=0,old_ready=0,arena_ready=0;
 unsigned child_ready=0,parent_ready=0,prep_ready=0,program_ready[4]={0};
 NVNcommandHandle handle=0,child_handle=0,seed_handle=0,prep_handle=0;
 unsigned completed=0,submissions=0,finishes=0,direct_submissions=0,indirect_submissions=0,prep_submissions=0,prep_finishes=0;
 unsigned recorded=0,released=0,replays=0,seeds=0,children=0,child_finalizes=0,calls=0,busy=0,zeros_checked=0,group_reads=0,uniform_reads=0;
 uint64_t data_checks=0,old_checks=0,command_checks=0,output_checks=0,readonly_checks=0,guard_checks=0;
 uint64_t base=0,old_base=0;unsigned current_program=~0u,current_case=~0u,current_repeat=~0u;size_t bad_offset=SIZE_MAX;unsigned bad_pool=~0u,bad_actual=0,bad_expected=0;
 size_t child_control_used=0,parent_control_used=0,prep_control_used=0,cases_used=0;char cases[20000]="";
 NVNdeviceBuilder db;NVNqueueBuilder qb;NVNmemoryPoolBuilder pb;
#define ENTRY(name,type) type name=(type)nvnBootstrapLoader(#name);if(!name)error=ENOSYS;
#include "../src/nvn_entries.inc"
#undef ENTRY
#define CALL(x) do{x;if((error=nn_nvn_error()))goto done;}while(0)
#define BOOL(x) do{NVNboolean ok_=(x);int e_=nn_nvn_error();if(!ok_||e_){error=e_?e_:EIO;goto done;}}while(0)
#define REQUIRE(x) do{int ok_=!!(x);int e_=nn_nvn_error();if(!ok_||e_){error=e_?e_:EINVAL;goto done;}}while(0)
#define GUARD(o) do{if((error=check_guard((o).before,sizeof((o).before),0x5a,&guard_checks))||(error=check_guard((o).after,sizeof((o).after),0x5a,&guard_checks)))goto done;}while(0)
 if(!json||!capacity)return -1;
 if(error)goto done;
 nn_nvn_clear_error();stage="profile";
 if(sysconf(_SC_PAGESIZE)!=4096){error=ENOTSUP;goto done;}
 for(unsigned p=0;p<4;p++){
  const struct nn_nvn_compiled_cs *c=test_programs[p];
  if(!c||!c->image||c->image->chip!=0x06050002||!c->source_bytes||c->source_bytes>CODE_STRIDE||c->control_bytes!=0x898||c->image->local[0]!=32||c->image->local[1]!=1||c->image->local[2]!=1){error=ENOTSUP;goto done;}
 }
 owner=calloc(1,sizeof(*owner));if(!owner){error=ENOMEM;goto done;}f=&owner->original;
 memset(&f->device,0x5a,sizeof(f->device));memset(&f->queue,0x5a,sizeof(f->queue));memset(&f->command,0x5a,sizeof(f->command));
 memset(&f->data_pool,0x5a,sizeof(f->data_pool));memset(&f->command_pool,0x5a,sizeof(f->command_pool));memset(f->programs,0x5a,sizeof(f->programs));
 memset(&owner->prep_queue,0x5a,sizeof(owner->prep_queue));memset(&owner->parent,0x5a,sizeof(owner->parent));memset(&owner->prep,0x5a,sizeof(owner->prep));memset(&owner->old_pool,0x5a,sizeof(owner->old_pool));
 if((error=posix_memalign((void**)&f->data,4096,DATA_BYTES))||(error=posix_memalign((void**)&f->commands,4096,DATA_BYTES))||(error=posix_memalign((void**)&owner->old_data,4096,DATA_BYTES)))goto done;
 memset(f->data,0xa5,DATA_BYTES);memset(f->commands,0xa5,DATA_BYTES);memset(owner->old_data,0xa5,DATA_BYTES);
 const uint32_t limit=LIMIT;memset(owner->old_data+UBO_OFFSET,0,64);memcpy(owner->old_data+UBO_OFFSET,&limit,4);memcpy(owner->old_expected,owner->old_data,DATA_BYTES);
 stage="initialize";CALL(nvnDeviceBuilderSetDefaults(&db));BOOL(nvnDeviceInitialize(&f->device.value,&db));device_ready=1;
 CALL(nvnQueueBuilderSetDefaults(&qb));CALL(nvnQueueBuilderSetDevice(&qb,&f->device.value));BOOL(nvnQueueInitialize(&f->queue.value,&qb));queue_ready=1;BOOL(nvnQueueInitialize(&owner->prep_queue.value,&qb));prep_queue_ready=1;
 CALL(nvnMemoryPoolBuilderSetDefaults(&pb));CALL(nvnMemoryPoolBuilderSetDevice(&pb,&f->device.value));
 CALL(nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED|NVN_MEMORY_POOL_FLAGS_SHADER_CODE));CALL(nvnMemoryPoolBuilderSetStorage(&pb,f->data,DATA_BYTES));BOOL(nvnMemoryPoolInitialize(&f->data_pool.value,&pb));data_ready=1;
 CALL(nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));CALL(nvnMemoryPoolBuilderSetStorage(&pb,f->commands,DATA_BYTES));BOOL(nvnMemoryPoolInitialize(&f->command_pool.value,&pb));arena_ready=1;
 CALL(nvnMemoryPoolBuilderSetStorage(&pb,owner->old_data,DATA_BYTES));BOOL(nvnMemoryPoolInitialize(&owner->old_pool.value,&pb));old_ready=1;
 REQUIRE(nvnMemoryPoolMap(&f->data_pool.value)==f->data&&nvnMemoryPoolMap(&f->command_pool.value)==f->commands&&nvnMemoryPoolMap(&owner->old_pool.value)==owner->old_data);
 base=nvnMemoryPoolGetBufferAddress(&f->data_pool.value);old_base=nvnMemoryPoolGetBufferAddress(&owner->old_pool.value);REQUIRE(base&&old_base&&base!=old_base);
 for(unsigned p=0;p<4;p++){
  stage="program";if((error=nn_nvn_register_compute(&f->device.value,test_programs[p])))goto done;
  memcpy(f->data+CODE_OFFSET+p*CODE_STRIDE,test_programs[p]->source_code,test_programs[p]->source_bytes);
  BOOL(nvnProgramInitialize(&f->programs[p].value,&f->device.value));program_ready[p]=1;
  NVNshaderData shader={base+CODE_OFFSET+p*CODE_STRIDE,test_programs[p]->source_control};BOOL(nvnProgramSetShaders(&f->programs[p].value,1,&shader));
 }
 /* Preserve the original indirect GPU producer. Its private copy jobs stay
  * on an ordinary queue, separated from Call by an actual successful Finish. */
 stage="record-prep";memset(owner->prep_control,0xa5,sizeof(owner->prep_control));BOOL(nvnCommandBufferInitialize(&owner->prep.value,&f->device.value));prep_ready=1;
 CALL(nvnCommandBufferAddCommandMemory(&owner->prep.value,&f->command_pool.value,8192,4096));CALL(nvnCommandBufferAddControlMemory(&owner->prep.value,owner->prep_control,sizeof(owner->prep_control)));CALL(nvnCommandBufferBeginRecording(&owner->prep.value));
 CALL(nvnCommandBufferClearBuffer(&owner->prep.value,base+INDIRECT_OFFSET,12,1));CALL(nvnCommandBufferCopyBufferToBuffer(&owner->prep.value,base+GROUP_SOURCE_OFFSET,base+INDIRECT_OFFSET,4,0));
 CALL(nvnCommandBufferUpdateUniformBuffer(&owner->prep.value,base+UBO_OFFSET,64,0,4,&limit));CALL(nvnCommandBufferBarrier(&owner->prep.value,NVN_BARRIER_ORDER_INDIRECT_DATA_BIT));
 prep_handle=nvnCommandBufferEndRecording(&owner->prep.value);REQUIRE(prep_handle);prep_control_used=nvnCommandBufferGetControlMemoryUsed(&owner->prep.value);REQUIRE(prep_control_used&&prep_control_used<=sizeof(owner->prep_control));CALL(nvnCommandBufferFinalize(&owner->prep.value));prep_ready=0;
 for(unsigned p=0;p<4;p++)for(unsigned shape=0;shape<4;shape++){
  unsigned indirect=shape==3,count=indirect?LIMIT:(unsigned[]){1,33,65}[shape],source_program=(p+1)%4;
  uint32_t direct_groups[3]={(count+31)/32,1,1};current_program=p;current_case=shape;stage="record-source";
  memset(f->commands,0xa5,8192);memset(f->control,0xa5,sizeof(f->control));memset(owner->parent_control,0xa5,sizeof(owner->parent_control));
  BOOL(nvnCommandBufferInitialize(&f->command.value,&f->device.value));child_ready=1;CALL(nvnCommandBufferAddCommandMemory(&f->command.value,&f->command_pool.value,0,4096));CALL(nvnCommandBufferAddControlMemory(&f->command.value,f->control,sizeof(f->control)));CALL(nvnCommandBufferBeginRecording(&f->command.value));
  /* CPU prerequisites for the existing ordinary encoder, never submitted.
   * The following child sequence itself writes none of these GPU bindings. */
  CALL(nvnCommandBufferBindProgram(&f->command.value,&f->programs[source_program].value,NVN_SHADER_STAGE_COMPUTE_BIT));CALL(nvnCommandBufferBindUniformBuffer(&f->command.value,NVN_SHADER_STAGE_COMPUTE,0,old_base+UBO_OFFSET,64));
  CALL(nvnCommandBufferBindStorageBuffer(&f->command.value,NVN_SHADER_STAGE_COMPUTE,0,old_base+SOURCE_OFFSET,LIMIT*test_oracles[source_program].stride));CALL(nvnCommandBufferBindStorageBuffer(&f->command.value,NVN_SHADER_STAGE_COMPUTE,1,old_base+TARGET_OFFSET,LIMIT*8));
  seed_handle=nvnCommandBufferEndRecording(&f->command.value);REQUIRE(seed_handle);++seeds;CALL(nvnDeviceFinalizeCommandHandle(&f->device.value,seed_handle));seed_handle=0;
  CALL(nvnCommandBufferBeginRecording(&f->command.value));
  if(indirect)CALL(nvnCommandBufferDispatchComputeIndirect(&f->command.value,base+INDIRECT_OFFSET));else CALL(nvnCommandBufferDispatchCompute(&f->command.value,(int)direct_groups[0],1,1));
  child_handle=nvnCommandBufferEndRecording(&f->command.value);REQUIRE(child_handle);++children;child_control_used=nvnCommandBufferGetControlMemoryUsed(&f->command.value);REQUIRE(child_control_used&&child_control_used<=sizeof(f->control));
  stage="record-parent";BOOL(nvnCommandBufferInitialize(&owner->parent.value,&f->device.value));parent_ready=1;CALL(nvnCommandBufferAddCommandMemory(&owner->parent.value,&f->command_pool.value,4096,4096));CALL(nvnCommandBufferAddControlMemory(&owner->parent.value,owner->parent_control,sizeof(owner->parent_control)));CALL(nvnCommandBufferBeginRecording(&owner->parent.value));
  CALL(nvnCommandBufferBindProgram(&owner->parent.value,&f->programs[p].value,NVN_SHADER_STAGE_COMPUTE_BIT));
  NVNbufferRange uniform={base+UBO_OFFSET,64},storage[2]={{base+SOURCE_OFFSET,LIMIT*test_oracles[p].stride},{base+TARGET_OFFSET,LIMIT*8}};
  if(shape&1){CALL(nvnCommandBufferBindUniformBuffers(&owner->parent.value,NVN_SHADER_STAGE_COMPUTE,0,1,&uniform));CALL(nvnCommandBufferBindStorageBuffers(&owner->parent.value,NVN_SHADER_STAGE_COMPUTE,0,2,storage));}
  else {CALL(nvnCommandBufferBindUniformBuffer(&owner->parent.value,NVN_SHADER_STAGE_COMPUTE,0,uniform.address,uniform.size));CALL(nvnCommandBufferBindStorageBuffer(&owner->parent.value,NVN_SHADER_STAGE_COMPUTE,0,storage[0].address,storage[0].size));CALL(nvnCommandBufferBindStorageBuffer(&owner->parent.value,NVN_SHADER_STAGE_COMPUTE,1,storage[1].address,storage[1].size));}
  CALL(nvnCommandBufferCallCommands(&owner->parent.value,1,&child_handle));++calls;handle=nvnCommandBufferEndRecording(&owner->parent.value);REQUIRE(handle);++recorded;parent_control_used=nvnCommandBufferGetControlMemoryUsed(&owner->parent.value);REQUIRE(parent_control_used&&parent_control_used<=sizeof(owner->parent_control));
  CALL(nvnDeviceFinalizeCommandHandle(&f->device.value,child_handle));child_handle=0;++child_finalizes;CALL(nvnCommandBufferFinalize(&f->command.value));child_ready=0;CALL(nvnCommandBufferFinalize(&owner->parent.value));parent_ready=0;
  memcpy(f->command_shadow,f->commands,DATA_BYTES);
  for(unsigned repeat=0;repeat<2;repeat++){
   current_repeat=repeat;uint32_t groups[3]={indirect?(repeat?3u:1u):direct_groups[0],1,1};unsigned outputs=indirect?(repeat?65u:32u):count;const uint32_t zero[3]={0};
   memset(f->data+SOURCE_OFFSET,0xa5,4096-SOURCE_OFFSET);memset(f->data+TARGET_OFFSET,0xa5,4096);
   for(unsigned i=0;i<LIMIT*test_oracles[p].stride/8;i++){uint64_t value=original_input(i,repeat);memcpy(f->data+SOURCE_OFFSET+i*8,&value,8);}
   memset(f->data+UBO_OFFSET,0,64);if(!indirect)memcpy(f->data+UBO_OFFSET,&count,4);memcpy(f->data+INDIRECT_OFFSET,zero,12);memcpy(f->data+GROUP_SOURCE_OFFSET,groups,4);
   if(indirect){REQUIRE(!memcmp(f->data+INDIRECT_OFFSET,zero,12));++zeros_checked;}
   memcpy(f->expected,f->data,DATA_BYTES);if(indirect){memcpy(f->expected+INDIRECT_OFFSET,groups,12);memcpy(f->expected+UBO_OFFSET,&limit,4);}
   for(unsigned i=0;i<outputs;i++){uint64_t value=original_result(f->data+SOURCE_OFFSET,i,p);memcpy(f->expected+TARGET_OFFSET+i*8,&value,8);}
   CALL(nvnMemoryPoolFlushMappedRange(&f->data_pool.value,0,DATA_BYTES));CALL(nvnMemoryPoolFlushMappedRange(&owner->old_pool.value,0,DATA_BYTES));CALL(nvnMemoryPoolFlushMappedRange(&f->command_pool.value,0,DATA_BYTES));
   if(indirect){stage="submit-prep";prep_pending=1;CALL(nvnQueueSubmitCommands(&owner->prep_queue.value,1,&prep_handle));++prep_submissions;stage="finish-prep";CALL(nvnQueueFinish(&owner->prep_queue.value));prep_pending=0;++prep_finishes;}
   /* No CPU read of indirect counts occurs between prep Finish and Dispatch. */
   stage="submit-call";submission="unknown";main_pending=1;CALL(nvnQueueSubmitCommands(&f->queue.value,1,&handle));submission="accepted";++submissions;if(indirect)++indirect_submissions;else ++direct_submissions;if(repeat)++replays;
   if(repeat){CALL(nvnDeviceFinalizeCommandHandle(&f->device.value,handle));handle=0;++released;}
   nvnProgramFinalize(&f->programs[p].value);if(nn_nvn_error()!=EBUSY){error=EINVAL;goto done;}++busy;nn_nvn_clear_error();
   stage="finish-call";CALL(nvnQueueFinish(&f->queue.value));main_pending=0;++finishes;
   CALL(nvnMemoryPoolInvalidateMappedRange(&f->data_pool.value,0,DATA_BYTES));CALL(nvnMemoryPoolInvalidateMappedRange(&owner->old_pool.value,0,DATA_BYTES));CALL(nvnMemoryPoolInvalidateMappedRange(&f->command_pool.value,0,DATA_BYTES));
   stage="readback";if(indirect){REQUIRE(!memcmp(f->data+INDIRECT_OFFSET,groups,12));++group_reads;}REQUIRE(!memcmp(f->data+UBO_OFFSET,indirect?&limit:&count,4));++uniform_reads;
   for(size_t i=0;i<DATA_BYTES;i++){
    if(f->data[i]!=f->expected[i]){bad_pool=0;bad_offset=i;bad_actual=f->data[i];bad_expected=f->expected[i];error=EIO;goto done;}++data_checks;
    if(owner->old_data[i]!=owner->old_expected[i]){bad_pool=1;bad_offset=i;bad_actual=owner->old_data[i];bad_expected=owner->old_expected[i];error=EIO;goto done;}++old_checks;
    if(f->commands[i]!=f->command_shadow[i]){bad_pool=2;bad_offset=i;bad_actual=f->commands[i];bad_expected=f->command_shadow[i];error=EIO;goto done;}++command_checks;
    if(i>=TARGET_OFFSET&&i<TARGET_OFFSET+outputs*8)++output_checks;
    if((i<TARGET_OFFSET||i>=TARGET_OFFSET+LIMIT*8)&&(!indirect||((i<INDIRECT_OFFSET||i>=INDIRECT_OFFSET+12)&&(i<UBO_OFFSET||i>=UBO_OFFSET+4))))++readonly_checks;
   }
   GUARD(f->device);GUARD(f->queue);GUARD(f->command);GUARD(f->data_pool);GUARD(f->command_pool);GUARD(owner->prep_queue);GUARD(owner->parent);GUARD(owner->prep);GUARD(owner->old_pool);for(unsigned i=0;i<4;i++){GUARD(f->programs[i]);}
   if((error=check_guard(f->control+child_control_used,sizeof(f->control)-child_control_used,0xa5,&guard_checks))||(error=check_guard(owner->parent_control+parent_control_used,sizeof(owner->parent_control)-parent_control_used,0xa5,&guard_checks))||(error=check_guard(owner->prep_control+prep_control_used,sizeof(owner->prep_control)-prep_control_used,0xa5,&guard_checks)))goto done;
   int n=snprintf(cases+cases_used,sizeof(cases)-cases_used,"%s{\"program\":%u,\"source_program\":%u,\"shape\":%u,\"indirect\":%s,\"replay\":%u,\"groups\":[%u,1,1],\"limit\":%u,\"output_elements\":%u,\"source_ubo\":%llu,\"source_ssbo\":[%llu,%llu],\"parent_ubo\":%llu,\"parent_ssbo\":[%llu,%llu],\"prep_queue_used\":%s,\"passed\":true}",cases_used?",":"",p,source_program,shape,indirect?"true":"false",repeat,groups[0],count,outputs,(unsigned long long)(old_base+UBO_OFFSET),(unsigned long long)(old_base+SOURCE_OFFSET),(unsigned long long)(old_base+TARGET_OFFSET),(unsigned long long)(base+UBO_OFFSET),(unsigned long long)(base+SOURCE_OFFSET),(unsigned long long)(base+TARGET_OFFSET),indirect?"true":"false");
   if(n<0||(size_t)n>=sizeof(cases)-cases_used){error=ENOSPC;goto done;}cases_used+=(size_t)n;++completed;
  }
 }
 stage="complete";
done:
 if(f&&main_pending&&queue_ready){nn_nvn_clear_error();nvnQueueFinish(&f->queue.value);if(!nn_nvn_error())main_pending=0;}
 if(owner&&prep_pending&&prep_queue_ready){nn_nvn_clear_error();nvnQueueFinish(&owner->prep_queue.value);if(!nn_nvn_error())prep_pending=0;}
 if(main_pending||prep_pending)cleanup=EBUSY;
 else if(f){
#define CLEAN(x) do{nn_nvn_clear_error();x;int e_=nn_nvn_error();if(e_&&!cleanup)cleanup=e_;}while(0)
  if(handle){CLEAN(nvnDeviceFinalizeCommandHandle(&f->device.value,handle));handle=0;}
  if(child_handle){CLEAN(nvnDeviceFinalizeCommandHandle(&f->device.value,child_handle));child_handle=0;}
  if(seed_handle){CLEAN(nvnDeviceFinalizeCommandHandle(&f->device.value,seed_handle));seed_handle=0;}
  if(child_ready)CLEAN(nvnCommandBufferFinalize(&f->command.value));
  if(parent_ready)CLEAN(nvnCommandBufferFinalize(&owner->parent.value));
  if(prep_handle)CLEAN(nvnDeviceFinalizeCommandHandle(&f->device.value,prep_handle));
  if(prep_ready)CLEAN(nvnCommandBufferFinalize(&owner->prep.value));
  for(unsigned p=0;p<4;p++)if(program_ready[p])CLEAN(nvnProgramFinalize(&f->programs[p].value));
  if(old_ready)CLEAN(nvnMemoryPoolFinalize(&owner->old_pool.value));
  if(data_ready)CLEAN(nvnMemoryPoolFinalize(&f->data_pool.value));
  if(arena_ready)CLEAN(nvnMemoryPoolFinalize(&f->command_pool.value));
  if(prep_queue_ready)CLEAN(nvnQueueFinalize(&owner->prep_queue.value));
  if(queue_ready)CLEAN(nvnQueueFinalize(&f->queue.value));
  if(device_ready)CLEAN(nvnDeviceFinalize(&f->device.value));
#undef CLEAN
 }
 if(owner){if(cleanup){f->next=quarantine;quarantine=f;}else{free(f->data);free(f->commands);free(owner->old_data);free(owner);}}
 int passed=!error&&!cleanup&&completed==32&&submissions==32&&finishes==32&&direct_submissions==24&&indirect_submissions==8&&prep_submissions==8&&prep_finishes==8&&recorded==16&&released==16&&replays==16&&seeds==16&&children==16&&child_finalizes==16&&calls==16&&busy==32&&zeros_checked==8&&group_reads==8&&uniform_reads==32&&data_checks==524288&&old_checks==524288&&command_checks==524288&&output_checks==9440&&readonly_checks==507520&&guard_checks;
 int n=snprintf(json,capacity,"{\"schema\":1,\"operation\":\"nvn_maxwell_compute_call\",\"uid\":%u,\"stage\":\"%s\",\"errno\":%d,\"cleanup_errno\":%d,\"submission\":\"%s\",\"completed_cases\":%u,\"submissions\":%u,\"finishes\":%u,\"direct_submissions\":%u,\"indirect_submissions\":%u,\"prep_submissions\":%u,\"prep_finishes\":%u,\"handles_recorded\":%u,\"handles_released\":%u,\"handle_replays\":%u,\"state_only_seeds_recorded\":%u,\"source_handles_recorded\":%u,\"source_handles_finalized_before_submit\":%u,\"CallCommands_recorded\":%u,\"program_busy_rejections\":%u,\"cpu_zero_group_checks\":%u,\"gpu_group_readbacks\":%u,\"uniform_readbacks\":%u,\"data_byte_checks\":%llu,\"old_binding_pool_byte_checks\":%llu,\"command_byte_checks\":%llu,\"output_byte_checks\":%llu,\"readonly_byte_checks\":%llu,\"guard_byte_checks\":%llu,\"source_resource_base\":%llu,\"parent_resource_base\":%llu,\"state_only_seed_submitted\":false,\"copy_jobs_in_call\":false,\"ordinary_prep_queue\":true,\"GPU_effective_bindings_verified\":%s,\"CPU_shader_execution\":false,\"runtime_compiler\":false,\"game_modules_executed\":false,\"resources_retained\":%s,\"program\":%u,\"shape\":%u,\"replay\":%u,\"bad_pool\":%u,\"mismatch_offset\":%zu,\"observed_byte\":%u,\"expected_byte\":%u,\"cases\":[%s],\"passed\":%s}\n",(unsigned)getuid(),stage,error,cleanup,submission,completed,submissions,finishes,direct_submissions,indirect_submissions,prep_submissions,prep_finishes,recorded,released,replays,seeds,children,child_finalizes,calls,busy,zeros_checked,group_reads,uniform_reads,(unsigned long long)data_checks,(unsigned long long)old_checks,(unsigned long long)command_checks,(unsigned long long)output_checks,(unsigned long long)readonly_checks,(unsigned long long)guard_checks,(unsigned long long)old_base,(unsigned long long)base,passed?"true":"false",cleanup?"true":"false",current_program,current_case,current_repeat,bad_pool,bad_offset,bad_actual,bad_expected,cases,passed?"true":"false");
#undef GUARD
#undef REQUIRE
#undef BOOL
#undef CALL
 return passed&&n>=0&&(size_t)n<capacity?0:-1;
}
