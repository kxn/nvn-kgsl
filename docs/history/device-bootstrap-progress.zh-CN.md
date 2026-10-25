> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 设备启动：真实参数、实现与新机验证

2026-10-23，独立工作树 `nvn-turnip-study-20261023`，仅使用新机 `DEVICE_SERIAL` / A650。运行时继续直接使用原生 KGSL；没有接入旧 Vulkan renderer，也没有执行 CPU 模拟器。

## 实际进展

先仅增加 typed 观察，在 lookup 前记录真实参数，保留原先的退出行为。完整原 RomFS 下原 ARM64 D2R 实际传入 `DeviceBuilderSetFlags(0x100)`，随后仍在未实现入口退出78。见 [取证记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-observed-flags.json`；未随公开仓库提供） 与 [日志](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-observed-flags-stderr.log`；未随公开仓库提供）。参数来自调用现场，不从 CONFIGRelease 或静态分支推断。

随后实现精确的 flags 合同：接受0和0x100，SetFlags替换旧值；拒绝其它值时保留原状态。SetDefaults恢复0，Initialize再次核验并将flags快照保存在device，后续builder复用不改变已初始化设备。调试/延迟回收标志没有被mask掉或假接受；GetFlags未知ABI仍未添加。

版本查询采用明确的项目部分ABI标识 `nn-nvn-partial-abi53.105-v1`，返回53/105，定义位于 `include/nvn_native/profile.h`。这是根据已pin重建类型头选择的兼容方言，**不是实测NVIDIA驱动版本，也不是完整NVN 53.105合规声明**。构建与实际初始化日志同时记录partial/strict及源码指纹；未知入口、未知属性和未实现能力仍各自拒绝。原完整初始化函数中两个版本输出未被比较或用于分支，见 [消费者静态证据及选择依据](device-version-followup.md)。没有改游戏版本分支。

真实原 ARM64 D2R 再运行后，`DeviceInitialize`返回1，属性0返回53、属性1返回105；下一具名未实现调用为 `nvnDeviceSetWindowOriginMode`，exit78。其PFN已在原pin头中声明，属于尚未实现的typed接口，不是未知ABI。见 [实际运行](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-device-bootstrap.json`；未随公开仓库提供） 与 [日志](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-device-bootstrap-stderr.log`；未随公开仓库提供）。这证明设备启动阶段推进；没有真实游戏Draw、GPU首帧或Activity中的D2R出图。

## 验证范围

- [正式host语义检查](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `device-bootstrap-host-proof.json`；未随公开仓库提供）：实际core配host内存backend，Clang函数类型检查和GCC ASan/UBSan各通过212项，覆盖21个拒绝flags值、初始化失败回滚/不触backend、device快照、版本单int及guards、错误/生命周期/持引用Finalize EBUSY。它们没有GPU提交，不计游戏通过数量。
- [typed桥回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `device-bootstrap-typed-bridge-proof.json`；未随公开仓库提供）：真实PFN转发、原HLE_FORCE、合法false/zero/float/聚合返回，以及错误前后明确截断。新增bootstrap观察不改参数、不清sticky错误；C适配层返回地址不被冒充guest PC。
- [显式0x100真机GPU](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-device-flags-100-texture.json`；未随公开仓库提供） 和 [SetDefaults真机GPU回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-device-flags-default-texture.json`；未随公开仓库提供）：各5次提交、4组原VS/FS用例、65536个颜色通道及16384个独立pool身份诊断通道通过；descriptor更新、跨pool相同ID、资源引用和清理检查保留。原始readbacks随记录保留。两轮版本查询均有前后guards。
- [资源复用mock检查](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `romfs-reuse-host-proof.json`；未随公开仓库提供）：复用须有本机原始RomFS，排斥再次指定source包；在独立runner锁内检查非symlink目录、完整NUL文件清单、逐文件SHA，缺失/多余/损坏均在host执行前失败。默认复制路径仍有回归。**真机**复用另在实际D2R记录中确认46文件/8973547258字节全部匹配；无需再次拷贝。

两个D2R阶段均保留同一原NSO统一改写、工具/规则/选项与产物验证流程；没有在生成binary上叠补丁。取证和实现后的运行是不同构建记录，未重写历史证据。所有本次probe/host进程均已退出。

## 后续工作

优先落地设备窗口原点/深度模式及录制时快照。现有固定图形模板的正Z用例不足以区分两种裁剪模式，不能作为完整NVN默认(0,0)资格。需显式scalar patch与真实上下翻转、负Z裁剪用例，见 [模式计划](mode-state-next.md)。

并行推进GPU-only pool：原SDK已直接确认CPU_NO_ACCESS的Map合法NULL、非CPU_CACHED的mapped cache操作合法NOP，以及PoolBuilder默认0x21且保留device。实际匿名WB backing须保留并做私有cache交接；不能伪标CPU_UNCACHED。压缩资格采用native-uncompressed策略属于明确的后端设计选择，纹理layout继续逐项资格。详见 [pool实施证据](pool-implementation-next.md)。窗口2/3buffer与资源寿命方案见 [窗口实施合同](window-implementation-next.md)。这些后续实现尚未被本节真机结果覆盖。
