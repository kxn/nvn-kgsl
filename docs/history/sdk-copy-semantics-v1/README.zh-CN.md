> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# NVN Copy 家族独立语义审计

仅原SDK53.314静态ARM64、公开重建NVN声明与独立deko3d源码；无Metal/Vulkan renderer、ARM执行/CPU模拟、ADB、GPU或runtime修改。原SDK反汇编只在本ignored目录，不归档进Git。`proof.json`记录原NSO/三段完整性、方法dict/RELA、各方法hash、18条静态模式检查及公开源码SHA。

## 来源与版本

- 原 `Update/ExeFS/sdk` SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，build-id `2d369d9999e3e10e665106f840ffff9bdaa0bb8e000000000000000000000000`。正常 `research_ubo_sdk.py --method ...`抽取dict523实际fallback，raw-v1与init-v1各重新验证原NSO及三段。原SDK offset均以此映像为准，不能套公开53.105实现。
- [open-ead/nnheaders pinned C++声明](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nvn/nvn_Cpp.h#L961) 与对应C PFN/FuncPtrInline；保存副本在既有 `parallel/public-nvn-definition-index/public`。这是公开重建定义，不是完整官方行为规范；其来源版本与本实际53.314不同。此处只用来交叉核签名、整数enum与方法身份。
- 独立 [deko3d image transfer](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/source/dk_image.cpp#L697) 和 [Maxwell transfer emit](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/source/maxwell/gpu_transfer.cpp#L28)，当前锁commit373bfd43d8b2d805fa2eb7d406dbb823032df958，来源与SHA在public/provenance.json。它是独立API/实现证据，**不是NVN规范**，flag位也不相同，不能直接复制常量或debug合法域。

## 公开ABI与实际raw值

三个C入口均void，纹理/view/region为const指针；flags为int。NVNcopyRegion为六个signed32的x/y/z offset,width/height/depth；buffer端是64bit GPUVA。CopyTextureToTexture有独立src/dst region，不能从签名假定二者尺寸相同。

CopyFlags精确值：NONE0、LINEAR_FILTER1、ENGINE_2D2、MIRROR_X4、MIRROR_Y8、MIRROR_Z16。公开名称只证明位身份，不证明每方向/格式/尺寸下所有组合合法。未知高位不能默默mask。

| 原SDK方法 | offset | 独立静态事实 |
|---|---|---|
|CommandBufferInitialize|45cd70|45cd84用64bit零一次清row/image两个32bit字段，初值0/0|
|SetCopyRowStride|465bb0|W1→command+88，无setter校验/命令发射|
|SetCopyImageStride|465bc0|W1→command+8c，同上|
|GetCopyRowStride|465bd0|LDR W0读+88，X0结果为零扩展low32|
|GetCopyImageStride|465be0|LDR W0读+8c，同上；该方法只到首RET，工具按相邻public边界抽出的后续helper不是getter|
|BeginRecording|45d170|完整方法未写+88/+8c，不能按每Begin清0处理；两个值为command对象持续CPU元数据|

公开ptrdiff_t为64bit，而本SDK store/read只有low32；这是已证release数值行为，不表示负数/超过consumer硬件域都合法。setter可保留raw低32但后续copy须验证后端可消费范围，不能以setter成功承诺GPU能力。若实现getter，不能把LDR W0错误变成负int32→signed64扩展。

## 每个copy实际做什么

### Buffer→Texture（464590..464bec）

- bit1(ENGINE_2D)在4645bc切换不同consumer。无bit1为DMA/copy helper464170；有bit1经2D helper465650，不能把两个路径当同一个flags0字节实现直接放开。
- view未给format时继承texture；presence bit2才用view format。其blockW/H来自所选格式表。
- 对压缩块，464644..678把xoffset、yoffset、width、height分别取ceil(/formatBlockW/H)。这里看不到“未对齐参数合法”的validation；release直接ceil不等于公开允许任意非对齐region。
- 46467c读row/image stride。row==0时取 ceil(width/blockW)*bytesPerBlock；image==0时取 row*ceil(height/blockH)。两个stride均为**字节**，不是像素数。显式image stride不是“行数”。非0直接作为raw值传入，后端不应把负/过小/溢出等未资格值照发。
- bit3/bit4通过移动线性buffer起始地址并取负row/plane stride实现Y/Z翻转；bit2的X翻转只在2D路径看到相应坐标步进。filterbit0在此方法未看到作为filter参数传给2D helper（调用处写filter=0），不据此承诺它在B→T是合法no-op。
- 正常2D array按region.z加view.baseLayer逐层调用view resolver，buffer按image stride推进；target1D_ARRAY(3)把API的y/height作为层域，不能沿2D假设处理。

### Texture→Buffer（464bf0..464ef8）

- 相同ceil块单位与row/image字节默认算法，464cb8..cd4。
- 相同空view/presence继承与层偏移，调用47609c后DMA helper464170；bit3/4分别处理buffer方向Y/Z翻转。此方法没有观察到ENGINE_2D/filter/X切换的对应实现；不能把另一方向的非零flags资格直接继承。

### Texture→Texture（465050..465640；465650起是独立共用2D helper）

- independently解析src/dst view格式和baseMip/baseLayer，支持独立src/dst region。
- 465158..160在目标width/有效height/depth任一为0时提前返回，**此前已读过texture、view与部分region**，不是允许所有NULL指针的no-op保证。B→T/T→B没有等价的全部维度早退证明；zero区域合法域仍未知，首版可明确拒绝。
- flags0也会在465278/284按src/dst区域尺寸形成定点步长，调用2D helper；不是“API flags0只允许same-format/equal-size”。不同格式路径也存在。LINEAR_FILTER在46529c与465498参与filter设置，mirror三轴调整源起点/步长。完整scaled/format-converting语义不在新raw-block内核资格范围。
- 压缩源路径以块域ceil处理src/dst坐标/尺寸，并调整实际view resolver输出。不能把压缩块作为RGBA texel解释，也不能把NVN copy flags当Adreno FDL descriptor flags。

## view与压缩边缘的最小明确规则

现原SDK `47609c` resolver与既有 `parallel/texture-view-sdk/view-resolver.asm`已核：空view或未presence字段继承原texture；指定base mip改变尺寸和source offset；baseLayer参与实际存储偏移，layerCount0在相关array目标表示剩余层。copy方法构造单mip/单层临时view逐层调用；原view的mipCount不能被理解为“每次copy自动复制整个mipchain”。格式override会调整block几何；swizzle/target/mode各自是否影响copy须逐消费核实，不能仅因采样view支持就宣布copy支持。

建议第一批公开实现明确是**flags0等尺寸同格式raw-block子集**：

1. 在所选实际mip的pixel域验证非负offset、正extent、加法无溢出、完整src/dst范围与层范围；支持的target目前2D/2D_ARRAY，其它target显式ENOTSUP。view由值快照解析，不保存caller指针，不把count0固定当1。
2. 压缩源/目标block几何和bytesPerBlock必须相同，格式先要求完全相同，禁止隐式decode/encode、sRGB转换或formatreinterpret。x/y起点要求block aligned；width/height允许非block倍数仅在该方向恰到真实所选mip边界，T→T两侧都满足；ceil后复制完整8/16B块。这是保守native能力界线，尚无证据称它等于全部NVN合法域。
3. row0→tight block-row bytes；image0→row*blockRows；显式row>=rowBytes，image>=该slice occupied span，先严格限制非负native可处理范围；计算last layer/row/byte span而不是row*height粗略读取padding。显式stride是bytes，非4B行尾与非零base不能越界load/store。alias/overlap按实现原子性和owner合同保守拒绝，不把copy等同memmove。
4. source canonical BO/extent/view metadata保持真实；GPU内核只变source storage布局/搬块，不CPU搬像素。记录immutable view/stride/region快照、source/dest强refs，enqueue所有consumer gate通过才发射；出错前不改command bytes/refs。
5. zero区域若实现no-op，先独立定义和测试NULL/非法对象是否仍验证、是否改变commandbytes/refs/sticky；不能把T→T的一条release早退泛化到三族。当前一律明确拒绝零区域不会假报copy成功。

## 哪些非零flags可做、哪些必须仍拒绝

- **当前完整可资格范围：flags0上述有限raw块copy。** 所有非0/未知位可先ENOTSUP，属于明确partial backend，不能silent清位。
- 后续uncompressed MIRROR_X/Y可用GPU索引变换准确实现，MIRROR_Z可按plane/layer逆序；必须用原对应方向实际源/目标语义与GPU矩阵验证。它们不要求CPU像素转换。
- 压缩MIRROR_X/Y不能只反转BC块顺序：每块内部texel/selector排列也须改变，边缘块尤其不同；新raw-block kernel没有该能力，严格拒绝。压缩MIRROR_Z仅整层重排理论可rawcopy，但仍是后续单独资格，不能继承X/Y。
- LINEAR_FILTER/缩放/转换涉及像素语义与源坐标精度，raw-block shader不满足，继续拒绝。即使1:1时某flag看似inactive，也需明确源条件与证明，不能通用mask。
- ENGINE_2D虽名字像路径选择，但原SDK确实选不同consumer；若以后以高通GPU kernel实现相同公开结果，应独立证明合法域与format行为，不伪称执行NVIDIA2D engine，也不能仅把flag忽略算支持。

## 对照限制与下一批必要资格

deko3d的独立debug path明确reject零region、压缩flipX/Y，并用相同ceil块算法和字节rowLength/imageHeight；copy/blit是不同API，flag位不同。它的压缩BlitImage当前源码还有疑似src坐标被dst坐标覆盖，不能拿实现细节当NVN答案。本文只将其用于互证可实施机制，不移植该实现，也不据其debug规则给NVN全域定性。

最小GPU矩阵应有三方向、tight/显式row+image strides、mip/layer view基址、BC非倍数真实边缘（例如7×5、mip3×2）、interior不对齐负例、源/目标非零base与guard、replay修改源、read-onlypadding及alias拒绝；返回按原source canonical byte oracle核对。CopyFlags0完整矩阵通过前不宣称此族全NVN支持。没有GPU运行结果在本报告中。
