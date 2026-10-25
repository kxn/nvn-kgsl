/* SPDX-License-Identifier: MIT. HOST fault injection, not GPU execution.
 * Links the real common compute.c and the normal generated kernel artifact. */
#include "nvn_native/compute.h"
#include "nvn_native/pm4.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef NN_COMPUTE_OWNER_IMAGE
#error normal generated kernel header required
#endif
#include NN_COMPUTE_OWNER_IMAGE
struct nn_device {uint32_t chip;};
struct nn_bo {unsigned char *data;size_t bytes;uint64_t address;unsigned live,owned;};
static unsigned checks,allocs,frees,destroy_calls,map_calls,live,alloc_fail,map_fail,destroy_fail;
static unsigned retained,retried,cases,direct_success,indirect_success;
static struct nn_bo *only;
#define C(x) do{checks++;assert(x);}while(0)
uint32_t nn_device_chip(const nn_device *d){C(d!=NULL);return d->chip;}
size_t nn_bo_size(const nn_bo *b){C(b&&b->live);return b->bytes;}
uint64_t nn_bo_address(const nn_bo *b){C(b&&b->live);return b->address;}
void *nn_bo_map(const nn_bo *b){C(b&&b->live);map_calls++;if(map_fail==map_calls)return NULL;return b->data;}
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **out)
{
 C(d&&out);*out=NULL;C(n==8192);if(alloc_fail)return ENOMEM;C(!only&&!live);
 nn_bo *b=calloc(1,sizeof(*b));C(b);b->data=malloc(n+64);C(b->data);memset(b->data,0xcc,n);memset(b->data+n,0x5b,64);
 b->bytes=n;b->address=UINT64_C(0x123450000);b->live=b->owned=1;only=b;live++;allocs++;*out=b;return 0;
}
int nn_bo_destroy(nn_bo *b)
{
 C(b&&b==only&&b->live&&b->owned);destroy_calls++;
 for(unsigned i=0;i<64;i++)C(b->data[b->bytes+i]==0x5b);
 if(destroy_fail){unsigned e=destroy_fail;destroy_fail=0;return (int)e;}
 C(live==1);live--;frees++;only=NULL;b->live=0;free(b->data);free(b);return 0;
}
static struct nn_device device={0x06050002};
static struct nn_bo code;
static struct nn_cs_buffer ubos[16],ssbos[16];
static const uint32_t groups[3]={3,2,1};
static unsigned char kernel_copy[16384];
static void reset(void){C(!live&&!only);alloc_fail=map_fail=destroy_fail=map_calls=0;}
static void finish_error(int got,int expected,nn_bo *owner,size_t bytes,unsigned failed_destroy)
{
 C(got==expected&&bytes==0);
 if(failed_destroy){C(owner==only&&owner&&owner->live&&live==1);retained++;C(!nn_bo_destroy(owner));retried++;}
 else C(!owner&&!only&&!live);
 C(!memcmp(NVN_COMPILED_IMAGE.code,kernel_copy,NVN_COMPILED_IMAGE.code_bytes));cases++;
}
static void bad_direct(const struct nn_cs_image *image,unsigned mode,int ordinary)
{
 reset();unsigned fault=mode? (mode==1?EIO:EBUSY):0;destroy_fail=fault;
 nn_bo *owner=(nn_bo*)(uintptr_t)1;size_t bytes=77;
 int e=nn_cs_encode(&device,&code,image,ubos,ssbos,groups,&owner,&bytes);
 finish_error(e,fault?(int)fault:ordinary,owner,bytes,fault);
}
static void bad_indirect(const struct nn_cs_image *image,unsigned fail_map,unsigned mode,int ordinary)
{
 reset();map_fail=fail_map;unsigned fault=mode? (mode==1?EIO:EBUSY):0;destroy_fail=fault;
 nn_bo *owner=(nn_bo*)(uintptr_t)1;size_t bytes=77;
 int e=nn_cs_encode_indirect(&device,&code,image,ubos,ssbos,UINT64_C(0x998800040),&owner,&bytes);
 finish_error(e,fault?(int)fault:ordinary,owner,bytes,fault);
}
int main(void)
{
 const struct nn_cs_image *normal=&NVN_COMPILED_IMAGE;
 C(normal->chip==0x06050002&&normal->local[0]==8&&normal->local[1]==8&&normal->local[2]==1);
 C(normal->code_bytes<=sizeof(kernel_copy)&&normal->patch_count<64&&normal->command_words<1024);
 memcpy(kernel_copy,normal->code,normal->code_bytes);
 code=(struct nn_bo){(unsigned char*)normal->code,normal->code_bytes,UINT64_C(0x456780000),1,0};
 ubos[2]=(struct nn_cs_buffer){UINT64_C(0x678900000),64};
 ssbos[0]=(struct nn_cs_buffer){UINT64_C(0x789a00004),124};ssbos[1]=(struct nn_cs_buffer){UINT64_C(0x789b0003c),128};
 /* Successful direct output: every nonpatch command word stays the original
  * normal producer word. Patch/descriptor words are checked by source kind. */
 reset();nn_bo *owner=NULL;size_t bytes=0;
 C(!nn_cs_encode(&device,&code,normal,ubos,ssbos,groups,&owner,&bytes));
 C(owner==only&&bytes==normal->command_words*4&&map_calls==1);
 uint32_t expected[1024];memcpy(expected,normal->commands,bytes);unsigned marked[1024]={0};
 for(size_t i=0;i<normal->patch_count;i++){
  const struct nn_cs_patch *p=normal->patches+i;C(p->word<normal->command_words);uint64_t value=0;
  if(p->kind==NN_CS_GROUP_COUNT||p->kind==NN_CS_GLOBAL_SIZE){value=groups[p->index]*(p->kind==NN_CS_GLOBAL_SIZE?normal->local[p->index]:1);expected[p->word]=(uint32_t)value;marked[p->word]=1;continue;}
  if(p->kind==NN_CS_CODE)value=code.address;
  else if(p->kind==NN_CS_DESCRIPTORS)value=owner->address+4096;
  else if(p->kind==NN_CS_SEQUENCE)value=owner->address+6144;
  else {C(p->kind==NN_CS_UBO&&p->index<16);value=ubos[p->index].address+p->offset;}
  expected[p->word]=(uint32_t)value;expected[p->word+1]=(uint32_t)(value>>32);marked[p->word]=marked[p->word+1]=1;
 }
 const uint32_t *w=(const uint32_t*)owner->data;
 for(unsigned i=0;i<normal->command_words;i++){C(w[i]==expected[i]);if(!marked[i])C(w[i]==normal->commands[i]);}
 for(unsigned i=0;i<2;i++){
  const uint32_t *d=w+1024+i*16;uint64_t a=(uint64_t)d[4]|(uint64_t)d[5]<<32;
  C(a==(ssbos[i].address&~UINT64_C(63)));C((d[1]&normal->width_mask)==ssbos[i].bytes/4);
  C(((d[2]&normal->offset_mask)>>normal->offset_shift)==(ssbos[i].address&63)/4);
 }
 C(!nn_bo_destroy(owner));direct_success++;cases++;
 /* An unreferenced pointer is passed as GPUVA: never read CPU group counts. */
 reset();uint64_t args=UINT64_C(0x998800040);
 C(!nn_cs_encode_indirect(&device,&code,normal,ubos,ssbos,args,&owner,&bytes));
 C(owner==only&&map_calls==2);size_t dispatch=SIZE_MAX;
 for(size_t pos=0;pos<normal->command_words;){unsigned count=nvn_pm4_packet_words(normal->commands[pos]);C(count&&count<=normal->command_words-pos);if(normal->commands[pos]==nvn_pm4_packet7(0x33,4))dispatch=pos;pos+=count;}
 C(dispatch!=SIZE_MAX);w=(const uint32_t*)owner->data;unsigned copies=0,indirects=0;
 for(size_t pos=0;pos<bytes/4;){unsigned count=nvn_pm4_packet_words(w[pos]);C(count&&count<=bytes/4-pos);
  if(w[pos]==nvn_pm4_packet7(0x73,5)){C(copies<3);uint64_t dst=(uint64_t)w[pos+2]|(uint64_t)w[pos+3]<<32,src=(uint64_t)w[pos+4]|(uint64_t)w[pos+5]<<32;C(src==args+copies*4&&dst==owner->address+7168+copies*4);copies++;}
  if(w[pos]==nvn_pm4_packet7(0x41,4)){C(!w[pos+1]&&(((uint64_t)w[pos+2]|(uint64_t)w[pos+3]<<32)==args));C(w[pos+4]==((7u<<2)|(7u<<12)));indirects++;}
  pos+=count;
 }
 C(copies==3&&indirects==1&&bytes==(normal->command_words+27)*4);
 C(((const uint32_t*)(owner->data+7168))[3]==3);
 C(!memcmp(w+dispatch+32,expected+dispatch+5,(normal->command_words-dispatch-5)*4));
 C(!nn_bo_destroy(owner));indirect_success++;cases++;
 /* Allocation failure creates no owner; NULL API arguments allocate nothing. */
 reset();alloc_fail=1;owner=(nn_bo*)(uintptr_t)1;bytes=99;C(nn_cs_encode(&device,&code,normal,ubos,ssbos,groups,&owner,&bytes)==ENOMEM);C(!owner&&!bytes&&!live);cases++;
 reset();alloc_fail=1;owner=(nn_bo*)(uintptr_t)1;bytes=99;C(nn_cs_encode_indirect(&device,&code,normal,ubos,ssbos,args,&owner,&bytes)==ENOMEM);C(!owner&&!bytes&&!live);cases++;
 reset();
 for(unsigned n=0;n<6;n++){
  owner=(nn_bo*)(uintptr_t)1;bytes=99;
  int e=nn_cs_encode(n==0?NULL:&device,n==1?NULL:&code,n==2?NULL:normal,n==3?NULL:ubos,n==4?NULL:ssbos,n==5?NULL:groups,&owner,&bytes);
  C(e==(n==2?ENOTSUP:EINVAL));C(!owner&&!bytes&&!live);cases++;
 }
 C(nn_cs_encode(&device,&code,normal,ubos,ssbos,groups,NULL,&bytes)==EINVAL);
 C(nn_cs_encode(&device,&code,normal,ubos,ssbos,groups,&owner,NULL)==EINVAL);
 struct nn_cs_image image=*normal;image.commands=NULL;bad_direct(&image,0,ENOTSUP);
 image=*normal;image.patches=NULL;bad_direct(&image,0,ENOTSUP);
 for(unsigned mode=0;mode<3;mode++){
  reset();map_fail=1;destroy_fail=mode?(mode==1?EIO:EBUSY):0;unsigned fault=destroy_fail;
  owner=(nn_bo*)(uintptr_t)1;bytes=99;int e=nn_cs_encode(&device,&code,normal,ubos,ssbos,groups,&owner,&bytes);
  finish_error(e,fault?(int)fault:ENOTSUP,owner,bytes,fault);
  ssbos[0].bytes=3;bad_direct(normal,mode,EINVAL);bad_indirect(normal,0,mode,EINVAL);ssbos[0].bytes=124;
  for(unsigned n=0;n<7;n++){
   struct nn_cs_patch patch={0,NN_CS_CODE,0,0};image=*normal;image.patches=&patch;image.patch_count=1;
   int error=EINVAL;
   switch(n){case 0:patch.word=(uint32_t)normal->command_words;break;case 1:patch.word=(uint32_t)normal->command_words-1;break;case 2:patch.kind=999;error=ENOTSUP;break;case 3:patch.kind=NN_CS_UBO;patch.index=16;break;case 4:patch.kind=NN_CS_UBO;patch.index=2;patch.offset=65;break;case 5:patch.kind=NN_CS_GROUP_COUNT;patch.index=3;break;case 6:patch.kind=NN_CS_GLOBAL_SIZE;patch.index=3;break;}
   bad_direct(&image,mode,error);
  }
  bad_indirect(normal,1,mode,ENOTSUP);bad_indirect(normal,2,mode,ENOTSUP);
  uint32_t padded[1024];memcpy(padded,normal->commands,normal->command_words*4);for(size_t k=normal->command_words;k<1024;k++)padded[k]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);
  image=*normal;image.commands=padded;image.command_words=1024;bad_indirect(&image,0,mode,E2BIG);
 }
 C(!live&&allocs==frees&&retained==retried);C(!memcmp(normal->code,kernel_copy,normal->code_bytes));
 printf("{\"passed\":true,\"scope\":\"HOST common encoder fault injection\",\"checks\":%u,\"cases\":%u,\"allocations\":%u,\"destroys\":%u,\"destroy_attempts\":%u,\"retained_error_owners\":%u,\"retried_error_owners\":%u,\"direct_success\":%u,\"indirect_success\":%u,\"live\":%u,\"gpu_execution\":false}\n",checks,cases,allocs,frees,destroy_calls,retained,retried,direct_success,indirect_success,live);return 0;
}
