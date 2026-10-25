/* SPDX-License-Identifier: MIT */
#include "nvn_native/compute.h"
#include <errno.h>
#include <string.h>
#include "nvn_native/pm4.h"

int nn_cs_encode(nn_device *device, nn_bo *code, const struct nn_cs_image *image,
                 const struct nn_cs_buffer ubos[16], const struct nn_cs_buffer ssbos[16],
                 const uint32_t groups[3], nn_bo **job, size_t *command_bytes)
{
    if(!job||!command_bytes)return EINVAL;
    *job = NULL; *command_bytes = 0;
    if (!device||!code||!ubos||!ssbos||!groups)return EINVAL;
    if (!image || image->schema != 1 || image->chip != nn_device_chip(device) ||
        image->ssbo_slots > 16 || !image->commands || !image->command_words || image->command_words > 1024 ||
        (image->patch_count&&!image->patches) ||
        !image->code_bytes || image->code_bytes > nn_bo_size(code)) return ENOTSUP;
    for (unsigned i = 0; i < 3; ++i)
        if (!groups[i] || !image->local[i] || (uint64_t)groups[i] * image->local[i] > UINT32_MAX) return EINVAL;
    nn_bo *bo = NULL;
    int e = nn_bo_alloc(device, 8192, &bo); if (e) return e;
    uint32_t *words = nn_bo_map(bo);
    if(!words){e=ENOTSUP;goto fail;}
    memset(words, 0, nn_bo_size(bo));
    memcpy(words, image->commands, image->command_words * 4);
    for (unsigned i = 0; i < image->ssbo_slots; ++i) {
        if (!ssbos[i].bytes) continue; // Unused table holes remain null.
        if (nn_cs_buffer_descriptor(image, words + 1024 + i * 16, ssbos[i].address, ssbos[i].bytes)) {
            e = EINVAL; goto fail;
        }
    }
    for (size_t i = 0; i < image->patch_count; ++i) {
        const struct nn_cs_patch *patch = image->patches + i;
        if (patch->word >= image->command_words) { e = EINVAL; goto fail; }
        uint64_t value;
        switch (patch->kind) {
        case NN_CS_CODE: value = nn_bo_address(code); break;
        case NN_CS_DESCRIPTORS: value = nn_bo_address(bo) + 4096; break;
        case NN_CS_SEQUENCE: value = nn_bo_address(bo) + 6144; break;
        case NN_CS_UBO:
            if (patch->index >= 16 || patch->offset > ubos[patch->index].bytes ||
                words[patch->word] > ubos[patch->index].bytes - patch->offset) { e = EINVAL; goto fail; }
            value = ubos[patch->index].address + patch->offset; break;
        case NN_CS_GROUP_COUNT:
        case NN_CS_GLOBAL_SIZE:
            if (patch->index >= 3) { e = EINVAL; goto fail; }
            words[patch->word] = groups[patch->index] *
                (patch->kind == NN_CS_GLOBAL_SIZE ? image->local[patch->index] : 1);
            continue;
        default: e = ENOTSUP; goto fail;
        }
        if (patch->word + 1 >= image->command_words) { e = EINVAL; goto fail; }
        words[patch->word] = value; words[patch->word + 1] = value >> 32;
    }
    *job = bo; *command_bytes = image->command_words * 4; return 0;
fail:
    /* A failed free is still a live owner, even before any GPU submission.
     * Return it explicitly so the caller can retain/retry the cleanup. */
    {int cleanup=nn_bo_destroy(bo);if(cleanup){*job=bo;return cleanup;}}
    return e;
}

int nn_cs_encode_indirect(nn_device *device, nn_bo *code, const struct nn_cs_image *image,
                 const struct nn_cs_buffer ubos[16], const struct nn_cs_buffer ssbos[16],
                 uint64_t indirect_address, nn_bo **job, size_t *command_bytes)
{
    if (!job || !command_bytes) return EINVAL;
    *job=NULL; *command_bytes=0;
    if (!device || !image || !indirect_address || indirect_address%4 ||
        indirect_address>UINT64_MAX-12) return EINVAL;
    /* A650 packet/register contract: adreno_pm4.xml CP_EXEC_CS_INDIRECT and
     * fd6_compute.cc. Other chips need their own register-path qualification. */
    if (image->chip != 0x06050002 || !image->commands || !image->patches ||
        image->command_words>1024) return ENOTSUP;
    for (unsigned axis=0;axis<3;axis++)
        if (!image->local[axis] || image->local[axis]>1024) return EINVAL;
    size_t dispatch=SIZE_MAX, driver=SIZE_MAX;
    uint32_t driver_control=0, seen_groups=0;
    for (size_t pos=0;pos<image->command_words;) {
        uint32_t count=nvn_pm4_packet_words(image->commands[pos]);
        if (!count || count>image->command_words-pos) return EINVAL;
        if (image->commands[pos]==nvn_pm4_packet7(0x33,4)) {
            if (dispatch!=SIZE_MAX || image->commands[pos+1]) return EINVAL;
            dispatch=pos;
        }
        /* A direct driver-parameter load starts with {groups.xyz,work_dim}.
         * Replace only that first vec4 at execution time, preserving all
         * remaining compiler-generated subgroup/local-size constants. */
        if ((image->commands[pos]>>28)==7 && ((image->commands[pos]>>16)&127)==0x34 && count>=8) {
            uint32_t ctl=image->commands[pos+1];
            if (((ctl>>14)&3)==1 && ((ctl>>16)&3)==0 && ((ctl>>18)&15)==13) {
                int has_group=0;
                for (size_t i=0;i<image->patch_count;i++)
                    if (image->patches[i].kind==NN_CS_GROUP_COUNT && image->patches[i].word==pos+4) has_group=1;
                if (has_group) {
                    if (driver!=SIZE_MAX || image->commands[pos+7]!=3 ||
                        image->commands[pos+2] || image->commands[pos+3] ||
                        !((ctl>>22)&1023) || count!=4+((ctl>>22)&1023)*4) return EINVAL;
                    driver=pos; driver_control=(ctl&0x3fffu)|(1u<<14)|(2u<<16)|(13u<<18)|(1u<<22);
                }
            }
        }
        pos+=count;
    }
    if (dispatch==SIZE_MAX || (driver!=SIZE_MAX && driver>=dispatch)) return EINVAL;
    for (size_t i=0;i<image->patch_count;i++) {
        const struct nn_cs_patch *p=image->patches+i;
        if (p->kind!=NN_CS_GROUP_COUNT) continue;
        if (p->index>=3) return EINVAL;
        if (p->word==dispatch+2+p->index) seen_groups|=1u<<p->index;
        else if (driver==SIZE_MAX || p->word!=driver+4+p->index) return ENOTSUP;
    }
    if (seen_groups!=7) return EINVAL;
    nn_bo *bo=NULL; size_t bytes=0; const uint32_t placeholder[3]={1,1,1};
    int e=nn_cs_encode(device,code,image,ubos,ssbos,placeholder,&bo,&bytes);
    if (e) { *job=bo;return e; }
    uint32_t replacement[32]; size_t n=0;
    uint32_t *words=nn_bo_map(bo);
    if(!words){int cleanup=nn_bo_destroy(bo);if(cleanup){*job=bo;return cleanup;}return ENOTSUP;}
    if (driver!=SIZE_MAX) {
        /* Pinned Gallium mem_to_mem/wait_mem_writes recipe. The GPU copies
         * exactly 12 bytes, including on replay or when another shader wrote
         * the counts. The fourth dword is our immutable work_dim, not an
         * out-of-bounds read from the caller's 12-byte argument structure. */
        const uint64_t params=nn_bo_address(bo)+7168;
        ((uint32_t *)((char *)words+7168))[3]=3;
        for (unsigned axis=0;axis<3;axis++) {
            uint64_t dst=params+axis*4,src=indirect_address+axis*4;
            replacement[n++]=nvn_pm4_packet7(0x73,5); replacement[n++]=0;
            replacement[n++]=(uint32_t)dst; replacement[n++]=dst>>32;
            replacement[n++]=(uint32_t)src; replacement[n++]=src>>32;
        }
        replacement[n++]=nvn_pm4_packet7(0x46,1); replacement[n++]=0x31; /* CACHE_INVALIDATE */
        replacement[n++]=nvn_pm4_packet7(0x12,0); /* WAIT_MEM_WRITES */
        replacement[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0);
        replacement[n++]=nvn_pm4_packet7(NVN_CP_WAIT_FOR_ME,0);
        replacement[n++]=nvn_pm4_packet7(0x34,3); replacement[n++]=driver_control;
        replacement[n++]=(uint32_t)params; replacement[n++]=params>>32;
    }
    replacement[n++]=nvn_pm4_packet7(0x41,4); replacement[n++]=0;
    replacement[n++]=(uint32_t)indirect_address; replacement[n++]=indirect_address>>32;
    replacement[n++]=((image->local[0]-1)<<2)|((image->local[1]-1)<<12)|((image->local[2]-1)<<22);
    if (n>32 || image->command_words+n-5>1024) {
        int cleanup=nn_bo_destroy(bo);if(cleanup){*job=bo;return cleanup;}return E2BIG;
    }
    memmove(words+dispatch+n,words+dispatch+5,(image->command_words-dispatch-5)*4);
    memcpy(words+dispatch,replacement,n*4);
    *job=bo; *command_bytes=bytes+(n-5)*4; return 0;
}
