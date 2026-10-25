/* SPDX-License-Identifier: MIT. HOST hardware-word/API policy checks, no GPU. */
#include "nvn_native/pipeline_frontend.h"
#include "nvn_native/render_state.h"
#include <nvn.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifndef NN_PIPELINE_FRONT_FACING
#define NN_PIPELINE_FRONT_FACING 0
#endif
static unsigned checks;
#define CHECK(c) do{checks++;if(!(c)){fprintf(stderr,"front_facing:%u %s\n",__LINE__,#c);return 1;}}while(0)
int main(void)
{
 struct nn_pipeline_frontend_queries s={.version=1,.chip=0x06050002,.primitive=4,.point_size=1};nn_render_state_defaults(&s.render);
 struct nn_pipeline_query q={.name="QueryHostHasFrontFacingBug",.type=NN_PIPELINE_QUERY_BOOL,.count=1};uint32_t v[32];size_t n;
 for(unsigned stage=0;stage<2;stage++)for(unsigned origin=0;origin<2;origin++)for(unsigned ff=0;ff<2;ff++){
  s.window_origin=origin;s.render.front_face=ff;n=99;memset(v,0xa5,sizeof(v));int e=nn_pipeline_frontend_query(&s,stage,&q,v,&n);
  if(NN_PIPELINE_FRONT_FACING){CHECK(!e&&n==1&&v[0]==0);for(unsigned i=1;i<32;i++)CHECK(v[i]==0xa5a5a5a5u);}
  else CHECK(e==ENOTSUP&&n==0);
 }
#if NN_PIPELINE_FRONT_FACING
 q.arguments=1;n=99;CHECK(nn_pipeline_frontend_query(&s,1,&q,v,&n)==EINVAL&&!n);q.arguments=0;
 q.count=2;n=99;CHECK(nn_pipeline_frontend_query(&s,1,&q,v,&n)==EINVAL&&!n);q.count=1;
 q.type=NN_PIPELINE_QUERY_I32;n=99;CHECK(nn_pipeline_frontend_query(&s,1,&q,v,&n)==EINVAL&&!n);q.type=NN_PIPELINE_QUERY_BOOL;
#endif
 s.chip=0x43030b00;n=99;CHECK(nn_pipeline_frontend_query(&s,1,&q,v,&n)==EINVAL&&!n);s.chip=0x06050002;
 n=99;CHECK(nn_pipeline_frontend_query(&s,2,&q,v,&n)==EINVAL&&!n);
 q.name="QueryHostUnknownFrontFaceWorkaround";n=99;CHECK(nn_pipeline_frontend_query(&s,1,&q,v,&n)==ENOTSUP&&!n);
 /* Actual A6 GRAS_SU_CNTL cull and front-CW fields. Window origin affects
  * viewport sign separately; no hidden XOR is introduced into this encoder. */
 for(unsigned origin=0;origin<2;origin++)for(unsigned ff=0;ff<2;ff++)for(unsigned cull=0;cull<4;cull++){
  struct nn_render_state state;nn_render_state_defaults(&state);state.front_face=ff;state.cull_face=cull;
  struct nn_render_target_info t={.chip=0x06050002,.window_origin=origin,.color_targets=1,.samples=1};uint32_t words[64];for(unsigned i=0;i<64;i++)words[i]=0xa5a5a5a5u;size_t count=99;int e=nn_render_state_encode(&state,&t,words,64,&count);
  if(!NN_PIPELINE_FRONT_FACING&&origin&&cull){CHECK(e==ENOTSUP&&!count);for(unsigned i=0;i<64;i++)CHECK(words[i]==0xa5a5a5a5u);continue;}
  CHECK(!e&&count==48);unsigned found=0;
  for(size_t i=0;i<count;i+=2)if(((words[i]>>8)&0x3ffff)==0x8090){CHECK((words[i+1]&7)==(cull|((ff==NVN_FRONT_FACE_CW)<<2)));found++;}
  CHECK(found==1);
  size_t small=99;uint32_t short_words[2]={0xa5a5a5a5u,0xa5a5a5a5u};CHECK(nn_render_state_encode(&state,&t,short_words,2,&small)==ENOSPC&&!small);CHECK(short_words[0]==0xa5a5a5a5u&&short_words[1]==0xa5a5a5a5u);
 }
 printf("{\"passed\":true,\"checks\":%u,\"front_facing_policy\":%u,\"gpu_execution\":false,\"integer_workaround_value\":0}\n",checks,NN_PIPELINE_FRONT_FACING);return 0;
}
