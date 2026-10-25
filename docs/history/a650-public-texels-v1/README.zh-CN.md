> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650 公开 NVN Texels 返回可见性 GPU 验证

替换测试机 pipa / Adreno650 / Android14，普通应用 APP_UID。公开 WriteTexels、ReadTexels 及 strided 版本共312次，覆盖37种原始块格式、156个几何场景；另以公开 ClearBuffer 产生一次忙队列，总计313次真实 GPU 提交和等待。FlushTexels、InvalidateTexels 后再次核对；测后停止应用并确认 pidof 为空。

完整实际数据池15,712,256B与独立坐标 oracle 逐字节一致，容差0：有效区域2,449,568B，其余13,262,688B保持原样；只读源6,323,784B以及 cache API 后15,712,256B稳定性均核对。独立指针和同BO不相交范围各78场景；原始输入每次重放变化。忙生产者额外保留65,536B完整实际数据池。313份实际读回合计15,777,792B，lossless存入 raw-readbacks.zip，逐文件长度/哈希见 raw-manifest.json。

普通CPU_CACHED|GPU_CACHED池0xa4；CPU_NO_ACCESS和忙队列各两项拒绝。当前三维拒绝发生于Texels操作（stage3），三维GetStorageSize/Initialize已可通过；这**不证明三维Texels支持**。格式数据仅以不透明原始块处理，不在CPU解压、转换或制造图形结果。本次没有采样、Draw、显示、原游戏模块或真实D2R帧。

normal-host-check.json保存两sanitizer、真实API29后端严格链接和oracle/runner负例。normal-apk-build-record.json记录正常生产源码构建、工具与产物哈希；candidate-freeze.json是资格程序的历史候选冻结证据。evidence.json记录实际nonce、UID、安装APK哈希、完整文件inventory及停止证据。后续源码变化不能倒写这些历史记录。
