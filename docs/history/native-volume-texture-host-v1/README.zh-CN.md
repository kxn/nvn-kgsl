> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原生3D纹理取样前置实现：正式HOST资格

本批加入实际FDL volume descriptor、与Program持有的精确SPIR-V配对绑定的NIR维度/操作表、原生TXF/TXS录制路径。仅支持six owner formats；独立NN_PIPELINE_VOLUME_SAMPLED默认关闭。未开放guest 3D resolver、过滤采样或IMAGE load/store；没有新增公开NVN入口。

root正式重建独立IR3（原Mesa固定归档，未构建Vulkan/Gallium驱动），正常生产原创3D shaders；GCC ASan/UBSan与Clang function各22,806检查、36实际FDL/Program/PM4编码jobs通过，Android API29真实KGSL/FDL严格链接通过；默认OFF在分配job之前拒绝3D。四种合法但未支持的SPIR-V均被编译器按预期profile拒绝。

正式图形目录也已重新生成：12个stage身份、32候选pair中16成功；32个VS/FS机器码产物与此前2D目录逐字节相同。此项只证明代码身份，不能代替GPU运行。旧FDL和搬运kernel仍可验证其固定旧工具闭包，未改写旧record，也不将其标成新编译器产物。

本批没有运行Android进程或GPU，不代表NVN 3D Draw通过，也不代表D2R出帧。设备保持停止。下一步接同一volume owner到实际texture-unit Draw，用独立完整读回验证。
