> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 深度附件基础

新增 actual FDL TILE6_3/nonUBWC/sample1 元数据和私有 BO owner。D16/D24X8/D32F 的 504 组布局、32 个负例和失败分配/销毁保活通过双 sanitizer；整数/合并 stencil 仅分类，未开放。正常 Android ARM64 元数据与真实 KGSL 严格链接通过，hidden 内部符号局部化后与完整 FDL 静态库 whole-archive 合链通过。没有 CPU 像素读写或 GPU/真实游戏深度绘制资格。
