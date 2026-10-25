/* HOST ONLY: real provider wrapper, typed synthetic DeviceInitialize driver. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int mode,diagnostic,registrations;
static NVNdevice *const device=(void *)(uintptr_t)UINT64_C(0x123456789abc);
static const NVNdeviceBuilder *const builder=(void *)(uintptr_t)UINT64_C(0xfedcba987650);
int nn_nvn_error(void){return diagnostic;}
void nn_nvn_clear_error(void){diagnostic=0;}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out){(void)out;return ENODATA;}
int nn_nvn_register_compute(NVNdevice *d,const struct nn_nvn_compiled_cs *entry)
{
 assert(d==device&&entry&&entry->source_bytes&&entry->control_bytes==0x898&&entry->image->schema==1);
 ++registrations;fprintf(stderr,"registered=%d\n",registrations);
 if(mode==3&&registrations==2)return ENOMEM;
 if(mode==4)diagnostic=EINVAL;
 return 0;
}
static NVNboolean raw(NVNdevice *d,const NVNdeviceBuilder *b)
{assert(d==device&&b==builder);fputs("raw_initialize\n",stderr);if(mode==2)diagnostic=EINVAL;return mode==0?0:0xa5;}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{assert(!strcmp(name,"nvnDeviceInitialize"));PFNNVNDEVICEINITIALIZEPROC f=raw;return (PFNNVNGENERICFUNCPTRPROC)f;}
int main(void)
{
 PFNNVNDEVICEINITIALIZEPROC initialize=(PFNNVNDEVICEINITIALIZEPROC)nn_d2r_nvn_bootstrap("nvnDeviceInitialize");
 for(mode=0;mode<6;++mode){int fds[2];assert(!pipe(fds));pid_t pid=fork();assert(pid>=0);
  if(!pid){close(fds[0]);assert(dup2(fds[1],2)==2);close(fds[1]);diagnostic=mode==5?EBUSY:0;
   NVNboolean result=initialize(device,builder);assert(result==(mode?0xa5:0));assert(registrations==(mode?NVN_TEST_ENTRY_COUNT:0));_exit(0);}
  close(fds[1]);char text[4096];size_t n=0;ssize_t k;while(n<sizeof(text)-1&&(k=read(fds[0],text+n,sizeof(text)-1-n))>0)n+=(size_t)k;close(fds[0]);text[n]=0;
  int status;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status));assert(WEXITSTATUS(status)==(mode>=2?78:0));
  if(mode==0||mode==2||mode==5)assert(!strstr(text,"registered="));
  if(mode==3)assert(strstr(text,"registered=2")&&strstr(text,"errno=12")&&strstr(text,"phase=offline-cache"));
  if(mode==4)assert(strstr(text,"registered=1")&&strstr(text,"phase=offline-cache"));
  if(mode==1)assert(strstr(text,"runtime_compiler=false"));
 }
 puts("PASS HOST ONLY: 6 init/provider forwarding cases, false/0xa5/sticky/registration failure; no GPU/game");return 0;
}
