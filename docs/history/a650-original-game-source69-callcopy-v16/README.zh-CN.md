> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始 D2R source69 / Call-copy v16 真机接入

固定源码 b35e9c8，从原始 3.3.94012 ExeFS 按仓库统一入口重新改写并构建 69 个实际编译单元（含 36 个原宿主/HLE单元），原模块、正常 HOST/APK、签名与完整46个RomFS文件共8,973,547,258B均经过记录核验。设备DEVICE_SERIAL为Adreno650，实际运行于普通应用APP_UID。没有接回Metal/Vulkan或CPU模拟后端。

原游戏模块、原宿主入口和nnMain实际执行；34次TextureInitialize返回成功，相比v15的18次，已跨过R11G11B10F 2880×1620 IMAGE|COMPRESSIBLE存储缺口。日志包含492次TexturePoolRegisterTexture、10次CopyBufferToTexture录制，四个单shader ProgramSetShaders成功。首个双shader ProgramSetShaders返回ENOTSUP/errno95，由桥接明确终止。此结果不能证明新格式GPU绘制或纹理复制已在真实游戏提交。

真实游戏QueueSubmit、Draw、Present均未发生，first_frame=false。原Activity没有waitpid退出码，OS退出码未知；不是timeout，runner最终force-stop并核实退出。程序完整控制身份仍严格匹配，尚未放宽匹配以绕过新shader缺口。

归档保存完整正常HOST/APK来源记录、rewrite/link审计与去字节日志；原始/加工NSO、shader/control/ASM不归档。所有hex=代码快照均替换为长度及SHA256，标量布局和日志计数可审计。计数不是独立API覆盖数量。
