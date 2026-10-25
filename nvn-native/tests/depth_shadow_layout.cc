/* HOST-only source/FDL metadata and authored input, no GPU/reference shader. */
#include "depth_shadow_cases.h"
#include "nvn_native/depth_shadow.h"
#include "nvn_native/pipeline_views.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main(int argc,char**argv)
{
 if(argc==2&&!strcmp(argv[1],"--inputs")){
  for(unsigned n=0;n<NN_DEPTH_SHADOW_CASES;n++){struct nn_depth_shadow_case c={};if(nn_depth_shadow_case(n,&c))return 1;size_t bytes=(8192+c.source.payload_bytes+4095)&~size_t(4095);unsigned char*map=(unsigned char*)malloc(bytes);if(!map)return 2;
   for(unsigned r=0;r<2;r++){if(nn_depth_fixture_input(&c,n,r,map,bytes)||fwrite(map,1,bytes,stdout)!=bytes)return 3;}free(map);
  }return 0;
 }
 struct nn_pipeline_views*views=nullptr;if(nn_pipeline_views_create(0x06050002,&views))return 4;
 puts("[");for(unsigned n=0;n<NN_DEPTH_SHADOW_CASES;n++){
  struct nn_depth_shadow_case c={};if(nn_depth_shadow_case(n,&c))return 1;struct nn_depth_shadow_plan p={};if(nn_depth_shadow_plan(&c.source,c.level,c.layer,&p))return 2;
  const nn_tex_native_view*color=nullptr;if(c.role<2&&nn_pipeline_views_get(views,c.source.level[c.level].width,c.source.level[c.level].height,0,&color))return 5;
  printf("%s{\"case\":%u,\"format\":%u,\"profile\":%u,\"selected\":%u,\"role\":%u,\"origin\":%u,\"level\":%u,\"layer\":%u,\"compare\":%u,\"width\":%u,\"height\":%u,\"cpp\":%u,\"layers\":%u,\"payload\":%llu,\"layer_stride\":%llu,\"canonical_bytes\":%llu,\"linear_pitch\":%u,\"linear_bytes\":%llu,\"native_bytes\":%llu,\"levels\":[",n?",\n":"",n,c.format,c.profile,c.selected,c.role,c.origin,c.level,c.layer,c.compare,c.source.level[c.level].width,c.source.level[c.level].height,c.source.bytes_per_block,c.source.layers,(unsigned long long)c.source.payload_bytes,(unsigned long long)c.source.layer_stride,(unsigned long long)((8192+c.source.payload_bytes+4095)&~UINT64_C(4095)),p.aliases.linear.pitch,(unsigned long long)p.aliases.linear.bytes,(unsigned long long)p.aliases.tiled.bytes);
  for(unsigned l=0;l<3;l++){const nn_tex_source_level&m=c.source.level[l];printf("%s{\"offset\":%llu,\"bytes\":%llu,\"width\":%u,\"height\":%u,\"pitch\":%u,\"bh\":%u}",l?",":"",(unsigned long long)m.offset,(unsigned long long)m.bytes,m.width,m.height,m.pitch,m.block_height_log2);}printf("],\"color_pitch\":%u,\"color_bytes\":%llu}",color?color->pitch:0,(unsigned long long)(color?((8192+color->required_span_bytes+4095)&~UINT64_C(4095)):0));
 }puts("\n]");nn_pipeline_views_destroy(views);return 0;
}
