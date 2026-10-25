> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 当前分支验证

七个已有强类型声明与原SDK逐字读写依据的入口已接入：六个border int/ui别名与BuilderGetDevice。没有新增无声明的SamplerGetDevice，也没有改变GPU border消费能力。当前注册入口总数344；这不代表完整NVN语义或游戏帧。

root额外核验新Device复用已释放私有对象的情况：Initialize和Builder登记同时核公开weak key，避免Builder意外转向另一个caller-owned Device。原有GetDevice在Finalize后仅返回保存的公开指针，不解引用已释放Device。当前源码三类sanitizer正常检查通过，完整命令与输出见normal-host-runs.json；候选冻结证明与当前分支证明分别保留。真实D2R v6构建/运行闭包也包含这些源码，已越过此前Sampler初始化边界，但没有把它当新GPU采样模式的资格。
