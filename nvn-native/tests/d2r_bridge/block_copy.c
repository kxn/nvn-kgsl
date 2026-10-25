/* SPDX-License-Identifier: MIT. HOST address/packet/owner tests; no GPU emulation. */
#include "nvn_native/block_copy.h"
#include "nvn_native/pm4.h"
#include "../block_copy_cases.h"
#include NN_BLOCK_COPY_IMAGE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long checks,cases,allocations,destroys;
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);abort();}}while(0)
struct nn_device{uint32_t chip;};
struct nn_bo{uint64_t address;size_t bytes;void *data;};
uint32_t nn_device_chip(const nn_device *d){return d->chip;}
uint64_t nn_bo_address(const nn_bo *b){return b->address;}
size_t nn_bo_size(const nn_bo *b){return b->bytes;}
void *nn_bo_map(const nn_bo *b){return b->data;}
int nn_bo_alloc(nn_device *d,size_t bytes,nn_bo **out){(void)d;allocations++;*out=calloc(1,sizeof(**out));if(!*out)return ENOMEM;(*out)->data=calloc(1,bytes);if(!(*out)->data){free(*out);*out=NULL;return ENOMEM;}(*out)->address=0x50000000+allocations*0x10000;(*out)->bytes=bytes;return 0;}
int nn_bo_destroy(nn_bo *b){destroys++;free(b->data);free(b);return 0;}
static void zero(const void *p,size_t n){const unsigned char *b=p;for(size_t i=0;i<n;i++)CK(!b[i]);}
static void geometry(const struct nn_tex_source_layout *s,const struct nn_block_copy_plan *p)
{
 const struct nn_block_copy_region *r=&p->region;unsigned char *canonical=calloc(1,(size_t)s->payload_bytes),*linear=calloc(1,(size_t)p->linear_bytes);CK(canonical&&linear);
 for(unsigned layer=0;layer<r->layers;layer++)for(unsigned y=0;y<r->rows;y++)for(unsigned x=0;x<r->row_bytes;x++){
  uint64_t a=nn_block_copy_oracle_offset(s,r->level,r->base_layer+layer,r->x_bytes+x,r->y_rows+y),actual;
  CK(!nn_tex_source_offset(s,r->level,r->base_layer+layer,(r->x_bytes+x)/s->bytes_per_block,r->y_rows+y,(r->x_bytes+x)%s->bytes_per_block,&actual));CK(a==actual);CK(a<s->payload_bytes&&!canonical[a]);canonical[a]=1;
  uint64_t b=(uint64_t)layer*r->linear_image_stride+(uint64_t)y*r->linear_row_stride+x;CK(b<p->linear_bytes&&!linear[b]);linear[b]=1;
 }
 unsigned long n=0,active=(unsigned long)r->layers*r->rows*r->row_bytes;
 /* Every flat DWORD has one dispatch owner even when two rows/layers share it.
  * Exactly the intended byte lanes are active, padding/tail has zero mask. */
 for(uint64_t word=0;word<p->args.linear_words;word++){
  unsigned mask=0;for(unsigned lane=0;lane<4;lane++){
   uint64_t b=word*4+lane;unsigned valid=0;
   if(b<p->args.linear_span){unsigned layer=b/r->linear_image_stride,within=b%r->linear_image_stride,y=within/r->linear_row_stride,x=within%r->linear_row_stride;valid=layer<r->layers&&y<r->rows&&x<r->row_bytes;}
   CK(valid==(unsigned)linear[b]);if(valid){mask|=255u<<(lane*8);n++;}
  }
  CK((mask&0xffu)==(linear[word*4]?255u:0u));
 }
 CK(n==active);
 /* Each retile (xDWORD,row,layer) maps injectively to an aligned physical
  * destination DWORD. Masked left/right bytes retain their old contents. */
 unsigned char *seen=calloc(1,(size_t)s->payload_bytes/4);CK(seen);n=0;
 for(unsigned layer=0;layer<r->layers;layer++)for(unsigned y=0;y<r->rows;y++)for(unsigned x=r->x_bytes&~3u;x<r->x_bytes+r->row_bytes;x+=4){
  uint64_t a=nn_block_copy_oracle_offset(s,r->level,r->base_layer+layer,x,r->y_rows+y);CK(!(a%4)&&a+4<=s->payload_bytes&&!seen[a/4]);seen[a/4]=1;
  for(unsigned lane=0;lane<4;lane++){unsigned valid=x+lane>=r->x_bytes&&x+lane<r->x_bytes+r->row_bytes;CK(canonical[a+lane]==valid);n+=valid;}
 }
 CK(n==active);free(seen);free(canonical);free(linear);
}
static void bind_encode(struct nn_tex_source_layout *s,struct nn_block_copy_plan *p)
{
 struct nn_device d={0x06050002};struct nn_bo c={0x1000000,(size_t)s->payload_bytes+4096,NULL},l={0x2000000,(size_t)p->linear_bytes+4096,NULL},u={0x3000000,4096,NULL},code={0x4000000,NVN_COMPILED_IMAGE.code_bytes,NULL};
 struct nn_block_copy_range cr={&c,0,c.bytes,512},lr={&l,0,l.bytes,68},ur={&u,0,u.bytes,0};struct nn_block_copy_binding b;
 unsigned a=allocations,z=destroys;CK(!nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,p,&cr,&lr,&ur,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));CK(allocations==a&&destroys==z);CK(!memcmp(&b.args,&p->args,64)&&!memcmp(b.groups,p->groups,12));CK(b.resources[0]==&c&&b.resources[1]==&l&&b.resources[2]==&u);CK(b.ubos[2].address==u.address&&b.ubos[2].bytes==64);
 CK(b.ssbos[p->args.direction?1:0].address==c.address+512);CK(b.ssbos[p->args.direction?0:1].address==l.address+68);
 nn_bo *job=NULL;size_t bytes=0;CK(!nn_cs_encode(&d,&code,&NVN_COMPILED_IMAGE,b.ubos,b.ssbos,b.groups,&job,&bytes));CK(bytes==NVN_COMPILED_IMAGE.command_words*4&&bytes<=4096&&job&&job->bytes==8192);
 uint32_t *words=job->data;for(size_t pos=0;pos<bytes/4;){unsigned n=nvn_pm4_packet_words(words[pos]);CK(n&&n<=bytes/4-pos);pos+=n;}
 for(size_t i=0;i<NVN_COMPILED_IMAGE.patch_count;i++){
  const struct nn_cs_patch *patch=NVN_COMPILED_IMAGE.patches+i;uint64_t value=words[patch->word];
  if(patch->kind!=NN_CS_GROUP_COUNT&&patch->kind!=NN_CS_GLOBAL_SIZE)value|=(uint64_t)words[patch->word+1]<<32;
  switch(patch->kind){case NN_CS_CODE:CK(value==code.address);break;case NN_CS_DESCRIPTORS:CK(value==job->address+4096);break;case NN_CS_SEQUENCE:CK(value==job->address+6144);break;case NN_CS_UBO:CK(patch->index==2&&value==u.address+patch->offset);break;case NN_CS_GROUP_COUNT:CK(value==p->groups[patch->index]);break;case NN_CS_GLOBAL_SIZE:CK(value==p->groups[patch->index]*NVN_COMPILED_IMAGE.local[patch->index]);break;default:CK(0);}
 }
 CK(!nn_bo_destroy(job));
 #define BAD(expr) do{memset(&b,0xa5,sizeof(b));CK((expr)!=0);zero(&b,sizeof(b));CK(allocations==a+1&&destroys==z+1);}while(0)
 BAD(nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,p,&cr,&lr,&ur,NN_BLOCK_COPY_ALIAS_UNKNOWN,&b));
 struct nn_block_copy_plan damaged=*p;damaged.args.canonical_base+=4;BAD(nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,&damaged,&cr,&lr,&ur,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));damaged=*p;damaged.groups[0]++;BAD(nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,&damaged,&cr,&lr,&ur,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));
 struct nn_block_copy_range bad=lr;bad.owner_offset=68;BAD(nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,p,&cr,&bad,&ur,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));bad=lr;bad.owner_bytes=lr.offset+p->linear_bytes-1;BAD(nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,p,&cr,&bad,&ur,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));bad=lr;bad.bo=&c;bad.offset=cr.offset;BAD(nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,p,&cr,&bad,&ur,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));bad=ur;bad.owner_bytes=63;BAD(nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,p,&cr,&lr,&bad,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));
 /* Canonical and staging may share the ORIGINAL pool BO. A valid full
  * access-span separation must survive normal descriptor/patch generation. */
 struct nn_block_copy_range shared=lr;shared.bo=&c;shared.offset=cr.offset+p->canonical_bytes+68;c.bytes=shared.offset+p->linear_bytes+64;shared.owner_bytes=cr.owner_bytes=c.bytes;
 CK(!nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,p,&cr,&shared,&ur,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));CK(b.resources[0]==b.resources[1]);CK(b.ssbos[p->args.direction?0:1].address==c.address+shared.offset);
 CK(!nn_cs_encode(&d,&code,&NVN_COMPILED_IMAGE,b.ubos,b.ssbos,b.groups,&job,&bytes));CK(!nn_bo_destroy(job));a++;z++;
 shared.offset=cr.offset+p->canonical_bytes-4;BAD(nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,p,&cr,&shared,&ur,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));
 c.address=UINT64_MAX-255;BAD(nn_block_copy_bind(&d,&NVN_COMPILED_IMAGE,s,p,&cr,&lr,&ur,NN_BLOCK_COPY_DISJOINT_PROVEN,&b));
 #undef BAD
}
int main(void)
{
 const unsigned heights[]={8,11,17,128};
 for(unsigned f=0;f<NN_BLOCK_COPY_FORMAT_COUNT;f++)for(unsigned h=0;h<4;h++)for(unsigned level=0;level<3;level++)for(unsigned partial=0;partial<2;partial++)for(unsigned dir=0;dir<2;dir++){
  struct nn_tex_source_layout s;struct nn_block_copy_region r;struct nn_block_copy_plan p;CK(!nn_block_copy_case(nn_block_copy_formats[f],heights[h],level,partial,dir,&s,&r));CK(!nn_block_copy_plan(&s,&r,&p));CK(p.canonical_bytes==s.payload_bytes&&p.linear_bytes%4==0);geometry(&s,&p);bind_encode(&s,&p);cases++;
  struct nn_block_copy_region bad=r;bad.linear_row_stride=bad.row_bytes-1;CK(nn_block_copy_plan(&s,&bad,&p));zero(&p,sizeof(p));bad=r;bad.linear_image_stride=(bad.rows-1)*bad.linear_row_stride+bad.row_bytes-1;CK(nn_block_copy_plan(&s,&bad,&p));bad=r;bad.base_layer=s.layers;CK(nn_block_copy_plan(&s,&bad,&p));bad=r;bad.level=s.levels;CK(nn_block_copy_plan(&s,&bad,&p));bad=r;bad.x_bytes=s.level[level].row_bytes;CK(nn_block_copy_plan(&s,&bad,&p));bad=r;bad.direction=2;CK(nn_block_copy_plan(&s,&bad,&p));bad=r;bad.layers=0;CK(nn_block_copy_plan(&s,&bad,&p));
 }
 /* Capacity/overflow edges without allocating huge pixel buffers. This
  * verifies flattened dispatch beyond a single 65535-group X dimension. */
 struct nn_tex_source_desc large={.target=4,.format=1,.width=13,.height=8,.layers=3,.levels=1,.block_height_log2=-1};struct nn_tex_source_layout ls;CK(!nn_tex_source_layout(&large,&ls));
 struct nn_block_copy_region lr={.layers=3,.row_bytes=13,.rows=8,.linear_row_stride=1000000,.linear_image_stride=7000013};struct nn_block_copy_plan lp;
 CK(!nn_block_copy_plan(&ls,&lr,&lp));CK(lp.groups[0]==65535&&lp.groups[1]==2&&lp.args.dispatch_width==65535*8);
 lr.layers=2;lr.rows=1;lr.linear_row_stride=13;lr.linear_image_stride=UINT32_MAX-16;CK(!nn_block_copy_plan(&ls,&lr,&lp));CK(lp.linear_bytes==UINT32_MAX-3u&&lp.args.linear_words==(UINT32_MAX-3u)/4&&lp.groups[1]==257);
 lr.linear_image_stride++;CK(nn_block_copy_plan(&ls,&lr,&lp)==EOVERFLOW);zero(&lp,sizeof(lp));
 lr.linear_image_stride=13;lr.x_bytes=1;lr.row_bytes=12;CK(!nn_block_copy_plan(&ls,&lr,&lp));ls.format=5;CK(nn_block_copy_plan(&ls,&lr,&lp)==EINVAL);zero(&lp,sizeof(lp));
 unsigned pattern_changes=0;for(unsigned i=0;i<1024;i++)pattern_changes+=nn_block_copy_pattern(i,17,0,0)!=nn_block_copy_pattern(i,17,1,0);CK(pattern_changes>900);
 CK(allocations==destroys);printf("{\"passed\":true,\"checks\":%lu,\"cases\":%lu,\"allocations\":%lu,\"destroys\":%lu,\"gpu_execution\":false}\n",checks,cases,allocations,destroys);return 0;
}
