> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Copy qualifier: source freeze, HOST/NDK only

No ADB, deployment, GPU execution, game execution, or tracked-file mutation was performed by this candidate. This closes the normal tool path for the previously reviewed A650 copy runtime. It does not implement production Window copy or claim D2R display support.

## Promotion

Apply `combined-promote.patch` from the worktree root to the current reviewed source. It contains 17 files and does not touch `src/nvn.c`, KGSL, Window, AHB allocation, shader code, or game binaries. `combined-manifest.json` pins each expected preimage and final file. `copy-only-promote.patch` is an equivalent alias against the metadata-qualified CMake/builder baseline; the two patches currently have the same hash because those baseline files already agree. Both were applied to disposable copies and all 17 final hashes checked.

The runner descriptor audit is taken from current tracked SHA `387fe780127402031ac7ae76a6473af14fa8f87858b98c819f801836393f276b`; its shared-UBO exclusion count remains **8*N**, and tests reject the stale 9*N count. Do not replace the current tracked texture probe with any older copy. Only the 17 explicit patch files are promoted.

Seven runtime/probe files are `src/copy_body.{c,h}`, `src/copy_job.c`, `include/nvn_native/copy_job.h`, `tests/nvn_copy_probe.{c,h}`, and `tests/d2r_bridge/copy_job.c`. Tool closure adds the CMake option, UI worker dispatch, `android/copy_probe_activity.c`, checked `adapters/d2r/copy_bundle.py`, build/run integration, independent `tools/copy_validation.py`, `tests/test_copy_tools.py`, and forced-OFF guards in both production host CMake files.

## Normal build/run contract

Use a fresh normal output after promotion. The already-built ignored candidate APK is evidence of compilation only; rebuild tracked source through the normal entry before deployment.

```
${D2R_REFERENCE_ROOT}/.local/venv/bin/python nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/a650-copy-qualification-apk \
  --nvn-copy-check .local/nvn-native/a650-modes-graphics-check/check.json
```

The copy mode excludes every other operation, host attachment, texture descriptor mode, and submode. `NVN_COPY_PROBE` defaults OFF. Both actual game-host CMake adapters force it OFF, clear its generated header, and force descriptor qualification flags0. Wrappers are linked only into the isolated copy qualifier, never the production host or ordinary driver.

The bundle validates original shader/compiler provenance through the established graphics bundle and regenerates PM4 using the pinned Mesa XML generator. The exact generated PM4 SHA is `8253409f517a2eb03edd5c889b9a654705e69c1d4b17b74d9b0b4edce5804e85`; all 39 compiled inputs are recorded. Matching cached PM4 header/audit may be reused; extra entries, header/audit corruption, and symlinks reject. Reuse the same Python executable for an existing output: its exact generator argv is part of the audit.

Root may subsequently use the usual `run_android_probe.py --serial ... --build ... --evidence ... --install` entry. The runner verifies exact source/tool/compiled-input/artifact closure and installed APK identity before launch. It writes a fresh 32-hex nonce and the worker creates `files/copy-<nonce>` exclusively. Publication uses `syscall(SYS_renameat2, ..., RENAME_NOREPLACE)` after fflush/fsync/fclose; no hardlink or overwriting-rename fallback. Unsupported or denied publication keeps the unique pending result and the runner fails closed. All eight `copy-mode-{0,1}-case-{0,1,2,3}.rgba` files must be regular, exactly16384 bytes, and the namespace must contain exactly those files before and after collection. Symlinks, extras, duplicates, stale nonce/UID/builds, failed cleanup, incorrect counters, changed bytes, or a surviving process reject. Existing local evidence/readbacks are never overwritten. Failure preserves available known regular readbacks and error metadata, then force-stops the isolated package and verifies exit. Empty `pidof` output proves absence only with status1 and empty stderr; transport errors, a missing executable, abnormal status, or empty status0 reject.

## Ownership and qualification limits

The UI only copies its private data path and starts one detached owner worker. It never joins GPU work and no `ANativeActivity*` or window pointer escapes to the worker. GPU uncertainty retains the entire runtime/probe cohort until process exit; the small worker context may be freed only after publishing the completed probe result. The runner stops the dedicated process after evidence collection, including failures.

This qualifier performs standard NVN Draw into an ordinary, original CPU_NO_ACCESS source pool/offset/GPUVA, with original schema3 VS/FS. Qualifier-only wrappers observe the real import and original NVN submission, then forward unchanged. They do not implement production source views. Provenance proves the source is the fresh exclusively-owned private anonymous backing and the independent AHB is a shared dma-buf backing; distinct GPUVA/BO/owner pointers alone are never alias evidence.

Eight cases cover samequeue and afterFinish with red, matrix translation, x-direction clipping, and original RGB interpolation. The source is never read by CPU after import; AHB is READ-locked only. Source color-cache cleaning comes from the verified graphics tail **before CP reads**. CP-copy completion/cache protocol and KGSL Finish precede AHB reading. AHB lock is not used to repair the GPU source-cache boundary. Expected RGBA is recomputed independently from the original shader formula, not copied from source CPU data.

Successful JSON is `operation=nvn_gpu_copy`, schema1, A650 chip, image_schema3, ordinary matching package UID, `completed_cases=8`, `channel_checks=131072`, NVN observations8, copy submissions8/finishes8, intermediate producer finishes4, read locks/unlocks8, owner pin/unpin8, source CPU reads0, source post-import writes0, target CPU writes0, raw draw calls0, and cleanup0/resources_retainedfalse. Exact imported fd/index/dev/inode/offset/range/pitch is reported. Source GPU guard and padded-stride GPU coverage remain explicitly false. No SurfaceControl, Window presentation, arbitrary layouts/strides, full resolution, or performance claim.

## Evidence

`proof/proof.json` gathers:

- canonical runtime HOST28cases/369checks each under normal, ASan, UBSan; intentional unknown-owner cohorts retain until process exit, so only this fixture disables ASan leak detection;
- actual worker HOST12cases/139checks each normal, ASan (leaks enabled), UBSan, including freed original Activity path, actual no-overwrite renameat2 publication, and fail-closed EACCES/ENOSYS/EOPNOTSUPP;
- normal tool12 unittest methods (all subprocesses mocked; no real ADB), 22 CLI exclusion variants, metadata/readback/oracle/runner negatives and preserved legacy8*N audit;
- legacy provenance7 unittest methods;
- normal Android ARM64/API29 `-Wall -Wextra -Werror` / `--no-undefined` full qualifier link and signed APK;
- normal default driver link with no copy/wrapper symbols, both production adapters force-OFF, four CMake rejection cases;
- one positive and five negative real PM4 cache cases, both textual patches applied to throwaway source copies;
- exact normal source, tools, 39 compiled inputs and artifact hashes, checked again after the final same-output rebuild.

Final normal build is `../copy-normal-integration-apk-v1/build-record.json`. This uses candidate source snapshots of the same current core/KGSL; final promotion still requires the normal root build on current tracked source. Reproduction scripts and additional worker tests remain ignored alongside the proof.
