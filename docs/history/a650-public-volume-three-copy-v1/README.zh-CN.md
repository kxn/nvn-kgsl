> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650 公开 NVN 真三维三类复制 GPU 验证

替换测试机 pipa / Adreno650 / Android14，普通应用 APP_UID。37种原始块格式的full/partial与三类Copy，加原游戏32³ BC1几何，共228场景、456次真实QueueSubmitCommands/QueueFinish通过。完整实际数据池51,650,560B由独立Python BD/Z地址oracle逐字节核对，容差0；C端也核对完整数据、命令和control字节。测后force-stop、pidof退出证据已核验。

公开Builder创建真实3D资源，mip-relative Z随mip深度缩小；full跨多个BD slab，partial使用非零Z、非紧凑奇数字节row/image stride、压缩块边缘。独立池与同池真实不相交范围分别覆盖。每场景公开录制后使用CopyCommands克隆，先finalize原handle和命令，再两次重放并改变源输入，公开texture在仍被引用时finalize被拒绝。测试不直接调用私有encoder或raw submit。

原canonical布局、公开地址与完整mip-chain extent保持；GPU执行独立volume shader处理BD/Z交错。CPU只生成测试输入、独立预期与核对实际结果，不在生产复制路径解压或转换图像。所有456份完整实际数据池lossless保存raw-readbacks.zip，长度/SHA256见raw-manifest.json；未将预期图像当作实际读回保存。

限定：flags0、same-format、Generic16Bx2、CPU_CACHED|GPU_CACHED池0xa4。没有证明CPU_NO_ACCESS复制、格式转换、缩放、跨2D/3D的T2T、3D Texels、三维采样、Draw、显示或真实D2R帧。独立资料推导的canonical chooser尚不能据此宣称SDK53.314全域等价；本次证明当前明确测试域在Adreno正确执行。

normal-host-check.json包含各sanitizer228公开场景/456MOCK提交、135项typed UBO与owner/边界检查、99480项独立BD/Z几何检查，另43项oracle/transport负例和真实API29整个后端/Activity严格链接。MOCK不执行GPU。normal-apk-build-record.json是从当前生产源码正常构建的APK及完整指纹，evidence.json记录实际安装哈希、nonce、UID、inventory、核对计数与停止证据。candidate-freeze.json为资格程序冻结来源，后续变更不得倒写历史记录。
