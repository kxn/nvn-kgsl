/* SPDX-License-Identifier: MIT
 * Immutable shader-only offline cache. No draw/texture/target template. */
#pragma once
#include <stddef.h>
#include <stdint.h>
enum nn_offline_graphics_stage_kind { NN_OFFLINE_VERTEX=0, NN_OFFLINE_FRAGMENT=1 };
struct nn_offline_cbuf { uint32_t slot,binding; };
/* Exact trusted standalone reflection, not decoded NVN API selectors. Native
 * binding, source handle word and source cbuf are independent namespaces.
 * No runtime JSON parser or archive-expected query value fills this table.
 * Semantic samplerlessness must be established separately by actual NIR use. */
struct nn_offline_texture_binding {
    uint32_t binding,set,type,format,array_length,flags,separate;
    int32_t handle_word,cbuf_slot;
    /* Known only when every linked native pair containing this exact stage
     * supplies identical actual common-NIR resource-use metadata. */
    uint32_t use_known,fetches,size_queries,samples,offsets,other;
};
/* Compiler-native binding and original CB global-pointer descriptor location.
 * slot is translator discovery order, NEVER an NVN API binding selector. */
struct nn_offline_storage_binding {uint32_t slot,binding,cbuf_slot,cbuf_word,flags;};
struct nn_offline_graphics_stage {
    uint32_t stage;
    /* source includes complete code AND compile-time C1 in original order. */
    const unsigned char *source,*control; size_t source_bytes,control_bytes,code_bytes;
    const unsigned char *specialization; /* exactly32B, explicit variant key */
    const unsigned char *spirv; size_t spirv_bytes;
    const struct nn_offline_cbuf *cbufs; size_t cbuf_count;
    /* Full accessor queries + handle domains/reflection. Unresolved sources
     * remain unresolved; native binding is never an NVN pool/API index. */
    const char *reflection_json;
    const struct nn_offline_texture_binding *textures; size_t texture_count;
    const struct nn_offline_storage_binding *storage;size_t storage_count;
};
struct nn_offline_graphics_pair {
    uint32_t chip,vs_index,fs_index;
    const unsigned char *vs_code,*fs_code;size_t vs_bytes,fs_bytes;
    /* Exact standalone-compiler metadata, including register IDs, const
     * allocations/push ranges, sampler prefetch and VPC linkage. No64x64 state. */
    const char *compiler_json;
};
struct nn_offline_graphics_archive {
    const struct nn_offline_graphics_stage *stages;size_t stage_count;
    const struct nn_offline_graphics_pair *pairs;size_t pair_count;
};
/* Caller must supply already validated readable source/control ranges, not raw
 * guest pointers. This helper does not establish CPU access/pool cache policy.
 * Caller explicitly supplies the actual specialization key; no default variant
 * is picked. Returned immutable pointers live as long as the archive. */
int nn_offline_graphics_find_stage(const struct nn_offline_graphics_archive *,uint32_t stage,
    const void *source,size_t source_bytes,const void *control,size_t control_bytes,
    const unsigned char specialization[32],const struct nn_offline_graphics_stage **out);
int nn_offline_graphics_find_pair(const struct nn_offline_graphics_archive *,uint32_t chip,
    const struct nn_offline_graphics_stage *vs,const struct nn_offline_graphics_stage *fs,
    const struct nn_offline_graphics_pair **out);
