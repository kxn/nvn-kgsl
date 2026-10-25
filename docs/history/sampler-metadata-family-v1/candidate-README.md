> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Sampler metadata / native consumption separation

Ignored review candidate. No tracked modifications, ADB, GPU or original game execution.
No Metal/Vulkan renderer source is a behavior source. No new API/PFN/alias is registered.

The six-file patch preserves the latest native sampler encoder including the root's inactive-border fix. Public setters retain all float bits (no invented clamp, rounding or quantization); min/mag, wrap, compare and reduction setters reject only illegal values of their own enum fields, atomically. Complete enum metadata is checked at SamplerInitialize and pool registration. Valid CPU state can be outside the current Adreno sampling profile. Initialize and registration copy the complete semantic snapshot. Native sampler validation/encoding remains unchanged and continues to reject unimplemented border-consuming wraps, comparison, reduction, LOD-snap, unsupported float ranges, anisotropy, and resource classes.

For the currently admitted sampled Draw path, the all-handles QueueSubmit preflight resolves the current sampler slot and runs native state validation before any private parent allocation, cache operation, or submission. This does not require slots to exist at recording time. Registration after recording/re-registration is visible at enqueue. Unsupported state in a later handle cannot partially submit an earlier handle. This patch does not expand generic-pipeline texture/sampler bindings, which remain rejected by that pipeline's existing admission check.

`SDK-facts.json` contains only original input/segment/method hashes, dictionary/RELA locations and human semantic conclusions. The SDK53.314 setters are scalar stores, getter bodies are loads, and Initialize clears 96 bytes then copies 80 bytes from the builder at destination+8 and returns true. The SDK does not perform the new backend's enum rejection: that rejection is our explicit defensive policy outside the legal public domains. Full metadata does not imply any particular native descriptor bits. Pool registration routes both CPU representations to the same native descriptor encoder; that encoder's unsupported capabilities are not inferred here. SDK disassembly and the original bytes stay ignored and are not part of the minimal promote patch.

Validation:

- Normal `check_samplers.py` in the isolated candidate tree: GCC ASan, GCC UBSan, Clang function sanitizer. Each variant: 64,849 native packer checks, 235 original sampler lifetime/Draw checks, 275 carrier checks, 5,990 new metadata/preflight checks. No GPU execution.
- New fixture covers 384 wrap/compare/function/reduction combinations, raw float getter roundtrips including NaN/Inf/-0, source finalization and registration snapshots, invalid-enum atomic rejection, and 367 native/slot rejections before work. Wrapped alloc/cache/submit counters are unchanged for these rejected two-handle batches.
- Existing defaults/empty-recording fixture stays at 103 checks under combined GCC ASan/UBSan and Clang function sanitizer. Nonfinite/reversed LOD now remains CPU metadata; actual native consumption still rejects it.
- Android API29 AArch64 actual KGSL/Android source closure linked with `-O2 -Wall -Wextra -Werror -Wl,--no-undefined`, no RAM backend. `ndk.json` records exact command; `proof.json` hashes source/tool/artifact closure.

Normal sampler checker reproduction (after promote, use the repository's normal path):

```
python3 nvn-native/tools/check_samplers.py --out .local/sampler-metadata-check \
  --image-header .local/nvn-native/a650-modes-textures-check/pair.graphics.h \
  --register-headers .local/nvn-native/ir3-modes/build/src/freedreno/registers \
  --mesa-registers .local/nvn-native/ir3-modes/source/mesa-26.2.5/src/freedreno/registers
```

The checked original shader header is a HOST metadata/packet fixture, not new native shader/GPU qualification. No real game progress is claimed by these tests. The six raw-border int/ui aliases and BuilderGetDevice are intentionally deferred so this patch does not enlarge the public ABI closure.
