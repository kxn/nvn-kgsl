> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650 公共 NVN BUFFER 采样与 Copy/Call 交错重放

当前生产源码正常构建、沿用既有签名安装到DEVICE_SERIAL，普通应用APP_UID。原始VS/C1FS、完整2176B图形控制范围和256B原C1不改写。12个float-compatible BUFFER格式 × Copy/Call × 两次重放，共48次真实GPU提交/完成等待通过。786,432颜色通道最大误差1；393,216guard字节、1,966,080源raw字节和7,077,888只读字节通过；96次在途销毁拒绝，60次真实注册对象Finalize，36句柄创建/释放配对。提交后同ID改为另一pool，下次入队消费新版本，在途版本保持旧owner。

真实二维RGBA8索引资源使原C1读取的BUFFER索引为0；该矩阵只资格index0。底层BUFFER一般范围由独立raw资格验证，这里不能宣称完整C1任意索引、2D R32F索引图像、整数/SNORM别名采样或真实游戏绑定。实现没有index0特例或fixture判断。

首轮2提交后失败原样归档。通用CopyCommands修复flat状态/绑定/native leaf独立复制和偏移重定位，源handle/arena不再成为语义来源；NODE_UNSUPPORTED保持、含Call源继续拒绝、未知private jobs不被伪装RAW。复制之前独立持有binding refs，支持命令内存回调中源句柄Finalize，失败不发布部分序列。公共HOST12正例均实际验证Copy→Call→Copy→Call→unknown-submit(5次mock提交)，27格式拒绝及defaultOFF39项；foundation双san各783断言覆盖source-independent owner、绑定保留、重定位、OOM/retain/range/overflow/Call/unknown负例。另原core资源/子视图/生命周期回归和API29实际KGSL链接通过。这些HOST结果不计GPU调用数。

48份完整raw压缩后重新解出、逐项核长度和SHA。runner最终force-stop=0、pidof=1且stdout/stderr为空，进程已退出。本轮为公共NVN资格，不是游戏帧；没有Metal/Vulkan渲染器、CPU shader解释器或CPU图像变换。
