/* SPDX-License-Identifier: MIT */
#include "nvn_native/command_sequence.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#define NN_COMMAND_MAX_NODES 65535u
#define NN_COMMAND_MAX_DEPTH 64u
#define NN_COMMAND_MAX_EXPANDED 1048576u
enum command_sequence_node_kind { NODE_STATE,NODE_BINDINGS,NODE_NATIVE,NODE_CALL,NODE_UNSUPPORTED };
struct render_delta {uint64_t mask;struct nn_render_state render;float viewport[4],depth_range[2];int32_t scissor[4];};
struct command_sequence_node { struct command_sequence_node *next;enum command_sequence_node_kind kind;union {
 struct render_delta state;
 struct {unsigned count;struct nn_command_binding_write *values;} bindings;
 struct nn_command_leaf native;
 const struct nn_command_sequence *call;
} u;};
struct nn_command_sequence {
 void *owner;const struct nn_command_owner_ops *ops;const void *domain;
 struct command_sequence_node *head,*tail;size_t count;unsigned sealed,depth;
};
static int sequence_mutable(const struct nn_command_sequence *s)
{return !s?EINVAL:s->sealed?EBUSY:s->count>=NN_COMMAND_MAX_NODES?E2BIG:0;}
static void append_node(struct nn_command_sequence *s,struct command_sequence_node *n)
{if(s->tail)s->tail->next=n;else s->head=n;s->tail=n;s->count++;}
int nn_command_sequence_create(void *owner,const struct nn_command_owner_ops *ops,
 const void *domain,struct nn_command_sequence **out)
{
 if(!out)return EINVAL;
 *out=NULL;
 if(!owner||!domain||!ops||!ops->retain||!ops->release||!ops->validate_span)return EINVAL;
 struct nn_command_sequence *s=calloc(1,sizeof(*s));if(!s)return ENOMEM;
 s->owner=owner;s->ops=ops;s->domain=domain;s->depth=1;*out=s;return 0;
}
int nn_command_sequence_state(struct nn_command_sequence *s,uint64_t mask,const struct nn_command_state *value)
{
 int e=sequence_mutable(s);if(e)return e;
 if(!value||!mask||(mask&~NN_COMMAND_STATE_BITS))return EINVAL;
 struct command_sequence_node *n=calloc(1,sizeof(*n));if(!n)return ENOMEM;
 n->kind=NODE_STATE;n->u.state.mask=mask;n->u.state.render=value->render;memcpy(n->u.state.viewport,value->viewport,sizeof(value->viewport));memcpy(n->u.state.scissor,value->scissor,sizeof(value->scissor));memcpy(n->u.state.depth_range,value->depth_range,sizeof(value->depth_range));
 append_node(s,n);return 0;
}
static int binding_bits_valid(const uint64_t *bits)
{return !(bits[NN_COMMAND_BINDING_WORDS-1]>>(NN_COMMAND_BINDING_COUNT-64*(NN_COMMAND_BINDING_WORDS-1)));}
int nn_command_binding_index(enum nn_command_binding_domain domain,unsigned stage,unsigned slot,unsigned *out)
{
 static const unsigned base[]={0,6,102,198,214,215,216,280,344,408,600,609,641};
 static const unsigned stages[]={6,6,6,1,1,1,2,2,2,6,1,1,1};
 static const unsigned slots[]={1,16,16,16,1,1,32,32,32,32,9,32,32};
 if(!out||(unsigned)domain>12||stage>=stages[domain]||slot>=slots[domain])return EINVAL;
 *out=base[domain]+stage*slots[domain]+slot;return 0;
}
const struct nn_command_binding *nn_command_state_binding(const struct nn_command_state *s,enum nn_command_binding_domain domain,unsigned stage,unsigned slot)
{
 unsigned index;if(!s||nn_command_binding_index(domain,stage,slot,&index)||!(s->binding_defined[index/64]&(UINT64_C(1)<<(index%64))))return NULL;
 return s->bindings+index;
}
static int binding_hold(const struct nn_command_binding *b)
{
 if(!b->owner)return b->ops?EINVAL:0;
 if(!b->ops||!b->ops->retain||!b->ops->release)return EINVAL;
 return b->ops->retain(b->owner);
}
static void binding_drop(const struct nn_command_binding *b)
{if(b->owner)b->ops->release(b->owner);}
int nn_command_sequence_binding_writes(struct nn_command_sequence *s,unsigned count,const struct nn_command_binding_write *values)
{
 int e=sequence_mutable(s);if(e)return e;
 if(count>32||(count&&!values))return EINVAL;
 if(!count)return 0;
 for(unsigned i=0;i<count;i++){
  if(values[i].index>=NN_COMMAND_BINDING_COUNT)return EINVAL;
  if((values[i].value.flags&NN_COMMAND_BINDING_PRESERVE_HIGH)&&
    (values[i].index<216||values[i].index>=408||values[i].value.owner))return EINVAL;
  for(unsigned j=0;j<i;j++)if(values[j].index==values[i].index)return EINVAL;
 }
 struct command_sequence_node *n=calloc(1,sizeof(*n));if(!n)return ENOMEM;
 struct nn_command_binding_write *v=calloc(count,sizeof(*v));if(!v){free(n);return ENOMEM;}
 memcpy(v,values,count*sizeof(*v));unsigned held=0;
 for(;held<count;held++)if((e=binding_hold(&v[held].value)))break;
 if(e){while(held)binding_drop(&v[--held].value);free(v);free(n);return e;}
 n->kind=NODE_BINDINGS;n->u.bindings.count=count;n->u.bindings.values=v;append_node(s,n);return 0;
}
int nn_command_sequence_bindings(struct nn_command_sequence *s,enum nn_command_binding_domain domain,unsigned stage,unsigned first,unsigned count,const struct nn_command_binding *values)
{
 int e=sequence_mutable(s);if(e)return e;
 unsigned begin,last;
 if(!count)return nn_command_binding_index(domain,stage,first?first-1:0,&begin);
 if(nn_command_binding_index(domain,stage,first,&begin)||count>32||!values||
   nn_command_binding_index(domain,stage,first+count-1,&last))return EINVAL;
 struct nn_command_binding_write writes[32];
 for(unsigned i=0;i<count;i++){writes[i].index=begin+i;writes[i].value=values[i];}
 return nn_command_sequence_binding_writes(s,count,writes);
}
int nn_command_state_clone(const struct nn_command_state *s,struct nn_command_state *out)
{
 if(!s||!out||(s->defined&~NN_COMMAND_STATE_BITS)||!binding_bits_valid(s->binding_defined))return EINVAL;
 unsigned held=0;int e=0;
 for(;held<NN_COMMAND_BINDING_COUNT;held++)if((s->binding_defined[held/64]&(UINT64_C(1)<<(held%64)))&&(e=binding_hold(s->bindings+held)))break;
 if(e){while(held){held--;if(s->binding_defined[held/64]&(UINT64_C(1)<<(held%64)))binding_drop(s->bindings+held);}return e;}
 *out=*s;return 0;
}
void nn_command_state_release(struct nn_command_state *s)
{
 if(!s)return;
 for(unsigned i=0;i<NN_COMMAND_BINDING_COUNT;i++)if(s->binding_defined[i/64]&(UINT64_C(1)<<(i%64)))binding_drop(s->bindings+i);
 memset(s,0,sizeof(*s));
}
int nn_command_sequence_native(struct nn_command_sequence *s,const struct nn_command_leaf *leaf)
{
 int e=sequence_mutable(s);if(e)return e;
 if(!leaf||leaf->kind>NN_COMMAND_COPY||!leaf->bytes||leaf->offset>SIZE_MAX-leaf->bytes||
    (leaf->required_state&~NN_COMMAND_STATE_BITS)||!binding_bits_valid(leaf->required_bindings))return EINVAL;
 if((e=s->ops->validate_span(s->owner,leaf->offset,leaf->bytes,leaf->kind)))return e;
 struct command_sequence_node *n=calloc(1,sizeof(*n));if(!n)return ENOMEM;
 n->kind=NODE_NATIVE;n->u.native=*leaf;append_node(s,n);return 0;
}
int nn_command_sequence_calls(struct nn_command_sequence *s,unsigned count,const struct nn_command_sequence *const *children)
{
 int e=sequence_mutable(s);if(e)return e;
 if(count>NN_COMMAND_MAX_NODES||count>NN_COMMAND_MAX_NODES-s->count||(count&&!children))return count>NN_COMMAND_MAX_NODES-s->count?E2BIG:EINVAL;
 unsigned depth=s->depth;
 for(unsigned i=0;i<count;i++){
  const struct nn_command_sequence *child=children[i];
  if(!child||!child->sealed||child==s||child->domain!=s->domain)return EINVAL;
  if(child->depth>=NN_COMMAND_MAX_DEPTH)return E2BIG;
  if(depth<child->depth+1)depth=child->depth+1;
 }
 struct command_sequence_node *head=NULL,*tail=NULL;
 for(unsigned i=0;i<count;i++){
  struct command_sequence_node *n=calloc(1,sizeof(*n));if(!n){e=ENOMEM;goto fail;}
  const struct nn_command_sequence *child=children[i];
  if((e=child->ops->retain(child->owner))){free(n);goto fail;}
  n->kind=NODE_CALL;n->u.call=child;
  if(tail)tail->next=n;else head=n;tail=n;
 }
 if(count){if(s->tail)s->tail->next=head;else s->head=head;s->tail=tail;s->count+=count;s->depth=depth;}
 return 0;
 fail:
 while(head){struct command_sequence_node *next=head->next;head->u.call->ops->release(head->u.call->owner);free(head);head=next;}
 return e;
}
int nn_command_sequence_call(struct nn_command_sequence *s,const struct nn_command_sequence *child)
{return nn_command_sequence_calls(s,1,&child);}
int nn_command_sequence_unsupported(struct nn_command_sequence *s)
{
 int e=sequence_mutable(s);if(e)return e;
 struct command_sequence_node *n=calloc(1,sizeof(*n));if(!n)return ENOMEM;n->kind=NODE_UNSUPPORTED;append_node(s,n);return 0;
}
int nn_command_sequence_seal(struct nn_command_sequence *s)
{if(!s)return EINVAL;if(s->sealed)return EBUSY;s->sealed=1;return 0;}
void nn_command_sequence_destroy(struct nn_command_sequence *s)
{
 if(!s)return;
 struct command_sequence_node *n=s->head;while(n){struct command_sequence_node *next=n->next;
  if(n->kind==NODE_CALL)n->u.call->ops->release(n->u.call->owner);
  if(n->kind==NODE_BINDINGS){for(unsigned i=0;i<n->u.bindings.count;i++)binding_drop(&n->u.bindings.values[i].value);free(n->u.bindings.values);}
  free(n);n=next;
 }free(s);
}
/* Internal record-time CopyCommands metadata. Flat nodes are independently
 * owned by the destination; source owners and command arenas are not retained.
 * The caller has already copied native bytes/typed draw metadata. Calls and
 * unknown native-job families keep their existing separate admission gates. */
static __attribute__((unused)) int sequence_append_prepared(struct nn_command_sequence *dst,struct nn_command_sequence *plan)
{
 int e=sequence_mutable(dst);if(e)return e;
 if(!plan||plan==dst||plan->sealed||plan->owner!=dst->owner||plan->ops!=dst->ops||plan->domain!=dst->domain)return EINVAL;
 if(plan->count>NN_COMMAND_MAX_NODES-dst->count)return E2BIG;
 if(plan->head){if(dst->tail)dst->tail->next=plan->head;else dst->head=plan->head;
  dst->tail=plan->tail;dst->count+=plan->count;plan->head=plan->tail=NULL;plan->count=0;}
 return 0;
}
static __attribute__((unused)) int sequence_copy_flat(struct nn_command_sequence *dst,unsigned count,
 const struct nn_command_sequence *const *sources,const size_t *adjustments)
{
 int e=sequence_mutable(dst);if(e)return e;
 if(count>NN_COMMAND_MAX_NODES||(count&&(!sources||!adjustments)))return EINVAL;
 size_t nodes=0;
 for(unsigned i=0;i<count;i++){
  const struct nn_command_sequence *s=sources[i];
  if(!s||s==dst||!s->sealed||s->domain!=dst->domain)return EINVAL;
  if(s->count>NN_COMMAND_MAX_NODES-dst->count-nodes)return E2BIG;
  nodes+=s->count;
  for(const struct command_sequence_node *n=s->head;n;n=n->next){
   if(n->kind==NODE_CALL)return ENOTSUP;
   if(n->kind==NODE_NATIVE){const struct nn_command_leaf *leaf=&n->u.native;
    if(leaf->offset>SIZE_MAX-adjustments[i]||leaf->bytes>SIZE_MAX-leaf->offset-adjustments[i])return EOVERFLOW;
    if((e=s->ops->validate_span(s->owner,leaf->offset,leaf->bytes,leaf->kind))||
       (e=dst->ops->validate_span(dst->owner,leaf->offset+adjustments[i],leaf->bytes,leaf->kind)))return e;
   }else if(n->kind!=NODE_STATE&&n->kind!=NODE_BINDINGS&&n->kind!=NODE_UNSUPPORTED)return EINVAL;
  }
 }
 struct nn_command_sequence *plan=NULL;
 if((e=nn_command_sequence_create(dst->owner,dst->ops,dst->domain,&plan)))return e;
 for(unsigned i=0;i<count&&!e;i++)for(const struct command_sequence_node *n=sources[i]->head;n&&!e;n=n->next){
  if(n->kind==NODE_BINDINGS)e=nn_command_sequence_binding_writes(plan,n->u.bindings.count,n->u.bindings.values);
  else if(n->kind==NODE_NATIVE){struct nn_command_leaf leaf=n->u.native;leaf.offset+=adjustments[i];e=nn_command_sequence_native(plan,&leaf);}
  else if(n->kind==NODE_UNSUPPORTED)e=nn_command_sequence_unsupported(plan);
  else {struct command_sequence_node *copy=calloc(1,sizeof(*copy));
   if(!copy)e=ENOMEM;else{copy->kind=NODE_STATE;copy->u.state=n->u.state;append_node(plan,copy);}}
 }
 if(!e)e=sequence_append_prepared(dst,plan);
 nn_command_sequence_destroy(plan);return e;
}
static void state_apply(struct nn_command_state *a,const struct command_sequence_node *n,uint64_t mask)
{
 const struct render_delta *b=&n->u.state;
 for(unsigned i=0;i<8;i++)if(mask&NN_COMMAND_STATE_BLEND(i))a->render.blend[i]=b->render.blend[i];
 if(mask&NN_COMMAND_STATE_COLOR){a->render.blend_enable=b->render.blend_enable;a->render.logic_op=b->render.logic_op;a->render.alpha_func=b->render.alpha_func;}
 if(mask&NN_COMMAND_STATE_MASK)a->render.color_mask=b->render.color_mask;
 if(mask&NN_COMMAND_STATE_POLYGON){a->render.cull_face=b->render.cull_face;a->render.front_face=b->render.front_face;a->render.polygon_mode=b->render.polygon_mode;a->render.polygon_offset_enables=b->render.polygon_offset_enables;}
 if(mask&NN_COMMAND_STATE_DEPTH_STENCIL)a->render.depth=b->render.depth;
 if(mask&NN_COMMAND_STATE_MULTISAMPLE)a->render.multisample=b->render.multisample;
 if(mask&NN_COMMAND_STATE_BLEND_COLOR)memcpy(a->render.blend_color,b->render.blend_color,sizeof(a->render.blend_color));
 if(mask&NN_COMMAND_STATE_DEPTH_BOUNDS){a->render.depth_bounds_enable=b->render.depth_bounds_enable;a->render.depth_bounds_min=b->render.depth_bounds_min;a->render.depth_bounds_max=b->render.depth_bounds_max;}
 if(mask&NN_COMMAND_STATE_POLYGON_OFFSET){a->render.polygon_offset_factor=b->render.polygon_offset_factor;a->render.polygon_offset_units=b->render.polygon_offset_units;a->render.polygon_offset_clamp=b->render.polygon_offset_clamp;}
 for(unsigned i=0;i<2;i++){
  if(mask&(NN_COMMAND_STATE_STENCIL_VALUE_FRONT<<i))a->render.stencil_value_mask[i]=b->render.stencil_value_mask[i];
  if(mask&(NN_COMMAND_STATE_STENCIL_WRITE_FRONT<<i))a->render.stencil_write_mask[i]=b->render.stencil_write_mask[i];
  if(mask&(NN_COMMAND_STATE_STENCIL_REF_FRONT<<i))a->render.stencil_ref[i]=b->render.stencil_ref[i];
 }
 if(mask&NN_COMMAND_STATE_PATCH_SIZE)a->render.patch_size=b->render.patch_size;
 if(mask&NN_COMMAND_STATE_VIEWPORT)memcpy(a->viewport,b->viewport,sizeof(a->viewport));
 if(mask&NN_COMMAND_STATE_SCISSOR)memcpy(a->scissor,b->scissor,sizeof(a->scissor));
 if(mask&NN_COMMAND_STATE_DEPTH_RANGE)memcpy(a->depth_range,b->depth_range,sizeof(a->depth_range));
 a->render.version=NN_RENDER_STATE_VERSION;a->defined|=mask;
}
int nn_command_state_overlay_render(struct nn_command_state *destination,const struct nn_command_state *source)
{
 if(!destination||!source||(source->defined&~NN_COMMAND_STATE_BITS))return EINVAL;
 struct command_sequence_node delta={0};delta.u.state.mask=source->defined;delta.u.state.render=source->render;
 memcpy(delta.u.state.viewport,source->viewport,sizeof(source->viewport));memcpy(delta.u.state.scissor,source->scissor,sizeof(source->scissor));memcpy(delta.u.state.depth_range,source->depth_range,sizeof(source->depth_range));
 state_apply(destination,&delta,source->defined);return 0;
}
struct walk {struct nn_command_state state;size_t expanded,native;uint32_t qualified;unsigned observe;nn_command_sequence_visitor visitor;void *context;};
static int sequence_walk(const struct nn_command_sequence *s,struct walk *w,unsigned depth)
{
 if(!s||!s->sealed)return EINVAL;
 if(depth>NN_COMMAND_MAX_DEPTH)return E2BIG;
 for(const struct command_sequence_node *n=s->head;n;n=n->next){
  if(w->expanded>=NN_COMMAND_MAX_EXPANDED)return E2BIG;
  w->expanded++;
  int e=0;
  switch(n->kind){
   case NODE_STATE:state_apply(&w->state,n,n->u.state.mask);break;
   case NODE_BINDINGS:
    for(unsigned i=0;i<n->u.bindings.count;i++){
     unsigned at=n->u.bindings.values[i].index;struct nn_command_binding value=n->u.bindings.values[i].value;
     if(value.flags&NN_COMMAND_BINDING_PRESERVE_HIGH){
      unsigned high=(w->state.binding_defined[at/64]&(UINT64_C(1)<<(at%64)))&&(w->state.bindings[at].flags&NN_COMMAND_BINDING_HIGH_DEFINED);
      value.offset=high?w->state.bindings[at].offset:0;
      value.flags&=~(NN_COMMAND_BINDING_PRESERVE_HIGH|NN_COMMAND_BINDING_HIGH_DEFINED);
      if(high)value.flags|=NN_COMMAND_BINDING_HIGH_DEFINED;
     }
     w->state.bindings[at]=value;w->state.binding_defined[at/64]|=UINT64_C(1)<<(at%64);
    }
    break;
   case NODE_NATIVE:{const struct nn_command_leaf *leaf=&n->u.native;
    if(!w->observe&&(!(w->qualified&(1u<<leaf->kind))||(leaf->required_state&~w->state.defined)))return ENOTSUP;
    if(!w->observe)for(unsigned i=0;i<NN_COMMAND_BINDING_WORDS;i++)if(leaf->required_bindings[i]&~w->state.binding_defined[i])return ENOTSUP;
    if((e=s->ops->validate_span(s->owner,leaf->offset,leaf->bytes,leaf->kind)))return e;
    if(w->visitor&&(e=w->visitor(w->context,s->owner,leaf,&w->state)))return e;
    w->native++;break;}
   case NODE_CALL:e=sequence_walk(n->u.call,w,depth+1);if(e)return e;break;
   case NODE_UNSUPPORTED:if(!w->observe)return ENOTSUP;memset(&w->state,0,sizeof(w->state));break;
   default:return ENOTSUP;
  }
 }return 0;
}
int nn_command_sequence_visit(const struct nn_command_sequence *s,uint32_t qualified,
 const struct nn_command_state *initial,nn_command_sequence_visitor visitor,void *context,
 struct nn_command_state *final,size_t *native_count)
{
 if(!final||!native_count||(qualified&~NN_COMMAND_LEAF_KIND_BITS)||
    (initial&&((initial->defined&~NN_COMMAND_STATE_BITS)||!binding_bits_valid(initial->binding_defined))))return EINVAL;
 struct walk check={.qualified=qualified};if(initial)check.state=*initial;
 int e=sequence_walk(s,&check,1);if(e)return e;
 if(visitor){struct walk visit={.qualified=qualified,.visitor=visitor,.context=context};if(initial)visit.state=*initial;
  e=sequence_walk(s,&visit,1);if(e)return e;
 }
 *final=check.state;*native_count=check.native;return 0;
}

int nn_command_sequence_observe(const struct nn_command_sequence *s,const struct nn_command_state *initial,struct nn_command_state *final)
{
 if(!final||(initial&&((initial->defined&~NN_COMMAND_STATE_BITS)||!binding_bits_valid(initial->binding_defined))))return EINVAL;
 struct walk observation={.qualified=NN_COMMAND_LEAF_KIND_BITS,.observe=1};if(initial)observation.state=*initial;
 int e=sequence_walk(s,&observation,1);if(!e)*final=observation.state;return e;
}
