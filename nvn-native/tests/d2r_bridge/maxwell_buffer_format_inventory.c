/* HOST metadata only. Actual pinned public NVN constants and current query
 * mapping, not a hardware format table or GPU capability claim. */
#include "nvn_native/texture_buffer_format.h"
#include "nvn.h"
#include <stdio.h>
#define FORMATS(X) \
 X(R8,R8Unorm) X(R8SN,R8Snorm) X(R8UI,R8Uint) X(R8I,R8Sint) \
 X(R16F,R16Float) X(R16,R16Unorm) X(R16SN,R16Snorm) X(R16UI,R16Uint) X(R16I,R16Sint) \
 X(R32F,R32Float) X(R32UI,R32Uint) X(R32I,R32Sint) \
 X(RG8,R8G8Unorm) X(RG8SN,R8G8Snorm) X(RG8UI,R8G8Uint) X(RG8I,R8G8Sint) \
 X(RG16F,R16G16Float) X(RG16,R16G16Unorm) X(RG16SN,R16G16Snorm) X(RG16UI,R16G16Uint) X(RG16I,R16G16Sint) \
 X(RG32F,R32G32Float) X(RG32UI,R32G32Uint) X(RG32I,R32G32Sint) \
 X(RGBA8,R8G8B8A8Unorm) X(RGBA8SN,R8G8B8A8Snorm) X(RGBA8UI,R8G8B8A8Uint) X(RGBA8I,R8G8B8A8Sint) \
 X(RGBA16F,R16G16B16A16Float) X(RGBA16,R16G16B16A16Unorm) X(RGBA16SN,R16G16B16A16Snorm) X(RGBA16UI,R16G16B16A16Uint) X(RGBA16I,R16G16B16A16Sint) \
 X(RGBA32F,R32G32B32A32Float) X(RGBA32UI,R32G32B32A32Uint) X(RGBA32I,R32G32B32A32Sint) \
 X(RGB10A2,R10G10B10A2Unorm) X(RGB10A2UI,R10G10B10A2Uint) X(R11G11B10F,R11G11B10Float)
int main(void)
{
 unsigned count=0;puts("[");
#define ROW(nvn,shader) do{uint32_t f=0,bytes=0;if(nn_tex_buffer_query_format(NVN_FORMAT_##nvn,&f)||nn_tex_buffer_element_bytes(NVN_FORMAT_##nvn,&bytes)||f!=count+1)return 1;printf("%s{\"nvn_name\":\"NVN_FORMAT_"#nvn"\",\"nvn\":%u,\"shader_name\":\""#shader"\",\"shader\":%u,\"bytes\":%u}",count?",\n":"",(unsigned)NVN_FORMAT_##nvn,f,bytes);count++;}while(0);
 FORMATS(ROW)
#undef ROW
 puts("\n]");
 const uint32_t rejected[]={0,34,35,36,0x39,0x40,0x41,0x45,0x9a,UINT32_MAX};
 for(unsigned i=0;i<sizeof(rejected)/sizeof(rejected[0]);i++){
  uint32_t value=123;if(nn_tex_buffer_query_format(rejected[i],&value)!=ENOTSUP||value)return 2;
 }
 if(nn_tex_buffer_query_format(NVN_FORMAT_RGBA32F,0)!=EINVAL)return 3;
 return count==39?0:1;
}
