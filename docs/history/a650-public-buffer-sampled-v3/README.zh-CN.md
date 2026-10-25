> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公共 BUFFER 最终复验：含多arena语义offset修正

在v2完整48次GPU通过后，扩展旧Copy/Counter回归发现原EndRecording在多arena中重复计算最后chunk长度：实际bytes236而call_limit440。command_close_chunk提交chunk时推进start到used，确保已经累计的区间不再进入实时span。NVN公开内存used查询保持原值，多个chunks、回调与语义leaf偏移一致。

从当前修复后的生产源码正常重建、既有签名安装，在同一Adreno650普通APP_UID再跑完整12格式×Copy/Call×两次重放48次提交/Finish，786,432通道最大误差1、393,216guard字节、1,966,080源raw字节、7,077,888只读字节全部通过，最终进程退出。与v2相同矩阵复验，不能累计为新增96种能力；完整原C1仍256B、图形control仍完整2176B、索引资格仍仅0。此结果不是真实游戏帧。

正常HOST public BUFFER覆盖12正例/27拒绝、Copy→Call→Copy→Call→未知提交以及defaultOFF39项，NDK实际KGSL链接通过。原Copy/Counter/Queue完整双san回归每轮：Copy+Counter556、Counter104、Counter words137、SDKdefaults103、Queue4252、defaults63断言；Android实际后端严格链接通过。原过时Call不存在断言改为确认确切typed入口；多arena明确断言源和目的call_limit==bytes，未移除所有权/未知完成/原有借用检查。

normal-copy-counter-queue-check保留完整指纹和命令；更早一次运行全部功能/链接通过但因补充断言时源文件改变，按设计判为指纹失败，故本目录是稳定源码后的完整正常重跑记录。48raw压缩后逐项重新解出核长度/SHA；没有原NSO/shader/control/ASM归档。相邻v1失败、v2首修证据均保持不变。
