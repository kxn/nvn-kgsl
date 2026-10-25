/* SPDX-License-Identifier: MIT. HOST actual FDL metadata only, no pixels. */
#include "volume_texture_cases.h"
#include "volume_shadow_cases.h"
#include <stdio.h>
int main(void)
{
 puts("[");for(unsigned n=0;n<18;n++){struct nn_tex_source_desc d;unsigned base,count;struct nn_tex_source_layout s;struct nn_volume_shadow_plan p;struct nn_tex_native_view t;
 if(nn_volume_shadow_case_spec(n,&d,&base,&count)||nn_tex_source_layout(&d,&s)||nn_volume_shadow_plan(&s,base,count,&p)||nn_volume_texture_target(p.native.view_width,p.native.view_height,&t))return 1;
 printf("%s{\"id\":%u,\"width\":%u,\"height\":%u,\"pitch\":%u,\"target_span\":%u,\"target_bo_bytes\":%llu,\"target_hex\":\"",n?",":"",n,t.width,t.height,t.pitch,t.required_span_bytes,(unsigned long long)((t.required_span_bytes+4096ull+4095)&~4095ull));const unsigned char*b=(const unsigned char*)&t;for(size_t i=0;i<sizeof(t);i++)printf("%02x",b[i]);puts("\"}");}puts("]");return 0;
}
