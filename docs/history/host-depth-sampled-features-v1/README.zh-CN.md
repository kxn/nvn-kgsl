> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 深度纹理采样接入正式宿主构建

新增defaultOFF --depth-sampled，CLI/Activity正常构建、APK/CLI复验命令完整转发并校验严格boolean；依赖已验证depth FDL/A2D/runtime库、正式depth targets和sampled images能力。当前宏开启真实depth sampled模块，未开启时源码和符号均缺席，不靠探针入口激活。

正常工具四组合均实际NDK API29构建与链接：ABI1普通深度72源；ABI1深度采样76源；ABI2深度采样77源；ABI2 depthOFF69源。每组均保留36原HOST源。55断言/52负例包含转发类型、缺依赖、重复源、实际宏/目录/链接不一致等检查；ELF依赖不含Vulkan/Metal旧渲染器、EGL/GLES或CPU模拟器。只有编译/链接资格，没有运行Android原模块，也没有新增GPU像素结论。

运行时深度采样GPU60整组见独立a650-native-depth-sampled60-v1；真实D2R最新仍为v20 nnMain后packaged storage95，没有Draw/Present/首帧。本批不改变该里程碑。初次HOST命令传错names头文件路径而失败，其原日志保留；纠正为已有正常names.inc.h后，在fresh输出目录完成正常四模式，不修改校验和产物记录。
