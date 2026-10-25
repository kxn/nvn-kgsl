> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# NVN 源纹理消费者与独立 Android 显示

此批沿用独立 NVN → KGSL/PM4/IR3 路线。NVN 的来源布局、公开地址及对象语义与 Adreno/Android 内部实现分开；没有链接旧 Metal/Vulkan 渲染器。定义来源与边界见 [资料分层](nvn-public-sources.zh-CN.md)、[原 SDK 状态审计](render-defaults-sdk-research.zh-CN.md)。

原池 BO、GPUVA、offset 和 Generic16Bx2 源布局保持不变。Descriptor 注册持有布局快照及原池引用；原 Texture 随后释放不使注册项悬空。每次真正使用及重放从 canonical 数据生成新 Adreno shadow，RT 写入后再写回 canonical，因此同池 Buffer 别名能看到真实 GPU 写入。当前保守地不猜测 dirty 状态、不依赖首次上传；后续优化必须有明确的 CPU/GPU 写入和别名可见性依据。这不是最终性能结论。

消费者 wrapper 保留已编码的 PM4 命令范围及其原有绝对地址，将转换、绘制/清除、写回合并为同一两层 IB 结构。Code、uniform、descriptor、sequence、shadow 和 wrapper 都在提交引用及退休账本中；未知完成状态整组保留。先预分配 owner 元数据，失败回滚也不会丢失 BO。缓存顺序明确覆盖 canonical 读取、计算生产者、graphics 消费和写回。CPU 只处理命令及元数据，不进行运行时 texel 转换。

Window 使用独立 AHB owner、真实 desc/pitch/FD/inode/extent 校验及标准 Acquire/Present。每槽保存 immutable conversion IB，Present 等待原生产队列后，在私有转换队列执行 GOB→AHB、Finish，再交给 SC；不把原 NVN pool 重绑定为 AHB。Finalize 先证明生产者/转换完成，再 SC retirement/destroy，最后释放私有显示及 canonical 引用。无法证明完成的 owner 保留。

Native-only READ/retire 诊断接口用于真机资格，未加入 guest NVN PFN。READ 必须排除 CURRENT/QUEUED 和未知转换；AHB 只做 READ baseline/readback，没有 CPU 图像写入。HOST 的失败声明仅用于 RAM fixture，Android 完整链接始终使用真实平台与 NDK 头。

当前资源消费者仍有明确范围：RGBA8、普通2D、单层单 mip。现有采样/绘制资格使用64×64 RT、2×2 source 的原 VS/FS，不能放宽旧 artifact 来冒充通用 pipeline。独立通用 shader/pipeline 候选另行推进。窗口转换接收实际全尺寸 AHB，与绘制模板尺寸分开。

正式构建接线包括 `--texture-bridge-check`，在真实 DeviceInitialize 成功后注册正常 producer 的 immutable bundle。CLI/Activity 构建记录核验当前 producer、全部输入/产物及实际编译依赖；APK/runner 向下完整转发有效选项。真实宿主强制关闭资格模式，实际源码和 ELF 闭包继续排除旧 renderer/CPU 模拟器。Original NSO 始终从同一正常入口重新生成及 verify，未叠补丁或手改原字节。

正常 HOST 入口为 `nvn-native/tools/check_texture_bridge.py`。整合后完成 words32932、core8465、Window643，各跑 GCC ASan/UBSan 与 Clang function-trap，并检查实际 probe 无 GPU/未知提交失败，真实 API29 backend/probe 严格链接。旧 Window292、render450/97/126、源资源及其旧边界也完成对应整合检查；各 proof 保留当时完整指纹，不把随后变化的源码写回旧记录。

整合时发现并修复两处资格接线问题：旧 RAM backend 的 REAL_WINDOW_SESSION 开关一并屏蔽了 display 失败声明，现拆为独立 guard；新 READ fixture 需要 HOST Android 声明，现放在专用 tests 目录且在 Android 编译时强制报错，不能替代 NDK。最终 Window fixture 按生产 ticket_check 的真实 EINVAL 语义校验 native READ 的 EBUSY 归一化，并增加未知转换禁止 READ 的负例。

两份独立候选 APK 虽然完成过正常编译/签名，但 producer 后续改变，最终复核发现 stale 指纹；旧记录与 APK 原样保留并禁止部署。实际尝试的旧 candidate runner 在 ADB 前正确拒绝 compiled-input 漂移。最终整合重新从正常 producer 生成 bundle、使用已有同一 keystore 构建，没有卸载应用或删除 RomFS。

实际 GPU 和原游戏结果单独记录。首次4case整合资格已经真实提交，但 Finish 返回 EDEADLK35，cleanup EBUSY16，保留资源后严格停止；没有像素输出，不通过。独立 N2 Window 首轮完成一次转换/Finish/SC shown，随后 native READ 否定检查因 HOST/实际错误码差异失败，不能当作全尺寸资格通过。第二个问题已按生产 session 的确切行为修正；第一个仍须独立定位，不能因 HOST 通过而删除缓存操作或忽略 GPU 错误。


## 原 canonical GOB → NVN Window 真机结果

后续 N2/N3 v2 正常构建均已完整通过：每种槽数分别经历首次窗口和 HOME 后新 generation，共 **40 次标准 NVN Window Acquire/Present → GPU GOB 转换/Finish → 独立 AHB → SurfaceControl**。原 caller pool、offset 和 GPUVA 未替换；同一转换 IB 重放，CURRENT 状态禁止 READ，退休后才读取 AHB，最终 WindowFinalize/会话退休及 canonical pool 释放全部完成，普通 APP_UID 进程严格停止。

| 资格 | generation | 实际 presents / 原始文件 | 独立零容差 channel checks | guard bytes | readonly bytes |
|---|---:|---:|---:|---:|---:|
| 2 槽 | 2 | 16 | 298,598,400 | 49,152 | 306,839,552 |
| 3 槽 | 2 | 24 | 447,897,600 | 73,728 | 460,259,328 |
| 合计 | 4 | 40 | 746,496,000 | 122,880 | 767,098,880 |

[2 槽记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-window-source-n2-v2-summary.json`；未随公开仓库提供）和[3 槽记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-window-source-n3-v2-summary.json`；未随公开仓库提供）保留每次 capture 的 device SHA、本地 SHA、generation、槽/case、归档成员及实际 FD/inode/layout。实际 AHB 为2880×1620 RGBA8、pitch11520、pixel offset0、完整 FD/FDL span18,665,472 字节；每次导出的 active RGBA 原始数据为18,662,400 字节。四张[实际屏幕截图](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-window-source-n2-v2-screen-0.png`；未随公开仓库提供）分别保留两种槽数的两个 generation，每张16个独立 RGB 采样点均为零差；图案为原创测试内容。

[无损归档](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-window-source-n2-n3-v2-lossless-readbacks.zip`；未随公开仓库提供）按内容 SHA 去重：40 个实际捕获对应4种不同原始内容，未重新生成或量化像素。[归档核验](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-window-source-v2-archive-proof.json`；未随公开仓库提供）重新核对全部40个本地原始文件与设备记录 SHA，解压4个成员再核对 SHA，并验证4张截图及正常 APK/ELF 哈希；[输入/源码指纹](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-window-source-v2-input-fingerprints.json`；未随公开仓库提供）保留当次正常构建的 source、编译依赖、producer、kernel 和产物闭包。历史捕获的指纹不被当前源码变动反向修改。

这项通过限于原创 caller 在合法 CPU_CACHED|GPU_CACHED|COMPRESSIBLE（0xa4）canonical 池内生成初始原始 GOB 测试图案和重复更新，再由 GPU 转换到独立 AHB；AHB 图像 CPU 写入、运行时 CPU texel 转换、NVN Draw 和 raw Draw 均为0。它没有执行原游戏模块，没有证明实际游戏 CPU_NO_ACCESS0xa1 池的写入消费者、NVIDIA 压缩、任意格式/mip/layer，亦没有消除混合 compute/graphics wrapper 的故障。

两个失败保留为独立记录：[consumer4 v1](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-consumer4-v1-failure-summary.json`；未随公开仓库提供）仍是实际 Submit1/Finish0、EDEADLK35、raw0、cleanup EBUSY16 和资源整组保留；后续缺文件采集错误是次要症状。[Window N2 v1](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-window-source-n2-v1-failure-summary.json`；未随公开仓库提供）完成一次转换/Finish/SC shown1 后在 native READ 错误码否定检查失败，不计入本次40次通过。v2 修正有效 ticket 非 ACQUIRED 状态的 EINVAL→EBUSY 映射并验证完整生命周期，不能用新成功记录覆盖旧失败。
