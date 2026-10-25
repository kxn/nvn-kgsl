/* SPDX-License-Identifier: MIT. Authored input bytes only, no conversion/GPU. */
#include "volume_texture_pattern.h"
#include "volume_shadow_cases.h"
#include <stdio.h>
int main(int argc,char**argv)
{
 if(argc!=2)return 1;
 FILE*f=fopen(argv[1],"wbx");if(!f)return 1;unsigned long long total=0;
 for(unsigned n=0;n<18;n++){struct nn_tex_source_desc d;unsigned base,count;struct nn_tex_source_layout s;if(nn_volume_shadow_case_spec(n,&d,&base,&count)||nn_tex_source_layout(&d,&s))return 1;
 for(unsigned r=0;r<2;r++)for(unsigned l=0;l<s.levels;l++)for(unsigned z=0;z<s.level_depths[l];z++)for(unsigned y=0;y<s.level[l].blocks_y;y++)for(unsigned x=0;x<s.level[l].blocks_x;x++){unsigned char b[16];nn_volume_texture_block(s.format,n,r,l,z,x,y,b);if(fwrite(b,1,s.bytes_per_block,f)!=s.bytes_per_block)return 1;total+=s.bytes_per_block;}}
 if(fclose(f))return 1;
 printf("%llu\n",total);return 0;
}
