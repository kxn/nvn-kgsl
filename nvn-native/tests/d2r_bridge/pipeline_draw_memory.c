/* HOST RAM only. Models ownership of submitted references, no packet execution. */
#define nn_queue_submit ram_unimplemented_submit
#include NN_REAL_MEMORY_BACKEND
#undef nn_queue_submit
static unsigned mock_submits;static int mock_submit_error;static nn_bo *mock_refs[128];static size_t mock_ref_count;
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
 if(!q||!ib||offset>ib->bytes||bytes>ib->bytes-offset||count>127)return EINVAL;
 nn_bo *pending[128];size_t n=0;pending[n++]=ib;
 for(size_t i=0;i<count;i++){
  if(!refs[i]||refs[i]->device!=q->device)return EINVAL;
  size_t j=0;while(j<n&&pending[j]!=refs[i])j++;if(j==n)pending[n++]=refs[i];
 }
 if(n>64-q->count)return ENOSPC;
 mock_ref_count=count;memcpy(mock_refs,refs,count*sizeof(*refs));
 for(size_t i=0;i<n;i++){pending[i]->busy++;q->pending[q->count++]=pending[i];}
 mock_submits++;if(timestamp)*timestamp=mock_submits;return mock_submit_error;
}
