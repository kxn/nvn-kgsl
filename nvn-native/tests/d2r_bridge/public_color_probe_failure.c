/* SPDX-License-Identifier: MIT. HOST actual public caller, no PM4 execution.
 * Both refused/unknown submit must retain all pending owners, not claim GPU. */
#define _POSIX_C_SOURCE 200809L
#define nn_queue_submit unused_ram_submit
#include NN_MEMORY_BACKEND
#undef nn_queue_submit
static unsigned submitted;
static int failure;
int nn_queue_submit(nn_queue*q,nn_bo*b,size_t o,size_t n,nn_bo*const*refs,size_t count,uint32_t*t)
{
 if(!q||!b||!t||!n||o>nn_bo_size(b)||n>nn_bo_size(b)-o||(count&&!refs))return EINVAL;
 submitted++;*t=submitted;return failure;
}
int nn_nvn_pipeline_probe(const char*,char*,size_t);
int main(int argc,char**argv)
{
 if(argc!=3)return 2;
 failure=!strcmp(argv[2],"unknown")?EIO:ENOTSUP;
 char result[32768];int e=nn_nvn_pipeline_probe(argv[1],result,sizeof(result));puts(result);
 if(e!=failure||submitted!=1||!strstr(result,"\"stage\":\"submit\"")||!strstr(result,"\"completed_cases\":0")||!strstr(result,"\"submissions\":0")||!strstr(result,"\"finishes\":0")||!strstr(result,"\"passed\":false")||!strstr(result,"\"resources_retained\":true")||!strstr(result,"\"cleanup_errno\":16"))return 1;
 fprintf(stderr,"HOST actual publicGOB22 caller reached refused/unknown submit; zero GPU execution, owner group retained\n");return 0;
}
