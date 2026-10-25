/* HOST copy catalogue: original normal public-copy setup, real NVN Call and
 * native encoders. The RAM queue never executes CP, shader, or image work. */
#define main previous_public_copy_fixture_main
#include "public_copy_probe_failure.c"
#undef main
#include NN_PUBLIC_CORE_SOURCE
static unsigned call_cases,inspected,call_replays;
static int call_admission(void)
{
 struct npc_owner o={0};CK(!api_load(&o.api));struct npc_api *p=&o.api;
 /* Reuse this fixture's operand/setup helper only. Its CopyCommands-shaped
  * callback is explicitly bound to the independently typed public Call ABI.
  * No CopyCommands success/count claim is made by this HOST test. */
 PFNNVNCOMMANDBUFFERCALLCOMMANDSPROC call=(PFNNVNCOMMANDBUFFERCALLCOMMANDSPROC)nvnBootstrapLoader("nvnCommandBufferCallCommands");CK(call);
 p->nvnCommandBufferCopyCommands=call;
 NVNdeviceBuilder db={0};NVNqueueBuilder qb={0};nn_nvn_clear_error();fault=7;
 p->nvnDeviceBuilderSetDefaults(&db);CK(p->nvnDeviceInitialize(&o.device,&db));o.device_ready=1;CK(!NN_PUBLIC_COPY_REGISTER(&o.device));
 p->nvnQueueBuilderSetDefaults(&qb);p->nvnQueueBuilderSetDevice(&qb,&o.device);CK(p->nvnQueueInitialize(&o.queue,&qb));o.queue_ready=1;
 for(unsigned n=0;n<NN_PUBLIC_COPY_CASES;n++){
  const struct nn_public_copy_case *c=nn_public_copy_cases+n;o.active=calloc(1,sizeof(*o.active));CK(o.active);unsigned calls=0,pins=0;
  CK(!pool_create(&o,0,c->pool_bytes[0]));if(c->pool_bytes[1])CK(!pool_create(&o,1,c->pool_bytes[1]));CK(!pool_create(&o,2,65536));
  CK(!texture_create(&o,c,0,0,c->canonical_offset));if(c->family==2)CK(!texture_create(&o,c,1,c->shared?0:1,c->destination_offset));
  CK(!buffer_create(&o,0,0,c->canonical_offset,c->source.payload_bytes));CK(!buffer_create(&o,1,c->shared?0:1,c->family==2?c->destination_offset:c->linear_offset,c->family==2?c->source.payload_bytes:c->linear_bytes));
  int e=record_copy(&o,c,&calls,&pins);if(e)fprintf(stderr,"Call record case%u error%d\n",n,e);CK(!e&&calls==1&&pins==(c->family==2?2u:1u));
  struct handle *parent=handles,*source=NULL;while(parent&&parent->id!=o.active->handles[1])parent=parent->next;
  for(struct handle *h=handles;h;h=h->next)if(h->copies){CK(!source);source=h;}
  CK(parent&&parent->call_nodes&&!parent->copies&&source&&source->public_retired&&source->call_references);
  CK(source->copies&&!source->copies->next&&source->copies->job_count==(c->family==2?6u:3u));
  CK(source->copies->row_stride==(ptrdiff_t)(c->variant?c->row_stride:0)&&source->copies->image_stride==(ptrdiff_t)(c->variant?c->image_stride:0));
  for(unsigned replay=0;replay<2;replay++){
   struct nn_command_state empty={0},final={0};struct handle *projected=NULL;struct object *queue=find(&o.queue,QUEUE);
   /* A private array/volume CS cannot become a public NVN UBO/SSBO write.
    * These actual pool owners are explicit inherited state for this visit. */
   unsigned uniform_index=0,storage_index=0;CK(!nn_command_binding_index(NN_COMMAND_UBO,NVN_SHADER_STAGE_COMPUTE,0,&uniform_index));CK(!nn_command_binding_index(NN_COMMAND_SSBO,NVN_SHADER_STAGE_COMPUTE,0,&storage_index));
   struct object *pool_owner=find(o.active->pools,POOL);empty.bindings[uniform_index]=command_binding(pool_owner,0,64,nn_bo_address(pool_owner->bo));empty.bindings[storage_index]=command_binding(pool_owner,64,64,nn_bo_address(pool_owner->bo)+64);
   empty.binding_defined[uniform_index/64]|=UINT64_C(1)<<(uniform_index%64);empty.binding_defined[storage_index/64]|=UINT64_C(1)<<(storage_index%64);
   e=command_project_sequence(queue,parent,&empty,&final,&projected);if(e)fprintf(stderr,"Call project case%u error%d\n",n,e);CK(!e&&projected&&final.defined==0);CK(!memcmp(&empty,&final,sizeof(empty)));
   unsigned nodes=0,patches=0;struct job *fresh[8]={0};
   for(struct job *j=projected->jobs;j;j=j->next){CK(nodes<8);fresh[nodes++]=j;patches+=j->projected_dispatch==2;
    for(struct job *old=source->jobs;old;old=old->next)CK(j->bo!=old->bo&&nn_bo_address(j->bo)!=nn_bo_address(old->bo));
   }
   CK(nodes==(c->family==2?7u:4u)&&patches==1);
   CK(!memcmp(nn_bo_map(fresh[0]->bo),NVN_COMPILED_IMAGE.code,NVN_COMPILED_IMAGE.code_bytes));
   for(unsigned u=0;u<(c->family==2?2u:1u);u++)CK(!memcmp(nn_bo_map(fresh[1+u]->bo),nn_bo_map(source->copies->jobs[1+u]->bo),64));
   void *staged=NULL;CK(!queue_stage_commands(queue,projected,&staged));
   const uint32_t *words=(const uint32_t*)((const char*)staged+source->copies->command_offset);
   for(unsigned i=0;i<(c->family==2?2u:1u);i++){
    uint64_t actual=words[4*i+1]|(uint64_t)words[4*i+2]<<32;CK(words[4*i]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3));
    CK(actual==nn_bo_address(fresh[c->family==2?4+i:2]->bo));
   }
   command_body_destroy(projected);inspected++;
   p->nvnQueueSubmitCommands(&o.queue,1,o.active->handles+1);if(nn_nvn_error())fprintf(stderr,"Call submit case%u error%d\n",n,nn_nvn_error());CK(!nn_nvn_error());
   p->nvnQueueFinish(&o.queue);CK(!nn_nvn_error());call_replays++;
   for(unsigned i=0;i<3;i++)if(o.active->pools_ready[i])CK(!memcmp(o.active->memory[i],o.active->expected[i],o.active->sizes[i]));
   for(unsigned i=0;i<2;i++)CK(!memcmp(o.active->control[i],o.active->control_expected[i],32768));
  }
  CK(!destroy_active(&o));call_cases++;
 }
 p->nvnQueueFinalize(&o.queue);CK(!nn_nvn_error());p->nvnDeviceFinalize(&o.device);CK(!nn_nvn_error());
 while(objects){CK(builder_kind(objects->kind));remove_object(objects);}CK(!handles);return 0;
}
int main(void)
{
 CK(!call_admission());printf("{\"passed\":true,\"checks\":%u,\"Call_copy_cases\":%u,\"fresh_job_groups_inspected\":%u,\"Call_replays\":%u,\"GPU_execution\":false,\"CPU_shader_execution\":false}\n",checks,call_cases,inspected,call_replays);return 0;
}
