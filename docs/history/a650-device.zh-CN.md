> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650 替换测试机与 NVN 复验

后续增量：纹理采样、NVN 描述符池/绑定与更新消费已取得真机结果，AHB 像素 backing 已鉴别并通过 KGSL 直接写入与原始 shader 绘制，见 [纹理实施进度](texture-progress.zh-CN.md)。本文保留换机当时的验证范围。

用户指定的新测试机 `DEVICE_SERIAL` 可以继续承担独立 native KGSL/NVN 后端验证。已通过普通应用权限下的命令提交、NVN 内存池、四个原始计算 shader，以及经 NVN 图形调用的原始 VS/FS 离屏绘制。旧 A735 手机不再用于本任务的测试。开发继续在独立 worktree 的 `codex/nvn-turnip-study-20261023` 分支进行。

## 设备与边界

实测型号 `23043RP34C`，产品 pipa，SoC SM8250；Android 14 / SDK 34，内核 `4.19.157-perf-g240e1d0a5f52`。当前无线 ADB 为 `DEVICE_SERIAL`，端口可能变化，连接前可用 `adb mdns services` 查询。

普通应用 UID `10234` 的 KGSL 查询返回精确 chip ID `0x06050002`、GMEM `1179648` 字节（1.125 MiB）、页大小 4096。sysfs 名称 `Adreno650v3` 不能替代精确 chip ID；没有按这个字符串猜 patch revision。原始 `gpu_id=0`、`mmu_enabled=256` 保留原值，不将后者直接解释成布尔值。只读模式 `--device-inspect` 不创建 context、不分配 BO、不提交 GPU 命令。

本次只测试独立 KGSL 用户态驱动，没有执行真实 ARM64 游戏模块，没有链接现有游戏渲染实现，没有把探针结果计作 D2R 游戏运行。CPU 执行的是新增 ARM64 调用者；GPU 执行的是原始 NSO 提取的 Maxwell shader 经严格前端、NIR/IR3 翻译后的机器码。原始 shader 输入与派生产物只存于忽略目录，提交的证据只有身份、元数据、结果和哈希。

## 已通过项目

| 项目 | 新机结果 | 证据 |
| --- | --- | --- |
| 普通 UID KGSL 查询 | 精确芯片、页大小、GMEM，零 GPU 提交 | [a650-inspect.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-inspect.json`；未随公开仓库提供） |
| CP 内存写、提交、等待、读回、清理 | accepted，timestamp 1，值一致，cleanup 0 | [a650-cp.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-cp.json`；未随公开仓库提供） |
| NVN ClearBuffer / 导入内存池 | 重叠视图、对象边界、命令预算、在途销毁拒绝通过 | [a650-nvn-resources.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-nvn-resources.json`；未随公开仓库提供） |
| 原始四个 CS 经 NVN 调用 | 每个 count 1/33/65，共 12 case；另 4 次句柄复用 | [a650-nvn-compute.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-nvn-compute.json`；未随公开仓库提供） |
| 原始无纹理 VS/FS，直接 KGSL | 4 case，65536 通道、98304 guard 字节 | [a650-native-graphics.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-native-graphics.json`；未随公开仓库提供） |
| 同一 VS/FS，经 NVN Program/Texture/Draw/Queue | 4 case，同一 command handle，65536 通道、167712 guard 字节，cleanup 0 | [a650-nvn-graphics.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-nvn-graphics.json`；未随公开仓库提供） |

图形四种情况是全屏红色、矩阵平移后的右半绿色、完全出视口保持背景，以及 RGB 顶点插值。纯色精确一致，插值仅允许一个 UNORM 编码单位。调用者录制一次 Draw，销毁录制器，再更新顶点、矩阵和目标后重复提交同一 handle；没有通过 CPU 写入预期输出替代 GPU 绘制。

当前图形 API 是明确受限的资格范围：原始无纹理 pair，RG32F 位置和 RGBA32F 颜色，stride 24；VS Uniform 0；64×64 linear RGBA8 单颜色目标；全 viewport/scissor，depth range 0..1；一个三顶点 triangle。公开 resolver 共 99 个入口，数量不是完整 NVN 一致性证明。未实现状态与 shader 身份明确拒绝，不补成功 stub。

## 换机发现并解决的差异

第一次 CP 测试在 CPU 写映射内存时发生 SIGBUS/BUS_ADRALN，fault address 尾部 `00c`。实际 ARM64 反汇编是 `stur d0,[x27,#0xc]`，为 8 字节非自然对齐写。公共 4.19 KGSL/arm64 页表实现显示 UNCACHED 可映射成 Device memory；这与错误吻合，但不宣称已逐字取得运行中的 vendor 内核。A650 驱动分配改用 WC / Normal-NC，保留提交与完成屏障后，CP 和图形通过。旧错误及反汇编保存在 [CPU 崩溃记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-uncached-cpu-failure.json`；未随公开仓库提供）。

最初 NVN ClearBuffer 的导入内存路径在 QueueFinish 返回 EDEADLK 35；直接分配的 WC BO 则正常。公开的小米 pipa 内核源码中，旧 MAP_USER_MEM 主动丢弃 CACHEMODE，而匿名内存分支不恢复 WRITEBACK，随后缓存同步对有效 WC 模式会跳过操作。这是源码支持的根因解释；旧路径没有单独记录运行内核的返回 flags，因此保留推断边界。新路径使用 GPUOBJ_IMPORT / ADDR，`priv_len` 是整个页对齐 BO 字节数，严格核对返回 ID、GPUVA、size 和有效 WRITEBACK 模式；失败不回退旧接口。新机实际通过这些核对及 NVN 内存测试。见 [旧导入失败](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-legacy-import-failure.json`；未随公开仓库提供） 和 [源码审计](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-import-source-review.json`；未随公开仓库提供）。

四个 CS 初次复验在 count 1、33 后，count 65 的第 33 项开始缺写；首次错误地址与目标差值 264 字节，恰是第 33 个 uint64。A6 原模板只有末尾 UCHE clean，缺少下次输入读取前的 invalidate，旧 count 33 仍可被缓存。补上 A6 专用 CACHE_INVALIDATE 和等待，保持原 shader、输入和 oracle 不变，12 个 case 与 4 次句柄复用全部通过。源码机制与边界复验支持缓存解释，未声称已捕获 UCHE 微观状态。见 [原错误](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-compute-input-cache-failure.json`；未随公开仓库提供） 和 [修复证明](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-compute-input-cache-proof.json`；未随公开仓库提供）。

图形和计算编译器现在显式接受精确目标 chip，所有正例、负例、binding15 变体都携带同一目标；设备证据与编译输出不符即拒绝。A650 的 A6 CCU color offset 为 196608，不能沿用 A735 的 786432。A6 timestamp cache event 使用内部 4 字节 sequence，图形 sequence 与其命令同属一个 job BO，随提交统一保活、退休。A735 四个 CS 的 bin/asm/native header/native metadata 共 16 个产物重新生成后 hash 不变，见 [回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a735-compute-regression.json`；未随公开仓库提供）；这是主机回归，没有再次使用旧手机。

## 下一步

原始纹理 VS/FS 的 A650 主机编译、稀疏槽位、descriptor/sampler 模板和独立像素 oracle 已准备，但尚未提交纹理 GPU draw。接下来先完成该 pair 的采样、discard、混色与输入更新验证，再扩 NVN descriptor pool 和图形状态。深度、窗口/Android 生命周期出图、同步和较完整的 NVN 语义仍待实施。

D2R bootstrap bridge 仍是可单独构建的原 host OBJECT，尚未选入真实游戏运行；已知启动缺口仍包括 DeviceBuilderSetFlags。接入时沿用原 loader/HLE 和 ARM64 硬件执行，不能把原有图形实现作为这条独立驱动的隐式回退。当前不需要用户提供 NVN 文档或原主机对照资料；继续使用已核验原始 NSO、公开逆向和现有项目黑盒输入。

本次 probe 每次执行后均由 runner 停止并核实退出；构建和分析期间不让测试进程持续运行。原 ZIP 仍保存在已提交的 `docs/reference/nvn-turnip-design-v3-20261023/` 中，没有留在临时目录。
