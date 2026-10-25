/* CPU authored-oracle consistency only; no GPU/decoder/device. */
#include "bc_sampled_cases.h"
#include <stdio.h>
int main(void)
{
 const unsigned shape[3][2]={{8,8},{37,21},{128,65}};
 for(unsigned f=0x41;f<=0x44;f++)for(unsigned s=0;s<3;s++)for(unsigned r=0;r<2;r++)
  for(unsigned y=0;y<shape[s][1];y++)for(unsigned x=0;x<shape[s][0];x++){
   uint8_t expected[4];nn_bc_test_expected(f,x,y,r,expected);if(fwrite(expected,1,4,stdout)!=4)return 1;
  }
 return fflush(stdout)?1:0;
}
