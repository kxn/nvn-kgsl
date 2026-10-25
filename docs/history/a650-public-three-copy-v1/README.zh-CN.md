> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650 公开 NVN 三类复制实际 GPU 验证

在替换测试机 pipa / Adreno650 / Android14 上，以普通应用 APP_UID 运行当前独立 KGSL 后端，公开 CopyBufferToTexture、CopyTextureToBuffer、CopyTextureToTexture 三类接口共234场景、468次真实 QueueSubmitCommands/QueueFinish 全部通过。正常工具从当前生产源码构建、签名，安装后复核 APK 哈希；原有游戏资产未删除。测后 force-stop 与 pidof 已确认退出。

37种原始块格式覆盖 full/partial、mip/array、自动 block-height、部分边缘压缩块、非紧凑正 row/image 字节 stride、同池真实不相交范围、独立池以及原 BC1 64×64/2048B布局。每个场景公开录制后再通过 CopyCommands 克隆，原 handle/录制对象先 finalize，再对 clone 重放两次，第二次改变原始输入；不调用 probe 私有 encoder 或 raw submit。

468份完整实际读回共27,541,504B，与独立原始坐标/输入模式 oracle 逐字节相等，容差0；其中有效写3,674,352B，未改变区域23,867,152B，包含源、padding、未选 mip/layer、邻接与 allocation guards。另核对 command/control 原样61,341,696B。C 和 Python 分别验证，预期结果不取自前一次 GPU 输出，不是 roundtrip 验证。完整原始字节 lossless 保存于 raw-readbacks.zip，每文件长度与 SHA256 见 raw-manifest.json。

资源使用普通 CPU_CACHED|GPU_CACHED 导入 caller storage（0xa4），copy flags0、相同 view format、canonical GOB。未证明游戏0xa1/CPU_NO_ACCESS、缩放、格式转换、非零 flags 或 LINEAR 混合。压缩数据只作为不透明块复制，**没有 BC 采样/解压、Draw、显示或真实 D2R 帧**。之前148-job raw helper验证是独立历史证据；不能合并计数冒充本次公开API。

normal-host-check.json 是先行 HOST 两 sanitizer/实际 API29 严格链接及负例记录；正常 APK 构建及源码/工具/编译产物指纹见 normal-apk-build-record.json。evidence.json 保存实际设备、nonce、UID、输出 inventory、全部计数和停止证据。
