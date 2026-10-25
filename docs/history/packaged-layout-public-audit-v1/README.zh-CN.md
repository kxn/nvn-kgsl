> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# PackagedTextureLayout 公开证据（只读，2026-10-24）

结论：可确认声明为8个连续字节；本轮公开第一手来源尚不能确认其字段编码。没有编写布局解码或放宽运行时。没有读取/反汇编原SDK CPU代码，没有重试此前拒绝的静态任务，没有读取Metal/Vulkan实现，没有ADB。

## 已确认声明和适用版本

1. [open-ead/nnheaders pinned246c3b48的C声明](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nvn/nvn.h#L227)是unsigned char layout[8]；[同revision C++声明](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nvn/nvn_Cpp.h#L1113)同为8B。SetPackagedTextureLayout接受const指针，GetPackagedTextureLayout的公开C++函数类型返回NVNboolean并通过输出指针交付layout。布尔返回1不能解释成blockheight=1。字段没有注释、掩码、版本号、保留位定义。

该头开头明确是Darksiders II DWARF重构，覆盖NVN53.105但可能含更高版本元素；不是NVIDIA发布的完整SDK规范，不能自动视为D2R SDK53.314编码证明。文件为MIT。现有公开Woboq/Doxygen页面只重复这些声明，未查得额外语义文档。

HOST declaration_abi.c使用该公开头实际编译/执行，验证size8/alignment1/offset0以及setter函数指针签名。abi-proof.json记录编译器/源哈希。它只证明公开声明的C ABI，不证明运行时读取方式或每字节含义。

## 作者容器源码：线索与缺失连接

2. [KillzXGaming/BntxLibrary e5c8d20c](https://github.com/KillzXGaming/BntxLibrary/blob/e5c8d20cb5aed9262b16d7be9e0e48e0f9c43987/BntxLibrary/BntxFile.cs#L436)将BNTX容器的TextureLayout低3位公开为BlockHeightLog2；ResBntxTextureInfo有相邻u32 TextureLayout/TextureLayout2，构造默认4和65543。MIT。ReadStruct/WriteStruct按作者定义读取/保存容器。这些是该容器作者的字段解释；未出现原生NVN SetPackagedTextureLayout调用，无法由名称、8B宽度或相邻关系推出NVN编码相同。默认65543也不能成为生产魔数。

3. [ScanMountGoat/bntx 87fffc7d](https://github.com/ScanMountGoat/bntx/blob/87fffc7d17238cb07ff93ec75e0a7b5db15f4683/src/lib.rs#L206)独立定义BRTI block_height_log2及六个相邻unknown u32，保留未知。不是NVN包结构定义，也未桥接NVN setter。所查根目录LICENSE未找到、Cargo.toml无license字段；本轮仅源码研究，不并入实现。

4. [XTX-Extractor作者66489b37](https://github.com/aboood40091/XTX-Extractor/blob/66489b373faa96e65bd0a0e87c266b6756f10567/xtx_extract.py#L79)读取NvTextureHeader的imageSize、alignment、尺寸、target、format、mips、sliceSize，另外读取17个mipOffset，随后跳过header剩余部分。没有解释NVN layout[8]。GPL-3.0-or-later，未复制其算法进运行时。它可作为独立容器元数据来源，不提供缺失的API编码。

网页搜索出现过使用Vulkan的移植项目和本项目旧renderer片段；未打开/使用这些实现作为依据。二手wiki将某字段标“Packaged texture layout?”，问号本身不能替代作者原始NVN桥接证据，故不作为确定事实。

## NVIDIA原厂原语

5. [NVIDIA/open-gpu-doc pinned9fdf5c40 clb197tex.h](https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clb197tex.h#L424)发布硬件TIC的GOBS_PER_BLOCK_WIDTH/HEIGHT/DEPTH位域，HEIGHT枚举0..5对应1..32 GOB。这是MIT原厂硬件描述符依据，能验证已独立得到的GOB参数是否合法；TIC是另一种硬件结构，不能把这些位位置搬进NVN8B。类似deko的布局结构也仅能用于物理布局独立核对，不能建立API编码等价。

## 对当前原游戏观察的最小建议

- 等root正常v12 observer交付完整8B、presence、真实stride/flags/target/尺寸/mips/samples/device/source policy，以及首次失败getter。仅记录原始字节；若另输出LE u32/u64，必须标诊断重解释而非已解析字段。
- 先区别packaged非NULL路径和implicit路径；即使8B全零也不能据本轮资料解释为default。保持typed setter按值快照与Get输出，不根据Boolean返回猜编码。
- 对128² BC3/mip8继续调用已有source calculator作独立metadata核对；它成功只排除implicit raw几何不支持，不能证明packaged layout被正确消费。实际getter95可能来自packaged/stride/flags/其他gate，不能先改mips或mask标志。
- 不增加游戏地址/8B哈希白名单。要实现后续可接受子集，应补齐“公开作者原生NVN调用明确把哪些容器字段传入layout[8]”或适用版本的公开字段说明，并验证version/reserved/kind/mip/array/stride。仅图形内容正确、默认尺寸一致或容器名称相似都不足以确认所有字段。
- 若有合规可用的原厂packager输入/输出及声明许可，优先用其产物+独立容器作者metadata建立有来源的对照；本轮未获取或运行非公开packager，不建议下载不明SDK泄漏。

## 本轮交付与边界

facts.json包含精确revision/哈希/缺口；downloads.json只pin已选公开作者容器源，未拉整仓renderer。repositories元数据及公开源保存在本ignored目录。abi-proof为唯一执行测试，HOST ONLY；无Android/GPU/原SDK执行。没有生产patch，因为当前没有足以安全写8B decoder的证据。这个结论仅说明本轮检索范围，不能宣称整个互联网不存在其它资料。
