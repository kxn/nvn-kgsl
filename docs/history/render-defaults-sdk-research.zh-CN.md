> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 独立核对 NVN 状态默认值与 sample locations

本次从用户原始 SDK53.314 的方法表、RELA、ARM64 setter/getter 和 sample-grid 分支表恢复状态语义，不使用原 Metal/Vulkan 后端。公开重建头只负责类型、枚举和 PFN；不把声明误当行为规范。来源分层见 [公开资料清单](nvn-public-sources.zh-CN.md)。

输入为未改写 `Update/ExeFS/sdk`，SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，build-id `2d369d9999e3e10e665106f840ffff9bdaa0bb8e000000000000000000000000`。复用仓库原 NSO 解包工具，逐段核验原始头内 SHA256，再恢复 523 项方法表中的 45 个状态方法。此次只静态读码，没有执行 SDK、CPU 模拟、GPU 或原游戏。

| 状态族 / SDK 相对地址 | 独立确认的默认标量 |
|---|---|
| Blend / `0x473c70` | target0；RGB/alpha 都为 ONE2、ZERO1、ADD1。advanced mode0、overlap0、premultipliedSrc1、normalizedDst1 也可读出，但后四项尚未实现，不能宣称整个 Blend 族完成。 |
| Color / `0x473ef0` | blend mask0；logic COPY3；alpha test ALWAYS8。 |
| ChannelMask / `0x473e50` | 8 个 target 的 RGBA 全开。 |
| Polygon / `0x474260` | cull NONE0、front CCW1、mode FILL2、offset enables0。 |
| DepthStencil / `0x474080` | depth test0、write1、LESS2；stencil test0；两面 ALWAYS8、KEEP1。 |
| VertexAttrib / `0x473fb0` | format、stream、relative offset 都为0。 |
| VertexStream / `0x474030` | stride、divisor 都为0。 |
| Multisample / `0x474330` | enable1、alpha-to-coverage0、**dither1**；其余标量0；16 个默认位置都是 XY=(8,8)/16。 |

原 multisample 默认 DWORD 为5，`GetAlphaToCoverageDither` (`0x474400`) 读取 bit2，因此此前 dither0 是实际语义错误，现已修正。运行时保存的是独立的语义结构；opaque 对象一般使用私有 memcpy-safe 编码，multisample 的24字节布局则按已恢复的标量域与 packed XY 表实现。所有 Bind 都按值快照，后续重置源状态不会修改已经绑定的状态。

`GetSampleLocationsGrid` (`0x474500`) 取 samples 与 rasterSamples 的较大值，并从 `0xabf0a2` 分支表得到：

| 较大样本数 | grid width × height |
|---|---|
| 0 或1 | 4×4 |
| 2 | 2×4 |
| 4 | 2×2 |
| 8 | 1×2 |
| 16 | 1×1 |

这不是按样本数扩大矩阵。每个 grid 覆盖16个位置，增加 samples 会减少覆盖的像素数。非法枚举没有可承诺的 release SDK 行为；本后端明确拒绝，不假称该错误策略就是正式 NVN 规范。

`SetSampleLocations` 的已审阅范围为 `0x4745b0..0x474860`，在下一个内部 helper 前截止。有限值和无穷值先 clamp，再限制到15/16，乘16、加0.5、转无符号整数，最后把 X/Y 打包到低/高 nibble。实现先暂存所有位置，再提交修改；NaN 的 scalar/SIMD 行为未统一证明，本后端选择原子拒绝。count0 不修改对象，最多保存16个位置。这恢复了位置状态，并不意味着 Adreno MSAA/sample-location 绘制已可用。

正常复现入口：

```sh
${D2R_REFERENCE_ROOT}/.local/venv/bin/python nvn-native/tools/research_render_sdk.py \
  --sdk ${D2R_REFERENCE_ROOT}/work/data/switch/3.3.94012/Update/ExeFS/sdk \
  --out .local/nvn-native/render-sdk-research-fresh
python3 nvn-native/tools/check_render_states.py --root . \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/render-sdk-family-fresh \
  --color-header .local/nvn-native/modes-host-final/color/graphics_bundle.h \
  --texture-header .local/nvn-native/modes-host-final/texture/texture_bundle.h
```

静态入口仅提取待人工审阅的方法及分支表，不自动证明上述语义。原汇编和临时解压字节只放忽略目录；归档 `render-defaults-sdk-static.json` 只含方法地址/范围/哈希、分支表目标和来源。`render-defaults-sdk-repro-proof.json` 对比原探索脚本与正常入口的全部45个方法及分支目标一致，并验证错误输入在产生输出前被拒绝。

正常 current-core 检查 `render-sdk-family-integrated-v3/proof.json` 通过：API450、color97、texture126 各跑 Clang function sanitizer 与 GCC ASan/UBSan；NDK/API29 编译通过；全部源在检查期间不变。API289基础上新增156个独立 SDK 位置/grid 检查和5个 Bind 快照检查。现有48-word A650 状态片段及相关默认寄存器断言保持通过，dither/位置在未启用相应特性时不引入额外硬件操作。这次没有重复既有 GPU 资格，也没有把 HOST 检查当 MSAA 或游戏画面通过。

前两轮失败均保留：family-v1 是 mutable `GetRasterSamples` PFN 被误写成 const 参数，严格编译拒绝；family-v2 执行450项全部通过，但 checker 仍要求旧289，整体按失败记录，修正精确预期后重新完整跑 v3。动态 command-state 的 blend color、offset、stencil masks/ref、bounds 和 patchsize 默认初始化仍标为 backend policy，尚未从 SDK command 初始化独立证明；不以状态对象的默认值推导未绑定命令状态。
