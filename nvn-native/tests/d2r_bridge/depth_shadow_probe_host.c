/* HOST ownership/protocol only: no PM4, shader, depth or raw-block is executed. */
#define _POSIX_C_SOURCE 200809L
#include "nvn_native/depth_shadow.h"
#define nn_device_open raw_open
#define nn_device_close raw_close
#define nn_bo_alloc raw_alloc
#define nn_bo_destroy raw_destroy
#define nn_bo_map raw_map
#define nn_bo_cache raw_cache
#define nn_queue_submit raw_submit
#define nn_queue_finish raw_finish
#include NN_DEPTH_MEMORY_BACKEND
#undef nn_device_open
#undef nn_device_close
#undef nn_bo_alloc
#undef nn_bo_destroy
#undef nn_bo_map
#undef nn_bo_cache
#undef nn_queue_submit
#undef nn_queue_finish
static struct nn_depth_shadow*observed;
static unsigned fault,checks,submits,finishes,per_probe,closes,private_access;
static int constructing;
int nn_device_open(nn_device**out){observed=NULL;constructing=0;per_probe=0;if(fault==1){*out=NULL;return EACCES;}return raw_open(out);}
int nn_device_close(nn_device*d){closes++;return raw_close(d);}
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**out){if(fault==2){*out=NULL;return ENOMEM;}return raw_alloc(d,n,out);}
int nn_bo_destroy(nn_bo*b){return raw_destroy(b);}
static void payload_forbidden(const nn_bo*b)
{
 if(observed&&(b==observed->attachment.storage||b==observed->linear.storage||(constructing&&b==observed->canonical.bo))){private_access++;assert(!"private depth payload CPU access");}
}
void*nn_bo_map(const nn_bo*b){payload_forbidden(b);return raw_map(b);}
int nn_bo_cache(nn_bo*b,size_t off,size_t n,int inv){payload_forbidden(b);if(fault==3)return EIO;return raw_cache(b,off,n,inv);}
int nn_queue_submit(nn_queue*q,nn_bo*ib,size_t offset,size_t bytes,nn_bo*const*refs,size_t count,uint32_t*timestamp)
{
 assert(!offset&&bytes&&count>=17&&count<=18&&observed&&observed->ready);submits++;per_probe++;
 nn_bo*owner[10];size_t owner_n;assert(!nn_depth_shadow_resources(observed,owner,10,&owner_n)&&owner_n==10);
 for(size_t i=0;i<owner_n;i++){size_t j=0;for(;j<count;j++)if(refs[j]==owner[i])break;assert(j<count);}
 /* RAM backend records one aggregate hold for each distinct BO across the
  * six pending submits. This is ownership-only and never fabricates pixels. */
 for(size_t i=0;i<count+1;i++){nn_bo*b=i==count?ib:refs[i];unsigned j=0;for(;j<q->count;j++)if(q->pending[j]==b)break;if(j==q->count){assert(q->count<64);q->pending[q->count++]=b;b->busy++;}}
 *timestamp=submits;return fault>=10&&fault<=15&&per_probe==fault-9?EIO:0;
}
int nn_queue_finish(nn_queue*q,uint32_t timeout){finishes++;return fault==16?ETIMEDOUT:raw_finish(q,timeout);}
static int tracked_create(nn_device*d,const struct nn_cs_image*k,const struct nn_pipeline_platform*p,const struct nn_tex_source_layout*s,const struct nn_depth_shadow_plan*v,const struct nn_block_copy_range*r,size_t cap,struct nn_depth_shadow*out)
{observed=out;constructing=1;int e=nn_depth_shadow_create(d,k,p,s,v,r,cap,out);constructing=0;return e;}
#define nn_depth_shadow_create tracked_create
#include NN_DEPTH_PROBE_SOURCE
#undef nn_depth_shadow_create
#define T(x)do{checks++;if(!(x)){fprintf(stderr,"Shadow HOST line%d %s\n",__LINE__,#x);return 1;}}while(0)
#if !NN_PIPELINE_DEPTH_NATIVE
int main(int argc,char**argv){T(argc==2);char json[32768];T(nn_pipeline_probe(argv[1],json,sizeof(json))==ENOTSUP);T(!submits&&!quarantine&&!private_access);puts("HOST canonical depth defaultOFF closed; GPU0");return 0;}
#else
int main(int argc,char**argv)
{
 T(argc==2);char json[32768];for(fault=1;fault<=3;fault++){struct owner*prior=quarantine;unsigned count=submits;T(nn_pipeline_probe(argv[1],json,sizeof(json))&&strstr(json,"\"passed\":false"));T(prior==quarantine&&submits==count&&strstr(json,"\"resources_retained\":false"));}
 for(fault=10;fault<=16;fault++){
  struct owner*prior=quarantine;unsigned count=submits,closed=closes;T(nn_pipeline_probe(argv[1],json,sizeof(json))&&strstr(json,"\"passed\":false"));T(quarantine&&quarantine!=prior&&quarantine->next==prior&&closes==closed);
  struct shadow_owner*d=(struct shadow_owner*)quarantine;T(d->shadow.ready&&d->canonical&&d->clear_job&&quarantine->program&&quarantine->work.job&&quarantine->queue);
  nn_bo*refs[10];size_t n;T(!nn_depth_shadow_resources(&d->shadow,refs,10,&n)&&n==10);for(size_t i=0;i<n;i++)T(nn_bo_idle(refs[i])==EBUSY);
  T(submits-count==(fault==16?6u:fault-9u)&&strstr(json,"\"cleanup_errno\":16")&&strstr(json,"\"resources_retained\":true"));
 }
 fault=0;unsigned before=submits;T(!nn_pipeline_probe(argv[1],json,sizeof(json)));T(submits-before==648&&!private_access&&strstr(json,"\"passed\":true"));puts(json);fprintf(stderr,"HOST protocol-only: checks=%u unknown_owners=7 GPU0 private_payload_CPU_access=0\n",checks);return 0;
}
#endif
