> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始D2R接入：完整目录与深度功能组合 v20

原始ExeFS经仓库正常构建入口重建，HOST/APK产物及运行前校验通过。原签名install-r，46份RomFS共8973547258字节全部核验；保留原游戏数据。main/sdk/subsdk0由原装载/HLE路线执行ARM64硬件指令，进入nnMain。实际编译源和ELF依赖见normal-link-audit.json，未接入Metal/Vulkan渲染器。

本次在源码HEAD322f8e2冻结期间运行。观察到16次成功TextureInitialize、441次TexturePoolRegisterTexture、5次CopyBufferToTexture及4次单阶段计算ProgramSetShaders；这些是日志调用次数，不是独立完整NVN能力数量。

明确失败：TextureBuilderGetStorageSize，errno95。输入format68、128×128、8mip，packaged layout八字节02:00:00:00:07:00:01:00。v20关闭原defaultOFF BNTX writer interop实验，不能将其视作通用NVN布局规范；v16曾在显式启用该实验时到达34个纹理及首个图形source缺口。选项不同，本次没有证明游戏边界前移，也不据此把旧实验写成通过或GPU回退。

没有原游戏Draw、QueueSubmitCommands、Present或首帧。退出状态没有Activity waitpid证据，保持unknown；原日志的exit78只是请求终止值。runner最终force-stop返回0，末次pidof返回1且stdout/stderr为空，严格确认退出；无本轮工具进程留在设备。证据中的所有code hex均已用长度和SHA256替代，原始输入/重写规则/正常构建记录可核对。
