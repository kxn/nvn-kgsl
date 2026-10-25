/* SPDX-License-Identifier: MIT. CPU METADATA ONLY; no GPU/BO/device/pixel access.
 * The only FDL interface used here is the promoted C facade runtime_fdl.h.
 * Address values are synthetic integers, never mapped or dereferenced. */
#include "nvn_native/runtime_fdl.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
_Static_assert(sizeof(struct nn_fdl_image_desc)==64,"image input C ABI");
_Static_assert(sizeof(struct nn_fdl_image_view)==680,"image output C ABI");
_Static_assert(sizeof(struct nn_fdl_buffer_desc)==24,"BUFFER input C ABI");
_Static_assert(sizeof(struct nn_fdl_buffer_view)==120,"BUFFER output C ABI");
static unsigned checks,images,buffers,negatives,records;
static FILE *output;
#define C(x) do { checks++; if(!(x)){fprintf(stderr,"fail:%d:%s\n",__LINE__,#x);return 1;} } while(0)
static void put32(unsigned char *p,uint32_t v){for(unsigned k=0;k<4;k++)p[k]=(unsigned char)(v>>(8*k));}
static void put64(unsigned char *p,uint64_t v){for(unsigned k=0;k<8;k++)p[k]=(unsigned char)(v>>(8*k));}
static int word(uint32_t v){unsigned char bytes[4];put32(bytes,v);return fwrite(bytes,1,4,output)==4?0:1;}
static int zero(const void *v,size_t n){const unsigned char *p=v;while(n--)if(*p++)return 0;return 1;}
/* Serialize every public field at its C-ABI offset into zeroed storage. Compare
 * the actual returned struct to this complete value representation, including
 * padding: nondeterministic/nonzero padding is a qualification failure. */
static int image_padding(const struct nn_fdl_image_view *v)
{
 unsigned char bytes[sizeof(*v)];memset(bytes,0,sizeof(bytes));
#define U32(n) put32(bytes+offsetof(struct nn_fdl_image_view,n),v->n)
#define U64(n) put64(bytes+offsetof(struct nn_fdl_image_view,n),v->n)
 U32(schema);U32(chip);U32(format);U32(target);U32(width);U32(height);U32(levels);U32(layers);
 U32(block_width);U32(block_height);U32(bytes_per_block);U32(alignment);U32(view_width);U32(view_height);
 U32(base_level);U32(level_count);U32(base_layer);U32(layer_count);U32(descriptor_address_word);
 U64(bytes);U64(required_span);U64(view_offset);U64(descriptor_address_mask);
 for(unsigned i=0;i<16;i++)put32(bytes+offsetof(struct nn_fdl_image_view,descriptor)+4*i,v->descriptor[i]);
 U32(color_target);U32(rb_mrt_buf_info);U32(rb_mrt_pitch);U32(rb_mrt_array_pitch);U32(sp_ps_mrt_reg);
#undef U32
#undef U64
 for(unsigned i=0;i<NN_FDL_LEVELS;i++){
  unsigned char *p=bytes+offsetof(struct nn_fdl_image_view,level)+i*sizeof(struct nn_fdl_level);
  const struct nn_fdl_level *m=v->level+i;
  put32(p+offsetof(struct nn_fdl_level,width),m->width);put32(p+offsetof(struct nn_fdl_level,height),m->height);
  put32(p+offsetof(struct nn_fdl_level,pitch),m->pitch);put32(p+offsetof(struct nn_fdl_level,layer_stride),m->layer_stride);
  put64(p+offsetof(struct nn_fdl_level,offset),m->offset);put64(p+offsetof(struct nn_fdl_level,span),m->span);
 }
 return !memcmp(bytes,v,sizeof(bytes));
}
static int buffer_padding(const struct nn_fdl_buffer_view *v)
{
 unsigned char bytes[sizeof(*v)];memset(bytes,0,sizeof(bytes));
#define U32(n) put32(bytes+offsetof(struct nn_fdl_buffer_view,n),v->n)
#define U64(n) put64(bytes+offsetof(struct nn_fdl_buffer_view,n),v->n)
 U32(schema);U32(chip);U32(format);U32(element_bytes);U32(elements);
 for(unsigned i=0;i<16;i++)put32(bytes+offsetof(struct nn_fdl_buffer_view,descriptor)+4*i,v->descriptor[i]);
 U64(base_address);U64(required_prefix);U64(required_span);U64(address_mask);
#undef U32
#undef U64
 return !memcmp(bytes,v,sizeof(bytes));
}
static int record(unsigned kind,unsigned index,unsigned input_null,const void *d,size_t dn,const void *v,size_t vn,int error)
{
 C(!word(kind)&&!word(index)&&!word(input_null)&&!word((uint32_t)dn)&&!word((uint32_t)vn)&&!word((uint32_t)error));
 C(fwrite(d,1,dn,output)==dn&&fwrite(v,1,vn,output)==vn);records++;return 0;
}
static int image(const struct nn_fdl_image_desc *d,int expected,unsigned input_null)
{
 struct nn_fdl_image_view a,b;memset(&a,0xa5,sizeof(a));memset(&b,0x5a,sizeof(b));
 int e=nn_fdl_image(input_null?NULL:d,&a);C(e==expected);C(nn_fdl_image(input_null?NULL:d,&b)==e);
 C(!memcmp(&a,&b,sizeof(a))&&image_padding(&a));
 if(e){C(zero(&a,sizeof(a)));negatives++;}
 else {
  C(a.schema==1&&a.chip==d->chip&&a.format==d->format&&a.target==d->target);
  C(a.width==d->width&&a.height==d->height&&a.levels==d->levels&&a.layers==d->layers);
  C(a.bytes&&a.required_span>=a.bytes&&a.alignment&&a.descriptor_address_word==4&&a.descriptor_address_mask);
  C(a.base_level==d->base_level&&a.level_count==d->level_count&&a.base_layer==d->base_layer&&a.layer_count==d->layer_count);
  C(a.view_width==(d->width>>d->base_level?d->width>>d->base_level:1)&&a.view_height==(d->height>>d->base_level?d->height>>d->base_level:1));
  C(a.color_target==((d->usage&NN_FDL_COLOR_TARGET)!=0));
  if(!d->address)C(!(a.descriptor[4]&(uint32_t)a.descriptor_address_mask)&&!(a.descriptor[5]&(uint32_t)(a.descriptor_address_mask>>32)));
  for(unsigned k=d->levels;k<NN_FDL_LEVELS;k++)C(zero(a.level+k,sizeof(a.level[k])));
  images++;
 }
 C(!record(e?3u:1u,e?negatives-1:images-1,input_null,d,sizeof(*d),&a,sizeof(a),e));return 0;
}
static int buffer(const struct nn_fdl_buffer_desc *d,int expected,unsigned input_null)
{
 struct nn_fdl_buffer_view a,b;memset(&a,0xa5,sizeof(a));memset(&b,0x5a,sizeof(b));
 int e=nn_fdl_buffer(input_null?NULL:d,&a);C(e==expected);C(nn_fdl_buffer(input_null?NULL:d,&b)==e);
 C(!memcmp(&a,&b,sizeof(a))&&buffer_padding(&a));
 if(e){C(zero(&a,sizeof(a)));negatives++;}
 else {C(a.schema==1&&a.chip==d->chip&&a.format==d->format&&a.element_bytes==4&&a.elements==d->bytes/4);
  C(a.base_address+a.required_prefix==d->address&&a.required_span==a.required_prefix+d->bytes&&a.address_mask);buffers++;}
 C(!record(e?4u:2u,e?negatives-1:buffers-1,input_null,d,sizeof(*d),&a,sizeof(a),e));return 0;
}
static int run(void)
{
 const uint32_t endian=1;C(*(const unsigned char*)&endian==1);C(nn_fdl_metadata_version()==1);
 C(fwrite("NNFDMETA",1,8,output)==8);C(!word(1)&&!word(456)&&!word(48)&&!word(26));
 C(!word(sizeof(struct nn_fdl_image_desc))&&!word(sizeof(struct nn_fdl_image_view))&&!word(sizeof(struct nn_fdl_buffer_desc))&&!word(sizeof(struct nn_fdl_buffer_view)));
 const unsigned dims[][2]={{1,1},{2,2},{3,5},{64,64},{127,73},{2880,1620},{197,113},{8191,17},{16384,1},{1,16384},{16384,16384}};
 const uint32_t formats[]={37,0x41,0x42,0x43,0x44};uint32_t seed=20261024;
 for(unsigned f=0;f<5;f++)for(unsigned i=0;i<75;i++){
  unsigned w,h;if(i<11){w=dims[i][0];h=dims[i][1];}else{seed=seed*1664525u+1013904223u;w=1+seed%2048;seed=seed*1664525u+1013904223u;h=1+seed%2048;}
  unsigned max=w>h?w:h,levels=1;while(max>>=1)levels++;if(i%2==0)levels=1;
  struct nn_fdl_image_desc d;memset(&d,0,sizeof(d));d.schema=1;d.chip=0x06050002;d.format=formats[f];d.target=i%3?1:4;d.width=w;d.height=h;d.levels=levels;d.layers=d.target==4&&i>10?3:1;
  d.base_level=levels>1?1:0;d.level_count=levels-d.base_level;d.base_layer=d.layers-1;d.layer_count=1;d.usage=NN_FDL_SAMPLED;d.address=i%2?UINT64_C(0x123450000):0;C(!image(&d,0,0));
  if(d.format==37){d.usage|=NN_FDL_COLOR_TARGET;d.level_count=1;C(!image(&d,0,0));}
 }
 const unsigned widths[]={1,197,2880};for(unsigned i=0;i<3;i++)for(unsigned j=0;j<2;j++){
  struct nn_fdl_image_desc d;memset(&d,0,sizeof(d));d.schema=1;d.chip=0x06050002;d.format=37;d.target=1;d.width=widths[i];d.height=113;d.levels=d.layers=d.level_count=d.layer_count=1;d.usage=3;d.explicit_pitch=((d.width*4+63)/64)*64+j*64;C(!image(&d,0,0));
 }
 const uint32_t fs[]={37,10},sizes[]={4,8,124,128,131072,0xfffffffcu};
 const uint64_t addresses[]={0,UINT64_C(0x123450000),UINT64_C(0x123450004),UINT64_C(0x12345003c)};
 for(unsigned f=0;f<2;f++)for(unsigned s=0;s<6;s++)for(unsigned a=0;a<4;a++){
  struct nn_fdl_buffer_desc d={1,0x06050002,fs[f],sizes[s],addresses[a]};C(!buffer(&d,0,0));
 }
 struct nn_fdl_image_desc good;memset(&good,0,sizeof(good));good.schema=1;good.chip=0x06050002;good.format=37;good.target=1;good.width=good.height=64;good.levels=good.layers=good.level_count=good.layer_count=1;good.usage=3;
 C(!image(&good,EINVAL,1));C(nn_fdl_image(&good,NULL)==EINVAL);
 for(unsigned n=0;n<18;n++){
  struct nn_fdl_image_desc d=good;int error=ENOTSUP;
  switch(n){case 0:d.schema=2;break;case 1:d.chip=0x43030b00;break;case 2:d.format=0x32;break; /* DEPTH16 remains unsupported. R32F is now an image format. */case 3:d.target=2;break;case 4:d.width=0;break;case 5:d.width=16385;break;case 6:d.levels=16;break;case 7:d.layers=0;break;case 8:d.layers=2;break;case 9:d.base_level=1;error=EINVAL;break;case 10:d.level_count=2;error=EINVAL;break;case 11:d.base_layer=1;error=EINVAL;break;case 12:d.layer_count=2;error=EINVAL;break;case 13:d.usage=4;break;case 14:d.format=0x42;break;case 15:d.address=1;error=EINVAL;break;case 16:d.explicit_pitch=257;break;case 17:d.width=d.height=16384;d.layers=2048;d.target=4;error=E2BIG;break;}
  C(!image(&d,error,0));
 }
 struct nn_fdl_buffer_desc b={1,0x06050002,37,4,0};C(!buffer(&b,EINVAL,1));C(nn_fdl_buffer(&b,NULL)==EINVAL);
 for(unsigned i=0;i<6;i++){struct nn_fdl_buffer_desc d=b;int error=EINVAL;switch(i){case 0:d.format=0x42;error=ENOTSUP;break;case 1:d.bytes=0;break;case 2:d.bytes=3;break;case 3:d.address=1;break;case 4:d.address=UINT64_C(1)<<49;break;case 5:d.address=(UINT64_C(1)<<49)-4;d.bytes=8;break;}C(!buffer(&d,error,0));}
 C(images==456&&buffers==48&&negatives==26&&records==530);return 0;
}
int main(int argc,char **argv)
{
 if(argc!=3){fprintf(stderr,"usage: %s METADATA_BIN RESULT_JSON\n",argv[0]);return 2;}
 output=fopen(argv[1],"wbx");if(!output){perror("metadata output");return 2;}
 int e=run();if(fclose(output))e=1;
 FILE *j=fopen(argv[2],"wx");if(!j){perror("JSON output");return 2;}
 if(fprintf(j,"{\"schema\":1,\"operation\":\"runtime_fdl_cpu_metadata\",\"passed\":%s,\"metadata_version\":1,\"checks\":%u,\"images\":%u,\"buffers\":%u,\"negative_cases\":%u,\"records\":%u,\"gpu_submits\":0,\"gpu_allocations\":0,\"device_opens\":0,\"image_data_access\":false}\n",e?"false":"true",checks,images,buffers,negatives,records)<0)e=1;
 if(fclose(j))e=1;
 return e?1:0;
}
