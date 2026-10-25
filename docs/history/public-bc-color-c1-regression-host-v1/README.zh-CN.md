> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

23dffb7源码在合入公开BC组合/分离texture-sampler资源查询后，使用既有正常check_pipeline_enqueue.py重新验证原color及C1采样器无关路径。两san HOST快照/批次preflight/资源owner检查及实际Android链接通过。新BC实际24次GPU已另行归档，本回归没有GPU/game执行，不扩大C1像素资格。
