> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Adreno true3D FDL元数据，HOST/NDK资格

追加独立nn_fdl_volume与volume_metadata_version，原image/buffer ABI和metadata_version1保持。实际调用原Mesa FDL的is_3d/depth0/array_size1及FDL_VIEW_TYPE_3D，返回真实每mip的depth、pitch、slice_stride、offset、span、MIN_LAYERSZ和descriptor。没有重写原厂布局算法，也没有把三维伪装成数组。

当前仅A650、RGBA8/BC1 RGB+RGBA/BC2/BC3 UNORM、native LINEAR/noUBWC/sample1、sampled元数据；拒绝3D MRT和未支持格式。这个受限facade支持域不是NVN或硬件的完整限制。函数不接受像素指针，不分配BO、不转换、不提交GPU；不改变pipeline的3D采样/ImageStore gate。

normal-build-check.json及normal-commands.json来自主线正常producer：真实Meson/NDK七个原FDL单元、独立vendor closure重编、完整依赖与导出校验；动态依赖仅m/dl/c，五个C导出（原三项加volume两项）。HOST GCC ASan/UBSan与Clang函数签名sanitizer各28315检查，含原456个image、48个buffer及390个true3D与真实FDL函数对照。candidate-c-abi-proof.json另保留候选struct大小/offset与实际NDK C链接核验。

**没有Android执行、GPU materialization或三维采样结果。** 其它体纹理/颜色格式扩展、volume owner和实际GPU消费须后续单独实现与验证；旧2D GPU结果不能改写成三维通过。
