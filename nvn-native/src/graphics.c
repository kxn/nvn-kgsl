/* SPDX-License-Identifier: MIT */
#include "nvn_native/graphics.h"
#include <errno.h>
#include <string.h>
#include "nvn_native/pm4.h"

_Static_assert(sizeof(float) == sizeof(uint32_t), "schema3 requires 32-bit IEEE float fields");

static uint32_t float_bits(float value)
{ uint32_t bits; memcpy(&bits, &value, sizeof(bits)); return bits; }

static int overlaps(const void *a, size_t an, const void *b, size_t bn)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
        (x < y + bn && y < x + an);
}

/* Register numbers/bits from pinned a6xx.xml:1422-1436,1543-1549.
 * The emitter uses generated packers; the bounded runtime needs no Mesa
 * runtime or command-word search. Each declared packet is checked directly. */
static int validate_state(const struct nn_gfx_image *image,
                          const struct nn_gfx_draw_state *state)
{
    if (image->schema != 3)
        return state || image->state_patches || image->state_patch_count ? ENOTSUP : 0;
    if (!state || state->window_origin > 1 || state->depth_mode > 1 ||
        !image->state_patches || image->state_patch_count != NN_GFX_STATE_KIND_COUNT ||
        (image->chip != 0x06050002 && image->chip != 0x43030b00)) return ENOTSUP;
    if (overlaps(image->commands, image->command_words * 4u, image->vs_code, image->vs_code_bytes) ||
        overlaps(image->commands, image->command_words * 4u, image->fs_code, image->fs_code_bytes)) return EINVAL;
    uint32_t seen = 0;
    for (unsigned i = 0; i < image->state_patch_count; ++i) {
        const struct nn_gfx_state_patch *p = image->state_patches + i;
        if (p->kind >= NN_GFX_STATE_KIND_COUNT || (seen & (1u << p->kind)) ||
            !p->word || p->word >= image->command_words) return EINVAL;
        const uint32_t registers[NN_GFX_STATE_KIND_COUNT] = {0x8013, 0x8014, 0x8015, 0x8000};
        const uint32_t mask = p->kind == NN_GFX_STATE_CLIP_HALFZ ? 1u << 6 : UINT32_MAX;
        const uint32_t expected[NN_GFX_STATE_KIND_COUNT] = {float_bits(32), 0, float_bits(1),
            (1u << 7) | (1u << 6) | (image->chip == 0x06050002 ? 0u : 1u << 5)};
        const uint32_t value0[NN_GFX_STATE_KIND_COUNT] = {float_bits(32), float_bits(.5f), float_bits(.5f), 0};
        const uint32_t value1[NN_GFX_STATE_KIND_COUNT] = {float_bits(-32), 0, float_bits(1), 1u << 6};
        uint32_t header = 0x40000000u | 1u | (nvn_pm4_odd_parity(1) << 7) |
            (registers[p->kind] << 8) | (nvn_pm4_odd_parity(registers[p->kind]) << 27);
        if (p->reg != registers[p->kind] || p->header != header || p->mask != mask ||
            p->expected != expected[p->kind] || p->value[0] != value0[p->kind] ||
            p->value[1] != value1[p->kind] || image->commands[p->word-1] != header ||
            image->commands[p->word] != p->expected) return EINVAL;
        /* A scalar header/value may not alias either dword of ANY GPUVA patch,
         * including shader code addresses and internal sequence/descriptor BOs. */
        for (unsigned j = 0; j < image->patch_count; ++j) {
            uint32_t word = image->patches[j].word;
            if (word >= image->command_words - 1 ||
                (p->word-1 <= word+1 && word <= p->word)) return EINVAL;
        }
        for (unsigned j = 0; j < i; ++j) {
            uint32_t word = image->state_patches[j].word;
            if (p->word-1 <= word && word-1 <= p->word) return EINVAL;
        }
        seen |= 1u << p->kind;
    }
    return seen == (1u << NN_GFX_STATE_KIND_COUNT)-1 ? 0 : EINVAL;
}

static int encode(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state,
                  const struct nn_gfx_draw *draw,
                  const struct nn_render_state *render,
                  const struct nn_sampler_state *sampler,
                  nn_bo **job, size_t *command_bytes)
{
    if (!job || !command_bytes) return EINVAL;
    *job = NULL; *command_bytes = 0;
    if (!device || !image || !buffers ||
        !((image->schema == 1 && image->profile == NN_GFX_COLOR) ||
          ((image->schema == 2 || image->schema == 3) && (image->profile == NN_GFX_TEXTURE_2D_NORMALIZED ||
                                image->profile == NN_GFX_COLOR))) ||
        image->chip != nn_device_chip(device) || image->gmem_bytes != nn_device_gmem_bytes(device) ||
        image->width != 64 || image->height != 64 ||
        image->target_pitch < image->width * 4 || image->target_pitch > 65536 ||
        (uint64_t)image->target_pitch * image->height > image->target_bytes ||
        image->target_bytes > 1048576 || image->target_layer_bytes > image->target_bytes ||
        image->vertex_bytes != (image->profile == NN_GFX_COLOR ? 72u : 96u) ||
        image->matrix_bytes != 64 || image->zero_bytes > 65536 ||
        !image->commands || !image->command_words || image->command_words > 8192 ||
        !image->patches || !image->patch_count || image->patch_count > image->command_words / 2 ||
        !image->vs_code || !image->fs_code || !image->vs_code_bytes || !image->fs_code_bytes ||
        image->vs_code_bytes > 65536 || image->fs_code_bytes > 65536 ||
        !image->target_alignment || (image->target_alignment & (image->target_alignment - 1)) ||
        !image->code_alignment || (image->code_alignment & (image->code_alignment - 1))) return ENOTSUP;
    int state_error = validate_state(image, state);
    if (state_error) return state_error;
    int textured = image->profile == NN_GFX_TEXTURE_2D_NORMALIZED;
    if (image->sequence_bytes != (image->chip == 0x06050002 ? 4u : 0u)) return ENOTSUP;
    if (textured && (image->vertex_stride != 32 || image->fs_uniform_bytes != 64 ||
        image->texture_width != 2 || image->texture_height != 2 || image->texture_pitch < 8 ||
        (uint64_t)image->texture_pitch * image->texture_height > image->texture_layer_bytes ||
        image->texture_layer_bytes > image->texture_bytes || image->texture_bytes > 1048576 ||
        !image->texture_alignment || (image->texture_alignment & (image->texture_alignment - 1)) ||
        image->texture_slots != 2 || !image->texture_descriptors || image->texture_descriptor_words != 32 ||
        !image->samplers || image->sampler_words != 8 || !image->texture_patches ||
        !image->texture_patch_count || image->texture_patch_count > 16 || !image->texture_address_mask)) return ENOTSUP;
    uint32_t sampler_words[4];
    if(sampler) {
        if(!textured||image->schema!=3||!state||!render)return ENOTSUP;
        const struct nn_sampler_layout layout={image->texture_width,image->texture_height,1,1,2,1,0,0};
        int e=nn_sampler_state_encode(image->chip,sampler,&layout,sampler_words);if(e)return e;
    }
    uint32_t required = (1u << NN_GFX_ZERO) - 1;
    if (textured) required |= (1u << NN_GFX_FS_UNIFORM) | (1u << NN_GFX_TEXTURE);
    const size_t minimum[NN_GFX_PATCH_KIND_COUNT] = {image->vs_code_bytes, image->fs_code_bytes,
        image->target_bytes, image->vertex_bytes, image->matrix_bytes, 0, image->fs_uniform_bytes,
        0, 0, image->texture_bytes};
    const size_t alignment[NN_GFX_PATCH_KIND_COUNT] = {image->code_alignment, image->code_alignment,
        image->target_alignment, 4, 16, 1, 16, 1, 1, image->texture_alignment};
    for (unsigned i = 0; i < NN_GFX_PATCH_KIND_COUNT; ++i) {
        const struct nn_gfx_buffer *b = buffers + i;
        if (!(required & (1u << i))) {
            if (b->bo || b->offset || b->bytes) return EINVAL;
            continue;
        }
        if (!b->bo || b->offset > nn_bo_size(b->bo) ||
            b->bytes < minimum[i] || b->bytes > nn_bo_size(b->bo) - b->offset ||
            nn_bo_address(b->bo) > UINT64_MAX - b->offset ||
            (nn_bo_address(b->bo) + b->offset) % alignment[i]) return EINVAL;
    }
    size_t draw_word=SIZE_MAX, draw_words=0; uint32_t draw_packet[20];
    if (draw) {
        if (image->chip!=0x06050002 || draw->mode>NN_GFX_DRAW_INDIRECT_COUNT ||
            draw->indexed>1 || draw->index_type>2) return ENOTSUP;
        /* NVN enum to hardware primitive. Quads/polygon/adjacency/tessellation
         * need topology/shader stages outside this VS/FS image contract. */
        static const uint32_t primitives[]={9,2,7,3,4,6,5};
        if (draw->primitive>=sizeof(primitives)/sizeof(primitives[0]) || draw->primitive==2) return ENOTSUP;
        if (draw->mode==NN_GFX_DRAW_DIRECT && (!draw->count || !draw->instances)) return EINVAL;
        if (draw->mode==NN_GFX_DRAW_DIRECT && !draw->indexed &&
            ((uint64_t)draw->first+draw->count)*(image->vertex_stride ? image->vertex_stride : 24u)>buffers[NN_GFX_VERTICES].bytes) return EINVAL;
        const struct nn_gfx_buffer *extra[]={&draw->index,&draw->indirect,&draw->indirect_count};
        for (unsigned i=0;i<3;i++) {
            int required=i==0 ? draw->indexed : i==1 ? draw->mode!=NN_GFX_DRAW_DIRECT : draw->mode==NN_GFX_DRAW_INDIRECT_COUNT;
            const struct nn_gfx_buffer *b=extra[i];
            if (!required) { if (b->bo || b->offset || b->bytes) return EINVAL; continue; }
            uint32_t alignment=i==0 ? 1u<<draw->index_type : 4u;
            if (!b->bo || !b->bytes || b->offset>nn_bo_size(b->bo) || b->bytes>nn_bo_size(b->bo)-b->offset ||
                nn_bo_address(b->bo)>UINT64_MAX-b->offset || (nn_bo_address(b->bo)+b->offset)%alignment) return EINVAL;
        }
        uint64_t indices=draw->indexed ? draw->index.bytes>>draw->index_type : 0;
        if (indices>UINT32_MAX || (draw->indexed && draw->mode==NN_GFX_DRAW_DIRECT && draw->count>indices)) return EINVAL;
        if (draw->mode!=NN_GFX_DRAW_DIRECT) {
            uint32_t record_bytes=draw->indexed ? 20u : 16u;
            if (!draw->max_draws || draw->stride<record_bytes || draw->stride%4 ||
                (uint64_t)(draw->max_draws-1)*draw->stride+record_bytes>draw->indirect.bytes ||
                (draw->mode==NN_GFX_DRAW_INDIRECT_COUNT && draw->indirect_count.bytes<4)) return EINVAL;
        }
        for (size_t pos=0;pos<image->command_words;) {
            uint32_t count=nvn_pm4_packet_words(image->commands[pos]);
            if (!count || count>image->command_words-pos) return EINVAL;
            if (image->commands[pos]==nvn_pm4_packet7(0x38,3)) {
                if (draw_word!=SIZE_MAX || image->commands[pos+1]!=0x184 ||
                    image->commands[pos+2]!=1 || image->commands[pos+3]!=3) return EINVAL;
                draw_word=pos;
            }
            pos+=count;
        }
        if (draw_word==SIZE_MAX) return ENOTSUP;
        for (unsigned i=0;i<image->patch_count;i++)
            if (image->patches[i].word<=draw_word+3 && image->patches[i].word+1>=draw_word) return EINVAL;
        for (unsigned i=0;i<image->state_patch_count;i++)
            if (image->state_patches[i].word<=draw_word+3 && image->state_patches[i].word>=draw_word) return EINVAL;
        uint32_t ctl=primitives[draw->primitive]|(1u<<8)|
            (draw->indexed ? draw->index_type<<10 : 2u<<6);
        /* fd6_draw.cc: set the real vertex/base-instance registers immediately
         * before drawing, after the template's fixed defaults. */
        draw_packet[draw_words++]=nvn_pm4_packet4(0xa00e,2);
        draw_packet[draw_words++]=draw->indexed ? (uint32_t)draw->base_vertex : draw->first;
        draw_packet[draw_words++]=draw->base_instance;
        if (draw->mode==NN_GFX_DRAW_DIRECT) {
            draw_packet[draw_words++]=nvn_pm4_packet7(0x38,draw->indexed ? 7 : 3);
            draw_packet[draw_words++]=ctl;
            draw_packet[draw_words++]=draw->instances;
            draw_packet[draw_words++]=draw->count;
            if (draw->indexed) {
                uint64_t address=nn_bo_address(draw->index.bo)+draw->index.offset;
                draw_packet[draw_words++]=0;
                draw_packet[draw_words++]=(uint32_t)address; draw_packet[draw_words++]=address>>32;
                draw_packet[draw_words++]=(uint32_t)indices;
            }
        } else {
            /* CP_DRAW_INDIRECT_MULTI updates VFD starts/count at execution;
             * DST_OFF=0 is valid for this image contract without shader draw
             * driver params. The GPU clamps count to max_draws. */
            int counted=draw->mode==NN_GFX_DRAW_INDIRECT_COUNT;
            uint32_t payload=draw->indexed ? (counted ? 11u : 9u) : (counted ? 8u : 6u);
            draw_packet[draw_words++]=nvn_pm4_packet7(0x2a,payload);
            draw_packet[draw_words++]=ctl;
            draw_packet[draw_words++]=draw->indexed ? (counted ? 7 : 4) : (counted ? 6 : 2);
            draw_packet[draw_words++]=draw->max_draws;
            if (draw->indexed) {
                uint64_t address=nn_bo_address(draw->index.bo)+draw->index.offset;
                draw_packet[draw_words++]=(uint32_t)address; draw_packet[draw_words++]=address>>32;
                draw_packet[draw_words++]=(uint32_t)indices;
            }
            uint64_t address=nn_bo_address(draw->indirect.bo)+draw->indirect.offset;
            draw_packet[draw_words++]=(uint32_t)address; draw_packet[draw_words++]=address>>32;
            if (counted) {
                address=nn_bo_address(draw->indirect_count.bo)+draw->indirect_count.offset;
                draw_packet[draw_words++]=(uint32_t)address; draw_packet[draw_words++]=address>>32;
            }
            draw_packet[draw_words++]=draw->stride;
        }
    }
    uint32_t render_packet[128]; size_t render_words=0;
    if (render) {
        if (!state) return EINVAL;
        const struct nn_render_target_info target={image->chip,state->window_origin,1,0,1};
        int e=nn_render_state_encode(render,&target,render_packet,128,&render_words);
        if (e) return e;
        if (draw_word==SIZE_MAX) {
            for (size_t pos=0;pos<image->command_words;) {
                size_t n=nvn_pm4_packet_words(image->commands[pos]);
                if (!n || n>image->command_words-pos) return EINVAL;
                if (image->commands[pos]==nvn_pm4_packet7(0x38,3)) {
                    if (draw_word!=SIZE_MAX || image->commands[pos+1]!=0x184 ||
                        image->commands[pos+2]!=1 || image->commands[pos+3]!=3) return EINVAL;
                    draw_word=pos;
                }
                pos+=n;
            }
            if (draw_word==SIZE_MAX) return ENOTSUP;
            memcpy(draw_packet,image->commands+draw_word,16);draw_words=4;
        }
    }
    size_t bytes = (image->command_words+render_words+((draw || render) ? draw_words-4 : 0))*4;
    size_t zero_offset = (bytes + 4095) & ~(size_t)4095;
    size_t zero_bytes = image->zero_bytes ? image->zero_bytes : 4096;
    size_t descriptor_offset = (zero_offset + zero_bytes + 4095) & ~(size_t)4095;
    size_t sampler_offset = (descriptor_offset + image->texture_descriptor_words * 4 + 4095) & ~(size_t)4095;
    size_t allocation = textured ? sampler_offset + image->sampler_words * 4 : zero_offset + zero_bytes;
    size_t sequence_offset = (allocation + 4095) & ~(size_t)4095;
    if (image->sequence_bytes) allocation = sequence_offset + image->sequence_bytes;
    nn_bo *bo = NULL;
    int e = nn_bo_alloc(device, allocation, &bo);
    if (e) return e;
    uint32_t *words = nn_bo_map(bo);
    memset(words, 0, nn_bo_size(bo));
    memcpy(words, image->commands, image->command_words*4u);
    if (state) for (unsigned i = 0; i < image->state_patch_count; ++i) {
        const struct nn_gfx_state_patch *p = image->state_patches + i;
        unsigned value = p->kind == NN_GFX_STATE_VIEWPORT_YSCALE ? state->window_origin : state->depth_mode;
        words[p->word] = (words[p->word] & ~p->mask) | (p->value[value] & p->mask);
    }
    uint32_t seen = 0;
    if (textured) {
        uint32_t *descriptors = (uint32_t *)((char *)words + descriptor_offset);
        memcpy(descriptors, image->texture_descriptors, image->texture_descriptor_words * 4);
        if(sampler) {
            /* Resource indices are declared by the sampled image: slot0 hole,
             * actual slot1. Never edit compiler bytes or scan command words. */
            memset((char *)words+sampler_offset,0,image->sampler_words*4);
            memcpy((char *)words+sampler_offset+16,sampler_words,sizeof(sampler_words));
        } else {
        memcpy((char *)words + sampler_offset, image->samplers, image->sampler_words * 4);
        }
        const struct nn_gfx_buffer *texture = buffers + NN_GFX_TEXTURE;
        for (unsigned i = 0; i < image->texture_patch_count; ++i) {
            const struct nn_gfx_patch *p = image->texture_patches + i;
            if (p->kind != NN_GFX_TEXTURE || p->word >= image->texture_descriptor_words - 1 ||
                p->offset > texture->bytes || !p->bytes || p->bytes > texture->bytes - p->offset) {
                e = EINVAL; goto fail;
            }
            uint64_t address = nn_bo_address(texture->bo) + texture->offset;
            if (address > UINT64_MAX - p->offset || address + p->offset > UINT64_MAX - p->bytes ||
                ((address + p->offset) & ~image->texture_address_mask) != 0 ||
                (address + p->offset) % image->texture_alignment) {
                e = EINVAL; goto fail;
            }
            address += p->offset;
            uint64_t original;
            memcpy(&original, descriptors + p->word, sizeof(original));
            address = (original & ~image->texture_address_mask) | address;
            memcpy(descriptors + p->word, &address, sizeof(address));
            seen |= 1u << NN_GFX_TEXTURE;
        }
    }
    for (unsigned i = 0; i < image->patch_count; ++i) {
        const struct nn_gfx_patch *p = image->patches + i;
        if (p->kind >= NN_GFX_PATCH_KIND_COUNT || p->word >= image->command_words - 1) { e = EINVAL; goto fail; }
        size_t available;
        uint64_t address;
        if (p->kind == NN_GFX_ZERO) {
            available = image->zero_bytes;
            address = nn_bo_address(bo) + zero_offset;
        } else if (image->sequence_bytes && p->kind == NN_GFX_SEQUENCE) {
            available = image->sequence_bytes;
            address = nn_bo_address(bo) + sequence_offset;
        } else if (textured && p->kind == NN_GFX_TEXTURE_DESCRIPTORS) {
            available = image->texture_descriptor_words * 4;
            address = nn_bo_address(bo) + descriptor_offset;
        } else if (textured && p->kind == NN_GFX_SAMPLERS) {
            available = image->sampler_words * 4;
            address = nn_bo_address(bo) + sampler_offset;
        } else {
            const struct nn_gfx_buffer *b = buffers + p->kind;
            if (!(required & (1u << p->kind)) || !b->bo) { e = EINVAL; goto fail; }
            available = b->bytes;
            address = nn_bo_address(b->bo) + b->offset;
        }
        if (p->offset > available || !p->bytes || p->bytes > available - p->offset ||
            address > UINT64_MAX - p->offset || address + p->offset > UINT64_MAX - p->bytes) { e = EINVAL; goto fail; }
        address += p->offset;
        words[p->word] = address; words[p->word + 1] = address >> 32;
        seen |= 1u << p->kind;
    }
    if (textured) required |= (1u << NN_GFX_TEXTURE_DESCRIPTORS) | (1u << NN_GFX_SAMPLERS);
    if (image->sequence_bytes) required |= 1u << NN_GFX_SEQUENCE;
    if ((seen & required) != required) { e = EINVAL; goto fail; }
    if (draw || render) {
        memmove(words+draw_word+render_words+draw_words,words+draw_word+4,(image->command_words-draw_word-4)*4u);
        memcpy(words+draw_word,render_packet,render_words*4u);
        memcpy(words+draw_word+render_words,draw_packet,draw_words*4u);
    }
    *job = bo; *command_bytes = bytes; return 0;
fail:
    nn_bo_destroy(bo); return e;
}

int nn_gfx_encode(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  nn_bo **job, size_t *command_bytes)
{ return encode(device, image, buffers, NULL, NULL, NULL, NULL, job, command_bytes); }

int nn_gfx_encode_state(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state,
                  nn_bo **job, size_t *command_bytes)
{
    if (!state) {
        if (job) *job = NULL;
        if (command_bytes) *command_bytes = 0;
        return EINVAL;
    }
    return encode(device, image, buffers, state, NULL, NULL, NULL, job, command_bytes);
}

int nn_gfx_encode_draw(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state, const struct nn_gfx_draw *draw,
                  nn_bo **job, size_t *command_bytes)
{
    if (!state || !draw) { if (job) *job=NULL; if (command_bytes) *command_bytes=0; return EINVAL; }
    return encode(device,image,buffers,state,draw,NULL,NULL,job,command_bytes);
}

int nn_gfx_encode_render(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state, const struct nn_render_state *render,
                  nn_bo **job, size_t *command_bytes)
{
    if (!state || !render) { if (job) *job=NULL; if (command_bytes) *command_bytes=0; return EINVAL; }
    return encode(device,image,buffers,state,NULL,render,NULL,job,command_bytes);
}
int nn_gfx_encode_draw_render(nn_device *device, const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state, const struct nn_gfx_draw *draw,
                  const struct nn_render_state *render, nn_bo **job, size_t *command_bytes)
{
    if (!state || !draw || !render) { if (job) *job=NULL; if (command_bytes) *command_bytes=0; return EINVAL; }
    return encode(device,image,buffers,state,draw,render,NULL,job,command_bytes);
}

int nn_gfx_encode_sampled_render(nn_device *device,const struct nn_gfx_image *image,
                  const struct nn_gfx_buffer buffers[NN_GFX_PATCH_KIND_COUNT],
                  const struct nn_gfx_draw_state *state,const struct nn_gfx_draw *draw,
                  const struct nn_render_state *render,const struct nn_sampler_state *sampler,
                  nn_bo **job,size_t *command_bytes)
{
    if(!state||!render||!sampler){if(job)*job=NULL;if(command_bytes)*command_bytes=0;return EINVAL;}
    return encode(device,image,buffers,state,draw,render,sampler,job,command_bytes);
}
