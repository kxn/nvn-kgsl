> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# COMPRESSIBLE、设备策略与 source-kind：原 SDK 静态事实

本报告只读取 D2R 3.3.94012 Update/ExeFS/sdk，验证 NSO 与三个解压段 SHA 后反汇编；不执行 ARM64，不含 Metal/Vulkan 行为依据，无 ADB/GPU/真实模块运行。`research.py` 可重现定位和 JSON。输入完整哈希/段哈希/BuildID 见 `static.json`。

## 已证实

1. `nvnDeviceInitialize` 字典220入口 SDK+466190，读取全局 c78d80（RELA -> c64eb0），首函数槽再 RELA -> **46af70**，后者依次调用 **46a120**、**46adb0**。不是猜测直连入口。
2. 46a140..164 清零整个 device 0x1b58。46a1d0令 x20=device+19f8；46a2b4、46a2c8把 literal **a3f680** 四个 u32 `{0,0,1,0}` 写到这里，所以 device+**1a00 默认1**。
3. 46a688..698 传 x2=&device[1a00]、x1=SDK+a1d0e1字符串 **`0x9abdc5`** 调用 46e804。46e810..824从全局运行时 hook 表（c78d70 -> d2a4b8）+0x60取得字符串；不为空则以 575700 做 base0 数字转换，57bb40返回的错误槽为0才在46e864覆写u32。该转换的具体 libc 名未通过 symbol表证明，不能借此声称它一定是环境变量接口；可确认是可覆写的数字配置。
4. 46ad2c..48 写 **device+1b1c = ((word & 2) != 0) || ((word & 4) == 0)**，word来自1a00。默认值为1。其直接来源是SDK设备策略/配置推导，不是直接复制某个 NvRm 硬件 capability 查询结果；bit1/bit2的原符号名尚未知，不能命名为已恢复的公开枚举。
5. 布局函数475b50中，flags&0x50非0才走LINEAR/LINEAR_RT；**DISPLAY1不令资源自动变成linear**。flags9保留COMPRESSIBLE8并走tiled路径。475db0..475e48只有flag8开、device+1b1c非0且格式条件允许时才请求压缩。device+1b1c=0明确令 **W3=0**，不清flags8。color还受device+19f8 sentinel 0x76165656禁用；depth/stencil使用19fc。同条件在GetAlignment/GetStorageClass重复。
6. RGBA8=0x25的48B格式记录在 **c62718**：+8=1、+e/+f=0、+10=4，满足color分支。默认19f8=0不等sentinel，故此SDK默认策略的flags9会请求压缩；native禁止NVIDIA压缩是明确的后端策略选择，不能伪称与该SDK默认策略完全一致。
7. 46e6bc把 W3&1 存临时结构byte+4，再经PLT58a000/GOTc74b10/JMPRELA64d3c8调用真正外部 **NvRmGpuDeviceChooseKind**。返回kind由该外部函数写入，SDK单体不包含实现；**不能仅靠本段证明compress=false必定返回0xfe**。返回失败则helper返回0，亦不能把0当成功的Generic种类。
8. `TextureInitialize`476440的4766a8..6bc对flag8要求 backing pool flags&0x280（COMPRESSIBLE0x80或VIRTUAL0x200），否则false。即使native选择不压缩也必须保存与验证这层eligibility，不能mask flag8后略过pool合同。

## source-kind / size / alignment

- 独立公开primary：[libnx pinned types.h](https://raw.githubusercontent.com/switchbrew/libnx/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/nvidia/types.h) 定义 `NvKind_Generic_16BX2=0xfe`。这是种类数值证据，不是NVN COMPRESSIBLE完整规范；不宣称Tegra压缩布局或Adreno UBWC等价。
- GetAlignment475540：bit6优先返回128；否则bit4返回32。普通tiled调用ChooseKind后，kind低8为0或fe返回512，其他kind返回device+ac。这里的512是**明确SDK getter分支**，与单独已知的GOB大小概念分开。
- layout475e74..9c：计算mip/array payload之后，kind为0或fe不做额外page round；其它kind按device+ac上取整。device+ac不是凭空固定64K：46e6b0把device信息临时记录交给46e944，46e970..974调用PLT58a020（JMPRELA64d3f8确认 **NvRmGpuAddressSpaceGetInfo**）写stack结构，46eab0..b4复制该结构+14到记录+ac；deviceinit46a1e4..288再复制此记录到device。字段正式公开名/当前真实值未恢复，因此本报告不填数。
- “非NVIDIA压缩 + 明确sourcekindGenericfe”可定义稳定native策略，按已经恢复的普通2D/mip/array公式算payload，并由fe分支给publicalignment512。**这是后端选定source-kind合同**，不是证明原默认SDK执行会返回fe。使用者必须在初始化、getters、descriptor、copy/readback、别名和后续GPU消费者上贯彻同一布局；尚未转换的消费者应明确拒绝。
- 2880×1620 RGBA8单mip不代表现有64×64或AHB LINEAR资格自然覆盖；source tiled backing与Android display backing仍需独立layout转换/同步/alias/格式尺寸资格。DISPLAY保留也不等于Window能展示该资源。

## 建议不可变native策略

| 项 | 当前可实施合同 | 仍不能宣称 |
|---|---|---|
| COMPRESSIBLE flags8/9 | 保存raw flags；设备native_no_nvidia_compression固定；验证pool eligibility | 原SDK默认compress开时的实际kind相同 |
| source layout | 支持范围内显式选择Generic16Bx2 fe，公开storage class=0x100\|fe，rawclass依格式 | 任何NVN/tiled/MSAA/sparse格式通用 |
| source storage | 真正pool/offset/size/alignment/refs初始化，未知布局拒绝 | 仅metadata成功便已GPU可用 |
| Adreno image | 另有明确shadow/转换/版本/同步后才准GPU使用 | Maxwell压缩bytes可直接给Adreno |
| DISPLAY | 保留意图和resource合同；Window额外资格 | 单靠标志自动转linear或首帧 |

## RawStorageClass补核

`477590..5e8`：flags50->1；否则format<=36(hex)查表abf113；base4775cc + 4*tablebyte。hex格式集合 `{1,5..9,25..30,38,41..50,9a,9c}` 全部返回2，逐项元数据在 `raw-format-classes.json`。<=36者tablebyte4->4775dc；>36直接4775dc。注意这里格式写法为十六进制；十进制49/50实际是0x31/32，返回3，不能误读。

## 证据边界

没有运行ChooseKind、原SDK或GPU；没有证明所有可能的SDK配置值；未恢复动态runtime hook的来源注册方或数字配置键的人类名称。网页仅补kind数值，行为结论来自同一原SDK的静态控制流。不存在Metal实现/常量依赖。未改tracked源码。
