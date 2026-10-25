> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 通用原始纹理块复制：元数据与编译基础

NVN 行为来自原 SDK53.314 copy 方法静态核对、公开 ABI 和独立 canonical 布局证据；没有以 Metal 对象或复制流程为模板。公开 setter 保存已知格式 enum，包括 BC 格式；支持范围由后续消费者判断。像素区域转成原始 block 坐标、mip/layer 和实际使用的字节 stride，不修改原有分配或 GPU 地址。未知格式重解释、非对齐压缩偏移和未知 flags 尚未放宽。

新内核经正常 GLSL/SPIR-V/NIR/IR3 编译，直接在 canonical GOB 与 linear block bytes 间复制。它不解压 BC、不在 CPU 上转换图像。奇数字节步长采用唯一目标 DWORD 所有权，避免相邻行/层并发写同一 DWORD；保留非目标字节。调用者仍须证明真实内存不别名及目的 DWORD 的独占访问，两个 GPUVA 不构成证明。

正常 HOST 检查覆盖 37 格式、1776 区域与两个方向，两个 sanitizer 各 63,986,826 项检查；真正 ARM64 API29 严格链接通过。另有完整原对象的区域/view/stride 检查与旧资源回归。这里没有运行 Android/GPU，没有实现公共 NVN copy 的提交路径，也没有产生游戏帧。normal-kernel.json 的代码来自本次正常编译，不继承旧内核 GPU 资格。

编译器增加了实际 compiler-owned immediate vec4 的上传，按 constlen 的 vec4 单位检查。129/512 DWORD 正例仅为 emitter 元数据边界测试，不是大常量 GPU shader 的执行证明。候选边界记录保留候选路径与 SHA；正常 compiler/kernel/host 记录来自主工作树的新构建。
