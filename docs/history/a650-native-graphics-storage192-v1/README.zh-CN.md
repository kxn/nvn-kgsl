> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Adreno原生图形存储缓冲整组资格

在新测试机 Adreno650 上，4种真实编译的 VS/FS 资源角色（读取、写回、读取尺寸、稀疏绑定15）×4种VS/FS缓冲偏移组合×3种范围×2种奇数目标尺寸×同一job两次重放，共192次Submit/Finish、576份完整BO读回。4416768个图像通道最大误差0；86016个存储字精确一致；4374528存储保护字节、4233984目标保护字节、18082944只读资源字节全部通过。实际KGSL提交、IR3 shader和FDL缓冲描述符执行；CPU只初始化原创夹具输入，不执行shader或转换图像。

本次源码同时补入默认关闭的FrontFacing及SSBO能力，以及完整原始shader身份目录：6903个身份（2344 VS、4559 FS），只读blob 77105536字节。Program拥有选定原始code/control/C1副本；未知native变体、query、pair或资源仍在Draw前明确拒绝。原始source目录与完整55阶段/96个显式成功pair分离，不能把身份匹配称为任意shader都可绘制。

正常构建证据包含535target IR3构建、现有51身份188对重编译的188份旧native binary一致性、SSBO 4组ON/OFF双san、192次mock提交整矩阵、source完整目录/core/公共12路径HOST、实际ABI1/ABI2与source OFF/ON/深度采样四种NDK宿主构建及严格链接。HOST结果不冒充Android执行或GPU结果；原始source blob及游戏二进制未封存到公开归档。

实际APK使用既有签名保留数据，普通Android应用APP_UID；现有runner核对nonce、文件清单、576份原始数据及严格退出（force-stop0、pidof1空输出）。896份实际构建源码和全部读回逐ZIP成员校验。现有深度采样等共享实现保留。本次不增加NVN入口计数；公共SSBO像素、FrontFacing GPU、真实D2R Draw/Present/首帧尚未完成。
