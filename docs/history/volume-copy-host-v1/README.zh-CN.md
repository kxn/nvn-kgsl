> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 真三维 canonical GOB 布局与独立 Adreno 复制内核

此阶段只证明HOST元数据、实际编译内核绑定及Android严格链接，**没有三维GPU执行或真实游戏通过的主张**。真实D2R最新记录仍为canonical-copy-v8：flags0、format66、target3D、32×32×32、单mip的GetStorageSize返回95。

生产元数据区分数组和三维：三维资源layers=1，selected mip的depth随mip缩小，Z按block-depth与GOB内交错；完整volume mip-chain extent不能当Z步长。新增volume_copy.comp独立GPU内核，原二维/数组kernel不改；无CPU像素转换/解压。公开三类Copy在相同format/flags0域选择对应typed plan和内核，共享原有owner、别名证明、命令内存保护、CopyCommands、回放及retire路径。三维Texels、采样、render target以及跨2D/3D的T2T尚未放开；这表示当前实现范围，不能当NVN规范限制。

正常工具compile_volume_copy.py使用既有SPIR-V/NIR/IR3编译链生成实际8×8×1、2SSBO、UBO2/64B内核并校验原编译器源码闭包。build_d2r_activity.py新增显式--volume-copy-check，连同kernel输入/工具/产物哈希纳入完整记录；默认关闭，不能拿旧游戏binary叠补丁部署。

独立资料和适用范围：

- deko3d 373bfd43d8b2d805fa2eb7d406dbb823032df958，zlib；作者自己的NVN逆向结果，source/dk_image.cpp提供3D tileDepth chooser和mip布局。其Dk API不是NVN合同。
- NVIDIA open-gpu-doc 9fdf5c4062007929d9f4e6cbad9c9771fe61b880，原厂公开Maxwell COPY类字段，可核对BD/depth/origin。不能向Adreno直接提交Maxwell命令。
- tegra_swizzle d7ed318ce233a4d557d4a2f207ed5dc04ed813c0，MIT；公开GOB深度公式、mip函数和16³向量。该项目明确说明图形算法源于Ryujinx、fixture由模拟器生成，因此不能称完全独立硬件观察，更不能算真机证明。本阶段没有导入CPU模拟后端，也未运行Rust库或复制其CPU像素转换到生产路径。

精确URL/长度/SHA256见public-sources.json。不同来源交叉核对后编写当前方程；**没有完成SDK53.314 chooser逐项实测等价确认**，不能把社区公式提升为完整官方NVN定义。后续真实游戏与独立GPU oracle仍须验证当前支持域。

normal-host-check.json：clang function sanitizer与GCC ASan/UBSan各42781元数据检查、107448绑定检查，覆盖37格式/4884区域及所有权、descriptor prefix、alias证明和损坏plan负例；使用实际正常编译内核头，不执行GPU。array-routing-regression.json：原公开二维复制每sanitizer各68584检查/151次不执行GPU的MOCK提交、38个T2T场景及真实API29后端严格链接。它们是固定源码时点的历史记录，后续修改不能倒写。
