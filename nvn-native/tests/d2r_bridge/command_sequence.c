/* Native GPU plan metadata only. No PM4, CPU interpreter or GPU execution. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static int fail_alloc;
static void *test_calloc(size_t count,size_t bytes){if(fail_alloc){fail_alloc=0;return NULL;}return calloc(count,bytes);}
#define calloc test_calloc
#include NN_COMMAND_SOURCE
#undef calloc
static unsigned checks,alive,freed,retains,releases;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
struct owner {struct nn_command_sequence *sequence;unsigned refs,deny;size_t bytes;};
static void owner_drop(void *key);
static int owner_hold(void *key){struct owner *o=key;if(o->deny)return EBUSY;if(o->refs==UINT32_MAX)return EOVERFLOW;o->refs++;retains++;return 0;}
static int owner_span(const void *key,size_t offset,size_t bytes,unsigned kind){const struct owner *o=key;(void)kind;return !o->refs||offset>o->bytes||bytes>o->bytes-offset?EINVAL:0;}
static const struct nn_command_owner_ops ops={owner_hold,owner_drop,owner_span};
static int domain,other_domain;
static struct owner *make_owner(const void *d)
{struct owner *o=calloc(1,sizeof(*o));T(o);o->refs=1;o->bytes=4096;T(!nn_command_sequence_create(o,&ops,d,&o->sequence));alive++;return o;}
static void owner_drop(void *key){struct owner *o=key;T(o->refs);releases++;if(--o->refs)return;nn_command_sequence_destroy(o->sequence);alive--;freed++;free(o);}
struct observed {uint64_t ids[16];uint32_t colors[16];float viewport[16];unsigned count,fail;};
static int observe(void *key,const void *owner,const struct nn_command_leaf *leaf,const struct nn_command_state *s)
{struct observed *out=key;T(owner);T(out->count<16);unsigned i=out->count++;out->ids[i]=leaf->semantic_id;out->colors[i]=s->render.color_mask;out->viewport[i]=s->viewport[0];if(out->fail==out->count)return EIO;return 0;}
static void state(struct owner *o,uint64_t mask,uint32_t color,float viewport)
{struct nn_command_state s={0};s.defined=NN_COMMAND_STATE_BITS;s.render.color_mask=color;s.viewport[0]=viewport;T(!nn_command_sequence_state(o->sequence,mask,&s));memset(&s,0xdd,sizeof(s));}
static void leaf(struct owner *o,uint64_t id,uint64_t required,unsigned kind)
{struct nn_command_leaf n={.offset=(size_t)id*4,.bytes=4,.required_state=required,.semantic_id=id,.kind=kind};T(!nn_command_sequence_native(o->sequence,&n));}
static void copy_flat(void)
{
 struct owner *src=make_owner(&domain),*dst=make_owner(&domain),*asset=make_owner(&domain);
 struct nn_command_reference_ops resource_ops={owner_hold,owner_drop};
 struct nn_command_binding binding={.owner=asset,.ops=&resource_ops,.address=0x123400,.offset=24,.bytes=80};
 state(src,NN_COMMAND_STATE_MASK,5,0);T(!nn_command_sequence_bindings(src->sequence,NN_COMMAND_UBO,4,2,1,&binding));
 leaf(src,2,NN_COMMAND_STATE_MASK,NN_COMMAND_DRAW);T(!nn_command_sequence_seal(src->sequence));
 const struct nn_command_sequence *sources[]={src->sequence};size_t adjustments[]={64};
 struct command_sequence_node *tail=dst->sequence->tail;unsigned refs=asset->refs;
 fail_alloc=1;T(sequence_copy_flat(dst->sequence,1,sources,adjustments)==ENOMEM);T(dst->sequence->tail==tail&&asset->refs==refs);
 asset->deny=1;T(sequence_copy_flat(dst->sequence,1,sources,adjustments)==EBUSY);T(!dst->sequence->count&&asset->refs==refs);asset->deny=0;
 adjustments[0]=SIZE_MAX;T(sequence_copy_flat(dst->sequence,1,sources,adjustments)==EOVERFLOW);T(!dst->sequence->count);
 adjustments[0]=4096;T(sequence_copy_flat(dst->sequence,1,sources,adjustments)==EINVAL);T(!dst->sequence->count);
 adjustments[0]=64;T(!sequence_copy_flat(dst->sequence,1,sources,adjustments));T(dst->sequence->count==3&&dst->sequence->tail->u.native.offset==72&&asset->refs==refs+1);
 unsigned freed_before=freed;owner_drop(src);T(freed==freed_before+1); /* Copy does not retain the source owner. */
 T(asset->refs==2);owner_drop(asset);T(!nn_command_sequence_seal(dst->sequence));
 struct observed observed={0};struct nn_command_state final;size_t count=0;
 T(!nn_command_sequence_visit(dst->sequence,1u<<NN_COMMAND_DRAW,NULL,observe,&observed,&final,&count));
 const struct nn_command_binding *got=nn_command_state_binding(&final,NN_COMMAND_UBO,4,2);
 T(count==1&&observed.colors[0]==5&&got&&got->owner==asset&&got->address==0x123400&&got->offset==24&&got->bytes==80);
 owner_drop(dst);T(!alive);
 src=make_owner(&domain);dst=make_owner(&domain);struct owner *child=make_owner(&domain);
 leaf(child,1,0,NN_COMMAND_RAW);T(!nn_command_sequence_seal(child->sequence));T(!nn_command_sequence_call(src->sequence,child->sequence));T(!nn_command_sequence_seal(src->sequence));
 sources[0]=src->sequence;T(sequence_copy_flat(dst->sequence,1,sources,adjustments)==ENOTSUP&&dst->sequence->count==0);
 owner_drop(src);owner_drop(child);owner_drop(dst);T(!alive);
 src=make_owner(&domain);dst=make_owner(&domain);T(!nn_command_sequence_unsupported(src->sequence));T(!nn_command_sequence_seal(src->sequence));sources[0]=src->sequence;
 T(!sequence_copy_flat(dst->sequence,1,sources,adjustments));owner_drop(src);T(!nn_command_sequence_seal(dst->sequence));
 T(nn_command_sequence_visit(dst->sequence,NN_COMMAND_LEAF_KIND_BITS,NULL,NULL,NULL,&final,&count)==ENOTSUP);
 owner_drop(dst);T(!alive);
}
int main(void)
{
 copy_flat();
 struct owner *child=make_owner(&domain),*root=make_owner(&domain);
 state(child,NN_COMMAND_STATE_MASK,2,99);leaf(child,2,NN_COMMAND_STATE_MASK|NN_COMMAND_STATE_VIEWPORT,NN_COMMAND_DRAW);T(!nn_command_sequence_seal(child->sequence));
 state(root,NN_COMMAND_STATE_MASK|NN_COMMAND_STATE_VIEWPORT,1,4);leaf(root,1,NN_COMMAND_STATE_MASK|NN_COMMAND_STATE_VIEWPORT,NN_COMMAND_DRAW);
 T(!nn_command_sequence_call(root->sequence,child->sequence));T(child->refs==2);owner_drop(child); /* child public handle is gone */
 leaf(root,3,NN_COMMAND_STATE_MASK|NN_COMMAND_STATE_VIEWPORT,NN_COMMAND_DRAW);state(root,NN_COMMAND_STATE_MASK,3,88);leaf(root,4,NN_COMMAND_STATE_MASK|NN_COMMAND_STATE_VIEWPORT,NN_COMMAND_DRAW);T(!nn_command_sequence_seal(root->sequence));
 struct nn_command_state initial={0},final;initial.defined=NN_COMMAND_STATE_SCISSOR;initial.scissor[0]=123;memset(&final,0xab,sizeof(final));size_t native_count=77;struct observed out={0};T(!nn_command_sequence_visit(root->sequence,1u<<NN_COMMAND_DRAW,&initial,observe,&out,&final,&native_count));
 T(native_count==4&&out.count==4);const uint64_t ids[]={1,2,3,4};const uint32_t colors[]={1,2,2,3};for(unsigned i=0;i<4;i++){T(out.ids[i]==ids[i]&&out.colors[i]==colors[i]);T(out.viewport[i]==4);}T(final.defined==(NN_COMMAND_STATE_MASK|NN_COMMAND_STATE_VIEWPORT|NN_COMMAND_STATE_SCISSOR));T(final.render.color_mask==3&&final.viewport[0]==4&&final.scissor[0]==123);T(!initial.render.color_mask&&initial.scissor[0]==123);
 /* No default fill and no callbacks when any native family is unqualified. */
 struct nn_command_state saved=final;native_count=77;out=(struct observed){0};T(nn_command_sequence_visit(root->sequence,1u<<NN_COMMAND_RAW,&initial,observe,&out,&final,&native_count)==ENOTSUP);T(!out.count&&native_count==77&&!memcmp(&saved,&final,sizeof(saved)));
 out=(struct observed){.fail=2};T(nn_command_sequence_visit(root->sequence,1u<<NN_COMMAND_DRAW,&initial,observe,&out,&final,&native_count)==EIO);T(out.count==2&&native_count==77&&!memcmp(&saved,&final,sizeof(saved)));
 T(nn_command_sequence_state(root->sequence,NN_COMMAND_STATE_MASK,&initial)==EBUSY);T(nn_command_sequence_call(root->sequence,root->sequence)==EBUSY);owner_drop(root);T(!alive);
 /* Top-level sequential handles propagate explicitly defined GPU state. */
 root=make_owner(&domain);state(root,NN_COMMAND_STATE_MASK,9,0);T(!nn_command_sequence_seal(root->sequence));initial=(struct nn_command_state){0};T(!nn_command_sequence_visit(root->sequence,0,&initial,NULL,NULL,&initial,&native_count));T(initial.defined==NN_COMMAND_STATE_MASK&&initial.render.color_mask==9);owner_drop(root);
 root=make_owner(&domain);leaf(root,5,NN_COMMAND_STATE_MASK,NN_COMMAND_RAW);T(!nn_command_sequence_seal(root->sequence));out=(struct observed){0};T(!nn_command_sequence_visit(root->sequence,1u<<NN_COMMAND_RAW,&initial,observe,&out,&final,&native_count));T(out.colors[0]==9);initial.defined=0;out=(struct observed){0};saved=final;native_count=77;T(nn_command_sequence_visit(root->sequence,1u<<NN_COMMAND_RAW,&initial,observe,&out,&final,&native_count)==ENOTSUP);T(!out.count&&native_count==77&&!memcmp(&saved,&final,sizeof(saved)));owner_drop(root);
 /* Unsupported operation after a valid span still prevents all visitation. */
 root=make_owner(&domain);leaf(root,1,0,NN_COMMAND_RAW);T(!nn_command_sequence_unsupported(root->sequence));T(!nn_command_sequence_seal(root->sequence));out=(struct observed){0};T(nn_command_sequence_visit(root->sequence,NN_COMMAND_LEAF_KIND_BITS,NULL,observe,&out,&final,&native_count)==ENOTSUP);T(!out.count);owner_drop(root);
 /* Observing accepted native work keeps later explicit values after an
  * unknown-state mutation, without inventing old fields or bindings. */
 root=make_owner(&domain);state(root,NN_COMMAND_STATE_MASK,13,0);
 T(!nn_command_sequence_unsupported(root->sequence));state(root,NN_COMMAND_STATE_VIEWPORT,0,21);
 T(!nn_command_sequence_seal(root->sequence));initial=(struct nn_command_state){.defined=NN_COMMAND_STATE_SCISSOR};
 T(!nn_command_sequence_observe(root->sequence,&initial,&final));
 T(final.defined==NN_COMMAND_STATE_VIEWPORT&&final.viewport[0]==21&&!final.render.color_mask&&!final.binding_defined[0]);owner_drop(root);
 /* Validation, domain, allocation and owner-retain failures are atomic. */
 root=make_owner(&domain);child=make_owner(&domain);struct owner *foreign=make_owner(&other_domain);T(nn_command_sequence_call(root->sequence,child->sequence)==EINVAL);T(!nn_command_sequence_seal(child->sequence));T(!nn_command_sequence_seal(foreign->sequence));T(nn_command_sequence_call(root->sequence,foreign->sequence)==EINVAL);child->deny=1;T(nn_command_sequence_call(root->sequence,child->sequence)==EBUSY);T(child->refs==1);child->deny=0;fail_alloc=1;T(nn_command_sequence_call(root->sequence,child->sequence)==ENOMEM);T(child->refs==1);
 fail_alloc=1;T(nn_command_sequence_state(root->sequence,NN_COMMAND_STATE_MASK,&initial)==ENOMEM);T(nn_command_sequence_state(root->sequence,UINT64_C(1)<<63,&initial)==EINVAL);struct nn_command_leaf bad={.offset=4094,.bytes=4};T(nn_command_sequence_native(root->sequence,&bad)==EINVAL);bad.offset=SIZE_MAX;T(nn_command_sequence_native(root->sequence,&bad)==EINVAL);T(!nn_command_sequence_seal(root->sequence));T(!nn_command_sequence_visit(root->sequence,0,NULL,NULL,NULL,&final,&native_count));T(!native_count&&!final.defined);owner_drop(root);owner_drop(child);owner_drop(foreign);T(!alive);
 /* Typed binding nodes own private resources; effective state is borrowed
  * until explicitly cloned for a queue/top-level continuation. */
 root=make_owner(&domain);struct owner *asset=make_owner(&domain);struct nn_command_reference_ops resource_ops={owner_hold,owner_drop};
 struct nn_command_binding binding={.owner=asset,.ops=&resource_ops,.address=0x400040,.offset=64,.bytes=96,.guest=0xabc,.flags=3};
 T(!nn_command_sequence_bindings(root->sequence,NN_COMMAND_UBO,3,7,1,&binding));T(asset->refs==2);owner_drop(asset);memset(&binding,0,sizeof(binding));
 struct nn_command_leaf need_binding={.offset=0,.bytes=4,.kind=NN_COMMAND_RAW};unsigned index=0;T(!nn_command_binding_index(NN_COMMAND_UBO,3,7,&index));need_binding.required_bindings[index/64]=UINT64_C(1)<<(index%64);T(!nn_command_sequence_native(root->sequence,&need_binding));T(!nn_command_sequence_seal(root->sequence));T(!nn_command_sequence_visit(root->sequence,1u<<NN_COMMAND_RAW,NULL,NULL,NULL,&final,&native_count));
 const struct nn_command_binding *got=nn_command_state_binding(&final,NN_COMMAND_UBO,3,7);T(got&&got->owner==asset&&got->address==0x400040&&got->offset==64&&got->bytes==96&&got->guest==0xabc&&got->flags==3);T(!nn_command_state_binding(&final,NN_COMMAND_UBO,3,8));
 struct nn_command_state owned={0};T(!nn_command_state_clone(&final,&owned));T(asset->refs==2);owner_drop(root);T(asset->refs==1);T(nn_command_state_binding(&owned,NN_COMMAND_UBO,3,7)->owner==asset);nn_command_state_release(&owned);T(!alive&&!owned.binding_defined[index/64]);
 root=make_owner(&domain);T(!nn_command_sequence_native(root->sequence,&need_binding));T(!nn_command_sequence_seal(root->sequence));out=(struct observed){0};T(nn_command_sequence_visit(root->sequence,1u<<NN_COMMAND_RAW,NULL,observe,&out,&final,&native_count)==ENOTSUP);T(!out.count);owner_drop(root);
 root=make_owner(&domain);child=make_owner(&domain);asset=make_owner(&domain);asset->deny=1;struct nn_command_binding pair[2]={{.owner=child,.ops=&resource_ops},{.owner=asset,.ops=&resource_ops}};T(nn_command_sequence_bindings(root->sequence,NN_COMMAND_VERTEX_BUFFER,0,0,2,pair)==EBUSY);T(child->refs==1&&asset->refs==1);T(!nn_command_sequence_bindings(root->sequence,NN_COMMAND_UBO,5,16,0,NULL));T(nn_command_sequence_bindings(root->sequence,NN_COMMAND_UBO,5,17,0,NULL)==EINVAL);T(nn_command_binding_index(NN_COMMAND_IMAGE,6,0,&index)==EINVAL);T(!nn_command_binding_index(NN_COMMAND_TARGET,0,8,&index)&&index==608);T(!nn_command_sequence_seal(root->sequence));T(!nn_command_sequence_visit(root->sequence,0,NULL,NULL,NULL,&final,&native_count));T(!native_count);owner_drop(root);owner_drop(child);asset->deny=0;owner_drop(asset);T(!alive);
 /* DAG depth and expanded work are independently bounded. */
 root=make_owner(&domain);T(!nn_command_sequence_seal(root->sequence));for(unsigned i=1;i<64;i++){child=make_owner(&domain);T(!nn_command_sequence_call(child->sequence,root->sequence));T(!nn_command_sequence_seal(child->sequence));owner_drop(root);root=child;}child=make_owner(&domain);unsigned refs=root->refs;T(nn_command_sequence_call(child->sequence,root->sequence)==E2BIG);T(root->refs==refs);owner_drop(child);owner_drop(root);T(!alive);
 root=make_owner(&domain);leaf(root,1,0,NN_COMMAND_RAW);T(!nn_command_sequence_seal(root->sequence));for(unsigned i=0;i<21;i++){child=make_owner(&domain);T(!nn_command_sequence_call(child->sequence,root->sequence));T(!nn_command_sequence_call(child->sequence,root->sequence));T(!nn_command_sequence_seal(child->sequence));owner_drop(root);root=child;}out=(struct observed){0};native_count=77;saved=final;T(nn_command_sequence_visit(root->sequence,1u<<NN_COMMAND_RAW,NULL,observe,&out,&final,&native_count)==E2BIG);T(!out.count&&native_count==77&&!memcmp(&saved,&final,sizeof(saved)));owner_drop(root);T(!alive);
 printf("{\"passed\":true,\"checks\":%u,\"owners_freed\":%u,\"retains\":%u,\"releases\":%u,\"GPU_execution\":false,\"NVN_CallCommands_implemented\":false}\n",checks,freed,retains,releases);return 0;
}
