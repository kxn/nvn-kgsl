> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

> Candidate freeze record. Root subsequently integrated these semantic hunks and reran normal HOST checks; see a650-independent-pacer-pipeline-progress.zh-CN.md. The historical candidate statements below describe its isolated freeze.

# Pinned NVN SDK handle carriers — independent ignored candidate

`minimal-promote.patch` is a five-file candidate. It changes no shader, PM4, GPU descriptor format, resource pool, Window, or fallback-selection code. It has not been applied to production and no ADB/GPU/game run was performed.

## Source evidence and exact encoding

The independent original-SDK audit is in sibling `c1-handle-source-research/`: verified original NSO hash/build ID, RELA-resolved function dictionary entries, full selected function disassembly, original C1 blobs, existing-decoder output, and metadata hashes. Only original SDK release code, the vendored shader frontend and NVIDIA public class fields were read; no Metal/Vulkan implementation is a semantic source.

For the legal descriptor-ID subset, the original SDK returns:

* `GetSeparateTextureHandle` and `GetTexelFetchHandle`: texture ID OR bit32.
* `GetSeparateSamplerHandle`: signed input ID widened, then shifted64-bit by20. Legal IDs256..4095 keep upper32=0, including samplers2048..4095.
* `GetTextureHandle`:32-bit OR of textureID with samplerID<<20, signextend that32-bit result, then OR bit32. Samplers2048..4095 therefore produce upper32=FFFFFFFF; samplers256..2047 produce upper32=1.
* `GetImageHandle`: signed index widened, with no marker. Its existing legal-ID behavior remains unchanged.

The original release getters do not validate negative/reserved/overflow IDs. That is recorded as an observation, not adopted as native policy. Native legal constructor domains stay texture256..1048575 and sampler256..4095. The highest combined legal pair can return UINT64_MAX exactly as the original instructions do; it is not automatically treated as an unbound slot.

The new named `GetTexelFetchHandle` entry is **one CPU constructor entry**. It does not add GPU texture-fetch, image load/store, or query capability.

## Payload versus opaque identity

Native descriptor lookup consumes only the normalized32-bit payload. Bind accepts valid legacy raw32 inputs (compatibility with existing authored fixtures) and the pinned SDK's exact decorated texture carriers, and rejects other upper-bit forms. SeparateSampler accepts only its own unsigned32-bit legal form; it does not share Combined's signextension rule. A valid GetTexelFetchHandle carrier has sampler0 and is accepted for binding; sampled Draw still independently requires a valid registered sampler. Normalization never feeds a marker into a descriptor ID, sampler ID, or GPUVA.

The original graphics **scalar** bindings write only low32 into the driver constant buffer. Original graphics **plural** bindings write both32-bit words at stride8; compute stage5 records a full64-bit deferred value. The source proof retains these distinct paths. In this native candidate, `recording.handle_carriers[domain]` preserves:

* the complete last public uint64 argument by value for each stage/slot;
* whether the last bind was plural;
* the previous plural-written high word and whether it has ever become defined.

Scalar binding changes the last opaque argument and low payload while preserving any earlier plural-written high word. A fresh scalar-only slot keeps the high word explicitly undefined; calloc zero is not asserted as the original driver's initial CB value. Invalid arrays commit neither normalized bindings nor provenance metadata. All three domains and both supported stages remain separate.

This is CPU source preservation only, **not** a full driver-CB2 upload/emulation. Current Draw snapshots retain normalized descriptor IDs and existing resource owners; the marker-qualified sampler fixture proves these IDs reach actual late Draw/materializer behavior. Future general CB2 reads would require an explicit per-Draw snapshot/upload contract including preserved high-word provenance. Unknown sourceCB2 reads and C1 source selection remain strict rejection. No unresolved source is replaced by an archive-first variant.

## Files and tests

Production candidate hunks:

1. `src/nvn.c`: exact constructors, typed payload decoder, atomic binds, opaque/provenance record fields.
2. `src/nvn_entries.inc`: one new GetTexelFetchHandle CPU constructor.
3. `tests/d2r_bridge/handle_carriers.c`: new275check typed actual-core fixture.
4. `tests/d2r_bridge/sampler_core.c`: existing actual Draw fixture now obtains handles from the real constructors and checks the late Draw's texture/sampler IDs stay257/259 (212checks).
5. `tests/d2r_bridge/real_driver.c`: one expected SeparateTexture return updated to the SDK marker carrier. This host-bridge fixture was not independently rerun here; root's full bridge regression remains its verification authority.

`checks-v6/proof.json` is the final proof:

*275carrier/core and212marked-handle Draw/descriptor/materializer checks under GCC ASan, GCC UBSan and Clang function sanitization.
*Typed public64-bit PFNs,2047/2048/4095 sampler boundaries, maxtexture ID, no-pool constructors, dead-device/sticky-error behavior, reserved/negative/overflow IDs, invalid upper words, legal legacy raw32, GetTexelFetch sampler0, unbind, array ownership/atomic failure, stage/domain isolation and scalar/plural high-word provenance.
*Actual ARM64 API29/O2/Wall/Wextra/Werror nonexecuted RAM-fixture shared link, plus real KGSL+platform/core shared link with `--no-undefined`. The actual current `present_pacer.c` dependency is included. No success/fallback stub is used in the real-backend link.

The marked-sampler source compiled for this proof is the byte-identical ignored `sampler_marked_fixture.c`; `ready.json` verifies its hash equals the proposed `tests/d2r_bridge/sampler_core.c`. The original211check test implementation is preserved with only constructor calls and one resolved-ID assertion changed.

The proof uses real current shared source/include dependencies, whose hashes are recorded; candidate nvn.c/entries are selected explicitly. Original/failed intermediate records remain unchanged, including a historical attempt to run the now-obsolete image-handle fixture that expected BindImage to be absent. That stale fixture expectation is unrelated to this candidate and is not reported as a pass.

The ignored `run_checks.py` contains exact commands and refuses to reuse its existing output directory. To reproduce in a fresh output location, preserve the recorded script and copy it with an explicitly fresh OUT (or root may integrate its command list into the normal checker). The current final output is checks-v6. Final canonical source integration and its fresh normal build are root's responsibility.

## Remaining C1 identity boundary

C1 actual nonbindless TXQ/TLDS immediate110/112 are texture-CB word offsets, not pool IDs or native bindings. The original SDK three table bases are proven. **Default texture-CB selection2 has not yet been proven from original SDK initialization**, and T112's original instruction is1D-fetch while the current frontend test specialization says2D. Actual registered resource type/pool/descriptor version must be resolved from real NVN state. The carrier fix does not close or bypass either gap.
