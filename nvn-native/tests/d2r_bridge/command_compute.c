/* HOST actual native compute jobs/transport ownership only; no shader or PM4
 * is executed by this RAM test. Normal four-original cache is external. */
#define _POSIX_C_SOURCE 200809L
#define nn_queue_submit compute_ram_unqualified_submit
#include NN_COMPUTE_MEMORY_BACKEND
#undef nn_queue_submit
#include <sys/mman.h>
static unsigned submits,submit_error;static uint32_t submitted[256];static size_t submitted_bytes;
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
 if(!q||!ib||offset>ib->bytes||bytes>ib->bytes-offset||bytes>sizeof(submitted)||count>127)return EINVAL;
 nn_bo *pending[128];size_t used=1;pending[0]=ib;
 for(size_t i=0;i<count;i++){
  if(!refs[i]||refs[i]->device!=q->device)return EINVAL;
  size_t j=0;while(j<used&&pending[j]!=refs[i])j++;if(j==used)pending[used++]=refs[i];
 }
 if(used>64-q->count)return ENOSPC;
 for(size_t i=0;i<used;i++){pending[i]->busy++;q->pending[q->count++]=pending[i];}
 memcpy(submitted,(const char*)nn_bo_map(ib)+offset,bytes);submitted_bytes=bytes;submits++;if(timestamp)*timestamp=submits;return submit_error;
}
#include NN_COMPUTE_CORE
#include NN_COMPUTE_CACHE_HEADER
static unsigned checks,cases,encoded,indirect_cases;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(x,e) do{x;T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static struct handle *lookup(NVNcommandHandle id){struct handle *h=handles;while(h&&(h->id!=id||h->public_retired))h=h->next;return h;}
static uint64_t pair64(const uint32_t *w){return w[0]|(uint64_t)w[1]<<32;}
static void verify_job(const struct job *job,struct object *program,uint64_t uniform,uint64_t source,uint64_t target,const uint32_t groups[3],uint64_t indirect)
{
 T(job&&job->compute&&job->projected_dispatch&&job->bo);
 const struct nn_cs_image *image=program->compiled->image;const uint32_t *words=nn_bo_map(job->bo);unsigned seen_code=0,seen_uniform=0,seen_groups=0;
 for(size_t i=0;i<image->patch_count;i++){
  const struct nn_cs_patch *p=image->patches+i;
  if(p->kind==NN_CS_CODE){T(pair64(words+p->word)==nn_bo_address(program->bo));seen_code++;}
  if(p->kind==NN_CS_UBO){T(pair64(words+p->word)==uniform+p->offset);seen_uniform++;}
  if(!indirect&&(p->kind==NN_CS_GROUP_COUNT||p->kind==NN_CS_GLOBAL_SIZE)){
   T(words[p->word]==groups[p->index]*(p->kind==NN_CS_GLOBAL_SIZE?image->local[p->index]:1));seen_groups++;
  }
 }
 T(seen_code&&seen_uniform);if(!indirect)T(seen_groups);
 T(program->compiled->sbuf_count==2);
 for(unsigned i=0;i<2;i++){
  const struct nn_nvn_sbuf *map=program->compiled->sbufs+i;unsigned slot=(map->cbuf_word-0x310/4)/4;
  const uint32_t *descriptor=words+1024+map->binding*16;uint64_t address=slot?target:source;
  T(pair64(descriptor+4)==(address&~UINT64_C(63)));
  T(((descriptor[2]&image->offset_mask)>>image->offset_shift)==(address&63)/4);
 }
 if(indirect){
  unsigned found=0;for(size_t at=0;at<job->command_bytes/4;){size_t n=nvn_pm4_packet_words(words[at]);T(n&&n<=job->command_bytes/4-at);
   if(words[at]==nvn_pm4_packet7(0x41,4)){T(pair64(words+at+2)==indirect);found++;}at+=n;
  }T(found==1);
 }
 encoded++;
}
static void one(unsigned index,unsigned indirect)
{
 submits=submit_error=0;NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *device=find(&dk,DEVICE);
 enum{CODE,OLD,NEW,ARENA,ARGS,POOLS};NVNmemoryPoolBuilder pb[POOLS]={0};NVNmemoryPool pool[POOLS]={0};void *ram[POOLS]={0};
 for(unsigned i=0;i<POOLS;i++){
  size_t bytes=i==ARGS?4096:16384;T(!posix_memalign(ram+i,4096,bytes));memset(ram[i],0x5b,bytes);
  OK(api_nvnMemoryPoolBuilderSetDefaults(pb+i));OK(api_nvnMemoryPoolBuilderSetDevice(pb+i,&dk));OK(api_nvnMemoryPoolBuilderSetStorage(pb+i,ram[i],bytes));
  OK(api_nvnMemoryPoolBuilderSetFlags(pb+i,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED|(i==CODE?NVN_MEMORY_POOL_FLAGS_SHADER_CODE:0)));T(api_nvnMemoryPoolInitialize(pool+i,pb+i));
 }
 NVNprogram pk[4]={0};T(NN_OFFLINE_COMPUTE_COUNT==4);
 for(unsigned i=0;i<4;i++){
  const struct nn_nvn_compiled_cs *entry=nn_offline_compute_entries[i];T(!nn_nvn_register_compute(&dk,entry));T(entry->source_bytes<=512);
  memcpy((char*)ram[CODE]+512*i,entry->source_code,entry->source_bytes);NVNshaderData shader={api_nvnMemoryPoolGetBufferAddress(pool+CODE)+512*i,entry->source_control};
  T(api_nvnProgramInitialize(pk+i,&dk));T(api_nvnProgramSetShaders(pk+i,1,&shader));
 }
 OK(api_nvnMemoryPoolFinalize(pool+CODE));free(ram[CODE]);ram[CODE]=NULL;
 uint64_t old=api_nvnMemoryPoolGetBufferAddress(pool+OLD),current=api_nvnMemoryPoolGetBufferAddress(pool+NEW),argument=api_nvnMemoryPoolGetBufferAddress(pool+ARGS)+64;
 T(!mprotect(ram[ARGS],4096,PROT_NONE));
 NVNcommandBuffer child={0},parent={0};_Alignas(16)unsigned char cc[4096],pc[4096];T(api_nvnCommandBufferInitialize(&child,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&child,pool+ARENA,0,8192));OK(api_nvnCommandBufferAddControlMemory(&child,cc,sizeof(cc)));OK(api_nvnCommandBufferBeginRecording(&child));
 OK(api_nvnCommandBufferBindProgram(&child,pk+index,NVN_SHADER_STAGE_COMPUTE_BIT));
 OK(api_nvnCommandBufferBindUniformBuffer(&child,NVN_SHADER_STAGE_COMPUTE,0,old+128,64));
 OK(api_nvnCommandBufferBindStorageBuffer(&child,NVN_SHADER_STAGE_COMPUTE,0,old+256,4096));OK(api_nvnCommandBufferBindStorageBuffer(&child,NVN_SHADER_STAGE_COMPUTE,1,old+8192,2048));
 /* Preserve the original ordinary recorder's finite validation stage. */
 size_t ordinary_used=find(&child,COMMAND)->record->used;
 BAD(api_nvnCommandBufferBindUniformBuffer(&child,NVN_SHADER_STAGE_COMPUTE,14,old+128,64),EINVAL);
 NVNbufferRange invalid_ranges[2]={{old+8192,2048},{UINT64_MAX-3,4}};
 BAD(api_nvnCommandBufferBindStorageBuffers(&child,NVN_SHADER_STAGE_COMPUTE,0,2,invalid_ranges),EINVAL);
 OK(api_nvnCommandBufferBindStorageBuffer(&child,NVN_SHADER_STAGE_COMPUTE,1,0,0));
 BAD(api_nvnCommandBufferDispatchCompute(&child,1,1,1),EINVAL);T(find(&child,COMMAND)->record->used==ordinary_used&&!find(&child,COMMAND)->record->current->dispatches);
 OK(api_nvnCommandBufferBindStorageBuffer(&child,NVN_SHADER_STAGE_COMPUTE,1,old+8192,2048));
 NVNcommandHandle seed=api_nvnCommandBufferEndRecording(&child);T(seed&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&dk,seed));
 /* The same recorder retains the original finite encoder's CPU prerequisites.
  * This child sequence itself writes no Program/UBO/SSBO GPU bindings. */
 OK(api_nvnCommandBufferBeginRecording(&child));
#ifdef NN_CALL_COPY_COMPUTE_MIXED
 OK(api_nvnCommandBufferCopyBufferToBuffer(&child,current+256,current+8192,64,0));
#endif
 if(indirect)OK(api_nvnCommandBufferDispatchComputeIndirect(&child,argument));else OK(api_nvnCommandBufferDispatchCompute(&child,1,2,3));
 struct command_dispatch *operation=find(&child,COMMAND)->record->current->dispatches;T(operation&&operation->recorded_job);
 uint64_t original_job=nn_bo_address(operation->recorded_job->bo);
 NVNcommandHandle source=api_nvnCommandBufferEndRecording(&child);T(source&&!nn_nvn_error());
 T(api_nvnCommandBufferInitialize(&parent,&dk));OK(api_nvnCommandBufferAddCommandMemory(&parent,pool+ARENA,8192,8192));OK(api_nvnCommandBufferAddControlMemory(&parent,pc,sizeof(pc)));OK(api_nvnCommandBufferBeginRecording(&parent));
 unsigned selected=(index+1)%4;struct object *program=find(pk+selected,PROGRAM);
 OK(api_nvnCommandBufferBindProgram(&parent,pk+selected,NVN_SHADER_STAGE_COMPUTE_BIT));
 if(index&1){
  NVNbufferRange uniform={current+128,64},storage[2]={{current+256,4096},{current+8192,2048}};
  OK(api_nvnCommandBufferBindUniformBuffers(&parent,NVN_SHADER_STAGE_COMPUTE,0,1,&uniform));
  OK(api_nvnCommandBufferBindStorageBuffers(&parent,NVN_SHADER_STAGE_COMPUTE,0,2,storage));
 }else{
  OK(api_nvnCommandBufferBindUniformBuffer(&parent,NVN_SHADER_STAGE_COMPUTE,0,current+128,64));
  OK(api_nvnCommandBufferBindStorageBuffer(&parent,NVN_SHADER_STAGE_COMPUTE,0,current+256,4096));OK(api_nvnCommandBufferBindStorageBuffer(&parent,NVN_SHADER_STAGE_COMPUTE,1,current+8192,2048));
 }
 OK(api_nvnCommandBufferCallCommands(&parent,1,&source));
#ifdef NN_CALL_COPY_COMPUTE_MIXED
 OK(api_nvnCommandBufferCopyBufferToBuffer(&parent,current+256,current+8192,64,0));
#endif
 OK(api_nvnCommandBufferBindUniformBuffer(&parent,NVN_SHADER_STAGE_COMPUTE,0,current+384,64));
 if(indirect)OK(api_nvnCommandBufferDispatchComputeIndirect(&parent,argument));else OK(api_nvnCommandBufferDispatchCompute(&parent,4,5,6));
 NVNcommandHandle top=api_nvnCommandBufferEndRecording(&parent);T(top&&!nn_nvn_error());
 unsigned char before[16384];memcpy(before,ram[ARENA],sizeof(before));OK(api_nvnDeviceFinalizeCommandHandle(&dk,source));OK(api_nvnCommandBufferFinalize(&child));
 NVNqueueBuilder qb={0};NVNqueue qk={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&qk,&qb));struct object *q=find(&qk,QUEUE);
 /* An ordinary CopyCommands body still has no semantic Call graph. Its
  * borrowed jobs must remain ENOTSUP rather than raw-passthrough. */
 NVNcommandBuffer copy={0},copy_parent={0};_Alignas(16)unsigned char copy_control[128],copy_parent_control[128];
 T(api_nvnCommandBufferInitialize(&copy,&dk));T(api_nvnCommandBufferInitialize(&copy_parent,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&copy,pool+ARENA,4096,1024));OK(api_nvnCommandBufferAddControlMemory(&copy,copy_control,sizeof(copy_control)));OK(api_nvnCommandBufferBeginRecording(&copy));
 OK(api_nvnCommandBufferCopyBufferToBuffer(&copy,current+256,current+8192,64,0));NVNcommandHandle original_copy=api_nvnCommandBufferEndRecording(&copy);T(original_copy&&!nn_nvn_error());
 OK(api_nvnCommandBufferBeginRecording(&copy));OK(api_nvnCommandBufferCopyCommands(&copy,1,&original_copy));NVNcommandHandle copy_h=api_nvnCommandBufferEndRecording(&copy);T(copy_h&&!nn_nvn_error());OK(api_nvnDeviceFinalizeCommandHandle(&dk,original_copy));
 OK(api_nvnCommandBufferAddCommandMemory(&copy_parent,pool+ARENA,12288,1024));OK(api_nvnCommandBufferAddControlMemory(&copy_parent,copy_parent_control,sizeof(copy_parent_control)));OK(api_nvnCommandBufferBeginRecording(&copy_parent));
 OK(api_nvnCommandBufferCallCommands(&copy_parent,1,&copy_h));NVNcommandHandle copy_call=api_nvnCommandBufferEndRecording(&copy_parent);T(copy_call&&!nn_nvn_error());
 unsigned before_copy_bos=device->gpu->bos;BAD(api_nvnQueueSubmitCommands(&qk,1,&copy_call),ENOTSUP);T(!submits&&device->gpu->bos==before_copy_bos);
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,copy_call));OK(api_nvnDeviceFinalizeCommandHandle(&dk,copy_h));OK(api_nvnCommandBufferFinalize(&copy_parent));OK(api_nvnCommandBufferFinalize(&copy));
 memcpy(before,ram[ARENA],sizeof(before));
 struct nn_command_state state={0},final={0};struct handle *projected=NULL;T(!command_project_sequence(q,lookup(top),&state,&final,&projected));
#ifdef NN_CALL_COPY_COMPUTE_MIXED
 const size_t first_dispatch=16,second_dispatch=56,expected_bytes=80;
#else
 const size_t first_dispatch=0,second_dispatch=24,expected_bytes=48;
#endif
 unsigned jobs=0;for(const struct job *job=projected->jobs;job;job=job->next){
#ifdef NN_CALL_COPY_COMPUTE_MIXED
  if(job->projected_dispatch!=1)continue;
#endif
  T(nn_bo_address(job->bo)!=original_job);T(job->command_offset==first_dispatch||job->command_offset==second_dispatch);
  uint32_t groups[3];if(job->command_offset==first_dispatch){groups[0]=1;groups[1]=2;groups[2]=3;}else{groups[0]=4;groups[1]=5;groups[2]=6;}
  verify_job(job,program,current+(job->command_offset==first_dispatch?128:384),current+256,current+8192,groups,indirect?argument:0);jobs++;
 }T(jobs==2&&projected->bytes==expected_bytes);command_body_destroy(projected);
 OK(api_nvnQueueSubmitCommands(&qk,1,&top));T(submits==1&&submitted_bytes==expected_bytes);
 T(submitted[first_dispatch/4]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3)&&pair64(submitted+first_dispatch/4+1)!=original_job);
 T(submitted[second_dispatch/4]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3)&&pair64(submitted+second_dispatch/4+1)!=original_job);
 T(!memcmp(before,ram[ARENA],sizeof(before)));for(unsigned i=0;i<16384;i++)T(((unsigned char*)ram[NEW])[i]==0x5b);
 submit_error=EIO;BAD(api_nvnQueueSubmitCommands(&qk,1,&top),EIO);submit_error=0;T(submits==2&&q->call_queue_poisoned&&device->retired);
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,top));OK(api_nvnCommandBufferFinalize(&parent));BAD(api_nvnProgramFinalize(pk+selected),EBUSY);
 OK(api_nvnQueueFinish(&qk));T(!device->retired);OK(api_nvnQueueFinalize(&qk));for(unsigned i=0;i<4;i++)OK(api_nvnProgramFinalize(pk+i));
 T(!mprotect(ram[ARGS],4096,PROT_READ|PROT_WRITE));for(unsigned i=1;i<POOLS;i++){OK(api_nvnMemoryPoolFinalize(pool+i));free(ram[i]);}
 T(!device->gpu->bos);OK(api_nvnDeviceFinalize(&dk));while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);cases++;indirect_cases+=indirect;
}
int main(void){for(unsigned i=0;i<4;i++)for(unsigned indirect=0;indirect<2;indirect++)one(i,indirect);printf("{\"passed\":true,\"checks\":%u,\"four_original_compute_cases\":%u,\"indirect_cases\":%u,\"inspected_rebuilt_jobs\":%u,\"GPU_execution\":false,\"CPU_shader_execution\":false}\n",checks,cases,indirect_cases,encoded);return 0;}
