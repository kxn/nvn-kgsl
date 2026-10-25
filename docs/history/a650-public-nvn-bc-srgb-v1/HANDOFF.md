> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# BC1/2/3 sRGB: frozen candidate, no new GPU execution

The candidate adds NVN formats 0x45 RGB_DXT1_SRGB, 0x46 RGBA_DXT1_SRGB, 0x47 RGBA_DXT3_SRGB and 0x48 RGBA_DXT5_SRGB. Total pure FDL image/volume format mapping is 47, not 48. The new FDL test loop contains 4×12=48 image cases and 48 volume cases, in addition to existing cases. Its independent reference invokes original Mesa FDL and compares full returned metadata at zero and actual GPU addresses. True3D sRGB metadata is retained; this is not 3D GPU sampling admission.

Production changes are limited to FDL format mapping, BC raw-block shadow format validation, actual native preparation's BC dispatch range, and feature manifest inventory. No new compiler or sampler encoding is introduced. Original FDL supplies the hardware SRGB descriptor bit and BC1 RGB alpha-ONE swizzle. Source GOB bytes are preserved, converted by the already qualified raw-block GPU kernel, and sampled as actual compressed native BC data. The runtime never decompresses/re-encodes pixels on CPU. Array and mip-chain storage metadata already exists, but current public sampling admission still requires ordinary 2D, one selected mip/layer. This batch does not enable array sampling, LOD ranges, linear filtering, border-table behavior, render-target BC, 3D sampling, or arbitrary compressed formats.

`all.patch` has 20 files. `source-manifest.json` lists baseline and candidate hashes. It contains no nvn.c, nvn.h, texture_source_resource.inc, host_bridge.c, pipeline.c, pipeline.h, or sampler_state.c. CMake/builder and prepare.inc were rebased against current root so volume-shadow options and texture_sampling_identity remain intact. Apply hunks, never overwrite complete files from snapshot. The ignored snapshot is an immutable test source tree, not a replacement production checkout.

## Source and oracle

- Khronos EXT_texture_sRGB, revision 0.8, 2007-01-24: https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_sRGB.txt . RGB uses the piecewise inverse transfer function; alpha is linear. Nearest endpoint controls avoid pre/post-filter conversion ordering ambiguity. This is a color-format semantic source, not an assertion of a complete official NVN specification.
- Khronos Data Format 1.3.1, BC1/2/3 sections 18.1–18.4: https://registry.khronos.org/DataFormat/specs/1.3/dataformat.1.3.html . Author-written controls use endpoint indices only, not interpolated RGB palette values. License permits implementing functionality; no specification text/assets are copied into the patch.
- Khronos EXT_texture_compression_s3tc, version 1.6, 2013-07-15: https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt . BC1 RGB keeps opaque black for selector3 while RGBA uses transparent black.
- Existing pinned Mesa 26.2.5 fd6_format_table.c supplies DXT1_SRGB/DXT1_SRGBA/DXT3_SRGBA/DXT5_SRGBA; fd6_view.cc derives SRGB from actual pipe-format metadata and applies RGB-only alpha ONE. These common FDL paths are source-locked by the normal build. No Vulkan/Metal implementation is read or ported.

Authored RGB565 endpoints are (8,16,16) and (16,32,8), with component denominators (31,63,31). They are neither black nor white; exact sRGB→linear quantized colors are (14,13,58) and (58,56,14). A missing SRGB conversion would produce roughly (66,65,132)/(132,130,66), so cannot pass the existing tolerance. C and Python independently generate expected endpoint colors from these constants; neither reads compressed bytes. The C header writes legal authored block controls only, not a general BC decoder. Alpha controls include BC1 transparency and BC2/3 nontrivial alpha. The original a767 fragment shader's established alpha discard still yields the existing clear color. RGB tolerance remains1, alpha exact. RGB565 8-bit endpoint replication versus exact /31,/63 normalization produces at most1 output code for these selected endpoints (16/31:58 versus59; 32/63:56 versus57); shader arithmetic is also exercised by actual qualification rather than assumed correct. Do not increase tolerance if device results fail.

The qualifier uses the exact established VS967/FSa767 bounded control profiles: 2176 bytes (0x880), C1 empty. It does not call these full0x898 graphics inputs and does not modify/truncate original inputs. No game-address or source-identity special case is added to runtime; the qualifier's explicit stage identity selects its independent mathematical oracle only. No game frame claim follows from it.

## Executed evidence

`ready.json` pins all current records. All are HOST/NDK compile/link only; no Android process, GPU submission, game module or display was executed by this task.

- snapshot-v1/.local/runtime-fdl-srgb-v2/check.json: normal producer, original7 Mesa compilation units, actual Android static/shared library, five exports, HOST ASan/UBSan and function sanitizer reference; 48 new image and48 new volume cases.
- normal-enqueue-v3/check.json: two sanitizer runs, each11132 assertions across8 formats×2 metadata-pool policies,32 mock submits. Actual ordinary public registration/bind/late update/Finalize/missing-resource/owner rollback paths and actual NDK whole-library/fixture link. Mock submissions are not GPU proof.
- snapshot-v1/.local/public-inputs-{srgb,unorm}-v4/check.json: normal input/package verification; both C sanitizer oracles agree with independent Python,393216 channels each. Original shader binaries are reused unchanged through normal verified source archive.
- snapshot-v1/.local/public-host-{srgb,unorm}-v4/check.json:31 strict collector negatives,4 incompatible builder combinations, actual full CMake NDK DSO link. Synthetic collector buffers are not GPU evidence.
- snapshot-v1/.local/production-host-srgb-v1/check.json:25 policy assertions/16 negatives; Activity-on67/CLI-on66/Activity-off66 actual compiled source closures, each includes original36 host sources. No original NSO is executed. No qualifier code is linked in these production hosts.
- default-color-regression/proof.json: default BC=0 color probe NDK machine text exactly identical,2016 bytes, SHA06d237e47576bacc813e17068925657d1ab3a3f73a82bcfc9d1bc544c9313d65.

v1/v2/v3 historical public input/check outputs remain untouched. Later v4 records supersede them after scope wording and source rebasing. Do not use stale records or rewrite their hashes.

## Normal canonical migration (root only)

Use the existing venv; no package installation or private compiler is required. No compiler source changed, so no IR3/Maxwell/kernel recompile is intrinsically required. The FDL facade/archive and public qualification inputs must be freshly generated after promotion because their source fingerprints changed. Existing compiler/pipeline/kernel/bridge records may only be reused if their normal verifier accepts the current source; otherwise regenerate normally into a fresh directory, never amend old records.

```sh
PY=${D2R_REFERENCE_ROOT}/.local/venv/bin/python
NDK=${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358
IR3=.local/nvn-native/ir3-typed-unit-immediates-integrated-v2
FDL=.local/nvn-native/runtime-fdl-srgb-integrated-v1
PIPE=.local/nvn-native/graphics-production-enqueue-pipelines-v1/check.json
BRIDGE=.local/nvn-native/parallel/public-bc-pipeline-gpu-candidate/normal-bridge-v1/check.json
KERNEL=.local/nvn-native/block-copy-kernel-integrated-v1/check.json
INPUTS=.local/nvn-native/public-bc-srgb-inputs-integrated-v1
$PY nvn-native/tools/build_runtime_fdl.py --root . --ir3 "$IR3" --ndk "$NDK" --out "$FDL" --jobs 6
$PY nvn-native/tools/nvn_pipeline_bc_qualification.py --root . --pipeline-check "$PIPE" --native-views "$BRIDGE" --runtime-fdl-check "$FDL/check.json" --kernel-check "$KERNEL" --pair-index 0 --srgb --out "$INPUTS"
$PY nvn-native/tools/check_nvn_pipeline_bc.py --root . --check "$INPUTS/check.json" --ndk "$NDK" --out .local/nvn-native/public-bc-srgb-host-integrated-v1
$PY nvn-native/tools/build_android_probe.py --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk --ndk "$NDK" --out .local/nvn-native/public-bc-srgb-apk-integrated-v1 --nvn-pipeline-bc-check "$INPUTS/check.json"
# Root supplies the actual device serial; this is the ordinary UID Activity path.
$PY nvn-native/tools/run_android_probe.py --serial DEVICE_SERIAL --build .local/nvn-native/public-bc-srgb-apk-integrated-v1 --evidence .local/nvn-native/evidence/a650-public-bc-srgb-integrated-v1.json --install
```

`--srgb` is an explicit qualification producer option; the checked record drives NVN_NVN_PIPELINE_BC_SRGB in normal CMake. Omit it for the existing UNORM qualifier. Default production feature remainsOFF. Production activation retains `--bc-sampled-fdl-check` plus verified graphics/texture bridge/block dependencies; it does not depend on qualifier input files, pair index or oracle.

Expected device contract remains24 Submit/24 Finish,12 handles/12 replay,393216 channels,196608 guard bytes,1703936 read-only bytes,24 late registrations,36 CPU texture finalizes,24 copied caller handle arrays,cleanup0/resources_retainedfalse,24 exact raw files24576B. New result `srgb=true` must match checked record; collector rejects mismatches. Nearest/clamp is an explicit qualification sampler state, not an assertion that all sampler modes are qualified. Root must preserve failing evidence and strict-stop after test. Only a successful ordinary UID device run proves this new public sRGB path.
