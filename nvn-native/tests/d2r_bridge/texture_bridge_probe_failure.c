/* HOST no-GPU result cannot pass; unknown submit keeps heap owner alive. */
#include <stdio.h>
#include <string.h>
int nn_nvn_texture_bridge_probe(const char *,char *,size_t);
void nn_test_ambiguous_next(void);
int main(int argc,char **argv)
{
 if(argc!=3)return 2;
 const int unknown=!strcmp(argv[2],"unknown");if(unknown)nn_test_ambiguous_next();
 char json[32768];int result=nn_nvn_texture_bridge_probe(argv[1],json,sizeof(json));puts(json);
 if(!result||!strstr(json,"\"passed\":false"))return 1;
 if(unknown)return strstr(json,"\"resources_retained\":true")&&strstr(json,"\"cleanup_errno\":16")?0:1;
 return strstr(json,"\"resources_retained\":false")&&strstr(json,"\"submits\":1")&&strstr(json,"\"finishes\":1")?0:1;
}
