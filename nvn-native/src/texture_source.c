/* SPDX-License-Identifier: MIT
 * Equations: original SDK native_tile_shape/native_mip_layout/static array
 * evidence, independent deko3d; Generic16Bx2 sector map: minigbm Tegra.
 * This does not implement NVIDIA hardware compression/metadata or Adreno UBWC. */
#include "nvn_native/texture_source.h"
#include "nvn_native/texture_volume.h"
#include "nvn_native/texture_buffer_format.h"
#include <errno.h>
#include <string.h>
static uint64_t up(uint64_t v,uint64_t a){return (v+a-1)&~(a-1);}
static unsigned reduced(unsigned exponent,uint32_t h)
{while(exponent&&h<=(8u<<(exponent-1)))exponent--;return exponent;}
static int format(uint32_t f,uint32_t *w,uint32_t *h,uint32_t *bytes)
{
    *w=*h=1;
    /* Share independently checked scalar storage sizes, not BUFFER usage or
     * native sampling capability. This source profile admits power-of-two
     * color elements only; RGB8/16/32 stay unqualified.
     * SDK53.314 format records +e/+f=0, +10=cpp and raw-class2 independently
     * cover this family, including packed HDR R11G11B10F/RGB9E5F. */
    if((f>=1&&f<=24)||(f>=37&&f<=48)||(f>=57&&f<=64)||f==154)
        return nn_tex_buffer_element_bytes(f,bytes);
    switch(f){
    /* SDK's uncompressed depth/stencil format records have the following
     * element bytes. This is storage only, never depth-render admission. */
    case 0x31:*bytes=1;break;
    case 0x32:*bytes=2;break;
    case 0x33:case 0x34:case 0x35:*bytes=4;break;
    case 0x36:*bytes=8;break;
    case 0x38:case 0x9c:*bytes=4;break;
    case 0x41:case 0x42:case 0x45:case 0x46:case 0x49:case 0x4a:*w=*h=4;*bytes=8;break;
    case 0x43:case 0x44:case 0x47:case 0x48:case 0x4b:case 0x4c:case 0x4d:case 0x4e:case 0x4f:case 0x50:*w=*h=4;*bytes=16;break;
    default:return ENOTSUP;
    }
    return 0;
}
#include "texture_volume.inc"
static int source_volume_layout(const struct nn_tex_source_desc *d,struct nn_tex_source_layout *out)
{
    /* IMAGE is a usage permission, not a second volume address equation.
     * Independent deko3d dk_image.cpp separates UsageLoadStore admission
     * from its dimsPerLayer==3 tileDepth calculation. This source-only
     * Generic16Bx2 policy does not grant an image load/store GPU consumer. */
    if((d->flags&~4u)||d->samples||d->stride||d->layers!=1||d->block_height_log2!=-1||d->block_depth_log2>5)return ENOTSUP;
    struct nn_tex_volume_desc v={d->format,d->width,d->height,d->depth,d->levels,
        d->block_depth_log2?(int32_t)d->block_depth_log2-1:-1};
    struct nn_tex_volume_layout a;int e=nn_tex_volume_layout(&v,&a);if(e)return e;
    struct nn_tex_source_layout l={0};l.schema=1;l.flags=d->flags;l.target=2;l.format=d->format;l.levels=d->levels;l.layers=1;
    l.block_w=a.block_w;l.block_h=a.block_h;l.bytes_per_block=a.bytes_per_block;l.gob_bytes=512;
    l.payload_bytes=l.layer_stride=a.payload_bytes;l.volume_depth=d->depth;l.base_block_depth_log2=a.base_block_depth_log2;
    for(unsigned i=0;i<a.levels;i++){
        const struct nn_tex_volume_level *m=a.level+i;
        l.level[i]=(struct nn_tex_source_level){.offset=m->offset,.bytes=m->bytes,.width=m->width,.height=m->height,
            .blocks_x=m->blocks_x,.blocks_y=m->blocks_y,.row_bytes=m->row_bytes,.pitch=m->pitch};
        l.level_depths[i]=m->depth;l.level_block_depths[i]=m->block_depth_log2;
    }
    *out=l;return 0;
}
int nn_tex_source_layout(const struct nn_tex_source_desc *d,struct nn_tex_source_layout *out)
{
    if(!out)return EINVAL;
    memset(out,0,sizeof(*out));if(!d)return EINVAL;
    if(d->target==2)return source_volume_layout(d,out);
    if(d->depth||d->block_depth_log2)return ENOTSUP;
    /* Only source storage is described; DISPLAY/IMAGE do not thereby gain a
     * GPU consumer. COMPRESSIBLE needs chosen-kind/page-alignment semantics. */
    if(d->flags&~0x55u||d->samples||(d->target!=1&&d->target!=4)||!d->width||!d->height||
       d->width>16384||d->height>16384||!d->layers||d->layers>2048||!d->levels||d->levels>NN_TEX_SOURCE_MAX_LEVELS||
       (d->target==1&&d->layers!=1)||d->block_height_log2< -1||d->block_height_log2>5)return ENOTSUP;
    struct nn_tex_source_layout l;memset(&l,0,sizeof(l));l.schema=1;l.flags=d->flags;l.target=d->target;l.format=d->format;l.levels=d->levels;l.layers=d->layers;
    int e=format(d->format,&l.block_w,&l.block_h,&l.bytes_per_block);if(e)return e;
    unsigned maxdim=d->width>d->height?d->width:d->height,maxlevels=1;while(maxdim>>=1)maxlevels++;
    if(d->levels>maxlevels)return EINVAL;
    if((d->flags&1u)&&(d->target!=1||d->levels!=1||d->flags&0x50u))return ENOTSUP;
    if(d->flags&0x50u){
        if(d->target!=1||d->levels!=1||l.block_w!=1||!d->stride)return ENOTSUP;
        l.linear=1;l.public_extent_known=1;l.alignment=d->flags&0x40u?128:32;
        if(d->stride<d->width*l.bytes_per_block||d->stride%l.alignment||d->stride>UINT32_MAX)return EINVAL;
        l.layer_stride=l.payload_bytes=d->stride*d->height;if(l.payload_bytes>UINT32_MAX)return EOVERFLOW;l.level[0]=(struct nn_tex_source_level){.bytes=l.payload_bytes,.width=d->width,.height=d->height,.blocks_x=d->width,.blocks_y=d->height,.row_bytes=d->width*l.bytes_per_block,.pitch=(uint32_t)d->stride};
        *out=l;return 0;
    }
    if(d->stride)return EINVAL;
    /* Generic non-sparse tileWidthGobs=1: SDK array alignment operates on
     * GOB widths, independently of the format texel-block width. */
    unsigned base=d->block_height_log2;
    if(d->block_height_log2<0){
        unsigned h=(d->height+l.block_h-1)/l.block_h,hhalf=h+h/2;base=0;
        while(base<4&&hhalf>=(16u<<base))base++;
    }
    l.gob_bytes=512;l.base_block_height_log2=base;uint64_t offset=0;
    for(unsigned i=0;i<d->levels;i++){
        uint32_t w=d->width>>i,h=d->height>>i;if(!w)w=1;if(!h)h=1;
        uint32_t bx=(w+l.block_w-1)/l.block_w,by=(h+l.block_h-1)/l.block_h,row=bx*l.bytes_per_block;
        unsigned sh=reduced(base,by);uint64_t pitch=up(row,64),size=pitch*up(by,8u<<sh);
        l.level[i]=(struct nn_tex_source_level){.offset=offset,.bytes=size,.width=w,.height=h,.blocks_x=bx,.blocks_y=by,.row_bytes=row,.pitch=(uint32_t)pitch,.block_height_log2=sh};offset+=size;
    }
    l.layer_stride=d->target==4?up(offset,512u<<l.level[0].block_height_log2):offset;
    l.payload_bytes=l.layer_stride*d->layers;if(l.payload_bytes>UINT32_MAX)return EOVERFLOW;
    *out=l;return 0;
}
int nn_tex_source_validate(const struct nn_tex_source_layout *l)
{
    if(!l||l->schema!=1)return EINVAL;
    struct nn_tex_source_desc d={.flags=l->flags,.target=l->target,.format=l->format,
        .width=l->level[0].width,.height=l->level[0].height,.layers=l->layers,.levels=l->levels,
        .stride=l->linear?l->level[0].pitch:0,.block_height_log2=l->target==2||l->linear?-1:(int32_t)l->base_block_height_log2,
        .depth=l->volume_depth,.block_depth_log2=l->target==2?l->base_block_depth_log2+1:0};
    struct nn_tex_source_layout expected;int e=nn_tex_source_layout(&d,&expected);
    if(e||memcmp(&expected,l,sizeof(expected)))return EINVAL;
    return 0;
}
int nn_tex_source_volume(const struct nn_tex_source_layout *l,struct nn_tex_volume_layout *out)
{
    if(!out)return EINVAL;
    memset(out,0,sizeof(*out));if(nn_tex_source_validate(l))return EINVAL;
    if(l->target!=2)return ENOTSUP;
    struct nn_tex_volume_desc d={l->format,l->level[0].width,l->level[0].height,l->volume_depth,l->levels,(int32_t)l->base_block_depth_log2};
    return nn_tex_volume_layout(&d,out);
}

static uint64_t gob(uint32_t x,uint32_t y,uint32_t width_gobs,unsigned sh)
{
    uint64_t bh=UINT64_C(1)<<sh;
    return (y/(8*bh))*512*bh*width_gobs+(x/64)*512*bh+((y%(8*bh))/8)*512+
        ((x%64)/32)*256+((y%8)/2)*64+((x%32)/16)*32+(y%2)*16+(x%16);
}
int nn_tex_source_offset(const struct nn_tex_source_layout *l,uint32_t level,uint32_t layer,
                        uint32_t bx,uint32_t by,uint32_t byte,uint64_t *out)
{
    if(!out)return EINVAL;
    *out=0;if(nn_tex_source_validate(l)||level>=l->levels||level>=NN_TEX_SOURCE_MAX_LEVELS||byte>=l->bytes_per_block)return EINVAL;
    if(l->target==2){struct nn_tex_volume_layout v;int e=nn_tex_source_volume(l,&v);if(e)return e;return nn_tex_volume_offset(&v,level,layer,bx,by,byte,out);}
    if(layer>=l->layers)return EINVAL;
    const struct nn_tex_source_level *m=l->level+level;if(bx>=m->blocks_x||by>=m->blocks_y||m->block_height_log2>5||!m->pitch||m->pitch%32)return EINVAL;
    uint64_t x=(uint64_t)bx*l->bytes_per_block+byte;
    uint64_t offset=l->linear?by*(uint64_t)m->pitch+x:gob((uint32_t)x,by,m->pitch/64,m->block_height_log2);
    if(offset>=m->bytes||m->offset+offset>=l->layer_stride)return EINVAL;
    *out=layer*l->layer_stride+m->offset+offset;return 0;
}
int nn_tex_gob_copy_args(const struct nn_tex_source_layout *l,uint32_t level,uint32_t direction,
                        uint32_t pitch,uint64_t bytes,struct nn_tex_gob_copy_args *out)
{
    if(!out)return EINVAL;
    memset(out,0,sizeof(*out));if(nn_tex_source_validate(l)||l->linear||l->target==2||level>=l->levels||level>=NN_TEX_SOURCE_MAX_LEVELS||direction>1)return ENOTSUP;
    const struct nn_tex_source_level *m=l->level+level;
    if(pitch<m->row_bytes||pitch%4||bytes>UINT32_MAX||bytes<(uint64_t)pitch*m->blocks_y||(uint64_t)pitch*m->blocks_y>UINT32_MAX||m->block_height_log2>5||!m->pitch||m->pitch%64)return EINVAL;
    *out=(struct nn_tex_gob_copy_args){0,0,m->row_bytes,m->blocks_y,pitch,m->pitch/64,m->block_height_log2,direction};return 0;
}

int nn_tex_linear_copy_args(const struct nn_tex_source_layout *l,uint32_t direction,
    uint32_t pitch,uint64_t bytes,struct nn_tex_linear_copy_args *out)
{
    if(!out)return EINVAL;
    memset(out,0,sizeof(*out));if(nn_tex_source_validate(l)||!l->linear||direction>1)return ENOTSUP;
    const struct nn_tex_source_level *m=l->level;
    if(pitch<m->row_bytes||pitch%4||bytes>UINT32_MAX||bytes<(uint64_t)pitch*m->blocks_y)return EINVAL;
    *out=(struct nn_tex_linear_copy_args){0,0,m->row_bytes,m->blocks_y,direction?pitch:m->pitch,direction?m->pitch:pitch};return 0;
}
