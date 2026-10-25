/* SPDX-License-Identifier: MIT. HOST metadata/owner boundaries, no GPU engine. */
#include "nvn_native/texture_buffer.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include NN_TEX_BUFFER_RECIPES
#include NN_TEX_BUFFER_EXPECTED
struct nn_bo {uint64_t address;size_t bytes;unsigned pending;};
static unsigned checks,reads;
uint64_t nn_bo_address(const nn_bo *b){reads++;return b->address;}
size_t nn_bo_size(const nn_bo *b){reads++;return b->bytes;}
void *nn_bo_map(const nn_bo *b){(void)b;abort();}
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"fail:%d:%s\n",__LINE__,#x);return 1;}}while(0)
static int zero(const void *p,size_t n){const unsigned char *s=p;while(n--)if(*s++)return 0;return 1;}
static int rejected(const struct nn_tex_buffer_recipe *r,nn_bo *bo,size_t off,size_t n,size_t start,size_t bytes,int expected)
{struct nn_tex_buffer_binding b;memset(&b,0x7c,sizeof(b));int e=nn_tex_buffer_bind(r,bo,off,n,start,bytes,&b);return e==expected&&zero(&b,sizeof(b));}
int main(void)
{
 C(NN_TEXTURE_BUFFER_RECIPE_COUNT==2);C(!nn_tex_buffer_recipe_validate(NN_TEXTURE_BUFFER_RECIPES));
 for(size_t i=0;i<sizeof(expected)/sizeof(*expected);i++){
  const struct expected_view *v=expected+i;const struct nn_tex_buffer_recipe *r=NN_TEXTURE_BUFFER_RECIPES+v->recipe;
  size_t offset=(size_t)(v->address&63);nn_bo bo={v->address-offset,offset+v->bytes,0};struct nn_tex_buffer_binding binding;
  C(!nn_tex_buffer_bind(r,&bo,0,bo.bytes,offset,v->bytes,&binding));
  C(!memcmp(binding.descriptor,v->words,64));C(binding.resource==&bo&&binding.address==v->address);
  C(binding.bytes==v->bytes&&binding.offset==offset&&binding.owner_bytes==bo.bytes);
  C(binding.elements==v->bytes/4&&binding.format==r->format&&binding.element_bytes==4);
  C(binding.descriptor_base==bo.address&&bo.address==v->address-offset&&bo.bytes==offset+v->bytes&&!bo.pending);
  /* Value copy retains the exact resource identity/range; it does not create a
   * second backing, reference count or a promise of physical disjointness. */
  struct nn_tex_buffer_binding copy;memcpy(&copy,&binding,sizeof(copy));C(!memcmp(&copy,&binding,sizeof(copy)));
  /* Pending/unknown GPU use does not cause hidden destroy/wait/cache here. The
   * caller retains this owner and submits it as an actual resource reference. */
  bo.pending=2;C(!nn_tex_buffer_bind(r,&bo,0,bo.bytes,offset,v->bytes,&binding)&&bo.pending==2);
 }
 const struct nn_tex_buffer_recipe *r=NN_TEXTURE_BUFFER_RECIPES;struct nn_tex_buffer_recipe bad=*r;nn_bo bo={0x100000,4096,0};
 C(nn_tex_buffer_recipe_validate(NULL)==EINVAL);C(nn_tex_buffer_bind(r,&bo,0,4096,0,4,NULL)==EINVAL);
 for(unsigned field=0;field<10;field++){
  bad=*r;switch(field){case 0:bad.schema++;break;case 1:bad.chip++;break;case 2:bad.format=36;break;case 3:bad.element_bytes=8;break;case 4:bad.width_mask++;break;case 5:bad.height_shift=32;break;case 6:bad.offset_shift=32;break;case 7:bad.address_mask=UINT64_MAX;break;case 8:bad.descriptor[4]=64;break;case 9:bad.descriptor[2]=0;break;}
  struct nn_tex_buffer_binding out;memset(&out,1,sizeof(out));C(nn_tex_buffer_bind(&bad,&bo,0,4096,0,4,&out)!=0&&zero(&out,sizeof(out)));
 }
 C(rejected(r,NULL,0,4096,0,4,EINVAL));C(rejected(r,&bo,4097,4,4097,4,EINVAL));
 C(rejected(r,&bo,0,4097,0,4,EINVAL));C(rejected(r,&bo,64,64,60,4,EINVAL));
 C(rejected(r,&bo,0,64,64,4,EINVAL));C(rejected(r,&bo,0,64,0,0,EINVAL));
 C(rejected(r,&bo,0,64,0,3,EINVAL));C(rejected(r,&bo,0,64,1,4,EINVAL));
 C(rejected(r,&bo,4,64,4,4,EINVAL)); /* aligned-down prefix outside owner */
 struct nn_tex_buffer_binding out;C(!nn_tex_buffer_bind(r,&bo,64,64,68,4,&out)&&out.descriptor_base==bo.address+64&&out.offset==68);
 bo=(nn_bo){0,4096,0};C(rejected(r,&bo,0,4096,0,4,EOVERFLOW));
 bo=(nn_bo){UINT64_MAX-15,4096,0};C(rejected(r,&bo,0,4096,0,4,EOVERFLOW));
 bo=(nn_bo){UINT64_C(1)<<49,4096,0};C(rejected(r,&bo,0,4096,0,4,EOVERFLOW));
 bo=(nn_bo){(UINT64_C(1)<<49)-64,4096,0};C(rejected(r,&bo,0,4096,60,8,EOVERFLOW));
 bo=(nn_bo){0x100000,(size_t)UINT32_MAX+1,0};C(rejected(r,&bo,0,bo.bytes,0,bo.bytes,EINVAL));
 /* Aliases are not relabelled independent backing: both snapshots retain their
  * own borrowed owner identity and canonical address, without any GPU copy. */
 nn_bo alias=bo;C(!nn_tex_buffer_bind(r,&bo,0,4096,4,4,&out)&&out.resource==&bo);
 struct nn_tex_buffer_binding second;C(!nn_tex_buffer_bind(r,&alias,0,4096,4,4,&second)&&second.resource==&alias&&second.address==out.address);
 C(reads>0);printf("HOST typed buffer checks=%u; no GPU/image mapping/public NVN API\n",checks);return 0;
}
