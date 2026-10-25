/* SPDX-License-Identifier: MIT. HOST actual FDL only, no pixel oracle. */
#include "buffer_texture_cases.h"
#include "volume_texture_cases.h"
#include <stdio.h>
#include NVN_BUFFER_SHADER_HEADER
int main(int argc,char**argv)
{
 if(argc==2){FILE*f=fopen(argv[1],"wb");if(!f)return 1;for(unsigned n=0;n<43;n++){struct nn_buffer_texture_case c;if(nn_buffer_texture_case(n,&c))return 1;for(unsigned r=0;r<2;r++)for(size_t i=0;i<c.source_bytes;i++){unsigned char v=nn_buffer_texture_byte(&c,r,i);if(fwrite(&v,1,1,f)!=1)return 1;}}return fclose(f)?1:0;}
 struct nn_tex_native_view t;if(argc!=1||nn_volume_texture_target(64,64,&t))return 1;
 printf("{\"target\":{\"width\":%u,\"height\":%u,\"pitch\":%u,\"span\":%u,\"bo_bytes\":%u,\"hex\":\"",t.width,t.height,t.pitch,t.required_span_bytes,(t.required_span_bytes+8191)&~4095u);const unsigned char*b=(const unsigned char*)&t;for(size_t i=0;i<sizeof(t);i++)printf("%02x",b[i]);printf("\"},\"cases\":[");
 for(unsigned n=0;n<43;n++){struct nn_buffer_texture_case c;if(nn_buffer_texture_case(n,&c))return 1;struct nn_fdl_buffer_desc d={1,0x06050002,c.format,c.elements*c.cpp,0x100000+c.source_offset};struct nn_fdl_buffer_view v;if(nn_fdl_buffer(&d,&v)||v.elements!=c.elements||v.required_prefix!=c.prefix)return 1;
 printf("%s{\"id\":%u,\"format\":%u,\"elements\":%u,\"cpp\":%u,\"prefix\":%u,\"float_result\":%u,\"source_offset\":%u,\"source_bytes\":%u}",n?",":"",c.id,c.format,c.elements,c.cpp,c.prefix,c.float_result,c.source_offset,c.source_bytes);}printf("],\"shader_metadata\":[");const struct nn_pipeline_native_artifact*a[]={&buffer_float_artifact,&buffer_size_artifact};for(unsigned m=0;m<2;m++){unsigned code=0,ubo=0;for(unsigned k=0;k<2;k++)code+=(a[m]->recipe->code_bytes[k]+4095)&~4095u;for(size_t k=0;k<a[m]->recipe->patch_count;k++)ubo|=a[m]->recipe->patches[k].role==NN_PIPELINE_UBO;printf("%s{\"mode\":%u,\"code_bo_bytes\":%u,\"has_ubo\":%u}",m?",":"",m,code,ubo);}puts("]}");return 0;
}
