> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 无Finalize builder地址再利用：根源码资格

中央registry允许显式资源Initialize替代无强引用的旧builder元数据。构造输入先快照，再创建新node及回收旧node；OOM保留旧builder；live资源和强引用builder继续拒绝。8类builder及Texture三条构造路径、Window借用数组、外部pool导入均纳入生命周期。声明不保证同时live且重叠对象合法，本实现明确是宿主opaque生命周期策略；不扫描栈、不猜作用域、不改公开ABI。

当前根源码正常工具通过两套sanitizer、8 builder kind/9源地址alias、旧45 getter/强制ABA、descriptor/pool/source快照保活、完整external module/core2/3/ordinary/quarantine和Window2/3槽回归，并完成两种BLOCK配置的实际API29 KGSL严格链接。HOST结果没有GPU或真实游戏执行，不增加入口，仍406。

旧external fixture两处调整保持有效覆盖：height63已受当前布局支持，非法样本改height0；无Begin/current的手工record不合法，改用正常command/control pool与Begin后验证NO_ACCESS目标拒绝。没有删除module检查，没有回退production显示尺寸支持。候选只说明来源，以normal-root-check.json为本分支验证。
