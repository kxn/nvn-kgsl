> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 标准 NVN 到 AHB 的直接绘制进展

2026-10-23，独立分支 codex/nvn-turnip-study-20261023，测试机仅 DEVICE_SERIAL（pipa / Adreno 650）。

本次标准 TextureBuilder / TextureInitialize / SetRenderTargets / Draw / QueueFinish 调用已经直接写入导入 KGSL 的 AHB 像素页。双目标与三目标测试分别完成 8、12 次提交，独立 Python oracle 检查共 327680 个颜色通道；包括红色、半屏绿色、完全裁剪与 RGB 渐变。各目标底层 dma-buf 身份不同，重复使用同一目标 4 次，退出时全部正常释放。目标没有 CPU 写入或图像复制，没有绕过 NVN 的原始 Draw。

证据：a650-nvn-external-n2.json、a650-nvn-external-n3.json 及同名前缀的 20 份 RGBA。每次通过 worker 线程运行，使用独立 nonce 和全新目录；APK 来源与工具指纹、安装包哈希、实际 fd/span/身份、guard 和只读检查保存在记录内。独立校验器额外拒绝 nonce、线程、CPU 写入、清理状态、别名以及像素篡改（每组 7 项）。

外部图像导入是明确的宿主接入接口，不是新的客体 NVN ABI；目前只资格化 A650 的 64×64 RGBA8、pitch256、指定 AHB usage 的完整目标。外部池拒绝普通 CPU Map、shader/descriptor/command arena 和 Buffer 用途；纹理及窗口 pin 必须结束后才能释放。所有权有疑义时保留整个上下文，不重复释放。主机所有权测试 212 项通过，故意保留的故障上下文在隔离子进程结束；该结果不是 GPU 通过证据。

同时修复 KGSL dma-buf 导入失败的所有权：分配 ioctl 或 GET_INFO 后失败且不能确认释放时，设备保留 id/未知 owner，直到确认可安全清理；检查导入与 GET_INFO 两处只读/安全标志。实际 backend 的 37 个主机用例、4297 次检查以及 Clang 函数类型与 GCC ASan/UBSan/泄漏检查通过。Queue 的 typed 参数观察也已加入，但真实 D2R 尚未执行到 Queue。

本次通过的是 NVN 离屏 AHB 目标。既有原始 AHB SurfaceControl 出图资格测试已通过，但本次尚未贯通标准 NVN WindowBuilder/Window/Acquire/QueuePresent 与窗口销毁重建；这些接口仍在独立候选中开发。不得将这 20 次提交表述为 NVN Window 或真实 D2R 首帧。

真实游戏最近证据仍是 a650-d2r-coherent-pools.json：原始 ARM64 SDK、构造器和 nnMain 已执行，完整原始 RomFS 校验后运行，0xa1、0x64、0x22 池建立成功，停在 1MiB 的 0x14 CPU_CACHED/GPU_UNCACHED 池。尚未执行真实游戏 Draw。本轮没有修改游戏逻辑或加工 binary，也未引入 Vulkan。接下来先资格化 Window 的 acquire/释放和两代 Surface 生命周期，并通过严格的 CPU-only 0x14 诊断策略记录真实游戏首次 GPU 消费点；该策略不能冒充 GPU_UNCACHED 语义实现完成。
