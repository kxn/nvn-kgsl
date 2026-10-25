> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始 D2R ARM64 v10：越过 RGBA16F 三维 IMAGE 资源

源码固定 dd2bdb4，从原始 3.3.94012 ExeFS 通过同一正常入口重新生成模块、host 和 APK，部署前正常核验记录及全部产物。沿用原项目 ARM64 硬件执行、装载/重定位、宿主门和系统 HLE；没有旧 Metal/Vulkan renderer 或 CPU 模拟执行。46 个 RomFS 文件完整逐 hash 复核后复用，未改变游戏流程或游戏数据。

普通 APP_UID、pipa/Adreno650/Android14 实际进入原模块和 nnMain。v9 的 flags4 IMAGE、RGBA16F(format41)、target3D、32³/mip1 资源本轮 GetStorageSize=262144、alignment512、Initialize=1，后续多次描述符注册成功。相同形状 flags0 资源创建、注册和 CopyBufferToTexture 录制也成功，继续装载更多游戏资源。

新的首个不支持点为 TexturePoolRegisterTexture(errno95)：format70(0x46)、target4(2D_ARRAY)、128×128×84、7 mip、flags0。GetStorageSize=1204224、alignment512、Initialize=1，id293 的 NULL view 和 id294 默认 view 注册成功；随后显式 viewSetLayers(base0,count1) 的 id295 被拒绝。下一步需补通用层/mip 子资源视图 metadata，而不能让未资格的采样/绘制 consumer 自动继承支持。

没有真实游戏 QueueSubmit/Finish/Draw/Present 或首帧证据。Activity 无 waitpid，OS 退出码 unknown；adapter exit78 仅表明不支持边界。runner finally force-stop 并确认退出，非 timeout。

证据中的所有 hex= 内容按块替换为长度与 SHA256；没有归档原始/加工 NSO、shader/control 或 ASM。完整正常构建、原始输入、重写和链接审计记录保留为历史状态，后续源变化不倒写本次记录。
