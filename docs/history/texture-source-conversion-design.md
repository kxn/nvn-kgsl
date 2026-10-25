> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Source-payload layout / native GPU conversion candidate

Frozen scope is calculator, authored shader producer and HOST-only tests. No tracked files, ADB, game execution, Vulkan/Metal backend or CPU image conversion was used. GPU conversion is **not executed here**. Normal GPU qualifier integration is owned by graphics_hw_research.

## Input facts and limits

`nn_tex_source_layout` describes a bounded Generic16Bx2 non-sparse 2D/2D-array payload, single sample (`samples=0` in NVN), mips <=15, dimensions <=16384, arrays <=2048 and total addressable payload <=UINT32_MAX. NVN format enums come from pinned `nvn-native/third_party/nvn/nvn.h`; cpp1 R8, cpp2 R16 family5..9, cpp4 RGBA8/BGRA8, cpp8 RGBA16, cpp16 RGBA32, BC/DXT/RGTC/BPTC raw blocks8/16. It does not decode BC.

Original SDK static evidence is separately hashed in `../texture-sdk-layout/manifest.json`. Original SDK475b50 selects linear by flags&0x50;475ca8..475ce8 gives stride*height;475540 selects bit6 alignment128 before bit4 alignment32. Nonlinear4a29d0 uses blockRows+floor(blockRows/2) against16/32/64/128 for global height exponent0..4. Effective mip exponent shrinks whenever mipBlockRows <=8<<(exp-1), including mip0. 4a2a60 shifts original **pixel** dimensions first, max1, then ceil-divides format block dimensions; BC mips must not shift an already rounded block count. Array4a2730 aligns chain to512<<(effectiveMip0TileW+H+D); this subset fixes GOB tileW/D=0. The branch multiplier1 is GOB tile width, independent of BC texel block width4.

A GOB is512 bytes, **not proven public allocation alignment**. Nonlinear results deliberately set `gob_bytes=512`, `alignment=0`, `public_extent_known=0`; `payload_bytes` is a bounded source-payload span, never automatically `GetStorageSize`. Nonlinear public GetStorageAlignment/GetStorageSize still require kind/device/page policy. Linear single-level/single-layer uncompressed results have known stride*height and32/128 alignment. DISPLAY/IMAGE bits only permit computing eligible source geometry; no display/image GPU support follows. COMPRESSIBLE/SPARSE/video/minimal/depth/MSAA/3D/packaged-layout decoding reject explicitly. DISPLAY array/mips/linear combinations reject in this subset.

Independent primary address source: ChromiumOS minigbm tegra.c commit74e4893783bb85d18470b847bd77ba3be57fd632. HOST test uses its inverse dword sector order; runtime/kernel uses the direct Generic16Bx2 equation. Full BSD attribution is in MINIGBM-LICENSE. Public deko3d is corroborating layout evidence, not an exact defaultchooser substitute: issue10 illustrates its historical height8 mismatch. No emulator/runtime CPU interpreter exists in this candidate. Downloaded public files and original SDK asm stay ignored and are not in candidate.patch.

## Kernel contract

`gob_copy` direction0 sourceGOB->nativeLINEAR, direction1 nativeLINEAR->destinationGOB. `linear_copy` handles independent source/native row pitches. Both use set0 SSBO0 source, SSBO1 destination and UBO2; **allocate/bind64 UBO bytes** although argument structs occupy32/24 bytes, zero remaining bytes. Native images have local_size8x8x1, no shared/private memory, no sampler, no preamble/immediate/constant table. Groups are `(ceil(ceil(row_bytes/4)/8),ceil(block_rows/8),1)`.

Each invocation owns four consecutive bytes at x multiple4. Generic16Bx2 preserves x low4bits and row segments are16-byte aligned, so active invocations own distinct destination DWORDs. For a final partial DWORD, n1..3 produces an RMW mask preserving padding. Source rounded DWORD reads require complete padded SSBO windows: GOB per-level512-multiple allocation, linear source stride32/128 and native pitch divisible4. Caller must validate each full window and4-aligned bases, all additions <=UINT32_MAX, source/destination disjoint and no overlapping feedback. Helpers produce base0 relative windows; each mip/layer is dispatched separately using source layer_stride+level.offset and independent native FDL offsets. No crop or in-place conversion.

**Native pitch/size must come from actual FDL.** This candidate accepts validated pitch/extent arguments and does not invent FDL allocation rules. Use `fdl6_layout` with real format, TILE6_LINEAR, ubwc=false, then actual fdl_pitch, surface offsets and layer strides. Shader copies raw format blocks so BC block row counts, not pixel heights, drive dispatch. Current sampler/image profile limitations remain separate (no claim arbitrary mip/array sampling).

## Required production integration (not implemented by calculator)

Original NVN pool bytes/GPUVA remain canonical and retain Buffer alias semantics. Before a texture read, GPU untile/copy canonical source into a per-enqueue native FDL shadow. After render/image writes, GPU retile/copy shadow back before later buffer/copy consumers; omitting writeback would break aliases. Source, shadow, kernel code, UBO, private descriptor/command jobs must remain immutable and retained to Finish; unknown completion quarantines them. Existing NOACCESS/UNQUALIFIED consumer gates apply before converter use; CPU descriptor role never exempts texel data.

Use existing ordering helpers for CCU/UCHE producer flush/waits and TP invalidate before sampling compute-written shadow. Retile canonical writes need correct barriers before CP/buffer consumers and normal KGSL cached/coherent CPU policy after Finish. This does not make cached CPU pages coherent. Per-enqueue conversion is the first conservative implementation; no invented dirty tracking/timers. Arbitrary alias feedback, compression metadata, UBWC, sparse and multi-sample remain unsupported. Do not wire this kernel into the old fixed64x64/2x2 draw profile and call that generic support.

## Reproduction and evidence

From worktree root, compile kernels using existing toolchain (fresh --out required):

```sh
python3 .local/nvn-native/parallel/texture-source-layout/tools/compile_texture_copy.py --root . --ir3 .local/nvn-native/ir3-modes --spirv-tools ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358/shader-tools/linux-x86_64 --out .local/nvn-native/texture-copy-fresh
```

`kernels/check.json` records source/tool/compiler closure (1713 inputs), actual commands, spirv-val, IR3 asmroundtrip and native image artifacts. This GLSL compiler SPIR-V environment name is not use of a Vulkan runtime/driver. Native code sizes are768 and384 bytes. Existing `nopreamble` compiler option gives the supported native image ABI; it is not a binary patch.

`layout-proof.json` and `native-proof.json` contain exact compile/run argv and results; `replay_host.py` reruns those argv in this same ignored candidate. Clang function-trap and GCC ASan/UBSan/leak tests pass219521 address checks and8198 native encode/ownership checks respectively. ARM64 API29 O2/Werror shared link is static qualification only. Native encoding test uses the real nn_cs_encode with HOST memory backend, checks both hardware buffer descriptors and verifies no data payload was modified on CPU; it does not execute shaders.

Preserved honest failures: first-compile.json rejected load_base_global_invocation_id; authored shader was rewritten using WorkGroupID*size+LocalID. second-compile.json native artifact rejected early preamble; third-compile.json and final kernels use existing nopreamble. No original game shader changed.

GPU next matrix: cpp1/2/4/8/16, BC raw8/16; height8 (defaultexp0), oddwidth partialDWORD, height11/17/128, mip/array offsets, independent native FDL pitch, nonzero4aligned bases, forward/reverse independent expected bytes, guards/readonly source and replay with changed source. A roundtrip alone can hide mutually inverse wrong maps; independently check each direction. GPU conversion, raw BC bytes, sampling qualification, public NVN flags/getters and actual game frame are distinct milestones.
