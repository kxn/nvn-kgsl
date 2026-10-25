> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原 SDK TextureBuilder / layout 静态事实

仅原始 NSO 的只读静态 ARM64，未运行 SDK/游戏/GPU，未使用 CPU 解释器或 Metal/Vulkan。没有修改 tracked 源码。本报告说明输入 SDK 的行为，**不把 Tegra 布局常量当作 Adreno 布局实现**。

输入 `Update/ExeFS/sdk` SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，build-id `2d369d9999e3e10e665106f840ffff9bdaa0bb8e000000000000000000000000`；此前已独立静态确认实际 SDK53.314。`research.py` 重新验证 NSO 三段哈希、MOD0/DYNAMIC/RELA、523名 dictionary→fallback 方法表，输出 `static.json`、`sdk-methods.asm` 及限定 helper反汇编。原始游戏bytes不进入源码。

## Builder：保存参数与资源创建是不同阶段

| 方法 | dictionary / SDK偏移 | 字段 / 行为 |
|---|---|---|
| SetDevice |419 / 475290|X1→+8|
| SetFlags / GetFlags |420 / 4752e0；396 / 4753f0|W1→+20 / W0读+20|
| SetTarget |434 / 4752f0|W1→+24|
| Width/Height/Depth |435 / 475300；423 / 475310；417 / 475320|W1→+28/+2c/+30|
| Size1D |428 / 475330|width=W1，height=depth=1|
| Size2D |429 / 475340|width=W1,height=W2,depth=1|
| Size3D |430 / 475350|width=W1,height=W2,depth=W3|
| SetLevels |424 / 475360|W1→+34|
| SetFormat |421 / 475370|W1→+38|
| SetSamples |427 / 475380|W1→+3c|
| SetStride |432 / 4753d0|只保存W1到+64；公开PFN仍为ptrdiff_t，不能因此改成32位参数ABI|

这些release方法没有调用布局/GPU函数或按支持profile拒绝。**纯store不证明任意非法值属于合法NVN输入**；其调用方可能需满足独立前提。可实现的通用分层是builder保存已知声明参数、getter读回；GetStorageSize/Alignment/Initialize才明确执行本后端资格检查，未知布局失败时不能发布资源或返回假成功。

SetDefaults dict416/4752a0 的三份常量真实值已保存于static.json：target2D1，尺寸0/0/0，levels1，format0，samples0，swizzle2/3/4/5，depth-stencil mode0；flags=0，pool/offset=0，stride=0；device+8保留。不能把默认flags0当作LINEAR。

## 尺寸、对齐和分类

- GetStorageAlignment dict408/475540：先测试flags bit6（LINEAR_RENDER_TARGET0x40），直接返回128；再测试bit4（LINEAR0x10），直接返回32。二者均没有时才进入target/format/device/kind路径。两bit同设时RT优先。这些是**该Tegra SDK的对齐值**；Adreno必须使用实际FDL/外部allocation合同，不能照抄。
- GetStorageSize dict410/4763d0：builder+68非0（已打包texture路径）返回0；普通路径调用475b50，format表valid bit0不满足也返回0，否则取layout结果+20。0结果在原SDK不是统一“发生错误”标志，typed adapter只能依据sticky诊断判失败。
- layout helper475b50：target BUFFER10单独处理；其余测试flags&0x50。非0时走475ca8..475ce8：保存stride，tile参数清0，计算height*stride，单次W寄存器乘法后以X8保存。此release实现对溢出没有防护，不应把这个事实变成本后端接受溢出尺寸的理由。此分支不自动乘depth/mips，说明LINEAR的输入前提与tiled路径不同，不能拿它伪支持任意数组/3D/MSAA。
- flags&0x50==0进入475cec后路径：原flags/target/samples/mips/format-block尺寸/width/height/depth→45b380(layout parameters)→45b428(tile shape)→45b86c(total mip layout)。GetRawStorageClass477590同样按flags&0x50返回LINEAR1，否则进入format类别2..5。原默认flags0走源Tegra块布局路径，不是自动线性。
- GetStorageClass dict409/4756a0调用kind选择后返回0x100|kind低8bit；TextureGetStorageClass476880也返回对象kind byte|0x100。它和RawStorageClass类别不是同一个概念。

## COMPRESSIBLE / DISPLAY 的确定边界

COMPRESSIBLE0x8在475db0..475e48（Alignment/GetStorageClass也有对应分支）结合device+1b1c能力、format表属性和device配置决定W3是否启用。原bit8、bit12另作为独立参数传入46e6bc；不能仅看公开旧头猜新bit12名称。46e6bc最终经PLT58a000/GOTc74b10；JMPRELA64d3c8解析到 **NvRmGpuDeviceChooseKind**，所以这里是实际memory-kind选择，不是“返回固定0成功”。该能力是条件化选择，不能仅凭COMPRESSIBLE名称认定每个资源必定使用某种外部压缩字节布局。

TextureInitialize dict460/476440：普通路径先复制builder metadata并调用同一475b50布局；COMPRESSIBLE位设置时，在4766a8..4766bc要求pool flags&0x280非0，否则返回false。这里0x280是声明的COMPRESSIBLE0x80或VIRTUAL0x200。随后linear/BUFFER与tiled获取pool backing的分支不同（4766c0..4767c0）。**native-uncompressed eligibility策略仍需独立说明可观察语义与source-byte处理**；此静态证据不能直接证明Maxwell字节可被Adreno读取。

DISPLAY0x1：在这次限定的GetSize/GetAlignment/普通Initialize及其上述layout/kind helpers中，未找到将此bit单独作为分支的指令。flags会保存在对象中；这不证明DISPLAY可无条件忽略，也不证明任意纹理可绑定真实ANW。真正Window/外部allocator格式、分辨率、usage、owner和生命周期仍需独立资格。当前game本轮SetFlags的实际值尚未采得，不能把静态可能值冒充实际日志。

## 给 source tiling 转换使用的精确2D规则

以下仅普通、uncompressed、单sample、non-sparse、无显式packaged layout的2D/2D-array数学规则；不是执行原SDK的结果。

1. 45b428把经format-block/sample调整后的height放W6，调用4a29d0。4a2a18计算H'=H+floor(H/2)，返回global GOB-height指数：H'<16→0，16..31→1，32..63→2，64..127→3，>=128→4；global GOB-width/depth指数0。最大16 GOB高。**height8→H'=12→globalH指数0**；不能替换成ceilPow2(H*1.5)/8而得到指数1。
2. 4a2a60计算每个mip；4a2c3c..4a2c68对mip0也会收缩effectiveH：若exp>0，threshold=8<<(exp-1)；只要threshold>=mipHeight就exp--并threshold>>=1，直到exp0或threshold<mipHeight。descriptor global tileH与每mip effective tileH不能混用。
3. `a4c6c0`前两u32是{2,0}，分别是layout维度和array开关。target2D_ARRAY4通过dispatch表abaAE8→45b410设置array=1；45b86c:45b924因此选择4a2730(array mip layout)，普通2D选4a2a60。
4. 4a2730先4a2a60累加整mip链的单层尺寸。普通GOB tile-width multiplier1路径（即1<<globalTileW，非BC的format block-width）4a2888..4a29ac按base-mip尺寸收缩本地tileW/H/D，再将chainSize向上取整到`512 << (effectiveMip0TileW+H+D)`。475e5c..475e74最后乘array层数；3D另走不乘array-count路径。这不是“逐mip都按global tileH填充”也不是“array stride恒512”。
5. 显式packaged tile参数builder+60会在475d40..475d6c覆盖默认选择；上面的默认heuristic不能覆盖这类输入。SPARSE、MSAA、压缩format、MINIMAL_LAYOUT、3D、zcull不在上述简式的支持声明中，保留完整反汇编供后续限定研究。

## 下一实施/验证边界

root可批量加入typed texture参数日志，再按真实最终builder profile决定布局资格；日志不代表支持。shader agent负责独立source-layout数学/转换，hw agent负责采样filter等原语。资源layout的有效sizes、mip/array offsets、descriptor及pool寿命必须一致；拒绝只mask flags、随便返回64对齐/宽高乘4或CPU生成oracle图像的成功路径。

本报告不提交实现patch，也不改变已有真机资格；只有静态事实、可复现来源和清楚的未知项。
