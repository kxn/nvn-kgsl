> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Texture texel transfer/cache family — frozen ignored candidate

Five-file minimal patch; no tracked/ADB/Android/GPU execution. It adds known-ABI `TextureWriteTexels`, `TextureWriteTexelsStrided`, `TextureFlushTexels` (346→349 implemented entries in this snapshot), extends existing canonical `TextureReadTexels`/Strided, and corrects this SDK's cache-span behavior. It does not consume BC textures in shaders or alter sampler/pipeline/resolver/Window code.

## Contract evidence

`sdk-facts-sanitized.json` contains original SDK53.314 NSO/segment/method/helper/PLT hashes and facts only. Read/Write non-strided zero both strides and tail-call Strided. The two Strided paths obtain the pool CPU mapping and use per-layer original-layout resolution then CPU layout helpers. Helpers' only BL destination resolves through actual JMPREL to `memcpy`. Thus successful return completes caller-buffer CPU consumption/output in this release; the paths contain no explicit producer queue wait. This does not establish that arbitrary unfinished GPU writes are synchronized.

The implementation deliberately uses the existing normal raw-block GPU kernel instead of a CPU image converter. On successful native return its own private submit+Finish and cache publication are complete. The caller must first complete any producer: all known same-driver duplicate imports / pool / Buffer / SSBO aliases must have no pending/unknown BO refs. This native subset rejects EBUSY without waiting other queues or allocating/submitting. That is an explicit backend admission boundary, not a claim that NVN universally requires an idle resource.

Flush/Invalidate in SDK53.314 ignore region, format and mip and cache full selected layer_stride spans; view layer count0 means the remaining layers. The new helper does not dereference even a non-null unused region. Region-null is accepted. CPU cached pools perform actual KGSL range maintenance; a pool with no CPU cache has no maintenance work, as existing PoolFlush/Invalidate. External allocator pools reject instead of bypassing AHB lock ownership. These exact release observations are not promoted to a cross-version NVN specification.

## Supported native data-transfer subset

Canonical2D /2D_ARRAY, all37 existing raw-block source formats, automatic public-builder BH, mips/array ranges, same-format views, block-aligned origins/true BC edges, nonnegative effective byte strides including odd row/image strides, caller GPU address4alignment. Original pool/offset/GPUVA/storage/layout are preserved. No compressed pixels are decoded and no CPU pixel conversion/copy exists. LINEAR Write, BUFFER/3D/sparse/reinterpret or arbitrary flags/layout are still unsupported; legacy LINEAR Read remains unchanged. This is not a claim of arbitrary NVN implementation completeness.

The source canonical pool and any borrowed caller pool require CPU_CACHED storage and qualified GPU policy. NO_ACCESS/CPU_UNCACHED/unknown external mappings reject for this new data path. A standalone caller pointer may be imported through its complete private-anonymous RW page envelope; descriptor prefix and ceil-DWORD tail stay within that actually pinned envelope. If an existing live pool covers it, the original BO is borrowed instead. Distinct BO/GPUVA never proves nonalias. Same-BO windows are allowed only when full canonical and actual linear DWORD ranges are disjoint; TBI is normalized before CPU-range arithmetic. Destination ranges cannot overwrite live command arenas/chunks/control. This is not a global malloc registry: ordinary caller lifetime and mapping stability remain required.

One fresh private code+64B UBO+normal CS IB and one private KGSL queue execute each transfer; code, UBO, actual source and destination BOs are submitted, same-BO refs deduplicated. No new shader or PM4 algorithm. Metadata/code memcpy is the only CPU copy. CPU publication before masked reads preserves neighbors; after GPU Finish the actual destination is invalidated for CPU visibility. Unknown submit/completion retains all objects, context and imported/pinned pages. Failed BO destruction also retains a recoverable owner. A fault is sticky-fatal at the genuine host boundary: pin retention does **not** give the caller permission to free/reuse a pointer after an unknown completion. No success is reported on failure.

## Validation actually performed

`tree/.local/check-v3/check.json`: GCC ASan/UBSan and Clang function sanitizer each40,002 checks;156 public resource/region cases with two calls each plus standalone caller-import success;313 **nonexecuting mock** submissions. The actual original NVN APIs, builder, registration and common encoder run; the HOST queue only retains refs and never executes CP/image instructions. Every caller/storage byte remains unchanged, explicitly proving no CPU conversion secretly supplied expected data. Uniform arguments / original addresses / owner counters are checked. Negative cases include aliases/busy/RO/NO_ACCESS/GPU policy, import/allocation/cache errors, and unknown submit/Finish/cleanup with finalize blocked by retained refs.

Existing resource/legacy Read tests each49,452 checks also pass both sanitizer configurations. API29 ARM64 complete real backend links strictly with --no-undefined in block-copy ON and OFF, graphics-source-cache1, all actual pipeline/platform modules. Dependencies are libandroid/libdl/libm/libc. This is ELF compile/link, **not** Android runtime or GPU qualification.

## Pending real GPU qualification

`future-gpu-matrix.json` explicitly proposes156 geometry cases /312 public Write/Read calls using the existing checked public Copy case catalogue IDs, independently comparing15,712,256 complete pool bytes and2,449,568 active bytes over two changed-input replays (tolerance0). It covers37 raw formats, BC edges, mip/layer, default/odd strides, independent and same-pool disjoint windows, extra64² BC1 and odd R8. Reuse `public_copy_validation.expected(id,replay)` as an independent inverse-GOB oracle; it does not use source CPU readback or a roundtrip to derive expected output. The authored test can initialize source raw patterns/guards, then must call only public Texel APIs for transfer and capture full actual buffers. No actual texel GPU result is asserted here; prior native148 and publicCopy234 results only establish reusable primitives, not these APIs. Root owns further normal Activity/build/run integration and ADB.

## Reproduce after applying to the final root tree

```
python3 nvn-native/tools/check_texture_texels.py --root "$PWD" --kernel-check .local/nvn-native/block-copy-kernel-integrated-v1/check.json --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 --out .local/nvn-native/texture-texels-host-v1
```

No CMake source-list or compiler/kernel producer changes are needed: runtime is an inc consumed by the existing core. A genuine host with checked block-copy registration can use the new subset; block-copy OFF fails data transfers explicitly while cache APIs remain real. Normal checker consumes existing production public_copy_bundle plus checked kernel instead of manual/generated artifact edits. Any root3D changes require normal rebase/recheck; this frozen snapshot does not claim their qualification.
