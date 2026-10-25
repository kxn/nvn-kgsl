> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Seven original sampler aliases / public weak Device key

Ignored review candidate; no tracked change, original game execution, GPU or ADB.
No Metal/Vulkan implementation is used. The prior metadata package is unchanged.

Four owned files: `src/nvn.c`, `src/nvn_entries.inc`, new `tests/d2r_bridge/sampler_aliases.c`, and its normal `tools/check_samplers.py` integration. Public declarations and extraction rules need no change: original main re-extraction and `--verify` reproduce all seven already-correct typed wrappers (six VOID pointer APIs and one const-NVNdevice-pointer return), without guessing an ABI. The full SDK dictionary and public header have no `nvnSamplerGetDevice`, so no such entry is created.

The six int/ui border APIs copy exactly16 bytes between caller words and the same immutable CPU border state as the float APIs. No integer/float conversion, clamp, tag or native descriptor policy is added. CPU SamplerInitialize snapshots survive builder/input mutation. Native border-consuming GPU modes remain unsupported exactly as before.

SamplerBuilderGetDevice returns a separately retained public weak key. It does not dereference a freed private Device. SetDefaults preserves this key; starting a different builder lifetime at the same storage resets it. SetDevice still requires a known live native Device (existing backend input policy); invalid Device assignment now leaves the prior binding unchanged. The builder takes no strong Device reference. SamplerInitialize continues to require the private weak device to be live; a successfully initialized Sampler retains its existing strong Device lifetime. This patch does not define arbitrary copied opaque builders or generalize weak-owner generation policy.

SDK53.314 facts (addresses relative to verified uncompressed SDK):

- Builder SetBorderColor/Colori/Colorui: 0x4733d0 / 0x473400 / 0x473430. The same four word loads/stores to builder+0x30..0x3c, with identical bounded method hashes.
- Builder getters: 0x473530 / 0x473560 / 0x473590, corresponding four word copies.
- Sampler getters: 0x4736d0 / 0x473700 / 0x473730, corresponding sampler+0x38..0x44 copies.
- SetDevice0x473330 stores the pointer at builder+0; GetDevice0x473490 loads it. GetDevice never dereferences that Device. Defaults0x473340 leaves this word untouched. These facts establish value/weak getter behavior; the backend's illegal input/unknown-object rejection is defensive policy, not a claim about SDK undefined inputs.

`SDK-facts.json` contains input/build/segment/method hashes and dictionary/RELA identities only. The previously audited disassembly remains ignored and is not copied into this package or patch. No original SDK bytes are delivered.

Validation: normal candidate `check_samplers.py` passed GCC ASan, GCC UBSan, Clang function sanitizer. Each variant retains words64849/core235/carriers275/metadata5990 and adds aliases290 checks. New checks use public typed resolver calls plus exact `_Generic` PFN assertions; cross float/int/ui arbitrary32-bit words include signed extrema, NaN payloads, infinity, signed zero; input/builder mutation; null and unknown-object atomic refusal; sticky-error preservation; DeviceFinalize/Defaults/GetDevice stale-private-pointer lifetime; Initialize refusal after Finalize; and cross-kind stack builder reuse. Actual KGSL/Android API29 closure also strict-linked with O2/Wall/Wextra/Werror/no-undefined, no RAM backend. No new native GPU qualification is claimed.
