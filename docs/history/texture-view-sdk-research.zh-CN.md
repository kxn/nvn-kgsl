> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# TextureView 与 builder 生命周期：原 SDK 独立核验

范围：只读原SDK静态ARM64和公开声明；无Metal/Vulkan依据、CPU解释/游戏/GPU执行、ADB、tracked修改。当前native中的历史`original HLE view_t`说明不能作为规范；以下提供可替换它的独立依据及实际实现差异。

## 来源与方法

原始 `Update/ExeFS/sdk` SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，build-id `2d369d9999e3e10e665106f840ffff9bdaa0bb8e000000000000000000000000`。此前独立确认实际SDK53.314。`research.py`重新验证NSO三段哈希、解析MOD0/DYNAMIC/RELA与523名dictionary→fallback，逐方法保存offset/hash/反汇编。只在RAM解压，不运行原ARM64。

公开声明来源（不是完整行为规范）：

- [Darksiders DWARF重建nvn.h](https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn.h.html)，文件23–26行明确适用53.105并可能混入较新元素；126–127行TextureView reserved40B，187–188行SamplerBuilder96B，220–221行WindowBuilder64B。
- [TextureView公开C++接口](https://smo.monsterdruide.one/docs/classnvn_1_1TextureView.html)：全部Set/Get及Compare、reserved数据，未出现Initialize/Finalize。
- [SamplerBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1SamplerBuilder.html)、[WindowBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1WindowBuilder.html)：只Set/Get和reserved数据，无资源Initialize/Finalize。公开C++setter返回引用是wrapper接口，不能改变已重建C PFN的void合同。
- 本轮重读FuncPtrInline页超时；未据其未成功返回的内容补充新结论。已存在public C PFN分别在项目third_party/nvn/nvn.h:1313起。

声明只能确定公开opaque容量与签名；实际字段/行为以下面已pin的原SDK为准，不能混用版本。

## TextureView 的实际值语义

方法 dictionary473..486，对应SDK475880..475b48；详见`static.json`。

| byte offset | 字段 | presence位 |
|---|---|---|
|0|bitmask；有效低6位|--|
|1 / 2|baseMip / mipCount，unsigned byte|bit0|
|4 / 6|baseLayer / layerCount，unsigned halfword|bit1|
|8|format，32bit|bit2|
|c /10 /14 /18|RGBA swizzles，各32bit|bit3|
|1c|depth/stencil mode，32bit|bit4|
|20|target，32bit|bit5|

- SetDefaults dict480/475880清零byte0..35（连续清零36字节；不发布原SDK指令摘录），不触40B对象末4B。默认是所有presence=false；不是强制mip/layer count=1，也不是显式format0/target0。
- SetLevels dict484/475890设置presence bit0再存两个byte；SetLayers dict483/4758b0设置bit1再存两个halfword。SetFormat4758d0、Swizzle4758f0、Mode475910、Target475a00分别设置对应bit并存值。没有指针、owner注册、alloc或GPU调用。
- GetLevels475930/GetLayers475950/GetFormat475970/GetSwizzle475990/GetMode4759c0/GetTarget4759e0：**先无条件写out参数，再返回对应presence bit**。默认false是合法“未指定”，不能被typed bridge当成失败。
- Compare dict473/475a20：先要求两view的presence低6位一致，再仅比较相应已指定字段。unused payload、高2个mask位、padding不决定相等。未指定与显式设置`(0,0)`即使最终解析同资源范围，也不是同一个view描述值。
- 字段存储宽度证明原SDKrelease实现会截取W1/W2，但不能由此声称负数或超出公开device能力的参数合法；合法输入范围仍应由资源大小、公开limits和目标API合同限定，不能把内部8/16位宽当“全范围支持”承诺。

### 实际消费者证明 count0 / base 语义

`TexturePoolRegisterTexture` dict468/4769d0在476a18调用491cf0；其view为X20、texture为X21：

- 491d34..491dc8：format/target/swizzle presence=false时继承texture；true时采用view相应字段，非空view不意味着所有字段都覆盖。
- 492040..49205c：对于相关array/cube目标，Layers已指定且layerCount=0时，用texture总层数减baseLayer；非0时直接用显式count。492010..492020将baseLayer*layerStride加到原GPU地址。
- 492270..4922f8：Levels已指定时读取baseMip；mipCount=0不收窄原maxMip；非0才用base+count-1收窄。实际TIC编码还受texture已分配levels及硬件上限限制；不是任意超范围都可采样。
- `TextureGetViewOffset` dict457/476890→47609c：baseMip参与尺寸与mip起始offset；baseLayer参与层offset。4763a8..4763b4同样在layerCount0时选择剩余depth-baseLayer。该resolver用于offset并非完整采样view验证，不能用它单独宣称swizzle/mode都已实现。
- 空指针view在这些消费者中走继承路径；它不需要一个已注册的native对象。

因此 TextureView 应是可赋值、memcpy、移动到新栈地址的独立值。后端可以采用自己的pointer-free opaque编码（不要求复刻Tegra私有offset），但presence、默认继承、指定范围及compare语义必须正确。不能把view地址登记成必须Finalize的resource。

### 当前 native 差异/限制

核对文件：`src/resource_layout.inc:43`起、`resource_copy.inc:22`起（实际行号以此次读取hash为准）。

- 当前28B私有resource_view加zero tail、无registry，支持复制本身符合value性质；与原SDK私有字节不同并不自动构成公开ABI错误。
- 当前只有levels/layers/format/target四类，尚无swizzle/mode及大部分Get/Compare入口；这些应明确是缺口，不能靠原HLE说明宣称完整view语义。
- 当前只资格single-level/layer/same-format/same-target；`base0,count0/1`在这份受限资源范围是合理子集。扩展mips/array后必须按上述remaining语义解析，不能继续把count0当固定1。
- native SetDefaults将整个40B清零，而原SDK只清36B。清padding属于内部差异；若未来Compare实现采用memcmp全40B，会错误地把padding当语义。当前resource_view_read要求其私有tail为0，这是本后端自有编码约束，**不是原SDK/公共NVN规范的保留区为0要求**。若允许原SDK内部创建的view跨入native路径，必须另行统一表示/边界，不能假称两者字节兼容。
- native setters的格式/target、计数范围资格与原SDK纯store不同，应作为明确partial profile；不能据source release纯store去接受未知格式并让实际GPU工作假成功。

## SamplerBuilder / WindowBuilder：仅值builder可重用栈地址

### SamplerBuilder

- SetDevice dict356/473330只是X1→[X0]；GetDevice dict343/473490读回指针。
- SetDefaults dict355/473340只写builder本身，保留device字段0；没有call/alloc/登记owner。默认值已由先前`parallel/sdk-defaults-recording/`完整核实。
- SamplerInitialize dict377/4735f0清零资源自身，再把builder前0x50B按值复制到资源+8..57，返回true；没有保存builder地址。因此之后重用/修改builder不能修改已创建sampler的描述值。
- 版本提醒：该原SDK函数清零sampler的0x60B，而公开53.105重建头NVNsampler只有0x30B。不能把公开旧头的private对象size/布局直接套到SDK53.314。现native采用out-of-line metadata，不按原函数写96B，因此这里不是已确认越界bug；测试/集成仍必须分开公开容量与原SDK内部版本证据。

### WindowBuilder

| 方法 | SDK offset | 行为 |
|---|---|---|
|SetDevice/GetDevice|4775f0/477650|device指针+8|
|SetDefaults|477600|count+10、numActive+14、arrayPtr+18、token+20、rawInterval+28清零；device+8保留|
|SetTextures|477620|count W1→+10；caller数组指针X2→+18；不复制数组|
|GetTexture|477670|检查index<count后，通过当前arrayPtr读对应指针；无owner/alloc|
|SetNativeWindow/Get|477610/4776b0|token指针+20|
|SetPresentInterval/Get|477630/477690|存interval-1；读raw+1；默认返回1|
|SetNumActiveTextures/Get|477640/4776a0|整数+14|

- WindowBuilder所有上述方法无call/alloc/资源注册。窗口资源初始化是另一个阶段：WindowInitialize dict518/477850从builder读取device/token/count，4779d8..4779ec **此时** 通过caller array逐一读取texture指针并快照到Window；4779f0把Window写入texture私有+98。live Window已建立资源联系，与builder不同。
- 当前native WindowBuilderSetTextures立即copy数组内容（nvn.c:1948附近）。这会使`SetTextures→caller修改数组→WindowInitialize/GetTexture`的可见性不同于原SDK；应修到Initialize前读取当前数组，或明确是受限合同，不能简单替换注释便声称已独立核实等价。当前2/3真机probe不自动覆盖这个时点负例。

### 可以安全修复的生命周期边界

从这些无owner的原builder方法可独立支持：同一栈地址在旧builder生命期结束后可作为另一种builder使用。实现中仅允许retag已知builder kind且无strong refs/owned resources的metadata；同kind Defaults保留weak device，跨kind不应继承旧类型的device字段语义。Device/Pool/Sampler/Window/Command/Program等live资源始终严格拒绝地址复用，不能靠Defaults抢占owner。

现generic `builder()`已有该guard，但SamplerBuilder helper和WindowBuilder helper仍各自find-or-create（nvn.c:865、1924附近），会在跨builder地址复用时EEXIST。后续应统一调用安全builder路径并明确保留同kind weak device；不要顺带改变live resource检查。

## 建议后续必要 HOST 用例（本次未执行）

1. View default getters写out0并返回false；setter→getter指定位、count0与非0；复制到新地址后原view覆写，副本不变；compare忽略未set值/padding、但区分presence；资源范围解析与超范围拒绝。
2. 同地址SB→WB→SB的Defaults/SetDevice顺序；同kindDevice-beforeDefaults保留；Device已Finalize后Initialize拒绝且不解引用旧pointer；liveSampler/Window地址不可retag。
3. Window SetTextures后在Init前修改caller数组：GetTexture和Initialize都看到当前array；Init后再改数组不得改变liveWindow快照。builder自身覆盖/离开作用域后，已成功Window仍持原资源强ref直到safe retire。

仅提交facts与可复核静态元数据；没有实现新view能力、没有GPU/游戏恢复声明。
