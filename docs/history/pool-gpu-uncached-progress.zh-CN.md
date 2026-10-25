> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# GPU_UNCACHED 池的受限 CPU 能力与真实游戏新边界

2026-10-23，仅新机 DEVICE_SERIAL，Adreno650。独立分支 codex/nvn-turnip-study-20261023。

精确 0x14（CPU_CACHED|GPU_UNCACHED）目前实现真实池建立、原页导入、CPU map/Buffer、缓存维护与生命周期；GPU 策略单独标为 UNQUALIFIED，所有潜在 GPU 消费均在修改引用、descriptor、command 或提交之前拒绝。已知虚拟地址范围的重叠别名也拒绝，不能经第二个缓存池绕过；不声称发现任意物理别名。本实现没有证明 GPU_UNCACHED 的硬件缓存语义。

原生诊断提供首个池消费拒绝的身份、原始 flags、策略、offset/size 和 API 名称，不制造 guest PC；首个 sticky 错误保留，clear_error 同时清除上下文。typed D2R 桥在退出前打印该上下文，runner 保留结构化观察。0x12 和 0x14 的修饰组合仍不支持。

主机检查：1075 项、22 个策略拒绝、0 次 GPU submit；Clang function trap 与 GCC ASan/UBSan/泄漏检查以及 NDK 严格编译/链接通过。检查还覆盖真实 typed TexturePool/SamplerPool NULL 解绑和引用归还，避免门禁改动导致空指针回归。原 coherent/noaccess 测试分别 20679/37182 项、两种 sanitizer 回归通过。typed bridge 四组测试各以两种 sanitizer 通过；两个 runner 单元检查确认诊断观察不会被写成游戏运行通过。

真实原始 ARM64 D2R 已通过原 host 的 SDK、构造器及 nnMain，完整 RomFS 46 文件/8973547258 字节逐文件校验。a650-d2r-gpu-uncached-first-use.json 及 stderr 记录：既有 0xa1、0x64、0x22 后，1MiB 0x14 初始化成功；接着 744488960 字节（710MiB）0x21 初始化成功；当前在 2MiB 0x12（CPU_UNCACHED|GPU_UNCACHED）MemoryPoolInitialize 返回 ENOTSUP、exit78。还未发生 0x14 的 GPU 消费，因此 pool_access_observations 为空是正常边界事实。

本轮没有执行真实游戏 Draw，没有真实游戏首帧。游戏代码仍由 ARM64 硬件执行；只修改独立 NVN 层、桥和构建工具，未修改游戏逻辑或叠加 binary 补丁。构建入口仍验证原 NSO、完整改写记录和产物；新增 external_image.c 已纳入精确编译清单。清单遗漏导致的第一次失败构建未部署，保留在旧 d2r-bootstrap-observe 目录；当前有效 host 与 APK 使用新的 d2r-gpu-uncached-observe / a650-gpu-uncached-host-apk 目录。

下一步先贯通标准 NVN Window 与两代 Surface 生命周期。0x12 可研究沿已资格化 WB+mandatoryIOCOHERENT 导入实现 CPU-only 能力，但所有 GPU 用途继续拒绝，直到得到硬件依据和真机资格证据。不能将 constructor 前进写成完整缓存策略完成。
