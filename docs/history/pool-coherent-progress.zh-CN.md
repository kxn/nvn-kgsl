> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# CPU_UNCACHED：同页硬件一致性实现与新机验证

独立分支 `codex/nvn-turnip-study-20261023`；仅使用 `DEVICE_SERIAL` / A650 `0x06050002`。原 ARM64 D2R 在 [前次运行](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-modes-bootstrap.json`；未随公开仓库提供） 实际请求 400 MiB 的 `0x22` pool 后停止；这里解决这一明确边界，没有修改游戏预算或调用参数。

## 实现政策与依据

新接口 `nn_bo_import_coherent` 仍导入调用者原有页面，保留原 CPU 地址和 WB 映射，明确请求 **WB + IOCOHERENT**。它同时检查 import 与 GET_INFO 返回的 cache mode／一致性位、大小、id、GPUVA、可写／非 secure 属性；缺位或属性不符即拒绝，不回退普通 WB、不换 allocator、不重映射 heap、不建 shadow。

这是一种以硬件一致性提供 NVN CPU_UNCACHED 可见性的后端实现政策，**不是声称 CPU PTE 为 NC，也不是将 KGSL cache mode 直接解码成 NVN flags**。CPU 到 GPU 和 GPU 到 CPU 的有序交接不需要调用者显式 cache maintenance；原有 GPU 侧 descriptor／UCHE／render-target cache 同步仍保留。

MiCode pipa-t-oss pinned 源的 ADDR import 保留 IOCOHERENT 并 pin 原 pages，MMU capability 决定是否清除此位，IOMMU mapping 最终设置 IOMMU_CACHE。公开源不被冒充为当前运行内核逐字节相同；[源路径与 SHA](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `coherent-kernel-route.json`；未随公开仓库提供） 和 [解释](coherent-kernel-route.md) 与实际探测分开保存。

## 分层真机证据

1. [阶段一：仅导入 metadata](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-iocoherent-import-metadata.json`；未随公开仓库提供）。普通 APP_UID 请求 `0x8c000000`，import 和 GET_INFO 均返回 `0x8c0c0060`，保留 WB 与 IOCOHERENT；8192B／2048 个原 CPU word 检查通过，清理成功。**零 GPU 提交**，该记录明确不宣称 shader coherence 或完整 NVN uncached 资格。GET_INFO.useraddr=0 是该 UAPI 的行为，不拿它伪造 pointer identity。
2. [全 coherent pool 图形资格](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-coherent-uncached-texture.json`；未随公开仓库提供）。标准 NVN API 使用 `0x22` data/descriptor/command pools 和 `0x62` source-shader pool；12 个 pool Map 和6个 buffer Map 等于调用者原地址。原始 sampled VS/FS 执行原5次提交加12组模式用例，共17次，显式 public cache 调用为0，确实跳过103次 Flush 和204次 Invalidate。
3. [混合 NOACCESS pool](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-coherent-uncached-noaccess-texture.json`；未随公开仓库提供）。其余11个 pool 保持 coherent、独立 GPU-only pool 仍为0xa1，原 shader 实际采样该池。17次提交、12组模式以及原有 descriptor／同ID跨pool／资源引用检查都通过；NOACCESS 的 public cache 调用也实际跳过，没有把未调用计作通过。
4. [原 cached 路径回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-coherent-cached-regression.json`；未随公开仓库提供）。另17次提交全部通过，保留实际307次 public cache 调用，验证原 WB 路径没有退回已失败的 legacy import 或丢失 cache mode。

两轮 coherent 测试在每次提交前通过 volatile 读取核对输出原 clear 值，共69632个 word；Finish 后从同一 CPU 指针观察真实 shader 输出，不执行 Invalidate。纹理、顶点和 UBO 在录制后更新，四组不同输入和独立闭式 oracle 检查 CPU→GPU 读取；输出和12份模式 readback检查 GPU→CPU 写入。普通用户空间不能保证每一次访问的 cache residency，因此只报告实际预读和可见结果，不编造 cache miss/hit 测量。

每轮12模式各196608通道、原4组65536通道、独立pool身份16384通道通过；模式 mask0xb2c，严格镜像／正负Z可见性／far clear 独立复核。readback 全部保留。没有CPU翻转、补图或写结果代替GPU。

## 资源所有权与离线检查

支持的 access subset 新增0x22/62/a2/e2；COMPRESSIBLE仍使用既有native-uncompressed资格政策。public Map可见、有效 pool/buffer mapped-cache调用合法NOP，错误范围仍拒绝。GPU_UNCACHED0x12/14、physical/virtual等组合没有借此放行。

metadata检查失败但已产生 kernel id 时，释放失败的 owner留在device failed-import列表，明确Finalize时才重试释放；有不确定id的 allocating ioctl失败保留整个fd寿命，不重试原mutation。整fd销毁前要求没有普通BO或queue。close失败保留poison owner，避免NVN对象指向已free device。正常 cached importer同样复用这些回滚保护。

- [真实core＋host memory backend](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `pool-coherent-host-proof.json`；未随公开仓库提供）：20682项、4种新增flags、拒绝／零fallback／Map与NOP／回滚／refs检查通过；原8种pool回归保留。它不执行GPU。
- [真实KGSL源码＋mock ioctl/close](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `kgsl-coherent-host-proof.json`；未随公开仓库提供）：26场景／4240项，含双返回flags校验、GET_INFO错误、释放失败保活、不确定id、close失败及普通WB两向cache ioctl；ASan/UBSan、function-type trap、ARM64严格编译通过。该检查发现UAPI的 signed `1 << 31U`，已明确本地修正为unsigned `1U << 31U`，ABI位值不变；没有关闭sanitizer或排除普通WB检查。
- [borrowed map释放失败回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `kgsl-coherent-map-retry-proof.json`；未随公开仓库提供）：最终4242项还覆盖正常imported BO释放失败后保持原Map／owner、明确重试成功；不提前清掉调用者仍存活的指针。两种host sanitizer和NDK严格编译通过。
- [typed桥回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `coherent-typed-bridge-proof.json`；未随公开仓库提供）：真实core继续在不支持的GPU_UNCACHED0x12具名退出；false/zero返回、sticky和原HLE_FORCE路径不变。

物理CPU cache属性、并发读写／atomics、任意shader/layout和完整NVN同步模型没有被这些有限用例概括为全部通过。当前图形范围仍为64×64、受限RGBA线性布局与原shader。真实D2R的下一调用边界和真实游戏首帧另行记录；离屏资格不代替游戏画面。

## 真实 D2R 已通过0x22，下一实际边界

[本轮原 ARM64 执行](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-coherent-pools.json`；未随公开仓库提供） 在完整原始 RomFS 下再次通过 SDK 初始化、constructors 和 nnMain。原参数512 MiB0xa1、10 MiB0x64、**400 MiB0x22** 的三个pool均Initialize返回1，没有修改原storage、预算或游戏逻辑。随后原caller实际请求 **1048576字节／1 MiB，0x14（CPU_CACHED | GPU_UNCACHED）**，在MemoryPoolInitialize ENOTSUP退出78，见 [参数与终止日志](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-coherent-pools-stderr.log`；未随公开仓库提供）。没有guest资源错误、没有超时，本次host已确认自行退出。

本次仍是普通UID run-as CLI 经原loader/HLE/宿主门执行真实模块，不是Activity中的D2R出图；没有游戏Draw或真实首帧。原RomFS仍按46文件／8973547258字节逐文件核验后复用。GPU_UNCACHED需要独立的GPU侧访问／缓存政策与资格，不能由本次CPU一致性结果推断通过。通用Window与caller queue-memory工作继续分开推进。
