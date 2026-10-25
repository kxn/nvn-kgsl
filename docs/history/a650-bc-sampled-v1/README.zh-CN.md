> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650 原生 BC1/2/3 硬件采样资格

Root从当前生产源码正常重建authored-fetch shader检查、shadow HOST检查、真实原shader qualification package与APK，验证输入/工具/产物闭包后，以普通应用APP_UID在替换A650设备执行24次Submit/Finish，293152个颜色通道与独立闭式oracle相同，最大误差0。测后force-stop及pidof确认退出。

机制是原canonical GOB压缩块→现有正常GPU raw-block kernel→实际FDL原生压缩storage→原967/a767 VS/FS硬件采样→RGBA8目标。CPU不映射native压缩数据，不解码/转换像素；descriptor与RGB alphaONE来自实际FDL在真GPUVA处再生成，不手造format/stride/swizzle。用了现有正常CS→graphics完整cache/顺序链，没有新增测试专用CP标记或同步间隙。

4种格式（BC1 RGB、BC1 RGBA、BC2、BC3 UNORM）×3尺寸（8²、37×21、128×65）×2同wrapper重放共24图。固定合法BC控制选endpoint/透明选择子：覆盖BC1 RGB与RGBA透明差异、BC2显式alpha、BC3指定alpha端点/选择子。白顶点/identity矩阵/mix0/gamma1/contrast1是显式资格输入，不是生产shader身份特判。此有限语料没有测完整插值、线性滤波、sRGB、array采样、任意LOD、3D或其余压缩格式。

293152B实际RGBA读回全部lossless归档于actual-rgba-readbacks.zip，逐文件长度/hash见raw-manifest.json，native与离线工具分别校验。native另核对380640B guards和393216B readonly源/VBO/UBO；这些保护区检查是设备记录，未把active RGBA文件宣称为完整pool归档。

Root HOST两sanitizer各256434项、112个实际FDL mip/view plan、8个owner场景；默认OFF明确拒绝的256426项及实际完整NDK链接均通过。Production sampled-only钩子默认OFF，target仍限原RGBA8；本批没有放宽公共NVN descriptor/prepared消费门。**这是raw-native资格，没有公共NVN采样或真实D2R帧证明。** 公共消费链及3D另行实现/验证。
