# NVN / Turnip 独立验证线详细设计 v3

日期：2026-10-23。只针对首条 Adreno / Android KGSL 路线细化实验设计；PanVK仍保留独立二版设计，不同时开工。本次没有修改生产工程、部署手机、编译Mesa或运行GPU。新增的是设计修订、接口草案、可执行host参考模型与实施任务。

## 0. 项目隔离与当前任务范围

这是单独的新验证线，不是现有D2R项目的优化分支或合入计划。将来若获准实施，使用独立实验目录/仓库、独立Mesa构建和native client、独立产物及资源副本。稳定D2R只作为测试负载与对照，不修改其日常构建、loader、启动选择器或部署。文中的函数、文件和阶段全部是设计，不是已经执行的开发任务。CPU翻译负载是首要验证目标；GMEM/TBDR优化后置。

## 1. 上一版确实有会影响实现的疏漏

上一版“静态检查通过”仅证明清单、引用和局部合同，没有证明整个系统可以直接照着实现。第三轮按真实失败时序和源码调用链重新审阅，发现并修订了这些关键问题：

- **只有writer不够**：必须跟踪未完成reader和tentative reservation，处理RAW/WAR/WAW；但WaitEvent polling不能进入会阻塞SignalEvent的普通reader集合。
- **copy不是万能退路**：两个可写别名各做一份shadow，会破坏同一draw内的读写/atomic关系。必须保持共同物理表示，否则拒绝该快路。
- **submit失败不是全没执行**：已接受前缀、未尝试后缀、接受性未知、fence导出失败分别记账；禁止整体回滚或重复提交。
- **对象generation不等于ABI正确**：真实SDK对象大小、inline字段、command用量计数均有跨游戏反例。候选头里的sizeof和返回类型不能不经核实就生成shim。
- **KGSL文件不能单独成为驱动**：同Mesa DSO内新增桥接导出，复用完整的冷初始化/析构链，必须维护BO/VMA/fd所有权及WSI真实fence。
- **native descriptor不是统一64B对象**：A6/A7 bank数量有限、graphics stage共用bank，某些SSBO占两个64B槽；shader编译以后还需要linkage、VFD/SP和const配置。
- **sysmem不自动排除并发binning**：首个可核验基线显式关闭该模式，Query/Event作为明确执行边界。
- **跨pool alias、跨extent MapEpoch、cacheline、raw CPU读与部分写**：均补齐具体身份来源、事务和失败路径，不再用几个opaque字段代替算法。

各项审计包含原文位置、反例、修正及测试；不是宣称这些都是已在设备上复现的bug。

## 2. 已撤销一个错误阻塞

完整的host-import和host-image补丁已经找到，在kxn/d2r-mobile独立分支：

- `codex/turnip-host-import-ext-v2`：`f122a40c2bc70017d163653ef91676133e9a7303`。
- `codex/turnip-host-image-native-v1`：`5c8da4df76784230adf30bd25f39063478084962`。

第二份是combined patch，不能在第一份之上再重复叠加。来源、builder和源码证据见bootstrap审计。仍未证明可重建当前f830完整二进制，也未证明可直接apply到新Mesa基准；这两个问题与“核心补丁源码不存在”不同。

普通BO、离屏native提交和host前端不依赖这两个补丁，所以不应被它们拖住开工。

## 3. 将来若实施，验证阶段怎样安排

CPU收益假设及验证顺序见cpu-validation-priority.md；对外一对一NVN库与宿主接入边界见integration-boundary.md。实验优先证明资源消费/绑定表示是否减少CPU指令和周期，不先以GMEM/TBDR带宽优化代替这个问题。

唯一实施顺序以 `experiment-stages.md` 的 V01–V12 为准。bootstrap文档的B0–B6是技术主题，不是另一条强制执行顺序。

### 不依赖设备的前期验证设计

1. V01：固定源码、构建输入、默认关闭的Mesa native bridge构建选项及专用export map。
2. V02：私有host-mock ABI profile、object registry与canary测试。这里的可加载前端不是生产NVN设备。
3. V03：双arena、事务Append、同步扩容callback、handle registry、明确的typed-v1 Copy/Call合同。
4. V04：canonical backing、不可变mapping snapshot、reservation/依赖frontier、retirement、submit receipt与故障注入。

### 后续硬件验证设计

5. V05–07：同一Mesa DSO/同一真实queue的设备生命周期、普通BO、单GPU_COMMAND提交/完成及保活。
6. V08–11：离屏clear/readback、固定4-bank描述符ABI、一个明确shader编译/链接、单sysmem draw。每一项有独立像素/字节判据。
7. V12：包含资源初值和真实依赖的有界replay包及whole-session选择器；不是拿一张三角形证明D2R能运行。

之后才进入真实D2R调用闭包、host-import、必要WSI与图像/同步/query资格；GMEM优化在正确sysmem/replay基线之后。完整场景兼容不是这12个验证阶段的隐含承诺。

## 4. 本版固定的实现选择

- native冷启动与配对销毁复用当前Turnip内部构造；公开逐draw Vulkan对象/命令转换不进入主路径。
- 首个device backend只用普通BO、单真实queue、offscreen、单sample RGBA8、sysmem、VS/FS，之后独立CS；禁止并发binning。depth、多附件、GS/tess/XFB、复杂feedback均按明确增量能力加入。
- `NVNN_ADRENO_ABI_1`采用4个bank，256B root和明确的stage/slot索引；具体硬件大小、索引单位、UBO/SSBO边界见gpu-execution-contract。
- 全表snapshot只是正确性基线；约2300次变化draw若每次复制18.25KiB，会写约41MiB/frame。该条件算术不是性能结果，更不是推荐的终态。必须记录真实dirty分布；普通KGSL不存在免费的不连续页COW。
- 用户应用的guest对象和GPU地址都不直接等同host C对象/指针。私有typed-v1可实现并测试，但真实D2R准入还要证明inline读写与Copy/descriptor/FenceEpoch的消费合同。
- unknown userptr alias、未封口raw command patch、未支持CPU/GPU atomic域等明确拒绝，不能静默造shadow或假no-op。

## 5. 文档权威顺序，避免旧稿和新稿互相打架

1. 本版 normative contracts：bootstrap-experiment-plan、recording-abi-contract、memory-contract、gpu-execution-contract。
2. 本版 implementation-backlog 与 open-questions。
3. 本版 `interface/nvn_kgsl_contract.h`：提交、身份、寿命和访问分类的C边界草案；它替代一版的单Completion submit接口，不是整个驱动实现。
4. 本版修订后的API JSON/CSV和 `api-overrides.json`。
5. `reference/` 下二版文档只保留源码与背景；与本版冲突的句子不再是实施依据。

版本域不同：文档/内存合同v3、native bridge导出API v1、shader ABI_1互相独立，不能用同一个整数代替全部缓存/ABI版本。各文档的NvnnStatus概念统一落到接口NvnnResult与独立detail reason；不要创建含义不同的重复枚举。

## 6. 审查通过的准确范围

本包执行了纯host参考模型、真实错误模型的反例检查、内部C/C++头编译和范围断言、API覆盖及文档引用校验。它们证明的是修订合同在这些输入上自洽、原有错误简化能被检测，不证明Mesa native实现存在或硬件正确。

明确未运行：Mesa完整build、Android加载、KGSL ioctl、GPU/shader、手机游戏、性能/温度验收。`qualification-gates.md`将未知问题细分为具体输入、判据和失败行为，并指出它阻断哪个里程碑；不是一句泛泛的“以后再测试”。

结论：**设计已细化到将来可按V01–V12验证的程度；当前没有执行开发授权，不创建PR、不修改稳定D2R、不构建或部署新后端。**
