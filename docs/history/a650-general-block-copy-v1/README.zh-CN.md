> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650 通用原始块 copy：实际 GPU 验证

DEVICE_SERIAL 新机 pipa / Adreno650、Android14，普通 Activity APP_UID；使用主工作树正常 NDK/CMake 构建、原调试签名及精确检查的 kernel 编译产物。没有使用旧 renderer、CPU GPU 指令执行、CPU 图像转换、BC 解压或真实游戏 shader。

37格式 × 两个方向 × 两组代表区域 = 148 jobs，每个修改源 pattern 后重放一次，共296次真实 KGSL Submit/Finish。覆盖 full/partial、mip1/2、array层、奇数 row/image 字节 stride、同一原BO内分离窗口、BC真实非整块边缘和R8/R16目标 DWORD 邻居掩码。这是1776 HOST矩阵的有界子集，没有宣称全维组合皆已GPU验证。

设备端测试与独立 Python inverse-sector oracle 均通过。296份完整GPU数据BO回读，逐字节容差0，总15,499,264B，其中2,431,256B active、13,068,008B保持不变；source readonly6,298,176B、aux readonly6,060,896B为分别记录的检查域，不能与总数简单相加。296次busy销毁拒绝和sequence检查通过。进程已强制停止并通过pidof无匹配核实。

raw-readbacks.zip 是真实回读的无损归档，不是期待值；raw-manifest.json 保留原文件名、长度、SHA256，并已解包重新核对。正常构建、设备结果、UID/nonce、安装APK摘要、完整文件inventory和独立复核均在evidence.json及normal记录中。

资格范围为直接调用native block-copy helper/公共KGSL owner的原始块复制。它没有通过公共 NVN Copy 入口，不能替代该调用链的GPU资格；尚未证明BC采样、其它CopyFlags、格式转换/缩放、D2R绘制或出帧。公开 B2T/T2B 接线已有独立 HOST/NDK证明，其整条GPU资格和T2T组合路径继续分别推进。
