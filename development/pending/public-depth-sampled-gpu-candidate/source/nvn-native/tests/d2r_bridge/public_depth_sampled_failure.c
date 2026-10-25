/* SPDX-License-Identifier: MIT. HOST actual public calls; never executes PM4. */
#define _POSIX_C_SOURCE 200809L
#define nn_queue_submit gd_unused_submit
#define nn_queue_finish gd_unused_finish
#include NN_MEMORY_BACKEND
#undef nn_queue_submit
#undef nn_queue_finish
#include "nvn_native/pm4.h"
static unsigned submitted,waits,rectangles,scissor_negatives;
/* Follow the actual submitted IB graph at packet boundaries. Scanning all
 * referenced BOs would count unused command copies and cannot prove execution. */
static int rectangles_in(nn_bo*b,size_t offset,size_t bytes,nn_bo*const*refs,size_t count,uint32_t tl,uint32_t br,unsigned depth,unsigned*found){
 if(depth>32||offset%4||bytes%4||offset>nn_bo_size(b)||bytes>nn_bo_size(b)-offset)return EINVAL;
 const uint32_t*p=(const uint32_t*)((const unsigned char*)nn_bo_map(b)+offset);size_t words=bytes/4;
 for(size_t at=0;at<words;){unsigned n=nvn_pm4_packet_words(p[at]);if(!n||n>words-at)return EINVAL;
  if(at+8<=words&&p[at]==nvn_pm4_packet4(0x80d0,1)&&p[at+1]==tl&&p[at+2]==nvn_pm4_packet4(0x80d1,1)&&p[at+3]==br&&p[at+4]==nvn_pm4_packet4(0x80b0,1)&&p[at+5]==tl&&p[at+6]==nvn_pm4_packet4(0x80b1,1)&&p[at+7]==br)(*found)++;
  if(p[at]==nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3)){
   uint64_t address=p[at+1]|((uint64_t)p[at+2]<<32);size_t span=(size_t)p[at+3]*4;nn_bo*child=NULL;size_t child_offset=0;
   for(size_t i=0;i<count;i++){uint64_t base=nn_bo_address(refs[i]);if(address>=base&&address-base<=nn_bo_size(refs[i])&&span<=nn_bo_size(refs[i])-(address-base)){if(child&&child!=refs[i])return EINVAL;child=refs[i];child_offset=(size_t)(address-base);}}
   if(!child)return EINVAL;
   int e=rectangles_in(child,child_offset,span,refs,count,tl,br,depth+1,found);if(e)return e;
  }at+=n;
 }return 0;
}
static int mode;
int nn_queue_submit(nn_queue*q,nn_bo*b,size_t o,size_t n,nn_bo*const*refs,size_t count,uint32_t*t)
{
 if(!q||!b||!t||!n||o>nn_bo_size(b)||n>nn_bo_size(b)-o||(count&&!refs))return EINVAL;
 for(size_t i=0;i<count;i++)if(!refs[i])return EINVAL;
 if(mode==3&&(q->count||count+1>sizeof(q->pending)/sizeof(q->pending[0])))return ENOSPC;
 unsigned test=submitted/2,shape=(test/6)%2,width=shape?73:64,height=shape?41:64;
 uint32_t tl=5u|(3u<<16),br=(width-8)|((2+height/2)<<16);unsigned found=0;
 int packet_error=rectangles_in(b,o,n,refs,count,tl,br,0,&found);if(packet_error)return packet_error;
 if(found!=1)return EPROTO;
 rectangles++;submitted++;*t=submitted;
 /* Acceptance owns all refs until an actual Finish, even though this HOST
  * fixture never executes PM4. The injected wait error cannot clear busy. */
 if(mode==3){q->pending[q->count++]=b;b->busy++;for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}return 0;}
 if(mode==2){b->busy++;for(size_t i=0;i<count;i++)refs[i]->busy++;return 0;}
 return mode?EIO:ENOTSUP;
}
int nn_queue_finish(nn_queue*q,uint32_t timeout)
{if(!q||!timeout)return EINVAL;waits++;return mode==3?gd_unused_finish(q,timeout):ETIMEDOUT;}
int nn_public_depth_sampled_bad_scissors(NVNcommandBuffer*cmd,unsigned w,unsigned h){
 PFNNVNCOMMANDBUFFERSETSCISSORPROC sc=(PFNNVNCOMMANDBUFFERSETSCISSORPROC)nvnBootstrapLoader("nvnCommandBufferSetScissor");
 PFNNVNCOMMANDBUFFERDRAWARRAYSPROC draw=(PFNNVNCOMMANDBUFFERDRAWARRAYSPROC)nvnBootstrapLoader("nvnCommandBufferDrawArrays");
 if(!sc||!draw)return ENOSYS;
 const int invalid[6][4]={{-1,0,1,1},{0,-1,1,1},{0,0,(int)w+1,1},{0,0,1,(int)h+1},{(int)w,0,1,1},{0,(int)h,1,1}};
 for(unsigned i=0;i<6;i++){sc(cmd,invalid[i][0],invalid[i][1],invalid[i][2],invalid[i][3]);int e=nn_nvn_error();if(i<2){if(e!=EINVAL)return EPROTO;nn_nvn_clear_error();scissor_negatives++;continue;}if(e)return EPROTO;draw(cmd,NVN_DRAW_PRIMITIVE_TRIANGLES,0,3);e=nn_nvn_error();if(e!=ENOTSUP)return EPROTO;nn_nvn_clear_error();scissor_negatives++;}
 return 0;
}
int nn_nvn_pipeline_probe(const char*,char*,size_t);
int main(int argc,char**argv)
{
 if(argc!=3)return 2;
 mode=!strcmp(argv[2],"metadata")?3:!strcmp(argv[2],"finish")?2:!strcmp(argv[2],"unknown");
 char result[32768];int e=nn_nvn_pipeline_probe(argv[1],result,sizeof(result));puts(result);
 if(mode==3){
  /* Completes every real public state/owner path in RAM. The saved target
   * pools contain unexecuted RAM input, not rendered pixels or GPU evidence. */
  if(e||scissor_negatives!=432||rectangles!=144||submitted!=144||waits!=144||!strstr(result,"\"completed_cases\":144")||!strstr(result,"\"finishes\":144")||
     !strstr(result,"\"passed\":true")||!strstr(result,"\"resources_retained\":false")||!strstr(result,"\"cleanup_errno\":0")||
     !strstr(result,"\"copy_wrappers\":24")||!strstr(result,"\"call_wrappers\":24"))return 1;
  fprintf(stderr,"HOST all144 public metadata/lifetime paths and actual PM4 scissor rectangles completed; no PM4 execution; saved RAM is not GPU pixels\n");return 0;
 }
 int want=mode==2?ETIMEDOUT:mode?EIO:ENOTSUP;
 if(e!=want||submitted!=1||waits!=(unsigned)(mode==2)||!strstr(result,"\"completed_cases\":0")||!strstr(result,"\"finishes\":0")||!strstr(result,"\"passed\":false")||!strstr(result,"\"resources_retained\":true")||!strstr(result,"\"cleanup_errno\":16"))return 1;
 if(!strstr(result,mode==2?"\"stage\":\"finish\"":"\"stage\":\"submit\""))return 1;
 fprintf(stderr,"HOST actual original public depth-source reached submit/finish failure; complete owner retained; no GPU execution\n");return 0;
}
