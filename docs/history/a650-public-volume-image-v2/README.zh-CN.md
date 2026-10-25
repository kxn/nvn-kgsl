> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650三维IMAGE权限与公开复制扩展资格

在pipa/Adreno650/Android14普通APP_UID执行228场景、456次真实公开NVN QueueSubmitCommands/Finish，全部通过，测后进程退出。完整456份实际读回51,650,560B由独立Python oracle零容差核验；有效写5,559,786B，其余46,090,774B保持，另核对源只读23,645,872B及命令/control59,768,832B。全部实际数据lossless保存raw-readbacks.zip及逐文件哈希，不保存预期替代品。

37种原始块格式的full/partial、三个Copy家族、CopyCommands与两次变更源重放保持；其中RGBA8与RGBA16F的partial各三个家族使用IMAGE flags4，共6场景/12次实际提交。公开GetFlags、资源创建、完整数据与guards均核验。copy flags仍0，source与destination同format。没有ImageStore、三维采样、3D Texels、Draw、显示或真实游戏提交的主张。

行为依据：公开NVN声明IMAGE=4、COMPRESSIBLE=8；独立deko3d固定373bfd43d8b2d805fa2eb7d406dbb823032df958的dk_image.cpp将UsageLoadStore准入与三维tileDepth/extent方程分开。在本后端已声明的Generic16Bx2/no-compression policy下，保留IMAGE权限，不另造布局方程；COMPRESSIBLE按既有设备/池policy作为permission处理，不能把权限当实际NVIDIA压缩元数据。精确来源/hash见前volume-copy-host-v1/public-sources.json。未声称已逐项实测等价SDK53.314完整chooser。

normal-host-check.json包括原228场景/456次MOCK提交、各211项typed UBO/owner/权限检查、99480项BD/Z几何、43个oracle/runner负例和实际API29完整后端/Activity严格链接。额外HOST权限覆盖RGBA8/RGBA16F实际32³的flags0/4/8/12，确认公开flag保留、payload只保留IMAGE而COMPRESSIBLE依no-compression policy归一化，原3D消费gate未放开。COMPRESSIBLE权限在此仅有HOST元数据资格。

normal-volume-math-check.json每sanitizer43595元数据及107448绑定检查。normal-apk-build-record.json保存正常主线构建及安装产物指纹。第一轮HOST曾有测试size_t/int比较告警，修正fixture后正常新目录重跑并重建APK；历史失败未删、旧record未改。真实D2R最新历史仍为v9的IMAGE RGBA16F32³ GetStorageSize95；此后源码改进的游戏验收须由下一轮正常原始NSO构建证明。
