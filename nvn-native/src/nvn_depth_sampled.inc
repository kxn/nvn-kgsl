/* CPU-only actual registered depth snapshot shape; no shader expectation. */
#if NN_NVN_DEPTH_SAMPLED
static int pipeline_depth_view(const struct descriptor_entry*entry,uint32_t*base)
{
 const struct texture_desc*t=&entry->snapshot.texture;const struct nn_tex_source_layout*s=entry->source_layout;
 if(!entry->valid||!s||t->target!=NVN_TEXTURE_TARGET_2D||t->depth!=1||t->samples||t->format<NVN_FORMAT_DEPTH16||t->format>NVN_FORMAT_DEPTH32F||
    t->depth_mode!=NVN_TEXTURE_DEPTH_STENCIL_MODE_DEPTH||s->linear||s->target!=1||s->layers!=1||s->block_w!=1||s->block_h!=1)return ENOTSUP;
 int e=nn_tex_source_validate(s);if(e)return e;
 if(t->levels<1||(uint32_t)t->levels!=s->levels||(uint32_t)t->format!=s->format||
    (uint32_t)t->width!=s->level[0].width||(uint32_t)t->height!=s->level[0].height||entry->size!=s->payload_bytes)return EINVAL;
 const struct descriptor_selection*v=&entry->selection;
 if(v->base_layer||v->layer_count!=1||v->level_count!=1||v->base_level>=s->levels||v->source_offset!=s->level[v->base_level].offset||
    v->subset!=!!(v->base_level||s->levels!=1))return ENOTSUP;
 *base=v->base_level;return 0;
}
static int pipeline_depth_sample_plan(const struct descriptor_entry*entry,const struct nn_pipeline_texture_types*types,unsigned binding,struct nn_depth_sampled_plan*out)
{
 uint32_t base=0,op=0;int e=pipeline_depth_view(entry,&base);if(e)return e;
 if((e=nn_depth_sampled_types(types,binding,&op)))return e;
 const struct texture_desc*t=&entry->snapshot.texture;
 uint32_t swizzle[4]={NN_DEPTH_R,NN_DEPTH_ZERO,NN_DEPTH_ZERO,NN_DEPTH_ONE};
 if(op&(NN_PIPELINE_TEX_SAMPLE|NN_PIPELINE_TEX_FETCH)){
  /* No claim about unspecified NVN default depth tuple components. Explicit
   * R/0/1 view swizzles define every observable channel for these operations. */
  if(!t->swizzle_set)return ENOTSUP;
  for(unsigned c=0;c<4;c++){
   unsigned s=texture_swizzle_component(t,c);
   if(s==NVN_TEXTURE_SWIZZLE_R)swizzle[c]=NN_DEPTH_R;
   else if(s==NVN_TEXTURE_SWIZZLE_ZERO)swizzle[c]=NN_DEPTH_ZERO;
   else if(s==NVN_TEXTURE_SWIZZLE_ONE)swizzle[c]=NN_DEPTH_ONE;
   else return ENOTSUP;
  }
 }else if((op&NN_PIPELINE_TEX_GATHER)&&texture_swizzle_component(t,0)!=NVN_TEXTURE_SWIZZLE_R)return ENOTSUP;
 /* Only R is consumed by proved TG4-R; SIZE consumes no channels. Their
  * internal unused channels have no observable default-tuple assumption. */
 return nn_depth_sampled_plan(entry->source_layout,base,0,swizzle,out);
}
#endif
