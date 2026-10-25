/* SPDX-License-Identifier: MIT. Common pipeline platform patch contract and
 * nn_order_words are reused; no independent render/device restore recipe. */
#include "nvn_native/depth_transfer.h"
#include "nvn_native/order.h"
#include "nvn_native/pm4.h"
#include <errno.h>
#include <string.h>
enum {COMMAND_WORDS=8192,ZERO=32768,SEQUENCE=40960,JOB_BYTES=40964};
static int overlap(nn_bo*a,nn_bo*b)
{
 uint64_t x=nn_bo_address(a),y=nn_bo_address(b);size_t nx=nn_bo_size(a),ny=nn_bo_size(b);
 if(!x||!y||!nx||!ny||nx>UINT64_MAX-x||ny>UINT64_MAX-y)return 1;
 return x<y+ny&&y<x+nx;
}
static int segment_validate(const struct nn_pipeline_platform_segment*s)
{
 /* Same actual platform schema/boundary validation as pipeline.c. */
 if(!s||!s->words||!s->word_count||s->word_count>2048||s->patch_count>32||(s->patch_count&&!s->patches))return EINVAL;
 for(size_t at=0;at<s->word_count;){size_t n=nvn_pm4_packet_words(s->words[at]);if(!n||n>s->word_count-at)return EINVAL;at+=n;}
 for(size_t i=0;i<s->patch_count;i++){
  const struct nn_pipeline_platform_patch*p=s->patches+i;
  if(p->word>=s->word_count-1||p->offset||p->kind>NN_PLATFORM_SEQUENCE||p->bytes!=(p->kind==NN_PLATFORM_ZERO?4096u:4u)||s->words[p->word]||s->words[p->word+1])return EINVAL;
  for(size_t j=0;j<i;j++)if(p->word<s->patches[j].word+2&&s->patches[j].word<p->word+2)return EINVAL;
  int found=0;for(size_t at=0;at<s->word_count;){size_t n=nvn_pm4_packet_words(s->words[at]);uint32_t h=s->words[at];if(p->word==at+1&&(h>>28)==4&&n==3&&p->kind==NN_PLATFORM_ZERO)found=1;if(p->word==at+2&&h==nvn_pm4_packet7(0x46,4)&&p->kind==NN_PLATFORM_SEQUENCE)found=1;at+=n;}if(!found)return EINVAL;
 }
 return 0;
}
static void segment(uint32_t*words,size_t*n,const struct nn_pipeline_platform_segment*s,uint64_t job)
{
 size_t base=*n;memcpy(words+base,s->words,s->word_count*4);*n+=s->word_count;
 for(size_t i=0;i<s->patch_count;i++){const struct nn_pipeline_platform_patch*p=s->patches+i;uint64_t a=job+(p->kind==NN_PLATFORM_ZERO?ZERO:SEQUENCE);words[base+p->word]=(uint32_t)a;words[base+p->word+1]=(uint32_t)(a>>32);}
}
int nn_depth_transfer_platform_validate(nn_device*d,const struct nn_pipeline_platform*p)
{
 if(!d)return EINVAL;
 if(!p||p->version!=1||p->chip!=0x06050002||p->chip!=nn_device_chip(d)||p->gmem_bytes!=nn_device_gmem_bytes(d))return ENOTSUP;
 int e=segment_validate(&p->begin);return e?e:segment_validate(&p->end);
}
int nn_depth_readback_destroy(struct nn_depth_readback*r)
{
 if(!r)return EINVAL;
 if(r->storage){int e=nn_bo_destroy(r->storage);if(e)return e;}
 memset(r,0,sizeof(*r));return 0;
}
int nn_depth_readback_validate(const struct nn_depth_attachment*a,const struct nn_depth_readback*r)
{
 int e=nn_depth_attachment_validate(a);if(e)return e;
 if(!r||!r->ready||!r->storage||overlap(a->storage,r->storage))return EINVAL;
 struct nn_depth_a2d_view v;e=nn_depth_a2d(&a->view.desc,nn_bo_address(r->storage),&v);if(e)return e;
 if(v.linear.bytes>nn_bo_size(r->storage)||memcmp(&v,&r->view,sizeof(v)))return EINVAL;
 return 0;
}
int nn_depth_readback_create(nn_device*d,const struct nn_depth_attachment*a,struct nn_depth_readback*r)
{
 if(!r)return EINVAL;
 if(r->storage||r->ready)return EBUSY;
 memset(r,0,sizeof(*r));if(!d)return EINVAL;
 int e=nn_depth_attachment_validate(a);if(e)return e;
 if(nn_device_chip(d)!=a->view.desc.chip)return ENOTSUP;
 struct nn_depth_a2d_view zero;if((e=nn_depth_a2d(&a->view.desc,0,&zero)))return e;
 if((e=nn_bo_alloc(d,(size_t)zero.linear.bytes,&r->storage)))return e;
 if(!r->storage)return EPROTO;
 if((e=nn_depth_a2d(&a->view.desc,nn_bo_address(r->storage),&r->view)))goto failed;
 r->ready=1;if((e=nn_depth_readback_validate(a,r)))goto failed;
 return 0;
failed:r->ready=0;(void)nn_depth_readback_destroy(r);return e;
}
int nn_depth_transfer_resources(const struct nn_depth_attachment*a,const struct nn_depth_readback*r,uint32_t op,nn_bo*job,nn_bo**out,size_t cap,size_t*count)
{
 if(!count)return EINVAL;
 *count=0;int e=nn_depth_attachment_validate(a);if(e)return e;
 if(op>NN_DEPTH_A2D_UPLOAD||!job||nn_bo_size(job)<JOB_BYTES||overlap(a->storage,job))return EINVAL;
 if(op!=NN_DEPTH_A2D_CLEAR){if((e=nn_depth_readback_validate(a,r)))return e;if(overlap(r->storage,job))return EINVAL;}
 size_t n=op!=NN_DEPTH_A2D_CLEAR?3:2;if(!out||cap<n)return E2BIG;
 out[0]=a->storage;if(op!=NN_DEPTH_A2D_CLEAR)out[1]=r->storage;out[n-1]=job;*count=n;return 0;
}
int nn_depth_transfer_encode(nn_device*d,const struct nn_depth_attachment*a,const struct nn_depth_readback*r,const struct nn_pipeline_platform*p,uint32_t op,const struct nn_depth_a2d_region*region,float clear_depth,nn_bo**out,size_t*bytes)
{
 if(out)*out=NULL;
 if(bytes)*bytes=0;
 if(!d||!out||!bytes)return EINVAL;
 int e=nn_depth_attachment_validate(a);if(e)return e;
 if((e=nn_depth_transfer_platform_validate(d,p)))return e;
 if(op>NN_DEPTH_A2D_UPLOAD)return EINVAL;
 struct nn_depth_a2d_view v;
 if(op!=NN_DEPTH_A2D_CLEAR){if((e=nn_depth_readback_validate(a,r)))return e;v=r->view;}else if((e=nn_depth_a2d(&a->view.desc,0,&v)))return e;
 uint32_t body[96];size_t body_n;if((e=nn_depth_a2d_words(&v,op,region,clear_depth,body,96,&body_n)))return e;
 size_t order_n;if((e=nn_order_words(p->chip,4,3,NULL,0,&order_n)))return e;
 if(p->begin.word_count+p->end.word_count+body_n+2*order_n+6>COMMAND_WORDS)return E2BIG;
 nn_bo*job=NULL;e=nn_bo_alloc(d,JOB_BYTES,&job);if(e){*out=job;return e;}if(!job)return EPROTO;
 uint64_t address=nn_bo_address(job);nn_bo*refs[3];size_t refs_n;
 if(!address||address%4096||address>UINT64_MAX-JOB_BYTES){e=EINVAL;goto failed;}
 if((e=nn_depth_transfer_resources(a,r,op,job,refs,3,&refs_n)))goto failed;
 uint32_t words[COMMAND_WORDS];size_t n=0;uint64_t seq=address+SEQUENCE;
 words[n++]=nvn_pm4_packet7(NVN_CP_MEM_WRITE,3);words[n++]=(uint32_t)seq;words[n++]=(uint32_t)(seq>>32);words[n++]=0;words[n++]=nvn_pm4_packet7(0x12,0);
 if((e=nn_order_words(p->chip,seq,3,words+n,COMMAND_WORDS-n,&order_n)))goto failed;
 n+=order_n;
 segment(words,&n,&p->begin,address);memcpy(words+n,body,body_n*4);n+=body_n;
 if((e=nn_order_words(p->chip,seq,3,words+n,COMMAND_WORDS-n,&order_n)))goto failed;
 n+=order_n;
 segment(words,&n,&p->end,address);
 void*map=nn_bo_map(job);if(!map){e=EFAULT;goto failed;}memset(map,0,JOB_BYTES);memcpy(map,words,n*4);
 if((e=nn_bo_cache(job,0,JOB_BYTES,0)))goto failed;
 *out=job;*bytes=n*4;return 0;
failed:if(nn_bo_destroy(job))*out=job;return e;
}
