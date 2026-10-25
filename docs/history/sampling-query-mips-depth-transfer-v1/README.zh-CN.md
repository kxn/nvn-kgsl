> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 整族资源查询、mip采样编码与深度传输

本批实现和HOST/API29链接证明保留明确边界：没有新增GPU测试或真实游戏执行，没有新增NVN入口数。

- 实际NVN注册ordinary2D现在与BUFFER独立地回答39种可表示标量颜色格式的Shader.TextureFormat查询。语义名字对应由实际公共NVN声明及独立C#枚举生成预期，覆盖39种格式×单/多mip、注册后Finalize、late re-register及旧快照保留，双san各5464检查/78组。无表示的BGRA/sRGB/BC/depth等仍95，不返回会令前端默认RGBA8的Unknown。GPU采样、image load/store及Draw不因CPU查询成功而获许可。
- 新显式isotropic2D mip sampler共享已有descriptor编码，六种minfilter、两种mag、三轴wrap组合、mip数量和LOD范围/bias的32400例与实际XML packer逐字对照；双san各103823检查。旧32400例/64849检查回归通过。真实采样GPU与一般consumer启用仍待独立批量资格；多mip各向异性等保留独立缺口。
- D16/D24X8/D32F的实际TILE6_3附件通过实际FDL构造A2D别名和独立LINEAR读回布局；258布局/11094双san检查。完整transfer job复用实际设备restore及ordering，命令/零数据/硬件序列分页，CP_BLIT清除或读回之后full fini；双san各24个HOST编码job/1045627检查和失败owner保留测试，真实KGSL API29严格链接。没有深度GPU提交，stencil/组合平面/MSAA/UBWC/LRZ未实现，不能写作已完成深度。
- 修复APK/CLI重建校验命令漏传clear-color-kernel/constant及color-targets参数。15组HOST选项/真实provider完整性检查通过；这不替代后续正式宿主/APK构建部署。

查询扩展初次测试的独立BUFFER夹具缺GC链接选项、旧临时builder在两编译器栈复用导致的计数差异已修复；开发失败原记录留在ignored目录，未重标。ZIP为终态正常记录，逐成员解压重算长度和SHA256。

下一步是公共22格式采样整族glue、576次mip采样和396次深度操作的真机批量资格，以及正式宿主接入。实际D2R仍以最新真实运行记录为准，无首帧。
