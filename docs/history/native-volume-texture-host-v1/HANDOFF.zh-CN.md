> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原生 true3D texture-unit 前置候选（HOST/Android ELF，非 GPU）

此包仅 ignored；不修改 production、不 ADB、不部署。所有 shader 都是原创测试，不替代原 Maxwell/D2R shader。没有 Metal/Vulkan renderer、CPU guest 解释器、CPU 像素转换；glslc 的 SPIR-V target-env 字符串只是已有离线工具约定，不引入 Vulkan runtime。

## 已实现

- 原 common-NIR sampled 入口接受 scalar float 非 array/non-MS 3D **TXF/TXS**。3D filtered sample、3D offsets、integer texture、cube 等仍拒绝。
- normal `graphics_ir3.c` 另产 `.pipeline.types.json`，binding dimension/op 直接来自实际 NIR texture 指令。旧 recipe-v1、`.pipeline.json`、PM4 emitter 不改。首表只有 FS；原 producer 对 VS sampled resource 的拒绝仍保留。
- `compile_volume_fetch.py` 沿正常 `compiler_inputs`/build-record、glslc/spirv-val、nvn-pipeline-ir3/platform 生产实际机器码、命令、SPIR-V、typed 表及逐输入/产物哈希。新增 authored TXF/TXS 配对；不伪造 Maxwell control/C1/API CB 映射。
- 新 `nn_pipeline_program_create_native` 是明确 internal raw-native SPIR-V 入口，共用原 Program code upload/retire/retained rollback，正常 immutable bundle 是调用方必须验证的信任边界。内部 native stage 保存实际 SPIR-V，control 为 NULL，绝不能送去 Maxwell source lookup 或充当 game archive。
- typed 表绑定 exact recipe 指针、两个实际 SPIR-V 完整字节身份。Program 固定持有该 immutable 表；Draw 必须是同一表，不能拿 caller free-form dimension 标签替代实际 stage facts。现 Maxwell constructor 返回 types=NULL；没有任何 guest resolver 因本包开放3D。
- draw state 末尾 append `sampled_volume[16]` 和 `texture_types`（内部 full-source rebuild）；旧 recipe ABI 不变。volume 与2D/BC descriptor明确互斥，dimension 必须3，actual op仅fetch/size。
- 新 `texture_volume_sample.c` 只支持现 six owner formats：RGBA8、RGBA16F、BC1 RGB/RGBA、BC2、BC3。它用实际 `nn_fdl_volume` 重建并逐字段核对所有 mip/descriptor/span，再用实际 BO+offset 重建GPU地址 descriptor；不从pitch猜layout，不自己pack描述符，不把纯FDL更广metadata域缩成6格式。
- target仍原RGBA8/2D；sampler_state.c、公共NVN/API、runtime query resolver、canonical/GOB materialization owner均不改。volume 采样由 `NN_PIPELINE_VOLUME_SAMPLED` 默认0严格关闭。ON时才把 helper+真实FDL库加入目标；不要把有FDL强引用的 helper 无条件放进 OFF whole-archive link。

## 实际验证（均非 GPU）

- 正常 dedicated IR3 build：`tree/.local/ir3-v1/build-record.json`，原 Mesa archive/正常 overlay构建，无手改 Mesa。
- 实际3D TXF/TXS artifacts：`tree/.local/volume-shaders-v3/check.json`。
- HOST `tree/.local/check-v2/check.json`：GCC ASan/UBSan、Clang function 各22,806检查，各36实际FDL/Program/PM4 jobs；六格式×base mip0/1/2×TXF/TXS。还独立编译默认OFF并确认每个实际3D view在分配job前拒绝。
- 逐16DWORD actualFDL descriptor、未绑定hole全零、完整packet边界、actual资源ledger、busy时program/texture拒销毁、Program upload/cache/rollback失败保活、錯SPIR-V/recipe/types/维度/op/unused字段/窗口/offset/descriptor全部否定。
- 用正常 root `runtime-fdl-srgb-integrated-v1/check.json` 原 Mesa FDL HOST reference；Android API29 actualKGSL+FDL全源 `--no-undefined` shared link及fixture ELF通过。没有 Android runtime执行。
- `tree/.local/compiler-negatives-v2/check.json`：四个合法SPIR-V，实际normal frontend分别拒绝filtered3D/integer3D/3D offset/cube；明确检查exit4与profile诊断，不把任意compiler崩溃算通过。
- `old-2d-regression.json`：既有2D BC authored原正常产物 vs 新正常compiler产物，VS/FS bytes、recipe header/JSON、platform header全部逐字节相同。仅新增types sidecar。

## 正常复现入口

先 root 正常 promote 两个shader源改动，再正常 build_ir3（新目录，不更改旧record）。之后：

```sh
python3 nvn-native/tools/compile_volume_fetch.py --root ROOT --ir3 NEW_NORMAL_IR3 --spirv-tools NDK/shader-tools/linux-x86_64 --out ROOT/.local/volume-texture-shaders
python3 nvn-native/tools/check_volume_sampled.py --root ROOT --runtime-fdl-check CURRENT_FDL/check.json --shader-check ROOT/.local/volume-texture-shaders/check.json --ndk NDK --out ROOT/.local/volume-texture-host
python3 nvn-native/tools/check_volume_fetch_compiler.py --root ROOT --shader-check ROOT/.local/volume-texture-shaders/check.json --spirv-tools NDK/shader-tools/linux-x86_64 --out ROOT/.local/volume-texture-negative
```

FDL原有4文件仅候选baseline跟随root最新sRGB metadata，**不在patch**。这里没有normal APK/GPU option；首批交付是可链接/可编码前置，不是空gate，也不声称完成真机texture3D采样。

## 下一 GPU qualification / 真实 guest 边界

独立 raw-native 模式可复用已冻 volume-shadow owner、normal Activity/nonce/strict-stop，不更换transport。选六格式，尺寸32³/多mip35×19×9/11×17×5，selected base0/1/2，真实owner每mip转换后同queue→texture-unit Draw，所有 returned BO进入refs，Finish失败整组保活。TXF用非零Z切片、mip0/1/2、重放更改原canonical；BC用合法独立endpoint/index raw block图案，F16用有限精确half值。必须从实际GPU RGBA读回独立解析数学oracle，不以CPU读取native影子代替oracle、不把转换owner曾通过当采样通过。

TXS输出为 `(width + (int(varying.x)&1), height, depth,255)/255`；测试可令全部顶点varying.x固定0或1，显式查询非0LOD/base，从而保留真实FS varying/VPC消费而避免浮点边界歧义。该原创shader写法是为硬件资格，不修改游戏shader。

target active bytes/guards、source canonical+native readonly、metadata/UBO/code readonly、两回放必须严格收集。BC/Half输出UNORM量化容差需按公式独立声明，不能因未知结果扩大。constructor不map像素；fixture map只用于HOST ownership/packet检查，不模拟GPU。

真实NVN3D消费者仍缺：实际selected stage/variant的typed表选择；actual registered texture.target/format/mip/view与Maxwell QuerySamplerType specialization闭环；actual shader TEX/FETCH/SIZE及shader scalar type对应；源canonical写入→shadow准备/依赖；selected descriptor owner pin、生命周期、unknown quarantine。C1 TidB112 既有1D/BUFFER问题不因本包变3D。IMAGE4仍是元数据/原始搬运许可，不是image load/store、storage descriptor或GPU consumer资格。3D normalized/filter/aniso/compare以及sRGB3Downer均未开放。
