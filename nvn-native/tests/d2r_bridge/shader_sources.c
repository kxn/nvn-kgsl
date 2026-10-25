/* HOST complete source ownership/index checks. No Maxwell/native shader runs. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include NN_SHADER_SOURCE_HEADER
static unsigned allocations,fail_allocation,checks;
static void*test_malloc(size_t n){if(++allocations==fail_allocation)return NULL;return malloc(n);}
static void*test_calloc(size_t n,size_t s){if(++allocations==fail_allocation)return NULL;return calloc(n,s);}
#define malloc test_malloc
#define calloc test_calloc
#include NN_SHADER_SOURCE_MODULE
#undef malloc
#undef calloc
#define T(x)do{checks++;if(!(x)){fprintf(stderr,"source catalogue line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 const struct nn_shader_source_archive*a=&NN_COMPLETE_GRAPHICS_SOURCE_ARCHIVE;struct nn_shader_source_catalogue*c=NULL;
 T(!nn_shader_source_catalogue_create(a,&c));T(c&&nn_shader_source_catalogue_count(c)==a->count);
 T(!nn_shader_source_entry_at(c,a->count));size_t sizes[2]={0};T(nn_shader_source_control_sizes(c,sizes)>0);
 const struct nn_shader_source_entry*first[2]={0};for(size_t i=0;i<a->count;i++){const struct nn_shader_source_entry*s=nn_shader_source_entry_at(c,i);if(!first[s->stage])first[s->stage]=s;}
 unsigned program_copies=0;for(size_t i=0;i<a->count;i++){
  const struct nn_shader_source_entry*s=nn_shader_source_entry_at(c,i);size_t begin=SIZE_MAX,end=SIZE_MAX;
  T(!nn_shader_source_candidates(c,s->control,s->control_bytes,&begin,&end)&&begin<=i&&i<end);
  T(end<=a->count);struct nn_shader_source_program*p=NULL;const struct nn_shader_source_entry*vs=s->stage?first[0]:s,*fs=s->stage?s:first[1];
  T(!nn_shader_source_program_create(c,vs,fs,&p));T(p);
  for(unsigned stage=0;stage<2;stage++){const struct nn_shader_source_entry*x=nn_shader_source_program_stage(p,stage),*ref=stage?fs:vs;T(x&&x->stage==stage&&x->source!=ref->source&&x->control!=ref->control&&x->code_bytes==ref->code_bytes&&x->source_bytes==ref->source_bytes&&x->control_bytes==ref->control_bytes);T(!memcmp(x->source,ref->source,x->source_bytes)&&!memcmp(x->control,ref->control,x->control_bytes)&&!memcmp(x->source_id,ref->source_id,32));}
  T(!nn_shader_source_program_stage(p,2));nn_shader_source_program_destroy(p);program_copies++;
 }
 unsigned char changed[0x898];memcpy(changed,first[0]->control,first[0]->control_bytes);changed[0]^=1;size_t begin=1,end=2;T(nn_shader_source_candidates(c,changed,first[0]->control_bytes,&begin,&end)==ENOTSUP&&!begin&&!end);
 T(nn_shader_source_candidates(c,changed,0x881,&begin,&end)==ENOTSUP);
 struct nn_shader_source_program*p=(void*)1;T(nn_shader_source_program_create(c,first[0],first[0],&p)==EINVAL&&!p);
 struct nn_shader_source_entry copied=*first[0];T(nn_shader_source_program_create(c,&copied,first[1],&p)==EINVAL&&!p);
 struct nn_shader_source_archive bad=*a;bad.version=2;struct nn_shader_source_catalogue*reject=(void*)1;T(nn_shader_source_catalogue_create(&bad,&reject)==EINVAL&&!reject);
 bad=*a;bad.count=65537;T(nn_shader_source_catalogue_create(&bad,&reject)==EINVAL&&!reject);
 bad=*a;bad.count=0;T(nn_shader_source_catalogue_create(&bad,&reject)==EINVAL&&!reject);
 struct nn_shader_source_entry*rows=malloc(a->count*sizeof(*rows));T(rows);memcpy(rows,a->entries,a->count*sizeof(*rows));bad=*a;bad.entries=rows;
 rows[0].stage=5;T(nn_shader_source_catalogue_create(&bad,&reject)==EINVAL&&!reject);rows[0]=a->entries[0];
 rows[0].profile=3;T(nn_shader_source_catalogue_create(&bad,&reject)==EINVAL&&!reject);rows[0]=a->entries[0];
 rows[0].source_bytes-=8;T(nn_shader_source_catalogue_create(&bad,&reject)==EINVAL&&!reject);rows[0]=a->entries[0];
 rows[0].code_bytes+=8;T(nn_shader_source_catalogue_create(&bad,&reject)==EINVAL&&!reject);rows[0]=a->entries[0];
 rows[0].control_bytes=0x880-1;T(nn_shader_source_catalogue_create(&bad,&reject)==EINVAL&&!reject);rows[0]=a->entries[0];
 if(a->count>1){rows[1]=rows[0];T(nn_shader_source_catalogue_create(&bad,&reject)==EEXIST&&!reject);}
 free(rows);
 for(unsigned fault=1;fault<=2;fault++){fail_allocation=allocations+fault;T(nn_shader_source_catalogue_create(a,&reject)==ENOMEM&&!reject);fail_allocation=0;}
 for(unsigned fault=1;fault<=2;fault++){fail_allocation=allocations+fault;T(nn_shader_source_program_create(c,first[0],first[1],&p)==ENOMEM&&!p);fail_allocation=0;}
 /* Program keeps full immutable C1/control after catalogue/index destruction. */
 T(!nn_shader_source_program_create(c,first[0],first[1],&p));struct nn_shader_source_entry expected[2]={*first[0],*first[1]};nn_shader_source_catalogue_destroy(c);c=NULL;
 for(unsigned stage=0;stage<2;stage++){const struct nn_shader_source_entry*s=nn_shader_source_program_stage(p,stage);T(s&&!memcmp(s->source,expected[stage].source,s->source_bytes)&&!memcmp(s->control,expected[stage].control,s->control_bytes));}
 nn_shader_source_program_destroy(p);nn_shader_source_catalogue_destroy(NULL);nn_shader_source_program_destroy(NULL);
 printf("{\"HOST_ONLY\":true,\"gpu_execution\":false,\"entries\":%zu,\"program_deep_copies\":%u,\"checks\":%u}\n",a->count,program_copies,checks);return 0;
}
