/* SPDX-License-Identifier: MIT. No compiler, reflection or query expectations. */
#include "nvn_native/shader_source.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
struct nn_shader_source_catalogue {struct nn_shader_source_entry *entries;size_t count,sizes[2],size_count;};
struct nn_shader_source_program {uint32_t version;struct nn_shader_source_entry stages[2];unsigned char *bytes;};
static uint32_t le32(const unsigned char*p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int valid(const struct nn_shader_source_entry*s)
{
 if(!s||s->stage>1||!s->source||!s->control||s->code_bytes<128||s->code_bytes%8||s->source_bytes<s->code_bytes||s->source_bytes>1048576||s->source_bytes%8||
    (s->profile!=1&&s->profile!=2)||s->control_bytes!=(s->profile==1?0x880u:0x898u))return EINVAL;
 if(le32(s->source)!=0x12345678||((le32(s->source+0x30)>>10)&15)!=(s->stage?5u:1u)||le32(s->control)!=0x98761234||
    le32(s->control+0x700)!=s->code_bytes||le32(s->control+0x704)!=s->source_bytes)return EINVAL;
 unsigned nonzero=0;for(unsigned i=0;i<32;i++)nonzero|=s->source_id[i];return nonzero?0:EINVAL;
}
static int control_cmp(const void*bytes,size_t n,const struct nn_shader_source_entry*s)
{if(n!=s->control_bytes)return n<s->control_bytes?-1:1;return memcmp(bytes,s->control,n);}
static int compare(const struct nn_shader_source_entry*a,const struct nn_shader_source_entry*b)
{
 int x=control_cmp(a->control,a->control_bytes,b);if(x)return x;
 if(a->source_bytes!=b->source_bytes)return a->source_bytes<b->source_bytes?-1:1;
 return memcmp(a->source,b->source,a->source_bytes);
}
int nn_shader_source_catalogue_create(const struct nn_shader_source_archive*a,struct nn_shader_source_catalogue**out)
{
 if(!out)return EINVAL;
 *out=NULL;if(!a||a->version!=1||!a->entries||!a->count||a->count>65536)return EINVAL;
 unsigned mask=0;for(size_t i=0;i<a->count;i++){int e=valid(a->entries+i);if(e)return e;mask|=1u<<a->entries[i].stage;if(i){int c=compare(a->entries+i-1,a->entries+i);if(c>=0)return c?EINVAL:EEXIST;}}
 if(mask!=3)return ENOTSUP;
 struct nn_shader_source_catalogue*c=calloc(1,sizeof(*c));if(!c)return ENOMEM;
 c->entries=malloc(a->count*sizeof(*c->entries));if(!c->entries){free(c);return ENOMEM;}
 memcpy(c->entries,a->entries,a->count*sizeof(*c->entries));c->count=a->count;
 for(size_t i=0;i<c->count;i++)if(!i||c->entries[i].control_bytes!=c->entries[i-1].control_bytes)c->sizes[c->size_count++]=c->entries[i].control_bytes;
 *out=c;return 0;
}
void nn_shader_source_catalogue_destroy(struct nn_shader_source_catalogue*c){if(c){free(c->entries);free(c);}}
size_t nn_shader_source_catalogue_count(const struct nn_shader_source_catalogue*c){return c?c->count:0;}
const struct nn_shader_source_entry*nn_shader_source_entry_at(const struct nn_shader_source_catalogue*c,size_t i){return c&&i<c->count?c->entries+i:NULL;}
size_t nn_shader_source_control_sizes(const struct nn_shader_source_catalogue*c,size_t out[2])
{if(!c||!out)return 0;for(size_t i=0;i<c->size_count;i++)out[i]=c->sizes[i];return c->size_count;}
int nn_shader_source_candidates(const struct nn_shader_source_catalogue*c,const void*control,size_t n,size_t*first,size_t*end)
{
 if(first)*first=0;
 if(end)*end=0;
 if(!c||!control||!first||!end)return EINVAL;
 if(n!=0x880&&n!=0x898)return ENOTSUP;
 size_t low=0,high=c->count;while(low<high){size_t mid=low+(high-low)/2;int cmp=control_cmp(control,n,c->entries+mid);if(cmp>0)low=mid+1;else high=mid;}
 size_t begin=low;if(begin==c->count||control_cmp(control,n,c->entries+begin))return ENOTSUP;
 high=c->count;while(low<high){size_t mid=low+(high-low)/2;int cmp=control_cmp(control,n,c->entries+mid);if(cmp<0)high=mid;else low=mid+1;}
 *first=begin;*end=low;return 0;
}
int nn_shader_source_program_create(const struct nn_shader_source_catalogue*c,const struct nn_shader_source_entry*vs,const struct nn_shader_source_entry*fs,struct nn_shader_source_program**out)
{
 if(!out)return EINVAL;
 *out=NULL;if(!c||!vs||!fs)return EINVAL;
 const struct nn_shader_source_entry*s[2]={vs,fs};size_t total=0;
 for(unsigned stage=0;stage<2;stage++){
  int member=0;for(size_t i=0;i<c->count;i++)if(s[stage]==c->entries+i){member=1;break;}
  if(!member||s[stage]->stage!=stage||valid(s[stage]))return EINVAL;
  if(s[stage]->source_bytes>SIZE_MAX-total||s[stage]->control_bytes>SIZE_MAX-total-s[stage]->source_bytes)return EOVERFLOW;
  total+=s[stage]->source_bytes+s[stage]->control_bytes;
 }
 struct nn_shader_source_program*p=calloc(1,sizeof(*p));if(!p)return ENOMEM;
 p->bytes=malloc(total);if(!p->bytes){free(p);return ENOMEM;}size_t cursor=0;
 for(unsigned i=0;i<2;i++){p->stages[i]=*s[i];p->stages[i].source=p->bytes+cursor;memcpy(p->bytes+cursor,s[i]->source,s[i]->source_bytes);cursor+=s[i]->source_bytes;p->stages[i].control=p->bytes+cursor;memcpy(p->bytes+cursor,s[i]->control,s[i]->control_bytes);cursor+=s[i]->control_bytes;}
 p->version=1;*out=p;return 0;
}
void nn_shader_source_program_destroy(struct nn_shader_source_program*p){if(p){free(p->bytes);memset(p,0,sizeof(*p));free(p);}}
const struct nn_shader_source_entry*nn_shader_source_program_stage(const struct nn_shader_source_program*p,unsigned i)
{return p&&p->version==1&&i<2?p->stages+i:NULL;}
