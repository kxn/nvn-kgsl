> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Device bootstrap next boundary — read-only research, 2026-10-23

This task changed no tracked source, ran no build/ADB/GPU workload and added no game patches. The adjacent JSON records source hashes and bounded original-module inspection. Parent reports the full 46-file resource set was hash-verified/reused and real original SDK/constructors/nnMain now reaches `nvnDeviceBuilderSetFlags`, exits 78 with errno 38; guest prints BUILD94012 / CONFIGRelease. This report did not independently run that test. The actual flags argument is not yet observed: CONFIGRelease must not substitute for observing w1.

## Public flag contract and smallest defensible implementation

`nvn-native/third_party/nvn/nvn.h:334-346` defines:

| Bit | Public name | Current support evidence |
|---|---|---|
| 0x001 | DEBUG_ENABLE / DEBUG_ENABLE_LEVEL_2 | No matching debug-service semantics qualified |
| 0x002 | DEBUG_SKIP_CALLS_ON_ERROR | Unsupported |
| 0x004 | DEBUG_DRAW_VALIDATION / DEBUG_ENABLE_LEVEL_3 | Unsupported |
| 0x008 | DEFERRED_FINALIZE | Existing EBUSY/reference tracking does not establish deferred-finalize semantics |
| 0x010 | DEBUG_DRAW_VALIDATION_HEAVY / DEBUG_ENABLE_LEVEL_4 | Unsupported |
| 0x020 | DEBUG_ENABLE_LEVEL_0 | Unsupported |
| 0x040 | DEBUG_ENABLE_LEVEL_1 | Unsupported |
| 0x080 | DEFERRED_FIRMWARE_MEMORY_RECLAIM | Unsupported |
| 0x100 | ENABLE_SEPARATE_SAMPLER_TEXTURE_SUPPORT | Independent resource domains are implemented and narrowly GPU-qualified; explicit setter is absent |

Known bits mask is 0x1ff, debug mask 0x77, deferred mask 0x88. `0x115 = 0x100 | 0x001 | 0x004 | 0x010`. It is a known request for additional debug behavior, not an unknown-bit typo. Do not mask it to 0x100, treat current core validation as a debug implementation, alter the game branch, or force its debug field to zero.

A useful minimal **proposed** contract accepts exactly 0 and 0x100. `SetDefaults` resets flags to zero. `SetFlags` replaces the stored value, never ORs or strips it; unsupported values return the existing sticky ENOTSUP diagnostic and leave prior builder state unchanged. Invalid object/type/null returns EINVAL without state changes. Validate the same accepted set in Initialize before opening KGSL, then snapshot flags into the device so builder reuse cannot change an existing device. Negative ints, 0x115, any debug/deferred bit and 0x200 must reject. This is scalar API state over existing real separate resource support, not a new GPU path.

Current core anchors: builder reset at `nvn-native/src/nvn.c:101-106`, SetDefaults at 127, DeviceInitialize at 130-134; currently Initialize only checks builder existence and does not copy flags. Separate pool selection at 906-919, validated distinct combined/texture/sampler arrays at 920-938, scalar and bulk entry points beginning 940. Public setter PFN is `void (*)(NVNdeviceBuilder*, int)` at public header 1122. There is no implemented setter/resolver entry in the inspected source.

The existing recorded A650 evidence `.local/nvn-native/evidence/a650-nvn-texture.json` really qualifies independent resource selection, three wrong-domain/slot rejections, array copying, two late registrations, distinct pools sharing IDs, five submissions, four original shader cases and 65536 channel checks. Its test at `nvn-native/tests/nvn_texture_probe.c:125` uses **SetDefaults**, not SetFlags. Thus default zero already allows the bounded separate path; this is not evidence of an explicit 0x100 call passing. There is no available behavioral specification proving zero must disable those APIs, so a new zero-mode gate is not justified by this enum alone. Supporting zero preserves current qualified behavior; accepting 0x100 acknowledges an already material independent-domain implementation. Neither value implies all NVN formats, stages, debug behavior or all separate-texture shaders are implemented.

Minimal validation before claiming setter support: defaults/reset; 0 -> 0x100 -> 0 transitions; each unsupported known bit plus 0x115, 0x200 and -1; rejected mutation preserving previous flags; invalid object/type/null; initialization snapshot unaffected by later builder mutation. An explicit 0x100 initialization followed by the existing original-texture five-submission qualifier supplies new positive GPU evidence; retain default-zero regressions. These tests are proposed, not performed here.

## Original caller: exact bounded evidence and next observation

Original main NSO SHA256 `55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e`; verified existing raw SHA256 `6843ab6909f96e1558b4b73b0fa807f3873409120cf3e1ef8a86dbebb275ee27`. Existing Capstone was used only for main module-relative interval [0x173cb10, 0x173cc50); no bytes copied into this record, no execution/interpreter.

* 0x173cb54 loads w20 from the caller object's +0x2c scalar. 0x173cb5c/60 prepare 0x100/0x115 and 0x173cb64 chooses 0x115 if w20 != 0, otherwise 0x100.
* SetDefaults uses import slot 0x320e910. SetFlags slot 0x320e918 is loaded at 0x173cb7c; x0 is builder, w1 receives the selected flags at 0x173cb84; actual indirect call is **main+0x173cb88**.
* Initialize actual call is main+0x173cb9c. Nonzero w20 also takes the optional debug-callback call at main+0x173cbd0.
* GetInteger import slot 0x320e948: property 0 call main+0x173cbe8, property 1 call main+0x173cbfc. There is no immediate version compare in this short inspected interval; no claim is made about all later consumers.

Next run should add narrowly typed adapter logging at the existing known setter PFN, **before require_driver/lookup**, so an absent function still records builder pointer and exact `uint32_t` flags hex. Correlate module/build identity and known static callsite main+0x173cb88; do not print an ASLR host address as if it were a guest offset. If a true dynamic guest return PC is available from the existing host gate, report module+offset from its verified module map; do not manufacture that from a C adapter return address. CONFIGRelease is contextual text, not flags evidence. Preserve normal fatal behavior after the diagnostic. No argument rewrites, retries or clearing the sticky error.

For subsequent known calls, log typed Initialize pointers, result and errno; GetInteger device/property/output-pointer before forwarding, then dereference/write result only after a successful call and zero diagnostic. Before forwarding matters because current error handling terminates immediately (`adapters/d2r/host_bridge.c:12-34`); the generic macros at 40-52 do not preserve argument diagnostics. Record property 0/1 even if it becomes the next ENOTSUP boundary. This is API scalar logging, not a timing/sampling system. Avoid a second emulated builder-state database in the adapter; core owns state.

## BuilderGetFlags: ABI still missing

The pinned public header has SetFlags but no `PFNNVNDEVICEBUILDERGETFLAGSPROC`. `dm/gfx/nvn/nvn_protos.h:485,532` explicitly lists the missing prototype. Generated `.local/nvn-native/d2r-host-cli/names/names.inc.h:17` correctly classifies the name as UNKNOWN with generic `(void)` solely for a fatal boundary, not a callable getter ABI. Existing slots metadata gives getter slot 0x320e920, approximate static uses 0. That is not proof no dynamic caller exists.

Do not infer a return type, parameter constness or callable ABI from the API name. Keep this call as UNKNOWN until a primary declaration or equally concrete original-code ABI evidence is obtained. Then add the exact typed declaration and regenerate typed metadata before implementing a real getter roundtrip. Lookup alone must not pretend a getter implementation exists.

## API version queries: numeric source is absent

Public header lines 232-233 establish that DeviceGetInteger properties 0 and 1 mean API major/minor. Public header lines 23-26 say its **partial enum/struct type reconstruction** targets NVN 53.105 and may contain later elements. This is provenance for an ABI basis; it is neither a measured runtime version nor proof the current partial native facade implements all NVN 53.105.

Public primary reconstructed source mirrors:
* https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn.h.html
* https://codeviewer.zeldamods.org/uking/uking/lib/NintendoSDK/include/nvn/nvn.h.html

Both are the same reconstructed source, not independent runtime measurements. Original HLE `dm/gfx/nvn/hle_nvn.c:207-210` uses 53.311 but **explicitly labels this API version unverified**. The broader table comment about measured Tegra properties cannot override that specific exception. No Vulkan implementation was used as evidence.

The bounded original SDK bootstrap inspection (verified NSO SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`, all three segment hashes verified; raw SHA256 in JSON) shows a cached loaded driver function-pointer trampoline at 0x4a3120..0x4a318c. It supplies no numeric DeviceGetInteger version result. No entire external driver was inspected or executed.

Therefore this research establishes **no sourced numeric runtime pair**. Keep property 0/1 as ENOTSUP with caller output unchanged (`src/nvn.c:348-360`) until a deliberate, sourced contract is chosen. Do not silently use 0.0, 53.105 or 53.311 to advance the real game. Do not derive NVN API version from Android, kernel, chip, Mesa or SPIR-V versions. A future project-defined partial-ABI compatibility profile would need an explicit declared identity, pinned version pair and justification; it must not be described as a measured NVIDIA runtime version or full API compliance. This task does not establish that profile.

Observed next milestone should say which original module/call passed, exact flags observed, source/rule/input fingerprints and the next unsupported call. Advancing device bootstrap alone is not an original game Draw/frame milestone.
