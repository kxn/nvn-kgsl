/* HOST RAM only. The real public caller must reach submit, then the unimplemented
 * GPU backend refuses it. A refusal must never become a qualification pass. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "../nvn_pipeline_probe.h"
int main(int argc,char **argv){assert(argc==2);char json[32768];int e=nn_nvn_pipeline_probe(argv[1],json,sizeof(json));
 fprintf(stderr,"return%d %s\n",e,json);assert(e==ENOTSUP);assert(strstr(json,"\"stage\":\"submit\""));assert(strstr(json,"\"cleanup_errno\":0"));
 assert(strstr(json,"\"resources_retained\":false"));assert(strstr(json,"\"completed_cases\":0"));assert(strstr(json,"\"passed\":false"));
 puts("HOST ONLY actual public NVN caller reached submit; GPU backend ENOTSUP preserved, no GPU execution/pass");return 0;}
