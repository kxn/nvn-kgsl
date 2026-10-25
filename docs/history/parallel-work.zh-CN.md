> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

并行实施安排
============

用户于 2026-10-23 明确要求并行开展研究，现已启动三个子代理。所有跟踪代码仍在独立分支 `codex/nvn-turnip-study-20261023` 整合；当前验证检查点为 `a58bfef`。本安排不改变 native KGSL 路线，不引入原项目的 Vulkan 实现。

| 工作线 | 可以立即独立推进的内容 | 交付及验收 | 与其他线的依赖 |
| --- | --- | --- | --- |
| 图形硬件状态 | A735 sysmem 初始化、VFD/VPC、VS/FS 程序与常量、RGBA8 MRT、Draw、CCU 完成协议 | 精确公共源码依据、必需状态清单；随后由主代理实现并读回离屏像素 | 使用已编译的原始 VS/FS 元数据，无需等纹理或 D2R 接入 |
| 纹理与 shader 前端 | 明确 sampler/query 输入合同、纹理反射、SPIR-V→NIR 的 texture/sampler binding、FDL 描述符 | 原始纹理 shader 离线编译与反射检查；随后连接首个 linear RGBA8 2D 采样 | 离线部分独立；硬件像素验证依赖图形 Draw |
| D2R 接入与 ABI | loader/host 门、bootstrap resolver、函数 ABI、调用者对象与资源约定、启动缺口 | 独立后端选择和桥接设计、可核查接口清单、明确的首个缺失入口 | 桥接/ABI 可先做；真实首帧依赖驱动接口与图形能力 |
| 主代理整合与设备验证 | 保留已通过版本，整合跟踪代码，构建及普通 UID KGSL 提交，管理资源寿命和失败证据 | 原始输入/源码/工具/产物身份、真实输出与拒绝检查；测试后停止进程 | 唯一设备提交者，按前述交付逐项整合 |

子代理当前只读调查并在忽略目录留中间笔记，不切分支、不改跟踪源码、不安装 APK、不操作 ADB。这样可以同时查硬件状态、编译器和调用入口，避免并发修改同一 emitter 或同时改变设备状态。后续若分配代码实现，按互不重叠的文件和接口合同划分；设备队列、测试 APK 和验收记录仍由主代理统一控制。

已得到的实施信息
----------------

- 图形线确认 A735 对应 A7XX gen2。图形 shader 加载不能套用 A6XX 的 shader preload；还需恢复图形上下文、显式配置 sysmem/CCU 和完整阶段状态。VS 的 `early_preamble` 不代表矩阵 UBO 会被自动加载，仍须使用该设备的常量加载路径。具体状态以公共 Freedreno 硬件编码及实际 IR3 元数据为据，尚未完成 GPU Draw 资格验证。
- 纹理线已用核对过身份的原始输入复现当前拒绝：`a7672f1d3bfeb7d0` 的 FS 被 texture binding 资格检查拒绝，`7c603a02d455070a` 的 FS 缺少明确的 `QuerySamplerType`。它们尚未通过编译。现有 NIR `nir_lower_samplers`、IR3 和 FDL 已提供主要机制，需要补资源合同及桥接，不必重写整个纹理 shader 编译器。
- 接入线定位到原 loader 的 `force_lookup` 与 `nvnBootstrapLoader` 替换路径，可以把 guest 调用指向独立后端。当前独立 resolver 的 52 个入口不能等同于游戏所需接口覆盖；原模块静态加载表中有 523 个名称。桥接不能链接耦合原渲染器的 `hle_nvn.c`，缺失接口的返回/失败行为须明确处理。

接入线进一步检查了同一原始 NSO 的 ARM64 调用点：`0x173cb74` 的 builder 默认设置后，`0x173cb88` 即调用当前缺失的 `nvnDeviceBuilderSetFlags`，参数为 `0x100` 或 `0x115`；此缺口早于 DeviceInitialize。随后还有 GetInteger、窗口原点和深度模式等状态入口。静态集合中，523 个加载名称有 216 个存在调用点，其中 34 个已被新表覆盖、182 个尚未覆盖；这些数量不代表首帧一定执行的接口数，也不能当成通过率。不能以忽略 flags 或返回统一假成功来绕过缺口。

这些是调研与实施边界，不是新增游戏运行里程碑。当前已经验证的是原始四个 CS 经新 NVN 资源槽位的 16 次硬件执行，以及原始 VS/FS 的离线编译和链接。尚未验证 VS/FS 图形像素、纹理采样或真实 ARM64 游戏模块通过新后端执行。

合流顺序
--------

1. 图形线交付硬件状态依据，主代理完成无纹理离屏 Draw 和像素读回。
2. 同期纹理线先完成输入合同和离线编译，接入线先完成 ABI 与 resolver 方案。
3. 在可重复的 Draw 上增加纹理采样，按实际输入补通用 NVN 图形接口。
4. 通过独立选择入口接入 D2R，记录真实游戏执行到的第一个缺口，再继续逐项补齐；不把原创调用者和 shader 探针算成游戏首帧。

本批合流结果：图形线已交付原生状态模板，并通过四种真实离屏输出；纹理线已通过原始匹配 VS/FS 的严格前端、NIR/IR3、汇编往返和链接，尚未 GPU 采样；接入线已交付原 HLE bootstrap 适配、具名失败边界及有完整核验记录的 ARM64 OBJECT 构建。详见 [实施进度](bringup-progress.zh-CN.md) 和 [图形真机证据](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `kgsl-graphics-check.json`；未随公开仓库提供）。NVN 图形入口、纹理/深度执行、窗口及真实 ARM64 游戏运行仍是后续工作。
