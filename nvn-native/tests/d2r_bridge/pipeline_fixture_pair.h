/* Qualification input selection only. Production selection starts from the
 * Program's already matched full source identity; it never scans color pairs. */
static const struct nn_pipeline_pair_recipe *pipeline_fixture_color_pair(const struct nn_pipeline_archive *a,
 struct nn_pipeline_frontend_queries *state)
{
 const struct nn_pipeline_pair_recipe *found=NULL;
 for(size_t i=0;i<a->pair_count;i++) {
  const struct nn_pipeline_pair_recipe *p=a->pairs+i;
  if(p->recipe->texture_mask||p->recipe->sampler_mask)continue;
  const struct nn_offline_graphics_stage *v=a->stages->stages+p->vs_index,*f=a->stages->stages+p->fs_index,*sv=NULL,*sf=NULL;
  int e=nn_pipeline_select_stage(a,0,v->source,v->source_bytes,v->control,v->control_bytes,nn_pipeline_frontend_query,state,&sv);
  if(e==ENOTSUP)continue;
  if(e)return NULL;
  e=nn_pipeline_select_stage(a,1,f->source,f->source_bytes,f->control,f->control_bytes,nn_pipeline_frontend_query,state,&sf);
  if(e==ENOTSUP)continue;
  if(e)return NULL;
  if(sv!=v||sf!=f)continue;
  if(found)return NULL;
  found=p;
 }
 return found;
}
