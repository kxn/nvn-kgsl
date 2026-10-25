> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# True3D volume-shadow owner GPU 资格候选：FINAL FREEZE

本包基于 root 冻结的46e9ce0独立snapshot：14文件最小patch，9新增、5薄修改；保留现生产BC接线，不覆盖旧整文件。`patch --dry-run` 对当时root当前树通过。本包没有改tracked、运行ADB或GPU；下面所有96/36和字节数均是**待root实际执行的预期**。

## 测试对象与严格边界

实际调用生产 `nn_volume_shadow_plan/create/dispatches/resources/destroy`，用正常已编译volume kernel逐mip提交至同一KGSL queue，最后已知Finish后收集完整canonical/native BO。FDL metadata来自root正常runtime库及同源实际HOST FDL reference，不重写descriptor/layout。

18 cases = RGBA8、RGBA16F、BC1 RGB、BC1 RGBA、BC2、BC3 ×三形状：

- 32³/1mip，selected mip0；RGBA16F source flags=IMAGE4。
- 35×19×9/3mips，selected base1/count2；RGBA16F仍IMAGE4。
- 11×17×5/4mips，selected base2/count2，覆盖非整BC边缘、小mip和Z深度递减。

owner始终materialize全部源mip，selected descriptor只改变其可见metadata，不将未选mip省略。每case保留同一owner/immutable IB，两次改原始canonical输入并重放。预期96 Submit、36 Finish、18 owner lifetimes、36全BO raw文件；72 inflight canonical/native destroy拒绝、96实际sequence字检查。

canonical是fresh driver WC BO（含非零512 offset及首尾guard），保持原BO/offset；private native由owner创建。**这不资格普通NVN pool导入、CPU_NO_ACCESS、Draw或texture-unit采样**。测试用CPU初始化canonical原始bytepattern及native全BO a5 guard基线，不把转换结果写入GPU目标，不执行CPU像素转换/解压。native guard初始化字节数明确正数，不声称零CPU目标初始化；生产owner constructor仍不map像素。

Python独立oracle不从source读回推导期望：从固定输入bytepattern公式、独立BD/Z/GOB sector地址和实际FDL native offset/pitch/slice_stride计算每个输出字节；raw包含整个canonical+native allocation，源/neighbor/不同mip空隙/FDL尾/分配尾均零容差。FDL算法仅由原Mesa执行，不另算一套native layout。metadata/code/UBO/commands全字节另在device probe比较；每job的4B自写completion字单独计数，不算readonly。

预期36个raw总6,946,816B，active raw-block字节1,220,688，canonical readonly1,998,848，native未写guard/padding3,727,280，CPU native基线填充4,947,968B；metadata readonly精确计数来自正常kernel code extent+实际common软件分配合同（code按4K、UBO4K、IB8K，每job减自写4B），不是pixel或时钟估算。

任何尝试submit之后、已知Finish之前失败，都保留整个process owner（Device/queue/canonical/private native/code/每mipUBO+IB/readonly快照）。unknown不能由idle0放行。框架保存stage/errno/retained和可用raw，再报失败，最后force-stop+严格pidof确认。transport/执行失败不能作为进程已停止证据。

## 正常工具与依赖

新选项 `build_android_probe.py --volume-shadow-check INPUTS.json`，独立模式/defaultOFF；INPUTS只有schema/kind/kernel_check/runtime_fdl_check四字段，样例 `inputs.json` 正常指向：

- `.local/nvn-native/volume-copy-kernel-integrated-v1/check.json`
- `.local/nvn-native/runtime-fdl-color-formats-integrated-v1/check.json`

正常builder核验两条原record的完整source/config/artifact闭包、当前producer/facade/header/测试身份，再生成实际HOST FDL catalogue与原kernel header；不改旧record或生成shader字节。CMake显式链接真实owner+verified ARM64 FDL archive，不链接图形API runtime。真正D2R host模式拒绝此qualifier。

`volume_shadow_activity.c` 只是已有 `public_copy_activity.c` detached-owner/nonce/publication的薄profile。`volume_shadow_runner.py` 复用 `public_copy_runner.exercise` 的安装身份、fresh namespace、ordinary UID、collector、严格inventory和finally force-stop；shared实现只新增可选shadow profile，原2D/3D默认分支保持。没有新transport、计时器、perf采集、UI/ANW/compositor路径。

## 已实际执行的HOST/NDK/构建证据

- `tree/.local/check-final-v2/check.json`：GCC ASan/UBSan与Clang function各53 HOST failure/owner checks，两个unknown owner保留。返回成功但不执行任何CP的HOST queue因真实completion/metadata/独立raw oracle而被拒；没有CPU执行GPU kernel。
- 39项synthetic oracle/transport检查：active/readonly/guard/selected-mip损坏、缺文件/extra、错误类型/计数/nonce/UID、APK身份、primary Finish35不被缺raw覆盖、pidof传输失败、严格停止、构建互斥等。synthetic raw不是GPU证据。
- API29 ARM64/O2/Wall/Wextra/Werror/`--no-undefined`严格full link：真实KGSL+owner/common compute/volume+正常FDL archive+实际Activity。
- `tree/.local/apk-final-v1/`：从当前rebased源由普通builder构建并签名的完整APK；复用root既有 `.local/nvn-native/probe/probe-debug.keystore`，没有新签名身份。`source-freeze-verification.json` 再次逐一核验build source inventory、compiled inputs、tools和全部artifact，均未改变。没有安装。
- 原目录 `../tree/.local/` 中早期编译/检查/APK记录仅历史；最终以本`rebase-v2`为准，不部署前序包。

root合入后正常复现（须fresh out；root自己串行设备测试）：

```sh
python3 nvn-native/tools/check_volume_shadow_probe.py --root "$PWD" \
 --check "$PWD/.local/nvn-native/parallel/volume-shadow-gpu-candidate/rebase-v2/inputs.json" \
 --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
 --out "$PWD/.local/nvn-native/volume-shadow-integrated-host-v1"

python3 nvn-native/tools/build_android_probe.py \
 --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
 --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
 --keystore "$PWD/.local/nvn-native/probe/probe-debug.keystore" \
 --volume-shadow-check "$PWD/.local/nvn-native/parallel/volume-shadow-gpu-candidate/rebase-v2/inputs.json" \
 --out "$PWD/.local/nvn-native/volume-shadow-integrated-apk-v1"

# 仅root后续执行，本agent未运行：
python3 nvn-native/tools/run_android_probe.py \
 --build "$PWD/.local/nvn-native/volume-shadow-integrated-apk-v1" \
 --serial DEVICE_SERIAL --install \
 --evidence "$PWD/.local/nvn-native/evidence/a650-volume-shadow-integrated-v1.json"
```

即使本批后续GPU通过，也只证明owner的canonical→实际FDL native转换/retire，不证明3D sampled shader、typed runtime/query、ImageLoad/Store、native写后canonical回写、public NVN Draw、真实游戏frame或显示；这些门仍保留。
