/* HOST ONLY deterministic whole-batch CPU preflight failure/rollback. */
static void metadata_preflight_oom(NVNqueue *key,NVNcommandHandle handle)
{
 struct object *q=find(key,QUEUE),*owners[128];size_t refs[128],count=0;
 for(struct object *o=objects;o;o=o->next){T(count<128);owners[count]=o;refs[count++]=o->refs;}
 NVNcommandHandle ids[2]={handle,handle};struct pipeline_batch batch={0};
 metadata_allocations=0;T(!pipeline_batch_prepare(q,2,ids,&batch));size_t needed=metadata_allocations;
 T(needed>=4);pipeline_batch_discard(&batch);
 for(size_t fail=1;fail<=needed;fail++){
  unsigned na=mock_allocations,nc=mock_caches,ns=mock_submits;metadata_allocations=0;metadata_fail_at=fail;
  api_nvnQueueSubmitCommands(key,2,ids);int result=nn_nvn_error();metadata_fail_at=0;
  T(result==ENOMEM);nn_nvn_clear_error();
  T(mock_allocations==na&&mock_caches==nc&&mock_submits==ns);
  for(size_t i=0;i<count;i++)T(owners[i]->refs==refs[i]);
 }
}
