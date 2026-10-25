> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# canonical source texture 的 CPU 登记与消费能力分离

CPU登记不再要求RGBA8单mip/layer的GPU桥能力。既有canonical source布局通过nn_tex_source_validate完整校验，再保留原backing权限检查、32B软件slot publication、完整typed/source-layout副本与强引用。37种已支持布局格式的多mip/array资源通过NULL、默认、显式完整范围/零count view登记；需要真正子范围布局的view仍拒绝，不能把单mip/layer请求静默变成全资源。内存失败/错误view不改变slot/refs/bytes。Draw/copy/target继续走原生format/conversion检查，不代表BCn或其它格式已经可采样。

正常源资源检查含6524项两类HOST检查、现有resource/clear/packet回归及真实KGSL/Android NDK严格链接，全部通过。真实D2Rv6普通应用进程初始化4张纹理、调用5次texture登记，原失败的RGBA_DXT1 id260与default-view id261都成功，随后在TextureGetViewOffset进入明确ENOTSUP边界。46份原RomFS共8973547258字节在设备逐文件核验，原ARM64模块由正常入口从原始NSO重建；finally确认退出。没有真实游戏Draw/Dispatch/Present或frame。
