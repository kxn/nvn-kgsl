> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# NVN 公开语义资料补充：Copy / Texture / Sampler / 同步

结论：没有找到可公开核查、覆盖 raw NVN 全族并匹配本游戏 SDK53.314 的完整官方行为手册，也没有发现可据此认定当前 public Copy 实现有误的证据。比53.105裸 C 头更有行为信息的是已知 open-ead 重建 wrapper 的具体调用链；新增独立作者 SaltyNX/FPSLocker 提供了原机同步用途的反例。它们的证明层级不能混同。原 Copy 候选保持冻结并重新核对 owned14文件 SHA 均未变；没有 tracked/ADB/GPU/旧 Metal 或 Vulkan renderer 读取。

| 家族/来源（固定版本） | 能核查的增量事实 | 明确不证明/使用建议 |
|---|---|---|
| [Copy region/stride wrapper](https://github.com/open-ead/nnsdk/blob/3565f8cd6a4ddecd8206248bbc6fe76d46e13fa7/lib/gfx/src/gfx/detail/gfx_CommandBuffer-api.nvn.8.cpp#L31)，`3565f8c`，31–85、451–637行 | 1D array 放Y，2D array放Z；mip view独立选择。简单B2T/T2B重设两stride为0；有buffer-image宽高的重载计算后显式设置。buffer offset加在BufferGetAddress上。 | 这是nn::gfx重建上层调用组合；并非raw NVN实现。compressed wrapper的整除公式依赖其上层输入，不能压倒原SDK53.314已核的ceil/真实BC边缘规则。不能从wrapper推断copy后stride自动复位、overlap合法或异步完成。 |
| [Sampler wrapper](https://github.com/open-ead/nnsdk/blob/3565f8cd6a4ddecd8206248bbc6fe76d46e13fa7/lib/gfx/src/gfx/detail/gfx_Sampler-api.nvn.8.cpp#L16)，同版本，16–71行 | wrap/filter/LOD/compare设置后**无条件**调用border setter；aniso有条件设置，min/max reduction有单独feature gate。 | 能补“非border wrap仍保留任意border元数据”的回归假设；不定义raw默认值或active border执行。不能拿三种上层预设色限制NVN任意borderbits，也不能代替Adreno border table。 |
| [Barrier wrapper](https://github.com/open-ead/nnsdk/blob/3565f8cd6a4ddecd8206248bbc6fe76d46e13fa7/lib/gfx/src/gfx/detail/gfx_CommandBuffer-api.nvn.8.cpp#L790)，同版本，790–897行 | ordering与texture/shader/descriptor invalidation按access/stage分别组合；indirect读另有ORDER_INDIRECT_DATA。 | 不是完整NVN内存模型，更不能直接证明A650 CCU/UAV/ME packet/cache或跨KGSL context。适合补consumer-domain测试，不因wrapper没有CPU等待便省backend依赖。 |
| [SaltyNX NVN原机hook](https://github.com/masagrator/SaltyNX/blob/cf2adb7b8facdc34e3cbf9ec6ff7686f67575048/saltysd_core/source/api/NVN.cpp#L592)，`cf2adb7`，592–610及803–840行 | 单独保存WindowAcquireTexture的sync身份，只有该sync的可选double-buffer workaround改timeout；其余SyncWait沿用uint64 ns参数和原函数。 | 新独立作者的原机调用证据，不是完整API规范。该工具主动改行为、各游戏兼容性不同。其宽松const原型（例如输出index仍const int*）不能直接恢复严谨PFN，更不能用它推断新返回型。没有采用其计时/等待/三缓冲实现。 |
| [FPSLocker作者同步警告](https://github.com/masagrator/FPSLocker/blob/7a427c0c94217a01540cb5391112956b924b4945/README.md#sync-wait)，`7a427c0`，45/133行 | 作者明确Window Sync Wait禁用会使某些游戏崩溃，列原机兼容差异。 | 仅作为“不能把所有Sync当vsync/no-op”的反例；不是D2R结论或规范，更非Android资格。没有新增工具、采样器、性能结论。 |
| [NVIDIA DMA B0B5](https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/dma-copy/clb0b5.h#L130)，`9fdf5c4`，130–137及190–253行 | 原厂copy-engine字段分开source/dest pitch、line length/count、GOB block size、origin。 | 可继续与原SDK method生成作静态交叉核对；没有给raw NVN复制旗标、pixel/block转换、overlap或生命周期合同，也不是Adreno opcode/FDL。 |
| [NVIDIA B197 texture/sampler](https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clb197tex.h)，同版本 | 原厂source格式/filter/LOD/wrap/border-source字段；可对照SDK。 | 不是NVN参数/默认值/资格域定义，更不是Adreno descriptor。不得以source硬件位域替代现normal FDL和A650 sampler实现。 |

## 版本、独立性及许可

- `open-ead/nnsdk@3565f8c`是既有nnheaders迁移，不计第二份独立RE。这里只新增具体wrapper行号/可实施边界；其53.105头说明不能升级为本游戏53.314完整契约。仓库无统一可识别license，单个nvn.h的MIT不能扩大到wrapper；未复制其实现到production。
- SaltyNX主仓库metadata license=null、tree未见根LICENSE；只在ignored保存所读原文/指纹，不移植代码。FPSLocker LICENSE是GPL-2.0，摘要只记作者事实/链接，未导入实现。
- NVIDIA两份header各有NVIDIA permissive MIT-style文件notice（copy2019；texture文件SPDX为2001–2010）；保存原notice。仍须区分NVIDIA source硬件与Qualcomm native硬件。
- 本次搜索命中的BotW/SMO codebrowser由open-ead相同头/重建wrapper生成，不计独立证据。GL、CUDA、DirectX或其他名字相似资料不定义NVN；未采用。其它移植renderer调参/优化日志不作为raw NVN契约。
- 已有deko3d/ScarletNX的边界沿用前报告：非NVN clone或别游戏作者观察，不能替代本游戏SDK。此次没有重复读取其renderer、CPU/HLE后端。

原URL、commit、bytes、SHA256在`sources.json`，逐项机器表在`table.json`；四个repo当前tree/metadata快照保留独立文件。没有收录原SDK NSO、原游戏控制字节或全局log。
