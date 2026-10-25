/* HOST ONLY original Mesa ONE-texel fixture packing + independent unpack oracle.
 * Not compiled/linked into Android. No game image or shader execution. */
#include NN_CLEAR_PATTERN_SOURCE
#include <stdio.h>
static const unsigned formats[]={1,2,5,6,7,10,13,14,17,18,19,22,37,38,41,42,43,46,56,63,154,156};
static const float authored[8][4]={
 {.25f,.5f,.75f,1},{-.25f,-.5f,.75f,1},{.125f,.375f,.625f,.5f},
 {.875f,.625f,.375f,1},{.25f,.75f,.5f,0},{.5f,-.25f,-.75f,-.5f},
 {1,0,1,1},{0,1,0,1}};
int main(void)
{
 putchar('[');
 for(unsigned f=0;f<sizeof(formats)/sizeof(*formats);f++){
  enum pipe_format fmt;pack_function pack;if(format(formats[f],&fmt,&pack))return 2;
  printf("%s{\"format\":%u,\"mesa\":\"%s\",\"banks\":[",f?",":"",formats[f],util_format_name(fmt));
  for(unsigned bank=0;bank<8;bank++){
   struct nn_clear_color_pattern p;if(nn_clear_color_pattern(formats[f],authored[bank],15,&p))return 3;
   float unpacked[4];util_format_unpack_rgba(fmt,unpacked,p.pattern,1);
   float transformed[4]={fminf(1,fabsf(unpacked[0])),fminf(1,fabsf(unpacked[1])),fminf(1,fabsf(unpacked[2])),unpacked[3]};
   unsigned char output[4];
   /* Actual original FS: white vertex; gamma=contrast=1,mix=0. Its NIR
    * contains fabs before gamma, alpha==0 KIL, then UNORM target conversion.
    * Never infer this math from rendered pixels. */
   if(unpacked[3]==0){const unsigned char clear[4]={17,29,43,59};memcpy(output,clear,4);}
   else util_format_r8g8b8a8_unorm_pack_rgba_float(output,4,transformed,16,1,1);
   printf("%s{\"cpp\":%u,\"packed\":[",bank?",":"",p.bytes_per_pixel);
   for(unsigned i=0;i<16;i++)printf("%s%u",i?",":"",((unsigned char*)p.pattern)[i]);
   printf("],\"unpacked\":[%.9g,%.9g,%.9g,%.9g],\"expected\":[%u,%u,%u,%u]}",unpacked[0],unpacked[1],unpacked[2],unpacked[3],output[0],output[1],output[2],output[3]);
  }
  printf("]}");
 }
 puts("]");return fflush(stdout)?1:0;
}
