/* HOST/Android qualifier config only. No production NVN/runtime policy. */
#pragma once
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int nn_texture_bridge_diag_config(const char *directory,unsigned *mask)
{
    if(!directory||!mask)return EINVAL;
    *mask=255;
    char path[1400];int length=snprintf(path,sizeof(path),"%s/../texture-bridge-diagnostic-mask",directory);
    if(length<0||(size_t)length>=sizeof(path))return ENAMETOOLONG;
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0){
#if !defined(__ANDROID__) && !defined(NN_TEX_BRIDGE_REQUIRE_CONFIG)
        if(errno==ENOENT)return 0; /* RAM fixtures never pretend Android identity. */
#endif
        return errno;
    }
    struct stat st;int e=0;
    if(fstat(fd,&st))e=errno;
    else if(!S_ISREG(st.st_mode)||st.st_uid!=getuid()||st.st_size!=36)e=EINVAL;
    char text[37];size_t done=0;
    while(!e&&done<sizeof(text)){
        ssize_t n=read(fd,text+done,sizeof(text)-done);
        if(n<0&&errno==EINTR)continue;
        if(n<0){e=errno;break;}if(!n)break;done+=(size_t)n;
    }
    if(close(fd)&&!e)e=errno;
    if(e)return e;
    if(done!=36)return EINVAL;
    const char *leaf=strrchr(directory,'/');leaf=leaf?leaf+1:directory;
    if(strlen(leaf)!=47||memcmp(leaf,"texture-bridge-",15)||memcmp(leaf+15,text,32)||text[32]!=' ')return EINVAL;
    for(unsigned i=0;i<32;i++)if(!((text[i]>='0'&&text[i]<='9')||(text[i]>='a'&&text[i]<='f')))return EINVAL;
    unsigned value=0;
    for(unsigned i=33;i<36;i++){if(text[i]<'0'||text[i]>'9')return EINVAL;value=value*10+(unsigned)(text[i]-'0');}
    if(value>255)return EINVAL;
    *mask=value;return 0;
}
