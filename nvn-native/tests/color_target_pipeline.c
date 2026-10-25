/* HOST ONLY unit of actual common target/sampled role + BO range admission.
 * Dummy BO identity/address metadata; no device, allocation, queue or GPU. */
#ifndef NN_COLOR_PIPELINE_SOURCE
#define NN_COLOR_PIPELINE_SOURCE "../src/pipeline.c"
#endif
#include NN_COLOR_PIPELINE_SOURCE
#include "nvn_native/color_target.h"
#include <stdio.h>
struct nn_bo {uint64_t address;size_t size;};
uint64_t nn_bo_address(const nn_bo *bo){return bo?bo->address:0;}
size_t nn_bo_size(const nn_bo *bo){return bo?bo->size:0;}
static unsigned checks;
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"target-role:%d check%u\n",__LINE__,checks);return 1;}}while(0)
int main(void)
{
 const uint32_t formats[]={1,2,5,6,7,10,13,14,17,18,19,22,37,38,41,42,43,46,154,56,156,63};
 for(unsigned i=0;i<sizeof(formats)/sizeof(formats[0]);i++){
  struct nn_tex_native_view v;C(!nn_color_target_create(0x06050002,formats[i],197,113,0,&v,0));
  struct nn_bo bo={UINT64_C(0x123450000),v.required_span_bytes+4096u};
  struct nn_pipeline_image image={&v,{&bo,0,v.required_span_bytes}};uint64_t address=0;
#if NN_PIPELINE_COLOR_TARGETS
  C(!target_layout(&v));C(!target_image(&image,&address)&&address==bo.address);
  image.buffer.bytes--;C(target_image(&image,&address)==EINVAL);image.buffer.bytes++;
  image.buffer.offset=bo.size;C(target_image(&image,&address)==EINVAL);image.buffer.offset=0;
  bo.address=0;C(target_image(&image,&address));bo.address=UINT64_C(0x123450001);C(target_image(&image,&address));
#else
  C((!target_layout(&v))==(formats[i]==37));C((!target_image(&image,&address))==(formats[i]==37));
#endif
  /* New target metadata cannot broaden the old ordinary sampled-image path. */
  C((!image_layout(&v))==(formats[i]==37));
 }
 printf("{\"passed\":true,\"checks\":%u,\"target_color_enabled\":%s,\"sampled_gate_unchanged\":true,\"gpu_execution\":false}\n",checks,NN_PIPELINE_COLOR_TARGETS?"true":"false");return 0;
}
