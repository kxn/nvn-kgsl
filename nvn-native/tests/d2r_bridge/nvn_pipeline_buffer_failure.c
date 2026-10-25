/* HOST ONLY no PM4 execution: a refused/unknown/unexecuted GPU job cannot pass. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
int nn_nvn_pipeline_probe(const char*,char*,size_t);
void nn_test_ambiguous_next(void);
int main(int argc,char**argv)
{
 if(argc!=3)return 2;
 int unknown=!strcmp(argv[2],"unknown");if(unknown)nn_test_ambiguous_next();
 char json[32768];int e=nn_nvn_pipeline_probe(argv[1],json,sizeof(json));puts(json);
 if(!e||!strstr(json,"\"passed\":false")||!strstr(json,"\"completed_cases\":0"))return 1;
 if(unknown)return e==EIO&&strstr(json,"\"resources_retained\":true")&&strstr(json,"\"cleanup_errno\":16")?0:1;
 return e==EILSEQ&&strstr(json,"\"resources_retained\":false")&&strstr(json,"\"submissions\":1")&&strstr(json,"\"finishes\":1")?0:1;
}
