> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Adreno650 原生 BUFFER 浮点读取与元素数量查询

实际普通应用 APP_UID、native KGSL/PM4/IR3/FDL 执行通过：43cases、40formats、两 replay，86次GPU CP_MEM_WRITE源数据准备、218 Submit、132 Draw/Finish。没有CPU写入源像素或CPU图像转换。20种float-result格式及32767/32768/65536边界共46次TXF；全部40格式共86次TXS。UINT/SINT TXF仍被编译器及nativepreflight拒绝，TXS通过不代表整数取样。

132份完整源/目标BO读回共9,027,584字节，ZIP逐项重新解压核验长度/SHA；2,162,688通道实际最大差0，540,672 guards、6,324,224 source readonly及15,162,864 metadata readonly检查通过。只有132个预先声明的4B GPU时间戳写位置作为写窗口。264次in-flight destroy拒绝，真实完成后正常退休，最终force-stop0及pidof1/双空输出，进程已退出。

FDL descriptor直接消费连续原BO及真实cpp/prefix/span；使用既有exact recipe/SPIRV类型表的BUF维度事实，不另写NIR解析器，不使用旧DWORD SSBO descriptor。默认OFF，公共NVN resolver未开放。float sampled声明的编译器profile保持不变；本包没有整数resultkind owner扩展。

这是一组原创RAW-NATIVE shader资格，不是公共NVN调用、原Maxwell/C1、D2R帧、display、任意import物理别名或同BO反馈资格。原公共BUFFER创建已由真实D2Rv15验证；其通用Draw消费正在另包接入。本次只新增直接硬件读取证据，未改变游戏状态。

正常root资格含双san各832,773检查、全部86个CP prepare字及tail、独立C/Python原始pattern和36个oracle/transport/CLI负例、真实NDK严格链接及34-target正常APK。foundation的20 float TXF/40TXS、typed owner、wrongshape/alias/OFF/rollback检查也独立保留。既有共享runner仅新增明确BUFFER分支，复用nonce/Activity/原子发布/完整inventory/严格stop，未写自制测量器。
