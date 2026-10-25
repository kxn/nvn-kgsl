> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Native Counter GPU qualification candidate

Ignored only. No ADB, installation, GPU execution or real game execution was performed here. The nine-file patch adds only qualification source/normal CMake/tool wiring on the current tracked Copy+Counter+two-compute-qualifier base; it does not replace core or backend. No Metal/Vulkan implementation was read or used for the Counter runtime, expected values, probe or validation. Inputs are the public NVN ABI, original color shader fixtures, Gallium/Adreno XML and KGSL.

## Apply and normal entry

`minimal-promote.patch` contains CMakeLists.txt, android/probe.c, tools/build_android_probe.py, tools/run_android_probe.py plus new tests/nvn_counter_probe.{c,h}, android/counter_probe_activity.c, tools/counter_validation.py and tools/counter_runner.py. Original production src/counter.c/nvn_counter.inc and Copy metadata hooks are already tracked and are not duplicated. Base hashes and disposable apply verification are in freeze.json.

Build through the existing tool with:

```
python3 nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out <fresh ignored APK directory> \
  --nvn-counter-check .local/nvn-native/a650-modes-graphics-check/check.json
```

Then parent alone may use the normal `run_android_probe.py --serial ... --build ... --evidence <fresh path> --install`. NVN_COUNTER_PROBE defaults OFF. It is mutually exclusive with copy/draw/window/texture/device/host and both compute qualifiers. There are no wrappers, generated PM4 headers, graphics API runtime, SC calls or new performance collectors in this probe. Normal host continues with qualifier OFF.

## Actual workload and pass boundary

One owner worker uses only public NVN methods; two NVN queues invoke actual kgsl.c nn_queue_create, which creates one KGSL draw context each (DRAWCTXT_CREATE). Original VS/FS, identity matrix, fullscreen triangle, RGBA8 single-sample no depth/blend/kill, two independent caller-allocated linear64x64 targets with 4KiB guards. Target bytes are initialized before import but never written by CPU after import. All subsequent target production is NVN Draw. Output counter pools are initialized to0xa5; CPU never invents or writes results after import.

Twelve actual submissions/Finishes, eight recorded/released handles, fifteen saved public counterData observations:

- Reset+one Draw+Report replayed identically twice: each actual4096 samples.
- Draw+Report without another reset across two submissions:8192,12288.
- Reset+Report without Draw:0; next Draw+Report:4096.
- Reset primary Queue, run second-context3 Draws and report12288, then primary Report0 and primary Draw+Report4096. This checks accumulation isolation *between* own submissions.
- Primary Reset/TOP/4096 Draws/BOTTOM/SAMPLES and secondary Reset/TOP/8 Draws/BOTTOM/SAMPLES submitted back-to-back without Finish between. Expected actual counts16777216 and32768. All six actual observations are saved even if a count mismatches. The probe introduces no GPU poll gate or concurrent calls into the single-owner core.

The final context check accepts only `primary.TOP < secondary.TOP < secondary.BOTTOM < primary.BOTTOM`, plus exact independent counts and valid clocks. This is positive evidence that the tested other KGSL context executed inside the primary query interval, rather than merely being queued. It is not a universal proof of all system contexts, firmware states, priorities, suspend or reset behavior. If the hardware did not schedule such an interval, probe returns `stage=context-overlap-not-observed, errno=ENODATA, passed=false` after saving the deterministic values and all completed outputs. There is exactly one fixed bounded stress attempt, no endless pursuit of overlap. A count contamination returns EIO and retains all actual report values for diagnosis. No isolation claim is inferred just from two Queue objects or successful submits.

Actual TOP/BOTTOM report fields must mirror the captured native Adreno clock. All fifteen GetTimestampInNanoseconds outputs are independently checked against exact floor(ticks*625/12). No host epoch, suspend persistence or Maxwell-clock equivalence is claimed. These timestamp reports exercise the API being qualified, not a custom profiler.

Ten fresh RGBA readbacks are independently compared to original shader math: red primary or green secondary, all alpha255, 163840 channels. Target/result guards and all shader/vertex/matrix/original-command bytes are checked after successful Finish. No CPU image producer/copy is used. Original command shadow remains immutable while queue-private parents receive actual query IBs.

## Ownership, worker and collection

All allocations/API objects live in a heap owner. Set pending before Submit, clear only after successful QueueFinish. Any uncertain submit/Finish retains the whole graph and reports resources_retained. Cleanup stops at the first failed release and retains the remaining owner; it never continues through stale pointers. Worker is detached and owns copied paths only: no Activity/window reference crosses to it. Result uses fsync plus renameat2(RENAME_NOREPLACE), with no overwrite/hardlink fallback.

Runner validates the ordinary package UID, installed APK, complete source/tool/artifact closure through the normal entry. It creates a fresh counter-<nonce> namespace, rejects duplicate JSON keys, requires the exact ten regular readback files before/after collection, preserves failed JSON/available known outputs, and always force-stops the dedicated process. Only shell-v2 pidof exit1 with empty stdout/stderr proves absence; transport/execution errors cannot prove stop. Stop failure clears counter_verified. File collection reuses the normal generic copy qualifier collector; transport inventory keys retain its existing copy_* names, while operation/readback names/validator are Counter-specific. This is code reuse, not image copying.

## HOST / strict build evidence

- proof/host-proof.json: current actual tracked core/Counter/Copy with RAM ownership backend; five negative cases in ordinary, ASan, UBSan. An unexecuted GPU result is rejected, and unknown submit/failed Finish keeps owners. No PM4/pixel/count/clock emulation.
- proof/validator.stdout:48 HOST synthetic oracle/type/nonce/no-overlap/count contamination/readback/stop-failure checks, no ADB.
- proof/cli-negatives.json:help and five mutually exclusive CLI cases, before any output/build/device action.
- proof/normal-cmake-v2/proof.json:current tracked actual backend plus nine-file overlay, ARM64 API29 strict CMake full shared link, no wrappers.
- proof/apk-v2/build-record.json:the actual normal build_android_probe CLI generated and signed an APK, with original color bundle/full input closure, all source/tool/artifact hashes and NVN_COUNTER_PROBE=ON. It was not installed/run. This is build proof only; parent must rebuild against its promoted current source inventory for deployment.

Primary hardware recipes and exact pinned URLs/SHA remain in the already-frozen counter-family-candidate/README.md, freeze.json and primary/sources.json: fd6_query.cc occlusion capture/sentinel/accumulation, fd6_emit.h RB_DONE_TS, XML atomic64 always-on register reads, and pipa KGSL opaque per-context SQE records. No claim is made that the latter publicly proves RB internal counter save/restore.
