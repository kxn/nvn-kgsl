/* SPDX-License-Identifier: MIT. HOST actual public calls; never executes PM4. */
#define _POSIX_C_SOURCE 200809L
#define nn_queue_submit gd_unused_submit
#define nn_queue_finish gd_unused_finish
#include NN_MEMORY_BACKEND
#undef nn_queue_submit
#undef nn_queue_finish
static unsigned submitted,waits;
static int mode;
int nn_queue_submit(nn_queue*q,nn_bo*b,size_t o,size_t n,nn_bo*const*refs,size_t count,uint32_t*t)
{
 if(!q||!b||!t||!n||o>nn_bo_size(b)||n>nn_bo_size(b)-o||(count&&!refs))return EINVAL;
 for(size_t i=0;i<count;i++)if(!refs[i])return EINVAL;
 if(mode==3&&(q->count||count+1>sizeof(q->pending)/sizeof(q->pending[0])))return ENOSPC;
 submitted++;*t=submitted;
 /* Acceptance owns all refs until an actual Finish, even though this HOST
  * fixture never executes PM4. The injected wait error cannot clear busy. */
 if(mode==3){q->pending[q->count++]=b;b->busy++;for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}return 0;}
 if(mode==2){b->busy++;for(size_t i=0;i<count;i++)refs[i]->busy++;return 0;}
 return mode?EIO:ENOTSUP;
}
int nn_queue_finish(nn_queue*q,uint32_t timeout)
{if(!q||!timeout)return EINVAL;waits++;return mode==3?gd_unused_finish(q,timeout):ETIMEDOUT;}
int nn_nvn_pipeline_probe(const char*,char*,size_t);
int main(int argc,char**argv)
{
 if(argc!=3)return 2;
 mode=!strcmp(argv[2],"metadata")?3:!strcmp(argv[2],"finish")?2:!strcmp(argv[2],"unknown");
 char result[32768];int e=nn_nvn_pipeline_probe(argv[1],result,sizeof(result));puts(result);
 if(mode==3){
  /* Completes every real public state/owner path in RAM. The saved target
   * pools contain unexecuted RAM input, not rendered pixels or GPU evidence. */
  if(e||submitted!=72||waits!=72||!strstr(result,"\"completed_cases\":72")||!strstr(result,"\"finishes\":72")||
     !strstr(result,"\"passed\":true")||!strstr(result,"\"resources_retained\":false")||!strstr(result,"\"cleanup_errno\":0")||
     !strstr(result,"\"copy_wrappers\":12")||!strstr(result,"\"call_wrappers\":12"))return 1;
  fprintf(stderr,"HOST all72 public metadata/lifetime paths completed; no PM4 execution; saved RAM is not GPU pixels\n");return 0;
 }
 int want=mode==2?ETIMEDOUT:mode?EIO:ENOTSUP;
 if(e!=want||submitted!=1||waits!=(unsigned)(mode==2)||!strstr(result,"\"completed_cases\":0")||!strstr(result,"\"finishes\":0")||!strstr(result,"\"passed\":false")||!strstr(result,"\"resources_retained\":true")||!strstr(result,"\"cleanup_errno\":16"))return 1;
 if(!strstr(result,mode==2?"\"stage\":\"finish\"":"\"stage\":\"submit\""))return 1;
 fprintf(stderr,"HOST actual original public CASC reached submit/finish failure; complete owner retained; no GPU execution\n");return 0;
}
