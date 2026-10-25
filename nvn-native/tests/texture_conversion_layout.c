/* SPDX-License-Identifier: MIT
 * HOST metadata producer: actual common FDL plus bounded source calculator.
 * Calculates layout only; no image contents or CPU image conversion. */
#include "nvn_native/texture_source.h"
#include "freedreno_layout.h"
#include "adreno/a6xx_enums.xml.h"
#include "freedreno_dev_info.h"
#include "util/format/u_format.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static enum pipe_format format(unsigned v)
{
    switch(v){case 1:return PIPE_FORMAT_R8_UNORM;case 6:return PIPE_FORMAT_R16_UNORM;
    case 0x25:return PIPE_FORMAT_R8G8B8A8_UNORM;case 0x2a:return PIPE_FORMAT_R16G16B16A16_UNORM;
    case 0x2e:return PIPE_FORMAT_R32G32B32A32_FLOAT;case 0x41:return PIPE_FORMAT_DXT1_RGB;
    case 0x44:return PIPE_FORMAT_DXT5_RGBA;default:return PIPE_FORMAT_NONE;}
}
int main(int argc,char **argv)
{
    if(argc!=10)return EINVAL;
    struct nn_tex_source_desc d={.flags=(uint32_t)strtoul(argv[1],NULL,0),.target=(uint32_t)strtoul(argv[2],NULL,0),
        .format=(uint32_t)strtoul(argv[3],NULL,0),.width=(uint32_t)strtoul(argv[4],NULL,0),.height=(uint32_t)strtoul(argv[5],NULL,0),
        .levels=(uint32_t)strtoul(argv[6],NULL,0),.layers=(uint32_t)strtoul(argv[7],NULL,0),.stride=strtoull(argv[8],NULL,0),
        .block_height_log2=(int32_t)strtol(argv[9],NULL,0)};
    struct nn_tex_source_layout source;int e=nn_tex_source_layout(&d,&source);if(e)return e;
    const struct fd_dev_id id={.chip_id=0x06050002};const struct fd_dev_info *info=fd_dev_info_raw(&id);
    const struct fdl_image_params params={.format=format(d.format),.nr_samples=1,.width0=d.width,.height0=d.height,.depth0=1,
        .mip_levels=d.levels,.array_size=d.layers,.tile_mode=TILE6_LINEAR,.ubwc=false};
    struct fdl_layout native={0};if(!info||params.format==PIPE_FORMAT_NONE||!fdl6_layout_image(&native,info,&params,NULL)||native.ubwc||native.size>UINT32_MAX)return ENOTSUP;
    printf("{\"source_bytes\":%"PRIu64",\"source_layer_stride\":%"PRIu64",\"source_alignment\":%"PRIu64",\"source_public_extent_known\":%u,\"gob_bytes\":%u,\"native_bytes\":%"PRIu64",\"native_alignment\":%u,\"linear\":%u,\"levels\":%u,\"layers\":%u,\"block_w\":%u,\"block_h\":%u,\"cpp\":%u,\"level\":[",source.payload_bytes,source.layer_stride,source.alignment,source.public_extent_known,source.gob_bytes,native.size,native.base_align,source.linear,d.levels,d.layers,source.block_w,source.block_h,source.bytes_per_block);
    for(unsigned i=0;i<d.levels;i++){
        const struct nn_tex_source_level *s=source.level+i;uint32_t pitch=fdl_pitch(&native,i),stride=fdl_layer_stride(&native,i);uint64_t offset=fdl_surface_offset(&native,i,0);
        if(fdl_tile_mode(&native,i)!=TILE6_LINEAR||pitch<s->row_bytes||pitch%4||offset>UINT32_MAX||stride<(uint64_t)pitch*s->blocks_y||
           offset+(uint64_t)stride*(d.layers-1)+(uint64_t)pitch*s->blocks_y>native.size)return EINVAL;
        printf("%s{\"source_offset\":%"PRIu64",\"source_bytes\":%"PRIu64",\"row_bytes\":%u,\"rows\":%u,\"source_pitch\":%u,\"block_height_log2\":%u,\"native_offset\":%"PRIu64",\"native_pitch\":%u,\"native_layer_stride\":%u,\"native_bytes\":%u}",i?",":"",s->offset,s->bytes,s->row_bytes,s->blocks_y,s->pitch,s->block_height_log2,offset,pitch,stride,pitch*s->blocks_y);
    }
    puts("]}");return 0;
}
