/* HOST normal source/FDL catalogue only. No GPU or image conversion. */
#include <stdio.h>
#include "nvn_native/depth_sampled.h"
#include "nvn_native/runtime_fdl.h"
#include NVN_PIPELINE_QUALIFICATION_HEADER
static size_t page(size_t n){return(n+4095)&~(size_t)4095;}
int main(void)
{
 puts("[");for(unsigned f=50;f<=52;f++)for(unsigned role=0;role<5;role++)for(unsigned shape=0;shape<2;shape++){
  unsigned test=(f-50)*10+role*2+shape;
  struct nn_tex_source_desc desc={.format=f,.target=shape?4:1,.width=shape?73:17,.height=shape?41:11,.levels=shape?3:1,.layers=shape?2:1,.block_height_log2=-1};struct nn_tex_source_layout source;if(nn_tex_source_layout(&desc,&source))return 1;
  struct nn_fdl_image_desc d={.schema=1,.chip=0x06050002,.format=37,.target=1,.width=shape?41:33,.height=shape?23:19,.levels=1,.layers=1,.level_count=1,.layer_count=1,.usage=NN_FDL_COLOR_TARGET};struct nn_fdl_image_view view;if(nn_fdl_image(&d,&view))return 2;
  const struct nn_pipeline_native_artifact*a=role<2?&depth_sampled_sample_artifact:role==2?&depth_sampled_fetch_artifact:role==3?&depth_sampled_size_artifact:&depth_sampled_gather_artifact;
  size_t canonical=page(8192+source.payload_bytes),ro=canonical+page(NN_DEPTH_SAMPLED_BLOCK_IMAGE.code_bytes)+4096+8192+page(40964)+page(45060)+4096+page(a->recipe->code_bytes[0])+page(a->recipe->code_bytes[1])-12;
  printf("%s{\"case\":%u,\"format\":%u,\"role\":%u,\"shape\":%u,\"level\":%u,\"layer\":%u,\"source_width\":%u,\"source_height\":%u,\"width\":%u,\"height\":%u,\"origin\":%u,\"pitch\":%u,\"span\":%llu,\"bo_bytes\":%zu,\"canonical_bytes\":%zu,\"resource_count\":%u,\"readonly_bytes\":%zu}",test?",\n":"",test,f,role,shape,shape,shape,source.level[shape].width,source.level[shape].height,d.width,d.height,shape,view.level[0].pitch,(unsigned long long)view.required_span,page(8192+view.required_span),canonical,role<2?12:11,ro);
 }puts("\n]");return 0;
}
