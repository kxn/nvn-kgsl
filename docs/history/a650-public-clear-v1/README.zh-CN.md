> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公共 NVN 原生清屏真机资格

普通应用 APP_UID 在替换机 Adreno 650 上执行独立 KGSL 后端。正常源码、工具、输入与 APK 哈希准入通过，使用原有签名 install-r；完整读回经独立 HOST oracle 验证，结束后 force-stop 与空 pidof 均确认。

22 格式、67 场景、两种 origin、ordinary/Call、同句柄两次回放，共 536 次真实 QueueSubmit/Finish。覆盖通道掩码、裁剪与空区域、子命令公开 Finalize、父目标继承、2880×1620 R11；只由 GPU 写入颜色，CPU 只打包一个 texel。完整原始 public pool 共 203259904 字节逐字节匹配，另检查命令和 control 内存不被改写。静态常量库只公开一个适配入口，与 FDL 原生静态库合链通过。

本批使用公共 NVN 调用者；不执行真实 ARM64 游戏模块，不证明 D2R Draw/Present 或首帧，不新增 NVN 公共入口计数，也不构成性能结论。Gather、真实深度附件和完整原游戏资源/图形流程继续推进。ZIP 每个成员均解压后重算大小及 SHA256；原始记录与失败记录未重写。
