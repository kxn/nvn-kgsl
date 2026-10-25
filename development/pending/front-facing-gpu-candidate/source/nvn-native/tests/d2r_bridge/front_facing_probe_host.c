/* HOST actual encoder/lifetime protocol, not PM4/CPU shader execution. */
#define _POSIX_C_SOURCE 200809L
#define nn_device_open raw_open
#define nn_device_close raw_close
#define nn_bo_alloc raw_alloc
#define nn_bo_cache raw_cache
#define nn_queue_submit raw_submit
#define nn_queue_finish raw_finish
#include NN_DEPTH_MEMORY_BACKEND
#undef nn_device_open
#undef nn_device_close
#undef nn_bo_alloc
#undef nn_bo_cache
#undef nn_queue_submit
#undef nn_queue_finish
#include <sys/stat.h>
static unsigned fault,fail_at,attempts,finishes,device_closes,checks;
int nn_device_open(nn_device**d){if(fault==1){*d=NULL;return EACCES;}return raw_open(d);}
int nn_device_close(nn_device*d){device_closes++;return raw_close(d);}
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**b){if(fault==2){*b=NULL;return ENOMEM;}return raw_alloc(d,n,b);}
int nn_bo_cache(nn_bo*b,size_t o,size_t n,int from){return fault==3?EIO:raw_cache(b,o,n,from);}
int nn_queue_submit(nn_queue*q,nn_bo*ib,size_t offset,size_t bytes,nn_bo*const*refs,size_t count,uint32_t*timestamp){assert(timestamp&&bytes&&count+1<=64-q->count&&offset==0);attempts++;q->pending[q->count++]=ib;ib->busy++;for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}*timestamp=attempts;return fault==4&&attempts==fail_at?EIO:0;}
int nn_queue_finish(nn_queue*q,uint32_t timeout){finishes++;return fault==5&&attempts==fail_at?ETIMEDOUT:raw_finish(q,timeout);}
#include NN_DEPTH_PROBE_SOURCE
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"FF HOST line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char**argv){T(argc==2);char text[32768];
#if !NN_PIPELINE_FRONT_FACING
 T(nn_pipeline_probe(argv[1],text,sizeof(text))==ENOTSUP);T(!attempts&&!quarantine&&!device_closes);T(strstr(text,"\"passed\":false")&&strstr(text,"\"resources_retained\":false"));puts("HOST FACE defaultOFF closed; actual_GPU_submits=0");return 0;
#else
 for(fault=1;fault<=3;fault++){unsigned before=device_closes;int e=nn_pipeline_probe(argv[1],text,sizeof(text));T(e&&strstr(text,"\"passed\":false")&&strstr(text,"\"resources_retained\":false"));T(!attempts&&!quarantine);T(device_closes==before+(fault>1));}
 /* Normal/indexed in all three groups plus clear/draw/read and final replay. */
 const unsigned boundaries[]={1,3,129,131,257,258,259,263,264,265,640};
 for(fault=4;fault<=5;fault++)for(unsigned i=0;i<sizeof(boundaries)/sizeof(boundaries[0]);i++){
  fail_at=boundaries[i];attempts=finishes=0;unsigned before=device_closes;struct owner*prior=quarantine;char dir[1600];T(snprintf(dir,sizeof(dir),"%s/fault-%u-%u",argv[1],fault,fail_at)>0);T(!mkdir(dir,0700));int e=nn_pipeline_probe(dir,text,sizeof(text));T(e&&strstr(text,"\"passed\":false")&&strstr(text,"\"resources_retained\":true")&&strstr(text,"\"cleanup_errno\":16"));T(attempts==fail_at&&device_closes==before);T(quarantine&&quarantine!=prior&&quarantine->next==prior&&quarantine->device&&quarantine->queue&&quarantine->program&&quarantine->work.job);T(quarantine->queue->count);if(strstr(text,"\"stage\":\"draw-submit\""))T(nn_pipeline_program_destroy(quarantine->program)==EBUSY);else T(nn_depth_attachment_destroy(&((struct face_owner*)quarantine)->attachment)==EBUSY);
 }
 fault=0;attempts=finishes=0;char dir[1600];T(snprintf(dir,sizeof(dir),"%s/success",argv[1])>0);T(!mkdir(dir,0700));T(!nn_pipeline_probe(dir,text,sizeof(text)));T(attempts==640&&finishes==640&&strstr(text,"\"passed\":true"));puts(text);fprintf(stderr,"HOST FACE checks=%u full_matrix=192/draw384 nonexecuting_submits640 unknown_owners22 actual_GPU_submits0\n",checks);return 0;
#endif
}
