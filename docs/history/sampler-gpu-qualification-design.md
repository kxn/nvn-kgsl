> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Frozen native sampler batch qualification

Patch: `minimal-promote.patch`, SHA256 `888430bfa1c6d518366d923caba7c27e891f95c0633029e327793549d16d0c85`.
Manifest: `freeze.json`, SHA256 `fec71553898a702f542453b0a5b9cfff26cf5858a3f5c6368b01e0aa2b524cb8`.
Exactly 11 files, no production core/IR3 producer/image schema changes. All five modified existing file bases matched tracked source at freeze. Requires already promoted native sampler core eight-file family. The normal NDK snapshot precedes root's latest weak-builder/device-lifetime change; it contains the actual tracked core at snapshot time, never an old substitute sampler core. Root's normal APK builder must build the promoted patch against current full source and verify its fresh complete source inventory.

## Normal integration

- CMake `NVN_SAMPLER_PROBE` defaults OFF. It is an independent Android ARM64 option, requires its sampled API header and rejects all other qualifier/host/submodes. The ordinary D2R host CMake explicitly forces this option OFF.
- `tools/build_android_probe.py --nvn-sampler-check CHECK` uses the existing `texture_api_bundle.py`, including all original input/tool/source/artifact integrity checks. CHECK is the unchanged A650 schema3 `a650-modes-textures-check/check.json`. No shader change, source-byte patch, new graphics schema, or manually supplied native binary.
- Probe Activity copies only its internal-data path and fresh nonce, then starts one detached owner worker. No ANativeActivity/window pointer or UI wait crosses into it. Result uses the already qualified `renameat2(RENAME_NOREPLACE)` publication primitive, with fsync, no hardlink or overwriting fallback.
- Normal runner automatically dispatches `sampler_runner.py`, verifies installed APK identity and ordinary app UID, and uses a fresh `sampler-<32hexnonce>` namespace. Exactly 38 regular files are accepted. Missing/extra/symlink/nonregular entries, duplicate JSON keys, nonce mismatch, changed source/artifact, and transport failures reject the result. Failure preserves collected raw outputs and result metadata before force-stop. Only successful force-stop plus shell-v2 pidof exit1/empty stdout+stderr proves stopped; transport failure cannot.
- No wrapper is linked. Actual public NVN APIs record DrawArrays and produce every pixel; there is no raw submission substitute, CPU rendering, new GPU timing tool, compositor path, or game execution.

## Batch and oracle

All cases use the original unmodified VS/FS and original 2x2 RGBA8 texture/profile, 64x64 linear RGBA8 target. Source matrix is identity, vertex color is one, FS mix=0/gamma=1/contrast=1. RGB texels stay the established `(64,128,192)`, `(192,64,128)`, `(128,192,64)`, `(96,160,224)`; alpha is deliberately 255 for all four so sampling checks do not confuse KIL with alpha filtering. This is a declared test input, not changed shader/game logic.

| Files | Cases |
| --- | --- |
| sampler-00..17 | NEAREST/NEAREST or LINEAR/LINEAR × independent S/T REPEAT, MIRRORED_REPEAT, CLAMP_TO_EDGE = 18 observable states |
| sampler-18..21 | minNEAREST/magLINEAR and minLINEAR/magNEAREST, each magnified and minified |
| sampler-22..27 | all six minfilter enums at the same minifying footprint, single mip |
| sampler-28..31 | four exact normalized texel centers, nearest |
| sampler-32..35 | four constant UVs with exact binary bilinear weights 1/4 or 3/4 |
| sampler-36..37 | exact replay of states 0 and 17 after all intervening registrations |

Magnified UVs cover `[-1.5,2.5]` with binary-exact span4; texel footprint is 1/8 texel/pixel. This intentionally extends beyond the range where MIRROR and EDGE are identical for a two-texel axis. Minification uses offset -1/4/span48, footprint 1.5 texels/pixel; UVs and filter fractions are binary exact, away from the min/mag crossover and nearest sampling ties. R is clamped for a genuine 2D texture, normalized coordinates, LOD bias0, LODmin0, LODmax1000 (hardware saturation follows the shared native encoder).

C and independent Python sampling equations apply repeat/mirror/edge to individual integer taps, then nearest or bilinear interpolation. The independently derived original FS mathematical chain reduces to sample under these declared constants. RGB tolerance is exactly one UNORM8 code for interpolation/output quantization and original LG2/EX2 rounding; alpha is exact255. No tolerance for missing coverage, wrong addressing, wrong filter, stale sampler, stale resource, or read-only/guard modification. The 18 base images must all differ. Replay 0/36 and 17/37 are byte-exact; single-mip 22/24/26 and 23/25/27 must be byte-exact.

The same recorded public NVN handle is replayed for all 38 submissions. Sampler registration occurs after recording, copies its immutable public value, and the temporary Sampler is finalized before enqueue. The Builder is then reset. Each enqueue must consume its current registered slot, producing a private native descriptor; old in-flight job mutation is independently covered by the normal core HOST fixture. Each batch iteration uses successful Finish before input/descriptor changes. Two recorded descriptor-pool identities remain pinned, and existing program/pool/descriptor EBUSY checks, whole-pool read-only shadows, target/source guards and object/control guards are retained from the established NVN texture qualification caller.

NVN object identities and caller control arrays live in one heap owner. A potentially accepted/unfinished submit retains all imported pages and all object identities until process exit. Cleanup stops on its first dependency-release failure and retains the remainder. CPU shadows are separate test-only allocations and never serve as GPU source memory. The oracle never reads target bytes as its expected image.

Expected complete result: 38 NVN submissions, 38 successful Finishes, 38 exact-inventory RGBA files, 622592 channel checks, 155648 shaded pixels, zero discard/outside pixels, 39 sampler object finalizations, 40 sampler registrations, cleanup0, resources_retained=false, stopped=true. No per-state deployment or repeated search for a pass.

## Completed non-device proof

- `proof/host-3/proof.json`: GCC ASan/UBSan C reference vs independent Python, 1245184 byte comparisons; 84 validator/stop negative checks (see exact proof count); wrong wrap/filter, damaged alpha/RGB, replay, mip equivalence, stale/types/counts all rejected.
- `proof/runner-3/proof.json`: 11 synthetic orchestration cases, no actual ADB/subprocess transport. Missing/extra/corrupt readbacks, stale nonce, bool UID, nonordinary UID, initial/final transport or permission failures and stop-execution failure reject; failures cannot claim stopped/verified.
- `proof/normal-2/proof.json`: actual normal CLI with nine mutual-exclusion negatives before output/build, verified original shader bundle, actual CMake ARM64/API29 `-Wall -Wextra -Werror` full shared link with `--no-undefined`, signed APK and AArch64 identity. No `__wrap_` exports.
- Native descriptor/core normal checker was separately handed off as `parallel/sampler-checker-candidate/minimal-promote.patch`: original Mesa XML generator exact matches for all three header inputs, 32400 combinations/64849 assertions, actual core211 assertions, GCC ASan/UBSan and Clang function-trap. That proof verifies anisotropy descriptor encodings 1/2/4/8/16, not anisotropy filtering quality on GPU.

These are HOST/build results only. Hardware filtering/wrap execution awaits root's one A650 batch. Single-mip 2x2 does not qualify multi-mip LOD selection, anisotropic quality, compare/depth/integer formats, arbitrary texture layouts/dimensions, or D2R visuals. No Metal/Vulkan implementation was consulted or used.
