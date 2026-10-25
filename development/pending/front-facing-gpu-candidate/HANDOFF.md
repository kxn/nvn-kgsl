# Authored raw-native FrontFacing whole-family GPU qualifier

FINAL SOURCE FREEZE. Ten owned files only; no Android/GPU execution by this agent. The compiler/FF13/SSBO joint packages remain immutable. This package does not modify compiler metadata, runtime state encoding, original game shaders, SDK code, or public NVN glue.

`minimal-promote.patch` contains three shared hooks relative to the exact immutable `hooks-baseline-v2/` bytes and seven new test/tool files. `source-files.json` records all before/after hashes. ROOT has since added Source/plumbing/SBO probe hooks; apply semantic hunks preserving those changes rather than replacing complete shared files. The actual source/header dependency snapshot was refreshed, with ROOT authorization, in `runtime-refresh-v3.json`; those dependency files are NOT in this patch. This avoids reverse lifecycle/Call/source changes.

## Normal proof

- `tree/.local/qualification-v4/check.json`: actual normal IR3/asmroundtrip for unchanged authored FACE, FACE+FragCoord, FACE+FragDepth programs; actual platform/FDL/D32F layouts and current depth-runtime-front-storage-v2 object. 2,494 input and 61 artifact hashes checked.
- `tree/.local/host-v4/check.json`: GCC ASan/UBSan and Clang function ABI, each complete 192-case/384-draw/640 mock-submit matrix; 22 ambiguous submit/Finish owner cases at eleven boundaries; three constructor faults; default OFF; 24 independent pixel/protocol/inventory negatives; three CLI mixed-mode rejects. Actual API29 ARM64 real KGSL DSO strict link and Activity compilation. Mock RAM pixels are explicitly rejected by the independent image oracle. HOST does not execute PM4.
- `tree/.local/apk-v2/build-record.json`: normal CMake 36-target API29 build, `--no-undefined`, existing ROOT probe-debug.keystore, normal inventory/hash checks. No installation or ADB.
- `runner-local-proof-v4.json`: actual normal entry verifies all recorded sources/tools/compiled inputs/artifacts before dispatch; all subprocess/ADB calls forbidden while dispatch mocked. Synthetic transport failure retains primary stage/errno35 despite raw collector failure; pidof transport error cannot prove stopped. Actual CMake default-OFF and invalid-ON dependency checks. Five profile negatives.
- `freeze-proof.json`: all above input/artifact/source hashes rechecked at final freeze. No source changes after these records.

Earlier qualification-v3/host-v3/apk-v1 remain historical. Their pipeline_source.h dependency changed when ROOT added Sourcecore18; normal runner correctly rejected the stale input before ANY ADB. They must not be deployed or relabelled. v4/APK-v2 use the fresh v2 runtime.

## Scope fixed before GPU

Three actual authored NIR recipes, 64 states each, two VBO winding replays: 2 origins × 2 frontface definitions × 4 cull modes × 2 windings × direct/indexed. Targets are real FDL 64x64 RGBA8 and 73x41 RGBA8; depth recipe uses actual D32F attachment plus GPU clear/readback. Expect 192 jobs-of-state, 384 Draws, 128 depth transfer jobs, 320 total jobs, 640 Submit/Finish, 512 complete files/14,680,064 bytes. Expected independent checks: 5,161,984 color channels, 383,104 D32F pixels, 7,985,664 guard bytes, 36,697,600 read-only bytes, 3,776 in-flight owner-destroy rejects. Device results, files, UID, nonce, exact inventory and all those counters must agree.

The oracle uses signed area of actual post-viewport triangle geometry, including Y scale and requested frontface/cull; no CPU shader interpreter/rasterizer or GPU-fit value. FACE channels and constant D32F .5/clear1 exact. Only authored FragCoord x/y UNORM conversion permits a predeclared one-storage-unit error; B/A remain exact. Full raw file includes prefix/tail/row padding. VBO winding updates occur only after Finish, using the same encoded job. CPU A5 initializes guards/empty targets only; actual graphics pixels are GPU output.

Every program, command, input, target, depth, clear and readback owner is held. Ambiguity is marked BEFORE submission; non-NULL timestamps are required. Unknown completion keeps the entire owner set quarantined until process exit. Fresh ordinary app namespace/nonce, normal existing Activity/collector and strict force-stop/pidof contracts are reused. No new profiler/timer/renderer.

This is authored raw-native qualification, NOT public NVN, original Maxwell, or D2R execution. The original twelve FrontFacing FS inputs remain gated at unknown texture106; no resource/query/pair fallback. Active SampleID remains unsupported.

## Hardware facts

The frozen FF13 source audit is `../front-facing-matrix-extension-candidate/HANDOFF.md` and its proof. Actual pinned Gallium sets FACEREGID, RB/GRAS FACENESS and need_size; IR3 FRONT_FACE is one half scalar in the merged register file, with full-register footprint accounting. The A650 GRAS bit10 reference is the actual pinned Gallium/generated packer behavior; the XML A740 annotation discrepancy is retained explicitly, not a claim that the annotation itself proves A650 behavior. GPU testing is the next qualification step, not already established by HOST.

## Reproduce on merged ROOT sources

Run the normal producer after semantic merge; do not reuse candidate build records as current ROOT deployment records. Commands below assume ROOT worktree current directory and PY/NDK as local existing tools:

```sh
PY=${LOCAL_VALIDATION_ROOT}/.local/venv/bin/python
NDK=${LOCAL_VALIDATION_ROOT}/.local/android-sdk/ndk/28.2.13676358
$PY nvn-native/tools/front_facing_qualification.py --root "$PWD" --compiler .local/nvn-native/ir3-front-storage-integrated-v1 --spirv-tools "$NDK/shader-tools/linux-x86_64" --fdl-check .local/nvn-native/runtime-fdl-front-storage-integrated-v1/check.json --depth-check .local/nvn-native/depth-foundation-front-storage-v1/check.json --a2d-check .local/nvn-native/depth-a2d-front-storage-v1/check.json --runtime-check .local/nvn-native/depth-runtime-front-storage-v2/check.json --out .local/nvn-native/front-facing-qualification-integrated-v1
$PY nvn-native/tools/check_front_facing_probe.py --root "$PWD" --check .local/nvn-native/front-facing-qualification-integrated-v1/check.json --ndk "$NDK" --out .local/nvn-native/front-facing-probe-host-integrated-v1
$PY nvn-native/tools/build_android_probe.py --sdk ${LOCAL_VALIDATION_ROOT}/.local/android-sdk --ndk "$NDK" --native-front-facing-check .local/nvn-native/front-facing-qualification-integrated-v1/check.json --keystore .local/nvn-native/probe/probe-debug.keystore --out .local/nvn-native/front-facing-probe-apk-integrated-v1
```

ROOT alone may then use the existing normal runner `--build ... --serial ... --install --evidence NEW_PATH`; the record dispatches to front_facing_runner. This agent has not invoked it on any device. Source changes to a dependency require fresh normal records, never editing an old record hash.

CMake flag: `NVN_NATIVE_FRONT_FACING_PROBE` defaults OFF. Probe path explicitly enables `NN_PIPELINE_FRONT_FACING=1` with the existing native-depth feature; it does not enable any public NVN source cache. New build-record key: `native_front_facing_check`; independent CLI: `--native-front-facing-check`. All existing modes remain separate.
