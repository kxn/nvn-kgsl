/* SPDX-License-Identifier: MIT. HOST addresses only; no image data execution. */
#include <stdio.h>
#include <string.h>
#include NVN_PUBLIC_COPY_HEADER
static unsigned checks;
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"geometry line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 for(unsigned n=0;n<NN_PUBLIC_COPY_CASES;n++){
  const struct nn_public_copy_case *c=nn_public_copy_cases+n;const struct nn_tex_source_layout *s=&c->source;struct nn_tex_source_layout actual;
  struct nn_tex_source_desc d={.flags=c->flags,.target=2,.format=c->format,.width=c->width,.height=c->height,.layers=1,.levels=c->levels,.block_height_log2=-1,.depth=s->volume_depth};
  CK(!nn_tex_source_layout(&d,&actual));CK(!memcmp(s,&actual,sizeof(actual)));CK(s->layers==1&&s->layer_stride==s->payload_bytes);
  CK(c->target==2&&(c->flags==0||c->flags==NVN_TEXTURE_FLAGS_IMAGE)&&s->volume_depth>1&&c->layer_count>1);CK(c->region[0].zoffset==(int)c->base_layer&&c->region[0].depth==(int)c->layer_count);
  CK(c->region[1].zoffset==0&&c->region[1].depth==(int)c->layer_count);
  for(unsigned l=0;l<s->levels;l++){
   const struct nn_tex_source_level *m=s->level+l;unsigned z=s->level_depths[l],bd=1u<<s->level_block_depths[l];
   unsigned zz[]={0,z/2,z-1,bd<z?bd:z-1},yy[]={0,m->blocks_y/2,m->blocks_y-1},xx[]={0,s->bytes_per_block-1,m->row_bytes/2,m->row_bytes-1};
   for(unsigned i=0;i<4;i++)for(unsigned j=0;j<3;j++)for(unsigned k=0;k<4;k++){
    uint64_t a=0;CK(!nn_tex_source_offset(s,l,zz[i],xx[k]/s->bytes_per_block,yy[j],xx[k]%s->bytes_per_block,&a));
    CK(a==nn_public_copy_oracle_offset(s,l,zz[i],xx[k],yy[j]));CK(a>=m->offset&&a<m->offset+m->bytes);
   }
   uint64_t bad=123;CK(nn_tex_source_offset(s,l,z,0,0,0,&bad)!=0&&bad==0);
   if(z>1){uint64_t a=nn_public_copy_oracle_offset(s,l,1,0,0);CK(a!=s->layer_stride+m->offset);}
  }
 }
 printf("{\"passed\":true,\"HOST_volume_geometry_checks\":%u,\"cases\":%u,\"GPU_execution\":false}\n",checks,NN_PUBLIC_COPY_CASES);return 0;
}
