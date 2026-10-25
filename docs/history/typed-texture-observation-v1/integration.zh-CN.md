> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 当前分支接入验证

三文件候选已提升到当前独立分支，正常 `check_program_observation.py` 使用当前 bridge 源码、最新独立声明 names 头与实际 NDK API29 编译/链接通过，记录在 `normal-host-proof.json`。`README.zh-CN.md`、`candidate-freeze.json` 与两个 audit 文件保留候选冻结时的事实，不能将其“未修改生产代码”解释为当前尚未接入。

这是参数观察能力，不增加 NVN 实现数、资源类型支持或游戏运行里程碑。此前真实 D2R v4 不含这份观察代码；下一次必须从正常重建的新 host/APK 获取真实绑定证据。只读取有明确 ABI 的有界公开句柄数组，不解析 opaque 纹理或 view，也不调用额外 getter 来改变原程序行为。
