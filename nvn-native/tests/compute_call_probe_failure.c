/* HOST actual public original-CS caller: native backend refuses submission.
 * This refusal must never become actual GPU qualification. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
int nn_maxwell_compute_probe(char *,size_t);
int main(void)
{
 char json[32768];int e=nn_maxwell_compute_probe(json,sizeof(json));
 fprintf(stderr,"result%d %s\n",e,json);assert(e!=0);
 assert(strstr(json,"\"stage\":\"submit-call\"")&&strstr(json,"\"errno\":95")&&strstr(json,"\"cleanup_errno\":0"));
 assert(strstr(json,"\"completed_cases\":0")&&strstr(json,"\"resources_retained\":false")&&strstr(json,"\"GPU_effective_bindings_verified\":false")&&strstr(json,"\"passed\":false"));
 puts("HOST ONLY public four-CS Call caller reached submit; GPU ENOTSUP preserved, no GPU execution/pass");return 0;
}
