/* SPDX-License-Identifier: MIT. HOST actual API+probe; no CP/image execution. */
#define _POSIX_C_SOURCE 200809L
#define nn_device_open raw_open
#define nn_device_close raw_close
#define nn_bo_alloc raw_alloc
#define nn_bo_import raw_import
#define nn_bo_destroy raw_destroy
#define nn_bo_cache raw_cache
#define nn_queue_submit raw_submit
#define nn_queue_finish raw_finish
#include NN_TEXELS_MEMORY_BACKEND
#undef nn_device_open
#undef nn_device_close
#undef nn_bo_alloc
#undef nn_bo_import
#undef nn_bo_destroy
#undef nn_bo_cache
#undef nn_queue_submit
#undef nn_queue_finish
static unsigned fault,submits,finishes,device_closes,checks;
int nn_device_inspect(struct nn_device_identity *i){if(!i)return EINVAL;*i=(struct nn_device_identity){.chip_id=0x06050002,.page_bytes=4096,.gmem_bytes=1179648};return 0;}
int nn_device_open(nn_device **d){if(fault==1){*d=NULL;return EACCES;}return raw_open(d);}
int nn_device_close(nn_device *d){device_closes++;return raw_close(d);}
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **b){return raw_alloc(d,n,b);}
int nn_bo_import(nn_device *d,void *p,size_t n,nn_bo **b){if(fault==2){*b=NULL;return ENOMEM;}return raw_import(d,p,n,b);}
int nn_bo_destroy(nn_bo *b){return raw_destroy(b);}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int from){return fault==3?EIO:raw_cache(b,o,n,from);}
int nn_queue_submit(nn_queue *q,nn_bo *b,size_t offset,size_t bytes,nn_bo *const *refs,size_t count,uint32_t *ts)
{
 (void)offset;assert(bytes&&count&&count+1<=64-q->count);submits++;q->pending[q->count++]=b;b->busy++;
 for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}
 if(fault==4)return EDEADLK;
 *ts=submits;return 0;
}
int nn_queue_finish(nn_queue *q,uint32_t t){finishes++;return fault==5?ETIMEDOUT:raw_finish(q,t);}
#include NN_TEXELS_PROBE_SOURCE
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"HOST line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
 CK(argc==2);CK(nn_texture_texels_probe(NULL,NULL,0)==-1);
 for(fault=1;fault<=6;fault++){
  struct ntp_owner *old=retained;unsigned s=submits,f=finishes,dc=device_closes;char text[8192];
  CK(nn_texture_texels_probe(argv[1],text,sizeof(text))==-1);CK(strstr(text,"\"passed\":false"));CK(strstr(text,"\"completed\":0"));
  if(fault==4||fault==5){
   CK(retained&&retained!=old&&retained->next==old);CK(retained->device_ready&&!retained->queue_ready&&retained->active&&retained->active->textures_ready[0]&&retained->active->pools_ready[0]);
   CK(retained->active->memory[1]&&!retained->active->pools_ready[1]);CK(strstr(text,"\"resources_retained\":true")&&strstr(text,"\"cleanup_errno\":16"));
   CK(strstr(text,"\"stage\":\"public-write\""));CK(device_closes==dc&&submits==s+1&&finishes==f+(fault==5));
   nn_nvn_clear_error();retained->api.nvnTextureFinalize(retained->active->textures);CK(nn_nvn_error()==EBUSY);nn_nvn_clear_error();
  }else{
   CK(retained==old);CK(strstr(text,"\"resources_retained\":false")&&strstr(text,"\"cleanup_errno\":0"));
   if(fault<4)CK(submits==s&&finishes==f);
   else{CK(submits==s+1&&finishes==f+1);CK(strstr(text,"\"files\":1")&&strstr(text,"\"stage\":\"return-visibility\""));}
   if(fault>1)CK(device_closes==dc+1);
  }
 }
 printf("{\"passed\":true,\"HOST_checks\":%u,\"actual_GPU_submits\":0,\"ambiguous_owners_retained\":2,\"nonexecuting_queue_rejected_before_caller_invalidate\":true}\n",checks);return 0;
}
