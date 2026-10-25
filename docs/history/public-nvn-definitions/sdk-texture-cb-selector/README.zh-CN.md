> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原 NVN SDK 图形 texture constant-buffer selector：已证实为2

**限定结论：在本次已核验原 D2R SDK53.314 的成功 QueueInitialize 图形路径中，驱动显式将 texture lookup 默认 constant-buffer 槽设为2，初始化命令在函数返回前交给原通道 kickoff。** 因此此前 C1 非bindless TidB110/112 的默认CB槽未知项可以关闭；不能再把`CbufSlot=-1`直接当无法确定任何来源。这个结论不依赖旧Metal/Vulkan、deko的CB编号、shader archive的2D预期值或一次探针绑定。

只做原始 NSO 静态解析/反汇编和公开 NVIDIA 字段核对；没有 CPU/MME 解释器、模拟执行、ADB、GPU观测或生产修改。`proof.json`/`sdk-path.asm` 含原SDK窄范围指令，仅留ignored；本目录 `static-facts.json` 仅保存进一步去除指令文本的静态事实。`prove.py`复用已存在的原NSO读入和Capstone，不新写ISA解码器。

## 输入与来源

- 原SDK NSO SHA256：`1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，build-id `2d369d9999e3e10e665106f840ffff9bdaa0bb8e000000000000000000000000`；三段SHA从原NSO重新解压核验，完整raw SHA及每证据范围SHA在proof。未改任何原字节。
- NVIDIA FIFO：[clb06f.h](https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/host/clb06f.h)，lines225–229定义IMMD address11:0、subchannel15:13、data28:16、opcode31:29=4。原厂下载SHA及URL记录于`fifo-source.json`。
- NVIDIA graphics：[clb197.h](https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clb197.h)，lines4105–4106的SET_BINDLESS_TEXTURE字节方法2608、slot字段4:0；lines4094–4098的stage BIND_GROUP_CONSTANT_BUFFER及valid/slot字段。A197/A297/B097的同名方法也为2608/4:0；本地pinned原厂头逐文件SHA记录在proof。
- 原C1输入、existing frontend instruction decoder、TidB和API绑定函数证据沿用冻结的`../c1-handle-source-research/metadata.json`，其SHA在proof。该前轮明确留下的默认CB2缺口正是本轮补足的部分。

原厂头带NVIDIA2020 permissive MIT-style notice。原SDK非公开源码，仅保存输入哈希/本地静态事实，不分发其NSO或反汇编到docs。

## 直接写入，而非常量猜测

SDK+48ac54、48ac58先后构造word `0x80020982`，48ac5c把它写入当前命令cursor后8B处。依据上述原厂FIFO字段，word拆分为：opcode4（IMMD），subchannel0，方法word982（字节2608），immediate data2；该方法有效slot字段4:0，结果恰为2。它是一条完整即时方法包，无另一个payload DWORD。

这段在48a900初始图形状态函数内。此前字符串/原始word扫描找不到`0x80020982`，是因为该word由CPU指令运行时构造；不能把旧literal扫描阴性当不存在设置。新的静态反汇编扫描找到唯一直接`mov ..., #0x982`构造点48ac54；另外两处`add x1,...,#0x982`属于其它代码地址计算，没有被当作GPU方法写入。

## 每个成功 QueueInitialize 的路径和时机

1. 原523名dictionary/RELA解析出 `nvnQueueInitialize` 为4723e0，尾跳471630内部实现。原SDK成功路径471ae0调用4886b0；后者从通道对象查询的class字段queue+EC构造subchannel0的SET_OBJECT。其它引擎使用另外的subchannel。这使后续selector写入属于同一queue的图形class，不是compute stage猜测。
2. 紧接471aec调用48a900；48ac54–5c的selector写入经过可选配置分支的共同汇合点，无分支在该路径直接跳过它。原SDK在471af8才设置初始化成功结果；前面的失败路径跳cleanup或返回false。唯一直接48a900调用点即此471aec。
3. 48a900的末段还调用48a260。该函数以物理graphics stage0..4循环，构造BIND_GROUP_CONSTANT_BUFFER(j)方法并用immediate payload21，按原厂字段解为valid1、shader-slot2。各stage的buffer地址来自queue所属原BO及分段offset；没有把所有stage绑定到同一个假指针。stage API→硬件映射表`0,4,3,1,2`也已按原字节记录；fragment API1对应硬件stage4。
4. QueueInitialize在471b10调用4721dc内部flush body；公开`nvnQueueFlush` dictionary入口4730f0单条尾跳同一body。内部先把当前命令范围封成GPFIFO entries（472980→46eb38），再472348调用4727c8。
5. 4727c8通过RELA恢复的dispatch：c79fc8→c64ed0→4727c0，后者把queue+18传给46ecf8；再46ec18到PLT58a030。JMPRELA64d410/GOTc74b28的实际符号为 **NvRmGpuChannelKickoffPbWithAttr**。所以这是实际初始化提交路径，不是未被用到的命令模板/常量数组。

“返回前kickoff”是CPU调用/队列排序事实，不声称GPU已经在Initialize返回前执行完成，也不是Android上执行原SDK的观测。本结论逐原NVN queue的graphics初始化成立，不泛指系统所有GPU context或compute默认槽。

## C1资源选择与必须保留的边界

既有原指令解码证明非bindless lookup的TidB为110/112，表示默认CB中的DWORD索引。因此本SDK图形默认槽为2时：

|原TidB|CB槽/byte offset|原NVN绑定路径|
|---|---|---|
|110|CB2 +1B8|原SeparateTexture基址168、stride8 → selector10|
|112|CB2 +1C0|同理 → selector11|

原BindSeparateTexture在481734按stage表选图形stage，481758–76c写`168+8*selector`的低DWORD。这个word对应原TIC/texture ID载体。C1的这些TXQ/TLDS为samplerless，**不引入SeparateSampler0**。完整guest高word与scalar/plural写入差异仍按前轮handle-carrier审计保留，本结论不等于补齐整个driverCB2仿真。

TidB112的原始TLDS仍是1D编码；不能把它的实际NVN target、buffer-vs-image、format/size或QuerySamplerType，从旧testspecialization=2D反向填入。后续resolver必须用确切stage/selector10或11、当前descriptor版本和实际registered typed resource决定；native未支持的1D/buffer路径仍拒绝。CB2中其它显式UBO字段/未知支持数据也不因这项结论自动放开。

## 后续覆盖审计范围

- 全SDK text直接调用48a900只有471aec；直接0x982构造只找到48ac54。相关standard BindTexture(s)/SeparateTexture(s)/SeparateSampler(s)更新driver常量数据，既有原始函数范围已留存；本次没有发现它们生成另一个直接SET_BINDLESS_TEXTURE值。
- 同一原SDK新queue会重发这段初始化。底层不同context/直接raw方法注入、动态计算或MME内部方法，不能由一次立即数扫描穷尽证明“不可能覆盖”；本报告不作这种全程序声明。
- 该限制不抹去已找到的正面初始化证据。生产可以把“已核SDK53.314的standard NVN graphics路径默认CB2”作为明确版本合同，并继续对未知driver、未知显式CB字段、未经解析的资源状态fail closed。不要把它升级成所有NVN版本/所有context的硬件reset默认值。

## 复现

在本worktree用现有venv执行 `prove.py`：重新核原SDK三段、字典/RELA、上述CPU指令构造、FIFO字段、初始state唯一直接caller、实际kickoff符号、五stage CB2绑定及110/112→10/11数学关系。输出proof不含执行或性能数据。`ready.json`固定工具、说明、脱敏摘要和证据SHA。

冻结复现包在本工作目录 `.local/nvn-native/parallel/sdk-texture-cb-selector/`；报告引用的原始 proof、asm、工具和 ready receipt 保留在那里，不随文档分发。公开来源的 URL/SHA 随本报告保存。
