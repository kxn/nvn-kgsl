> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# SDK53.314 UBO 绑定静态审计

本轮只读原 SDK/公开 Maxwell/deko3d 资料，产物全部 ignored。未改生产/窗口候选、未 ADB、未执行原 ARM/GPU；没有 Metal/Vulkan 依据。最有价值的确定结论是：**五个 graphics stage 的 NVN API uniform index 0..13 对应原 Maxwell source CB3..16，即 sourceCB=APIindex+3。** 这不是 API0 单个fixture外推。

## 输入与可重复入口

原 Update/ExeFS/sdk SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，build-id `2d369d9999e3e10e665106f840ffff9bdaa0bb8e000000000000000000000000`。原始三段各自 SHA 验证后，按 MOD0 动态表 RELA 解析 dictionary523 的方法指针，不按名字邻近地址猜函数。完整段/方法/工具指纹在 `normal-v5/static.json`。

可评审正常工具候选 `tree/nvn-native/tools/research_ubo_sdk.py`，保留原 nso.py 解压入口，临时 raw 自动删除，反汇编只允许 fresh `.local` 输出；没有把 SDK 机器码写进工具/源码。将来promote相对路径可直接使用；当前通过 `--root` 明确生产checkout：

```
${D2R_REFERENCE_ROOT}/.local/venv/bin/python TREE/nvn-native/tools/research_ubo_sdk.py \
  --root ${LEGACY_D2R_WORKTREE} \
  --sdk ${D2R_REFERENCE_ROOT}/work/data/switch/3.3.94012/Update/ExeFS/sdk \
  --out ${LEGACY_D2R_WORKTREE}/.local/NEW_OUTPUT
```

`check_static.py` 实际两次 fresh 运行：normal-v5/v6 方法ASM逐byte一致、除out选项外metadata一致；既有out与错误原SDK均拒绝。`proof.json` 记录命令、结果、输入/产物hash。70项合法 index/stage 位代数检查只是静态位域核验，不称 CPU/SDK/GPU 执行测试。

## ABI、stage和具体编码

| 方法 | dictionary / RELA槽 | SDK地址 | 本release行为 |
| --- | --- | --- | --- |
| BindUniformBuffer | 68 / 0xc64078 | 0x480bf0 | x0 command,w1 NVNshaderStage,w2 index,x3 address,x4 size_t |
| BindUniformBuffers | 69 / 0xc64080 | 0x480e80 | x0 command,w1 stage,w2 first,w3 count,x4 range数组；每range16B，address64+size64 |
| UpdateUniformBuffer | 186 / 0xc64428 | 0x485a00 | x0 command,x1 baseGPUVA,x2 bufferSize,x3 offset,x4 dataSize,x5 CPUdata；没有stage/index |

公开重建头的 NVNshaderStage 0VS/1FS/2GS/3TCS/4TES/5CS，与 SDK 在 0x480c14/0x480ea4 比较stage==5的计算分流及 graphics method表闭合。不能把 BindProgram 的 stage **bitmask** 当成这个 stage **enum**。

Graphics method table `0xabf4c8+4*stage` 的原值如下（不是IR3 UBO binding）：

| NVN API stage | Maxwell byte method | Bind group |
| --- | --- | --- |
| 0 Vertex | 0x2410 | 0 |
| 1 Fragment | 0x2490 | 4 |
| 2 Geometry | 0x2470 | 3 |
| 3 Tess control | 0x2430 | 1 |
| 4 Tess evaluation | 0x2450 | 2 |

单个绑定：0x480ca8 `index<<20`，0x480cac `+0x300000`，0x480cb0 `&0x1f00000`，OR入 immediate packet，再以stage表method>>2绑定。非零size还置VALID bit；size0在0x480cc8路径只发VALID0解绑相同slot。批量0x480f40/0x480f4c起始也加3，0x480fa0每条slot加1；无特殊device/program flag改变这个映射。

NVIDIA官方 MAXWELL_B `clb197.h` 独立证明硬件字段：BIND_GROUP_CONSTANT_BUFFER(j)=0x2410+j*32；VALID bit0；SHADER_SLOT bits8:4。SDK immediate数据在packet bits16以上，因此`index+3`正落入该slot字段。selector0x2380/84/88为size/GPUVAhi/lo，LOAD_OFFSET0x238c、LOAD_DATA0x2390。官方固定commit/hash/本地文件见 primary/sources.json。

SDK GetInteger(2 UNIFORM_BUFFER_BINDINGS_PER_STAGE)=14；GetInteger(3 MAX_UNIFORM_BUFFER_SIZE)=65536；(4 UNIFORM_BUFFER_ALIGNMENT)=256；(0x3c UNIFORM_BUFFER_UPDATE_ALIGNMENT)=4。helper0x45b0c8预置14，property2直接跳公共store0x45b17c；其余3个直接常量分支，4个property在device consumer0x46afc0都走0x46b028读取helper值，没有device覆盖。

Release Bind方法本身**不验证** index/stage/addressalignment。合法界限要按公开property解释为0..13；不能把超域index的5位wrap、未知stage的越界table读视为合法合同。非零size编码roundup到256（低32位），size0解绑；这也不代表可以向上层接受溢出/过大size。当前native core已把UBO槽数限制14，但其已资格profile允许16B地址/大小对齐；这是targetbackend政策，不能冒称 SDK 报告对齐16。

## Compute 与 Update 的不能混同处

Compute不从graphics stage表发同样的GPU packet：单/数组录制24B control opcode12，+4为原APIindex、+8为GPUVA、+0x10为低32size。QueueSubmit内部 consumer0x472da0 的 table0xabf097(byte34)指向0x472e54，提取这些值后0x472e64调用0x486f40。

0x486f64明确计算APIindex+3，0x486f68比较<=7后直接写QMD相应CB描述符；所以APIindex0..4直接进CB3..7。**APIindex5..13虽保存到driver元数据并置dirty，却不在该函数直接写QMD。** 更完整的driver元数据→shader读取路径这轮没有闭合，不能据此宣称compute全14槽都简单直绑CB3..16。这是原SDK实现的真实例外；不限制已核实的graphics全14槽规律。

UpdateUniformBuffer只设置一个GPUVA selector window、offset，并把CPUdata在**record时**内联到GPU命令流；dataSize>>2，每块最多127个word，拷贝调用0x56b770。bufferSize selector至少256并roundup256。0x485a80某command flag会额外发PIPE_NOP(0x1a2c)，不改变CB绑定index。它不是“再绑定某个API UBO”，也不能把GPU序列中的更新偷换为submit前CPU写目标；其顺序、绑定和数据寿命须分别满足。

## CB0/1/2与generic Draw resolver

本轮直接证明的是三者**不在** NVN BindUniformBuffer 的graphics API槽域。不能以 sourceSlot-3 为负数后强转成任一用户索引，也不能将独立translator分配的 native IR3 UBO binding 当成API index。

- sourceCB1：原shader package中存在经完整控制/代码/C1身份核对的immutable C1 payload，shader agent已在当前C1 FS确认其sourceSlot1。该payload应按program/stage独立绑定。公开deko3d cmd_bind_common.cpp:62/119同样将constbuf1作为program内部数据，但deko不是NVN规范；本轮未完整追完SDK ProgramSetShaders所有C1构造分支，不扩大此结论。
- sourceCB2：已有原shader handle表/SeparateTexture接口的独立事实属于专用driver/table域；该族必须由经验证的handle域与selector实现，不用用户UBO槽填充。当前未知C1 handle(-1,110/112)仍不能据本报告猜成slot10/11。
- sourceCB0：driver参数/存储表属于专用payload；本轮没有重审其全部字段。未知字段保持未实现。**native IR3 UBO0的support_buffer不能因为编号0而称为sourceCB0**，其来源由shader agent在ResourceManager注入审计中独立核对。

可实施graphics resolver：对compiled stage的每项(sourceCB,nativeBinding)，sourceCB3..16读取该NVN stage的API index=sourceCB-3状态，随后把canonical BO/range绑定到指定nativeBinding；未绑定或不足范围明确拒绝，不造zero buffer。sourceCB1只接受已验证program immutable payload；CB0/2必须走具名driver provider，否则ENOTSUP。nativeBinding始终以编译反射/ResourceManager身份为准，不等于sourceCB或APIindex。

## 独立公开资料的适用性

`primary/clb197.h` 来自NVIDIA官方 open-gpu-doc，固定commit `d965102724767171840bcbdf1a4af061f60f3e36`，SHA `175c50f29798b39c6858cafe31497e9e9798c70ca068a64856719003b5d088d7`，可确认Maxwell硬件方法/位域，不能单独规定NVN API映射。

固定deko3d commit `373bfd43d8b2d805fa2eb7d406dbb823032df958`，原始URL/hash在primary/sources.json。它的Dk uniform slots=16，graphics明确`2+firstId+i`、compute`id+2`且仅前6项直接QMD。**这与NVN53.314的14槽/+3不同，不能继承它的API布局。** 它只辅助理解同一Maxwell硬件的selector/绑定/内部program constant分类。没有引用旧Metal/HLE的返回值、对象结构或布局作规范。


root在正常工作树通过 nvn-native/tools/research_ubo_sdk.py 再次从原NSO生成，见uniform-binding-sdk-static.json。原始NSO、解压件和机器码反汇编均未提交；没有执行原SDK ARM代码、GPU或游戏。
