> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始 D2R v15：RGBA32F BUFFER 创建成功，下一处为 packed HDR source 格式

固定提交 `3e49ec5`，从未改写的 3.3.94012 原始 ExeFS 经正常入口重建原 ARM64 模块及原宿主/HLE，正常 APK、部署前 verify 和全部 46 项 RomFS（8,973,547,258 字节）核验通过。Adreno 650、普通应用 APP_UID 实际执行原始模块及 nnMain；没有 CPU 模拟或旧 Metal/Vulkan renderer。

v14 的 BUFFER 阻塞已实际越过：format46/RGBA32F、width65536、flags0、pool offset18002624 的 SetStorage、Initialize(result1) 和 RegisterTexture(id701) 均成功。全程观察到 18 次成功纹理初始化、445 条纹理注册及 10 条 CopyBufferToTexture 录制日志；这些不是 GPU 完成数或新增入口数。

下一首错为 `nvnTextureBuilderGetStorageSize` errno95：flags12（IMAGE|COMPRESSIBLE）、target1（2D）、format63（R11G11B10F）、2880×1620×1、单 mip、samples0、stride0、非 packaged。既有 immutable no-NVIDIA-compression/Generic16Bx2 source policy 已处理 flags12；缺口在 source 存储格式表遗漏了 packed 颜色格式。后续按独立格式元数据扩展存储族，并保持 GPU 消费资格单独验证。

没有实际游戏 QueueSubmit、Draw、Present 或首帧。非 timeout；最终 force-stop 并按 shell-v2 的 pidof 返回码及空输出确认退出。Activity 没有 waitpid 观察，不能把日志中的预期 exit78 当作已观测 OS 退出码。

仅归档构建、执行及资源元数据。不含原始 NSO、加工 binary、shader/control 或 ASM。所有完整 `hex=` 代码快照递归替换为长度与 SHA256，保留 packaged8B 的冒号布局元数据。
