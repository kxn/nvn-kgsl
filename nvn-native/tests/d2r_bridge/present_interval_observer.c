/* HOST ABI/scalar trace only. No platform or guest. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int diagnostic,missing,after,value;
static const char *names[]={"nvnWindowBuilderSetPresentInterval","nvnWindowBuilderGetPresentInterval","nvnWindowSetPresentInterval","nvnWindowGetPresentInterval"};
int nn_nvn_error(void){return diagnostic;}
void nn_nvn_clear_error(void){assert(0);}
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out){(void)out;return ENODATA;}
/* This scalar observer fixture has no texture-builder object registry. */
int nn_nvn_get_texture_builder_observation(const NVNtextureBuilder *builder,struct nn_nvn_texture_builder_observation *out)
{(void)builder;(void)out;return ENOENT;}
static void called(void){fputs("raw-called\n",stderr);if(after)diagnostic=ENOTSUP;}
static void bs(NVNwindowBuilder *p,int v){assert(p==(void *)0x1234);value=v;called();}
static int bg(const NVNwindowBuilder *p){assert(p==(void *)0x1234);called();return value;}
static void ws(NVNwindow *p,int v){assert(p==(void *)0x5678);value=v;called();}
static int wg(const NVNwindow *p){assert(p==(void *)0x5678);called();return value;}
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
    fputs("lookup\n",stderr);if(missing)return NULL;
#define F(n,f,t) if(!strcmp(name,names[n])){t check=f;return(PFNNVNGENERICFUNCPTRPROC)check;}
    F(0,bs,PFNNVNWINDOWBUILDERSETPRESENTINTERVALPROC)
    F(1,bg,PFNNVNWINDOWBUILDERGETPRESENTINTERVALPROC)
    F(2,ws,PFNNVNWINDOWSETPRESENTINTERVALPROC)
    F(3,wg,PFNNVNWINDOWGETPRESENTINTERVALPROC)
#undef F
    return NULL;
}
static void invoke(unsigned i,PFNNVNGENERICFUNCPTRPROC f)
{
    switch(i){
    case 0:((PFNNVNWINDOWBUILDERSETPRESENTINTERVALPROC)f)((void *)0x1234,value);break;
    case 1:assert(((PFNNVNWINDOWBUILDERGETPRESENTINTERVALPROC)f)((void *)0x1234)==value);break;
    case 2:((PFNNVNWINDOWSETPRESENTINTERVALPROC)f)((void *)0x5678,value);break;
    case 3:assert(((PFNNVNWINDOWGETPRESENTINTERVALPROC)f)((void *)0x5678)==value);break;
    }
}
int main(void)
{
    unsigned checks=0;
    for(unsigned i=0;i<4;i++){
        PFNNVNGENERICFUNCPTRPROC f=nn_d2r_nvn_get_proc_address(NULL,names[i]);
        for(value=-1;value<=6;value++){invoke(i,f);checks++;}
        for(unsigned fail=0;fail<3;fail++){
            int fd[2];assert(!pipe(fd));pid_t pid=fork();assert(pid>=0);
            if(!pid){close(fd[0]);assert(dup2(fd[1],2)==2);close(fd[1]);
                value=-77;missing=fail==0;diagnostic=fail==1?EBUSY:0;after=fail==2;
                invoke(i,f);_exit(99);}
            close(fd[1]);char text[4096];size_t n=0;ssize_t got;
            while((got=read(fd[0],text+n,sizeof(text)-1-n))>0)n+=(size_t)got;
            close(fd[0]);text[n]=0;int status;assert(waitpid(pid,&status,0)==pid);
            assert(WIFEXITED(status)&&WEXITSTATUS(status)==78);
            assert(strstr(text,names[i])&&strstr(text,"guest_pc=unavailable")&&strstr(text,"terminating execution"));
            char *trace=strstr(text,names[i]),*lookup=strstr(text,"lookup\n"),*raw=strstr(text,"raw-called");
            if(fail==1)assert(!lookup&&!raw);else assert(lookup&&trace<lookup);
            if(fail==2)assert(raw&&strstr(text,"phase=after"));
            if(i%2==0)assert(strstr(text,"interval=-77"));else if(fail==2)assert(!strstr(text,"result="));
            checks++;
        }
    }
    printf("{\"checks\":%u,\"gpu_executes\":0,\"scope\":\"HOST typed scalar trace and sticky termination\"}\n",checks);
}
