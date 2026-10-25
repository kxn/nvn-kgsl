#!/usr/bin/env python3
"""Read-only typed compiler facts from pinned common IR3 lowering.

Changes no NIR, IR3, allocation or machine-code decisions. The exact lowerings
that allocate constants expose the used DWORDs and remaining bindful UBOs;
textual NIR/disassembly is diagnostic only, never parsed into the contract.
"""
PROFILE = 'mesa-26.2.5-ir3-runtime-constants-facts-v1'
FILES = ('src/freedreno/ir3/ir3_shader.h',
         'src/freedreno/ir3/ir3_nir.c',
         'src/freedreno/ir3/ir3_nir_analyze_ubo_ranges.c')

def replace(data, old, new):
    old, new = old.encode(), new.encode()
    if data.count(old) != 1:
        raise ValueError('pinned common IR3 runtime-fact hook changed')
    return data.replace(old, new)

def patch(name, data):
    if name == FILES[0]:
        return replace(data, '   unsigned num_driver_params; /* scalar */', '''   unsigned num_driver_params; /* scalar */

   /* Independent standalone consumer facts, not compiler policy. Masks come
    * from actual const-state NIR passes; unknown inputs are separately marked.
    * These fields never affect compilation or driver code generation. */
   uint32_t nvn_driver_word_mask;
   bool nvn_driver_unknown;
   uint32_t nvn_remaining_ubo_mask;
   bool nvn_remaining_ubo_unknown;''')
    if name == FILES[1]:
        return replace(data, '''   const_state->num_driver_params =
      ir3_nir_scan_driver_consts(compiler, nir, &const_state->image_dims);''', '''   const_state->num_driver_params =
      ir3_nir_scan_driver_consts(compiler, nir, &const_state->image_dims);

   /* Observe the same actual driver-param intrinsics used by the allocator.
    * Four-DWORD aligned allocation alone does not identify consumed values. */
   const_state->nvn_driver_word_mask = 0;
   const_state->nvn_driver_unknown = false;
   nir_foreach_function (f, nir) {
      if (!f->impl) continue;
      nir_foreach_block (b, f->impl) {
         nir_foreach_instr (i, b) {
            if (i->type != nir_instr_type_intrinsic) continue;
            nir_intrinsic_instr *intr = nir_instr_as_intrinsic(i);
            struct driver_param_info p;
            if (!ir3_get_driver_param_info(nir, intr, &p)) continue;
            unsigned count = p.extra_size + nir_intrinsic_dest_components(intr);
            if (p.offset >= 32 || count > 32 - p.offset) {
               const_state->nvn_driver_unknown = true;
            } else {
               for (unsigned k = 0; k < count; k++)
                  const_state->nvn_driver_word_mask |= 1u << (p.offset + k);
            }
         }
      }
   }''')
    if name == FILES[2]:
        return replace(data, '''      ir3_const_state_mut(v)->num_ubos = num_ubos;''', '''      ir3_const_state_mut(v)->num_ubos = num_ubos;

      /* Observe residual loads AFTER the real push-range lowering. This is
       * native compiler binding space, not a guessed application UBO table. */
      struct ir3_const_state *facts = ir3_const_state_mut(v);
      facts->nvn_remaining_ubo_mask = 0;
      facts->nvn_remaining_ubo_unknown = false;
      nir_foreach_function (f, nir) {
         if (!f->impl) continue;
         nir_foreach_block (b, f->impl) {
            nir_foreach_instr (i, b) {
               if (!instr_is_load_ubo(i)) continue;
               struct ir3_ubo_info ubo = {};
               if (!get_ubo_info(nir_instr_as_intrinsic(i), &ubo) ||
                   ubo.bindless || ubo.global || ubo.block >= 16) {
                  facts->nvn_remaining_ubo_unknown = true;
               } else {
                  facts->nvn_remaining_ubo_mask |= 1u << ubo.block;
               }
            }
         }
      }''')
    raise ValueError('unknown common IR3 overlay path')
