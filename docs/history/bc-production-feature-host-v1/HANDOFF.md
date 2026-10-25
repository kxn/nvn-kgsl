> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Frozen production BC feature — build wiring only

Apply `all.patch` (8 files) as semantic hunks to the current root after the public BC batch. No runtime/core/shader/qualified-profile modifications are included. It preserves the already promoted public-qualifier CMake/build/run options.

New explicit production builder option: `--bc-sampled-fdl-check PATH`. Default OFF. Requires normal `--graphics-pipeline-check`, `--texture-bridge-check`, and `--block-copy-check`. CLI gains the same verified normal block-copy opt-in already available in Activity. CMake receives only `NVN_D2R_BC_SAMPLED_FDL_LIBRARY`; validates prerequisites and compiles `texture_bc_shadow.c` with `NN_PIPELINE_BC_SAMPLED=1`, linking the exact verified static FDL archive. The archive's complete original-source/config/tool/generated-artifact closure is carried into normal host source fingerprints and `native_bc_sampled` manifest.

This production feature does not use a qualifier header, pair index, test dimension, test-format query answer, oracle, game address or game-logic branch. Existing actual descriptor/sampler/source-query/recipe/native validation is unchanged. Current consumer remains 2D single selected mip/layer BC1 RGB/RGBA, BC2/BC3 UNORM with supported nearest/clamp state; no sRGB/array/mip filtering or 3D consumer is unlocked.

Both normal builders verify the immutable feature manifest on `--verify`, audit actual compiled core/pipeline/BC macros and actual FDL archive link token, and reject unexpected BC activation when OFF. Activity APK host verification, CLI runner, and legacy CLI-host APK wrapper forward both feature and block-copy options. Installed APK verification continues to call the normal original-host verifier. Nothing bypasses existing source/input/tool/artifact checks.

`normal-host-v2/check.json` passed:

- 25 HOST assertions,16 rejects: default OFF, all7 missing-prerequisite combinations, stale normal FDL source, Activity/CLI verification argv ON/OFF, premature builder rejection before any NSO read, missing/duplicated consumer/macro/kernel/archive and qualification-source leakage checks.
- Actual standard CMake NDK API29/AArch64 full original-host link: Activity ON67 source units, CLI ON66, Activity OFF66. Every case compiles all36 original host/HLE/platform source units. ON links verified runtime FDL and BC shadow; OFF has no BC shadow symbol. ELF has no Vulkan/EGL/GLES/libstdc++ dependency. Actual command/source/header/tool fingerprints are in the proof.
- This checker does not rewrite/load/run NSO modules, execute Android, submit GPU work or deploy. It links the existing original host, not a replacement or test stub. The root-provided current names header is an explicit hashed input.

The separate actual ordinary-UID public BC24 GPU pass was performed by root: `evidence/a650-public-bc-sampled-integrated-v1.json`. This build-only candidate does not itself add GPU evidence. Root can now choose the explicit feature in the next normal original-NSO host rebuild and observe the next genuine boundary; no game Draw/Present/frame is asserted here.

## Normal use after promote

Append to the existing normal Activity (or CLI) host command:

```
--bc-sampled-fdl-check ${LEGACY_D2R_WORKTREE}/.local/nvn-native/runtime-fdl-color-formats-integrated-v1/check.json
```

Keep the existing graphics pipeline/texture bridge/block kernel arguments. Use a fresh normal host output (or the existing canonical tool's verified cached-generation path); do not edit an old build record. `build_d2r_activity_apk.py --host-record NEW_RECORD ...` automatically preserves the option on its mandatory host `--verify` call.

Reproducible HOST/NDK checker (after promotion, source matches normal paths):

```sh
ROOT=${LEGACY_D2R_WORKTREE}
PY=${D2R_REFERENCE_ROOT}/.local/venv/bin/python
$PY "$ROOT/nvn-native/tools/check_bc_sampled_feature.py" --root "$ROOT" --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 --fdl-check "$ROOT/.local/nvn-native/runtime-fdl-color-formats-integrated-v1/check.json" --graphics-check "$ROOT/.local/nvn-native/graphics-production-enqueue-pipelines-v1/check.json" --bridge-check "$ROOT/.local/nvn-native/parallel/public-bc-pipeline-gpu-candidate/normal-bridge-v1/check.json" --block-check "$ROOT/.local/nvn-native/block-copy-kernel-integrated-v1/check.json" --names "$ROOT/.local/nvn-native/activity-image-volume-host-v10/names/names.inc.h" --out "$ROOT/.local/nvn-native/bc-production-feature-integrated-v1"
```

Only the ignored checker preparation used a complete immutable `snapshot-v1/nvn-native` and links to existing original `dm`/`scripts`. Canonical root already has those directories. Do not overlay this snapshot onto tracked files; apply only `all.patch`.
