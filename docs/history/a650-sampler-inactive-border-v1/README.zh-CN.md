> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 未使用边界颜色的采样器状态与真机资格

原 SDK 静态字段读写和初始化复制证明，边界颜色是保存的原始四分量状态。当前原生后端支持的 REPEAT、MIRRORED_REPEAT、CLAMP_TO_EDGE 均不消费边界颜色，因此不应因颜色非零而拒绝 Builder 更新、初始化或描述符编码。消耗边界颜色的 wrap 仍明确拒绝；本次不声称边界色表已经实现。

正常 HOST 检查包含 32400 个实际 XML 描述符组合、任意未使用边界颜色的原始位保持（含 NaN payload、负无穷、负零）、Builder/Sampler/注册表快照寿命及既有错误边界。实际真机 v3 普通应用 UID 在 A650 上完成 38 次提交与完成、622592 通道检查、76 次公开边界颜色 getter 检查，以及守卫和只读区域检查，最后进程已停止。shader 与原颜色 oracle 没有改变；没有执行真实游戏模块或呈现游戏画面。

保留 v1/v2 两次失败：都在 qualifier 的旧句柄相等断言处返回 EINVAL，提交数为零。按已独立证明的原 SDK bit32 carrier 标记修正测试端后，v3 才完成实际 GPU 执行。这些早期失败不算 GPU 采样失败或成功。

完整实际记录与构建指纹见 actual-v3.json、normal-apk-record.json，38 份原始读回无损存于 lossless-readbacks.zip。SDK 静态证据只归档位置、哈希和结论，原 SDK 字节及反汇编保留在本机忽略目录。
