/* Public canonical depth read/write admission; pure CPU metadata only. */
#if NN_NVN_DEPTH_TARGETS
static int public_depth_pool_alias(const struct object*z,struct object*pool,size_t offset,size_t bytes)
{
 if(!pool||!bytes)return 0;
 if(offset>pool->size||bytes>pool->size-offset)return EINVAL;
 struct descriptor_entry read={.pool=pool,.offset=offset,.size=bytes};return bridge_feedback(z,&read);
}
static int public_depth_arena_alias(const struct object*z)
{
 for(struct object*o=objects;o;o=o->next)if(o->kind==COMMAND&&o->record){
  struct recording*r=o->record;int e=public_depth_pool_alias(z,r->pool,r->offset,r->capacity);if(e)return e;
  if(r->current)for(struct command_chunk*c=r->current->chunks;c;c=c->next){e=public_depth_pool_alias(z,c->pool,c->offset,c->bytes);if(e)return e;}
 }
 /* Published/retired-but-borrowed handle bodies are real command arenas too. */
 for(struct handle*h=handles;h;h=h->next){
  if(h->chunks){for(struct command_chunk*c=h->chunks;c;c=c->next){int e=public_depth_pool_alias(z,c->pool,c->offset,c->bytes);if(e)return e;}}
  else{int e=public_depth_pool_alias(z,h->commands,h->offset,h->bytes);if(e)return e;}
 }
 return 0;
}
static int public_depth_bound_alias(const struct object*z,const struct bound*b)
{return public_depth_pool_alias(z,b->pool,b->offset,b->size);}
static int public_depth_preflight_aliases(const struct late_draw*draw,const struct pipeline_draw_snapshot*s,
 const struct nn_pipeline_shader_recipe*r,const struct descriptor_entry*const entries[16])
{
 const struct object*z=draw->depth;if(!z)return EINVAL;
 int e=gpu_pool_access(z->pool,z->offset,z->size,"NVN depth canonical consumer");if(e)return e;
 if((e=public_depth_arena_alias(z)))return e;
 if(draw->target&&(e=public_depth_pool_alias(z,draw->target->pool,draw->target->offset,draw->target->size)))return e;
 for(unsigned i=0;i<16;i++)if(entries[i]){if((e=public_depth_pool_alias(z,entries[i]->pool,entries[i]->offset,entries[i]->size)))return e;}
 for(size_t i=0;i<r->input_count;i++){
  unsigned location=r->inputs[i].location;if(location>=32)return EINVAL;
  unsigned stream=s->native.vertex.attributes[location].stream;if(stream>=32)return EINVAL;
  if((e=public_depth_bound_alias(z,s->vertices+stream)))return e;
 }
 for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<14;i++)if(s->uniforms[stage][i].pool){
  const struct bound*b=s->uniforms[stage]+i;
  for(unsigned native=0;native<16;native++)if(s->native.ubo[stage][native].bo==b->pool->bo&&s->native.ubo[stage][native].offset==b->offset&&s->native.ubo[stage][native].bytes==b->size){if((e=public_depth_bound_alias(z,b)))return e;break;}
 }
 const struct nn_gfx_buffer*arguments[]={&s->native.draw.index,&s->native.draw.indirect,&s->native.draw.indirect_count};
 for(unsigned i=0;i<3;i++)if(arguments[i]->bo){uint64_t base=nn_bo_address(arguments[i]->bo);if(arguments[i]->offset>UINT64_MAX-base)return EOVERFLOW;
  struct object*pool=address_pool(z->device,base+arguments[i]->offset,arguments[i]->bytes);
  if(!pool||pool->bo!=arguments[i]->bo)return EINVAL;
  if((e=public_depth_pool_alias(z,pool,arguments[i]->offset,arguments[i]->bytes)))return e;
 }
 return 0;
}
#endif
