/* HOST real-file qualifier configuration only. No guest, Android or GPU. */
#define _POSIX_C_SOURCE 200809L
#include "../window_interval_config.h"
#include <assert.h>
#include <stdlib.h>
#include <dirent.h>
static unsigned checks;
#define T(x) do{checks++;assert(x);}while(0)
static void put(const char *path,const void *text,size_t bytes)
{int f=open(path,O_WRONLY|O_CREAT|O_TRUNC,0600);T(f>=0);T(write(f,text,bytes)==(ssize_t)bytes);T(!close(f));T(!chmod(path,0600));}
static unsigned fd_count(void)
{DIR *d=opendir("/proc/self/fd");T(d);unsigned n=0;while(readdir(d))n++;T(!closedir(d));return n;}
int main(void)
{
    char root[]="/tmp/nn-window-interval-XXXXXX";T(mkdtemp(root));char directory[400],path[400],other[400];
    const char *nonce="0123456789abcdef0123456789abcdef";
    T(snprintf(directory,sizeof(directory),"%s/window-source-%s-g1",root,nonce)>0);T(!mkdir(directory,0700));
    T(snprintf(path,sizeof(path),"%s/window-source-present-interval",root)>0);
    T(snprintf(other,sizeof(other),"%s/other",root)>0);
    struct nn_window_interval_config c;T(!nn_window_interval_read_config(directory,&c)&&!c.enabled&&c.interval==1);
    T(nn_window_interval_read_config(NULL,&c)==EINVAL);T(nn_window_interval_read_config(directory,NULL)==EINVAL);
    unsigned before=fd_count();char text[37];
    for(int interval=0;interval<=5;interval++){
        T(snprintf(text,sizeof(text),"%s %03d",nonce,interval)==36);put(path,text,36);
        T(!nn_window_interval_read_config(directory,&c)&&c.enabled&&c.interval==interval);
    }
    for(int i=0;i<36;i++){
        char bad[37];memcpy(bad,text,37);bad[i]='!';put(path,bad,36);
        T(nn_window_interval_read_config(directory,&c)==EINVAL&&!c.enabled&&c.interval==1);
    }
    for(unsigned size=0;size<=37;size++)if(size!=36){put(path,text,size);T(nn_window_interval_read_config(directory,&c)==EINVAL&&!c.enabled);}
    const char *values[]={"006","255","-01"," 01","1.0"};
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){memcpy(text+33,values[i],3);put(path,text,36);T(nn_window_interval_read_config(directory,&c)==EINVAL);}
    T(snprintf(text,sizeof(text),"%s 003",nonce)==36);put(path,text,36);
    const char *leaves[]={"wrong","window-source-0123456789abcdef0123456789abcdef-g0","window-source-0123456789abcdef0123456789abcdef-g01","window-source-0123456789abcdef0123456789abcdef-g1x","window-source-1123456789abcdef0123456789abcdef-g1"};
    for(unsigned i=0;i<sizeof(leaves)/sizeof(leaves[0]);i++){char bad[400];T(snprintf(bad,sizeof(bad),"%s/%s",root,leaves[i])>0);T(!mkdir(bad,0700));T(nn_window_interval_read_config(bad,&c)==EINVAL);T(!rmdir(bad));}
    T(!chmod(path,0644));T(nn_window_interval_read_config(directory,&c)==EINVAL);T(!chmod(path,0600));
    T(!rename(path,other));T(!symlink(other,path));T(nn_window_interval_read_config(directory,&c)==ELOOP);T(!unlink(path));
    T(!mkdir(path,0700));T(nn_window_interval_read_config(directory,&c)==EINVAL);T(!rmdir(path));
    T(!mkfifo(path,0600));T(nn_window_interval_read_config(directory,&c)==EINVAL);T(!unlink(path));T(!unlink(other));
    T(fd_count()==before);T(!rmdir(directory));T(!rmdir(root));
    printf("{\"passed\":true,\"checks\":%u,\"gpu_executes\":0,\"scope\":\"HOST private real-file config only\"}\n",checks);
}
