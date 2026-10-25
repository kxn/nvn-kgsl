/* SPDX-License-Identifier: MIT
 * Address-oracle bit equations derived from Chromium OS minigbm tegra.c,
 * Copyright2014 The Chromium OS Authors, BSD; notice: ../MINIGBM-LICENSE.
 * HOST ONLY geometry and address oracle. Never copies/decodes image pixels. */
#include "nvn_native/texture_source.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static unsigned checks;
#define CK(x) do{assert(x);checks++;}while(0)
static void address_check(const struct nn_tex_source_layout *l)
{
    CK(nn_tex_source_validate(l)==0);
    for(unsigned level=0;level<l->levels;level++){
        const struct nn_tex_source_level *m=l->level+level;unsigned bh=1u<<m->block_height_log2;
        unsigned char *seen=calloc(1,(size_t)m->bytes);CK(seen);
        for(unsigned br=0;br<(m->blocks_y+8*bh-1)/(8*bh);br++)for(unsigned bc=0;bc<m->pitch/64;bc++)for(unsigned k=0;k<128*bh;k++){
            /* Independent inverse dword ordering from minigbm Tegra's public
             * transfer_tile(k). It is an address oracle, not a runtime copy. */
            unsigned x=bc*16+(((k>>3)&8)|((k>>1)&4)|(k&3));
            unsigned y=br*8*bh+((k>>7<<3)|((k>>3)&6)|((k>>2)&1));
            for(unsigned lane=0;lane<4;lane++){
                unsigned xb=x*4+lane;if(xb>=m->row_bytes||y>=m->blocks_y)continue;
                uint64_t relative=((uint64_t)br*(m->pitch/64)+bc)*512*bh+k*4+lane,address=UINT64_MAX;
                CK(relative<m->bytes);CK(!seen[relative]);seen[relative]=1;
                CK(nn_tex_source_offset(l,level,l->layers-1,xb/l->bytes_per_block,y,xb%l->bytes_per_block,&address)==0);
                CK(address==(l->layers-1)*l->layer_stride+m->offset+relative);
            }
        }
        size_t n=0;for(size_t i=0;i<m->bytes;i++)n+=seen[i]!=0;CK(n==(uint64_t)m->row_bytes*m->blocks_y);free(seen);
        struct nn_tex_gob_copy_args a;CK(nn_tex_gob_copy_args(l,level,0,(m->row_bytes+255)&~255u,((uint64_t)m->row_bytes+255u)/256u*256u*m->blocks_y,&a)==0);
        CK(a.row_bytes==m->row_bytes&&a.rows==m->blocks_y&&a.block_height_log2==m->block_height_log2);
        CK(nn_tex_gob_copy_args(l,level,1,a.linear_pitch,(uint64_t)a.linear_pitch*a.rows,&a)==0&&a.direction==1);
    }
}
int main(void)
{
    struct nn_tex_source_desc d={.target=1,.format=0x25,.width=32,.height=8,.layers=1,.levels=1,.block_height_log2=-1};struct nn_tex_source_layout l;
    CK(nn_tex_source_layout(&d,&l)==0);CK(l.payload_bytes==1024&&l.alignment==0&&l.gob_bytes==512&&!l.public_extent_known&&l.base_block_height_log2==0);address_check(&l);
    d.width=64;d.height=33;d.levels=7;d.target=4;d.layers=3;CK(nn_tex_source_layout(&d,&l)==0);CK(l.level[0].bytes==16384&&l.level[1].bytes==2048&&l.layer_stride==22528&&l.payload_bytes==67584);address_check(&l);
    d.width=d.height=128;d.levels=8;d.layers=2;d.format=0x42;CK(nn_tex_source_layout(&d,&l)==0);CK(l.layer_stride==14336&&l.payload_bytes==28672);address_check(&l);
    d.format=0x4d;CK(nn_tex_source_layout(&d,&l)==0);CK(l.layer_stride==24576&&l.payload_bytes==49152);address_check(&l);
    d.width=20;d.height=13;d.levels=5;d.format=0x42;
    CK(nn_tex_source_layout(&d,&l)==0);CK(l.level[1].blocks_x==3&&l.level[1].blocks_y==2);address_check(&l);
    d.width=32;d.height=8;d.levels=1;d.format=0x25;d.block_height_log2=1;
    CK(nn_tex_source_layout(&d,&l)==0);CK(l.base_block_height_log2==1&&l.level[0].block_height_log2==0&&l.layer_stride==1024);address_check(&l);
    d=(struct nn_tex_source_desc){.target=1,.format=1,.width=31,.height=17,.layers=1,.levels=5,.block_height_log2=-1};CK(nn_tex_source_layout(&d,&l)==0);address_check(&l);
    for(unsigned f=5;f<=9;f++){
        struct nn_tex_source_desc r16=d;r16.format=f;
        CK(nn_tex_source_layout(&r16,&l)==0&&l.bytes_per_block==2);address_check(&l);
    }
    /* New scalar/packed storage classes exercise actual inverse GOB address
     * equations at each byte size; they do not decode/manufacture pixels. */
    const unsigned new_formats[]={2,3,4,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
        57,58,59,60,61,62,63,64};
    const unsigned new_cpp[]={1,1,1,4,4,4,2,2,2,2,4,4,4,4,4,8,8,8,
        2,2,2,2,4,4,4,4};
    for(unsigned i=0;i<sizeof(new_formats)/sizeof(*new_formats);i++){
        struct nn_tex_source_desc scalar=d;scalar.format=new_formats[i];
        CK(nn_tex_source_layout(&scalar,&l)==0&&l.bytes_per_block==new_cpp[i]);address_check(&l);
        scalar.flags=4;CK(nn_tex_source_layout(&scalar,&l)==0);CK(l.flags==4);address_check(&l);
    }
    const unsigned depth_cpp[]={1,2,4,4,4,8};
    for(unsigned i=0;i<6;i++){
        struct nn_tex_source_desc depth=d;depth.format=49+i;
        CK(nn_tex_source_layout(&depth,&l)==0&&l.bytes_per_block==depth_cpp[i]);address_check(&l);
    }
    d.flags=0x10;d.levels=1;d.stride=32;CK(nn_tex_source_layout(&d,&l)==0);CK(l.linear&&l.public_extent_known&&l.gob_bytes==0&&l.payload_bytes==544&&l.alignment==32);CK(nn_tex_source_validate(&l)==0);
    d.flags=0x40;d.stride=128;CK(nn_tex_source_layout(&d,&l)==0);CK(l.payload_bytes==2176&&l.alignment==128);
    d.flags=0x50;CK(nn_tex_source_layout(&d,&l)==0);CK(l.alignment==128);
    uint64_t at;CK(nn_tex_source_offset(&l,0,0,30,16,0,&at)==0&&at==2078);
    struct nn_tex_linear_copy_args linear;CK(nn_tex_linear_copy_args(&l,0,64,1088,&linear)==0&&linear.source_pitch==128&&linear.destination_pitch==64);
    CK(nn_tex_linear_copy_args(&l,1,64,1088,&linear)==0&&linear.source_pitch==64&&linear.destination_pitch==128);
    CK(nn_tex_linear_copy_args(&l,0,28,1088,&linear)==EINVAL);
    struct nn_tex_source_layout damaged=l;damaged.payload_bytes++;CK(nn_tex_source_validate(&damaged)==EINVAL);CK(nn_tex_source_offset(&damaged,0,0,0,0,0,&at)==EINVAL&&at==0);
    for(unsigned flags=0;flags<2048;flags++)if(flags&~0x55u){d.flags=flags;memset(&l,0x7c,sizeof(l));CK(nn_tex_source_layout(&d,&l)==ENOTSUP);struct nn_tex_source_layout zero={0};CK(!memcmp(&zero,&l,sizeof(l)));}
    d.flags=0x10;d.stride=31;CK(nn_tex_source_layout(&d,&l)==EINVAL);
    d.flags=0x8;CK(nn_tex_source_layout(&d,&l)==ENOTSUP);
    printf("HOST ONLY source-layout/address checks=%u; no GPU or CPU pixel conversion\n",checks);return 0;
}
