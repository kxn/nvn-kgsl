# 待合入工作交接

这些候选保留了迁移当时尚未进入主线的实际工作。基准是旧验证工程提交 `8f052d4`（完整提交号在 manifest 中）；不是四个新的已完成 GPU 能力。`source/` 是候选实际修改的源文件快照，`minimal-promote.patch` 或 `unfrozen-wip.patch` 是精确薄差异。源码 hash 记录在各目录的 `snapshot-files.json`；`manifest.json` 同时保留旧原始路径的占位符与原始文件 hash，公开路径替换后以 snapshot hash 为准。**不要直接用整个 source/ 覆盖主线。** 根据当前源文件逐个合并，保留其他已实现能力，再重新正常构建和验证。

四个候选都没有实际 Android/GPU 执行。HOST 的 mock submit、合成 raw oracle 和 NDK 链接不代表 GPU 或真实游戏进度。所有私有原 Maxwell code/control/C1、离线衍生 shader、生成原始目录、SDK 头、APK/ELF、签名密钥和第三方源码均未迁入。本地检查记录仅提取了安全 scalar/数量/hash 元数据；完整原始记录仍留在验证工程的 ignored 工作区。`${LEGACY_WORKTREE}` 是该本地工作区，`${LOCAL_VALIDATION_ROOT}` 是外部验证工程根目录。这些是描述占位符，不是 shell 默认值。

按顺序接续：

1. formal-storage-feature-candidate：基准精确，8 文件薄接入，先合并/重测 ON/OFF 四种正常宿主构建。
2. public-depth-sampled-gpu-candidate：13 文件，HOST 全144矩阵与正常 APK 已有有效证据。需要审核其中公开 COLOR scissor gate 修复，然后在当前源正常重建，执行真实 GPU144。
3. front-facing-gpu-candidate：10 文件资格，HOST/NDK/APK已通过；共享 build_android_probe hook 不能直接应用，须语义合并。再正常重建并执行真实 GPU640提交/384绘制。
4. graphics-driver-ubo-candidate：29 文件未冻结 WIP，当前 runtime/public HOST 测试没有有效的新结果，先重测并修复，再做正式接入与 GPU 整组资格。

同时修改 shared CMake/build/run hooks 时必须逐项合并。D2R 仍是外部验证工程；原游戏 Draw/QueueSubmit/Present/首帧尚无证据。
