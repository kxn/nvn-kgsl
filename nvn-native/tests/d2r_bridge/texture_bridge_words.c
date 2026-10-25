/* HOST real encoders and owner failures; never executes shader/PM4. */
#include NN_BRIDGE_MEMORY_BACKEND
#include "nvn_native/texture_bridge.h"
#include "nvn_native/pm4.h"
#include NN_BRIDGE_BUNDLE
static size_t data_offset(nn_device *d,size_t bytes)
{size_t page=nn_device_page(d);return (bytes+page-1)&~(page-1);}
static unsigned checks,allocations;static int fail_at,free_fail;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);abort();}}while(0)
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
static unsigned diagnostic_mask=255;
unsigned nn_tex_bridge_probe_mask(void){return diagnostic_mask;}
static nn_bo *tracked_wrapper;static size_t tracked_offset;static unsigned tracked_inputs,tracked_writeback,tracked_calls;
void nn_tex_bridge_probe_track(nn_bo *bo,size_t offset,unsigned inputs,unsigned writeback)
{tracked_wrapper=bo;tracked_offset=offset;tracked_inputs=inputs;tracked_writeback=writeback;tracked_calls++;}
#endif
int __real_nn_bo_alloc(nn_device *,size_t,nn_bo **);
int __wrap_nn_bo_alloc(nn_device *d,size_t bytes,nn_bo **out)
{ allocations++;if(fail_at && allocations==(unsigned)fail_at){*out=NULL;return ENOMEM;}return __real_nn_bo_alloc(d,bytes,out); }
int __real_nn_bo_destroy(nn_bo *);
int __wrap_nn_bo_destroy(nn_bo *b)
{ if(free_fail){free_fail=0;return EIO;}return __real_nn_bo_destroy(b); }
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
static void mask_boundaries(nn_device *d,struct nn_tex_shadow *shadow,nn_bo *child)
{
 const struct nn_tex_shadow *in[]={shadow,shadow};const unsigned masks[]={0,1,2,4,8,16,32,64,128,255};
 nn_bo *gold=NULL;size_t bytes=0;diagnostic_mask=255;
 T(!nn_tex_shadow_chain(d,in,2,child,16,shadow,&gold,&bytes));
 size_t positions[8]={0},found=0;const uint32_t phases[]={0x10,0x20,0x21,0x30,0x40,0x50,0x60,0x70};
 const uint32_t *base=nn_bo_map(gold);uint64_t progress_address=nn_bo_address(gold)+data_offset(d,bytes)+4;
 for(size_t at=0;at<bytes/4;){size_t n=nvn_pm4_packet_words(base[at]);T(n&&n<=bytes/4-at);
  if(base[at]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,3)&&
      ((uint64_t)base[at+1]|(uint64_t)base[at+2]<<32)==progress_address){
   T(found<8&&base[at+3]==phases[found]);positions[found++]=at;
  }at+=n;
 }T(found==8);
 unsigned stable_bos=d->bos;
 unsigned char *pre=malloc(shadow->pre_bytes),*post=malloc(shadow->post_bytes);T(pre&&post);
 memcpy(pre,nn_bo_map(shadow->bos[NN_TEX_SHADOW_PRE]),shadow->pre_bytes);
 memcpy(post,nn_bo_map(shadow->bos[NN_TEX_SHADOW_POST]),shadow->post_bytes);
 for(unsigned m=0;m<sizeof(masks)/sizeof(masks[0]);m++){
  diagnostic_mask=masks[m];nn_bo *test=NULL;size_t actual=0;unsigned before_alloc=allocations;
  T(!nn_tex_shadow_chain(d,in,2,child,16,shadow,&test,&actual));
  T(actual==bytes&&nn_bo_size(test)==nn_bo_size(gold)&&allocations==before_alloc+1&&d->bos==stable_bos+1);
  T(tracked_wrapper==test&&tracked_offset==data_offset(d,bytes)+4&&tracked_inputs==2&&tracked_writeback==1);
  const uint32_t *words=nn_bo_map(test);size_t phase=0;
  for(size_t at=0;at<bytes/4;){size_t n=nvn_pm4_packet_words(base[at]);T(n&&n<=bytes/4-at);
   if(phase<8&&at==positions[phase]){
    T(n==4&&nvn_pm4_packet_words(words[at])==4);
    if(masks[m]&(1u<<phase)){
     T(words[at]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,3)&&words[at+3]==phases[phase]);
     T(((uint64_t)words[at+1]|(uint64_t)words[at+2]<<32)==nn_bo_address(test)+data_offset(d,bytes)+4);
    }else T(words[at]==nvn_pm4_packet7(0x10,3)&&!words[at+1]&&!words[at+2]&&!words[at+3]);
    phase++;
   }else if(base[at]==nvn_pm4_packet7(0x46,4)&&
      ((uint64_t)base[at+2]|(uint64_t)base[at+3]<<32)==nn_bo_address(gold)+data_offset(d,bytes)){
    T(words[at]==base[at]&&words[at+1]==base[at+1]&&words[at+4]==base[at+4]);
    T(((uint64_t)words[at+2]|(uint64_t)words[at+3]<<32)==nn_bo_address(test)+data_offset(d,bytes));
   }else T(!memcmp(base+at,words+at,n*4));
   at+=n;
  }
  T(phase==8&&!memcmp(pre,nn_bo_map(shadow->bos[NN_TEX_SHADOW_PRE]),shadow->pre_bytes));
  T(!memcmp(post,nn_bo_map(shadow->bos[NN_TEX_SHADOW_POST]),shadow->post_bytes));
  T(!nn_bo_destroy(test)&&d->bos==stable_bos);
 }
 free(pre);free(post);T(!nn_bo_destroy(gold));diagnostic_mask=255;
}
#endif
static void page_contract(nn_device *d,struct nn_tex_shadow *shadow)
{
 size_t page=nn_device_page(d);T(page>=4&&!(page&(page-1)));
 nn_bo *child=NULL;T(!nn_bo_alloc(d,page*12,&child));
 uint32_t *commands=nn_bo_map(child);
 for(size_t i=0;i<nn_bo_size(child)/4;i++)commands[i]=nvn_pm4_packet7(0x10,0);
 const struct nn_tex_shadow *in[]={shadow,shadow};unsigned stable=d->bos;
 for(unsigned count=0;count<=2;count++)for(unsigned edge=0;edge<3;edge++){
  const struct nn_tex_shadow *out=count?shadow:NULL;
  size_t overhead=42+count*(shadow->pre_bytes/4)+(out?27+shadow->post_bytes/4:0);
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
  overhead+=(4+count+(out?2:0))*4;
#endif
  size_t total_words=page/4*4+edge-1;T(total_words>overhead);
  nn_bo *owner=NULL;size_t bytes=0;
  T(!nn_tex_shadow_chain(d,in,count,child,(total_words-overhead)*4,out,&owner,&bytes));
  size_t offset=data_offset(d,bytes),data_bytes=4;
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
  data_bytes=8;T(tracked_wrapper==owner&&tracked_offset==offset+4);
#endif
  T(bytes==total_words*4&&offset>=bytes&&offset%page==0);
  T(offset+data_bytes<=nn_bo_size(owner)&&nn_bo_address(owner)%page==0);
  T((nn_bo_address(owner)+bytes-1)/page<(nn_bo_address(owner)+offset)/page);
  /* Parse whole packets and identify only writes to this wrapper's BO.
   * Embedded kernel resources retain their original distinct owners. */
  const uint32_t *w=nn_bo_map(owner);size_t sequence_stores=0,progress_stores=0;
  for(size_t at=0;at<bytes/4;){size_t n=nvn_pm4_packet_words(w[at]);T(n&&n<=bytes/4-at);
   if(w[at]==nvn_pm4_packet7(0x46,4)){
    uint64_t a=(uint64_t)w[at+2]|(uint64_t)w[at+3]<<32;
    if(a>=nn_bo_address(owner)&&a-nn_bo_address(owner)<nn_bo_size(owner)){
     T(a==nn_bo_address(owner)+offset&&a-nn_bo_address(owner)>=bytes);sequence_stores++;
    }
   }
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
   if(w[at]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,3)){
    uint64_t a=(uint64_t)w[at+1]|(uint64_t)w[at+2]<<32;
    if(a>=nn_bo_address(owner)&&a-nn_bo_address(owner)<nn_bo_size(owner)){
     T(a==nn_bo_address(owner)+offset+4&&a-nn_bo_address(owner)>=bytes);progress_stores++;
    }
   }
#endif
   at+=n;
  }
  T(sequence_stores==(out?6u:3u));
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
  T(progress_stores==4+count+(out?2u:0u));
#else
  T(!progress_stores);
#endif
  const unsigned char *raw=nn_bo_map(owner);
  for(size_t i=bytes;i<offset+data_bytes;i++)T(!raw[i]);
  T(!nn_bo_destroy(owner)&&d->bos==stable);
 }
 T(!nn_bo_destroy(child));
}

static void many_shadow_inputs(nn_device *d,struct nn_tex_shadow *shadow,nn_bo *child)
{
 const struct nn_tex_shadow *inputs[NN_TEX_SHADOW_CHAIN_MAX_INPUTS];
 for(unsigned i=0;i<NN_TEX_SHADOW_CHAIN_MAX_INPUTS;i++)inputs[i]=shadow;
 unsigned baseline=d->bos;nn_bo *owner=NULL;size_t bytes=0;
 T(!nn_tex_shadow_chain(d,inputs,NN_TEX_SHADOW_CHAIN_MAX_INPUTS,child,16,shadow,&owner,&bytes));
 T(owner&&d->bos==baseline+1);T(bytes>17*shadow->pre_bytes+shadow->post_bytes);
 /* Flattened commands borrow source GPUVAs; destroying wrapper cannot destroy
  * any repeated input owner, and no third IB nesting level is introduced. */
 const uint32_t *words=nn_bo_map(owner);size_t packets=0,indirect=0;
 for(size_t at=0;at<bytes/4;){size_t n=nvn_pm4_packet_words(words[at]);T(n&&n<=bytes/4-at);
  if((words[at]>>28)==7&&((words[at]>>16)&127)==0x3f)indirect++;
  packets++;at+=n;
 }
 T(packets&&!indirect);T(!nn_bo_destroy(owner)&&d->bos==baseline);
 T(shadow->bos[0]&&shadow->bos[3]&&shadow->bos[4]);
 unsigned allocations_before=allocations;
 T(nn_tex_shadow_chain(d,inputs,18,child,16,shadow,&owner,&bytes)==EINVAL&&!owner&&!bytes);
 T(allocations==allocations_before&&d->bos==baseline);
 size_t saved=shadow->pre_bytes;size_t actual=nn_bo_size(shadow->bos[3]);
 shadow->pre_bytes=actual+4;
 T(nn_tex_shadow_chain(d,inputs,17,child,16,shadow,&owner,&bytes)==EINVAL&&!owner&&!bytes);
 shadow->pre_bytes=saved;T(allocations==allocations_before&&d->bos==baseline);
 /* Per-input huge ranges cannot overflow aggregate length or reach allocate. */
 size_t old_bytes=shadow->bos[3]->bytes;shadow->bos[3]->bytes=(size_t)0x100000*4;
 shadow->pre_bytes=shadow->bos[3]->bytes;
 T(nn_tex_shadow_chain(d,inputs,17,child,16,shadow,&owner,&bytes)==E2BIG&&!owner&&!bytes);
 shadow->bos[3]->bytes=old_bytes;shadow->pre_bytes=saved;
 T(allocations==allocations_before&&d->bos==baseline);
}
int main(void)
{
 nn_device *d=NULL;nn_bo *source=NULL,*native=NULL,*child=NULL;T(!nn_device_open(&d));T(!nn_bo_alloc(d,32768,&source));T(!nn_bo_alloc(d,32768,&native));T(!nn_bo_alloc(d,4096,&child));
 memset(nn_bo_map(source),0x71,nn_bo_size(source));memset(nn_bo_map(native),0x83,nn_bo_size(native));
 memset(nn_bo_map(child),0,16);*(uint32_t *)nn_bo_map(child)=nvn_pm4_packet7(0x10,3); /* complete CP_NOP fixture */
 const struct nn_tex_bridge_bundle *bundle=&NN_TEXTURE_BRIDGE_BUNDLE;T(!nn_tex_bridge_bundle_validate(bundle));
 struct nn_tex_source_desc desc={.target=1,.format=37,.width=64,.height=64,.levels=1,.layers=1,.block_height_log2=-1};
 struct nn_tex_source_layout layout;T(!nn_tex_source_layout(&desc,&layout));T(!nn_tex_bridge_source_validate(&layout));
 unsigned baseline=d->bos;
 for(int i=1;i<=5;i++){
  struct nn_tex_shadow s;fail_at=i;allocations=0;T(nn_tex_shadow_create(d,bundle,source,512,layout.payload_bytes,&layout,&bundle->clear_layout,&s)==ENOMEM);fail_at=0;
  for(unsigned k=0;k<5;k++)T(!s.bos[k]);
  T(d->bos==baseline);
 }
 struct nn_tex_shadow s;T(!nn_tex_shadow_create(d,bundle,source,512,layout.payload_bytes,&layout,&bundle->clear_layout,&s));T(d->bos==baseline+5);
 for(unsigned dir=0;dir<2;dir++){
  const struct nn_tex_gob_copy_args *u=(const void*)((char*)nn_bo_map(s.bos[2])+dir*64);T(u->direction==dir&&u->row_bytes==256&&u->rows==64&&u->linear_pitch==256&&u->block_height_log2==3);
  for(unsigned k=32;k<64;k++)T(!((unsigned char*)u)[k]);
 }
 void *source_map=source->map;source->map=(void*)(UINTPTR_MAX-255u);struct nn_tex_shadow overflow;
 T(nn_tex_shadow_create(d,bundle,source,512,layout.payload_bytes,&layout,&bundle->clear_layout,&overflow)==EINVAL);source->map=source_map;
 const struct nn_tex_shadow *inputs[]={&s};nn_bo *wrapper=NULL;size_t bytes=0;T(!nn_tex_shadow_chain(d,inputs,1,child,16,&s,&wrapper,&bytes));T(wrapper&&bytes>s.pre_bytes+s.post_bytes&&bytes<=nn_bo_size(wrapper));
#ifdef NN_TEX_BRIDGE_QUAL_DIAGNOSTIC
 T(tracked_calls==1&&tracked_wrapper==wrapper&&tracked_inputs==1&&tracked_writeback==1);
 T(tracked_offset==data_offset(d,bytes)+4&&tracked_offset+4<=nn_bo_size(wrapper));
 T(!*(const uint32_t *)((const char *)nn_bo_map(wrapper)+tracked_offset));
 /* Parse whole packets; all diagnostic stores target this owner's scratch,
  * beyond executable commands. No arbitrary payload/opcode scan is used. */
 const uint32_t expected[]={0x10,0x20,0x30,0x40,0x50,0x60,0x70};size_t seen=0;
 const uint32_t *packet=nn_bo_map(wrapper);
 for(size_t at=0;at<bytes/4;){
  uint32_t count=nvn_pm4_packet_words(packet[at]);T(count&&count<=bytes/4-at);
  if(packet[at]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,3)&&
     ((uint64_t)packet[at+1]|(uint64_t)packet[at+2]<<32)==nn_bo_address(wrapper)+tracked_offset){
   T(count==4&&seen<7&&packet[at+3]==expected[seen]);seen++;
  }
  at+=count;
 }
 T(seen==7);
 mask_boundaries(d,&s,child);
#endif
 page_contract(d,&s);
 size_t saved_pre=s.pre_bytes,saved_post=s.post_bytes;s.pre_bytes=1;nn_bo *invalid=NULL;size_t zero=999;
 T(nn_tex_shadow_chain(d,inputs,1,child,16,&s,&invalid,&zero)==EINVAL&&invalid==NULL&&!zero);s.pre_bytes=nn_bo_size(s.bos[3])+4;
 T(nn_tex_shadow_chain(d,inputs,1,child,16,&s,&invalid,&zero)==EINVAL&&!invalid);s.pre_bytes=saved_pre;
 s.post_bytes=2;T(nn_tex_shadow_chain(d,inputs,1,child,16,&s,&invalid,&zero)==EINVAL&&!invalid);s.post_bytes=nn_bo_size(s.bos[4])+4;
 T(nn_tex_shadow_chain(d,inputs,1,child,16,&s,&invalid,&zero)==EINVAL&&!invalid);s.post_bytes=saved_post;
 nn_bo *badout=child;T(nn_tex_shadow_chain(d,inputs,NN_TEX_SHADOW_CHAIN_MAX_INPUTS+1,child,16,&s,&badout,&bytes)==EINVAL);T(!badout&&!bytes);
 /* Simulate pending ownership only, without a GPU execution claim. */
 for(unsigned k=0;k<5;k++)s.bos[k]->busy++;
 many_shadow_inputs(d,&s,child);
 T(nn_tex_shadow_destroy(&s)==EBUSY);T(d->bos==baseline+6);
 for(unsigned k=0;k<5;k++)s.bos[k]->busy--;
 T(!nn_tex_shadow_destroy(&s));T(!nn_bo_destroy(wrapper));
 for(unsigned off=0;off<nn_bo_size(source);off++)T(((unsigned char*)nn_bo_map(source))[off]==0x71);
 for(int i=1;i<=4;i++){
  struct nn_tex_conversion c;fail_at=i;allocations=0;T(nn_tex_conversion_create(d,bundle,source,512,layout.payload_bytes,&layout,&bundle->clear_layout,child,0,4096,0,&c)==EINVAL);fail_at=0;T(d->bos==baseline);
 }
 /* Native view can be unmapped dma-buf on Android; no access is made to its CPU
  * pixels. The HOST BO remains mapped so readonly assertions can prove it. */
 baseline=d->bos;
 for(int i=1;i<=4;i++){
  struct nn_tex_conversion c;fail_at=i;allocations=0;T(nn_tex_conversion_create(d,bundle,source,512,layout.payload_bytes,&layout,&bundle->clear_layout,native,1024,16384,0,&c)==ENOMEM);fail_at=0;T(d->bos==baseline);
 }
 for(unsigned direction=0;direction<2;direction++){
  struct nn_tex_conversion c;T(!nn_tex_conversion_create(d,bundle,source,512,layout.payload_bytes,&layout,&bundle->clear_layout,native,1024,16384,direction,&c));
  const struct nn_tex_gob_copy_args *u=nn_bo_map(c.bos[1]);T(u->direction==direction&&u->linear_pitch==256);T(c.entry_bytes&&c.bos[3]);
  c.bos[0]->busy++;T(nn_tex_conversion_destroy(&c)==EBUSY);T(c.bos[0]&&c.bos[3]);c.bos[0]->busy--;T(!nn_tex_conversion_destroy(&c));T(d->bos==baseline);
 }
 fail_at=2;allocations=0;free_fail=1;struct nn_tex_conversion retained;T(nn_tex_conversion_create(d,bundle,source,512,layout.payload_bytes,&layout,&bundle->clear_layout,native,1024,16384,0,&retained)==EIO);fail_at=0;T(retained.bos[0]);T(d->bos==baseline+1);T(!nn_tex_conversion_destroy(&retained));T(d->bos==baseline);
 struct nn_tex_source_layout wrong=layout;wrong.format=38;T(nn_tex_bridge_source_validate(&wrong));
 struct nn_tex_native_layout bad=bundle->clear_layout;bad.pitch=128;T(nn_tex_shadow_create(d,bundle,source,512,layout.payload_bytes,&layout,&bad,&s)==ENOTSUP);
 T(!nn_bo_destroy(native));T(!nn_bo_destroy(child));T(!nn_bo_destroy(source));T(!nn_device_close(d));
 printf("{\"passed\":true,\"checks\":%u,\"scope\":\"HOST real conversion encoder/packet/owner only; no GPU execution\"}\n",checks);return 0;
}
