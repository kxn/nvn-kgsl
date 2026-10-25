> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原SDK独立核验：空录制与Sampler defaults（冻结）

全部行为依据本游戏原 SDK 静态 ARM64，没有使用 Metal/Vulkan 实现、CPU模拟或游戏执行。输入sdk SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017` / build-id `2d369d9999e3e10e665106f840ffff9bdaa0bb8e000000000000000000000000`，三段按原NSO header SHA验证。`static.json`含方法字典index、fallback slot/RELA、函数定位、必要代码/常量span hash，`sdk-methods.asm`可直接复核。SDK原bytes仅在工具RAM解压；没有执行它。

## 适用版本

原 `DeviceGetInteger` dictionary→sdk+4660e0，传property到内部46afc0。内部46b004预置W0=53；property0的jump-table项直接到46b010返回；property1到46b020写W0=0x13a=314。故该**固定SDK静态报告53.314**，不是Android运行测得版本。公开Darksiders DWARF重建头声明覆盖53.105且自述包含可能较新的项；不能将其注释当本游戏SDK实际版本。后端现53.105 partial是项目声明的ABI子集，不在此擅改成53.314或伪造版本资格。

## Begin/End 空录制合法

- Begin dictionary46 / fallbackslot c63fc8 / RELA→45d170，保存当前GPU cursor(object+18)到+30，设置active bit0（+70），初始化control header16B。可用command/control memory是前提；缺control memory仍有真正错误路径。
- End dictionary103 / fallbackslot c64190 / RELA→45d220。45d234检active bit0；不active直接返回0。active时45d244取当前GPUcursor、45d24c取Begin时cursor；45d250比较，两者相等在45d254**直接跳45d34c，未拒绝空录制**。
- 空control路径45d34c比较control当前cursor与header+16；相等则45d360..368置bit1。45d3ac..3b0追加8B terminal control记录。45d3c4见bit1走45d3f0返回header+16作为非零handle，45d3f4清active等bits。这里没有“至少一个Draw/GPU命令”的要求。
- 原 `IsRecording` dictionary123→45f480只返回active bit0，支持上述对应。
- 原state-only实例：`SetCopyRowStride` dictionary147→465bb0仅写object+88，`SetCopyImageStride` dictionary146→465bc0仅写+8c；`SetMemoryCallbackData`→45cde0仅写+10。它们不改GPUcursor+18、Begin cursor+30、active+70。因此Begin→这些CPU状态setter→End仍能走上述合法空分支。这是原函数数据流证明，不是运行/解释ARM64的实验。

root当前空/状态-only录制修复已有此SDK独立依据，不再需其它渲染后端作为规范。native仍须自行维持其真实terminal ordering、资源pin和handle生命期；不是复制原SDK的Maxwell/control格式。

## SamplerBuilderSetDefaults 实际值

Defaults dictionary355 / slotc64970 / RELA→473340。Getters的对应字段读均另有dictionary/RELA证据，不能仅根据store猜字段：

| 字段 | 原object offset | 原SDK默认 | 来源 |
|---|---:|---|---|
| device | 0x00 | 保留 | Defaults无写该字段；GetDevice=473490 |
| min/mag | 0x08/0x0c | LINEAR1 / LINEAR1 | 47336c形成四个整数1，再在473378写入从+8起的对应字段；GetMinMag=4734a0 |
| wrap S/T/R | 0x10/14/18 | REPEAT1×3 | 上述qstore前两个wrap；473340..344另写R1；GetWrap=4734c0 |
| min/maxLOD | 0x1c/20 | 0.0 /1000.0 | 473350读a4c2f0，473358存+1c；原常量u32={0,0x447a0000}；GetLodClamp=4734e0 |
| bias | 0x24 | 0 | 473370写0；GetLodBias=473500 |
| compare mode/func | 0x28/2c | NONE0 / LESS2 | 47335c读a4c528={0,2}；473368写+28；GetCompare=473510 |
| border | 0x30..3c | 0×4 | 47334c清零四个边界颜色分量；三种Border getter均读同4words |
| maxAniso | 0x40 | 1.0 | 473360..364 64位store0x3f800000；GetMaxAnisotropy=4735c0 |
| reduction | 0x44 | AVERAGE0 | 同64位store高32位0；GetReduction=4735d0 |
| lodSnap | 0x48 | 0.0 | 473374写0；GetLodSnap=4735e0 |

当前后端存在两处与该SDK不符：min=NEAREST_MIPMAP_LINEAR4、minLOD=-1000。候选 `minimal-promote.patch` 仅修src/nvn.c：恢复minLINEAR、minLOD0；同一单mip nearest native资格域允许真实默认{0,1000}，profile与SetLodClamp共用helper；保留原已资格{-1000,1000}/{0,0}请求，不拓展任意LOD/mip/filter。默认LINEAR+REPEAT仍明确不具备native采样资格，不为通过Initialize改成nearest/clamp或返回假成功。

## 验证

`checks-1/proof.json`：实际candidate core+RAM backend102checks，GCC ASan/UBSan与Clang函数签名trap均PASS；NDK29 `-O2 -Wall -Wextra -Werror` core与RAMfixture `--no-undefined`严格链接，readelf ELF64/AArch64。检查前后输入SHA一致。

检查真实默认getter、device保留、默认采样unsupported、显式nearest/clamp后{0,1000}getter/setter等价、原两个LOD子集、NaN/Inf与其它LOD拒绝且不改状态、实际empty/state-only handle/arena引用与回收。没有PM4/GPU、原模块或ADB执行，不能用来声称更广的sampler过滤或游戏首帧已资格。root另负责实际硬件及正式host/APK回归。
