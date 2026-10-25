> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650 原生22颜色目标 GPU 资格

当前生产源码正常构建、既有签名安装至DEVICE_SERIAL，普通应用APP_UID。22格式 × 覆盖写/SRC_ALPHA混合/RB通道掩码 × 两次同job重放，共132次真实提交/完成等待通过，66个job与66次重放。格式包含R/RG/RGBA整数归一化、16F/32F、R11G11B10F、RGBA/BGRA及sRGB；目标73×41，实际FDL元数据与原color VS/FS执行，没有CPU制造颜色结果。

1,023,606通道校验实际最大误差0；2,849,064 guard字节、8,650,224只读字节、221,400重放保留字节、149,650掩码分量通过。只豁免job内明确声明的4B硬件timestamp，邻接字节仍只读。设备只保存raw并检查guards/所有权；颜色oracle在HOST复用Mesa pack/unpack与原color闭式运算，不进入Android像素路径。132份raw压缩后重新解出、逐项核长度与SHA。

HOST双sanitizer各164,612断言/69编码状态，默认RGBA8命令只归一化声明地址重定位后逐字一致；23个validator负例、NDK严格链接通过。原FDL新增R11，BUFFER仍严格40 plain格式。原图形2176B控制数据是完整容器声明范围，C1按原包保留；没有扩成compute的2200B或放宽身份匹配。正常39查询清单/51stage/94pair重建覆盖更新头文件指纹，历史记录保持不变。

本轮是raw native颜色目标资格，不是公共NVN非RGBA8目标接入、GOB渲染、深度/模板/整数输出、MRT/MSAA或真实游戏帧。公共入口继续按已资格范围门禁。runner最终force-stop=0、pidof=1且stdout/stderr为空，进程已退出。无Metal/Vulkan渲染器或CPU模拟。
