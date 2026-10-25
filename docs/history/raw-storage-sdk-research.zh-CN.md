> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# RawStorageClass ABI / LINEAR 最小候选

实现依据只有本游戏原 SDK 的静态 ARM64、原 main 的调用消费方式及公开独立重建的枚举。没有 Metal runtime/renderer 依赖；没有采用旧实现的无条件常量返回、布局或渲染行为。没有读取 Vulkan，实现仍在独立 native KGSL core。

## 可复核原始输入与 ABI

`static.json` 固定原 SDK SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，原 main SHA256 `55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e`，两者各三段均按原 NSO header hash 验证。SDK仅在研究脚本 RAM 解压，没有执行 ARM64、CPU 模拟或生成加工游戏binary；报告目录只保存必要元数据/反汇编。

- 原 SDK bootstrap suffix 字典 index448 = `TextureGetRawStorageClass`，fallback slot `sdk+0xc64c58` 的原RELA明确指向 `sdk+0x477590`。不是按函数外观猜地址。
- `sdk+0x477590..4775e8` 只从 X0 指向的纹理读取；没有其它参数、无写入/out pointer；所有返回写 W0。因此是单个 `const NVNtexture*` 参数、32位标量返回。公开重建 `nvn::RawStorageClass` 的枚举含负sentinel，候选用32位 `NVNrawStorageClass` enum并有sizeof/PFN断言。
- `477590` 从object+0x20取flags，`477594..47759c` 测0x50；非零则 在4775a0分支返回整数1。本仓库重建公有flag正是LINEAR0x10、LINEAR_RENDER_TARGET0x40。
- 无这两个bit时按object+0x38 format分支返回2/3/4/5。独立公开[Odyssey重建RawStorageClass](https://smo.monsterdruide.one/docs/structnvn_1_1RawStorageClass.html)列出NONE、LINEAR、COLOR_AND_ZF32、S8_AND_Z16、Z24_AND_Z24S8、ZF32S8，与原函数返回值分类吻合；不把重建页面称正式NVN规范。
- 分别由原 `TextureGetFlags`（sdk+4768d0）和 `TextureGetFormat`（+476930）确认字段offset20/38。`TextureGetStorageClass`（+476880）却返回object+0x70 byte OR 0x100；后者是另一套kind标识，不能混作RawStorageClass或将其编码交KGSL。

## 原调用者的可证实用途

原slot main+0x320f048有8次静态读址，见8份 `main-*.asm`。前四组小函数经X0=输入纹理对象+8调用，使用W0与1比较；不等则对caller对象+0x334或+0x314的状态字OR0x100。另两组各调用两次，把第一次W0存W21，再与第二次W0比较，不等同样置0x100。精确BLR为17489a8、17489e8、1748a28、1748a68、1748ab0、1748ac0、1748b0c、1748b1c。

这证明游戏使用布局类别决定一项状态差异；未证明该bit的完整调度含义、动态调用顺序，也不宣称这些调用已在Android实测发生。不能据此总返回1让比较绕过。

## 候选范围 / 元数据接入

`minimal-promote.patch`只动3文件：

1. `third_party/nvn/nvn.h` 添加原ABI恢复的enum及 `PFNNVNTEXTUREGETRAWSTORAGECLASSPROC`；
2. `src/nvn.c` 添加getter，通过真实object类型/生命期检查，再按现native `linear_texture_bytes` 验证已支持layout及实际storage size，成功返回LINEAR1；
3. `src/nvn_entries.inc` 注册标准typed入口。

现 `linear_texture_bytes` 是原生Adreno LINEAR资格布局计算，不是Maxwell swizzle转换。有效1D/2D、已支持format、单mip、非MSAA的实际LINEAR对象可返回1；未知/tiled/MSAA/LINEAR_RENDER_TARGET/packaged不因此获得支持，仍在资源创建或防御性getter验证处显式拒绝。getter是资源元数据查询，没有需要伪造的GPU工作；它没有执行图像复制、压缩、queue等待或任何游戏逻辑。

名字提取/typed wrapper沿**原有** `extract_nvn_names.py` 规则，不改生成器：候选公共PFN使原NSO正常重提取后public ABI由477变478、typed forward由476变477，unknown由46变45。`nvnTextureGetRawStorageClass` 生成VALUE typed包装，保留before/after sticky fail-closed；不是仅把名字塞入resolver。候选root中的工具是原源码副本，原NSO bytes不复制到源码；正式合入仍须root正常canonical构建刷新记录，不能复用候选产物交付。

正式测试中旧 `resource_copy.c` 的 `Raw=NULL` 断言须由root同步改为typed非NULL+实际LINEAR1；新候选测试直接覆盖真实getter/typedwrapper，未悄悄跳过旧断言冒充回归。

## 最终检查与交付边界

`checks-2/proof.json`全部通过，保存源输入SHA/完整argv/stdout/stderr/产物hash，且检查前后输入一致：原name generator正常提取及两次`--verify`；篡改生成header明确因`cached artifact differs: names.inc.h`拒绝；GCC ASan/UBSan与Clang函数签名trap；NDK29 core严格编译及typed HLE_FORCE fixture `--no-undefined`链接，readelf确认ELF64/AArch64及swforce section。

HOST实际执行86项接口/ABI/生命周期/错误观察检查，另65536字节仅检查原backing未写；总65622不能当65622种API或GPU资格。包括实际初始化LINEAR纹理的1D/2D与5个format、返回class1、非linear/MSAA/多mip/未知format防御拒绝、Finalized/NULL对象错误、typed before/after sticky终止、其余未重建ABI仍fail-closed。

没有执行原ARM64模块、GPU/PM4、真实Android设备或ADB；NDK fixture是HOST测试源码的静态链接产物，绝不作为production或真机图形证明。无需为此getter伪造GPU工作。正常canonical host/APK重建、旧fixture预期更新及后续原game运行均由root串行完成；当前并不声称原game已调用此getter或已出首帧。
