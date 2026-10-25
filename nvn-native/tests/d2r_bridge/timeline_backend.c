/* HOST ONLY deterministic queue-timeline fixture; never executes PM4/GPU code. */
#define _POSIX_C_SOURCE 200809L
#define NN_HOST_QUEUE_TIMELINE 1
#define nn_bo_destroy ram_bo_destroy
#define nn_queue_create ram_queue_create
#define nn_queue_destroy ram_queue_destroy
#define nn_queue_submit ram_queue_submit_unsupported
#define nn_queue_finish ram_queue_finish
#define nn_queue_timestamp ram_queue_timestamp
#define nn_bo_cache ram_bo_cache
#include NN_MEMORY_BACKEND_SOURCE
#undef nn_queue_create
#undef nn_queue_destroy
#undef nn_queue_submit
#undef nn_queue_finish
#undef nn_queue_timestamp
#undef nn_bo_cache
#undef nn_bo_destroy
#include "nvn_native/kgsl.h"
struct epoch { nn_bo *bo; uint32_t timestamp; };
struct model { nn_queue *queue; uint32_t issued, retired; struct epoch refs[256]; unsigned count, unknown; };
static struct model models[16];
static int failure, faults, ambiguous;
static nn_bo*last_refs[256];static unsigned last_count;
static const void*destroyed[4096];static unsigned destroyed_count;
static unsigned accepted, deps_seen, flushes, waits, cache_calls;
static struct model *model(nn_queue*q)
{for(unsigned i=0;i<16;i++)if(models[i].queue==q)return models+i;return NULL;}
void nn_test_fail_next(int e){failure=e;}
void nn_test_ambiguous_next(void){ambiguous=1;}
unsigned nn_test_destroy_count(const void*p){unsigned n=0;for(unsigned i=0;i<destroyed_count;i++)n+=destroyed[i]==p;return n;}
int nn_test_last_has(nn_bo*b){for(unsigned i=0;i<last_count;i++)if(last_refs[i]==b)return 1;return 0;}
int nn_bo_destroy(nn_bo*b){int e=ram_bo_destroy(b);if(!e){assert(destroyed_count<4096);destroyed[destroyed_count++]=b;}return e;}

void nn_test_faults(int n){faults=n;}
unsigned nn_test_accepted(void){return accepted;}
unsigned nn_test_dependencies(void){return deps_seen;}
unsigned nn_test_flushes(void){return flushes;}
unsigned nn_test_waits(void){return waits;}
unsigned nn_test_cache_calls(void){return cache_calls;}
void nn_test_retire(nn_queue*q,uint32_t timestamp)
{
    struct model*m=model(q);assert(m);m->retired=timestamp;
    for(unsigned i=0;i<m->count;){
        if((int32_t)(timestamp-m->refs[i].timestamp)>=0){assert(m->refs[i].bo->busy);m->refs[i].bo->busy--;m->refs[i]=m->refs[--m->count];}
        else i++;
    }
    q->count=m->count;
}
int nn_queue_create(nn_device*d,nn_queue**out)
{
    if(failure){int e=failure;failure=0;*out=NULL;return e;}
    int e=ram_queue_create(d,out);if(e)return e;
    for(unsigned i=0;i<16;i++)if(!models[i].queue){models[i]=(struct model){.queue=*out};return 0;}
    ram_queue_destroy(*out);*out=NULL;return ENOSPC;
}
int nn_queue_destroy(nn_queue*q)
{
    struct model*m=model(q);assert(m);
    int e=ram_queue_destroy(q);if(!e)*m=(struct model){0};return e;
}
int nn_queue_submit_waits(nn_queue*q,nn_bo*ib,size_t offset,size_t bytes,nn_bo*const*refs,size_t count,
                         const struct nn_queue_dependency*deps,size_t n,uint32_t*timestamp)
{
    struct model*m=model(q);assert(m);
    if(m->unknown)return EBUSY;
    if(failure){int e=failure;failure=0;return e;}
    if(!ib || !bytes || offset>ib->bytes || bytes>ib->bytes-offset || n>16 || (n&&!deps) || count>128)return EINVAL;
    for(size_t i=0;i<n;i++)if(!model(deps[i].queue) || model(deps[i].queue)->issued<deps[i].timestamp)return EINVAL;
    if(m->count+count+1>256)return ENOSPC;
    m->issued++;*timestamp=m->issued;
    m->refs[m->count++]=(struct epoch){ib,m->issued};ib->busy++;
    for(size_t i=0;i<count;i++){m->refs[m->count++]=(struct epoch){refs[i],m->issued};refs[i]->busy++;}
    q->count=m->count;accepted++;deps_seen+=(unsigned)n;
    last_count=0;last_refs[last_count++]=ib;for(size_t i=0;i<count;i++)last_refs[last_count++]=refs[i];
    if(ambiguous){ambiguous=0;m->unknown=1;return EIO;}
    return 0;
}
int nn_queue_submit(nn_queue*q,nn_bo*ib,size_t offset,size_t bytes,nn_bo*const*refs,size_t count,uint32_t*timestamp)
{return nn_queue_submit_waits(q,ib,offset,bytes,refs,count,NULL,0,timestamp);}
uint32_t nn_queue_timestamp(const nn_queue*q){return model((nn_queue*)q)->issued;}
int nn_queue_timestamp_status(nn_queue*q,uint32_t timestamp,int*complete)
{
    struct model*m=model(q);if(!m||!complete)return EINVAL;
    *complete=0;if(m->unknown)return EBUSY;if(failure){int e=failure;failure=0;return e;}
    if(timestamp>m->issued)return EINVAL;
    *complete=m->retired>=timestamp;return 0;
}
int nn_queue_wait_timestamp(nn_queue*q,uint32_t timestamp,uint32_t timeout)
{
    waits++;
    struct model*m=model(q);if(!m||timestamp>m->issued)return EINVAL;
    if(m->unknown)return EBUSY;
    if(failure){int e=failure;failure=0;return e;}
    if(m->retired>=timestamp)return 0;
    if(!timeout)return ETIMEDOUT;
    /* Fixture models a kernel completion, not a GPU result. No PM4 is run. */
    nn_test_retire(q,timestamp);return 0;
}
int nn_queue_finish(nn_queue*q,uint32_t timeout)
{return nn_queue_wait_timestamp(q,nn_queue_timestamp(q),timeout);}
int nn_queue_faults(nn_queue*q,int*out)
{if(!model(q)||!out)return EINVAL;if(failure){int e=failure;failure=0;return e;}*out=faults;return 0;}
int nn_queue_flush(nn_queue*q){if(!model(q))return EINVAL;flushes++;if(failure){int e=failure;failure=0;return e;}return 0;}
int nn_bo_cache(nn_bo*b,size_t o,size_t n,int from)
{cache_calls++;if(failure){int e=failure;failure=0;return e;}return ram_bo_cache(b,o,n,from);}

/* Explicit test-only resolution of the ambiguous syscall fixture, not an API
 * or GPU retirement claim. The real backend never offers this escape hatch. */
void nn_test_resolve_unknown(nn_queue*q){struct model*m=model(q);assert(m&&m->unknown);m->unknown=0;nn_test_retire(q,m->issued);}

nn_bo *nn_test_last_parent(void){return last_count?last_refs[0]:NULL;}
