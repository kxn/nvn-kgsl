> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公共 BUFFER 首轮失败：Copy/Call 交错后的队列继续

实际应用APP_UID/A650，原C1完整256B和两项真实资源。phase0 Copy、phase1 Call各一次GPU提交/完成等待及颜色检查通过；phase2 Copy句柄第二次重放在提交前返回ENOTSUP，实际总提交/完成等待均为2。不是首Call失败，也不是已通过完整48次矩阵。

首次Call激活queue.call_queue_active，此后句柄必须有语义序列。旧CopyCommands只复制字节和部分元数据，command_capture_unknown明确插入NODE_UNSUPPORTED，因此phase2被正确拒绝。修复是通用flat状态/绑定/native节点独立复制和命令偏移重定位，不能删除未知节点或停用队列延续检查。复验见相邻a650-public-buffer-sampled-v2。

保留原失败证据及两份完整raw，并逐项重新解压核长度/哈希。最终force-stop=0、pidof=1且输出为空，进程退出。未执行真实游戏模块，无游戏帧。
