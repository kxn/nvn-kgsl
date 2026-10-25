> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 通用清屏、Gather 编译与公共颜色目标接入

本批只证明正常构建、HOST 语义与 Android ARM64 编译/链接。没有新的 GPU 或真实游戏执行结果。入口计数仍为 406，已知剩余 102，未知 ABI 15；不能把入口数当完整 NVN 资格。

通用 ClearColor 支持 22 种有限浮点颜色格式的单层单 mip 2D canonical 目标。CPU 只打包一个 texel，GPU 处理裁剪、通道掩码和真实目标存储。Call 回放使用父命令的目标与 scissor，并保留命令、目标和未知完成资源。四组 HOST 检查及实际 API29 KGSL 严格链接通过，尚待独立公共接口真机资格。

正常正式宿主合链暴露常量库与 FDL 内部 util_format_description 重名；修复进入正常常量库生成器，使用 LLVM 对可重定位对象 localize-hidden，仅公开 nn_clear_color_pattern，并复编原始源码闭包。完整常量库与 FDL 静态库 whole-archive/--no-undefined 合链通过。保留失败原因，不手改已生成库或原游戏 binary。

原始 CASC VS/FS 在未改写完整 code/control/C1 下经 Maxwell→SPIR-V→IR3 编译成功。实际 Gather 为原生 tg4，独立 operation16；other1/samples0 保留。FS 原始 C1 256 字节、FragDepth 实际寄存器14仍存在，旧 color-only recipe 正确拒绝。R/G/B/A 分量与偏移、shadow、set1 拒绝通过；旧三组 0/1/2 纹理程序的机器码、汇编和 link.json 逐字节不变。运行时 Gather 和真正 depth 附件仍需接入。

公共 GOB22 target 路径已与 ClearColor 分离合入，actual FDL 格式缓存、GPU 原始字节双向转换、5 BO owner 与 Copy/Call 提交保活通过 HOST/NDK 检查。没有把原生 RT132 的历史结果当作此公共路径的 GPU 证明。

Activity 正式宿主通过原始 ExeFS 的同一规范入口重新生成模块，73 个编译单元中有 36 个原 host/HLE 单元；新增颜色能力通过显式参数注册。实际源码闭包、ELF 依赖与缓存完整性核验通过，不含旧 Metal/Vulkan renderer 或 CPU 模拟执行。该构建没有部署或执行游戏，不能代替真实游戏里程碑。

最近真实 D2R 仍是 v16：34 次纹理初始化、4 个单 CS 程序设置成功；第一个图形 ProgramSetShaders 失败，尚无原游戏 Draw/QueueSubmit/Present 或首帧。
