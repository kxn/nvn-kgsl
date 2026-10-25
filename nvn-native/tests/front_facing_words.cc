/* SPDX-License-Identifier: MIT. Register reference from pinned Gallium
 * emit_fs_inputs and actual generated Qualcomm packers; no GPU execution. */
#include <cstdio>
#include <cstring>
#include "fd6_hw.h"
#include "ir3/ir3_shader.h"
#include NN_FRONT_FACING_RECIPE
static unsigned checks;
#define CHECK(c) do{checks++;if(!(c)){fprintf(stderr,"facewords:%u %s\n",__LINE__,#c);return 1;}}while(0)
static uint32_t reg(uint32_t id,unsigned *found)
{
 const auto&r=nn_pipeline_recipe;uint32_t value=0;*found=0;
 for(size_t i=0;i<r.word_count;){uint32_t h=r.words[i++];unsigned count;
  if((h>>28)==4){count=h&0x7f;if(i+count>r.word_count)return ~0u;unsigned base=(h>>8)&0x3ffff;if(id>=base&&id<base+count){value=r.words[i+id-base];(*found)++;}i+=count;}
  else if((h>>28)==7){count=h&0x3fff;if(i+count>r.word_count)return ~0u;i+=count;}else return ~0u;
 }
 return value;
}
int main(void)
{
 unsigned n;uint32_t face=NN_FRONT_FACE_PRESENT?NN_FRONT_FACE_REGID:INVALID_REG;
 uint32_t value=reg(REG_A6XX_SP_REG_PROG_ID_0,&n);CHECK(n==1);CHECK(value==(A6XX_SP_REG_PROG_ID_0_FACEREGID(face)|A6XX_SP_REG_PROG_ID_0_SAMPLEID(INVALID_REG)|A6XX_SP_REG_PROG_ID_0_SAMPLEMASK(INVALID_REG)|A6XX_SP_REG_PROG_ID_0_CENTERRHW(INVALID_REG)));
 value=reg(REG_A6XX_GRAS_CL_INTERP_CNTL,&n);CHECK(n==1);CHECK(bool(value&A6XX_GRAS_CL_INTERP_CNTL_FACENESS)==bool(NN_FRONT_FACE_PRESENT));CHECK(bool(value&A6XX_GRAS_CL_INTERP_CNTL_IJ_LINEAR_PIXEL)==bool(NN_FRONT_FACE_NEED_SIZE));
 value=reg(REG_A6XX_RB_INTERP_CNTL,&n);CHECK(n==1);CHECK(bool(value&A6XX_RB_INTERP_CNTL_IJ_LINEAR_PIXEL)==bool(NN_FRONT_FACE_NEED_SIZE));
 value=reg(REG_A6XX_RB_PS_INPUT_CNTL,&n);CHECK(n==1);CHECK(bool(value&A6XX_RB_PS_INPUT_CNTL_FACENESS)==bool(NN_FRONT_FACE_PRESENT));CHECK(!(value&(A6XX_RB_PS_INPUT_CNTL_SAMPLEID|A6XX_RB_PS_INPUT_CNTL_SAMPLEMASK)));
 value=reg(REG_A6XX_SP_PS_CNTL_0,&n);CHECK(n==1);CHECK(!(value&A6XX_SP_PS_CNTL_0_HALFREGFOOTPRINT__MASK));CHECK((value&A6XX_SP_PS_CNTL_0_FULLREGFOOTPRINT__MASK)==A6XX_SP_PS_CNTL_0_FULLREGFOOTPRINT(NN_FRONT_FACE_FULL+1));CHECK(value&A6XX_SP_PS_CNTL_0_MERGEDREGS);
 printf("{\"passed\":true,\"checks\":%u,\"face_present\":%u,\"gpu_execution\":false}\n",checks,NN_FRONT_FACE_PRESENT);return 0;
}
