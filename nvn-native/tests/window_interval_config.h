/* SPDX-License-Identifier: MIT
 * Private qualification configuration, not a guest API or presentation clock. */
#pragma once
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct nn_window_interval_config { int enabled, interval; };
static int nn_window_interval_read_config(const char *directory,struct nn_window_interval_config *out)
{
    if(!directory||!out)return EINVAL;
    *out=(struct nn_window_interval_config){0,1};
    char path[1400];int n=snprintf(path,sizeof(path),"%s/../window-source-present-interval",directory);
    if(n<0||(size_t)n>=sizeof(path))return ENAMETOOLONG;
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return errno==ENOENT?0:errno; /* No file is the unchanged default qualifier. */
    struct stat st;int e=0;
    if(fstat(fd,&st))e=errno;
    else if(!S_ISREG(st.st_mode)||st.st_uid!=getuid()||(st.st_mode&077)||st.st_size!=36)e=EINVAL;
    char text[37];size_t done=0;
    while(!e&&done<sizeof(text)){
        ssize_t got=read(fd,text+done,sizeof(text)-done);
        if(got<0&&errno==EINTR)continue;
        if(got<0){e=errno;break;}if(!got)break;done+=(size_t)got;
    }
    if(close(fd)&&!e)e=errno;
    if(e)return e;
    if(done!=36)return EINVAL;
    const char *leaf=strrchr(directory,'/');leaf=leaf?leaf+1:directory;
    if(strlen(leaf)<49||memcmp(leaf,"window-source-",14)||memcmp(leaf+14,text,32)||memcmp(leaf+46,"-g",2)||text[32]!=' ')return EINVAL;
    for(unsigned i=0;i<32;i++)if(!((text[i]>='0'&&text[i]<='9')||(text[i]>='a'&&text[i]<='f')))return EINVAL;
    if(leaf[48]<'1'||leaf[48]>'9')return EINVAL;
    for(size_t i=49;leaf[i];i++)if(leaf[i]<'0'||leaf[i]>'9')return EINVAL;
    if(text[33]!='0'||text[34]!='0'||text[35]<'0'||text[35]>'5')return EINVAL;
    *out=(struct nn_window_interval_config){1,text[35]-'0'};return 0;
}
