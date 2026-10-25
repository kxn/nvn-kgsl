# 独立验证项目的资格门：未决项与决策测试

本文件只定义未来独立实验项目的资格条件，不授权或启动任何实现、设备执行、稳定 D2R 修改或 PR。稳定 D2R 仅作为工作负载/reference，实验应使用独立 Mesa 树、产物与 client。

这是资格门清单，不是“所有事情都等硬件”的免责声明。静态可确定的选择直接定下来；只有无法从当前证据确定的语义/设备行为才保留问题。通过某个窄测试仅解锁相应能力，不把它外推成所有设备支持。实验阶段和未来实验顺序见 experiment-stages.md。

## 1. 现在已作出的选择

- 首条实现为 Android Adreno/KGSL；PanVK 分文档保留设计，不在首批实现共同 backend/vtable。
- 独立实验 Mesa 工作树内、同一 DSO 导出版本化 native bridge；冷初始化允许复用 `tu_CreateInstance/tu_CreateDevice`。hot draw 直接 native 资源/状态/提交，不从 NVN 重建一套 Vulkan descriptor-set。
- 普通分配 BO + 生命周期内稳定地址先行。VBO、任意 guest hostptr 导入不是前置条件。
- 单 native context、sysmem、完整必要缓存清理先行；A7 concurrent binning 明确关闭。GMEM/尾 flush 合并后置。
- 通用资源身份使用 canonical backing/span + mapping snapshot；不把 CPU 指针或 PoolId 当跨 pool 无别名证明。首次只支持受控 backing，host 访问采用显式 lease。
- guest 权威 command storage 与派生 IR 分开。私有 typed profile 的测试成功不是原 NVN command 字节布局兼容。
- 不静默填零 query/event，不静默丢 unsupported draw，不用场景名推导 discard；qualified asset/画质优化单独 profile。
- 独立实验 native client 与原有未修改 reference 程序分别运行。运行中不能因为遇到一个 unsupported API 就用另一个 Vulkan device 接着执行。

## 2. 阻断表

“阻断”指不能宣称该级别完成，不是所有编程工作停工。

| ID/缺口 | 精确决策测试/证据 | 默认与失败策略 | 解锁对象 |
|---|---|---|---|
| B01 目标 device tuple 尚未固定 | 记录 SoC/chip-id、KGSL版本、内核 build、Android/API/ABI、驱动 build-id、可用 heap/cache 属性；运行普通 BO→GPU sentinel→完成→CPU 读回 probe | 只列 compile-qualified；不挑一个 GPU 家族号就默认全部支持 | V05–08 device pass，不阻断 host/build |
| B02 f830 产物是否对应公开补丁 | 已找到 `kxn/d2r-mobile` 的 f122a40c… host-import 与 5c8da4df… host-image 分支完整 patch/builder；5c8 是 combined patch，不能再叠加 f122。收集当时 Mesa base、补丁 hash、构建命令/编译器、二进制 build-id，重建并逐项对照行为 | 不再说“源码不存在”；在 provenance 未闭合前只把公开 patch 当候选，不能宣称复现 f830 | 共享 guest import/native image fast path，不阻断普通 BO |
| B03 ADDR import 的缓存/生命周期与别名 | 同页不同 cacheline、同 cacheline CPU/GPU部分写、CPU只读、双 alias、多线程 lease、提前 unmap/free、失败注入；读写均有受控同步和预期 bytes | 关闭任意 userptr；受控 BO/显式拷贝。IOCOHERENT 不能豁免 GPU CCU/UCHE或 ownership | guest 原页共享路径 |
| B04 对象真实可写范围/函数 ABI | 对照目标 SDK/reconstructed declarations，并用 ARM64 canary harness 验证对象写范围/参数与返回；特别核查 GetMemoryCallbackData 候选错误 void、QueueErrorInfo 未知尺寸 | sidecar 存内部状态；未核实 wrapper 不可执行；不能 sizeof(dummy) 清“真实”对象 | L0 对应调用、D2R loader |
| B05 Copy/Call/handle lifetime 与 patch | 录制 A；Copy/Call 至 B；结束后修改/复用/释放 A，含嵌套 Call；对照参考 NVN 行为和目标游戏实际不变性证据；GetCommandMemoryUsed/直接patch偏移另测 | 私有 typed profile 明确 shallow Copy、保活源/子块，不能泛称 NVN；真实语义未知阻断该模式 | 真实 D2R command profile；LM3另 gate |
| B06 descriptor 消费时点 | bind→register slot A→end→register B→submit；submit 后/执行前再改；CPU raw pool write、GPU生成handle分别测试 | 不一律 record-time snapshot；按已核实消费点生成版本，无法确定则拒绝该时序。延迟到提交不是普遍正确答案 | 稳定 descriptor 通用语义 |
| B07 shader 输入到规范 NIR 的桥 | 冻结 D2R 实际 Program 数据/metadata，指明原 translator 或离线产物来源；VS/FS IO、CB/texture映射、specialization验证；同一个 shader在参考路径输出对比 | synthetic NIR仅标 compiler probe，不宣称能接 NVN binary。ir3不是 Maxwell binary decoder | L2，不阻断 clear/NIR triangle |
| B08 native root/bank 与 linker | 用固定私有 layout schema验证 bank/slot、root UBO、SSBO/resource lowering、VS/FS varying、VFD/SP；含 active slots边界、动态 handle、各stage联合使用负控 | 按 gpu-execution-contract 小固定 ABI 起步，超范围显式 unsupported；不承诺全 slot容量 | V09–11、真实 shader 集合 |
| B09 CPU read/write observation | GPU写native image后CPU直接读、CPU写回旧快照同值、两writer同页、被保护范围跨pool，测试含只读观察；验证 fault路径不递归/死锁 | 初版显式 host lease；写保护+snapshot diff 不是通用 CPU read/write observer，无法托管的 guest访问不进入 qualified 模式 | 真游戏共享内存/图像 |
| B10 raw↔native图像表示 | 随机pattern、tail mip、row padding、layer/sample、partial raw overwrite、buffer/image同backing交叠；GOB布局与CPU参考比对 | 先限定格式/单sample，native assets独立 manifest。BC→ASTC后不能反推原BC字节；保留原canonical bytes或拒绝raw观察 | L2对应texture/RT；通用图像后续 |
| B11 WaitSync 的 fence epoch锁存 | 同 sync对象连续 fence、wait录制后再fence、wait在队列执行前替换、跨queue；比较真实 wait锁存版本与stage/可见性 | 单队列内部依赖可测试，公开WaitSync模式未知不得空实现或任意锁存最新值 | 实际触发的 Sync语义 |
| B12 Event mode/location/flags与原子性 | 获取真实调用参数，测试 WRITE/ADD、CPU/GPU并发、wait→reset→signal→wait、大小/地址对齐；确定原子域与值比较规则 | 不用host普通load/store模拟GPU ADD；不把所有WaitEvent删掉。未覆盖mode拒绝 | 实际Event路径 |
| B13 query/counter枚举与输出ABI | 捕获实际 ReportCounter类型/报告结构、reset、timestamp单位/位宽，分离 begin/end/写出；参考读回比对 | 不用submit完成timestamp伪装所有GPU timestamp；未核实输出结构不写guest内存 | D2R实际query，BotW未来用途 |
| B14 提交部分失败/不确定结果 | mock 第N个IB/aux动作失败、内核接受后用户态失败、device lost；记录 accepted-prefix/unknown receipt与refs | 提交前失败可回收；接受/unknown不重放、不提前free。详见 memory-contract 的 submit journal | 所有可靠提交，不等GPU才写模型 |
| B15 Android WSI外部所有权 | 固定窗口buffer格式/修饰符/import支持、acquire/release sync_fd、resize/suspend、surface销毁时未完成提交 | offscreen先行；不把present成功等同显示完成；QueueSignalReleaseImageANDROID 零 wait 可返回 -1，不表示 raw submit 已完成，也不临时换Vulkan device | L3窗口，不阻断L1/L2离屏 |
| B16 GMEM binning/once-only副作用 | 同轨迹1tile/Ntile/binning/sysmem；shader SSBO/atomic/XFB、query/event必须次数相同；附件溢出可退sysmem | 默认sysmem并关concurrent binning；无sideeffect与反馈的窄段先开放 | 第二阶段GMEM收益 |
| B17 L2实际调用闭包 | replay manifest包含初值、跨帧持久资源、shader、descriptor变更、外部依赖和观察点；首个缺失参数/初值必须拒绝 | 静态216名称不是运行闭包，42k帧源码注释不是本次trace；没有完整包则只标synthetic | 真D2R小核心，不承诺前12阶段跑全游戏 |

## 3. 不能用“保守 fallback”藏起来的问题

1. unknown alias 若无法证明 backing 独立，不能造两份各自私有的 view copy：同一 draw 内两个 storage 地址的 atomic/读写交叠会立刻破坏语义。允许慢路必须维持共同物理身份/顺序，做不到则 unsupported。
2. 消费时点未知不是“多等一会即可”：晚于应锁存时刻读取 descriptor/Sync 也可能读到错误版本。必须 oracle 或明确 profile 限制。
3. GPU native write 后 CPU部分写无法仅凭旧 snapshot差分恢复；即使CPU写的值等于旧snapshot，也可能是在覆盖GPU新值。CPU取得写lease前物化当前字节并处理同cacheline ownership。
4. unknown submit outcome 不能自动重试：同一 draw/atomic可能已执行，重复会改变结果。保留 receipt/refs直到可信终止或context销毁确认。
5. 简化 shader不能作为不支持输入的fallback；clear/triangle演示与真实 NVN Program是不同等级。

## 4. 性能问题的明确决策规则

- **是否先做 VBO**：默认不做。先报告真实跨window slowpath次数、import页驻留、patch地址/额外copy时间。只有该项成为可观察主要成本且组合probe通过，才独立立项。
- **是否先做 GMEM**：默认不做。先完成同输入 native sysmem与reference对照，分离 CPU translation、GPU stall/bandwidth。CPU绑定未消除前不以GMEM掩盖热点转移；游戏目标所必需的 WSI 和 sysmem replay 资格先于 GMEM优化。
- **是否保留 Vulkan 冷依赖**：保留，不作为失败。第一目标是 hot draw不经 Vulkan对象和重复资源整理；后续只有维护/体积/启动成本证据足够，才拆冷初始化。
- **是否保留 mprotect**：不是二选一口号。受控API/lease可直接记发布；不受控CPU访问需经过经过验证的observer。没有完整 observer 不能仅为降低signal数关闭它。
- **是否允许 D2R专用优化**：可以，但必须是独立profile：输入manifest、资格predicate、失效动作、降级、独立计数和golden。场景名/上一帧行为不能独自成为正确性证明。

## 5. 以后获准实施时的起点，而非当前执行任务

当前不执行以下工作；若用户以后授权独立实验实施，无需等待硬件答案即可验证 V01–04、mock KGSL receipts、descriptor byte fixtures、compile manifest和错误路径；并从现有D2R源码导出“实际已知参数模式”而非只统计函数名。在同一未来授权范围内，可同时构建V05–11；其GPU测试保持pending。B01/B07/B17应最早取得答案，因为它们分别决定设备资格、真实shader接入、D2R范围。B02/B03/B15可并行，但不挡离屏闭环。

版本域另行说明：内部合同 header v3 不等于 bridge API v1，也不等于 shader ABI_1。编译该 header 仅证明声明可编译，不证明 Mesa抽取/设备路径构建通过。

每个未决项最终只能变为：证据支持的具体能力、清楚的profile限制、或者带失败行为的unsupported；不能最后变成一句“应该没问题”。
