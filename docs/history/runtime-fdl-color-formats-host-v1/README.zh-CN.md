> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Native FDL metadata: 43 image formats

仅 ignored 候选；未改tracked、未操作ADB、不修改Mesa源码或硬件agent的3D shadow API/源码包。基于root已合的独立runtime FDL volume facade；不读取/复用Metal或Vulkan实现、不重新实现FDL、不扩CPU模拟或游戏补丁。

`minimal-promote.patch`仅4文件，SHA256 `d722f0cc7b7cd0f782e23dde20d65c9ca6cd9de86930a496862beec00af66e29`，当前root `git apply --check`通过。

## 格式与scope

映射从5扩为43，准确public enum→PIPE表、channel width/count和数值类型见`format-map.json`：

- R/RG/RGBA 的8位UNORM/SNORM/UINT/SINT。
- R/RG/RGBA 的16位float/UNORM/SNORM/UINT/SINT。
- R/RG/RGBA 的32位float/UINT/SINT。
- RGBA8 sRGB、BGRA8 UNORM、BGRA8 sRGB。
- 原4个BC1 RGB/RGBA、BC2、BC3 UNORM继续保留。

39个uncompressed color包含原RGBA8，因此本包新增metadata格式38个。真实游戏所需RGBA16F public enum0x29明确映射PIPE_FORMAT_R16G16B16A16_FLOAT；RGBA32F、RG16F/RG32F/R32F同样保留正确float class，未映射为UNORM或整数。

2D/array、真正3D的layout/view仍全部调用原 `fdl6_layout_image`、`fdl6_view_init<A6XX>`。新增admission读取原 `fd6_texture_format_supported`/`fd6_color_format_supported` 与util format traits：2D MRT只允许真实表支持的uncompressed非depth格式、单selected mip/layer；BC/depth不能被当作color target。3D仍sampled-only，所有3D color-target请求继续ENOTSUP。显式pitch仍仅原RGBA8 mip1/layer1；BUFFER仍仅RGBA8/R32F。没有为扩大metadata格式而扩大这些usage/profile gates。

facade struct大小、schema/version、5个导出函数ABI不变；shader标量类型、采样器类别、render state/clear/Draw、实际资源owner和GPU资格均另行gate。**metadata格式支持不等于新增NVN入口、可采样/绘制能力或D2R帧**。没有修改这些GPU consumer的代码。三分量RGB、scaled I2F/UI2F、depth/stencil、ASTC/BPTC等其他format仍不在本包mapping范围，不猜其语义。

## 独立依据

- 当前公开MIT/h1k421重构 `third_party/nvn/nvn.h` 的format enum；文件声明来源为Darksiders II DWARF、53.105部分信息，不当作53.314完整规范。对应固定[公开nnheaders C声明](https://raw.githubusercontent.com/open-ead/nnheaders/246c3b482d045934ed004f50c5ada37f3b10b703/include/nvn/nvn.h)的R/RG/RGBA、类型和通道位宽命名；没有从旧renderer复制映射。
- 已固定Mesa26.2.5真实 `fd6_format_table.c/.h`、`fd6_layout.c`、`fd6_view.cc`、util format descriptions与原A650 device traits；实际源hash由normal compiler记录和Ninja依赖全量核对，不用另一套手写layout/table冒充FDL。
- 新fixture除逐descriptor/slice直接比较原FDL返回外，独立核实NVN semantic trait：channel count/width、float/UNORM/SNORM/UINT/SINT、sRGB、BGRA swizzle。production switch用numeric ABI，fixture用public enum绑定，避免只检查两份相同数字表。

## 实际正常验证

`source-root-v3/.local/normal-runtime-fdl-v2/check.json`由**原正常**`build_runtime_fdl.py`从头生成，producer未修改：

- G++ ASan/UBSan和Clang function sanitizer，各先通过旧28,315检查/456 image/48 BUFFER/390 volume，再通过新增103,236检查/2,496 image/624 volume。每个compiler合计131,551检查、2,952 image/48 BUFFER/1,014 volume。旧reference输出是真实独立baseline执行的计数，没有将新矩阵计数伪写为旧值。
- 新增matrix覆盖39 common color的NPOT、1像素、最大单轴、2D/array多个mip/selected layer、真正3D/Z/最小末级slice、address0和非零synthetic整数地址、2D sampled与Mesa支持的MRT metadata、3D RT拒绝、非RGBA8 explicit-pitch拒绝、BC/depth错误RT拒绝及failure全零。
- 原7个Mesa FDL/device/util source units实际Android API29 cross-config、严格链接；实际compiled deps闭包68个source/header/config文件和378个外部NDK依赖固定。独立source-closure再次重编/链接通过。实际ELF仅导出原5个facade函数，动态依赖仅libm/libdl/libc，无renderer、shader compiler或pixel pack/unpack/codec。
- `metadata-source-closure.tar.gz`由正常producer生成，包含原许可证与actual Android-generated config；没有复制HOST config来伪装Android构建。

`source-root-v3/.local/normal-runtime-fdl-cpu-v1/check.json`由**原正常**`qualify_runtime_fdl_cpu.py build`额外通过：

- 纯C facade fixture，GCC/Clang两个sanitizer各两次执行、每次11,064检查/456 image/48 BUFFER/26 negative/530 records，完整序列化metadata跨compiler/replay相同。
- 实际API29 C qualifier binary严格链接通过但未运行Android。该旧C矩阵原把R32F当unsupported negative；本包改为本来就未映射的DEPTH16，否则新正确支持会被旧错误negative阻止。
- 旧C qualifier reference直接执行前述原FDL oracle，再确认其baseline仍456/48；没有削弱/改写其检查器以跳过差异。

实际GPU submit/allocation/device open/image-data access均0；整个工作是CPU metadata及交叉构建证据，未读写任何像素。首轮NDK闭包已通过后新增fixture有C++ struct-tag拼写编译错误，失败保留在`source-root-v2/.local/normal-runtime-fdl-v1`；修复仅fixture，随后新snapshot/正常producer全量重跑通过，不修改失败记录。

## 合并后

合并4文件后，在正式root重新运行正常producer和C qualifier，不能将ignored snapshot proof冒充正式source closure：

```sh
python3 nvn-native/tools/build_runtime_fdl.py --root "$PWD" \
  --ir3 .local/nvn-native/ir3-typed-unit-immediates-integrated-v2 \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/runtime-fdl-color-formats-integrated-v1 --jobs 4
python3 nvn-native/tools/qualify_runtime_fdl_cpu.py build --root "$PWD" \
  --runtime-fdl-check .local/nvn-native/runtime-fdl-color-formats-integrated-v1/check.json \
  --out .local/nvn-native/runtime-fdl-color-formats-cpu-integrated-v1
```

真机采样/type/RT正确性及真实D2R接入由后续实际owner、shader与GPU证据验收；本包未扩大这些结论。
