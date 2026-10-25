/* SPDX-License-Identifier: MIT
 * HOST ONLY: real common encoder, RAM allocation only. Does not execute an IB. */
#include "nvn_native/compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include NN_GOB_COPY_HEADER
static const struct nn_cs_image *gob=&NVN_COMPILED_IMAGE;
#undef NVN_COMPILED_IMAGE
#include NN_LINEAR_COPY_HEADER
static const struct nn_cs_image *linear=&NVN_COMPILED_IMAGE;
int main(void)
{
    nn_device *device=NULL;assert(nn_device_open(&device)==0);unsigned checks=0;
    const struct nn_cs_image *images[]={gob,linear};
    for(unsigned k=0;k<2;k++){
        const struct nn_cs_image *image=images[k];nn_bo *code=NULL,*source=NULL,*destination=NULL,*uniform=NULL,*job=NULL;
        assert(image->chip==0x06050002&&image->local[0]==8&&image->local[1]==8&&image->ssbo_slots==2);
        assert(nn_bo_alloc(device,image->code_bytes,&code)==0);assert(nn_bo_alloc(device,4096,&source)==0);assert(nn_bo_alloc(device,4096,&destination)==0);assert(nn_bo_alloc(device,4096,&uniform)==0);
        memcpy(nn_bo_map(code),image->code,image->code_bytes);memset(nn_bo_map(source),0x71,4096);memset(nn_bo_map(destination),0x82,4096);
        struct nn_cs_buffer ubos[16]={{0}},ssbos[16]={{0}};ubos[2]=(struct nn_cs_buffer){nn_bo_address(uniform),64};ssbos[0]=(struct nn_cs_buffer){nn_bo_address(source)+4,1024};ssbos[1]=(struct nn_cs_buffer){nn_bo_address(destination)+32,2048};
        const uint32_t groups[3]={4,1,1};size_t bytes=0;assert(nn_cs_encode(device,code,image,ubos,ssbos,groups,&job,&bytes)==0);assert(bytes>0&&bytes<=4096&&nn_bo_size(job)==8192);checks++;
        for(unsigned i=0;i<2;i++){uint32_t expected[16];assert(nn_cs_buffer_descriptor(image,expected,ssbos[i].address,ssbos[i].bytes)==0);assert(!memcmp(expected,(char*)nn_bo_map(job)+4096+i*64,64));checks++;}
        /* No GPU was run, so the real encoder cannot produce converted data. */
        for(unsigned i=0;i<4096;i++){assert(((unsigned char*)nn_bo_map(source))[i]==0x71&&((unsigned char*)nn_bo_map(destination))[i]==0x82);checks++;}
        assert(nn_bo_destroy(job)==0);assert(nn_bo_destroy(uniform)==0);assert(nn_bo_destroy(destination)==0);assert(nn_bo_destroy(source)==0);assert(nn_bo_destroy(code)==0);
    }
    assert(nn_device_close(device)==0);printf("HOST ONLY native encode checks=%u; no GPU execution or CPU image conversion\n",checks);return 0;
}
