/* HOST ONLY: deterministic CPU allocation failures in real core metadata.
 * Actual RAM-backend/native-module BO allocation seams remain unchanged. */
static size_t metadata_fail_at,metadata_allocations;
static int metadata_fail(void){metadata_allocations++;return metadata_fail_at&&metadata_allocations==metadata_fail_at;}
static void *metadata_malloc(size_t n){return metadata_fail()?NULL:malloc(n);}
static void *metadata_calloc(size_t n,size_t z){return metadata_fail()?NULL:calloc(n,z);}
static void *metadata_realloc(void *v,size_t n){return metadata_fail()?NULL:realloc(v,n);}
#define malloc metadata_malloc
#define calloc metadata_calloc
#define realloc metadata_realloc
#include NN_ENQUEUE_CORE_SOURCE
#undef malloc
#undef calloc
#undef realloc
