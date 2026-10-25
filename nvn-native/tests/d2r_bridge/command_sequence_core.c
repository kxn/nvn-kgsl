#define _POSIX_C_SOURCE 200809L
#define nn_queue_submit command_ram_unqualified_submit
#include NN_VIEW_MEMORY_BACKEND
#undef nn_queue_submit
static unsigned accept_submit,mock_submits;static uint32_t captured_words[256];static uintptr_t captured_refs[128];static size_t captured_bytes,captured_ref_count;
/* Transport capture only: never interprets PM4 or modifies caller data. */
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
 (void)q;mock_submits++;if(!accept_submit)return ENOTSUP;
 if(bytes>sizeof(captured_words)||count>128||offset>nn_bo_size(ib)||bytes>nn_bo_size(ib)-offset)return EINVAL;
 memcpy(captured_words,(const char *)nn_bo_map(ib)+offset,bytes);captured_bytes=bytes;captured_ref_count=count;
 for(size_t i=0;i<count;i++)captured_refs[i]=(uintptr_t)refs[i];
 if(timestamp)*timestamp=1;
 return 0;
}
#include NN_VIEW_CORE
static unsigned checks,leaves;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define BAD(e) do{T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static struct handle *lookup(NVNcommandHandle id){struct handle *h=handles;while(h&&h->id!=id)h=h->next;return h;}
struct observations {unsigned count;float blend[16],viewport[16];size_t ubo[16],high[16];uint32_t high_flags[16];uint64_t guest[16];const void *owners[16];};
static int visit(void *key,const void *owner,const struct nn_command_leaf *leaf,const struct nn_command_state *s)
{
 struct observations *o=key;T(o->count<16);T(leaf->bytes>0&&leaf->offset<=((const struct handle *)owner)->bytes&&leaf->bytes<=((const struct handle *)owner)->bytes-leaf->offset);
 unsigned i=o->count++;o->blend[i]=s->render.blend_color[0];o->viewport[i]=s->viewport[2];o->owners[i]=owner;
 const struct nn_command_binding *u=nn_command_state_binding(s,NN_COMMAND_UBO,NVN_SHADER_STAGE_FRAGMENT,1);if(u)o->ubo[i]=u->offset;
 const struct nn_command_binding *h=nn_command_state_binding(s,NN_COMMAND_SEPARATE_TEXTURE,NVN_SHADER_STAGE_FRAGMENT,0);if(h){o->high[i]=h->offset;o->high_flags[i]=h->flags;o->guest[i]=h->guest;}
 leaves++;return 0;
}
static void empty_and_state_only(void)
{
 accept_submit=1;mock_submits=0;NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));
 void *ram=NULL;T(!posix_memalign(&ram,4096,8192));memset(ram,0xa5,8192);
 NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool={0};OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&dk));OK(api_nvnMemoryPoolBuilderSetStorage(&pb,ram,8192));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));T(api_nvnMemoryPoolInitialize(&pool,&pb));
 NVNcommandBuffer source={0},parent={0};_Alignas(16)unsigned char sc[512],pc[512];T(api_nvnCommandBufferInitialize(&source,&dk));T(api_nvnCommandBufferInitialize(&parent,&dk));
 OK(api_nvnCommandBufferAddCommandMemory(&source,&pool,0,4096));OK(api_nvnCommandBufferAddControlMemory(&source,sc,sizeof(sc)));
 OK(api_nvnCommandBufferBeginRecording(&source));NVNcommandHandle child[2];child[0]=api_nvnCommandBufferEndRecording(&source);T(child[0]&&!nn_nvn_error());
 OK(api_nvnCommandBufferBeginRecording(&source));const float color[4]={7,6,5,4};OK(api_nvnCommandBufferSetBlendColor(&source,color));child[1]=api_nvnCommandBufferEndRecording(&source);T(child[1]&&!nn_nvn_error());
 OK(api_nvnCommandBufferAddCommandMemory(&parent,&pool,4096,4096));OK(api_nvnCommandBufferAddControlMemory(&parent,pc,sizeof(pc)));OK(api_nvnCommandBufferBeginRecording(&parent));
 OK(api_nvnCommandBufferCallCommands(&parent,2,child));NVNcommandHandle root=api_nvnCommandBufferEndRecording(&parent);T(root&&!nn_nvn_error());
 OK(api_nvnDeviceFinalizeCommandHandle(&dk,child[0]));OK(api_nvnDeviceFinalizeCommandHandle(&dk,child[1]));OK(api_nvnCommandBufferFinalize(&source));
 NVNqueueBuilder qb={0};NVNqueue qk={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&dk));T(api_nvnQueueInitialize(&qk,&qb));
 OK(api_nvnQueueSubmitCommands(&qk,1,&root));struct object *q=find(&qk,QUEUE);T(mock_submits==1&&captured_bytes>0&&q->call_queue_active&&q->call_queue_state);
 T(q->call_queue_state->defined==NN_COMMAND_STATE_BLEND_COLOR&&!memcmp(q->call_queue_state->render.blend_color,color,sizeof(color)));
 OK(api_nvnQueueSubmitCommands(&qk,0,NULL));T(mock_submits==1&&q->call_queue_state->render.blend_color[0]==7);
 OK(api_nvnQueueFinish(&qk));OK(api_nvnDeviceFinalizeCommandHandle(&dk,root));OK(api_nvnCommandBufferFinalize(&parent));OK(api_nvnQueueFinalize(&qk));OK(api_nvnMemoryPoolFinalize(&pool));OK(api_nvnDeviceFinalize(&dk));free(ram);
 while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
}
int main(void)
{
 PFNNVNCOMMANDBUFFERCALLCOMMANDSPROC call=(PFNNVNCOMMANDBUFFERCALLCOMMANDSPROC)nvnBootstrapLoader("nvnCommandBufferCallCommands");T(call);
 NVNdeviceBuilder db={0};NVNdevice device={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&device,&db));
 void *ram=NULL;T(!posix_memalign(&ram,4096,262144));memset(ram,0x5b,262144);NVNmemoryPoolBuilder pb={0};NVNmemoryPool pool={0};OK(api_nvnMemoryPoolBuilderSetDefaults(&pb));OK(api_nvnMemoryPoolBuilderSetDevice(&pb,&device));OK(api_nvnMemoryPoolBuilderSetFlags(&pb,NVN_MEMORY_POOL_FLAGS_CPU_CACHED|NVN_MEMORY_POOL_FLAGS_GPU_CACHED));OK(api_nvnMemoryPoolBuilderSetStorage(&pb,ram,262144));T(api_nvnMemoryPoolInitialize(&pool,&pb));
 NVNcommandBuffer source={0},parent={0},other={0};unsigned char source_control[4096] __attribute__((aligned(8))),parent_control[4096] __attribute__((aligned(8))),other_control[4096] __attribute__((aligned(8)));
 T(api_nvnCommandBufferInitialize(&source,&device));T(api_nvnCommandBufferInitialize(&parent,&device));T(api_nvnCommandBufferInitialize(&other,&device));
 OK(api_nvnCommandBufferAddCommandMemory(&source,&pool,0,4096));OK(api_nvnCommandBufferAddControlMemory(&source,source_control,sizeof(source_control)));
 OK(api_nvnCommandBufferAddCommandMemory(&parent,&pool,4096,4096));OK(api_nvnCommandBufferAddControlMemory(&parent,parent_control,sizeof(parent_control)));
 OK(api_nvnCommandBufferAddCommandMemory(&other,&pool,8192,4096));OK(api_nvnCommandBufferAddControlMemory(&other,other_control,sizeof(other_control)));
 const float c1[4]={1,0,0,0},c2[4]={2,0,0,0},c3[4]={3,0,0,0};NVNbufferAddress address=api_nvnMemoryPoolGetBufferAddress(&pool)+16384;
 OK(api_nvnCommandBufferBeginRecording(&source));OK(api_nvnCommandBufferSetBlendColor(&source,c2));OK(api_nvnCommandBufferBindUniformBuffer(&source,NVN_SHADER_STAGE_FRAGMENT,1,address+4096,64));OK(api_nvnCommandBufferBindSeparateTexture(&source,NVN_SHADER_STAGE_FRAGMENT,0,(NVNseparateTextureHandle){257}));OK(api_nvnCommandBufferClearBuffer(&source,address,4,0x11111111));OK(api_nvnCommandBufferSetBlendColor(&source,c3));NVNcommandHandle child=api_nvnCommandBufferEndRecording(&source);T(child);struct handle *body=lookup(child);T(body&&body->call_sequence&&!body->call_references);
 OK(api_nvnCommandBufferBeginRecording(&parent));OK(api_nvnCommandBufferSetViewport(&parent,0,0,64,64));OK(api_nvnCommandBufferSetBlendColor(&parent,c1));OK(api_nvnCommandBufferBindUniformBuffer(&parent,NVN_SHADER_STAGE_FRAGMENT,1,address+4160,64));NVNseparateTextureHandle full=api_nvnDeviceGetSeparateTextureHandle(&device,257);OK(api_nvnCommandBufferBindSeparateTextures(&parent,NVN_SHADER_STAGE_FRAGMENT,0,1,&full));NVNcommandHandle invalid[2]={child,UINT64_MAX};call(&parent,-1,NULL);BAD(EINVAL);call(&parent,1,NULL);BAD(EINVAL);call(&parent,2,invalid);BAD(EINVAL);T(!body->call_references);OK(call(&parent,0,NULL));OK(call(&parent,1,&child));T(body->call_references==1);OK(api_nvnCommandBufferClearBuffer(&parent,address+4,4,0x22222222));
 /* Equal-to-current CPU snapshot is still an explicit GPU write: c1 must
  * override c3 inherited from the Call, so binary before/after diff is wrong. */
 OK(api_nvnCommandBufferSetBlendColor(&parent,c1));OK(api_nvnCommandBufferClearBuffer(&parent,address+8,4,0x33333333));NVNcommandHandle top=api_nvnCommandBufferEndRecording(&parent);T(top);struct handle *root=lookup(top);T(root&&root->call_nodes);
 OK(api_nvnDeviceFinalizeCommandHandle(&device,child));T(lookup(child)==body&&body->public_retired&&body->call_references==1);OK(api_nvnCommandBufferFinalize(&source));
 struct observations observed={0};struct nn_command_state final;size_t native_count=0;T(!nn_command_sequence_visit(root->call_sequence,(1u<<NN_COMMAND_RAW)|(1u<<NN_COMMAND_COPY),NULL,visit,&observed,&final,&native_count));T(native_count==4&&observed.count==4);T(observed.owners[0]==body&&observed.blend[0]==2&&observed.viewport[0]==64);T(observed.owners[1]==body&&observed.blend[1]==3&&observed.viewport[1]==64);T(observed.owners[2]==root&&observed.blend[2]==3);T(observed.owners[3]==root&&observed.blend[3]==1);T(final.defined==(NN_COMMAND_STATE_VIEWPORT|NN_COMMAND_STATE_BLEND_COLOR));for(unsigned i=0;i<4;i++){T(observed.ubo[i]==20480);T(observed.high[i]==1&&observed.high_flags[i]==NN_COMMAND_BINDING_HIGH_DEFINED&&observed.guest[i]==257);}
 /* Real queue rejects referenced work before pinning/materialization until
  * lowering is implemented. Public retired source cannot be submitted/copied. */
 NVNqueueBuilder qb={0};NVNqueue queue={0};OK(api_nvnQueueBuilderSetDefaults(&qb));OK(api_nvnQueueBuilderSetDevice(&qb,&device));T(api_nvnQueueInitialize(&queue,&qb));unsigned bos=find(&device,DEVICE)->gpu->bos;
 accept_submit=1;OK(api_nvnQueueSubmitCommands(&queue,1,&top));T(mock_submits==1&&captured_bytes==64&&find(&device,DEVICE)->gpu->bos==bos);
 uint32_t expected[16];unsigned at=0;
 for(unsigned i=0;i<3;i++){
  uint64_t dst=address+4*i;expected[at++]=nvn_pm4_packet7(NVN_CP_MEM_WRITE,3);expected[at++]=(uint32_t)dst;expected[at++]=(uint32_t)(dst>>32);expected[at++]=(i+1)*UINT32_C(0x11111111);
  if(!i||i==2){expected[at++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);expected[at++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0);}
 }
 T(at==16&&!memcmp(expected,captured_words,64));unsigned has_pool=0;for(size_t i=0;i<captured_ref_count;i++)has_pool|=captured_refs[i]==(uintptr_t)find(&pool,POOL)->bo;T(has_pool);for(unsigned i=0;i<12;i++)T(((unsigned char *)ram)[16384+i]==0x5b);
 struct object *qo=find(&queue,QUEUE);T(qo->call_queue_active&&!qo->call_queue_poisoned&&qo->call_queue_state);T(qo->call_queue_state->render.blend_color[0]==1);T(nn_command_state_binding(qo->call_queue_state,NN_COMMAND_UBO,NVN_SHADER_STAGE_FRAGMENT,1)->offset==20480);
 accept_submit=0;api_nvnQueueSubmitCommands(&queue,1,&top);BAD(ENOTSUP);T(mock_submits==2&&qo->call_queue_poisoned&&find(&device,DEVICE)->gpu->bos==bos);api_nvnQueueSubmitCommands(&queue,1,&child);BAD(EINVAL);
 OK(api_nvnCommandBufferBeginRecording(&other));size_t used=find(&other,COMMAND)->record->used;api_nvnCommandBufferCopyCommands(&other,1,&top);BAD(ENOTSUP);T(find(&other,COMMAND)->record->used==used);api_nvnCommandBufferCopyCommands(&other,1,&child);BAD(EINVAL);T(find(&other,COMMAND)->record->used==used);
 /* An unmodeled GPU-state family is explicit, never silently omitted. */
 OK(api_nvnCommandBufferSetShaderScratchMemory(&other,&pool,16384,16));NVNcommandHandle unsupported=api_nvnCommandBufferEndRecording(&other);T(unsupported);observed=(struct observations){0};T(nn_command_sequence_visit(lookup(unsupported)->call_sequence,NN_COMMAND_LEAF_KIND_BITS,NULL,visit,&observed,&final,&native_count)==ENOTSUP);T(!observed.count);
 OK(api_nvnDeviceFinalizeCommandHandle(&device,unsupported));OK(api_nvnDeviceFinalizeCommandHandle(&device,top));T(!handles);OK(api_nvnCommandBufferFinalize(&other));OK(api_nvnCommandBufferFinalize(&parent));OK(api_nvnMemoryPoolFinalize(&pool));T(!nn_command_state_binding(qo->call_queue_state,NN_COMMAND_UBO,NVN_SHADER_STAGE_FRAGMENT,1));OK(api_nvnQueueFinalize(&queue));OK(api_nvnDeviceFinalize(&device));free(ram);while(objects){T(builder_kind(objects->kind));remove_object(objects);}T(!handles);
 empty_and_state_only();
 printf("{\"passed\":true,\"checks\":%u,\"actual_handle_span_visits\":%u,\"GPU_execution\":false,\"NVN_CallCommands_RAW_genericDraw_profile\":true}\n",checks,leaves);return 0;
}
