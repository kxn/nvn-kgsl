> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

独立 NVN 硬件后端的实施细化
==========================

最新增量：已切换用户指定的 A650 测试机，原始无纹理与纹理 VS/FS 均通过 NVN 图形入口真机执行；纹理部分还验证了独立绑定域、池身份、录制后登记和重提交换 backing。154 个入口仍是明确的受限 profile，尚未运行真实 ARM64 游戏模块。AHB 的 CPU 像素 backing/offset 已在新机鉴别，KGSL dma-buf 导入和显示仍待验证。结果见 [纹理与出图准备](texture-progress.zh-CN.md)。下文保留初始设计和较早阶段记录，不应把历史“尚未实现”描述当作当前唯一状态。

实施更新：独立目录 `nvn-native/` 已建立，普通应用 UID 的 KGSL 内存、提交、完成和首批 NVN 资源/ClearBuffer 均已通过。四个原始 D2R 内嵌 Maxwell CS 现已经严格前端、NIR/IR3 和 NVN Program/Dispatch 在 Adreno 真正执行，12 组主用例与 4 次句柄复用读回通过。最新实现与边界见 [实施进度](bringup-progress.zh-CN.md)；证据包括 [NVN 资源](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `nvn-resource-check.json`；未随公开仓库提供）、[原始 Maxwell 编译](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `maxwell-ir3-check.json`；未随公开仓库提供） 和 [NVN compute 真机记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `nvn-compute-check.json`；未随公开仓库提供）。运行期编译、完整资源语义、绘制、上屏及真实 ARM64 游戏调用接入仍未完成。

T06 新进度：首批 NVN 资源/录制/队列及 Program/Dispatch 已落地。当前 shader 通过显式离线编译缓存匹配原始 code/control，缺失返回 ENOTSUP；测试调用者提供 NVN Uniform/Storage 绑定，尚未执行真实 ARM64 游戏模块。范围、前端算术修复和剩余工作见实施进度与真机记录。

最新更新：Uniform/Storage 资源绑定已修正为 NVN 槽位，调用者不再提供原始 c0 表；12 组原始 CS 与 4 次复用继续通过，见 [资源槽位验证](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `nvn-buffer-slots-check.json`；未随公开仓库提供）。一对原始 VS/FS 已通过 KGSL 原生离屏 Draw：纯色、矩阵平移、裁剪和颜色插值共四次提交，65,536 个通道与 98,304 个边界字节检查通过，见 [图形真机证据](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `kgsl-graphics-check.json`；未随公开仓库提供）。它尚未通过 NVN 图形入口调用，纹理/深度、窗口和真实 ARM64 游戏执行仍未完成，不能算 T05/T06/T07 整项完成。

目标是独立 NVN 兼容用户态 GPU 驱动：接收 NVN 调用，生成 Adreno 描述符、机器码与命令，经 KGSL 执行。D2R 只提供真实调用者、输入和回归环境。第一款设备采用当前 Adreno 735；Mali 是后续独立硬件后端。本细化以“没有官方 NVN 文档、没有原机测试环境，只能使用现有项目黑盒和公开逆向成果”为既定条件，不再把取得 SDK 文档当作开工前置条件。

**可以开工；shader 不需要整体重写。首轮复用 Maxwell 翻译前端、NIR 和 IR3，新增的是 NVN 输入语义、资源绑定转换、Adreno 程序状态与 KGSL 执行胶合。** 当前已完成源码调查和一次主机翻译复验，下面的驱动模块与接口是待实施设计，不是已有实现。

1. shader 复用方案
-----------------

建议首轮使用这条编译链：

```text
NVN Program 的原始代码 + 控制段 + 编译所需状态
  → 有边界检查的输入解析与状态快照
  → Ryujinx Maxwell 解码 / 控制流 / 指令语义翻译
  → SPIR-V + 资源使用信息
  → Mesa spirv_to_nir
  → 新的 NVN 资源和系统值 lowering
  → IR3 优化 / 寄存器分配 / 调度 / 汇编
  → Adreno 机器码 + 程序元数据
  → 原生命令和 KGSL 提交
```

这里 SPIR-V 只是编译器间的数据格式。已有包装器里的 `TargetApi.Vulkan` 选择其输出约定；新桥需要明确处理坐标、资源及内存语义，不创建 Vulkan 设备、队列或 pipeline，也不经过该 API 的执行路径。为了去掉一个中间文件格式而先重写成熟的控制流、SSA 和代码生成，不是首轮的合理投入。直接从现有 IR 输出 NIR 是后续可评估选项，需要新增 emitter、跨语言桥与对应回归，不是现成能力。

| 工作 | 可复用的具体代码 | 必须补的边界 |
|---|---|---|
| Maxwell 二进制解码、分支恢复 | `dm/third_party/ryujinx/src/Ryujinx.Graphics.Shader/Decoders/Decoder.cs`、`Translation/Translator.cs` | 图形 SPH 与 CS 输入不同；间接跳转读取 C1；未解析分支不能默认为成功 |
| 指令语义、SSA、结构化控制流、SPIR-V | 同库 `Instructions/`、`Translation/TranslatorContext.cs`、`StructuredIr/`、`CodeGen/Spirv/` | 保留有效修复；已存在的移动端特例须单独识别，不能一概开启或丢弃 |
| 翻译环境接口 | 同库 `IGpuAccessor.cs`、`ShaderProgramInfo.cs` | 新建独立 accessor，回答实际 NVN 状态、资源类型、内存大小和目标能力；记录所有实际查询 |
| 输入提取线索 | `dm/gfx/tools/dmsh_build.py:253` 附近 `job_for`、`maxwell_refs.py`、`shader_map.py` | D2R 包布局与候选控制段偏移放在测试语料适配器，不能默认所有 NVN 版本都一样 |
| SPIR-V 导入 | Mesa `src/compiler/spirv/nir_spirv.h:176` 的 `spirv_to_nir`，`spirv2nir.c` 是独立工具样例 | capability、地址格式、显式内存、系统值及资源访问转换由新后端提供 |
| Adreno 编译器 | Mesa `src/freedreno/ir3/ir3_compiler.h`、`ir3_nir.h`、`ir3_shader.h` | 不止调用一个 compile 函数：必须准备 NIR、变体键、常量布局及最终硬件元数据 |

仓库中的 translator README 仍说“unmodified”，但当前代码和 Git 历史已有修改，例如 fragment quad transfer 和 AST 优化修复。采用当前源码时，应记录完整树指纹和修复清单，不能只记“Ryujinx 1.3.2”。MIT 声明见仓内 `LICENSE.txt`；三个项目独立于模拟器 CPU、系统服务和整套 GPU 运行时，但 `Ryujinx.Common` 仍带 NuGet 依赖，需锁定并评估裁剪。

现有 `dmsh_xlate/Program.cs` 不能直接作为新库的通用前端：

- `TexBase=0x168`、`SampBase=0x568`、数组长度 32 是已有输入假设，要归入可验证的 shader ABI profile。
- texture 类型、image 格式、顶点输入、片元输出与 CS shared/local size，部分来自 PC shader 配对表。该表可作 D2R 的辅助对照，不应成为新 NVN 核心依赖。
- 缺数据时会回退 2D、RGBA8、线程组 1 或记录 note 后继续；新资格测试应返回结构化“信息不足/不支持”，不能将这些回退计为通用覆盖。
- C1 越界当前返回 0；间接跳转的错误目标可能形成 GPU 死循环。新前端要界定范围、失败原因和允许编译的输入。
- `QueryGraphicsState` 固定了若干 topology、深度和窗口约定；必须改为相关状态或可证明无关的规则。
- shader 还会使用 translator 自己的 support buffer。`Translation/ResourceReservations.cs` 明确保留了它；不能只绑定游戏 UBO 后认为所有常量齐全。

IR3 胶合建议按如下顺序实现，并逐阶段调用 NIR 验证和现有反汇编工具：

1. 使用目标 `fd_dev_id`、`fd_dev_info` 创建 `ir3_compiler_create`。其主机测试已有 `dev=NULL` 的用法，因此离线编译不必先创建图形设备；精确 Adreno 修订仍须设备查询确定。
2. 用 `spirv_to_nir` 导入，处理入口、显式内存、资源索引和地址格式；保存导入前后 IR。
3. 编写 NVN 资源 lowering：把 guest cbuf、SSBO 来源、独立纹理/采样器和动态句柄映射到本库的硬件布局。使用现有 NIR pass，新增 pass 只覆盖缺口。
4. 依据公共编译器接口安排 `ir3_finalize_nir`、I/O lowering、`ir3_shader_from_nir` 和 variant 创建。`ir3_shader_create_variant` 内部会做 post-finalize；不能照抄名字后把同一个有前置条件的 pass 重复执行。具体顺序是首个编译 spike 的交付之一。
5. 读取 `ir3_shader_variant` 的机器码、寄存器、常量长度、输入输出、private/shared memory、局部线程组和 wave 信息。使用 `ir3_link_shaders` 及相关 I/O 查询构造阶段连接；机器码成功不代表 VS/FS 已正确连接。
6. 将元数据转成 Adreno 程序状态。由本库分配代码和 private memory，绑定 descriptor/constant，承担 cache、提交和在途寿命。

Mesa Gallium 的 `ir3_descriptor.c` 可说明 NIR 如何变成 IR3 bindless 访问，但它采用自己的固定偏移，并且断言不支持 API 层 bindless 输入。不能原样搬成 NVN 动态句柄实现。NVN 句柄可能来自动态 buffer，不能只靠静态扫描列出的纹理完成通用兼容。

**编译缓存键至少包含**：代码、控制段和实际读取常量的完整 hash；解析/翻译器源码指纹；被查询的状态与资源类型；目标芯片修订；lowering 与资源 ABI 版本；精度和 subgroup 选项。资源内容若未参与 specialization，不应无条件导致重编。区分代码缓存、硬件状态缓存和在途资源版本。

2. 手机端 shader 编译不能被离线成功掩盖
-------------------------------------

首轮主机离线编译已知 shader 可以解除 GPU bring-up 对 C# Android 打包的依赖，但只覆盖已准备的输入。通用 NVN 库最终必须处理新程序、状态变体以及运行期修改；缓存 miss 不能静默跳过 draw。

目前有两个真实选择：

| 候选 | 源码事实 | 开工决策 |
|---|---|---|
| 现有 Ryujinx C# 前端 | 独立项目已有，主机可构建且已复验；包含可复用语义和现有修复 | 首轮采用；单列 Android ARM64 编译部署试验，验证支持的运行方式、体积、冷启动、分配和确定性，不能凭主机成功承诺手机已可用 |
| yuzu 派生的 C++ shader compiler | 已实际读取 Eden 仓库的 `TranslateProgram`、`Environment`、`EmitSPIRV` 和静态库 CMake；依赖 `common`、`fmt`、`sirit` | 手机原生 C++ 的候选；先验证仅图形编译器的依赖闭包和语料一致性，不接入它的 CPU 或服务；这批文件标记 GPL-2.0-or-later / GPL-3.0-or-later，须按项目交付方式选择依赖策略 |

C++ 候选不是“拿到一个现成无依赖 .a”。`Environment` 仍需回答代码、Cbuf、纹理类型/格式、local/shared memory、workgroup 等；换语言不会消除输入语义问题。首轮不移植整个模拟器，也不同时维护两套生产前端。

UAM/NAK/nouveau 是往 NVIDIA 硬件生成代码的编译器，不是现成的 Maxwell → Adreno 翻译器。UAM 可生成可控 Maxwell 测试输入，但其 DKSH 和 driver constant ABI 属于 deko3d，不能把 DKSH 直接当 NVN control 段。envytools 的 `envydis/gm107.c` 可做指令解码交叉检查，不提供跨 GPU 完整语义翻译。

3. 在没有官方资料时建立可用的行为依据
-----------------------------------

现有 `hle_nvn.c` 强制接管 `nvnBootstrapLoader`，原 SDK 内 NVN 实现不执行。因此在现有项目跑一个 API 用例，只能得到**当前兼容实现的行为**，不能直接证明原 NVN 应当如此。可用它持续推进，但每项结果要注明依据：

| 证据 | 能证明什么 | 不能自动证明什么 |
|---|---|---|
| 游戏实际调用、传入对象/资源、内存读回、输出画面 | 当前调用者需要哪些功能和实际输入组合，修改是否回归 | 从未调用的 API、时点边界、其他 NVN 版本 |
| 当前项目的黑盒对照 | 对已知输入，新库与现有实现是否一致 | 两边共同保留的错误是否符合 NVN |
| 公开逆向代码、寄存器表、容器说明 | 实现候选、格式字段和可鉴别用例 | API 级完整合同；不同仓库若复制同一来源，也不是独立佐证 |
| 原始二进制静态调用点与数据结构检查 | 参数传递、对象可写范围、特定版本的字段和路径 | 硬件 cache 可见性或 GPU 执行正确性 |
| 新库自己设计的模型/变形测试 | 实现的一致性、资源寿命、定义好的不变量 | 原 NVN 的未证实语义 |

建立逐项 evidence ledger：API、版本/profile、观察、源码/样本 hash、解释、置信程度、冲突、下一项鉴别实验。对不影响当前输入的未知项先明确不支持；不要用最方便的行为填满 524 个入口。

首批黑盒语料从原生游戏调用边界提取，先核实现有日志/工具可用能力，仅补 NVN 语义所需信息：调用序列、资源初值、buffer offset 和 alias、Program 原始输入、参数何时变化、实际等待/读回、有限输出区域。记录对象稳定 ID 与内容版本，不能只保存宿主指针。使用现有 GPU decode/replay 工具处理命令，不另写 profiler 或通用 GPU 重放器。

优先缩减五类可重复用例：shader 纹理类型/常量变化、Copy/Call 后源命令的变化、录制后到提交前的 descriptor/数据更新、同一 Sync 重复 Fence、写入后 GPU/CPU 消费。先比较实际游戏出现的合法序列，再谨慎扩展；如果现有实现本来把两种行为合并，差分结果应记为无法鉴别。ZIP 的反例是测试设计输入，不是这些行为已被原库证明。

本次确认的公开来源：

- [deko3d](https://github.com/devkitPro/deko3d)：作者说明它来自 NVN 逆向，但不是 NVN 克隆。实际源码包括 shader/control 分离、command/control list、TIC/TSC、QMD 和 cache 操作；适合形成硬件行为假设，不能直接把其 API 合同移植为 NVN。
- [envytools Maxwell 解码](https://github.com/envytools/envytools/blob/master/envydis/gm107.c)：可核对 opcode、控制字、寄存器及字段。
- 本仓 `dm/gfx/nvn/include/nvn.h` 头注释说明来源是 Darksiders II DWARF，并提到 NVN 53.105 的局部定义。不是完全没有公开类型线索，但必须逐项核对当前输入版本。
- [UAM](https://github.com/devkitPro/uam)：用于构造 shader 测试输入与对照生成规则，编译方向和 ABI 如前述。
- [GLSLC NVN 研究项目](https://github.com/masagrator/glslc-nvn-static-recompiler)：其 `glslcinterface.h` 提供 shader 输出区段、反射、control/code 大小、版本等候选字段。静态重编译运行方案不加入本运行时，也不是本轮开工依赖。
- [C++ shader compiler 源码](https://github.com/eden-emulator/mirror/tree/master/src/shader_recompiler)：已核对真实接口和构建依赖，作为候选或独立对照。

还检索到 Wonder 项目声称有 NVN shader 容器与翻译调研；本次 GitHub connector 和网页直接读取相关文件均返回 404。搜索摘要中的具体偏移不提升为已核验事实，也不纳入生产解析器。当前未找到并核验一份完整公开 NVN 规范。

4. 独立模块和首批实现任务
-----------------------

建议新代码位于仓库根部 `nvn-native/`，保持独立构建边界；该路径目前是设计提案。旧 renderer 仅由 `adapters/d2r/` 访问以做输入导出和对照，不让其类型泄漏入核心。

| 目录 | 职责与交界 |
|---|---|
| `abi/` | resolver、版本/profile、对象允许写入范围、句柄；外部对象与内部 side table 分开 |
| `core/` | 资源身份与 backing、命令/control arena、录制/消费、依赖、Sync/Event/Query、退休 |
| `shader/` | 输入解析、严格 accessor、需求和查询记录、前端适配、NIR lowering、IR3 编译、缓存 |
| `hw/adreno/` | 芯片能力、FDL、descriptor/state 编码、PM4、cache 操作、程序与阶段连接 |
| `os/kgsl/` | 实际 UAPI 探测、BO/context、提交、时间戳/fence、失败与回收；不接触游戏类型 |
| `platform/android/` | 测试 APK、窗口缓冲导入、acquire/release 和生命周期 |
| `adapters/d2r/` | 原生调用入口适配、语料导出、实验副本集成；不承接游戏玩法 |
| `tests/`、`tools/` | 有界语义用例、既有工具编排、证据和构建清单 |

建议的内部边界：`ShaderInput → CompileResult` 返回机器码、需求、资源映射和诊断；`RecordedBatch → PreparedSubmission` 固定实际消费的版本和资源；`submit → accepted/rejected/unknown` 分开表示提交结果；`CompletionToken` 才能触发退休。GPU 地址由 BO 层管理；不得把客户指针直接当目标 GPUVA。所有名称仅表示待实现合同。

按以下顺序可以直接拆提交：

| 编号 | 具体实现 | 依赖 | 验收与退出条件 |
|---|---|---|---|
| T01 | 独立构建入口、公共依赖与源码锁；host compiler 和 Android `.so`/测试 APK 输出分离 | 无 | 构建日志/产物 hash/依赖列表；不链接原 renderer、CPU 模拟后端或 Vulkan loader |
| T02 | shader fixture schema、D2R 原始输入导出、严格 accessor、查询日志 | T01 | 每个样本携带 code/control/C1 与必要状态；不靠默认值冒充事实；缓存键随实际 specialization 输入变化 |
| T03 | `spirv_to_nir` + 资源 lowering + IR3 单阶段离线编译 | T01、T02 | 先一个固定 buffer CS，再一个 VS/FS；NIR 验证、IR3 反汇编和资源元数据可读；尚不声称 GPU 通过 |
| T04 | KGSL 设备/context、小 BO、CPU/cache 边界、PM4 提交、完成与回收 | T01 | 普通测试 APK UID 真正分配/提交；GPU 写哨兵并读回；超时不重用在途内存；记录精确 chip ID |
| T05 | 用 T03 机器码执行 CS，再执行离屏 VS/FS | T03、T04 | CS 输出可计算字节；VS/FS 小图包含 UBO、纹理和深度；改变资源后输出相应变化，验证 cache 和连接 |
| T06 | NVN 最小前端：Device/Queue/Pool/Buffer/Program/CommandBuffer/Sync | T04、T05 | 同一语料经 NVN 入口执行；别名、消费时点、重复 Fence 有记录；未知语义标注，不能全部映射 queue idle |
| T07 | Android 原生窗口 + 有界 D2R 真调用接入 | T06 | 缓冲导入和 acquire/release 正确；实验副本执行真实 ARM64 游戏模块、上屏/退后台/退出；旧分支不受影响 |
| T08 | 手机端新 shader/variant 编译资格、能力矩阵扩展、性能 | 可从 T03 开始独立验证，广泛兼容发布前完成 | 新输入 miss 有完整路径；按实际错误补功能；测 CPU/GPU/内存，不把离线全集当通用运行期编译 |

第一份可验收交付应同时包含 T01–T04 的最小结果：**一个可追溯的真实 shader 编译样本、一个通过普通 APK UID 的原生 KGSL 写入读回，以及两者尚未接通之处的明确记录。** 下一份 T05 才证明 shader 真正在 Adreno 上执行。完成这些后再连接真实 NVN 入口，避免把 ABI、编译器和硬件故障混在整场游戏里调试。

5. 已查清的工具限制会影响开工顺序
--------------------------------

`src/freedreno/decode/replay.c:717` 起有独立 KGSL 路径：GPUMEM_ALLOC_ID、DRAWCTXT_CREATE、GPU_COMMAND、WAITTIMESTAMP。优先用现有 decode/replay 设施；该工具按 1 GiB 区域、固定地址和 `id << 12` 映射工作，必须先做有界适配，不能直接当生产 BO 管理器。

`src/freedreno/computerator/a6xx.cc` 有 A7XX 的 compute 状态、代码、UBO/SSBO 和 dispatch 示例，`main.cc` 支持第 7 代。但入口调用 `fd_device_open`，本机源码中的该函数打开 DRM msm/virtio 节点，没有直接打开 Android `/dev/kgsl-3d0`。它的 README 能运行 compute，并不等于原样可在这台 Android KGSL 手机运行。可复用硬件示例和工具组件；提交路径先走已确认的 KGSL UAPI。

Mesa 构建也不是复制几个 `.c` 就行：IR3 依赖 NIR、Mesa util、IR3 ISA encode/decode、XML 生成头和 Bison/Flex。`libvtn` 有独立静态库，`spirv2nir` 是现成工具；Freedreno 的 Meson 子目录和工具开关仍需一起整理。根 Meson 的 `needs_flex_bison` 条件没有单独覆盖所有只开工具的情况，必须验证纯公共库配置，不把尚未执行的 meson 参数写成已跑通命令。冻结独立输出目录，不重用他人现有构建树。

6. 本次新增验证与现阶段需要的东西
-------------------------------

已用独立工作树的当前源码、.NET SDK 10.0.402，构建 `dmsh_xlate.csproj`，0 warning/0 error。取本机既有 `nvidia-shader-isa` 的 5 个片元 shader 提取样本，补入仓内配对事实表，关闭 debug/reduced 后翻译：5 成功；NDK 自带 `spirv-val` 的默认环境检查全部通过，accessor notes 为空。详细输入/输出 hash 与范围见 `shader-reuse-check.json`。

这组结果验证的是主机前端复用。样本依赖历史提取数据和 PC 来源事实，没有重新核验原始包到样本的全部提取链；没有 CS/VS 覆盖、NIR/IR3 编译执行、Adreno shader 执行或原 NVN 语义验收。空 notes 也不证明所有未覆盖的默认 accessor 答案正确。不能把 5 个样本写成游戏/驱动完成度。

当前不需要用户提供 NVN 文档，也不需要再购设备。现有源码、工具链、已连接 Adreno 和项目输入足够进入 T01/T02/T04。需要工程补齐的是 shader 输入及状态语料、纯公共编译器构建、普通 UID 硬件资格、语义证据表和手机端编译方案；这些应由实施工作完成。后续若扩展到更多游戏/SDK，再增加不同调用者的合法输入样本，不能凭单一 D2R 推断全部 NVN。

原 ZIP 和解压文件保持原样，研究修订只写在研究目录。新后端性能、功能及真机里程碑单独计量，不继承旧 renderer 的成绩。
