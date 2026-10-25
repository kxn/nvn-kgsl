> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公开 NVN BC sRGB：实际 Adreno650 采样验证

普通APP_UID、pipa/Adreno650/Android14，从公开NVN Program/Draw/QueueSubmit/Finish路径实际完成24次Submit/Finish、12handle各重放一次，24全RT读回。BC1 RGB/RGBA、BC2/BC3 sRGB (0x45..48)，8²/37×21/127×65；39万3216通道，RGB最大差1，alpha严格相等；19万6608 RT guard字节与170万3936只读字节一致。正常source/tool/FDL/原shader归属及APK部署前完整核验；同签名install-r，未清空游戏资源。

输入使用非0/1 RGB565端点(8/31、16/31、16/63、32/63等)及BC1透明/黑、BC2显式alpha、BC3 alpha特殊索引，独立C/Python sRGB闭式oracle。未调用CPU codec或生成CPU转换结果；raw BC blocks经GPU转换至实际FDL LINEAR BC storage，硬件sRGB texture descriptor负责解码。RGB容差1与alpha0容差在运行前固定；实际差1不写成零差。CPU仅为fixture原始输入和丢弃片元背景初始化，不替GPU写活跃结果。

EndRecording后才注册T257/S259，CPU Texture/Sampler释放后继续真实提交；同handle重新注册另一source backing、旧源poison、caller binding arrays按值复制。所有24份24576B完整raw已压缩并逐hash复核。cleanup errno0、无保留资源，runner finally force-stop并确认退出。

原a767 VS/FS为声明0x880 bounded profile，不证明实际游戏完整0x898。资格仍普通2D/mip1/layer1/nearest-clamp/LINEAR RGBA8 RT，不声称array、多mip过滤、3D sampled、ImageStore、真实D2R Draw或frame。两san enqueue覆盖16个格式/metadata组合，每san11132累计检查及32MOCK提交；31collector负例和4builder拒绝通过，均不是GPU提交数。HEAD516540d加已完整hash记录的sRGB20源变更；本证据在v12原始游戏构建并行时生成，保持code/HEAD冻结直到该游戏run终态。
