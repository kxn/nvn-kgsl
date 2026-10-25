/* HOST ownership seam; no PM4 or shader is executed. */
#define _POSIX_C_SOURCE 200809L
#define nn_device_open raw_open
#define nn_device_close raw_close
#define nn_bo_alloc raw_alloc
#define nn_bo_destroy raw_destroy
#define nn_bo_cache raw_cache
#define nn_queue_submit raw_submit
#define nn_queue_finish raw_finish
#include NN_STORAGE_GPU_MEMORY_BACKEND
#undef nn_device_open
#undef nn_device_close
#undef nn_bo_alloc
#undef nn_bo_destroy
#undef nn_bo_cache
#undef nn_queue_submit
#undef nn_queue_finish
static unsigned fault,submissions,finishes,device_closes,checks;
int nn_device_open(nn_device**d){if(fault==1){*d=NULL;return EACCES;}return raw_open(d);}
int nn_device_close(nn_device*d){device_closes++;return raw_close(d);}
int nn_bo_alloc(nn_device*d,size_t n,nn_bo**b){if(fault==2){*b=NULL;return ENOMEM;}return raw_alloc(d,n,b);}
int nn_bo_destroy(nn_bo*b){return raw_destroy(b);}
int nn_bo_cache(nn_bo*b,size_t o,size_t n,int inv){return fault==3?EIO:raw_cache(b,o,n,inv);}
int nn_queue_submit(nn_queue*q,nn_bo*ib,size_t off,size_t bytes,nn_bo*const*refs,size_t count,uint32_t*timestamp)
{assert(timestamp&&off==0&&bytes&&bytes%4==0&&bytes<=nn_bo_size(ib)&&count+1<=64-q->count);submissions++;q->pending[q->count++]=ib;ib->busy++;for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}*timestamp=submissions;return fault==4?EIO:0;}
int nn_queue_finish(nn_queue*q,uint32_t timeout){finishes++;return fault==5?ETIMEDOUT:raw_finish(q,timeout);}
#include NN_STORAGE_GPU_PROBE_SOURCE
#define T(x)do{checks++;if(!(x)){fprintf(stderr,"StorageGPU HOST line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char**argv)
{
 T(argc==2);
#if !NN_PIPELINE_GRAPHICS_STORAGE
 char text[32768];T(nn_pipeline_probe(argv[1],text,sizeof(text))==ENOTSUP);T(!submissions&&!quarantine&&device_closes==1);T(strstr(text,"\"passed\":false")&&strstr(text,"\"resources_retained\":false"));puts("HOST graphics storage defaultOFF closed actual_GPU_submits=0");
#else
 for(fault=1;fault<=5;fault++){
  struct owner*prior=quarantine;unsigned close_before=device_closes,submit_before=submissions;char text[32768];T(nn_pipeline_probe(argv[1],text,sizeof(text))&&strstr(text,"\"passed\":false"));
  if(fault>=4){T(quarantine&&quarantine!=prior&&quarantine->next==prior&&quarantine->program&&quarantine->target&&quarantine->work.job&&quarantine->queue);for(unsigned i=0;i<INPUTS;i++)T(quarantine->input[i]);T(device_closes==close_before&&submissions==submit_before+1);T(strstr(text,"\"resources_retained\":true"));}
  else{T(quarantine==prior&&submissions==submit_before);T(strstr(text,"\"resources_retained\":false"));if(fault>1)T(device_closes==close_before+1);}
 }
 fault=0;unsigned before=submissions;char text[32768];T(!nn_pipeline_probe(argv[1],text,sizeof(text)));T(submissions-before==192&&strstr(text,"\"passed\":true"));puts(text);fprintf(stderr,"HOST graphics storage checks=%u nonexecuting_submits=192 actual_GPU_submits=0 ambiguous_owners=2\n",checks);
#endif
 return 0;
}
