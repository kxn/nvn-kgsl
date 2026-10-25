/* HOST ONLY counted real RAM backend. No PM4 execution. */
#define nn_bo_alloc ram_original_alloc
#define nn_bo_cache ram_original_cache
#define nn_bo_destroy ram_original_destroy
#define nn_queue_submit ram_original_submit
#include NN_REAL_MEMORY_BACKEND
#undef nn_bo_alloc
#undef nn_bo_cache
#undef nn_bo_destroy
#undef nn_queue_submit
static unsigned fail_bo_alloc,fail_bo_destroy,bo_destroys;
static unsigned mock_allocations,mock_caches,mock_submits;static int mock_submit_error;static nn_bo *mock_refs[128];static size_t mock_ref_count;
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **b){mock_allocations++;if(fail_bo_alloc==mock_allocations){*b=NULL;return ENOMEM;}return ram_original_alloc(d,n,b);}
int nn_bo_destroy(nn_bo *b){bo_destroys++;if(fail_bo_destroy==bo_destroys)return EIO;return ram_original_destroy(b);}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int inv){mock_caches++;return ram_original_cache(b,o,n,inv);}
int nn_queue_submit(nn_queue *q,nn_bo *ib,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *timestamp)
{
 if(!q||!ib||offset>ib->bytes||bytes>ib->bytes-offset||count>127)return EINVAL;
 nn_bo *pending[128];size_t n=0;pending[n++]=ib;
 for(size_t i=0;i<count;i++){
  if(!refs[i]||refs[i]->device!=q->device)return EINVAL;
  size_t j=0;while(j<n&&pending[j]!=refs[i])j++;if(j==n)pending[n++]=refs[i];
 }
 if(n>64-q->count)return ENOSPC;
 for(size_t i=0;i<n;i++){pending[i]->busy++;q->pending[q->count++]=pending[i];}
 mock_ref_count=count;memcpy(mock_refs,refs,count*sizeof(*refs));
 mock_submits++;if(timestamp)*timestamp=mock_submits;return mock_submit_error;
}
