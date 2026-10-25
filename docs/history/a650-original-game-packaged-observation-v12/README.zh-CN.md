> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始D2R ARM64 v12：明确packaged layout边界

HEAD516540d加构建record逐hash记录的sRGB20文件变更，全程保持code与HEAD冻结至runner终态。正常原始3.3.94012 NSO重建、host/APK与部署前完整verify，67实际编译源含36原host/HLE，ELF仅m/dl/android/c；复用46项RomFS前逐hash完整核验。同设备普通APP_UID，原模块及nnMain实际执行，无CPU模拟/旧renderer/游戏binary叠补丁。新sRGB24公开GPU资格在本轮host构建期间独立完成，不计为游戏绘制。

本轮仍在v11同一GetStorageSize(errno95)边界，但新增只读观察明确根因条件：fields=0xff、device_live=1、source_policy=1、flags0、target1、format68(BC3)、128²、depth1、levels8、samples0、stride0、packaged=1，实际packaged_bytes=02:00:00:00:07:00:01:00。裸canonical计算器同元组已能返回payload24064，故不应盲改mip限制；当前source_builder_layout拒绝一切非NULL packaged，是这次实际拒绝条件。33次builder存储观察均不改变sticky errno、refs、资源或guest字节。

两LE32为2与0x00010007，与公开MIT BntxLibrary写入模式以及low3 BlockHeightLog2存在对应；这不是完整NVN8B规范证明。后续独立defaultOFF实验profile仅接受公开writer的确定模式/BH范围，保留原8B并用显式BH+作者地址/extent对照验证。未知marker/upper bits必须95；不清packaged、不自动退默认，不凭game/shader identity放行。实验正式GPU和真实game验证仍待做。

本轮无真实游戏QueueSubmit/Finish/Draw/Present或首帧。非timeout、OS退出码unknown(Activity无waitpid)、runner finally force-stop并确认退出。原始/加工NSO、shader/control/ASM没有归档；所有hex=代码快照按块长度/SHA替换，packaged8B作为布局元数据明确保留。
