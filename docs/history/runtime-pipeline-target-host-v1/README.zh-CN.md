> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 任意已支持尺寸RGBA8目标：实际FDL元数据路径

新增Device持有、地址稳定、无淘汰的实际FDL metadata cache，按chip/width/height/显式native pitch查询。纯字段适配，不自行实现FDL公式；公共NVN大小与对齐不变，GOB canonical与Adreno native分开。Device最终安全销毁时释放cache，inflight或关闭失败保留；已录制Draw不会持有悬空视图。

正常开关--pipeline-target-fdl-check默认OFF，与BC功能独立；两者同时打开必须用同一个验证过的FDL库。开启后使用运行时FDL查询，不注册或回退静态尺寸表。现范围RGBA8/普通2D/mip1/layer1/sample1，不扩大深度、MSAA或其他attachment语义。

root用最新正常3D扩展编译器/12stage与16pair图形包复验：201 metadata keys、分配失败、指针稳定性及73×41公开NVN LINEAR/GOB录制、enqueue、CopyCommands、重放、失败及Finalize生命周期，双sanitizer通过；真实KGSL/FDL API29链接和无FDL的OFF路径通过。Activity/CLI ON/OFF正常feature检查全部通过，含原36个host源码及实际构建/验证选项传播。BC8格式、16组元数据owner回归双san通过（每套32次MOCK提交），不计为实际GPU。

没有运行Android/GPU，也没有新增公开NVN入口或真实D2R帧。随后使用原public NVN Draw探针在静态表之外的尺寸做完整真机读回资格。
