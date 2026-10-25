> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Experimental BNTX-writer 8B interoperability

This package is an **explicit, default-OFF experiment**, not a recovered complete NVN packaged-layout specification. Actual D2R v12 supplied `02 00 00 00 07 00 01 00` for BC3 128²/mip8. The public BNTX author serializes the same adjacent words for BH2. That association is correlation; the NVN declaration only guarantees an opaque 8-byte object. No SDK CPU disassembly, Metal/Vulkan implementation, or CPU pixel conversion is used.

`nn_packaged_bntx_height` accepts only two little-endian words `{BH,0x00010007}`, BH0..5, all other first-word bits zero. Unknown marker/upper bits/BH6..7 return ENOTSUP. Source-resource admission additionally requires the existing immutable Generic16Bx2/no-NVIDIA-compression device policy, flags0, ordinary 2D/2D-array, sample0, valid complete fields and stride0. Existing pool eligibility/ranges/refs remain. Explicit BH is passed to the existing source calculator and deep-copied in the initialized source layout. No fallback to implicit/default BH occurs. LINEAR, volume, special compression and unknown profiles remain rejected. Public GetPackagedTextureLayout is not newly implemented or claimed; the qualifier uses actual setter followed by poisoning its caller bytes, then storage getters/initialization/Copy to observe the copied source layout.

## Sources and author checks

- Public independent NVN declaration: open-ead/nnheaders `246c3b482d045934ed004f50c5ada37f3b10b703`, MIT, opaque `unsigned char layout[8]`, not encoding documentation. Existing public report in `../packaged-layout-public-v1`.
- BntxLibrary author: https://github.com/KillzXGaming/BntxLibrary/blob/e5c8d20cb5aed9262b16d7be9e0e48e0f9c43987/BntxLibrary/BntxFile.cs — MIT. Actual original FileWriter.WriteStruct / FileReader.ReadStruct, with original author source unchanged, produced and read six BH patterns. TextureLayout byte offset52 and next word65543. `writer-proof.json` pins actual sources/harness/DLL and output; this proves BNTX writer representation, not a native NVN call.
- Existing author address tool: https://github.com/aboood40091/BNTX-Extractor/blob/2c08e8e55fcc666ba6568a102f36dabd6c584e25/swizzle.py — original `getAddrBlockLinear` invoked on numeric byte coordinates only. No invocation of its CPU pixel swizzle/deswizzle. `author-address-proof.json` compares 1,426,560 addresses /144 subresource shapes, including effectiveBH5, to actual C source metadata. The source and LICENSE stay ignored and are not linked/distributed in the patch.

## Actual validation

Final immutable snapshot: `final-snapshot-v2`.

- `.local/check-v1/check.json`: actual core ON/OFF constructor and real caller-BO/ref/deep-copy checks, both sanitizers; 72 public Copy/CopyCommands admission cases and144 nonexecuting HOST submissions per sanitizer; allocator/cache/submit/finish failure paths and two ambiguous-owner quarantines; independent synthetic complete-readback oracle/transport negatives; API29 strict entire KGSL/Activity link. HOST only, not GPU execution.
- `.local/feature-v1/check.json`: normal CMake Activity-on / CLI-on / Activity-off each compiles all36 original host/HLE/platform sources; ARM64 strict link and no renderer/qualification dependency. Option reconstruction checked for both APK and CLI verification.
- `.local/apk-v1/build-record.json`: real normal build_android_probe APK generation, signatures/source/tool/artifact closure. It was not installed or executed by this agent.
- `default-oracle-regression.json`: original default234 cases/468 synthetic whole-buffer oracle outputs remain byte-identical; aggregate hash recorded.

The GPU collector strictly requires ordinary application UID, exactly72 cases /144 submits+Finish /72 replay /144 files, full raw equality, guards/readonly, successful cleanup and confirmed process stop. Its new JSON fields require `packaged_experiment:true`, `NVN_encoding_normative_proof:false`. This qualifier does not sample/decode BC, run shaders from the game, or prove a frame/display.

## GPU catalogue

Two actual formats: RGBA8(0x25), BC3_UNORM(0x44); BH0..5; three public copy families; two geometries. Ordinary2D 128²/mip8 (evenBH selects mip0, oddBH mip1) plus array3/mip4, width73, height163 RGBA8 /651 BC3 (163 block rows). High shape selects mip0, partial region in layers1..2, shared pool/disjoint spans and odd linear row/image strides. **EffectiveBH5 is actually dispatched in the high cases**, and nonzero-mip selection remains independent. Existing complete source pattern, destination guards, inactive mips/layers, CopyCommands and changed-input replay are retained. No CPU image conversion initializes expected GPU output.

## Normal commands after root promotion

Run from repository root, using the existing venv. `O` must be fresh ignored output. No source or historical record edits.

```
PY=${D2R_REFERENCE_ROOT}/.local/venv/bin/python
NDK=${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358
$PY nvn-native/tools/check_public_packaged_copy.py --root "$PWD" --kernel-check .local/nvn-native/block-copy-kernel-integrated-v1/check.json --ndk "$NDK" --out .local/nvn-native/packaged-copy-host-v1
$PY nvn-native/tools/check_packaged_bntx_feature.py --root "$PWD" --ndk "$NDK" --names .local/nvn-native/activity-image-volume-host-v10/names/names.inc.h --out .local/nvn-native/packaged-production-host-v1
$PY nvn-native/tools/build_android_probe.py --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk --ndk "$NDK" --out .local/nvn-native/packaged-copy-apk-v1 --public-packaged-copy-check .local/nvn-native/block-copy-kernel-integrated-v1/check.json
```

Parent uses the existing normal `run_android_probe.py --build ... --evidence ... --install` transport with its authorized device parameters. Never deploy this agent's isolated snapshot APK as a substitute for the parent's promoted-source build.

Production Activity/CLI normal builders add **`--bntx-packaged-layout-experiment`** to the existing original-ExeFS/full-cache/kernel options. Effective Boolean is recorded, rebuilt identically by ActivityAPK.host_verify and CLI host_verify_argv, and CMake `NVN_BNTX_PACKAGED_LAYOUT_EXPERIMENT` defaultsOFF. Actual compilation closure audits the decoder module and macro1. This does not register a qualification identity or select pair0. No game-specific branch/address/hash is introduced.

## Patch ownership

`core.patch` (4files), `qualification.patch`, `production.patch`; combined `all.patch`. `patch-inputs.json` pins root baseline and final candidate hashes. Shared CMake/build/run have been rebased onto root's runtime-target qualifier and preserve that independent option. Parent should apply minimal hunks, not copy the old entire source tree. Nothing under tracked paths was modified by this agent.
