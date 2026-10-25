> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公共二维采样整族与正式宿主接线

本批将实际FDL的22种未压缩FLOAT结果格式接入公共NVN资源准备和提交：完整mip链、连续选定视图、SAMPLE/FETCH/SIZE、独立采样器，以及严格typed Gather角色。每次入队创建新native存储，使用已有GPU block-copy上传，保留canonical源、对象/内存池引用与未知完成状态的owner。颜色目标、BC、BUFFER、体积和深度仍分别验证，没有借用其他角色的许可。

合入源码的HOST检查每个sanitizer504组：原真实source-backed SAMPLE176、FETCH/SIZE88、实际公共描述符加原创native Gather88、公共开关关闭88、旧BC64。两种编译器与实际API29 KGSL链接通过。这里没有22格式公共GPU结果，也没有原游戏Draw。

正式CLI/Activity的`--sampled-images`、`--sampled-mip-filter`、`--gather`贯通CMake、实际源码/宏/静态库/Device kernel审计、构建记录和四个校验入口。共享owner模块只编译一次，链接它不会隐式启用BC。78项断言、43项负例及四个实际原36源HOST/HLE链接通过；没有执行游戏模块。采样依赖正常graphics/FDL/block-copy，不要求额外bridge；canonical目标按各自路径单独验证。

真正的零颜色附件native深度编码也已合入：不存在颜色目标BO、地址或资源引用，使用真实深度尺寸；HOST每san21657检查/36个编码job，其中12个零颜色。GPU验证仍另列，396深度资格中的mode10仅证明FS无DATA0并仍有颜色附件。

新清除常量库与FDL整库共存检查修复了旧导出名单遗漏`nn_fdl_image_format_info`的问题，仍严格要求仅7个声明接口；没有放宽第三方内部符号检查。

`normal-records.zip`保存原始正常记录，`facts.json`列明范围和每个成员的原路径、长度与SHA256；逐成员解压复核。记录对应各自源码快照，后续无关文件变化导致旧宽依赖记录失效时，不能改哈希复用。正式部署仍须当前源码的规范构建与验证。

注册入口仍为406、已知待补102、未知ABI15。本批没有新增入口计数，也不把HOST次数或raw-native GPU通过数量记为原游戏进度。
